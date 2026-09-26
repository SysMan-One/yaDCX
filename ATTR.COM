$	l_ver = 'F$VERIFY(0)'		! first line: keep the header quiet under SET VERIFY
$!
$!  ATTR.COM -- show the RMS attributes of a file in one line
$!
$!  __MODULE__	YADCX_ATTR
$!  __IDENT__	X-01-00ECO01
$!  __REV__	01.00.01
$!
$!  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
$!
$!  MODIFICATION HISTORY:
$!
$!	26-SEP-2026	CLD	X-01-00ECO01: initial version (demo helper).
$!
$!  Usage:	$ @dev:[dir]ATTR file
$!
$!  Prints the name, record format, maximum record size, record attributes
$!  and the exact size in bytes (from EOF and FFB), e.g.:
$!
$!	  SRC.BCK          rfm=FIX mrs=32256 rat=none bytes=258048
$!
$	l_f = F$SEARCH(P1)
$	IF l_f .EQS. ""
$	THEN
$		WRITE SYS$OUTPUT "  ", P1, ": no such file"
$		GOTO L_END
$	ENDIF
$	l_n = F$PARSE(l_f,,,"NAME") + F$PARSE(l_f,,,"TYPE")
$	l_rat = F$FILE_ATTRIBUTES(l_f,"RAT")
$	IF l_rat .EQS. "" THEN l_rat = "none"
$	l_eof = F$FILE_ATTRIBUTES(l_f,"EOF")
$	l_ffb = F$FILE_ATTRIBUTES(l_f,"FFB")
$	l_b = l_eof * 512
$	IF l_ffb .NE. 0 THEN l_b = (l_eof - 1) * 512 + l_ffb
$	WRITE SYS$OUTPUT F$FAO("  !16AS rfm=!3AS mrs=!5UL rat=!4AS bytes=!UL", l_n, F$FILE_ATTRIBUTES(l_f,"RFM"), -
		F$FILE_ATTRIBUTES(l_f,"MRS"), l_rat, l_b)
$ L_END:
$	EXIT 1 + 0 * F$VERIFY(l_ver)
