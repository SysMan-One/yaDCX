AAAREADME.TXT -- yaDCX, self-extracting kits for OpenVMS

__MODULE__  AAAREADME
__IDENT__   X-02-00
__REV__     02.00.00

AUTHORS:    StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

MODIFICATION HISTORY:

    26-SEP-2026  CLD  X-01-00: initial version.
    26-SEP-2026  CLD  X-01-00ECO01: example session (YADCX_DEMO.COM), ACTIMAGE,
                      yadcx_check.py.
    26-SEP-2026  CLD  X-01-00ECO02: CONTRIBUTING.md, AGENTS.md, mk_aaareadme.py.
    26-SEP-2026  CLD  X-01-01: strings by descriptor (no .ASCIZ) in the code.
    26-SEP-2026  CLD  X-01-01ECO01: IOSB from IOSBDEF.H.
    26-SEP-2026  CLD  X-02-00: /COMPRESSION=DCX|NONE, kit format 2.

Made from README.md.  The text is UTF-8: the Russian part needs a UTF-8
terminal, or read README.md on the git side.

YADCX -- SELF-EXTRACTING KITS FOR OPENVMS
========================================

Русский текст -- ниже, после английского.

Version X-02-00 (02.00.00). Authors: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).

What it is
----------

yaDCX turns one OpenVMS file (usually a BACKUP saveset, .BCK) into one
program (.EXE). You copy that program to another OpenVMS system and run it.
The program writes the original file back -- the same bytes, the same record
format, the same end of file.

The data in the kit are compressed with the standard VMS DCX routines (the
same ones LIBRARY uses), so a kit is usually a third smaller than the file.
Every OpenVMS system has them: nothing else is needed to unpack a kit.

Before it writes anything, the program checks a CRC-32 of the data. If the
kit was damaged on the way (for example, sent over FTP in text mode), it
says so and writes nothing. After unpacking, it checks a second CRC-32 of
the file itself.

Why: a .BCK file loses its record attributes when it goes through FTP,
mail, a web server or a USB stick. An .EXE in the "Fixed 512" format goes
through safely, and the attributes of the saveset travel inside it.

How a kit looks inside
----------------------

    VBN 1 .. S        the stub, YADCX_SFX.EXE (the program that restores)
    VBN S+1           the kit header, one 512-byte block
    VBN S+2 .. S+1+D  the stored data, D blocks

The stored data depend on the compression (/COMPRESSION):

- DCX (the default): the DCX map, then the file in pieces of 32256 bytes
  (63 blocks), each one compressed -- or kept as is when DCX can not make it
  smaller. If the whole file gets no smaller, the kit is made as NONE.
- NONE: the file, block for block, as it is on disk.

The header holds: the marker yaDCX-KIT-HEADER, the format version (2), the
file size in bytes and blocks, the 32-byte RMS record attributes (FAT) of
the file, the build time, the builder version, the file name, the method,
the size of the stored data, two CRC-32 (of the stored data and of the file)
and the CRC-32 of the header itself. All numbers are little-endian. The
CRC-32 is the same as zlib.crc32() (polynomial EDB88320), so you can check
a kit on Linux too. Kits of format 1 (yaDCX X-01) are still read.

Files
-----

  YADCX.C
        the kit builder, YADCX.EXE (main module)
  YADCX_SFX.C
        the stub, YADCX_SFX.EXE
  YADCX_COMMON.C
        code used by both: block I/O, FAT, CRC, messages, command line
  YADCX.H
        the kit format and the prototypes
  YADCX_MSG.MSG
        messages (facility YADCX, number 1100)
  YADCX_CLI.CLD
        command syntax (verbs YADCX and SFX)
  YADCX_BUILD.COM
        builds both images
  YADCX.COM
        defines the YADCX command
  YADCX_DEMO.COM
        the demo, see "Example session"
  ATTR.COM
        prints the RMS attributes of a file in one line (used by the demo)
  tools/mk_aaareadme.py
        makes AAAREADME.TXT from README.md
  CONTRIBUTING.md
        code style and rules for contributors (English + Russian)
  AGENTS.md, CLAUDE.md
        the same rules, short, for AI coding agents
  AAAREADME.TXT
        this guide, for VMS users
  tools/yadcx_check.py
        checks kits on Linux (or anywhere with Python 3)

Build
-----

You need HP C (tested with V7.1) on OpenVMS Alpha.

    $ SET DEFAULT dev:[dir]           ! the directory with the sources
    $ @YADCX_BUILD                    ! builds YADCX.EXE and YADCX_SFX.EXE
    $ @YADCX                          ! defines the YADCX command

