#define	__NEW_STARLET	1

#include	<string.h>
#include	<stdarg.h>
#include	<rms.h>
#include	<descrip.h>
#include	<iodef.h>
#include	<fibdef.h>
#include	<atrdef.h>
#include	<efndef.h>
#include	<jpidef.h>
#include	<iledef.h>
#include	<stsdef.h>
#include	<ssdef.h>
#include	<starlet.h>
#include	<lib$routines.h>
#include	<cli$routines.h>

#include	"YADCX.H"

#define	__MODULE__	"YADCX_COMMON"
#define	__IDENT__	"X-01-00"
#define	__REV__		"01.00.00"

/*
**++
**  FACILITY:	yaDCX -- Yet Another Data Compression/eXpansion kit builder
**
**  MODULE DESCRIPTION:
**
**	Routines shared by the builder YADCX.EXE and the stub YADCX_SFX.EXE:
**	message output through $PUTMSG, the kit header marker, CRC-32 through
**	LIB$CRC, and block I/O on files opened by RMS with FAB$M_UFO (the
**	channel comes back in FAB$L_STV, the blocks move with IO$_READVBLK/
**	IO$_WRITEVBLK, and the RMS record attributes are read and written as
**	the raw 32-byte FAT through the ACP-QIO interface).
**
**	No C RTL file I/O at all.
**
**  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  26-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	26-SEP-2026	CLD	X-01-00: initial version.
**--
*/

/*
**  I/O status block of the $QIOW calls
*/
typedef struct yadcx_iosb_t {
	uint16_t	status;
	uint16_t	count;
	uint32_t	devdep;
} YADCX$IOSB;

/*
**  One item of an ACP attribute list, the list ends with a zero item
*/
typedef struct yadcx_atr_t {
	uint16_t	size;
	uint16_t	type;
	void		*addr;
} YADCX$ATR;

