# Contributing to yaDCX

*Русский текст — ниже, после английского.*

Thank you for helping. yaDCX is OpenVMS code written in the house style of
StarLet Squad. The style is **not optional**: a change that works but does
not follow it will be asked to be reformatted. This file tells you everything
you need; the sources themselves are the best examples.

AI agents: read [`AGENTS.md`](AGENTS.md) as well.

## 1. Before you start

- You need OpenVMS Alpha (V8.3 is the reference) and HP C (V7.1 is the
  reference). HP C V7.1 has **no `<stdint.h>`**: use `<inttypes.h>`, it gives
  `uint32_t`, `uint64_t` and friends.
- Build with `@YADCX_BUILD` (`@YADCX_BUILD DEBUG` for a debug build). The build
  must end with `%YADCX_BUILD-S-DONE` and show **no** `%CC-W-`/`%CC-E-` or
  `%LINK-` messages.
- Run `@YADCX_DEMO` after every change. Every step must end as in the protocol
  in README.md (`Example session`): kits restore byte-identical, damaged kits
  are refused, nothing is written on a bad CRC.

## 2. Files and headers

Every file starts with a header carrying `__MODULE__`, `__IDENT__`, `__REV__`,
the authors and an OpenVMS-style `MODIFICATION HISTORY`:

```c
#define	__MODULE__	"YADCX_COMMON"
#define	__IDENT__	"X-01-00ECO01"
#define	__REV__		"01.00.01"

/*
**++
**  FACILITY:	yaDCX -- Yet Another Data Compression/eXpansion kit builder
**
**  MODULE DESCRIPTION:
**
**	What the module is for, in a few lines.
**
**  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  26-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	26-SEP-2026	CLD	X-01-00: initial version.
**
**	26-SEP-2026	CLD	X-01-00ECO01: yadcx$putmsg_rms() - a failed
**				$OPEN/$CREATE shows its STS and STV.
**--
*/
```

- `AUTHORS` is always *StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)*.
- In the history, sign with **your own** three-letter initials. `RRL` is the
  maintainer's own mark — never use it. `CLD` marks changes made by Claude.
- DCL procedures (`.COM`, `.CLD`, `.MSG`) carry the same header as `$!` / `!`
  comments.

### IDENT and REV

- `__IDENT__` is VMS style: `X-aa-bb`, a bug fix adds `ECOcc`: `X-01-00ECO01`.
- `__REV__` is the Unix form made from it: `aa.bb.cc` → `01.00.01`.
- Bump **both** together, in every file you change, and add a history line.
- The version of the product is the one of the head module `YADCX.C`; it is
  written into every kit header, so it must be right.
- Archives are named `<package>-<ident>.tar.gz`: `yaDCX-X-01-00ECO01.tar.gz`.

## 3. Formatting

- **Tabs only**, tab width 8. No spaces for indentation or alignment —
  anywhere, including continuation lines.
- Lines may be up to **192** columns; do not wrap a condition just to fit 80.
- **Whitesmiths** braces: the brace goes on its own line, indented to the level
  of the block it opens. The outer braces of a routine body are **not**
  indented. Structure braces are at column 0, members are indented.
- A single statement under `if`/`else`/`for` takes **no** braces.
- Declarations first, at column 0, grouped by type. Loop counters go inline:
  `for (int i = 0; i < n; i++)`.

A routine, exactly as in `YADCX_COMMON.C`:

