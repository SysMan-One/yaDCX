#define	__NEW_STARLET	1

#include	<string.h>
#include	<stdarg.h>
#include	<rms.h>
#include	<descrip.h>
#include	<iodef.h>
#include	<fibdef.h>
#include	<atrdef.h>
#include	<efndef.h>
#include	<iosbdef.h>
#include	<jpidef.h>
#include	<iledef.h>
#include	<stsdef.h>
#include	<ssdef.h>
#include	<starlet.h>
#include	<lib$routines.h>
#include	<cli$routines.h>
#include	<dcx$routines.h>

#include	"YADCX.H"

#define	__MODULE__	"YADCX_COMMON"
#define	__IDENT__	"X-02-00"
#define	__REV__		"02.00.00"

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
**
**	26-SEP-2026	CLD	X-01-00ECO01: yadcx$putmsg_rms() - a failed
**				$OPEN/$CREATE shows its STS and STV.
**
**	26-SEP-2026	CLD	X-01-01: all strings by descriptor, no .ASCIZ/strlen
**				(open/create/putmsg_rms/cli_xxx/fmt_uq/self).
**
**	26-SEP-2026	CLD	X-01-01ECO01: IOSB from <iosbdef.h> instead of the
**				private YADCX$IOSB and the ad hoc $GETJPIW block.
**
**	26-SEP-2026	CLD	X-02-00: kit format 2 - yadcx$dcx_map(),
**				yadcx$compress(), yadcx$expand(), yadcx$percent();
**				find_hdr reads versions 1 and 2, check runs on the
**				stored data.
**--
*/

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
**	Output a YADCX message about a failed $OPEN/$CREATE, followed by the
**	RMS status and its STV, so the real reason (NOPRIV, device full ...)
**	shows as a third line, the way DCL utilities do it.
**
**  INPUTS:
**
**	a_msg	-	YADCX$_OPENIN or YADCX$_OPENOUT
**	a_file	-	the file context of the failed call, its FAB holds
**			the STS and STV
**	a_arg	-	file name for the !AS of the message
**
**  RETURNS:
**
**	a_msg
**--
*/
unsigned int	yadcx$putmsg_rms	(
		unsigned int	a_msg,
		YADCX$FILE	*a_file,
		struct dsc$descriptor_s	*a_arg
			)
{
uint32_t	l_vec[6];

	l_vec[0] = 5;
	l_vec[1] = a_msg;
	l_vec[2] = 1;
	l_vec[3] = (uint32_t) a_arg;
	l_vec[4] = a_file->fab.fab$l_sts;
	l_vec[5] = a_file->fab.fab$l_stv;
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
**	takes a string descriptor (16-bit length), so longer data - a DCX map
**	is often more than 64K - go through it in pieces of at most 65535.
**
**  INPUTS:
**
**	a_buf	-	data
**	a_len	-	data length
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
uint32_t	l_n;
char		*l_p = (char *) a_buf;
struct dsc$descriptor_s	l_dsc;

	for ( ; a_len; a_len -= l_n, l_p += l_n)
		{
		l_n = (a_len > 65535) ? 65535 : a_len;
		$DSCINIT(l_dsc, l_p, l_n);
		*a_crc = lib$crc(s_crctab, (int *) a_crc, &l_dsc);
		}
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
		YADCX$FILE		*a_file,
		struct dsc$descriptor_s	*a_name,
		struct dsc$descriptor_s	*a_defname
			)
{
	memset(a_file, 0, sizeof(YADCX$FILE));

	a_file->fab = cc$rms_fab;
	a_file->nam = cc$rms_nam;
	a_file->fhc = cc$rms_xabfhc;

	a_file->fab.fab$l_fna = a_name->dsc$a_pointer;
	a_file->fab.fab$b_fns = (uint8_t) a_name->dsc$w_length;
	if ( a_defname )
		{
		a_file->fab.fab$l_dna = a_defname->dsc$a_pointer;
		a_file->fab.fab$b_dns = (uint8_t) a_defname->dsc$w_length;
		}
	a_file->fab.fab$l_fop = FAB$M_UFO;
	a_file->fab.fab$l_nam = &a_file->nam;
	a_file->fab.fab$l_xab = (void *) &a_file->fhc;

	a_file->nam.nam$l_esa = a_file->esa;
	a_file->nam.nam$b_ess = sizeof(a_file->esa);
	a_file->nam.nam$l_rsa = a_file->rsa;
	a_file->nam.nam$b_rss = sizeof(a_file->rsa);

	$DSCINIT(a_file->rsadsc, a_file->rsa, 0);
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
**	a_name	-	file specification, descriptor
**	a_defname -	default file specification, descriptor, or NULL
**
**  OUTPUTS:
**
**	a_file	-	file context: channel, resultant name (rsadsc), bytes, blocks
**
**  RETURNS:
**
**	condition value of the $OPEN
**--
*/
unsigned int	yadcx$open	(
		YADCX$FILE		*a_file,
		struct dsc$descriptor_s	*a_name,
		struct dsc$descriptor_s	*a_defname
			)
{
unsigned int	l_sts;

	s_init(a_file, a_name, a_defname);
	a_file->fab.fab$b_fac = FAB$M_GET;
	a_file->fab.fab$b_shr = FAB$M_SHRGET;

	if ( !(1 & (l_sts = sys$open(&a_file->fab))) )
		return	l_sts;

	a_file->rsadsc.dsc$w_length = a_file->nam.nam$b_rsl;
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
**	a_name	-	file specification, descriptor
**	a_defname -	default file specification, descriptor, or NULL
**	a_blocks -	initial allocation
**
**  OUTPUTS:
**
**	a_file	-	file context: channel, resultant name (rsadsc)
**
**  RETURNS:
**
**	condition value of the $CREATE
**--
*/
unsigned int	yadcx$create	(
		YADCX$FILE		*a_file,
		struct dsc$descriptor_s	*a_name,
		struct dsc$descriptor_s	*a_defname,
		uint32_t		a_blocks
			)
{
unsigned int	l_sts;

	s_init(a_file, a_name, a_defname);
	a_file->fab.fab$b_fac = FAB$M_PUT;
	a_file->fab.fab$b_rfm = FAB$C_UDF;
	a_file->fab.fab$l_alq = a_blocks;

	if ( !(1 & (l_sts = sys$create(&a_file->fab))) )
		return	l_sts;

	a_file->rsadsc.dsc$w_length = a_file->nam.nam$b_rsl;
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
IOSB		l_iosb;
YADCX$ATR	l_atr[2];
struct dsc$descriptor_s	l_dev, l_fibdsc;

	$DSCINIT(l_dev, &a_file->nam.nam$t_dvi[1], (uint8_t) a_file->nam.nam$t_dvi[0]);	/* NAM$T_DVI is .ASCIC	*/

	if ( !(1 & (l_sts = sys$assign(&l_dev, &l_chan, 0, 0, 0))) )
		return	l_sts;

	s_fib(a_file, &l_fib, &l_fibdsc);

	l_atr[0].size = YADCX$K_FATSZ;
	l_atr[0].type = ATR$C_RECATTR;
	l_atr[0].addr = a_fat;
	l_atr[1].size = l_atr[1].type = 0;
	l_atr[1].addr = 0;

	l_sts = sys$qiow(EFN$C_ENF, l_chan, IO$_ACCESS, &l_iosb, 0, 0, &l_fibdsc, 0, 0, 0, (__int64) l_atr, 0);
	if ( 1 & l_sts )
		l_sts = l_iosb.iosb$w_status;

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
IOSB		l_iosb;
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

		l_sts = sys$qiow(EFN$C_ENF, a_file->chan, IO$_DEACCESS, &l_iosb, 0, 0, &l_fibdsc, 0, 0, 0, (__int64) l_atr, 0);
		if ( 1 & l_sts )
			l_sts = l_iosb.iosb$w_status;
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
**	a_verb	-	verb to parse under, descriptor: "YADCX" or "SFX"
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
		struct dsc$descriptor_s	*a_verb,
		int		a_prompt,
		int		*a_given
			)
{
unsigned int	l_sts;
uint16_t	l_len = 0;
int		l_vlen = a_verb->dsc$w_length;
char		l_line[1024];
struct dsc$descriptor_s	l_dsc;

	*a_given = 0;

	memcpy(l_line, a_verb->dsc$a_pointer, l_vlen);
	l_line[l_vlen] = ' ';

	$DSCINIT(l_dsc, l_line + l_vlen + 1, sizeof(l_line) - l_vlen - 1);

	if ( !(1 & (l_sts = lib$get_foreign(&l_dsc, 0, &l_len, 0))) )
		return	l_sts;

	if ( !l_len && !a_prompt )
		return	SS$_NORMAL;

	$DSCINIT(l_dsc, l_line, l_vlen + 1 + l_len);

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
**	a_name	-	entity name, descriptor
**
**  INPUTS/OUTPUTS:
**
**	a_val	-	(yadcx$cli_value) descriptor of the value buffer: on
**			input its length is the buffer size, on output the
**			length of the value (0 on a failure)
**
**  RETURNS:
**
**	yadcx$cli_present - 1 if the entity is present, 0 otherwise
**	yadcx$cli_value   - condition value of CLI$GET_VALUE
**--
*/
int	yadcx$cli_present	(
		struct dsc$descriptor_s	*a_name
			)
{
	return	(1 & cli$present(a_name)) ? 1 : 0;
}

unsigned int	yadcx$cli_value	(
		struct dsc$descriptor_s	*a_name,
		struct dsc$descriptor_s	*a_val
			)
{
unsigned int	l_sts;
uint16_t	l_len = 0;

	l_sts = cli$get_value(a_name, a_val, &l_len);
	a_val->dsc$w_length = (1 & l_sts) ? l_len : 0;

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
**	SS$_NORMAL	- found and valid; a version 1 header is brought to
**			  the version 2 form (method NONE, stored data = payload)
**	YADCX$_NOHEADER	- no header in the file
**	YADCX$_BADHDR, YADCX$_BADVER, YADCX$_BADMETH - found but not usable
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

			if ( !a_hdr->version || a_hdr->version > YADCX$K_VERSION || a_hdr->hdrsz != sizeof(YADCX$HDR) )
				return	YADCX$_BADVER;

			if ( a_hdr->version == 1 )
				{
				a_hdr->method = YADCX$K_METHOD_NONE;
				a_hdr->reclen = 0;
				a_hdr->mapbytes = 0;
				a_hdr->sbytes = a_hdr->bytes;
				a_hdr->sblocks = a_hdr->blocks;
				a_hdr->scrc = a_hdr->crc;
				}

			if ( a_hdr->method > YADCX$K_METHOD_DCX )
				return	YADCX$_BADMETH;

			return	SS$_NORMAL;
			}
		}

	return	YADCX$_NOHEADER;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Check that all the stored data are in the kit and compute their CRC-32
**	over exactly <sbytes> bytes.  Nothing is expanded here: for method NONE
**	this is the CRC of the payload itself, for DCX the one of the stream.
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
**	SS$_NORMAL	- the CRC matches the recorded <scrc>
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
uint64_t	l_left = a_hdr->sbytes;

	*a_crc = 0;

	if ( a_vbn + a_hdr->sblocks > a_file->blocks )
		return	YADCX$_TRUNCATED;

	*a_crc = 0xFFFFFFFF;

	for (uint32_t l_done = 0; l_done < a_hdr->sblocks; l_done += l_n)
		{
		l_n = a_hdr->sblocks - l_done;
		if ( l_n > YADCX$K_IOBLKS )
			l_n = YADCX$K_IOBLKS;

		if ( !(1 & (l_sts = yadcx$read(a_file, a_vbn + 1 + l_done, s_buf, l_n))) )
			return	l_sts;

		l_len = (l_left < l_n * YADCX$K_BLKSZ) ? (uint32_t) l_left : l_n * YADCX$K_BLKSZ;
		yadcx$crc(a_crc, s_buf, l_len);
		l_left -= l_len;
		}

	*a_crc = ~*a_crc;

	return	(*a_crc == a_hdr->scrc) ? SS$_NORMAL : YADCX$_CRCERR;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Turn a quadword into decimal text for an !AS of a message.  $PUTMSG
**	has a 32-bit message vector, its $FAOL takes !UQ as a longword value,
**	so the number is formatted here by $FAOL_64, which takes quadwords by
**	value.
**
**  INPUTS:
**
**	a_val	-	the value
**
**  INPUTS/OUTPUTS:
**
**	a_txt	-	descriptor of the text buffer: on input its length is
**			the buffer size, on output the length of the text
**--
*/
void	yadcx$fmt_uq	(
		uint64_t	a_val,
		struct dsc$descriptor_s	*a_txt
			)
{
uint16_t	l_len = 0;
uint64_t	l_prm[1];
$DESCRIPTOR(l_ctr, "!UQ");

	l_prm[0] = a_val;

	if ( !(1 & sys$faol_64((void *) &l_ctr, &l_len, (void *) a_txt, (void *) l_prm)) )
		l_len = 0;
	a_txt->dsc$w_length = l_len;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Show what a kit carries: KITINFO, KITCRC and, for a compressed one,
**	KITDCX.
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
uint32_t	l_int, l_frac;
char		l_buf[32];
struct dsc$descriptor_s	l_txt;

	/*
	**  !%D takes the address of a quadword; the header is packed, so hand
	**  $FAO an aligned copy.
	*/
	memcpy(&l_bytes, &a_hdr->bytes, sizeof(l_bytes));
	memcpy(&l_built, &a_hdr->built, sizeof(l_built));
	$DSCINIT(l_txt, l_buf, sizeof(l_buf));
	yadcx$fmt_uq(l_bytes, &l_txt);

	yadcx$putmsg(YADCX$_KITINFO, 0, 6, a_hdr->namelen, a_hdr->name, &l_txt, a_hdr->blocks, &l_built, a_hdr->ident);
	yadcx$putmsg(YADCX$_KITCRC, 0, 1, a_hdr->crc);

	if ( a_hdr->method == YADCX$K_METHOD_DCX )
		{
		memcpy(&l_bytes, &a_hdr->sbytes, sizeof(l_bytes));
		$DSCINIT(l_txt, l_buf, sizeof(l_buf));
		yadcx$fmt_uq(l_bytes, &l_txt);
		yadcx$percent(a_hdr->sbytes, a_hdr->bytes, &l_int, &l_frac);
		yadcx$putmsg(YADCX$_KITDCX, 0, 4, &l_txt, a_hdr->sblocks, l_int, l_frac);
		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Get the full file specification of the running image (for the stub - the kit).
**
**  INPUTS/OUTPUTS:
**
**	a_name	-	descriptor of the name buffer: on input its length is
**			the buffer size, on output the length of the name
**
**  RETURNS:
**
**	condition value of the $GETJPIW
**--
*/
unsigned int	yadcx$self	(
		struct dsc$descriptor_s	*a_name
			)
{
unsigned int	l_sts;
uint16_t	l_len = 0;
ILE3		l_itm[2];
IOSB		l_iosb;

	l_itm[0].ile3$w_length = a_name->dsc$w_length;
	l_itm[0].ile3$w_code = JPI$_IMAGNAME;
	l_itm[0].ile3$ps_bufaddr = a_name->dsc$a_pointer;
	l_itm[0].ile3$ps_retlen_addr = &l_len;
	memset(&l_itm[1], 0, sizeof(ILE3));

	l_sts = sys$getjpiw(EFN$C_ENF, 0, 0, l_itm, &l_iosb, 0, 0);
	if ( 1 & l_sts )
		l_sts = l_iosb.iosb$l_getxxi_status;		/* $GETxxI: the status is a longword		*/

	a_name->dsc$w_length = l_len;

	return	l_sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Part of a whole in percent with two decimals, for a "!UL.!2ZL%" of a
**	message (no floating point in the images).
**
**  INPUTS:
**
**	a_part	-	the part
**	a_whole	-	the whole, 0 gives 0.00
**
**  OUTPUTS:
**
**	a_int	-	integer percent
**	a_frac	-	hundredths
**--
*/
void	yadcx$percent	(
		uint64_t	a_part,
		uint64_t	a_whole,
		uint32_t	*a_int,
		uint32_t	*a_frac
			)
{
uint64_t	l_p = a_whole ? a_part * 10000 / a_whole : 0;

	*a_int = (uint32_t) (l_p / 100);
	*a_frac = (uint32_t) (l_p % 100);
}


/*
**  The stored data are a byte stream laid over whole blocks.  A writer
**  collects bytes in s_wbuf and writes full buffers at consecutive VBNs; a
**  NULL file only counts and CRCs (the sizing pass of the builder).  A
**  reader refills s_rbuf from consecutive VBNs, never past its last block.
*/
typedef struct yadcx_wr_t {
	YADCX$FILE	*file;			/* Output file, NULL - count only			*/
	uint32_t	vbn;			/* VBN of the start of the buffer			*/
	uint32_t	pos;			/* Bytes in the buffer					*/
	uint32_t	crc;			/* Running CRC of the bytes put				*/
	uint64_t	total;			/* Bytes put						*/
} YADCX$WR;

typedef struct yadcx_rd_t {
	YADCX$FILE	*file;			/* Input file						*/
	uint32_t	vbn;			/* Next VBN to read					*/
	uint32_t	left;			/* Blocks not read yet					*/
	uint32_t	pos;			/* Next byte in the buffer				*/
	uint32_t	len;			/* Bytes in the buffer					*/
} YADCX$RD;

static char	s_wbuf[YADCX$K_IOBLKS * YADCX$K_BLKSZ];	/* Writer buffer				*/
static char	s_rbuf[YADCX$K_IOBLKS * YADCX$K_BLKSZ];	/* Reader buffer				*/
static char	s_cbuf[YADCX$K_OUTSZ];			/* One compressed record			*/
static char	s_xbuf[YADCX$K_RECLEN];			/* One expanded record				*/

static	$DESCRIPTOR(s_n_analyze, "DCX$ANALYZE_xxx");	/* Names for YADCX$_DCXERR			*/
static	$DESCRIPTOR(s_n_makemap, "DCX$MAKE_MAP");
static	$DESCRIPTOR(s_n_compress, "DCX$COMPRESS_xxx");
static	$DESCRIPTOR(s_n_expand, "DCX$EXPAND_xxx");
static	$DESCRIPTOR(s_n_stream, "the stored data stream");


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Writer: put bytes, flush the rest (zero padded to the block boundary).
**
**  INPUTS:
**
**	a_wr	-	writer context
**	a_data	-	bytes to put
**	a_len	-	their number
**
**  RETURNS:
**
**	condition value of the write
**--
*/
static unsigned int	s_wr_put	(
		YADCX$WR	*a_wr,
		void		*a_data,
		uint32_t	a_len
			)
{
unsigned int	l_sts;
uint32_t	l_n;
char		*l_p = (char *) a_data;

	yadcx$crc(&a_wr->crc, l_p, a_len);
	a_wr->total += a_len;

	if ( !a_wr->file )
		return	SS$_NORMAL;

	for ( ; a_len; a_len -= l_n, l_p += l_n)
		{
		l_n = sizeof(s_wbuf) - a_wr->pos;
		if ( l_n > a_len )
			l_n = a_len;
		memcpy(s_wbuf + a_wr->pos, l_p, l_n);
		a_wr->pos += l_n;

		if ( a_wr->pos == sizeof(s_wbuf) )
			{
			if ( !(1 & (l_sts = yadcx$write(a_wr->file, a_wr->vbn, s_wbuf, YADCX$K_IOBLKS))) )
				return	l_sts;
			a_wr->vbn += YADCX$K_IOBLKS;
			a_wr->pos = 0;
			}
		}

	return	SS$_NORMAL;
}

static unsigned int	s_wr_flush	(
		YADCX$WR	*a_wr
			)
{
uint32_t	l_n = (a_wr->pos + YADCX$K_BLKSZ - 1) / YADCX$K_BLKSZ;

	if ( !a_wr->file || !a_wr->pos )
		return	SS$_NORMAL;

	memset(s_wbuf + a_wr->pos, 0, l_n * YADCX$K_BLKSZ - a_wr->pos);
	a_wr->pos = 0;

	return	yadcx$write(a_wr->file, a_wr->vbn, s_wbuf, l_n);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Reader: get exactly a_len bytes from the stream.
**
**  INPUTS:
**
**	a_rd	-	reader context
**	a_len	-	number of bytes
**
**  OUTPUTS:
**
**	a_data	-	the bytes
**
**  RETURNS:
**
**	condition value of the read, SS$_ENDOFFILE when the stored blocks end
**--
*/
static unsigned int	s_rd_get	(
		YADCX$RD	*a_rd,
		void		*a_data,
		uint32_t	a_len
			)
{
unsigned int	l_sts;
uint32_t	l_n;
char		*l_p = (char *) a_data;

	for ( ; a_len; a_len -= l_n, l_p += l_n)
		{
		if ( a_rd->pos == a_rd->len )
			{
			if ( !a_rd->left )
				return	SS$_ENDOFFILE;
			l_n = (a_rd->left > YADCX$K_IOBLKS) ? YADCX$K_IOBLKS : a_rd->left;
			if ( !(1 & (l_sts = yadcx$read(a_rd->file, a_rd->vbn, s_rbuf, l_n))) )
				return	l_sts;
			a_rd->vbn += l_n;
			a_rd->left -= l_n;
			a_rd->pos = 0;
			a_rd->len = l_n * YADCX$K_BLKSZ;
			}

		l_n = a_rd->len - a_rd->pos;
		if ( l_n > a_len )
			l_n = a_len;
		memcpy(l_p, s_rbuf + a_rd->pos, l_n);
		a_rd->pos += l_n;
		}

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Read the payload record by record, YADCX$K_RECLEN bytes (63 blocks)
**	each, the last one shorter, into s_buf.
**
**  INPUTS:
**
**	a_pay	-	the payload, open
**	a_rec	-	record number, 0-based
**
**  OUTPUTS:
**
**	a_dsc	-	descriptor of the record in s_buf
**
**  RETURNS:
**
**	condition value of the read
**--
*/
static unsigned int	s_get_rec	(
		YADCX$FILE	*a_pay,
		uint32_t	a_rec,
		struct dsc$descriptor_s	*a_dsc
			)
{
uint64_t	l_off = (uint64_t) a_rec * YADCX$K_RECLEN;
uint32_t	l_len, l_blks;

	l_len = (a_pay->bytes - l_off < YADCX$K_RECLEN) ? (uint32_t) (a_pay->bytes - l_off) : YADCX$K_RECLEN;
	l_blks = (l_len + YADCX$K_BLKSZ - 1) / YADCX$K_BLKSZ;

	$DSCINIT(*a_dsc, s_buf, l_len);

	return	yadcx$read(a_pay, 1 + a_rec * YADCX$K_RECBLKS, s_buf, l_blks);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	DCX analysis of the payload and the map; the CRC-32 of the payload is
**	computed on the first pass.  DCX$MAKE_MAP asks for another pass with
**	DCX$_AGAIN, an informational status (DCX$_NORMAL is a success one).
**
**  INPUTS:
**
**	a_pay	-	the payload, open, not empty
**
**  OUTPUTS:
**
**	a_map	-	address of the map (from LIB$GET_VM, released by the
**			caller with LIB$FREE_VM of a_mapsz bytes)
**	a_mapsz	-	size of the map
**	a_crc	-	CRC-32 of the payload
**
**  RETURNS:
**
**	SS$_NORMAL or a message code already shown
**--
*/
unsigned int	yadcx$dcx_map	(
		YADCX$FILE	*a_pay,
		void		**a_map,
		uint32_t	*a_mapsz,
		uint32_t	*a_crc
			)
{
unsigned int	l_sts, l_ctx = 0, l_pass = 0;
uint32_t	l_nrec = (uint32_t) ((a_pay->bytes + YADCX$K_RECLEN - 1) / YADCX$K_RECLEN);
struct dsc$descriptor_s	l_rec;

	*a_map = 0;
	*a_mapsz = 0;
	*a_crc = 0xFFFFFFFF;

	if ( !(1 & (l_sts = dcx$analyze_init(&l_ctx))) )
		return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_analyze);

	do	{
		l_pass++;

		for (uint32_t i = 0; i < l_nrec; i++)
			{
			if ( !(1 & (l_sts = s_get_rec(a_pay, i, &l_rec))) )
				{
				dcx$analyze_done(&l_ctx);
				return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, &a_pay->rsadsc);
				}
			if ( l_pass == 1 )
				yadcx$crc(a_crc, l_rec.dsc$a_pointer, l_rec.dsc$w_length);
			if ( !(1 & (l_sts = dcx$analyze_data(&l_ctx, &l_rec))) )
				{
				dcx$analyze_done(&l_ctx);
				return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_analyze);
				}
			}

		if ( !(1 & (l_sts = dcx$make_map(&l_ctx, a_map, a_mapsz))) )
			{
			dcx$analyze_done(&l_ctx);
			return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_makemap);
			}
		}
	while ( $VMS_STATUS_SEVERITY(l_sts) == STS$K_INFO );

	*a_crc = ~*a_crc;
	dcx$analyze_done(&l_ctx);			/* The map is not part of the context, it stays	*/

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Compress the payload with a DCX map into the stored data stream: the
**	map, then per record a longword length and the compressed record, or
**	the record as is (YADCX$M_RAW) when DCX can not make it shorter.
**	Without a kit file nothing is written, only the size and the CRC are
**	computed (the sizing pass: the kit is allocated exactly).
**
**  INPUTS:
**
**	a_pay	-	the payload, open
**	a_map	-	ADDRESS of the map pointer: DCX$COMPRESS_INIT takes
**			map_addr.ra.r, see [DCX.SRC]COMPRESS.B32
**	a_mapsz	-	size of the map
**	a_kit	-	the kit, or NULL for the sizing pass
**	a_vbn	-	first VBN of the stored data in the kit
**
**  OUTPUTS:
**
**	a_sbytes -	size of the stored data
**	a_scrc	-	CRC-32 of the stored data
**	a_raws	-	number of records stored as is
**
**  RETURNS:
**
**	SS$_NORMAL or a message code already shown
**--
*/
unsigned int	yadcx$compress	(
		YADCX$FILE	*a_pay,
		void		**a_map,
		uint32_t	a_mapsz,
		YADCX$FILE	*a_kit,
		uint32_t	a_vbn,
		uint64_t	*a_sbytes,
		uint32_t	*a_scrc,
		uint32_t	*a_raws
			)
{
unsigned int	l_sts, l_ctx = 0;
uint32_t	l_nrec = (uint32_t) ((a_pay->bytes + YADCX$K_RECLEN - 1) / YADCX$K_RECLEN), l_hdr;
uint16_t	l_olen;
struct dsc$descriptor_s	l_rec, l_out;
YADCX$WR	l_wr;

	memset(&l_wr, 0, sizeof(l_wr));
	l_wr.file = a_kit;
	l_wr.vbn = a_vbn;
	l_wr.crc = 0xFFFFFFFF;
	*a_raws = 0;

	if ( !(1 & (l_sts = s_wr_put(&l_wr, *a_map, a_mapsz))) )
		return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &a_kit->rsadsc);

	if ( !(1 & (l_sts = dcx$compress_init(&l_ctx, a_map))) )
		return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_compress);

	for (uint32_t i = 0; i < l_nrec; i++)
		{
		if ( !(1 & (l_sts = s_get_rec(a_pay, i, &l_rec))) )
			{
			dcx$compress_done(&l_ctx);
			return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, &a_pay->rsadsc);
			}

		/*
		**  DCX-W-TRUNC: the compressed record does not fit even twice
		**  the size - such data (already compressed, encrypted) is
		**  stored as is, as is a record that did not get shorter.
		*/
		$DSCINIT(l_out, s_cbuf, sizeof(s_cbuf));
		l_olen = 0;
		l_sts = dcx$compress_data(&l_ctx, &l_rec, &l_out, &l_olen);
		if ( !(1 & l_sts) && $VMS_STATUS_SEVERITY(l_sts) != STS$K_WARNING )
			{
			dcx$compress_done(&l_ctx);
			return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_compress);
			}

		if ( !(1 & l_sts) || l_olen >= l_rec.dsc$w_length )
			{
			l_hdr = l_rec.dsc$w_length | YADCX$M_RAW;
			(*a_raws)++;
			if ( 1 & (l_sts = s_wr_put(&l_wr, &l_hdr, sizeof(l_hdr))) )
				l_sts = s_wr_put(&l_wr, l_rec.dsc$a_pointer, l_rec.dsc$w_length);
			}
		else	{
			l_hdr = l_olen;
			if ( 1 & (l_sts = s_wr_put(&l_wr, &l_hdr, sizeof(l_hdr))) )
				l_sts = s_wr_put(&l_wr, s_cbuf, l_olen);
			}

		if ( !(1 & l_sts) )
			{
			dcx$compress_done(&l_ctx);
			return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &a_kit->rsadsc);
			}
		}

	dcx$compress_done(&l_ctx);

	if ( !(1 & (l_sts = s_wr_flush(&l_wr))) )
		return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &a_kit->rsadsc);

	*a_sbytes = l_wr.total;
	*a_scrc = ~l_wr.crc;

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Bring the payload back from the stored data of a kit - copy for NONE,
**	DCX expansion for DCX - and compute its CRC-32.  The stored data have
**	been checked by yadcx$check() already.  Without an output file nothing
**	is written: that is the full check of /VERIFY and YADCX/LIST.
**
**  INPUTS:
**
**	a_kit	-	the kit, open
**	a_vbn	-	VBN of the header
**	a_hdr	-	the header
**	a_out	-	output file from yadcx$create(), or NULL
**
**  OUTPUTS:
**
**	a_crc	-	CRC-32 of the payload brought back, to compare with
**			a_hdr->crc
**
**  RETURNS:
**
**	SS$_NORMAL or a message code already shown
**--
*/
unsigned int	yadcx$expand	(
		YADCX$FILE	*a_kit,
		uint32_t	a_vbn,
		YADCX$HDR	*a_hdr,
		YADCX$FILE	*a_out,
		uint32_t	*a_crc
			)
{
unsigned int	l_sts, l_ctx = 0;
uint32_t	l_hdr, l_len, l_want, l_mapsz = a_hdr->mapbytes;
uint64_t	l_left = a_hdr->bytes;
uint16_t	l_olen;
char		*l_data;
void		*l_map = 0;
struct dsc$descriptor_s	l_in, l_out;
YADCX$RD	l_rd;
YADCX$WR	l_wr;

	memset(&l_rd, 0, sizeof(l_rd));
	l_rd.file = a_kit;
	l_rd.vbn = a_vbn + 1;
	l_rd.left = a_hdr->sblocks;

	memset(&l_wr, 0, sizeof(l_wr));
	l_wr.file = a_out;
	l_wr.vbn = 1;
	l_wr.crc = 0xFFFFFFFF;

	/*
	**  NONE: the stored data are the payload
	*/
	if ( a_hdr->method == YADCX$K_METHOD_NONE )
		{
		for ( ; l_left; l_left -= l_len)
			{
			l_len = (l_left < YADCX$K_RECLEN) ? (uint32_t) l_left : YADCX$K_RECLEN;
			if ( !(1 & (l_sts = s_rd_get(&l_rd, s_xbuf, l_len))) )
				return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, &a_kit->rsadsc);
			if ( !(1 & (l_sts = s_wr_put(&l_wr, s_xbuf, l_len))) )
				return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &a_out->rsadsc);
			}
		}

	/*
	**  DCX: the map, then the records
	*/
	else	{
		if ( !(1 & (l_sts = lib$get_vm((int *) &l_mapsz, &l_map))) )
			return	yadcx$putmsg(YADCX$_SYSERR, l_sts, 1, &s_n_expand);

		if ( !(1 & (l_sts = s_rd_get(&l_rd, l_map, l_mapsz))) )
			{
			lib$free_vm((int *) &l_mapsz, &l_map);
			return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, &a_kit->rsadsc);
			}

		if ( !(1 & (l_sts = dcx$expand_init(&l_ctx, &l_map))) )	/* map_addr.ra.r: &pointer	*/
			{
			lib$free_vm((int *) &l_mapsz, &l_map);
			return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_expand);
			}

		for ( ; l_left; l_left -= l_len)
			{
			l_want = (l_left < a_hdr->reclen) ? (uint32_t) l_left : a_hdr->reclen;

			if ( 1 & (l_sts = s_rd_get(&l_rd, &l_hdr, sizeof(l_hdr))) )
				{
				l_len = l_hdr & ~YADCX$M_RAW;
				l_sts = (l_len > sizeof(s_cbuf)) ? SS$_BADPARAM : s_rd_get(&l_rd, s_cbuf, l_len);
				}
			if ( !(1 & l_sts) )
				{
				dcx$expand_done(&l_ctx);
				lib$free_vm((int *) &l_mapsz, &l_map);
				if ( l_sts == SS$_BADPARAM || l_sts == SS$_ENDOFFILE )
					return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_stream);
				return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, &a_kit->rsadsc);
				}

			if ( l_hdr & YADCX$M_RAW )
				l_data = s_cbuf;
			else	{
				$DSCINIT(l_in, s_cbuf, l_len);
				$DSCINIT(l_out, s_xbuf, sizeof(s_xbuf));
				l_olen = 0;
				if ( !(1 & (l_sts = dcx$expand_data(&l_ctx, &l_in, &l_out, &l_olen))) )
					{
					dcx$expand_done(&l_ctx);
					lib$free_vm((int *) &l_mapsz, &l_map);
					return	yadcx$putmsg(YADCX$_DCXERR, l_sts, 1, &s_n_expand);
					}
				l_len = l_olen;
				l_data = s_xbuf;
				}

			if ( l_len != l_want )				/* Every record but the last is full	*/
				{
				dcx$expand_done(&l_ctx);
				lib$free_vm((int *) &l_mapsz, &l_map);
				return	yadcx$putmsg(YADCX$_DCXERR, SS$_BADPARAM, 1, &s_n_stream);
				}

			if ( !(1 & (l_sts = s_wr_put(&l_wr, l_data, l_len))) )
				{
				dcx$expand_done(&l_ctx);
				lib$free_vm((int *) &l_mapsz, &l_map);
				return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &a_out->rsadsc);
				}
			}

		dcx$expand_done(&l_ctx);
		lib$free_vm((int *) &l_mapsz, &l_map);
		}

	if ( !(1 & (l_sts = s_wr_flush(&l_wr))) )
		return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &a_out->rsadsc);

	*a_crc = ~l_wr.crc;

	return	SS$_NORMAL;
}