static int	s_crctab[16];			/* LIB$CRC_TABLE output, built by yadcx$crc_init()	*/


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Output a message of the YADCX facility through $PUTMSG, optionally
**	followed by a secondary status (a system service or RMS failure code)
**	so the reason is never swallowed.
**
**  INPUTS:
**
**	a_msg	-	YADCX$_xxx message code
**	a_sts	-	secondary status to show under it, 0 - none
**	a_nfao	-	number of FAO arguments that follow
**	...	-	FAO arguments, longwords; pointers are passed as is
**			(32-bit pointers, the default of HP C)
**
**  IMPLICIT OUTPUTS:
**
**	SYS$OUTPUT, SYS$ERROR (for non-success messages) - by $PUTMSG
**
**  RETURNS:
**
**	a_msg, so a caller can write: return yadcx$putmsg(...)
**--
*/
unsigned int	yadcx$putmsg	(
		unsigned int	a_msg,
		unsigned int	a_sts,
		int		a_nfao,
			...
			)
{
uint32_t	l_vec[32];
int		l_idx = 1;
va_list		l_ap;

	l_vec[l_idx++] = a_msg;
	l_vec[l_idx++] = a_nfao;

	va_start(l_ap, a_nfao);
	for (int i = 0; (i < a_nfao) && (l_idx < 28); i++)
		l_vec[l_idx++] = va_arg(l_ap, unsigned int);
	va_end(l_ap);

	/*
	**  SYSTEM (0) and RMS (1) facility codes take no FAO count; an RMS
	**  code would take its STV next, we have none here, so the vector just
	**  ends.  Anything else takes an FAO count of zero.
	*/
	if ( a_sts )
		{
		l_vec[l_idx++] = a_sts;
		if ( $VMS_STATUS_FAC_NO(a_sts) > 1 )
			l_vec[l_idx++] = 0;
		}

	l_vec[0] = l_idx - 1;
	sys$putmsg((void *) l_vec, 0, 0, 0);

	return	a_msg;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Build the kit header marker "yaDCX-KIT-HEADER" from two fragments,
**	so that the full string never appears as a literal in the images.
**
**  OUTPUTS:
**
**	a_marker -	buffer of YADCX$K_MARKSZ bytes, not zero-terminated
**--
*/
void	yadcx$marker	(
		char	*a_marker
			)
{
	memcpy(a_marker, "yaDCX-KIT", 9);
	memcpy(a_marker + 9, "-HEADER", 7);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Build the 16-entry CRC table for the zlib CRC-32 polynomial, once per
**	image run, before any yadcx$crc() call.
**
**  IMPLICIT OUTPUTS:
**
**	s_crctab
**--
*/
void	yadcx$crc_init	(void)
{
uint32_t	l_poly = YADCX$K_CRCPOLY;

	lib$crc_table(&l_poly, s_crctab);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Run a buffer through LIB$CRC.  The caller starts with 0xFFFFFFFF and
**	inverts the final value, that gives the zlib/PKZIP CRC-32.  LIB$CRC
**	takes a string descriptor, so the length is limited to 65535 bytes.
**
**  INPUTS:
**
**	a_buf	-	data
**	a_len	-	data length, <= 65535
**
**  INPUTS/OUTPUTS:
**
**	a_crc	-	running CRC value
**
**  IMPLICIT INPUTS:
**
**	s_crctab
**--
*/
void	yadcx$crc	(
		uint32_t	*a_crc,
		void		*a_buf,
		uint32_t	a_len
			)
{
struct dsc$descriptor_s	l_dsc;

	l_dsc.dsc$w_length = (uint16_t) a_len;
	l_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_dsc.dsc$b_class = DSC$K_CLASS_S;
	l_dsc.dsc$a_pointer = (char *) a_buf;

	*a_crc = lib$crc(s_crctab, (int *) a_crc, &l_dsc);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Get/put a longword field of the FAT, which is kept as two words, the
**	high one first.
**
**  INPUTS:
**
**	a_fat	-	32-byte FAT
**	a_off	-	offset of the field, YADCX$K_FAT_xxx
**	a_val	-	value to store (yadcx$fat_put)
**
**  RETURNS:
**
**	yadcx$fat_get - the field value
**--
*/
uint32_t	yadcx$fat_get	(
		uint8_t		*a_fat,
		int		a_off
			)
{
	return	((uint32_t) a_fat[a_off + 1] << 24) | ((uint32_t) a_fat[a_off] << 16) | ((uint32_t) a_fat[a_off + 3] << 8) | a_fat[a_off + 2];
}

void	yadcx$fat_put	(
		uint8_t		*a_fat,
		int		a_off,
		uint32_t	a_val
			)
{
	a_fat[a_off]     = (uint8_t) (a_val >> 16);
	a_fat[a_off + 1] = (uint8_t) (a_val >> 24);
	a_fat[a_off + 2] = (uint8_t) a_val;
	a_fat[a_off + 3] = (uint8_t) (a_val >> 8);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Common part of yadcx$open() and yadcx$create(): fill the FAB, NAM and
**	XABFHC of the file context.
**--
*/
static void	s_init	(
		YADCX$FILE	*a_file,
		char		*a_name,
		int		a_namelen,
		char		*a_defname
			)
{
	memset(a_file, 0, sizeof(YADCX$FILE));

	a_file->fab = cc$rms_fab;
	a_file->nam = cc$rms_nam;
	a_file->fhc = cc$rms_xabfhc;

	a_file->fab.fab$l_fna = a_name;
	a_file->fab.fab$b_fns = (uint8_t) a_namelen;
	if ( a_defname )
		{
		a_file->fab.fab$l_dna = a_defname;
		a_file->fab.fab$b_dns = (uint8_t) strlen(a_defname);
		}
	a_file->fab.fab$l_fop = FAB$M_UFO;
	a_file->fab.fab$l_nam = &a_file->nam;
	a_file->fab.fab$l_xab = (void *) &a_file->fhc;

	a_file->nam.nam$l_esa = a_file->esa;
	a_file->nam.nam$b_ess = sizeof(a_file->esa);
	a_file->nam.nam$l_rsa = a_file->rsa;
	a_file->nam.nam$b_rss = sizeof(a_file->rsa) - 1;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Open an existing file for block reads (RMS $OPEN with FAB$M_UFO) and
**	work out its exact size from the XABFHC.
**
**  INPUTS:
**
**	a_name	-	file specification, need not be zero-terminated
**	a_namelen -	its length
**	a_defname -	default file specification, .ASCIZ, or NULL
**
**  OUTPUTS:
**
**	a_file	-	file context: channel, resultant name, bytes, blocks
**
**  RETURNS:
**
**	condition value of the $OPEN
**--
*/
unsigned int	yadcx$open	(
		YADCX$FILE	*a_file,
		char		*a_name,
		int		a_namelen,
		char		*a_defname
			)
{
unsigned int	l_sts;

	s_init(a_file, a_name, a_namelen, a_defname);
	a_file->fab.fab$b_fac = FAB$M_GET;
	a_file->fab.fab$b_shr = FAB$M_SHRGET;

	if ( !(1 & (l_sts = sys$open(&a_file->fab))) )
		return	l_sts;

	a_file->rsa[a_file->nam.nam$b_rsl] = '\0';
	a_file->chan = (uint16_t) a_file->fab.fab$l_stv;

	if ( a_file->fhc.xab$l_ebk )
		a_file->bytes = (uint64_t) (a_file->fhc.xab$l_ebk - 1) * YADCX$K_BLKSZ + a_file->fhc.xab$w_ffb;
	a_file->blocks = (uint32_t) ((a_file->bytes + YADCX$K_BLKSZ - 1) / YADCX$K_BLKSZ);

	return	l_sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Create a new file for block writes (RMS $CREATE with FAB$M_UFO).  The
**	record attributes given here are placeholders, the real ones are put
**	by yadcx$close() from a FAT.
**
**  INPUTS:
**
**	a_name	-	file specification, need not be zero-terminated
**	a_namelen -	its length
**	a_defname -	default file specification, .ASCIZ, or NULL
**	a_blocks -	initial allocation
**
**  OUTPUTS:
**
**	a_file	-	file context: channel, resultant name
**
**  RETURNS:
**
**	condition value of the $CREATE
**--
*/
unsigned int	yadcx$create	(
		YADCX$FILE	*a_file,
		char		*a_name,
		int		a_namelen,
		char		*a_defname,
		uint32_t	a_blocks
			)
{
unsigned int	l_sts;

	s_init(a_file, a_name, a_namelen, a_defname);
	a_file->fab.fab$b_fac = FAB$M_PUT;
	a_file->fab.fab$b_rfm = FAB$C_UDF;
	a_file->fab.fab$l_alq = a_blocks;

	if ( !(1 & (l_sts = sys$create(&a_file->fab))) )
		return	l_sts;

	a_file->rsa[a_file->nam.nam$b_rsl] = '\0';
	a_file->chan = (uint16_t) a_file->fab.fab$l_stv;

	return	l_sts;
}


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
YADCX$IOSB	l_iosb;

	for ( ; a_blocks; a_blocks -= l_n, a_vbn += l_n, a_buf += l_n * YADCX$K_BLKSZ)
		{
		l_n = (a_blocks > YADCX$K_IOBLKS) ? YADCX$K_IOBLKS : a_blocks;

		l_sts = sys$qiow(EFN$C_ENF, a_file->chan, a_func, (void *) &l_iosb, 0, 0, a_buf, l_n * YADCX$K_BLKSZ, a_vbn, 0, 0, 0);
		if ( 1 & l_sts )
			l_sts = l_iosb.status;
		if ( !(1 & l_sts) )
			return	l_sts;
		}

	return	SS$_NORMAL;
}

unsigned int	yadcx$read	(
		YADCX$FILE	*a_file,
		uint32_t	a_vbn,
		void		*a_buf,
		uint32_t	a_blocks
			)
{
	return	s_xfer(a_file, IO$_READVBLK, a_vbn, (char *) a_buf, a_blocks);
}

unsigned int	yadcx$write	(
		YADCX$FILE	*a_file,
		uint32_t	a_vbn,
		void		*a_buf,
		uint32_t	a_blocks
			)
{
	return	s_xfer(a_file, IO$_WRITEVBLK, a_vbn, (char *) a_buf, a_blocks);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Build the FIB descriptor that addresses the file by its FID.
**--
*/
static void	s_fib	(
		YADCX$FILE		*a_file,
		FIBDEF			*a_fib,
		struct dsc$descriptor_s	*a_dsc
			)
{
	memset(a_fib, 0, sizeof(FIBDEF));
	memcpy(&a_fib->fib$w_fid, a_file->nam.nam$w_fid, sizeof(a_file->nam.nam$w_fid));

	a_dsc->dsc$w_length = sizeof(FIBDEF);
	a_dsc->dsc$b_dtype = 0;
	a_dsc->dsc$b_class = 0;
	a_dsc->dsc$a_pointer = (char *) a_fib;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Read the file's RMS record attributes (the 32-byte FAT) with an
**	IO$_ACCESS on a channel of its own to the file's device.
**
**  INPUTS:
**
**	a_file	-	file context from yadcx$open()
**
**  OUTPUTS:
**
**	a_fat	-	32-byte FAT
**
**  RETURNS:
**
**	condition value
**--
*/
unsigned int	yadcx$get_fat	(
		YADCX$FILE	*a_file,
		uint8_t		*a_fat
			)
{
unsigned int	l_sts;
uint16_t	l_chan;
FIBDEF		l_fib;
YADCX$IOSB	l_iosb;
YADCX$ATR	l_atr[2];
struct dsc$descriptor_s	l_dev, l_fibdsc;

	l_dev.dsc$w_length = (uint8_t) a_file->nam.nam$t_dvi[0];
	l_dev.dsc$b_dtype = DSC$K_DTYPE_T;
	l_dev.dsc$b_class = DSC$K_CLASS_S;
	l_dev.dsc$a_pointer = &a_file->nam.nam$t_dvi[1];

	if ( !(1 & (l_sts = sys$assign(&l_dev, &l_chan, 0, 0, 0))) )
		return	l_sts;

	s_fib(a_file, &l_fib, &l_fibdsc);

	l_atr[0].size = YADCX$K_FATSZ;
	l_atr[0].type = ATR$C_RECATTR;
	l_atr[0].addr = a_fat;
	l_atr[1].size = l_atr[1].type = 0;
	l_atr[1].addr = 0;

	l_sts = sys$qiow(EFN$C_ENF, l_chan, IO$_ACCESS, (void *) &l_iosb, 0, 0, &l_fibdsc, 0, 0, 0, (__int64) l_atr, 0);
	if ( 1 & l_sts )
		l_sts = l_iosb.status;

	sys$dassgn(l_chan);

	return	l_sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Close a file.  For a file being written a FAT is given: its HIBLK is
**	set to the real allocation and the whole FAT is written back by the
**	IO$_DEACCESS, which sets the record format, the record attributes and
**	the exact end of file in one go.
**
**  INPUTS:
**
**	a_file	-	file context
**	a_fat	-	32-byte FAT to put on the file, NULL - just close
**
**  RETURNS:
**
**	condition value
**--
*/
unsigned int	yadcx$close	(
		YADCX$FILE	*a_file,
		uint8_t		*a_fat
			)
{
unsigned int	l_sts = SS$_NORMAL;
FIBDEF		l_fib;
YADCX$IOSB	l_iosb;
YADCX$ATR	l_atr[2];
uint8_t		l_fat[YADCX$K_FATSZ];
struct dsc$descriptor_s	l_fibdsc;

	if ( a_fat )
		{
		memcpy(l_fat, a_fat, sizeof(l_fat));
		yadcx$fat_put(l_fat, YADCX$K_FAT_HIBLK, a_file->fab.fab$l_alq);

		s_fib(a_file, &l_fib, &l_fibdsc);

		l_atr[0].size = YADCX$K_FATSZ;
		l_atr[0].type = ATR$C_RECATTR;
		l_atr[0].addr = l_fat;
		l_atr[1].size = l_atr[1].type = 0;
		l_atr[1].addr = 0;

		l_sts = sys$qiow(EFN$C_ENF, a_file->chan, IO$_DEACCESS, (void *) &l_iosb, 0, 0, &l_fibdsc, 0, 0, 0, (__int64) l_atr, 0);
		if ( 1 & l_sts )
			l_sts = l_iosb.status;
		}

	sys$dassgn(a_file->chan);
	a_file->chan = 0;

	return	l_sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Drop a half-written output file: close the channel and delete the
**	file by its resultant name, so no truncated kit or payload is left.
**
**  INPUTS:
**
**	a_file	-	file context from yadcx$create()
**--
*/
void	yadcx$abort	(
		YADCX$FILE	*a_file
			)
{
struct FAB	l_fab = cc$rms_fab;

	if ( a_file->chan )
		sys$dassgn(a_file->chan);
	a_file->chan = 0;

	l_fab.fab$l_fna = a_file->rsa;
	l_fab.fab$b_fns = a_file->nam.nam$b_rsl;
	sys$erase(&l_fab);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Get the foreign command line, put the verb in front of it and parse it
**	with CLI$DCL_PARSE against the YADCX_CLI table (YADCX_CLI.CLD).  An
**	image started with RUN, or as a foreign command without anything
**	after it, has an empty line: nothing is parsed then, the caller takes
**	its defaults.
**
**  INPUTS:
**
**	a_verb	-	verb to parse under, .ASCIZ: "YADCX" or "SFX"
**	a_prompt -	1 - a line is always parsed, a missing required
**			parameter is prompted for (the builder); 0 - an
**			empty line is left alone (the stub)
**
**  OUTPUTS:
**
**	a_given	-	1 - a command line was parsed, 0 - it was empty
**
**  RETURNS:
**
**	condition value; CLI$DCL_PARSE has already shown what is wrong
**--
*/
extern int	YADCX_CLI;			/* The command table, from YADCX_CLI.OBJ		*/

unsigned int	yadcx$cli_parse	(
		char		*a_verb,
		int		a_prompt,
		int		*a_given
			)
{
unsigned int	l_sts;
uint16_t	l_len = 0;
int		l_vlen = (int) strlen(a_verb);
char		l_line[1024];
struct dsc$descriptor_s	l_dsc;

	*a_given = 0;

	memcpy(l_line, a_verb, l_vlen);
	l_line[l_vlen] = ' ';

	l_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_dsc.dsc$b_class = DSC$K_CLASS_S;
	l_dsc.dsc$a_pointer = l_line + l_vlen + 1;
	l_dsc.dsc$w_length = (uint16_t) (sizeof(l_line) - l_vlen - 1);

	if ( !(1 & (l_sts = lib$get_foreign(&l_dsc, 0, &l_len, 0))) )
		return	l_sts;

	if ( !l_len && !a_prompt )
		return	SS$_NORMAL;

	l_dsc.dsc$a_pointer = l_line;
	l_dsc.dsc$w_length = (uint16_t) (l_vlen + 1 + l_len);

	if ( !(1 & (l_sts = cli$dcl_parse(&l_dsc, (void *) &YADCX_CLI, lib$get_input, lib$get_input, 0))) )
		return	l_sts;

	*a_given = 1;

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Check for a qualifier/parameter and fetch its value.
**
**  INPUTS:
**
**	a_name	-	entity name, .ASCIZ
**	a_bufsz	-	size of a_buf, one byte is kept for the terminating zero
**
**  OUTPUTS:
**
**	a_buf	-	the value, .ASCIZ (yadcx$cli_value)
**	a_len	-	its length (yadcx$cli_value)
**
**  RETURNS:
**
**	yadcx$cli_present - 1 if the entity is present, 0 otherwise
**	yadcx$cli_value   - condition value of CLI$GET_VALUE
**--
*/
int	yadcx$cli_present	(
		char		*a_name
			)
{
struct dsc$descriptor_s	l_dsc;

	l_dsc.dsc$w_length = (uint16_t) strlen(a_name);
	l_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_dsc.dsc$b_class = DSC$K_CLASS_S;
	l_dsc.dsc$a_pointer = a_name;

	return	(1 & cli$present(&l_dsc)) ? 1 : 0;
}

unsigned int	yadcx$cli_value	(
		char		*a_name,
		char		*a_buf,
		int		a_bufsz,
		int		*a_len
			)
{
unsigned int	l_sts;
uint16_t	l_len = 0;
struct dsc$descriptor_s	l_name, l_val;

	l_name.dsc$w_length = (uint16_t) strlen(a_name);
	l_name.dsc$b_dtype = DSC$K_DTYPE_T;
	l_name.dsc$b_class = DSC$K_CLASS_S;
	l_name.dsc$a_pointer = a_name;

	l_val.dsc$w_length = (uint16_t) (a_bufsz - 1);
	l_val.dsc$b_dtype = DSC$K_DTYPE_T;
	l_val.dsc$b_class = DSC$K_CLASS_S;
	l_val.dsc$a_pointer = a_buf;

	l_sts = cli$get_value(&l_name, &l_val, &l_len);
	a_buf[(1 & l_sts) ? l_len : 0] = '\0';
	*a_len = (1 & l_sts) ? l_len : 0;

	return	l_sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Look for the kit header: the first block of the file that starts with
**	the marker.  The header found is checked for its own CRC-32 and for
**	the format version.
**
**  INPUTS:
**
**	a_file	-	file context from yadcx$open()
**
**  OUTPUTS:
**
**	a_vbn	-	VBN of the header block
**	a_hdr	-	copy of the header
**
**  RETURNS:
**
**	SS$_NORMAL	- found and valid
**	YADCX$_NOHEADER	- no header in the file
**	YADCX$_BADHDR, YADCX$_BADVER - found but not usable
**	or a condition value of the read
**--
*/
static char	s_buf[YADCX$K_IOBLKS * YADCX$K_BLKSZ];	/* I/O buffer of the scans			*/

unsigned int	yadcx$find_hdr	(
		YADCX$FILE	*a_file,
		uint32_t	*a_vbn,
		YADCX$HDR	*a_hdr
			)
{
unsigned int	l_sts;
uint32_t	l_vbn, l_n, l_crc;
char		l_marker[YADCX$K_MARKSZ];

	yadcx$marker(l_marker);

	for (l_vbn = 1; l_vbn <= a_file->blocks; l_vbn += l_n)
		{
		l_n = a_file->blocks - l_vbn + 1;
		if ( l_n > YADCX$K_IOBLKS )
			l_n = YADCX$K_IOBLKS;

		if ( !(1 & (l_sts = yadcx$read(a_file, l_vbn, s_buf, l_n))) )
			return	l_sts;

		for (uint32_t i = 0; i < l_n; i++)
			{
			if ( memcmp(s_buf + i * YADCX$K_BLKSZ, l_marker, YADCX$K_MARKSZ) )
				continue;

			*a_vbn = l_vbn + i;
			memcpy(a_hdr, s_buf + i * YADCX$K_BLKSZ, sizeof(YADCX$HDR));

			l_crc = 0xFFFFFFFF;
			yadcx$crc(&l_crc, a_hdr, (uint32_t) ((char *) &a_hdr->hdrcrc - (char *) a_hdr));
			if ( ~l_crc != a_hdr->hdrcrc )
				return	YADCX$_BADHDR;

			if ( a_hdr->version != YADCX$K_VERSION || a_hdr->hdrsz != sizeof(YADCX$HDR) )
				return	YADCX$_BADVER;

			return	SS$_NORMAL;
			}
		}

	return	YADCX$_NOHEADER;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Check that the whole payload is in the kit and compute its CRC-32 over
**	exactly <bytes> bytes.
**
**  INPUTS:
**
**	a_file	-	the kit, from yadcx$open()
**	a_vbn	-	VBN of the header
**	a_hdr	-	the header
**
**  OUTPUTS:
**
**	a_crc	-	CRC-32 computed
**
**  RETURNS:
**
**	SS$_NORMAL	- the CRC matches the recorded one
**	YADCX$_TRUNCATED, YADCX$_CRCERR, or a condition value of the read
**--
*/
unsigned int	yadcx$check	(
		YADCX$FILE	*a_file,
		uint32_t	a_vbn,
		YADCX$HDR	*a_hdr,
		uint32_t	*a_crc
			)
{
unsigned int	l_sts;
uint32_t	l_n, l_len;
uint64_t	l_left = a_hdr->bytes;

	*a_crc = 0;

	if ( a_vbn + a_hdr->blocks > a_file->blocks )
		return	YADCX$_TRUNCATED;

	*a_crc = 0xFFFFFFFF;

	for (uint32_t l_done = 0; l_done < a_hdr->blocks; l_done += l_n)
		{
		l_n = a_hdr->blocks - l_done;
		if ( l_n > YADCX$K_IOBLKS )
			l_n = YADCX$K_IOBLKS;

		if ( !(1 & (l_sts = yadcx$read(a_file, a_vbn + 1 + l_done, s_buf, l_n))) )
			return	l_sts;

		l_len = (l_left < l_n * YADCX$K_BLKSZ) ? (uint32_t) l_left : l_n * YADCX$K_BLKSZ;
		yadcx$crc(a_crc, s_buf, l_len);
		l_left -= l_len;
		}

	*a_crc = ~*a_crc;

	return	(*a_crc == a_hdr->crc) ? SS$_NORMAL : YADCX$_CRCERR;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Turn a quadword into decimal text for an !AZ of a message.  $PUTMSG
**	has a 32-bit message vector, its $FAOL takes !UQ as a longword value,
**	so the number is formatted here by $FAOL_64, which takes quadwords by
**	value.
**
**  INPUTS:
**
**	a_val	-	the value
**	a_bufsz	-	size of a_buf
**
**  OUTPUTS:
**
**	a_buf	-	the text, .ASCIZ
**--
*/
void	yadcx$fmt_uq	(
		uint64_t	a_val,
		char		*a_buf,
		int		a_bufsz
			)
{
uint16_t	l_len = 0;
uint64_t	l_prm[1];
$DESCRIPTOR(l_ctr, "!UQ");
struct dsc$descriptor_s	l_out;

	l_prm[0] = a_val;

	l_out.dsc$w_length = (uint16_t) (a_bufsz - 1);
	l_out.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out.dsc$b_class = DSC$K_CLASS_S;
	l_out.dsc$a_pointer = a_buf;

	if ( !(1 & sys$faol_64((void *) &l_ctr, &l_len, (void *) &l_out, (void *) l_prm)) )
		l_len = 0;
	a_buf[l_len] = '\0';
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Show what a kit carries: KITINFO and KITCRC.
**
**  INPUTS:
**
**	a_hdr	-	the kit header
**--
*/
void	yadcx$show	(
		YADCX$HDR	*a_hdr
			)
{
uint64_t	l_bytes, l_built;
char		l_txt[32];

	/*
	**  !%D takes the address of a quadword; the header is packed, so hand
	**  $FAO an aligned copy.
	*/
	memcpy(&l_bytes, &a_hdr->bytes, sizeof(l_bytes));
	memcpy(&l_built, &a_hdr->built, sizeof(l_built));
	yadcx$fmt_uq(l_bytes, l_txt, sizeof(l_txt));

	yadcx$putmsg(YADCX$_KITINFO, 0, 6, a_hdr->namelen, a_hdr->name, l_txt, a_hdr->blocks, &l_built, a_hdr->ident);
	yadcx$putmsg(YADCX$_KITCRC, 0, 1, a_hdr->crc);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Get the full file specification of the running image (for the stub - the kit).
**
**  INPUTS:
**
**	a_namesz -	size of a_name
**
**  OUTPUTS:
**
**	a_name	-	buffer for the name, .ASCIZ
**	a_len	-	its length
**
**  RETURNS:
**
**	condition value of the $GETJPIW
**--
*/
unsigned int	yadcx$self	(
		char		*a_name,
		int		a_namesz,
		int		*a_len
			)
{
unsigned int	l_sts;
uint16_t	l_len = 0;
ILE3		l_itm[2];
struct { uint32_t status, spare; }	l_iosb;

	l_itm[0].ile3$w_length = (uint16_t) (a_namesz - 1);
	l_itm[0].ile3$w_code = JPI$_IMAGNAME;
	l_itm[0].ile3$ps_bufaddr = a_name;
	l_itm[0].ile3$ps_retlen_addr = &l_len;
	memset(&l_itm[1], 0, sizeof(ILE3));

	l_sts = sys$getjpiw(EFN$C_ENF, 0, 0, l_itm, (void *) &l_iosb, 0, 0);
	if ( 1 & l_sts )
		l_sts = l_iosb.status;

	a_name[l_len] = '\0';
	*a_len = l_len;

	return	l_sts;
}