```c
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Read/write whole blocks at a given VBN, in chunks of YADCX$K_IOBLKS.
**
**  INPUTS:
**
**	a_file	-	file context from yadcx$open()/yadcx$create()
**	a_vbn	-	first virtual block, 1-based
**	a_buf	-	buffer of a_blocks * 512 bytes
**	a_blocks -	number of blocks
**
**  RETURNS:
**
**	condition value of the $QIOW or of its IOSB
**--
*/
static unsigned int	s_xfer	(
		YADCX$FILE	*a_file,
		unsigned int	a_func,
		uint32_t	a_vbn,
		char		*a_buf,
		uint32_t	a_blocks
			)
{
unsigned int	l_sts;
uint32_t	l_n;
IOSB		l_iosb;

	for ( ; a_blocks; a_blocks -= l_n, a_vbn += l_n, a_buf += l_n * YADCX$K_BLKSZ)
		{
		l_n = (a_blocks > YADCX$K_IOBLKS) ? YADCX$K_IOBLKS : a_blocks;

		l_sts = sys$qiow(EFN$C_ENF, a_file->chan, a_func, &l_iosb, 0, 0, a_buf, l_n * YADCX$K_BLKSZ, a_vbn, 0, 0, 0);
		if ( 1 & l_sts )
			l_sts = l_iosb.iosb$w_status;
		if ( !(1 & l_sts) )
			return	l_sts;
		}

	return	SS$_NORMAL;
}
```

A structure, exactly as in `YADCX.H`:

```c
typedef struct yadcx_file_t {
	struct FAB	fab;
	struct NAM	nam;
	char		rsa[NAM$C_MAXRSS];	/* Resultant file name buffer				*/
	struct dsc$descriptor_s	rsadsc;		/* Resultant file name, NAM$B_RSL long		*/
	uint16_t	chan;			/* Channel of the UFO open				*/
	uint64_t	bytes;			/* Exact size in bytes					*/
} YADCX$FILE;
```

## 4. Names

| What | Rule | Example |
|---|---|---|
| global variable | `g_` | `g_count` |
| local variable | `l_` | `l_sts`, `l_vbn` |
| static variable or routine | `s_` | `s_buf`, `s_xfer()` |
| routine input parameter | `a_` | `a_file`, `a_blocks` |
| bit mask / bit field in a structure | `m_` / `v_` | `m_list`, `v_verify` |
| exported routine | `yadcx$name` | `yadcx$open()` |
| constant, macro | `MODULE$K_NAME`, `$MACRO` | `YADCX$K_BLKSZ` |
| structure tag / type | `name_t` / `MODULE$NAME` | `yadcx_hdr_t` / `YADCX$HDR` |
| message code | `YADCX$_NAME` (in `YADCX_MSG.MSG`) | `YADCX$_CRCERR` |

- Ordered constants go in an `enum`.
- Use sized types: `uint16_t`, `uint32_t`, `uint64_t` — never `long long`.
- Do not put `$` in shell environment variable names or CMake identifiers.

## 5. Code rules

- **A routine returns a status**; results go back through output parameters.
- **Strings travel as descriptors** (`struct dsc$descriptor_s`), never as
  `.ASCIZ`: no `strlen`, `strcpy`, `strchr` on text, no `'\0'` terminators.
  Literals are `static $DESCRIPTOR(s_q_name, "TEXT")`; a buffer gets a
  descriptor with `$DSCINIT(dsc, buffer, length)` (`YADCX.H`). An output string
  is a buffer descriptor whose length is the size on input and the length of the
  result on output (`yadcx$cli_value`, `yadcx$fmt_uq`, `yadcx$self`). Messages
  take strings as `!AS` (or `!AD`/`!AC` for counted data), never `!AZ`. Use the
  `STR$` routines for searching (`STR$FIND_FIRST_IN_SET`, …).
- **Every status is checked on its low bit**: `if ( !(1 & l_sts) )`.
- **No C RTL file I/O** (`fopen`, `fread`, `printf` to files…). Files are
  opened by RMS (`FAB$M_UFO`) and moved with `$QIOW` `IO$_READVBLK` /
  `IO$_WRITEVBLK`; record attributes are read and written through the ACP
  interface (`IO$_ACCESS` / `IO$_DEACCESS`, `ATR$C_RECATTR`).
- **Messages only through `$PUTMSG`**: add the code to `YADCX_MSG.MSG`, show it
  with `yadcx$putmsg()` (or `yadcx$putmsg_rms()` for a failed `$OPEN`/`$CREATE`,
  so the STV shows). Always pass the underlying status as the secondary one —
  never hide the real reason. Main programs exit with
  `sys$exit(code | STS$M_INHIB_MSG)`, so `$STATUS` is meaningful.
