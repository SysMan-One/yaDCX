#ifndef	__YADCX_H__
#define	__YADCX_H__	1

#define	__MODULE__	"YADCX.H"
#define	__IDENT__	"X-02-00"
#define	__REV__		"02.00.00"

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
**		VBN S+2 .. S+1+D	the stored data, D blocks, <sbytes> long
**
**	The stored data depend on the compression method:
**
**	  NONE	the payload, block for block, exactly as it sits on disk
**		(normally a BACKUP saveset); the only method of format version 1.
**
**	  DCX	the payload compressed by the VMS DCX$ routines: first the DCX
**		map (<mapbytes> bytes, as made by DCX$MAKE_MAP), then one entry
**		per YADCX$K_RECLEN bytes of the payload - a longword length
**		followed by the compressed record, or by the record as is when
**		bit YADCX$M_RAW of the length is set (data DCX can not shrink).
**		The stream is padded with zeros to the block boundary.
**
**	Two CRC-32 are kept: <scrc> over the stored data, checked before
**	anything is written, and <crc> over the original payload, checked while
**	it is expanded.  For NONE they are the same.
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
**
**	26-SEP-2026	CLD	X-01-00ECO01: yadcx$putmsg_rms(), yadcx$fmt_uq().
**
**	26-SEP-2026	CLD	X-01-01: $DSCINIT, YADCX$FILE.rsadsc, descriptor
**				prototypes.
**
**	26-SEP-2026	CLD	X-02-00: kit format version 2 - compression method
**				(NONE/DCX), stored data size and CRC; DCX routines.
**--
*/

#include	<inttypes.h>			/* uint32_t, uint64_t ... - HP C V7.1 has no <stdint.h>	*/
#include	<rms.h>
#include	<descrip.h>


/*
**  Kit format constants
*/
#define	YADCX$K_BLKSZ		512		/* Disk block size					*/
#define	YADCX$K_IOBLKS		127		/* Blocks per $QIO, the chunk also fits a LIB$CRC	*/
						/* string descriptor (65024 < 65535)			*/
#define	YADCX$K_VERSION		2		/* Kit header format version: 1 - NONE only, 2 - method	*/
#define	YADCX$K_RECLEN		32256		/* DCX record: 63 blocks, the BACKUP block size		*/
#define	YADCX$K_RECBLKS		63		/* Blocks per DCX record				*/
#define	YADCX$K_OUTSZ		65535		/* Compressed record buffer, a record may grow		*/
#define	YADCX$M_RAW		0x80000000	/* Record length flag: the record is stored as is	*/
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
**  Compression methods, YADCX$HDR.method
*/
enum	{
	YADCX$K_METHOD_NONE = 0,		/* Stored as is						*/
	YADCX$K_METHOD_DCX			/* VMS DCX$ routines					*/
	};


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
	uint32_t	blocks;			/* Payload size in blocks				*/
	uint8_t		fat[YADCX$K_FATSZ];	/* Payload's RMS record attributes (FAT)		*/
	uint64_t	built;			/* Build time, VMS binary time				*/
	char		ident[YADCX$K_IDENTSZ];	/* Builder's __IDENT__, .ASCIC				*/
	uint8_t		namelen;		/* Length of the name					*/
	char		name[YADCX$K_NAMESZ];	/* Payload's original "name.type"			*/
						/* --- version 2, offset 348: zero in version 1 ---	*/
	uint16_t	method;			/* YADCX$K_METHOD_xxx					*/
	uint16_t	reclen;			/* DCX: payload bytes per record, YADCX$K_RECLEN	*/
	uint32_t	mapbytes;		/* DCX: size of the map at the start of the stream	*/
	uint64_t	sbytes;			/* Stored data size in bytes				*/
	uint32_t	sblocks;		/* Stored data size in blocks, follow the header	*/
	uint32_t	scrc;			/* CRC-32 of the <sbytes> stored bytes			*/
	uint8_t		spare[YADCX$K_BLKSZ - 16 - 2 - 2 - 4 - 8 - 4 - YADCX$K_FATSZ - 8 - YADCX$K_IDENTSZ - 1 - YADCX$K_NAMESZ
				- 2 - 2 - 4 - 8 - 4 - 4 - 4];
	uint32_t	hdrcrc;			/* CRC-32 of all the preceding bytes of the header	*/
} YADCX$HDR;

#pragma member_alignment restore


/*
**  Fill a fixed-length text string descriptor: $DSCINIT(dsc, address, length).
**  All strings of yaDCX travel as descriptors, never as .ASCIZ.
*/
#define	$DSCINIT(d, p, l)	((d).dsc$w_length = (uint16_t) (l), (d).dsc$b_dtype = DSC$K_DTYPE_T, (d).dsc$b_class = DSC$K_CLASS_S, (d).dsc$a_pointer = (char *) (p))