Keep YADCX.EXE and YADCX_SFX.EXE in the same directory: the builder
takes the stub from its own directory.

Make a kit
----------

    $ BACKUP [MYAPP...]*.* MYAPP.BCK/SAVE_SET
    $ YADCX MYAPP.BCK                 ! makes MYAPP.EXE in the current directory

Qualifiers:

  /OUTPUT=file
        where to put the kit. Default: <payload name>.EXE in the current
        directory
  /NAME=name.type
        the file name stored in the kit. Default: the payload's own name. No
        device, directory or version
  /STUB=file
        another stub. Default: YADCX_SFX.EXE next to YADCX.EXE
  /COMPRESSION=DCX
        compress with the VMS DCX routines -- the default
  /COMPRESSION=NONE
        keep the file as it is (a bigger kit, a faster build)
  /LIST
        do not build: show a kit and check it fully (both CRC): YADCX /LIST
        MYAPP.EXE

Use a kit
---------

Copy the kit in binary mode (FTP: binary or type i). Then:

    $ RUN MYAPP.EXE                       ! writes MYAPP.BCK to the current directory

With qualifiers, define it as a command first:

    $ KIT :== $DKA0:[WORK]MYAPP.EXE
    $ KIT /LIST                           ! show what is inside
    $ KIT /VERIFY                         ! unpack in memory, check both CRC, write nothing
    $ KIT /OUTPUT=[.SUB]OTHER.BCK         ! write under another name

Check a kit on Linux
--------------------

Fetch the kit in binary mode, then:

    $ python3 tools/yadcx_check.py SRC.EXE SRCNONE.EXE
    SRC.EXE: v2, header at VBN 49, DCX stream 142075 bytes, recorded 680DB3A6, computed 680DB3A6 -> OK
    SRC.EXE: payload 290304 bytes, CRC-32 7AB6A331 - checked by the stub on VMS only (DCX)
    SRCNONE.EXE: v2, header at VBN 49, payload 290304 bytes, recorded 7AB6A331, computed 7AB6A331 -> OK

The exit status is 0 when all kits are OK. It checks the stored data -- the
same check the stub makes before it writes anything. A NONE kit is then
fully checked; a DCX kit can only be unpacked on VMS, so the CRC-32 of the
file itself is checked there (KIT /VERIFY).

Example session
---------------

YADCX_DEMO.COM shows everything on sample files. Run it yourself (it works in a
new directory, [.YADCX_DEMO] by default, and touches nothing else):

    $ @dev:[dir]YADCX_DEMO                       ! dev:[dir] is where yaDCX is built
    $ @dev:[dir]YADCX_DEMO/OUTPUT=DEMO.LOG       ! the same, with a protocol file

Below is a real protocol. Lines that start with $ are commands (SET VERIFY),
the rest is what they print. ATTR.COM prints the RMS attributes of a file in
one line.

What it shows:

1. three sample files: a BACKUP saveset (Fixed 32256), a text file (Variable, CR), an image (Fixed 512);
2. a kit for each of them (DCX-compressed), one with /OUTPUT, one with /NAME;
   the saveset once more with /COMPRESSION=NONE to compare the sizes;
3. YADCX /LIST looks into a kit;
4. the kit is copied under another name and run: the saveset comes back with the same attributes,
   DIFFERENCES finds nothing, and BACKUP restores the files from it;
5. /VERIFY and /OUTPUT of a kit;
6. damaged kits, made by the demo itself: one changed byte (CRCERR, nothing written),
   a lost end (TRUNCATED), the kit turned into a text file (VMS can not start it);
