#ifndef	__YADCX_H__
#define	__YADCX_H__	1

#define	__MODULE__	"YADCX.H"
#define	__IDENT__	"X-01-00"
#define	__REV__		"01.00.00"

/*
**++
**  FACILITY:	yaDCX -- Yet Another Data Compression/eXpansion kit builder
**
**  MODULE DESCRIPTION:
**
**	Common definitions of the yaDCX builder (YADCX.EXE) and of the self-
**	extracting stub (YADCX_SFX.EXE): the kit layout, the kit header block,
**	the message codes and the prototypes of the shared routines in
**	YADCX_COMMON.C.
**
**	A kit is a single Fixed-512 file:
**
**		VBN 1 .. S		the stub image YADCX_SFX.EXE, block for block
**		VBN S+1			the kit header (YADCX$HDR, one 512-byte block)
**		VBN S+2 .. S+1+P	the payload, block for block, exactly as it sits
**					on disk (normally a BACKUP saveset)
**
**	The header carries the payload's own 32-byte RMS record attributes (the
**	FAT, as read by IO$_ACCESS/ATR$C_RECATTR), its exact byte size and a
**	CRC-32 of those bytes.  The stub writes the blocks back and puts the FAT
**	back with IO$_DEACCESS, so the restored file is identical to the
**	original: record format, record size, attributes and end-of-file byte.
**
**	The CRC-32 is the one of zlib/PKZIP/Ethernet (polynomial EDB88320,
**	initial value FFFFFFFF, result inverted), computed with LIB$CRC, so a
**	kit can be checked on any other system with zlib.crc32().
**
**	All multi-byte fields of the header are little-endian (native Alpha).
**
**  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  26-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	26-SEP-2026	CLD	X-01-00: initial version, replaces the DX-only
**				SFX.C/MKCAT.C pair of [LAISHEV.YADX_KIT].
**--
*/

#include	<inttypes.h>			/* uint32_t, uint64_t ... - HP C V7.1 has no <stdint.h>	*/
#include	<rms.h>


/*
**  Kit format constants
*/
#define	YADCX$K_BLKSZ		512		/* Disk block size					*/
#define	YADCX$K_IOBLKS		127		/* Blocks per $QIO, the chunk also fits a LIB$CRC	*/
						/* string descriptor (65024 < 65535)			*/
#define	YADCX$K_VERSION		1		/* Kit header format version				*/
#define	YADCX$K_MARKSZ		16		/* Size of the header marker				*/
#define	YADCX$K_FATSZ		32		/* Size of the FAT - ATR$S_RECATTR			*/
#define	YADCX$K_NAMESZ		255		/* Max size of the stored file name			*/
#define	YADCX$K_IDENTSZ		16		/* Size of the builder's ident field			*/
#define	YADCX$K_CRCPOLY		0xEDB88320	/* CRC-32 polynomial (zlib), reflected			*/

/*
**  Offsets in the FAT (FAT$ layout, see the I/O User's Reference, ACP-QIO
**  interface).  HIBLK and EFBLK are longwords stored as two words, high
**  word first.
*/
#define	YADCX$K_FAT_HIBLK	4		/* Highest allocated VBN				*/
#define	YADCX$K_FAT_EFBLK	8		/* End-of-file VBN					*/
#define	YADCX$K_FAT_FFBYTE	12		/* First free byte in the EFBLK block			*/


/*
**  The kit header, exactly one disk block.  The marker is never stored as a
**  literal in either image, it is built at run time by yadcx$marker(), so a
**  scan of the stub's own blocks can not match its own string constant.
*/
#pragma member_alignment save
#pragma nomember_alignment

typedef struct yadcx_hdr_t {
	char		marker[YADCX$K_MARKSZ];	/* "yaDCX-KIT-HEADER"					*/
	uint16_t	version;		/* YADCX$K_VERSION					*/
	uint16_t	hdrsz;			/* sizeof(YADCX$HDR) == YADCX$K_BLKSZ			*/
	uint32_t	crc;			/* CRC-32 of the payload's <bytes> bytes		*/
	uint64_t	bytes;			/* Exact payload size in bytes				*/
	uint32_t	blocks;			/* Payload size in blocks, follow the header		*/
	uint8_t		fat[YADCX$K_FATSZ];	/* Payload's RMS record attributes (FAT)		*/
	uint64_t	built;			/* Build time, VMS binary time				*/
	char		ident[YADCX$K_IDENTSZ];	/* Builder's __IDENT__, .ASCIC				*/
	uint8_t		namelen;		/* Length of the name					*/
	char		name[YADCX$K_NAMESZ];	/* Payload's original "name.type"			*/
	uint8_t		spare[YADCX$K_BLKSZ - 16 - 2 - 2 - 4 - 8 - 4 - YADCX$K_FATSZ - 8 - YADCX$K_IDENTSZ - 1 - YADCX$K_NAMESZ - 4];
	uint32_t	hdrcrc;			/* CRC-32 of all the preceding bytes of the header	*/
} YADCX$HDR;