- Every new message gets a line in the **"Messages and what to do"** tables of
  README.md, in both languages.
- `$QIO`/`$QIOW` without a real need for an event flag use `EFN$C_ENF`.
- **System structures and constants come from the STARLET headers**, never
  from private copies: `IOSB` from `<iosbdef.h>` (`iosb$w_status` for `$QIO`,
  `iosb$l_getxxi_status` for `$GETxxI`), `FIBDEF`, `ILE3`, `ATR$C_*`, `SS$_*`…
- `#define __NEW_STARLET 1` at the top of every C module; cast the ACP
  attribute list to `(__int64)` for the `p5` of `$QIOW`.
- Use the Run-Time Library and system services (`LIB$`, `CLI$`, `SYS$`) as much
  as possible instead of writing your own.
- Comment in detail but to the point: every routine gets the VMS block
  (`FUNCTIONAL DESCRIPTION`, `INPUTS`, `OUTPUTS`, `IMPLICIT INPUTS/OUTPUTS`,
  `RETURNS`); no half-screen essay above a constant.

## 6. Traps already met — do not step into them again

- **CLD verbs are told apart by their first 4 characters.** `YADCX` and
  `YADCX_SFX` were the same verb `YADC`; the stub's verb is `SFX` for this reason.
- **`$PUTMSG` can not print a quadword.** Its 32-bit message vector goes
  through `$FAOL`, which takes `!UQ` as a longword value. Format 64-bit numbers
  with `yadcx$fmt_uq()` (`$FAOL_64`) and pass them as `!AZ`.
- **`YADCX$HDR` is packed** (`#pragma nomember_alignment`): copy a quadword out
  of it into an aligned local before passing its address anywhere.
- **The kit header marker is never a literal** — it is built by
  `yadcx$marker()`, otherwise the stub would find its own string.
- **FFBYTE in the FAT is a word**, stored normally; HIBLK and EFBLK are
  longwords stored high word first (`yadcx$fat_get/put`).
- **DCX map by reference**: `DCX$COMPRESS_INIT`/`DCX$EXPAND_INIT` take the
  *address of the longword holding the map address* (`map_addr.ra.r`), though
  `dcx$routines.h` says `void *map`; passing the map pointer gives
  `DCX-E-INVMAP`. `DCX$_NORMAL` is -S-, `DCX$_AGAIN` is -I-: repeat the analysis
  only on an informational status. A compressed record can be longer than the
  input — give `DCX$COMPRESS_DATA` twice the record (≤ 65535) and store the
  record as is on `DCX-W-TRUNC`. `out_len` is a word. The ground truth is the
  VMS source listing of DCX (`[DCX.SRC]*.B32`).
- **`LIB$CRC` takes a 16-bit descriptor**: `yadcx$crc()` feeds it pieces of at
  most 65535 bytes — a DCX map is often longer than that.
- **DCX records are 63 blocks (32256 bytes)**: a record is read with one
  `$QIO`, and it is the BACKUP block size.
- **DCL**: `WRITE` of a record longer than 255 bytes needs `/SYMBOL`;
  `DIFFERENCES a b` takes the directory of `a` for `b` — write `[]b`.

## 7. Documentation

- Two READMEs: `README.md` (source, English + Russian) and `AAAREADME.TXT`
  (for VMS). **Never edit `AAAREADME.TXT` by hand**: change README.md and run
  `python3 tools/mk_aaareadme.py`.
- User documentation is written for beginners: short sentences, simple
  English words, the Russian part says the same thing.
- If you change what the programs print, run `@YADCX_DEMO/OUTPUT=DEMO.LOG` and
  put the new protocol into the `Example session` section.

## 8. Sending a change

- One logical change per commit. Message: `yaDCX <IDENT>: what and why`,
  in English.
- Say in the description what you tested (at least `@YADCX_DEMO`), on which
  VMS version and compiler.
- When you explain a change in a review, point at `MODULE:line`
  (for example `YADCX_COMMON.C:157`).

---

# Как помочь проекту yaDCX