7. typical mistakes and their messages;
8. the same CRC check on Linux (tools/yadcx_check.py).

    $ @DQB0:[LAISHEV.YADCX]YADCX_DEMO DQB0:[LAISHEV.YADCX_DEMO]      ! OpenVMS Alpha V8.3, 26-SEP-2026
    $ !
    $ ! --- 0. Setup: define the YADCX command
    $ @DQB0:[LAISHEV.YADCX]YADCX
    $ SHOW SYMBOL YADCX
      YADCX == "$DQB0:[LAISHEV.YADCX]YADCX.EXE"
    $ !
    $ ! --- 1. Sample files: a BACKUP saveset, a text file, an image
    $ BACKUP DQB0:[LAISHEV.YADCX]*.C,*.H,*.MSG,*.CLD,*.COM,*.TXT SRC.BCK/SAVE_SET
    $ COPY DQB0:[LAISHEV.YADCX]AAAREADME.TXT []NOTES.TXT
    $ COPY DQB0:[LAISHEV.YADCX]YADCX.EXE []TOOL.EXE
    $ @DQB0:[LAISHEV.YADCX]ATTR SRC.BCK
      SRC.BCK          rfm=FIX mrs=32256 rat=none bytes=290304
    $ @DQB0:[LAISHEV.YADCX]ATTR NOTES.TXT
      NOTES.TXT        rfm=VAR mrs=    0 rat=CR   bytes=28502
    $ @DQB0:[LAISHEV.YADCX]ATTR TOOL.EXE
      TOOL.EXE         rfm=FIX mrs=  512 rat=none bytes=28160
    $ !
    $ ! --- 2. Build kits
    $ YADCX SRC.BCK
    %YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]SRC.BCK;1, 290304 bytes (567 blocks), CRC-32 7AB6A331
    %YADCX-I-COMPRESSED, payload compressed by DCX to 142075 bytes (278 blocks), 48.94% of the original; map 4420 bytes, 1 record(s) stored as is
    %YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO]SRC.EXE;1 built, 48 stub + 1 header + 278 data blocks
    $ SHOW SYMBOL $STATUS
      $STATUS == "%X1C4C8009"
    $ YADCX NOTES.TXT /OUTPUT=[.KITS]NOTESKIT
    %YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]NOTES.TXT;1, 28502 bytes (56 blocks), CRC-32 A41F9BAE
    %YADCX-I-COMPRESSED, payload compressed by DCX to 21295 bytes (42 blocks), 74.71% of the original; map 2220 bytes, 0 record(s) stored as is
    %YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO.KITS]NOTESKIT.EXE;1 built, 48 stub + 1 header + 42 data blocks
    $ YADCX TOOL.EXE /OUTPUT=[.KITS]TOOLKIT /NAME=NEWTOOL.EXE
    %YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]TOOL.EXE;1, 28160 bytes (55 blocks), CRC-32 867207C5
    %YADCX-I-COMPRESSED, payload compressed by DCX to 20755 bytes (41 blocks), 73.70% of the original; map 608 bytes, 0 record(s) stored as is
    %YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO.KITS]TOOLKIT.EXE;1 built, 48 stub + 1 header + 41 data blocks
    $ YADCX SRC.BCK /OUTPUT=[.KITS]SRCNONE /COMPRESSION=NONE
    %YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]SRC.BCK;1, 290304 bytes (567 blocks), CRC-32 7AB6A331
    %YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO.KITS]SRCNONE.EXE;1 built, 48 stub + 1 header + 567 data blocks
    $ DIRECTORY/SIZE SRC.BCK,SRC.EXE,[.KITS]*.EXE

    Directory DQB0:[LAISHEV.YADCX_DEMO]

    SRC.BCK;1                567
    SRC.EXE;1                327

    Total of 2 files, 894 blocks.

    Directory DQB0:[LAISHEV.YADCX_DEMO.KITS]

    NOTESKIT.EXE;1            91
    SRCNONE.EXE;1            616
    TOOLKIT.EXE;1             90

    Total of 3 files, 797 blocks.

    Grand total of 2 directories, 5 files, 1691 blocks.
    $ !
    $ ! --- 3. Look into a kit without unpacking it
    $ YADCX /LIST SRC.EXE
    %YADCX-I-KITINFO, kit contains SRC.BCK, 290304 bytes (567 blocks), built 26-SEP-2026 18:44:37.85 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is 7AB6A331
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 142075 bytes in 278 blocks, 48.94% of the original
    %YADCX-S-VERIFIED, payload CRC-32 7AB6A331 verified, nothing written
    $ !
    $ ! --- 4. "Another system": copy the kit under another name and run it
    $ COPY SRC.EXE [.OTHER]MYSOURCES.EXE
    $ SET DEFAULT [.OTHER]
    $ RUN MYSOURCES.EXE
    %YADCX-I-KITINFO, kit contains SRC.BCK, 290304 bytes (567 blocks), built 26-SEP-2026 18:44:37.85 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is 7AB6A331
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 142075 bytes in 278 blocks, 48.94% of the original
    %YADCX-S-RESTORED, DQB0:[LAISHEV.YADCX_DEMO.OTHER]SRC.BCK;1 restored, 290304 bytes, CRC-32 7AB6A331 verified
    $ SHOW SYMBOL $STATUS
      $STATUS == "%X1C4C8011"
    $ @DQB0:[LAISHEV.YADCX]ATTR SRC.BCK
      SRC.BCK          rfm=FIX mrs=32256 rat=none bytes=290304
    $ DIFFERENCES [-]SRC.BCK []SRC.BCK
    Number of difference sections found: 0
    Number of difference records found: 0

    DIFFERENCES /IGNORE=()/MERGED=1-
        DQB0:[LAISHEV.YADCX_DEMO]SRC.BCK;1-
        DQB0:[LAISHEV.YADCX_DEMO.OTHER]SRC.BCK;1
    $ BACKUP SRC.BCK/SAVE_SET [.FILES]
    $ DIRECTORY/SIZE [.FILES]

    Directory DQB0:[LAISHEV.YADCX_DEMO.OTHER.FILES]

    AAAREADME.TXT;8           56
    ATTR.COM;1                 3
    YADCX.C;9                 34
    YADCX.COM;4                2
    YADCX.H;6                 20
    YADCX_BUILD.COM;1          4
    YADCX_CLI.CLD;3            5
    YADCX_COMMON.C;9          76
    YADCX_DEMO.COM;2          12
    YADCX_MSG.MSG;5            6
    YADCX_SFX.C;8             16

    Total of 11 files, 234 blocks.
    $ DIFFERENCES [.FILES]YADCX.C DQB0:[LAISHEV.YADCX]YADCX.C
    Number of difference sections found: 0
    Number of difference records found: 0

    DIFFERENCES /IGNORE=()/MERGED=1-
        DQB0:[LAISHEV.YADCX_DEMO.OTHER.FILES]YADCX.C;9-
        DQB0:[LAISHEV.YADCX]YADCX.C;9
    $ !
    $ ! --- 5. Qualifiers of a kit: /VERIFY, /OUTPUT; a name given with /NAME
    $ K := $DQB0:[LAISHEV.YADCX_DEMO.KITS]NOTESKIT.EXE;
    $ K /VERIFY
    %YADCX-I-KITINFO, kit contains NOTES.TXT, 28502 bytes (56 blocks), built 26-SEP-2026 18:44:38.11 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is A41F9BAE
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 21295 bytes in 42 blocks, 74.71% of the original
    %YADCX-S-VERIFIED, payload CRC-32 A41F9BAE verified, nothing written
    $ K /OUTPUT=README_COPY.TXT
    %YADCX-I-KITINFO, kit contains NOTES.TXT, 28502 bytes (56 blocks), built 26-SEP-2026 18:44:38.11 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is A41F9BAE
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 21295 bytes in 42 blocks, 74.71% of the original
    %YADCX-S-RESTORED, DQB0:[LAISHEV.YADCX_DEMO.OTHER]README_COPY.TXT;1 restored, 28502 bytes, CRC-32 A41F9BAE verified
    $ K := $DQB0:[LAISHEV.YADCX_DEMO.KITS]TOOLKIT.EXE;
    $ K
    %YADCX-I-KITINFO, kit contains NEWTOOL.EXE, 28160 bytes (55 blocks), built 26-SEP-2026 18:44:38.56 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is 867207C5
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 20755 bytes in 41 blocks, 73.70% of the original
    %YADCX-S-RESTORED, DQB0:[LAISHEV.YADCX_DEMO.OTHER]NEWTOOL.EXE;1 restored, 28160 bytes, CRC-32 867207C5 verified
    $ @DQB0:[LAISHEV.YADCX]ATTR README_COPY.TXT
      README_COPY.TXT  rfm=VAR mrs=    0 rat=CR   bytes=28502
    $ @DQB0:[LAISHEV.YADCX]ATTR NEWTOOL.EXE
      NEWTOOL.EXE      rfm=FIX mrs=  512 rat=none bytes=28160
    $ DIFFERENCES [-]NOTES.TXT []README_COPY.TXT
    Number of difference sections found: 0
    Number of difference records found: 0

    DIFFERENCES /IGNORE=()/MERGED=1-
        DQB0:[LAISHEV.YADCX_DEMO]NOTES.TXT;1-
        DQB0:[LAISHEV.YADCX_DEMO.OTHER]README_COPY.TXT;1
    $ DIFFERENCES [-]TOOL.EXE []NEWTOOL.EXE
    Number of difference sections found: 0
    Number of difference records found: 0

    DIFFERENCES /IGNORE=()/MERGED=1-
        DQB0:[LAISHEV.YADCX_DEMO]TOOL.EXE;1-
        DQB0:[LAISHEV.YADCX_DEMO.OTHER]NEWTOOL.EXE;1
    $ !
    $ ! --- 6. Damaged kits, made here from SRC.EXE
    $ SET DEFAULT [-.BROKEN]
    $ !        BAD.EXE: one byte of the payload changed (a record near the end)
    $ COPY [-]SRC.EXE BAD.EXE
    $ RUN BAD.EXE
    %YADCX-I-KITINFO, kit contains SRC.BCK, 290304 bytes (567 blocks), built 26-SEP-2026 18:44:37.85 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is 7AB6A331
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 142075 bytes in 278 blocks, 48.94% of the original
    %YADCX-F-CRCERR, CRC-32 of the stored data mismatch: recorded 680DB3A6, computed 5D2773F6 -- the kit is damaged, nothing written
    $ SHOW SYMBOL $STATUS
      $STATUS == "%X1C4C8074"
    $ DIRECTORY SRC.BCK
    %DIRECT-W-NOFILES, no files found
    $ !        SHORT.EXE: the last 100 blocks lost
    $ COPY [-]SRC.EXE SHORT.EXE
    $ l_ebk = F$FILE_ATTRIBUTES("SHORT.EXE","EOF") - 99
    $ SET FILE/ATTRIBUTE=(EBK:228,FFB:0) SHORT.EXE
    $ RUN SHORT.EXE
    %YADCX-I-KITINFO, kit contains SRC.BCK, 290304 bytes (567 blocks), built 26-SEP-2026 18:44:37.85 by yaDCX X-02-00
    %YADCX-I-KITCRC, recorded CRC-32 of the payload is 7AB6A331
    %YADCX-I-KITDCX, payload is stored DCX-compressed: 142075 bytes in 278 blocks, 48.94% of the original
    %YADCX-F-TRUNCATED, the kit is truncated: 327 blocks expected, 227 present -- transfer it again in binary mode
    $ !        TEXT.EXE: the kit turned into a text file, as a text-mode transfer does
    $ CREATE TEXT.FDL
    RECORD
      FORMAT  STREAM_LF
    $ CONVERT/FDL=TEXT.FDL [-]SRC.EXE TEXT.EXE
    $ RUN TEXT.EXE
    %DCL-W-ACTIMAGE, error activating image TEXT.EXE
    -CLI-E-IMGNAME, image file DQB0:[LAISHEV.YADCX_DEMO.BROKEN]TEXT.EXE;1
    -SYSTEM-F-PROTINSTALL, protected images must be installed
    $ YADCX /LIST TEXT.EXE
    %YADCX-F-NOHEADER, no yaDCX kit header in DQB0:[LAISHEV.YADCX_DEMO.BROKEN]TEXT.EXE;1 -- this is the bare stub or not a yaDCX kit
    $ !
    $ ! --- 7. User mistakes
    $ RUN DQB0:[LAISHEV.YADCX]YADCX_SFX.EXE
    %YADCX-F-NOHEADER, no yaDCX kit header in DQB0:[LAISHEV.YADCX]YADCX_SFX.EXE;11 -- this is the bare stub or not a yaDCX kit
    $ YADCX [-]NOTES.TXT /STUB=[-]SRC.EXE
    %YADCX-F-NESTED, DQB0:[LAISHEV.YADCX_DEMO]SRC.EXE;1 already carries a kit header -- use the bare YADCX_SFX.EXE as the stub
    $ YADCX [-]NOTES.TXT /NAME=[OTHER]X.TXT
    %YADCX-F-BADNAME, /NAME value must be a plain name.type, without device, directory or version
    $ YADCX NOSUCH.BCK
    %YADCX-E-OPENIN, error opening NOSUCH.BCK as input
    -RMS-E-FNF, file not found
    $ YADCX [-]NOTES.TXT /OUTPUT=[.NODIR]
    %YADCX-E-OPENOUT, error creating [.NODIR]
    -RMS-E-DNF, directory not found
    -SYSTEM-W-NOSUCHFILE, no such file

    %YADCX_DEMO-S-DONE, the files are in DQB0:[LAISHEV.YADCX_DEMO]
    -YADCX_DEMO-I-CLEANUP, to delete them: DELETE DQB0:[LAISHEV.YADCX_DEMO...]*.*;* a few times until
    -YADCX_DEMO-I-CLEANUP, nothing is left, then SET PROTECTION=O:RWED and DELETE the .DIR itself

    # ---- Linux: SRC.EXE, KITS/SRCNONE.EXE and BROKEN/BAD.EXE fetched in binary mode ----
    $ python3 tools/yadcx_check.py SRC.EXE SRCNONE.EXE BAD.EXE
    SRC.EXE: v2, header at VBN 49, DCX stream 142075 bytes, recorded 680DB3A6, computed 680DB3A6 -> OK
    SRC.EXE: payload 290304 bytes, CRC-32 7AB6A331 - checked by the stub on VMS only (DCX)
    SRCNONE.EXE: v2, header at VBN 49, payload 290304 bytes, recorded 7AB6A331, computed 7AB6A331 -> OK
    BAD.EXE: v2, header at VBN 49, DCX stream 142075 bytes, recorded 680DB3A6, computed 5D2773F6 -> DAMAGED