/*
**  A file opened for block I/O by yadcx$open()/yadcx$create()
*/
typedef struct yadcx_file_t {
	struct FAB	fab;
	struct NAM	nam;
	struct XABFHC	fhc;
	char		esa[NAM$C_MAXRSS];	/* Expanded file name					*/
	char		rsa[NAM$C_MAXRSS];	/* Resultant file name buffer				*/
	struct dsc$descriptor_s	rsadsc;		/* Resultant file name, NAM$B_RSL long		*/
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
	YADCX$_NESTED, YADCX$_BADNAME, YADCX$_EMPTY, YADCX$_BADMETH, YADCX$_EXPCRC, YADCX$_DCXERR,
	YADCX$_COMPRESSED, YADCX$_NOGAIN, YADCX$_KITDCX,
	YADCX$_OPENIN, YADCX$_OPENOUT, YADCX$_READERR, YADCX$_WRITEERR, YADCX$_ATTRERR,
	YADCX$_SYSERR;
#pragma standard


/*
**  Shared routines, YADCX_COMMON.C
*/
unsigned int	yadcx$putmsg	(unsigned int a_msg, unsigned int a_sts, int a_nfao, ...);
unsigned int	yadcx$putmsg_rms (unsigned int a_msg, YADCX$FILE *a_file, struct dsc$descriptor_s *a_arg);
void		yadcx$marker	(char *a_marker);
void		yadcx$crc_init	(void);
void		yadcx$crc	(uint32_t *a_crc, void *a_buf, uint32_t a_len);
unsigned int	yadcx$open	(YADCX$FILE *a_file, struct dsc$descriptor_s *a_name, struct dsc$descriptor_s *a_defname);
unsigned int	yadcx$create	(YADCX$FILE *a_file, struct dsc$descriptor_s *a_name, struct dsc$descriptor_s *a_defname, uint32_t a_blocks);
unsigned int	yadcx$read	(YADCX$FILE *a_file, uint32_t a_vbn, void *a_buf, uint32_t a_blocks);
unsigned int	yadcx$write	(YADCX$FILE *a_file, uint32_t a_vbn, void *a_buf, uint32_t a_blocks);
unsigned int	yadcx$get_fat	(YADCX$FILE *a_file, uint8_t *a_fat);
unsigned int	yadcx$close	(YADCX$FILE *a_file, uint8_t *a_fat);
void		yadcx$abort	(YADCX$FILE *a_file);
void		yadcx$fat_put	(uint8_t *a_fat, int a_off, uint32_t a_val);
uint32_t	yadcx$fat_get	(uint8_t *a_fat, int a_off);
unsigned int	yadcx$cli_parse	(struct dsc$descriptor_s *a_verb, int a_prompt, int *a_given);
int		yadcx$cli_present (struct dsc$descriptor_s *a_name);
unsigned int	yadcx$cli_value	(struct dsc$descriptor_s *a_name, struct dsc$descriptor_s *a_val);
unsigned int	yadcx$find_hdr	(YADCX$FILE *a_file, uint32_t *a_vbn, YADCX$HDR *a_hdr);
unsigned int	yadcx$check	(YADCX$FILE *a_file, uint32_t a_vbn, YADCX$HDR *a_hdr, uint32_t *a_crc);
void		yadcx$show	(YADCX$HDR *a_hdr);
unsigned int	yadcx$dcx_map	(YADCX$FILE *a_pay, void **a_map, uint32_t *a_mapsz, uint32_t *a_crc);
unsigned int	yadcx$compress	(YADCX$FILE *a_pay, void **a_map, uint32_t a_mapsz, YADCX$FILE *a_kit, uint32_t a_vbn,
				uint64_t *a_sbytes, uint32_t *a_scrc, uint32_t *a_raws);
unsigned int	yadcx$expand	(YADCX$FILE *a_kit, uint32_t a_vbn, YADCX$HDR *a_hdr, YADCX$FILE *a_out, uint32_t *a_crc);
void		yadcx$percent	(uint64_t a_part, uint64_t a_whole, uint32_t *a_int, uint32_t *a_frac);
void		yadcx$fmt_uq	(uint64_t a_val, struct dsc$descriptor_s *a_txt);
unsigned int	yadcx$self	(struct dsc$descriptor_s *a_name);

#undef	__MODULE__				/* The including module defines its own			*/
#undef	__IDENT__
#undef	__REV__

#endif	/* __YADCX_H__ */
