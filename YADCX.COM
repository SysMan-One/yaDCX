$	l_ver = 'F$VERIFY(0)'		! first line: keep the header quiet under SET VERIFY
$!
$!  YADCX.COM -- define the YADCX foreign command for this process
$!
$!  __MODULE__	YADCX_SETUP
$!  __IDENT__	X-01-00ECO01
$!  __REV__	01.00.01
$!
$!  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
$!
$!  MODIFICATION HISTORY:
$!
$!	26-SEP-2026	CLD	X-01-00: initial version.
$!	26-SEP-2026	CLD	X-01-00ECO01: quiet under SET VERIFY.
$!
$!  Usage:	$ @dev:[dir]YADCX	(put it in LOGIN.COM to have YADCX always)
$!
$	l_here = F$ENVIRONMENT("PROCEDURE")
$	l_here = F$PARSE(l_here,,,"DEVICE") + F$PARSE(l_here,,,"DIRECTORY")
$	YADCX :== $'l_here'YADCX.EXE
$	EXIT 1 + 0 * F$VERIFY(l_ver)