A kit really sent by FTP in text (ASCII) mode gave, in another run:

    $ RUN TEXTMODE.EXE
    %DCL-W-ACTIMAGE, error activating image TEXTMODE.EXE
    -CLI-E-IMGNAME, image file DQB0:[LAISHEV.YADCX.DEMO.BROKEN]TEXTMODE.EXE;1
    -IMGACT-F-NOTNATIVE, image is not an OpenVMS Alpha image

Messages and what to do
-----------------------

$STATUS is set after every run, so you can test it in a command procedure.

  BUILT (S)
        the kit is ready -- nothing
  RESTORED (S)
        the file is written and its CRC is right -- nothing
  VERIFIED (S)
        the kit is whole and the file would come back intact (for DCX it was
        unpacked in memory); nothing was written -- nothing
  KITINFO, KITCRC, KITDCX, PAYLOAD, COMPRESSED (I)
        information -- nothing
  NOGAIN (I)
        DCX could not make the file smaller (already compressed or encrypted
        data), the kit is made without compression -- nothing;
        /COMPRESSION=NONE gives the same kit faster
  NOHEADER (F)
        the file has no kit header -- you ran the bare YADCX_SFX.EXE, or the
        file is not a yaDCX kit. Run the kit, not the stub
  BADHDR (F)
        the header is damaged -- get the kit again, in binary mode
  BADVER (F)
        the kit was made by a newer yaDCX -- get a newer YADCX_SFX.EXE/kit
        from the author of the kit
  BADMETH (F)
        the kit uses a compression this program does not know -- use a newer
        YADCX.EXE for /LIST; the kit itself unpacks with its own stub
  TRUNCATED (F)
        the end of the kit is missing -- copy it again, in binary mode;
        check free disk space and disk quota on the way
  CRCERR (F)
        the data in the kit is damaged; nothing was written -- copy it
        again, in binary mode. Text mode FTP is the usual cause
  EXPCRC (F)
        the unpacked file is not the original one; the new file is deleted
        -- should never happen with a whole kit: run KIT /VERIFY, get the
        kit again; tell the authors if it repeats
  NESTED (F)
        the /STUB file is already a kit -- use the bare YADCX_SFX.EXE as the
        stub
  BADNAME (F)
        /NAME has a device, directory or version -- give only name.type; use
        /OUTPUT when you run the kit to choose a place
  EMPTY (F)
        the payload file has 0 bytes -- check the file, maybe BACKUP failed
  OPENIN (E)
        a file can not be opened -- read the second line (-RMS-E-FNF = no
        such file, -RMS-E-PRV = no privilege)
  OPENOUT (E)
        the output file can not be created -- read the second line (-RMS-E-
        DNF = no such directory, -RMS-E-PRV, disk full)
  READERR, WRITEERR (E)
        an I/O error -- read the second line; check the disk (SHOW DEVICE,
        ANALYZE/DISK)
  ATTRERR (E)
        record attributes can not be read or set -- read the second line;
        check that the disk is ODS-2 or ODS-5
  SYSERR (E)
        a system service failed -- read the second line; tell the authors
  DCXERR (E)
        a DCX routine failed while packing or unpacking -- read the second
        line (for example -LIB-F-INSVIRMEM: not enough memory -- raise the
        page file quota); for a build, /COMPRESSION=NONE avoids DCX
  DCL-W-ACTIMAGE (with IMGACT-F-NOTNATIVE, SYSTEM-F-PROTINSTALL or another
    reason)
        VMS can not start the kit at all -- the kit was sent in text (ASCII)
        mode; copy it again in binary mode
  CLI-W-CONFLICT
        bad mix of qualifiers -- /LIST goes alone; /OUTPUT does not go with
        /LIST or /VERIFY