#pragma member_alignment restore


/*
**  A file opened for block I/O by yadcx$open()/yadcx$create()
*/
typedef struct yadcx_file_t {
	struct FAB	fab;
	struct NAM	nam;
	struct XABFHC	fhc;
	char		esa[NAM$C_MAXRSS];	/* Expanded file name					*/
	char		rsa[NAM$C_MAXRSS + 1];	/* Resultant file name, .ASCIZ				*/
	uint16_t	chan;			/* Channel of the UFO open				*/
	uint64_t	bytes;			/* Exact size in bytes					*/
	uint32_t	blocks;			/* Size in blocks, the last one may be partial		*/
} YADCX$FILE;


/*
**  Message codes, from YADCX_MSG.MSG
*/
#pragma nostandard
globalvalue unsigned int
	YADCX$_BUILT, YADCX$_RESTORED, YADCX$_VERIFIED,
	YADCX$_KITINFO, YADCX$_KITCRC, YADCX$_PAYLOAD,
	YADCX$_NOHEADER, YADCX$_BADHDR, YADCX$_BADVER, YADCX$_TRUNCATED, YADCX$_CRCERR,
	YADCX$_NESTED, YADCX$_BADNAME, YADCX$_EMPTY,
	YADCX$_OPENIN, YADCX$_OPENOUT, YADCX$_READERR, YADCX$_WRITEERR, YADCX$_ATTRERR,
	YADCX$_SYSERR;
#pragma standard


/*
**  Shared routines, YADCX_COMMON.C
*/
unsigned int	yadcx$putmsg	(unsigned int a_msg, unsigned int a_sts, int a_nfao, ...);
void		yadcx$marker	(char *a_marker);
void		yadcx$crc_init	(void);
void		yadcx$crc	(uint32_t *a_crc, void *a_buf, uint32_t a_len);
unsigned int	yadcx$open	(YADCX$FILE *a_file, char *a_name, int a_namelen, char *a_defname);
unsigned int	yadcx$create	(YADCX$FILE *a_file, char *a_name, int a_namelen, char *a_defname, uint32_t a_blocks);
unsigned int	yadcx$read	(YADCX$FILE *a_file, uint32_t a_vbn, void *a_buf, uint32_t a_blocks);
unsigned int	yadcx$write	(YADCX$FILE *a_file, uint32_t a_vbn, void *a_buf, uint32_t a_blocks);
unsigned int	yadcx$get_fat	(YADCX$FILE *a_file, uint8_t *a_fat);
unsigned int	yadcx$close	(YADCX$FILE *a_file, uint8_t *a_fat);
void		yadcx$abort	(YADCX$FILE *a_file);
void		yadcx$fat_put	(uint8_t *a_fat, int a_off, uint32_t a_val);
uint32_t	yadcx$fat_get	(uint8_t *a_fat, int a_off);
unsigned int	yadcx$cli_parse	(char *a_verb, int a_prompt, int *a_given);
int		yadcx$cli_present (char *a_name);
unsigned int	yadcx$cli_value	(char *a_name, char *a_buf, int a_bufsz, int *a_len);
unsigned int	yadcx$find_hdr	(YADCX$FILE *a_file, uint32_t *a_vbn, YADCX$HDR *a_hdr);
unsigned int	yadcx$check	(YADCX$FILE *a_file, uint32_t a_vbn, YADCX$HDR *a_hdr, uint32_t *a_crc);
void		yadcx$show	(YADCX$HDR *a_hdr);
void		yadcx$fmt_uq	(uint64_t a_val, char *a_buf, int a_bufsz);
unsigned int	yadcx$self	(char *a_name, int a_namesz, int *a_len);

#undef	__MODULE__				/* The including module defines its own			*/
#undef	__IDENT__
#undef	__REV__

#endif	/* __YADCX_H__ */