Спасибо за помощь. yaDCX — код для OpenVMS в стиле StarLet Squad. Стиль
**обязателен**: изменение, которое работает, но оформлено иначе, попросят
переоформить. Всё нужное — в этом файле; лучшие примеры — сами исходники.
Образцы кода — в английской части выше.

AI-агентам: прочитайте также [`AGENTS.md`](AGENTS.md).

## 1. Перед началом

- Нужны OpenVMS Alpha (эталон — V8.3) и HP C (эталон — V7.1). В HP C V7.1
  **нет `<stdint.h>`**: используйте `<inttypes.h>`.
- Сборка: `@YADCX_BUILD` (`@YADCX_BUILD DEBUG` — отладочная). Сборка должна
  закончиться `%YADCX_BUILD-S-DONE` **без** сообщений `%CC-W-`/`%CC-E-`/`%LINK-`.
- После каждого изменения запускайте `@YADCX_DEMO`. Каждый шаг должен
  закончиться как в протоколе README.md: киты восстанавливаются байт в байт,
  испорченные отвергаются, при плохом CRC ничего не пишется.

## 2. Файлы и шапки

- Каждый файл начинается с шапки: `__MODULE__`, `__IDENT__`, `__REV__`,
  авторы, `MODIFICATION HISTORY` в стиле OpenVMS.
- `AUTHORS` — всегда *StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)*.
- В истории подписывайтесь **своими** тремя буквами. `RRL` — метка
  сопровождающего, её не используйте. `CLD` — изменения, сделанные Claude.
- `__IDENT__` — стиль VMS: `X-aa-bb`, исправление ошибки добавляет `ECOcc`.
  `__REV__` — Unix-форма: `aa.bb.cc`. Меняйте **оба** вместе в каждом
  изменённом файле и добавляйте строку в историю. Версия продукта — версия
  головного модуля `YADCX.C`, она пишется в заголовок каждого кита.
- Архивы: `<пакет>-<ident>.tar.gz`, например `yaDCX-X-01-00ECO01.tar.gz`.

## 3. Оформление

- **Только табуляция**, ширина 8. Никаких пробелов для отступов и
  выравнивания — нигде.
- Строки до **192** колонок.
- Скобки **Whitesmiths**: скобка на своей строке с отступом блока; внешние
  скобки тела рутины **без** отступа; скобки структуры в колонке 0, члены с
  отступом.
- Одиночный оператор под `if`/`else`/`for` — **без** скобок.
- Объявления в начале, с колонки 0, сгруппированы по типу. Счётчики циклов —
  прямо в `for (int i = 0; ...)`.

## 4. Имена

Префиксы: `g_` — глобальные, `l_` — локальные, `s_` — статические переменные
и рутины, `a_` — входные параметры, `m_`/`v_` — битовые маски/поля.
Экспортируемые рутины — `yadcx$имя`, константы — `MODULE$K_ИМЯ`, типы —
`имя_t` / `MODULE$ИМЯ`, коды сообщений — `YADCX$_ИМЯ`. Упорядоченные
константы — `enum`. Типы с явным размером (`uint32_t`, `uint64_t`). Никаких
`$` в именах переменных окружения shell и в CMake.

## 5. Правила кода

- **Рутина возвращает статус**, результаты — через выходные параметры.
- **Строки — только дескрипторами** (`struct dsc$descriptor_s`), никакого
  `.ASCIZ`: ни `strlen`, ни `strcpy`/`strchr` по тексту, ни `'\0'` в конце.
  Литералы — `static $DESCRIPTOR(s_q_имя, "ТЕКСТ")`, буфер — через
  `$DSCINIT(dsc, буфер, длина)` (`YADCX.H`). Выходная строка — дескриптор
  буфера: на входе длина = размер, на выходе = длина результата. В сообщениях
  строки — `!AS` (или `!AD`/`!AC`), не `!AZ`. Для поиска — `STR$`
  (`STR$FIND_FIRST_IN_SET` и т. п.).