A failed restore never leaves a half-written file: it is deleted.

******************************************************************************

YADCX -- САМОРАСПАКОВЫВАЮЩИЕСЯ КИТЫ ДЛЯ OPENVMS
==============================================

Что это
-------

yaDCX превращает один файл OpenVMS (обычно сейвсет BACKUP, .BCK) в одну
программу (.EXE). Эту программу копируют на другую систему OpenVMS и
запускают. Программа записывает исходный файл обратно: те же байты, тот же
формат записей, тот же конец файла.

Данные в ките сжаты штатными процедурами VMS DCX (теми же, что использует
LIBRARY), поэтому кит обычно на треть меньше файла. Они есть в любой OpenVMS:
для распаковки больше ничего не нужно.

До того как что-то записать, программа проверяет CRC-32 данных. Если кит
испортился по дороге (например, его передали по FTP в текстовом режиме),
она об этом скажет и ничего не запишет. После распаковки проверяется второй
CRC-32 -- самого файла.

Зачем: .BCK теряет атрибуты записей, пройдя через FTP, почту, веб-сервер
или флешку. .EXE в формате "Fixed 512" проходит без потерь, а атрибуты
сейвсета едут внутри него.

Устройство кита
---------------

    VBN 1 .. S        заглушка YADCX_SFX.EXE (программа восстановления)
    VBN S+1           заголовок кита, один блок 512 байт
    VBN S+2 .. S+1+D  хранимые данные, D блоков

