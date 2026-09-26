# yaDCX — self-extracting kits for OpenVMS

*Русский текст — ниже, после английского.*

Version X-01-00ECO01 (01.00.01). Authors: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).

## What it is

yaDCX turns one OpenVMS file (usually a BACKUP saveset, `.BCK`) into one
program (`.EXE`). You copy that program to another OpenVMS system and run it.
The program writes the original file back — the same bytes, the same record
format, the same end of file.

Before it writes anything, the program checks a CRC-32 of the data. If the
kit was damaged on the way (for example, sent over FTP in text mode), it
says so and writes nothing.

Why: a `.BCK` file loses its record attributes when it goes through FTP,
mail, a web server or a USB stick. An `.EXE` in the "Fixed 512" format goes
through safely, and the attributes of the saveset travel *inside* it.

## How a kit looks inside

```
VBN 1 .. S        the stub, YADCX_SFX.EXE (the program that restores)
VBN S+1           the kit header, one 512-byte block
VBN S+2 .. S+1+P  the payload, block for block
```

The header holds: the marker `yaDCX-KIT-HEADER`, the format version, the
exact size in bytes, the size in blocks, the 32-byte RMS record attributes
(FAT) of the payload, the build time, the builder version, the file name,
the CRC-32 of the payload and the CRC-32 of the header itself. All numbers
are little-endian. The CRC-32 is the same as `zlib.crc32()` (polynomial
EDB88320), so you can check a kit on Linux too.

## Files

| File | What it is |
|---|---|
| `YADCX.C` | the kit builder, `YADCX.EXE` (main module) |
| `YADCX_SFX.C` | the stub, `YADCX_SFX.EXE` |
| `YADCX_COMMON.C` | code used by both: block I/O, FAT, CRC, messages, command line |
| `YADCX.H` | the kit format and the prototypes |
| `YADCX_MSG.MSG` | messages (facility `YADCX`, number 1100) |
| `YADCX_CLI.CLD` | command syntax (verbs `YADCX` and `SFX`) |
| `YADCX_BUILD.COM` | builds both images |
| `YADCX.COM` | defines the `YADCX` command |
| `YADCX_DEMO.COM` | the demo, see "Example session" |
| `ATTR.COM` | prints the RMS attributes of a file in one line (used by the demo) |
| `AAAREADME.TXT` | this guide, for VMS users |
| `tools/yadcx_check.py` | checks kits on Linux (or anywhere with Python 3) |

## Build

You need HP C (tested with V7.1) on OpenVMS Alpha.

```
$ SET DEFAULT dev:[dir]           ! the directory with the sources
$ @YADCX_BUILD                    ! builds YADCX.EXE and YADCX_SFX.EXE
$ @YADCX                          ! defines the YADCX command
```

Keep `YADCX.EXE` and `YADCX_SFX.EXE` in the same directory: the builder
takes the stub from its own directory.

## Make a kit

```
$ BACKUP [MYAPP...]*.* MYAPP.BCK/SAVE_SET
$ YADCX MYAPP.BCK                 ! makes MYAPP.EXE in the current directory
```

Qualifiers:

| Qualifier | Meaning |
|---|---|
| `/OUTPUT=file` | where to put the kit. Default: `<payload name>.EXE` in the current directory |
| `/NAME=name.type` | the file name stored in the kit. Default: the payload's own name. No device, directory or version |
| `/STUB=file` | another stub. Default: `YADCX_SFX.EXE` next to `YADCX.EXE` |
| `/LIST` | do not build: show a kit and check its CRC: `YADCX /LIST MYAPP.EXE` |

## Use a kit

Copy the kit in **binary** mode (FTP: `binary` or `type i`). Then:

```
$ RUN MYAPP.EXE                       ! writes MYAPP.BCK to the current directory
```

With qualifiers, define it as a command first:

```
$ KIT :== $DKA0:[WORK]MYAPP.EXE
$ KIT /LIST                           ! show what is inside
$ KIT /VERIFY                         ! check the CRC, write nothing
$ KIT /OUTPUT=[.SUB]OTHER.BCK         ! write under another name
```

## Check a kit on Linux

Fetch the kit in binary mode, then:

```
$ python3 tools/yadcx_check.py SRC.EXE
SRC.EXE: header at VBN 35, 258048 bytes, recorded C5EC8DE2, computed C5EC8DE2 -> OK
```