- **Каждый статус проверяется по младшему биту**: `if ( !(1 & l_sts) )`.
- **Никакого файлового ввода-вывода C RTL.** Только RMS (`FAB$M_UFO`),
  `$QIOW` `IO$_READVBLK`/`IO$_WRITEVBLK`, атрибуты — через ACP
  (`IO$_ACCESS`/`IO$_DEACCESS`, `ATR$C_RECATTR`).
- **Сообщения — только через `$PUTMSG`**: код в `YADCX_MSG.MSG`, вывод через
  `yadcx$putmsg()` / `yadcx$putmsg_rms()`. Настоящую причину (вторичный
  статус, STV) всегда показывать. Выход — `sys$exit(code | STS$M_INHIB_MSG)`.
- Каждое новое сообщение — строка в таблицах «Сообщения и что делать» README
  на обоих языках.
- `$QIO`/`$QIOW` без нужды в флаге события — `EFN$C_ENF`.
- **Системные структуры и константы — только из заголовков STARLET**, никаких
  своих копий: `IOSB` из `<iosbdef.h>` (`iosb$w_status` для `$QIO`,
  `iosb$l_getxxi_status` для `$GETxxI`), `FIBDEF`, `ILE3`, `ATR$C_*`, `SS$_*`…
- `#define __NEW_STARLET 1` в каждом C-модуле.
- По максимуму использовать RTL и системные сервисы (`LIB$`, `CLI$`, `SYS$`).
- Комментарии подробные, но по делу: у каждой рутины блок VMS
  (`FUNCTIONAL DESCRIPTION`, `INPUTS`, `OUTPUTS`, `IMPLICIT`, `RETURNS`).

## 6. Грабли, на которые уже наступали

- Глаголы CLD различаются по **первым 4 символам** — поэтому глагол стаба `SFX`.
- `$PUTMSG` **не печатает quadword** (`!UQ` в 32-битном `$FAOL` — longword по
  значению). 64-битные числа — через `yadcx$fmt_uq()` и `!AZ`.
- `YADCX$HDR` **упакован**: quadword из него сначала копировать в выровненную
  локальную переменную.
- Маркер заголовка **никогда не литерал** — его строит `yadcx$marker()`.
- В FAT **FFBYTE — слово** в обычном порядке; HIBLK и EFBLK — longword,
  старшее слово первым.
- **Карта DCX по ссылке**: `DCX$COMPRESS_INIT`/`DCX$EXPAND_INIT` принимают
  *адрес longword с адресом карты* (`map_addr.ra.r`), хотя в `dcx$routines.h`
  написано `void *map`; сам указатель даёт `DCX-E-INVMAP`. `DCX$_NORMAL` — -S-,
  `DCX$_AGAIN` — -I-: повторять анализ только на информационном статусе.
  Сжатая запись бывает длиннее исходной — буфер вдвое больше записи (≤ 65535),
  на `DCX-W-TRUNC` запись хранится как есть. `out_len` — word.
- **`LIB$CRC` берёт 16-битный дескриптор**: `yadcx$crc()` подаёт куски не
  длиннее 65535 байт — карта DCX часто длиннее.
- **Записи DCX — 63 блока (32256 байт)**: читаются одним `$QIO`, это размер
  блока BACKUP.
- DCL: `WRITE` записи длиннее 255 байт — с `/SYMBOL`; `DIFFERENCES a b` берёт
  каталог `a` для `b` — пишите `[]b`.

## 7. Документация

- `README.md` — источник (английский + русский), `AAAREADME.TXT` — для VMS.
  **`AAAREADME.TXT` руками не править**: правьте README.md и запускайте
  `python3 tools/mk_aaareadme.py`.
- Руководство пользователя — для начинающих, простыми словами.
- Если меняется то, что печатают программы, — прогоните
  `@YADCX_DEMO/OUTPUT=DEMO.LOG` и обновите раздел `Example session`.

## 8. Как прислать изменение

- Один логический шаг — один коммит. Сообщение по-английски:
  `yaDCX <IDENT>: что и зачем`.
- В описании — что проверено (минимум `@YADCX_DEMO`), на какой версии VMS и
  каком компиляторе.
- В пояснениях ссылайтесь на `МОДУЛЬ:строка` (например `YADCX_COMMON.C:157`).