Хранимые данные зависят от сжатия (/COMPRESSION):

- DCX (по умолчанию): карта DCX, затем файл кусками по 32256 байт
  (63 блока), каждый сжат -- или лежит как есть, если DCX его не уменьшает.
  Если весь файл меньше не становится, кит делается как NONE.
- NONE: файл блок в блок, как лежит на диске.

В заголовке: маркер yaDCX-KIT-HEADER, версия формата (2), размер файла в
байтах и блоках, 32 байта RMS-атрибутов записей (FAT), время сборки, версия
сборщика, имя файла, метод, размер хранимых данных, два CRC-32 (хранимых
данных и самого файла) и CRC-32 самого заголовка. Все числа little-endian.
CRC-32 -- тот же, что zlib.crc32(), так что кит можно проверить и на Linux:
python3 tools/yadcx_check.py MYAPP.EXE (кит забирать в двоичном режиме;
код возврата 0 -- всё в порядке). Скрипт проверяет хранимые данные; файл из
DCX-кита распаковывается и проверяется только на VMS (KIT /VERIFY).
Киты формата 1 (yaDCX X-01) по-прежнему читаются.

Сборка
------

Нужен HP C (проверено на V7.1) на OpenVMS Alpha.

    $ SET DEFAULT dev:[dir]           ! каталог с исходниками
    $ @YADCX_BUILD                    ! собирает YADCX.EXE и YADCX_SFX.EXE
    $ @YADCX                          ! определяет команду YADCX

