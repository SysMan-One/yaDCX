#!/usr/bin/env python3
#
#  yadcx_check.py -- check yaDCX kits on a non-VMS system
#
#  __MODULE__	YADCX_CHECK
#  __IDENT__	X-01-00ECO01
#  __REV__	01.00.01
#
#  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#  MODIFICATION HISTORY:
#
#	26-SEP-2026	CLD	X-01-00ECO01: initial version.
#
#  Usage:	python3 yadcx_check.py KIT.EXE [...]
#
#  The kit must be fetched in binary mode.  The header is the first 512-byte
#  block that starts with "yaDCX-KIT-HEADER"; its CRC-32 (offset 20) and byte
#  size (offset 24) are little-endian, the payload follows the header block.
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
	l_crc, l_size = struct.unpack_from('<IQ', l_kit, l_hdr + 20)
	l_got = zlib.crc32(l_kit[l_hdr + 512 : l_hdr + 512 + l_size])
	l_ok = l_got == l_crc and len(l_kit) >= l_hdr + 512 + l_size
	print(f'{l_name}: header at VBN {l_hdr // 512 + 1}, {l_size} bytes, recorded {l_crc:08X}, computed {l_got:08X} ->', 'OK' if l_ok else 'DAMAGED')
	l_rc |= 0 if l_ok else 1
sys.exit(l_rc)
