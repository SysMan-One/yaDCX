$	l_ver = 'F$VERIFY(0)'		! first line: keep the header quiet under SET VERIFY
$!
$!  YADCX_DEMO.COM -- show yaDCX at work on sample files
$!
$!  __MODULE__	YADCX_DEMO
$!  __IDENT__	X-01-00ECO01
$!  __REV__	01.00.01
$!
$!  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
$!
$!  MODIFICATION HISTORY:
$!
$!	26-SEP-2026	CLD	X-01-00ECO01: initial version.
$!
$!  Usage:	$ @dev:[dir]YADCX_DEMO [work-directory]
$!		$ @dev:[dir]YADCX_DEMO /OUTPUT=DEMO.LOG	- keep a protocol
$!
$!  dev:[dir] is the yaDCX directory, YADCX.EXE and YADCX_SFX.EXE must be
$!  built there (@YADCX_BUILD).  The demo works in a new directory, default
$!  [.YADCX_DEMO] under the current one, and leaves everything there for a
$!  look; it prints how to delete it at the end.  Nothing else is touched.
$!
$!  The steps, all with SET VERIFY on, so the commands show with their output:
$!
$!	1. sample files: a BACKUP saveset of the yaDCX sources (Fixed 32256),
$!	   a text file (Variable, CR), an image (Fixed 512)
$!	2. a kit of each, with /OUTPUT and /NAME
$!	3. YADCX /LIST
$!	4. the kit is copied under another name and run; the saveset comes
$!	   back with its attributes, DIFFERENCES and BACKUP check it
$!	5. /VERIFY and /OUTPUT of a kit
$!	6. damaged kits: one byte changed (CRCERR), the end lost (TRUNCATED),
$!	   the kit turned into a text file as a text-mode transfer does
$!	7. user mistakes and their messages
$!
$	SET NOON
$	l_home = F$ENVIRONMENT("PROCEDURE")
$	l_home = F$PARSE(l_home,,,"DEVICE") + F$PARSE(l_home,,,"DIRECTORY")
$	IF F$SEARCH(l_home + "YADCX.EXE") .EQS. "" .OR. F$SEARCH(l_home + "YADCX_SFX.EXE") .EQS. ""
$	THEN
$		WRITE SYS$OUTPUT "%YADCX_DEMO-F-NOTBUILT, no YADCX.EXE/YADCX_SFX.EXE in ", l_home, " - run @", l_home, "YADCX_BUILD first"
$		EXIT 44 + 0 * F$VERIFY(l_ver)
$	ENDIF
$	l_work = P1
$	IF l_work .EQS. "" THEN l_work = "[.YADCX_DEMO]"
$	l_work = F$PARSE(l_work,,,"DEVICE","SYNTAX_ONLY") + F$PARSE(l_work,,,"DIRECTORY","SYNTAX_ONLY")
$	IF F$PARSE(l_work) .NES. ""
$	THEN
$		WRITE SYS$OUTPUT "%YADCX_DEMO-F-EXISTS, ", l_work, " already exists - delete it or give another directory as P1"
$		EXIT 44 + 0 * F$VERIFY(l_ver)
$	ENDIF
$	l_old = F$ENVIRONMENT("DEFAULT")
$	CREATE/DIRECTORY 'l_work'
$	SET DEFAULT 'l_work'
$	CREATE/DIRECTORY [.KITS],[.OTHER],[.BROKEN]
$	SET VERIFY
$ !
$ ! --- 0. Setup: define the YADCX command
$	@'l_home'YADCX
$	SHOW SYMBOL YADCX
$ !
$ ! --- 1. Sample files: a BACKUP saveset, a text file, an image
$	BACKUP 'l_home'*.C,*.H,*.MSG,*.CLD,*.COM,*.TXT SRC.BCK/SAVE_SET
$	COPY 'l_home'AAAREADME.TXT []NOTES.TXT
$	COPY 'l_home'YADCX.EXE []TOOL.EXE
$	@'l_home'ATTR SRC.BCK
$	@'l_home'ATTR NOTES.TXT
$	@'l_home'ATTR TOOL.EXE
$ !
$ ! --- 2. Build kits
$	YADCX SRC.BCK
$	SHOW SYMBOL $STATUS
$	YADCX NOTES.TXT /OUTPUT=[.KITS]NOTESKIT
$	YADCX TOOL.EXE /OUTPUT=[.KITS]TOOLKIT /NAME=NEWTOOL.EXE
$	DIRECTORY/SIZE SRC.EXE,[.KITS]*.EXE
$ !
$ ! --- 3. Look into a kit without unpacking it
$	YADCX /LIST SRC.EXE
$ !
$ ! --- 4. "Another system": copy the kit under another name and run it
$	COPY SRC.EXE [.OTHER]MYSOURCES.EXE
$	SET DEFAULT [.OTHER]
$	RUN MYSOURCES.EXE
$	SHOW SYMBOL $STATUS
$	@'l_home'ATTR SRC.BCK
$	DIFFERENCES [-]SRC.BCK []SRC.BCK
$	BACKUP SRC.BCK/SAVE_SET [.FILES]
$	DIRECTORY/SIZE [.FILES]
$	DIFFERENCES [.FILES]YADCX.C 'l_home'YADCX.C
$ !
$ ! --- 5. Qualifiers of a kit: /VERIFY, /OUTPUT; a name given with /NAME
$	K := $'F$PARSE("[-.KITS]NOTESKIT.EXE")'
$	K /VERIFY
$	K /OUTPUT=README_COPY.TXT
$	K := $'F$PARSE("[-.KITS]TOOLKIT.EXE")'
$	K
$	@'l_home'ATTR README_COPY.TXT
$	@'l_home'ATTR NEWTOOL.EXE
$	DIFFERENCES [-]NOTES.TXT []README_COPY.TXT
$	DIFFERENCES [-]TOOL.EXE []NEWTOOL.EXE
$ !
$ ! --- 6. Damaged kits, made here from SRC.EXE
$	SET DEFAULT [-.BROKEN]
$ !        BAD.EXE: one byte of the payload changed (a record near the end)
$	COPY [-]SRC.EXE BAD.EXE
$	l_ver2 = 'F$VERIFY(0)'
$	l_n = F$FILE_ATTRIBUTES("BAD.EXE","EOF") - 5
$	OPEN/READ/WRITE l_fd BAD.EXE
$	l_i = 0
$ L_READ:
$	READ l_fd l_rec
$	l_i = l_i + 1
$	IF l_i .LT. l_n THEN GOTO L_READ
$	l_byte = " "
$	l_byte[0,8] = (F$CVUI(800,8,l_rec) + 1) .AND. 255
$	l_rec = F$EXTRACT(0,100,l_rec) + l_byte + F$EXTRACT(101,411,l_rec)
$	WRITE/UPDATE/SYMBOL l_fd l_rec
$	CLOSE l_fd
$	l_ver2 = F$VERIFY(l_ver2)
$	RUN BAD.EXE
$	SHOW SYMBOL $STATUS
$	DIRECTORY SRC.BCK
$ !        SHORT.EXE: the last 100 blocks lost
$	COPY [-]SRC.EXE SHORT.EXE
$	l_ebk = F$FILE_ATTRIBUTES("SHORT.EXE","EOF") - 99
$	SET FILE/ATTRIBUTE=(EBK:'l_ebk',FFB:0) SHORT.EXE
$	RUN SHORT.EXE
$ !        TEXT.EXE: the kit turned into a text file, as a text-mode transfer does
$	CREATE TEXT.FDL
RECORD
	FORMAT	STREAM_LF
$	CONVERT/FDL=TEXT.FDL [-]SRC.EXE TEXT.EXE
$	RUN TEXT.EXE
$	YADCX /LIST TEXT.EXE
$ !
$ ! --- 7. User mistakes
$	RUN 'l_home'YADCX_SFX.EXE
$	YADCX [-]NOTES.TXT /STUB=[-]SRC.EXE
$	YADCX [-]NOTES.TXT /NAME=[OTHER]X.TXT
$	YADCX NOSUCH.BCK
$	YADCX [-]NOTES.TXT /OUTPUT=[.NODIR]
$	l_ver2 = 'F$VERIFY(0)'
$	SET DEFAULT 'l_old'
$	WRITE SYS$OUTPUT ""
$	WRITE SYS$OUTPUT "%YADCX_DEMO-S-DONE, the files are in ", l_work
$	WRITE SYS$OUTPUT "-YADCX_DEMO-I-CLEANUP, to delete them: DELETE ", l_work - "]", "...]*.*;* a few times until"
$	WRITE SYS$OUTPUT "-YADCX_DEMO-I-CLEANUP, nothing is left, then SET PROTECTION=O:RWED and DELETE the .DIR itself"
$	EXIT 1 + 0 * F$VERIFY(l_ver)