YADCX.EXE и YADCX_SFX.EXE держите в одном каталоге: сборщик берёт
заглушку из своего каталога.

Как сделать кит
---------------

    $ BACKUP [MYAPP...]*.* MYAPP.BCK/SAVE_SET
    $ YADCX MYAPP.BCK                 ! получится MYAPP.EXE в текущем каталоге

  /OUTPUT=файл
        куда положить кит. По умолчанию <имя нагрузки>.EXE в текущем
        каталоге
  /NAME=имя.тип
        имя файла, которое запишется в кит. По умолчанию -- собственное имя
        нагрузки. Без устройства, каталога и версии
  /STUB=файл
        другая заглушка. По умолчанию YADCX_SFX.EXE рядом с YADCX.EXE
  /COMPRESSION=DCX
        сжать процедурами VMS DCX -- по умолчанию
  /COMPRESSION=NONE
        не сжимать (кит больше, сборка быстрее)
  /LIST
        не собирать, а показать кит и полностью проверить (оба CRC): YADCX
        /LIST MYAPP.EXE

Как пользоваться китом
----------------------

Копируйте кит в двоичном режиме (FTP: binary или type i). Потом:

    $ RUN MYAPP.EXE                       ! запишет MYAPP.BCK в текущий каталог
    $ KIT :== $DKA0:[WORK]MYAPP.EXE       ! для квалификаторов - как команду
    $ KIT /LIST                           ! показать, что внутри
    $ KIT /VERIFY                         ! распаковать в памяти, проверить оба CRC, ничего не писать
    $ KIT /OUTPUT=[.SUB]OTHER.BCK         ! записать под другим именем

Пример работы
-------------

YADCX_DEMO.COM показывает всё на примерах. Запустите сами (работает в новом
каталоге, по умолчанию [.YADCX_DEMO], больше ничего не трогает):

    $ @dev:[dir]YADCX_DEMO                       ! dev:[dir] - где собран yaDCX
    $ @dev:[dir]YADCX_DEMO/OUTPUT=DEMO.LOG       ! то же, с файлом протокола

Реальный протокол -- в разделе Example session выше. Строки с $ -- команды,
остальное -- их вывод. ATTR.COM печатает атрибуты RMS файла одной строкой.

Что в нём показано:

