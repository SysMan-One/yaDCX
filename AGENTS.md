# AGENTS.md — instructions for AI coding agents

This file is for AI agents (Claude, Codex, Cursor, Aider, …) working on
yaDCX. Humans: see [`CONTRIBUTING.md`](CONTRIBUTING.md) — it is the full
version of the same rules, with examples. When the two differ,
CONTRIBUTING.md wins; tell the maintainer.

## What this project is

yaDCX builds self-extracting kits for **OpenVMS Alpha**: `YADCX.EXE` glues the
stub `YADCX_SFX.EXE`, a 512-byte kit header and a payload file into one image;
the stub checks a CRC-32 (== `zlib.crc32`) and restores the payload with its
exact RMS attributes. Kit layout and header: `YADCX.H`. User docs: `README.md`.

| File | Role |
|---|---|
| `YADCX.C` | builder, **head module** — its `__IDENT__` is the product version |
| `YADCX_SFX.C` | the stub |
| `YADCX_COMMON.C` | shared: `$PUTMSG`, CRC, RMS/`$QIO` block I/O, FAT, CLI, header scan |
| `YADCX.H` | kit format, `YADCX$HDR`, `YADCX$FILE`, message codes, prototypes |
| `YADCX_MSG.MSG`, `YADCX_CLI.CLD` | messages; command syntax (verbs `YADCX`, `SFX`) |
| `YADCX_BUILD.COM`, `YADCX.COM` | build; define the command |
| `YADCX_DEMO.COM`, `ATTR.COM` | end-to-end demo / test; attribute helper |
| `tools/yadcx_check.py`, `tools/mk_aaareadme.py` | kit check on Linux; makes `AAAREADME.TXT` |

## Build and test

The code only builds on OpenVMS (HP C V7.1 reference). There is no Linux
build and no unit-test framework — do not invent one.

```
$ @YADCX_BUILD                    ! must print %YADCX_BUILD-S-DONE, no %CC-/%LINK- messages
$ @YADCX_DEMO/OUTPUT=DEMO.LOG     ! the regression test; compare with README "Example session"
```

On Linux you can run `python3 tools/yadcx_check.py KIT.EXE` on a kit fetched in
binary mode, and `python3 tools/mk_aaareadme.py` after editing README.md.

If you can not reach an OpenVMS system, say so plainly in your report: code
you have not compiled on VMS is **untested**, never claim otherwise.

## Hard rules (a change that breaks one is wrong even if it works)

Formatting
- Tabs only, width 8; no spaces for indentation or alignment anywhere. Lines up to 192 columns.
- Whitesmiths braces; a routine's outer braces are not indented; struct braces at column 0.
- No braces around a single statement under `if`/`else`/`for`.
- Declarations first, column 0, grouped by type; loop counters inline `for (int i = 0; ...)`.
- Copy the layout of an existing routine (`s_xfer` in `YADCX_COMMON.C`) — parameter list, `)` placement, VMS comment block.

Names
- `g_` global, `l_` local, `s_` static (also static routines), `a_` input parameter, `m_`/`v_` bit mask/field.
- Exported routines `yadcx$name`; constants `YADCX$K_NAME`; types `name_t` + `YADCX$NAME`; messages `YADCX$_NAME`.
- Sized types from `<inttypes.h>` (`uint32_t`, `uint64_t`); **no `<stdint.h>`** (HP C V7.1 has none). Ordered constants in `enum`.

Behaviour
- Routines return a status; results through output parameters. Check every status on its low bit.
- No C RTL file I/O and no `printf`: RMS (`FAB$M_UFO`) + `$QIOW` block I/O, ACP for attributes.
- All output through `$PUTMSG` with codes from `YADCX_MSG.MSG` (`yadcx$putmsg`, `yadcx$putmsg_rms`);
  always pass the real underlying status; exit with `sys$exit(code | STS$M_INHIB_MSG)`.
- `EFN$C_ENF` for `$QIO`/`$QIOW`; `#define __NEW_STARLET 1` in every C module.
- Prefer RTL/system services (`LIB$`, `CLI$`, `SYS$`) over hand-written code.

Headers and versions
- Every file has `__MODULE__`, `__IDENT__`, `__REV__`, `AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)`
  and an OpenVMS `MODIFICATION HISTORY`.
- `__IDENT__` `X-aa-bb[ECOcc]` ↔ `__REV__` `aa.bb.cc`. Bump **both** in every file you change, add a history line.
  A bug fix bumps the ECO number.
- Sign history lines with **your own** initials (Claude uses `CLD`). **Never use `RRL`** — it is the maintainer's.
  Never rewrite someone else's history line.
- Archives: `yaDCX-<IDENT>.tar.gz`.

Documentation
- New or changed message → update the "Messages and what to do" tables in README.md, English **and** Russian.
- Never edit `AAAREADME.TXT` by hand; regenerate it with `tools/mk_aaareadme.py`.
- User docs are for beginners: simple English, the Russian part says the same.

## Traps (each one cost a debugging session)

1. CLD verbs are unique only in their **first 4 characters** (`YADCX_SFX` ≡ `YADCX`) → stub verb is `SFX`.
2. `$PUTMSG` → 32-bit `$FAOL`: `!UQ` prints a longword **value**. Use `yadcx$fmt_uq()` (`$FAOL_64`) + `!AZ`.
3. `YADCX$HDR` is packed: copy quadwords to an aligned local before passing their address (`!%D`, etc.).
4. The header marker must never be a string literal in either image — use `yadcx$marker()`.
5. FAT: HIBLK/EFBLK are longwords stored high word first (`yadcx$fat_get/put`); FFBYTE is a plain word.
6. DCL: records > 255 bytes need `WRITE/SYMBOL`; `DIFFERENCES a b` inherits `a`'s directory for `b`.
7. DCL has no command separator: driving VMS over `ssh host "cmd"` runs one command per call — put sequences into a `.COM` file.

## How to report

- Explain changes with `MODULE:line` references.
- State exactly what was built and run, where, and what the output was. If something failed or was skipped, say so.
- Commits: `yaDCX <IDENT>: what and why`, in English.
