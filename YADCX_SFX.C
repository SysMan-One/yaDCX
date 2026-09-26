#define	__NEW_STARLET	1

#include	<string.h>
#include	<rms.h>
#include	<efndef.h>
#include	<stsdef.h>
#include	<ssdef.h>
#include	<starlet.h>

#include	"YADCX.H"

#define	__MODULE__	"YADCX_SFX"
#define	__IDENT__	"X-02-00"
#define	__REV__		"02.00.00"

/*
**++
**  FACILITY:	yaDCX -- Yet Another Data Compression/eXpansion kit builder
**
**  MODULE DESCRIPTION:
**
**	The self-extracting stub.  YADCX.EXE puts a copy of this image in
**	front of a kit header and a payload (normally a BACKUP saveset); the
**	kit is then run like any image:
**
**		$ RUN kit				restore the payload
**		$ KIT :== $dev:[dir]kit.EXE
**		$ KIT /OUTPUT=[.SUB]OTHER.BCK		restore under another name
**		$ KIT /LIST				show what is inside
**		$ KIT /VERIFY				check the CRC, write nothing
**
**	The stub finds its own image by JPI$_IMAGNAME (a kit may be renamed at
**	will), finds the header block after its own blocks, checks the header,
**	checks the payload's CRC-32 BEFORE anything is written, then writes the
**	payload blocks to a new file and puts the payload's original RMS record
**	attributes (FAT) back on it, so the file is restored exactly - record
**	format, record size, end of file - whatever it was.
**
**  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  26-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	26-SEP-2026	CLD	X-01-00: initial version, replaces SFX.C of
**				[LAISHEV.YADX_KIT]: payload name and record
**				attributes come from the kit header instead of
**				being built in (DX_V29.BCK, Fixed 32256), the
**				payload is checked by CRC-32 before it is
**				written, CLI qualifiers /OUTPUT, /LIST, /VERIFY.
**
**	26-SEP-2026	CLD	X-01-00ECO01: OPENIN/OPENOUT show the RMS STV
**				(yadcx$putmsg_rms).
**
**	26-SEP-2026	CLD	X-01-01: all strings by descriptor, no .ASCIZ/strlen;
**				the stored name is used in place as the default.
**
**	26-SEP-2026	CLD	X-02-00: kit format 2 - restore and /VERIFY through
**				yadcx$expand() (NONE and DCX), EXPCRC, BADMETH.
**--
*/

static	$DESCRIPTOR(s_q_verb, "SFX");			/* Verb in YADCX_CLI.CLD			*/
static	$DESCRIPTOR(s_q_list, "LIST");
static	$DESCRIPTOR(s_q_verify, "VERIFY");
static	$DESCRIPTOR(s_q_output, "OUTPUT");
static	$DESCRIPTOR(s_jpi, "$GETJPIW JPI$_IMAGNAME");


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write the payload out of the kit to a new file - copied or expanded -
**	and put its original record attributes on it.  The CRC-32 of the
**	stored data has been checked already; the one of the payload is
**	checked here, a mismatch deletes the new file (YADCX$_EXPCRC).
**
**  INPUTS:
**
**	a_kit	-	the kit, open
**	a_vbn	-	VBN of the header
**	a_hdr	-	the header
**	a_out	-	/OUTPUT value, descriptor, may be empty - then the
**			name stored in the kit goes to the current default
**			directory
**
**  RETURNS:
**
**	YADCX$_RESTORED or an error message code (already shown)
**--
*/
static unsigned int	s_restore	(
		YADCX$FILE	*a_kit,
		uint32_t	a_vbn,
		YADCX$HDR	*a_hdr,
		struct dsc$descriptor_s	*a_out
			)
{
unsigned int	l_sts;
uint32_t	l_crc;
uint64_t	l_bytes;
char		l_buf[32];
struct dsc$descriptor_s	l_defname, l_txt;
YADCX$FILE	l_out;

	$DSCINIT(l_defname, a_hdr->name, a_hdr->namelen);	/* The name stored in the kit		*/

	if ( a_out->dsc$w_length )
		l_sts = yadcx$create(&l_out, a_out, &l_defname, a_hdr->blocks);
	else	l_sts = yadcx$create(&l_out, &l_defname, 0, a_hdr->blocks);

	if ( !(1 & l_sts) )
		return	yadcx$putmsg_rms(YADCX$_OPENOUT, &l_out, a_out->dsc$w_length ? a_out : &l_defname);

	/*
	**  Copy (NONE) or expand (DCX) into the new file; the CRC-32 of what was
	**  written must be the one of the original payload, else the file goes.
	*/
	if ( !(1 & (l_sts = yadcx$expand(a_kit, a_vbn, a_hdr, &l_out, &l_crc))) )
		{
		yadcx$abort(&l_out);
		return	l_sts;
		}

	if ( l_crc != a_hdr->crc )
		{
		yadcx$abort(&l_out);
		return	yadcx$putmsg(YADCX$_EXPCRC, 0, 2, a_hdr->crc, l_crc);
		}

	if ( !(1 & (l_sts = yadcx$close(&l_out, a_hdr->fat))) )
		{
		yadcx$abort(&l_out);
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, &l_out.rsadsc);
		}

	memcpy(&l_bytes, &a_hdr->bytes, sizeof(l_bytes));		/* The header is packed				*/
	$DSCINIT(l_txt, l_buf, sizeof(l_buf));
	yadcx$fmt_uq(l_bytes, &l_txt);

	return	yadcx$putmsg(YADCX$_RESTORED, 0, 3, &l_out.rsadsc, &l_txt, l_crc);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Main: parse the command line (if any), open the own image, find and