The exit status is 0 when all kits are OK. The same check in a few lines:

```python
import zlib, struct
k = open('MYAPP.EXE', 'rb').read()
h = k.find(b'yaDCX-KIT-HEADER')                 # always at a 512-byte boundary
crc, size = struct.unpack_from('<IQ', k, h + 20)
print(zlib.crc32(k[h + 512 : h + 512 + size]) == crc)
```

## Example session

`YADCX_DEMO.COM` shows everything on sample files. Run it yourself (it works in a
new directory, `[.YADCX_DEMO]` by default, and touches nothing else):

```
$ @dev:[dir]YADCX_DEMO                       ! dev:[dir] is where yaDCX is built
$ @dev:[dir]YADCX_DEMO/OUTPUT=DEMO.LOG       ! the same, with a protocol file
```

Below is a real protocol. Lines that start with `$` are commands (`SET VERIFY`),
the rest is what they print. `ATTR.COM` prints the RMS attributes of a file in
one line.

What it shows:

1. three sample files: a BACKUP saveset (Fixed 32256), a text file (Variable, CR), an image (Fixed 512);
2. a kit for each of them, one with `/OUTPUT`, one with `/NAME`;
3. `YADCX /LIST` looks into a kit;
4. the kit is copied under another name and run: the saveset comes back with the same attributes,
   `DIFFERENCES` finds nothing, and BACKUP restores the files from it;
5. `/VERIFY` and `/OUTPUT` of a kit;
6. damaged kits, made by the demo itself: one changed byte (`CRCERR`, nothing written),
   a lost end (`TRUNCATED`), the kit turned into a text file (VMS can not start it);
7. typical mistakes and their messages;
8. the same CRC check on Linux (`tools/yadcx_check.py`).

