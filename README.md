# yaDCX — self-extracting kits for OpenVMS

*Русский текст — ниже, после английского.*

Version X-01-00 (01.00.00). Authors: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).

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
| `AAAREADME.TXT` | this guide, for VMS users |

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
$ YADCX MYAPP.BCK
%YADCX-I-PAYLOAD, payload DKA0:[WORK]MYAPP.BCK;1, 1645056 bytes (3213 blocks), CRC-32 95DEB4C2
%YADCX-S-BUILT, kit DKA0:[WORK]MYAPP.EXE;1 built, 33 stub + 1 header + 3213 payload blocks
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

```python
import zlib, struct
k = open('MYAPP.EXE', 'rb').read()
h = k.find(b'yaDCX-KIT-HEADER')                 # always at a 512-byte boundary
crc, size = struct.unpack_from('<IQ', k, h + 20)
print(zlib.crc32(k[h + 512 : h + 512 + size]) == crc)
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
(полином EDB88320), так что кит можно проверить и на Linux (пример выше).

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
| `CLI-W-CONFLICT` | недопустимое сочетание квалификаторов | `/LIST` — только один; `/OUTPUT` не сочетается с `/LIST` и `/VERIFY` |

При неудачном восстановлении недописанный файл не остаётся — он удаляется.