**	check the header and the payload, then list, verify or restore.
**
**  IMPLICIT INPUTS:
**
**	the foreign command line, the running image file
**
**  RETURNS:
**
**	exits with the final message code, the message itself is already
**	shown (STS$M_INHIB_MSG), so $STATUS/$SEVERITY are meaningful in DCL
**--
*/
int	main	(void)
{
unsigned int	l_sts;
int		l_given, l_list = 0, l_verify = 0;
uint32_t	l_vbn, l_crc;
char		l_selfbuf[NAM$C_MAXRSS], l_outbuf[NAM$C_MAXRSS];
struct dsc$descriptor_s	l_self, l_out;
YADCX$FILE	l_kit;
YADCX$HDR	l_hdr;

	$DSCINIT(l_self, l_selfbuf, sizeof(l_selfbuf));
	$DSCINIT(l_out, l_outbuf, 0);

	if ( !(1 & (l_sts = yadcx$cli_parse(&s_q_verb, 0, &l_given))) )
		sys$exit(l_sts | STS$M_INHIB_MSG);

	if ( l_given )
		{
		l_list = yadcx$cli_present(&s_q_list);
		l_verify = yadcx$cli_present(&s_q_verify);
		if ( yadcx$cli_present(&s_q_output) )
			{
			l_out.dsc$w_length = sizeof(l_outbuf);
			yadcx$cli_value(&s_q_output, &l_out);
			}
		}

	yadcx$crc_init();

	if ( !(1 & (l_sts = yadcx$self(&l_self))) )
		sys$exit(yadcx$putmsg(YADCX$_SYSERR, l_sts, 1, &s_jpi) | STS$M_INHIB_MSG);

	if ( !(1 & (l_sts = yadcx$open(&l_kit, &l_self, 0))) )
		sys$exit(yadcx$putmsg_rms(YADCX$_OPENIN, &l_kit, &l_self) | STS$M_INHIB_MSG);

	/*
	**  The header: marker, own CRC, version
	*/
	l_sts = yadcx$find_hdr(&l_kit, &l_vbn, &l_hdr);
	if ( l_sts == YADCX$_BADVER )
		sys$exit(yadcx$putmsg(l_sts, 0, 2, l_hdr.version, YADCX$K_VERSION) | STS$M_INHIB_MSG);
	if ( l_sts == YADCX$_BADMETH )
		sys$exit(yadcx$putmsg(l_sts, 0, 1, l_hdr.method) | STS$M_INHIB_MSG);
	if ( l_sts == YADCX$_NOHEADER || l_sts == YADCX$_BADHDR )
		sys$exit(yadcx$putmsg(l_sts, 0, 1, &l_kit.rsadsc) | STS$M_INHIB_MSG);
	if ( !(1 & l_sts) )
		sys$exit(yadcx$putmsg(YADCX$_READERR, l_sts, 1, &l_kit.rsadsc) | STS$M_INHIB_MSG);

	yadcx$show(&l_hdr);

	if ( l_list )
		sys$exit(YADCX$_KITINFO | STS$M_INHIB_MSG);

	/*
	**  The stored data: all blocks present, CRC-32 right - before anything is written
	*/
	l_sts = yadcx$check(&l_kit, l_vbn, &l_hdr, &l_crc);
	if ( l_sts == YADCX$_TRUNCATED )
		sys$exit(yadcx$putmsg(l_sts, 0, 2, l_vbn + l_hdr.sblocks, l_kit.blocks) | STS$M_INHIB_MSG);
	if ( l_sts == YADCX$_CRCERR )
		sys$exit(yadcx$putmsg(l_sts, 0, 2, l_hdr.scrc, l_crc) | STS$M_INHIB_MSG);
	if ( !(1 & l_sts) )
		sys$exit(yadcx$putmsg(YADCX$_READERR, l_sts, 1, &l_kit.rsadsc) | STS$M_INHIB_MSG);

	/*
	**  /VERIFY: a compressed payload is expanded in memory and its own
	**  CRC-32 checked, so VERIFIED means the payload would come back intact
	*/
	if ( l_verify )
		{
		if ( l_hdr.method != YADCX$K_METHOD_NONE )
			{
			if ( !(1 & (l_sts = yadcx$expand(&l_kit, l_vbn, &l_hdr, 0, &l_crc))) )
				sys$exit(l_sts | STS$M_INHIB_MSG);
			if ( l_crc != l_hdr.crc )
				sys$exit(yadcx$putmsg(YADCX$_EXPCRC, 0, 2, l_hdr.crc, l_crc) | STS$M_INHIB_MSG);
			}
		sys$exit(yadcx$putmsg(YADCX$_VERIFIED, 0, 1, l_crc) | STS$M_INHIB_MSG);
		}

	l_sts = s_restore(&l_kit, l_vbn, &l_hdr, &l_out);
	yadcx$close(&l_kit, 0);

	sys$exit(l_sts | STS$M_INHIB_MSG);
	return	SS$_NORMAL;
}