```
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
  SRC.BCK          rfm=FIX mrs=32256 rat=none bytes=258048
$ @DQB0:[LAISHEV.YADCX]ATTR NOTES.TXT
  NOTES.TXT        rfm=VAR mrs=    0 rat=CR   bytes=26306
$ @DQB0:[LAISHEV.YADCX]ATTR TOOL.EXE
  TOOL.EXE         rfm=FIX mrs=  512 rat=none bytes=19968
$ !
$ ! --- 2. Build kits
$ YADCX SRC.BCK
%YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]SRC.BCK;1, 258048 bytes (504 blocks), CRC-32 939EEF3C
%YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO]SRC.EXE;1 built, 34 stub + 1 header + 504 payload blocks
$ SHOW SYMBOL $STATUS
  $STATUS == "%X1C4C8009"
$ YADCX NOTES.TXT /OUTPUT=[.KITS]NOTESKIT
%YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]NOTES.TXT;1, 26306 bytes (52 blocks), CRC-32 6344FE6D
%YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO.KITS]NOTESKIT.EXE;1 built, 34 stub + 1 header + 52 payload blocks
$ YADCX TOOL.EXE /OUTPUT=[.KITS]TOOLKIT /NAME=NEWTOOL.EXE
%YADCX-I-PAYLOAD, payload DQB0:[LAISHEV.YADCX_DEMO]TOOL.EXE;1, 19968 bytes (39 blocks), CRC-32 049E9B2F
%YADCX-S-BUILT, kit DQB0:[LAISHEV.YADCX_DEMO.KITS]TOOLKIT.EXE;1 built, 34 stub + 1 header + 39 payload blocks
$ DIRECTORY/SIZE SRC.EXE,[.KITS]*.EXE

Directory DQB0:[LAISHEV.YADCX_DEMO]

SRC.EXE;1                539

Total of 1 file, 539 blocks.

Directory DQB0:[LAISHEV.YADCX_DEMO.KITS]

NOTESKIT.EXE;1            87
TOOLKIT.EXE;1             74

Total of 2 files, 161 blocks.

Grand total of 2 directories, 3 files, 700 blocks.
$ !
$ ! --- 3. Look into a kit without unpacking it
$ YADCX /LIST SRC.EXE
%YADCX-I-KITINFO, kit contains SRC.BCK, 258048 bytes (504 blocks), built 26-SEP-2026 12:53:59.83 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 939EEF3C
%YADCX-S-VERIFIED, payload CRC-32 939EEF3C verified, nothing written
$ !
$ ! --- 4. "Another system": copy the kit under another name and run it
$ COPY SRC.EXE [.OTHER]MYSOURCES.EXE
$ SET DEFAULT [.OTHER]
$ RUN MYSOURCES.EXE
%YADCX-I-KITINFO, kit contains SRC.BCK, 258048 bytes (504 blocks), built 26-SEP-2026 12:53:59.83 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 939EEF3C
%YADCX-S-RESTORED, DQB0:[LAISHEV.YADCX_DEMO.OTHER]SRC.BCK;1 restored, 258048 bytes, CRC-32 939EEF3C verified
$ SHOW SYMBOL $STATUS
  $STATUS == "%X1C4C8011"
$ @DQB0:[LAISHEV.YADCX]ATTR SRC.BCK
  SRC.BCK          rfm=FIX mrs=32256 rat=none bytes=258048
$ DIFFERENCES [-]SRC.BCK []SRC.BCK
Number of difference sections found: 0
Number of difference records found: 0

DIFFERENCES /IGNORE=()/MERGED=1-
    DQB0:[LAISHEV.YADCX_DEMO]SRC.BCK;1-
    DQB0:[LAISHEV.YADCX_DEMO.OTHER]SRC.BCK;1
$ BACKUP SRC.BCK/SAVE_SET [.FILES]
$ DIRECTORY/SIZE [.FILES]

Directory DQB0:[LAISHEV.YADCX_DEMO.OTHER.FILES]

AAAREADME.TXT;3           52
ATTR.COM;1                 3
YADCX.C;4                 24
YADCX.COM;4                2
YADCX.H;4                 14
YADCX_BUILD.COM;1          4
YADCX_CLI.CLD;2            4
YADCX_COMMON.C;5          46
YADCX_DEMO.COM;1          11
YADCX_MSG.MSG;2            5
YADCX_SFX.C;5             13

Total of 11 files, 178 blocks.
$ DIFFERENCES [.FILES]YADCX.C DQB0:[LAISHEV.YADCX]YADCX.C
Number of difference sections found: 0
Number of difference records found: 0

DIFFERENCES /IGNORE=()/MERGED=1-
    DQB0:[LAISHEV.YADCX_DEMO.OTHER.FILES]YADCX.C;4-
    DQB0:[LAISHEV.YADCX]YADCX.C;4
$ !
$ ! --- 5. Qualifiers of a kit: /VERIFY, /OUTPUT; a name given with /NAME
$ K := $DQB0:[LAISHEV.YADCX_DEMO.KITS]NOTESKIT.EXE;
$ K /VERIFY
%YADCX-I-KITINFO, kit contains NOTES.TXT, 26306 bytes (52 blocks), built 26-SEP-2026 12:54:00.36 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 6344FE6D
%YADCX-S-VERIFIED, payload CRC-32 6344FE6D verified, nothing written
$ K /OUTPUT=README_COPY.TXT
%YADCX-I-KITINFO, kit contains NOTES.TXT, 26306 bytes (52 blocks), built 26-SEP-2026 12:54:00.36 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 6344FE6D
%YADCX-S-RESTORED, DQB0:[LAISHEV.YADCX_DEMO.OTHER]README_COPY.TXT;1 restored, 26306 bytes, CRC-32 6344FE6D verified
$ K := $DQB0:[LAISHEV.YADCX_DEMO.KITS]TOOLKIT.EXE;
$ K
%YADCX-I-KITINFO, kit contains NEWTOOL.EXE, 19968 bytes (39 blocks), built 26-SEP-2026 12:54:00.76 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 049E9B2F
%YADCX-S-RESTORED, DQB0:[LAISHEV.YADCX_DEMO.OTHER]NEWTOOL.EXE;1 restored, 19968 bytes, CRC-32 049E9B2F verified
$ @DQB0:[LAISHEV.YADCX]ATTR README_COPY.TXT
  README_COPY.TXT  rfm=VAR mrs=    0 rat=CR   bytes=26306
$ @DQB0:[LAISHEV.YADCX]ATTR NEWTOOL.EXE
  NEWTOOL.EXE      rfm=FIX mrs=  512 rat=none bytes=19968
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
%YADCX-I-KITINFO, kit contains SRC.BCK, 258048 bytes (504 blocks), built 26-SEP-2026 12:53:59.83 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 939EEF3C
%YADCX-F-CRCERR, payload CRC-32 mismatch: recorded 939EEF3C, computed 3F9D5021 -- the kit is damaged, nothing written
$ SHOW SYMBOL $STATUS
  $STATUS == "%X1C4C805C"
$ DIRECTORY SRC.BCK
%DIRECT-W-NOFILES, no files found
$ !        SHORT.EXE: the last 100 blocks lost
$ COPY [-]SRC.EXE SHORT.EXE
$ l_ebk = F$FILE_ATTRIBUTES("SHORT.EXE","EOF") - 99
$ SET FILE/ATTRIBUTE=(EBK:440,FFB:0) SHORT.EXE
$ RUN SHORT.EXE
%YADCX-I-KITINFO, kit contains SRC.BCK, 258048 bytes (504 blocks), built 26-SEP-2026 12:53:59.83 by yaDCX X-01-00ECO01
%YADCX-I-KITCRC, recorded CRC-32 of the payload is 939EEF3C
%YADCX-F-TRUNCATED, the kit is truncated: 539 blocks expected, 439 present -- transfer it again in binary mode
$ !        TEXT.EXE: the kit turned into a text file, as a text-mode transfer does
$ CREATE TEXT.FDL
RECORD
	FORMAT	STREAM_LF
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
%YADCX-F-NOHEADER, no yaDCX kit header in DQB0:[LAISHEV.YADCX]YADCX_SFX.EXE;5 -- this is the bare stub or not a yaDCX kit
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

# ---- Linux: SRC.EXE and BROKEN/BAD.EXE fetched in binary mode ----
$ python3 tools/yadcx_check.py SRC.EXE BAD.EXE
SRC.EXE: header at VBN 35, 258048 bytes, recorded 939EEF3C, computed 939EEF3C -> OK
BAD.EXE: header at VBN 35, 258048 bytes, recorded 939EEF3C, computed 3F9D5021 -> DAMAGED
```

