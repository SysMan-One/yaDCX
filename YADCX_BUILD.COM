$!
$!  YADCX_BUILD.COM -- build YADCX.EXE (the kit builder) and YADCX_SFX.EXE (the stub)
$!
$!  __MODULE__	YADCX_BUILD
$!  __IDENT__	X-01-00
$!  __REV__	01.00.00
$!
$!  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
$!
$!  MODIFICATION HISTORY:
$!
$!	26-SEP-2026	CLD	X-01-00: initial version.
$!
$!  Usage:	$ @YADCX_BUILD		from the source directory
$!		$ @YADCX_BUILD DEBUG	/DEBUG/NOOPTIMIZE build
$!
$	SET NOON
$	l_here = F$ENVIRONMENT("PROCEDURE")
$	l_here = F$PARSE(l_here,,,"DEVICE") + F$PARSE(l_here,,,"DIRECTORY")
$	l_old = F$ENVIRONMENT("DEFAULT")
$	SET DEFAULT 'l_here'
$	l_cc = "CC/NOLIST/WARNINGS=ENABLE=QUESTCODE"
$	l_link = "LINK/NOTRACEBACK"
$	IF P1 .EQS. "DEBUG"
$	THEN
$		l_cc = "CC/NOLIST/DEBUG/NOOPTIMIZE"
$		l_link = "LINK/DEBUG"
$	ENDIF
$	l_err = 0
$!
$	MESSAGE YADCX_MSG.MSG
$	IF .NOT. $STATUS THEN l_err = l_err + 1
$	SET COMMAND/OBJECT YADCX_CLI.CLD
$	IF .NOT. $STATUS THEN l_err = l_err + 1
$	l_mods = "YADCX,YADCX_COMMON,YADCX_SFX"
$	l_i = 0
$ L_CC:
$	l_mod = F$ELEMENT(l_i, ",", l_mods)
$	IF l_mod .EQS. "," THEN GOTO L_LINK
$	'l_cc' 'l_mod'.C
$	IF .NOT. $STATUS THEN l_err = l_err + 1
$	l_i = l_i + 1
$	GOTO L_CC
$ L_LINK:
$	IF l_err .NE. 0 THEN GOTO L_DONE
$	'l_link'/EXECUTABLE=YADCX.EXE YADCX.OBJ,YADCX_COMMON.OBJ,YADCX_MSG.OBJ,YADCX_CLI.OBJ
$	IF .NOT. $STATUS THEN l_err = l_err + 1
$	'l_link'/EXECUTABLE=YADCX_SFX.EXE YADCX_SFX.OBJ,YADCX_COMMON.OBJ,YADCX_MSG.OBJ,YADCX_CLI.OBJ
$	IF .NOT. $STATUS THEN l_err = l_err + 1
$ L_DONE:
$	SET DEFAULT 'l_old'
$	IF l_err .NE. 0
$	THEN
$		WRITE SYS$OUTPUT "%YADCX_BUILD-E-FAILED, ''l_err' step(s) failed, see the messages above"
$		EXIT 44
$	ENDIF
$	WRITE SYS$OUTPUT "%YADCX_BUILD-S-DONE, YADCX.EXE and YADCX_SFX.EXE built in ''l_here'"
$	EXIT 1
