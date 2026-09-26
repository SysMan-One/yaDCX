#!/usr/bin/env python3
#
#  mk_aaareadme.py -- make AAAREADME.TXT (plain text for VMS) from README.md
#
#  __MODULE__	YADCX_MK_AAAREADME
#  __IDENT__	X-01-01ECO01
#  __REV__	01.01.01
#
#  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#  MODIFICATION HISTORY:
#
#	26-SEP-2026	CLD	X-01-00ECO01: initial version.
#	26-SEP-2026	CLD	X-01-00ECO02: header of AAAREADME.TXT for ECO02.
#	26-SEP-2026	CLD	X-01-01: header of AAAREADME.TXT for X-01-01.
#	26-SEP-2026	CLD	X-01-01ECO01: header of AAAREADME.TXT for X-01-01ECO01.
#
#  Usage:	python3 tools/mk_aaareadme.py		(from the top directory)
#
#  README.md is the source, never edit AAAREADME.TXT by hand.  Tables become
#  indented lists, code blocks are indented by 4, headings are underlined,
#  typographic dashes and quotes are replaced by ASCII.  The header block
#  below (IDENT, history) is kept here and must be bumped with the project.
#
import re, textwrap
src=open('README.md').read().splitlines()
out=[]; code=False
for l in src:
	if l.startswith('```'): code=not code; continue
	if code: out.append(('    '+l.replace('\t','  ')).rstrip()); continue
	if l.startswith('|'):
		cells=[c.strip().replace('`','') for c in l.strip('|').split('|')]
		if set(''.join(cells))<=set('-'): continue
		if cells[0] in ('File','Qualifier','Message','Файл','Квалификатор','Сообщение'): continue
		out+=textwrap.wrap(cells[0],74,initial_indent='  ',subsequent_indent='    ')
		out+=textwrap.wrap(' -- '.join(cells[1:]),76,initial_indent='        ',subsequent_indent='        ')
		continue
	if l.startswith('#'):
		t=l.lstrip('#').strip().replace('`','')
		out+=['',t.upper() if l.startswith('# ') else t,('=' if l.startswith('# ') else '-')*len(t)]; continue
	if l.strip()=='---': out+=['','*'*78]; continue
	out.append(l.replace('`','').replace('**','').replace('*',''))
txt=re.sub(r'\n{3,}','\n\n','\n'.join(out)).strip()
txt=txt.replace('—','--').replace('«','"').replace('»','"')
hdr="""AAAREADME.TXT -- yaDCX, self-extracting kits for OpenVMS

__MODULE__  AAAREADME
__IDENT__   X-01-01ECO01
__REV__     01.01.01

AUTHORS:    StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

MODIFICATION HISTORY:

    26-SEP-2026  CLD  X-01-00: initial version.
    26-SEP-2026  CLD  X-01-00ECO01: example session (YADCX_DEMO.COM), ACTIMAGE,
                      yadcx_check.py.
    26-SEP-2026  CLD  X-01-00ECO02: CONTRIBUTING.md, AGENTS.md, mk_aaareadme.py.
    26-SEP-2026  CLD  X-01-01: strings by descriptor (no .ASCIZ) in the code.
    26-SEP-2026  CLD  X-01-01ECO01: IOSB from IOSBDEF.H.

Made from README.md.  The text is UTF-8: the Russian part needs a UTF-8
terminal, or read README.md on the git side.

"""
open('AAAREADME.TXT','w').write(hdr+txt+'\n')