A kit really sent by FTP in text (ASCII) mode gave, in another run:

```
$ RUN TEXTMODE.EXE
%DCL-W-ACTIMAGE, error activating image TEXTMODE.EXE
-CLI-E-IMGNAME, image file DQB0:[LAISHEV.YADCX.DEMO.BROKEN]TEXTMODE.EXE;1
-IMGACT-F-NOTNATIVE, image is not an OpenVMS Alpha image
```

## Messages and what to do

`$STATUS` is set after every run, so you can test it in a command procedure.

| Message | What it means | What to do |
|---|---|---|
| `BUILT` (S) | the kit is ready | nothing |
| `RESTORED` (S) | the file is written and its CRC is right | nothing |
| `VERIFIED` (S) | the CRC is right, nothing was written | nothing |
| `KITINFO`, `KITCRC`, `PAYLOAD` (I) | information | nothing |
| `NOHEADER` (F) | the file has no kit header | you ran the bare `YADCX_SFX.EXE`, or the file is not a yaDCX kit. Run the kit, not the stub |
| `BADHDR` (F) | the header is damaged | get the kit again, in binary mode |
| `BADVER` (F) | the kit was made by a newer yaDCX | get a newer `YADCX_SFX.EXE`/kit from the author of the kit |
| `TRUNCATED` (F) | the end of the kit is missing | copy it again, in binary mode; check free disk space and disk quota on the way |
| `CRCERR` (F) | the data in the kit is damaged; nothing was written | copy it again, in binary mode. Text mode FTP is the usual cause |
| `NESTED` (F) | the `/STUB` file is already a kit | use the bare `YADCX_SFX.EXE` as the stub |
| `BADNAME` (F) | `/NAME` has a device, directory or version | give only `name.type`; use `/OUTPUT` when you run the kit to choose a place |
| `EMPTY` (F) | the payload file has 0 bytes | check the file, maybe BACKUP failed |
| `OPENIN` (E) | a file can not be opened | read the second line (`-RMS-E-FNF` = no such file, `-RMS-E-PRV` = no privilege) |
| `OPENOUT` (E) | the output file can not be created | read the second line (`-RMS-E-DNF` = no such directory, `-RMS-E-PRV`, disk full) |
| `READERR`, `WRITEERR` (E) | an I/O error | read the second line; check the disk (`SHOW DEVICE`, `ANALYZE/DISK`) |
| `ATTRERR` (E) | record attributes can not be read or set | read the second line; check that the disk is ODS-2 or ODS-5 |
| `SYSERR` (E) | a system service failed | read the second line; tell the authors |
| `DCL-W-ACTIMAGE` (with `IMGACT-F-NOTNATIVE`, `SYSTEM-F-PROTINSTALL` or another reason) | VMS can not start the kit at all | the kit was sent in text (ASCII) mode; copy it again in binary mode |
| `CLI-W-CONFLICT` | bad mix of qualifiers | `/LIST` goes alone; `/OUTPUT` does not go with `/LIST` or `/VERIFY` |

