#!/usr/bin/env python3
#
#  yadcx_check.py -- check yaDCX kits on a non-VMS system
#
#  __MODULE__	YADCX_CHECK
#  __IDENT__	X-02-00
#  __REV__	02.00.00
#
#  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#  MODIFICATION HISTORY:
#
#	26-SEP-2026	CLD	X-01-00ECO01: initial version.
#	26-SEP-2026	CLD	X-02-00: kit format 2 (NONE/DCX): the stored data are
#				checked; a DCX payload can not be expanded here.
#
#  Usage:	python3 yadcx_check.py KIT.EXE [...]
#
#  The kit must be fetched in binary mode.  The header is the first 512-byte
#  block that starts with "yaDCX-KIT-HEADER" (see YADCX.H, all little-endian):
#
#	16 version (2)	20 crc (4)	24 bytes (8)	508 hdrcrc (4)
#	version 2 only:	348 method (2, 0 NONE, 1 DCX)	356 sbytes (8)	368 scrc (4)
#
#  The stored data follow the header block.  For NONE they are the payload
#  and its CRC-32 is checked.  For DCX the CRC-32 of the stored stream is
#  checked - that is what the stub checks before it writes anything; the
#  payload itself can only be expanded on VMS (DCX$EXPAND_DATA).
#  Exit status: 0 - all kits OK, 1 - a kit is damaged or not a kit.
#
import sys, zlib, struct

l_rc = 0
for l_name in sys.argv[1:]:
	l_kit = open(l_name, 'rb').read()
	l_hdr = l_kit.find(b'yaDCX-KIT-HEADER')
	while l_hdr >= 0 and l_hdr % 512:
		l_hdr = l_kit.find(b'yaDCX-KIT-HEADER', l_hdr + 1)
	if l_hdr < 0:
		print(f'{l_name}: no kit header')
		l_rc = 1
		continue
	l_block = l_kit[l_hdr : l_hdr + 512]
	l_ver, = struct.unpack_from('<H', l_block, 16)
	l_crc, l_size = struct.unpack_from('<IQ', l_block, 20)
	l_hcrc, = struct.unpack_from('<I', l_block, 508)
	if zlib.crc32(l_block[:508]) != l_hcrc:
		print(f'{l_name}: the kit header is damaged')
		l_rc = 1
		continue
	l_method, l_scrc, l_sbytes = 0, l_crc, l_size
	if l_ver >= 2:
		l_method, = struct.unpack_from('<H', l_block, 348)
		l_sbytes, = struct.unpack_from('<Q', l_block, 356)
		l_scrc, = struct.unpack_from('<I', l_block, 368)
	l_data = l_kit[l_hdr + 512 : l_hdr + 512 + l_sbytes]
	l_got = zlib.crc32(l_data)
	l_ok = l_got == l_scrc and len(l_data) == l_sbytes
	l_what = {0: 'payload', 1: 'DCX stream'}.get(l_method, f'method {l_method} data')
	print(f'{l_name}: v{l_ver}, header at VBN {l_hdr // 512 + 1}, {l_what} {l_sbytes} bytes, recorded {l_scrc:08X}, computed {l_got:08X} ->', 'OK' if l_ok else 'DAMAGED')
	if l_ok and l_method == 1:
		print(f'{l_name}: payload {l_size} bytes, CRC-32 {l_crc:08X} - checked by the stub on VMS only (DCX)')
	l_rc |= 0 if l_ok else 1
sys.exit(l_rc)
