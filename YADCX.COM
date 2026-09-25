$!
$!  YADCX.COM -- define the YADCX foreign command for this process
$!
$!  __MODULE__	YADCX_SETUP
$!  __IDENT__	X-01-00
$!  __REV__	01.00.00
$!
$!  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
$!
$!  MODIFICATION HISTORY:
$!
$!	26-SEP-2026	CLD	X-01-00: initial version.
$!
$!  Usage:	$ @dev:[dir]YADCX	(put it in LOGIN.COM to have YADCX always)
$!
$	l_here = F$ENVIRONMENT("PROCEDURE")
$	l_here = F$PARSE(l_here,,,"DEVICE") + F$PARSE(l_here,,,"DIRECTORY")
$	YADCX :== $'l_here'YADCX.EXE
$	EXIT 1