A failed restore never leaves a half-written file: it is deleted.

---

# yaDCX — самораспаковывающиеся киты для OpenVMS

## Что это

yaDCX превращает один файл OpenVMS (обычно сейвсет BACKUP, `.BCK`) в одну
программу (`.EXE`). Эту программу копируют на другую систему OpenVMS и
запускают. Программа записывает исходный файл обратно: те же байты, тот же
формат записей, тот же конец файла.

До того как что-то записать, программа проверяет CRC-32 данных. Если кит
испортился по дороге (например, его передали по FTP в текстовом режиме),
она об этом скажет и ничего не запишет.

Зачем: `.BCK` теряет атрибуты записей, пройдя через FTP, почту, веб-сервер
или флешку. `.EXE` в формате «Fixed 512» проходит без потерь, а атрибуты
сейвсета едут *внутри* него.

## Устройство кита

```
VBN 1 .. S        заглушка YADCX_SFX.EXE (программа восстановления)
VBN S+1           заголовок кита, один блок 512 байт
VBN S+2 .. S+1+P  полезная нагрузка, блок в блок
```

В заголовке: маркер `yaDCX-KIT-HEADER`, версия формата, точный размер в
байтах и в блоках, 32 байта RMS-атрибутов записей (FAT) нагрузки, время
сборки, версия сборщика, имя файла, CRC-32 нагрузки и CRC-32 самого
заголовка. Все числа little-endian. CRC-32 — тот же, что `zlib.crc32()`
(полином EDB88320), так что кит можно проверить и на Linux:
`python3 tools/yadcx_check.py MYAPP.EXE` (кит забирать в двоичном режиме;
код возврата 0 — всё в порядке).

## Сборка

Нужен HP C (проверено на V7.1) на OpenVMS Alpha.

```
$ SET DEFAULT dev:[dir]           ! каталог с исходниками
$ @YADCX_BUILD                    ! собирает YADCX.EXE и YADCX_SFX.EXE
$ @YADCX                          ! определяет команду YADCX
```

`YADCX.EXE` и `YADCX_SFX.EXE` держите в одном каталоге: сборщик берёт
заглушку из своего каталога.

## Как сделать кит

```
$ BACKUP [MYAPP...]*.* MYAPP.BCK/SAVE_SET
$ YADCX MYAPP.BCK                 ! получится MYAPP.EXE в текущем каталоге
```

| Квалификатор | Смысл |
|---|---|
| `/OUTPUT=файл` | куда положить кит. По умолчанию `<имя нагрузки>.EXE` в текущем каталоге |
| `/NAME=имя.тип` | имя файла, которое запишется в кит. По умолчанию — собственное имя нагрузки. Без устройства, каталога и версии |
| `/STUB=файл` | другая заглушка. По умолчанию `YADCX_SFX.EXE` рядом с `YADCX.EXE` |
| `/LIST` | не собирать, а показать кит и проверить CRC: `YADCX /LIST MYAPP.EXE` |

## Как пользоваться китом

Копируйте кит в **двоичном** режиме (FTP: `binary` или `type i`). Потом:

```
$ RUN MYAPP.EXE                       ! запишет MYAPP.BCK в текущий каталог
$ KIT :== $DKA0:[WORK]MYAPP.EXE       ! для квалификаторов - как команду
$ KIT /LIST                           ! показать, что внутри
$ KIT /VERIFY                         ! проверить CRC, ничего не писать
$ KIT /OUTPUT=[.SUB]OTHER.BCK         ! записать под другим именем
```

## Пример работы