1. три файла: сейвсет BACKUP (Fixed 32256), текст (Variable, CR), образ (Fixed 512);
2. кит для каждого (со сжатием DCX), один с /OUTPUT, другой с /NAME;
   сейвсет ещё раз с /COMPRESSION=NONE -- сравнить размеры;
3. YADCX /LIST показывает, что внутри кита;
4. кит копируется под другим именем и запускается: сейвсет возвращается с теми же атрибутами,
   DIFFERENCES различий не находит, BACKUP восстанавливает из него файлы;
5. /VERIFY и /OUTPUT у кита;
6. испорченные киты, их делает сама демо-процедура: один изменённый байт (CRCERR, ничего
   не записано), потерянный конец (TRUNCATED), кит, превращённый в текстовый файл (VMS его не запускает);
7. типичные ошибки и их сообщения;
8. та же проверка CRC на Linux (tools/yadcx_check.py).

Сообщения и что делать
----------------------

После каждого запуска выставляется $STATUS, его можно проверять в
командной процедуре.

  BUILT (S)
        кит готов -- ничего
  RESTORED (S)
        файл записан, CRC верный -- ничего
  VERIFIED (S)
        кит цел, файл вернётся без искажений (DCX-кит распакован в памяти);
        ничего не записано -- ничего
  KITINFO, KITCRC, KITDCX, PAYLOAD, COMPRESSED (I)
        справка -- ничего
  NOGAIN (I)
        DCX не смог уменьшить файл (уже сжатые или зашифрованные данные),
        кит сделан без сжатия -- ничего; /COMPRESSION=NONE даст тот же кит
        быстрее
  NOHEADER (F)
        в файле нет заголовка кита -- запустили голую заглушку YADCX_SFX.EXE
        или это не кит yaDCX. Запускайте кит
  BADHDR (F)
        заголовок испорчен -- получите кит заново, в двоичном режиме
  BADVER (F)
        кит сделан более новой версией yaDCX -- возьмите новую версию у
        автора кита
  BADMETH (F)
        кит сжат методом, которого эта программа не знает -- для /LIST
        возьмите новый YADCX.EXE; сам кит распаковывается своей заглушкой
  TRUNCATED (F)
        у кита нет конца -- скопируйте заново в двоичном режиме; проверьте
        место на диске и дисковую квоту
  CRCERR (F)
        данные в ките испорчены, ничего не записано -- скопируйте заново в
        двоичном режиме. Обычная причина -- FTP в текстовом режиме
  EXPCRC (F)
        распакованный файл не совпал с исходным, новый файл удалён -- с
        целым китом так не бывает: запустите KIT /VERIFY, получите кит
        заново; если повторяется -- сообщите авторам
  NESTED (F)
        файл из /STUB уже является китом -- укажите голую YADCX_SFX.EXE
  BADNAME (F)
        в /NAME есть устройство, каталог или версия -- дайте только имя.тип;
        место выбирают через /OUTPUT при запуске кита
  EMPTY (F)
        файл нагрузки пустой -- проверьте файл, возможно BACKUP не удался
  OPENIN (E)
        не открывается файл -- смотрите вторую строку (-RMS-E-FNF -- нет
        файла, -RMS-E-PRV -- нет прав)
  OPENOUT (E)
        не создаётся выходной файл -- смотрите вторую строку (-RMS-E-DNF --
        нет каталога, -RMS-E-PRV, диск полон)
  READERR, WRITEERR (E)
        ошибка ввода-вывода -- смотрите вторую строку; проверьте диск (SHOW
        DEVICE, ANALYZE/DISK)
  ATTRERR (E)
        не читаются или не ставятся атрибуты записей -- смотрите вторую
        строку; диск должен быть ODS-2 или ODS-5
  SYSERR (E)
        отказ системного сервиса -- смотрите вторую строку; сообщите авторам
  DCXERR (E)
        ошибка процедуры DCX при сжатии или распаковке -- смотрите вторую
        строку (например -LIB-F-INSVIRMEM -- не хватает памяти, увеличьте
        квоту файла подкачки); при сборке /COMPRESSION=NONE обходит DCX
  DCL-W-ACTIMAGE (с IMGACT-F-NOTNATIVE, SYSTEM-F-PROTINSTALL или другой
    причиной)
        VMS вообще не запускает кит -- кит передан в текстовом (ASCII)
        режиме; скопируйте заново в двоичном
  CLI-W-CONFLICT
        недопустимое сочетание квалификаторов -- /LIST -- только один;
        /OUTPUT не сочетается с /LIST и /VERIFY

При неудачном восстановлении недописанный файл не остаётся -- он удаляется.