`YADCX_DEMO.COM` показывает всё на примерах. Запустите сами (работает в новом
каталоге, по умолчанию `[.YADCX_DEMO]`, больше ничего не трогает):

```
$ @dev:[dir]YADCX_DEMO                       ! dev:[dir] - где собран yaDCX
$ @dev:[dir]YADCX_DEMO/OUTPUT=DEMO.LOG       ! то же, с файлом протокола
```

Реальный протокол — в разделе **Example session** выше. Строки с `$` — команды,
остальное — их вывод. `ATTR.COM` печатает атрибуты RMS файла одной строкой.

Что в нём показано:

1. три файла: сейвсет BACKUP (Fixed 32256), текст (Variable, CR), образ (Fixed 512);
2. кит для каждого, один с `/OUTPUT`, другой с `/NAME`;
3. `YADCX /LIST` показывает, что внутри кита;
4. кит копируется под другим именем и запускается: сейвсет возвращается с теми же атрибутами,
   `DIFFERENCES` различий не находит, BACKUP восстанавливает из него файлы;
5. `/VERIFY` и `/OUTPUT` у кита;
6. испорченные киты, их делает сама демо-процедура: один изменённый байт (`CRCERR`, ничего
   не записано), потерянный конец (`TRUNCATED`), кит, превращённый в текстовый файл (VMS его не запускает);
7. типичные ошибки и их сообщения;
8. та же проверка CRC на Linux (`tools/yadcx_check.py`).

## Сообщения и что делать

После каждого запуска выставляется `$STATUS`, его можно проверять в
командной процедуре.

| Сообщение | Что значит | Что делать |
|---|---|---|
| `BUILT` (S) | кит готов | ничего |
| `RESTORED` (S) | файл записан, CRC верный | ничего |
| `VERIFIED` (S) | CRC верный, ничего не записано | ничего |
| `KITINFO`, `KITCRC`, `PAYLOAD` (I) | справка | ничего |
| `NOHEADER` (F) | в файле нет заголовка кита | запустили голую заглушку `YADCX_SFX.EXE` или это не кит yaDCX. Запускайте кит |
| `BADHDR` (F) | заголовок испорчен | получите кит заново, в двоичном режиме |
| `BADVER` (F) | кит сделан более новой версией yaDCX | возьмите новую версию у автора кита |
| `TRUNCATED` (F) | у кита нет конца | скопируйте заново в двоичном режиме; проверьте место на диске и дисковую квоту |
| `CRCERR` (F) | данные в ките испорчены, ничего не записано | скопируйте заново в двоичном режиме. Обычная причина — FTP в текстовом режиме |
| `NESTED` (F) | файл из `/STUB` уже является китом | укажите голую `YADCX_SFX.EXE` |
| `BADNAME` (F) | в `/NAME` есть устройство, каталог или версия | дайте только `имя.тип`; место выбирают через `/OUTPUT` при запуске кита |
| `EMPTY` (F) | файл нагрузки пустой | проверьте файл, возможно BACKUP не удался |
| `OPENIN` (E) | не открывается файл | смотрите вторую строку (`-RMS-E-FNF` — нет файла, `-RMS-E-PRV` — нет прав) |
| `OPENOUT` (E) | не создаётся выходной файл | смотрите вторую строку (`-RMS-E-DNF` — нет каталога, `-RMS-E-PRV`, диск полон) |
| `READERR`, `WRITEERR` (E) | ошибка ввода-вывода | смотрите вторую строку; проверьте диск (`SHOW DEVICE`, `ANALYZE/DISK`) |
| `ATTRERR` (E) | не читаются или не ставятся атрибуты записей | смотрите вторую строку; диск должен быть ODS-2 или ODS-5 |
| `SYSERR` (E) | отказ системного сервиса | смотрите вторую строку; сообщите авторам |
| `DCL-W-ACTIMAGE` (с `IMGACT-F-NOTNATIVE`, `SYSTEM-F-PROTINSTALL` или другой причиной) | VMS вообще не запускает кит | кит передан в текстовом (ASCII) режиме; скопируйте заново в двоичном |
| `CLI-W-CONFLICT` | недопустимое сочетание квалификаторов | `/LIST` — только один; `/OUTPUT` не сочетается с `/LIST` и `/VERIFY` |

При неудачном восстановлении недописанный файл не остаётся — он удаляется.
