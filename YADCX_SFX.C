#define	__NEW_STARLET	1

#include	<string.h>
#include	<rms.h>
#include	<efndef.h>
#include	<stsdef.h>
#include	<ssdef.h>
#include	<starlet.h>

#include	"YADCX.H"

#define	__MODULE__	"YADCX_SFX"
#define	__IDENT__	"X-01-00ECO01"
#define	__REV__		"01.00.01"

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
**--
*/

static char	s_buf[YADCX$K_IOBLKS * YADCX$K_BLKSZ];	/* Payload copy buffer				*/


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write the payload out of the kit to a new file and put its original
**	record attributes on it.  The CRC has been checked already.
**
**  INPUTS:
**
**	a_kit	-	the kit, open
**	a_vbn	-	VBN of the header
**	a_hdr	-	the header
**	a_out	-	/OUTPUT value, .ASCIZ, may be empty - then the name
**			stored in the kit goes to the current default directory
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
		char		*a_out
			)
{
unsigned int	l_sts;
uint32_t	l_n;
uint64_t	l_bytes;
char		l_defname[YADCX$K_NAMESZ + 1], l_txt[32];
YADCX$FILE	l_out;

	memcpy(l_defname, a_hdr->name, a_hdr->namelen);
	l_defname[a_hdr->namelen] = '\0';

	if ( *a_out )
		l_sts = yadcx$create(&l_out, a_out, (int) strlen(a_out), l_defname, a_hdr->blocks);
	else	l_sts = yadcx$create(&l_out, l_defname, a_hdr->namelen, 0, a_hdr->blocks);

	if ( !(1 & l_sts) )
		return	yadcx$putmsg_rms(YADCX$_OPENOUT, &l_out, *a_out ? a_out : l_defname);

	for (uint32_t l_done = 0; l_done < a_hdr->blocks; l_done += l_n)
		{
		l_n = a_hdr->blocks - l_done;
		if ( l_n > YADCX$K_IOBLKS )
			l_n = YADCX$K_IOBLKS;

		if ( !(1 & (l_sts = yadcx$read(a_kit, a_vbn + 1 + l_done, s_buf, l_n))) )
			{
			yadcx$abort(&l_out);
			return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, a_kit->rsa);
			}

		if ( !(1 & (l_sts = yadcx$write(&l_out, 1 + l_done, s_buf, l_n))) )
			{
			yadcx$abort(&l_out);
			return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, l_out.rsa);
			}
		}

	if ( !(1 & (l_sts = yadcx$close(&l_out, a_hdr->fat))) )
		{
		yadcx$abort(&l_out);
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, l_out.rsa);
		}

	memcpy(&l_bytes, &a_hdr->bytes, sizeof(l_bytes));		/* The header is packed				*/
	yadcx$fmt_uq(l_bytes, l_txt, sizeof(l_txt));

	return	yadcx$putmsg(YADCX$_RESTORED, 0, 3, l_out.rsa, l_txt, a_hdr->crc);
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
int		l_given, l_list = 0, l_verify = 0, l_selflen, l_outlen = 0;
uint32_t	l_vbn, l_crc;
char		l_self[NAM$C_MAXRSS + 1], l_out[NAM$C_MAXRSS + 1];
YADCX$FILE	l_kit;
YADCX$HDR	l_hdr;

	l_out[0] = '\0';

	if ( !(1 & (l_sts = yadcx$cli_parse("SFX", 0, &l_given))) )
		sys$exit(l_sts | STS$M_INHIB_MSG);

	if ( l_given )
		{
		l_list = yadcx$cli_present("LIST");
		l_verify = yadcx$cli_present("VERIFY");
		if ( yadcx$cli_present("OUTPUT") )
			yadcx$cli_value("OUTPUT", l_out, sizeof(l_out), &l_outlen);
		}

	yadcx$crc_init();

	if ( !(1 & (l_sts = yadcx$self(l_self, sizeof(l_self), &l_selflen))) )
		sys$exit(yadcx$putmsg(YADCX$_SYSERR, l_sts, 1, "$GETJPIW JPI$_IMAGNAME") | STS$M_INHIB_MSG);

	if ( !(1 & (l_sts = yadcx$open(&l_kit, l_self, l_selflen, 0))) )
		sys$exit(yadcx$putmsg_rms(YADCX$_OPENIN, &l_kit, l_self) | STS$M_INHIB_MSG);

	/*
	**  The header: marker, own CRC, version
	*/
	l_sts = yadcx$find_hdr(&l_kit, &l_vbn, &l_hdr);
	if ( l_sts == YADCX$_BADVER )
		sys$exit(yadcx$putmsg(l_sts, 0, 2, l_hdr.version, YADCX$K_VERSION) | STS$M_INHIB_MSG);
	if ( l_sts == YADCX$_NOHEADER || l_sts == YADCX$_BADHDR )
		sys$exit(yadcx$putmsg(l_sts, 0, 1, l_kit.rsa) | STS$M_INHIB_MSG);
	if ( !(1 & l_sts) )
		sys$exit(yadcx$putmsg(YADCX$_READERR, l_sts, 1, l_kit.rsa) | STS$M_INHIB_MSG);

	yadcx$show(&l_hdr);

	if ( l_list )
		sys$exit(YADCX$_KITINFO | STS$M_INHIB_MSG);

	/*
	**  The payload: all blocks present, CRC-32 right - before anything is written
	*/
	l_sts = yadcx$check(&l_kit, l_vbn, &l_hdr, &l_crc);
	if ( l_sts == YADCX$_TRUNCATED )
		sys$exit(yadcx$putmsg(l_sts, 0, 2, l_vbn + l_hdr.blocks, l_kit.blocks) | STS$M_INHIB_MSG);
	if ( l_sts == YADCX$_CRCERR )
		sys$exit(yadcx$putmsg(l_sts, 0, 2, l_hdr.crc, l_crc) | STS$M_INHIB_MSG);
	if ( !(1 & l_sts) )
		sys$exit(yadcx$putmsg(YADCX$_READERR, l_sts, 1, l_kit.rsa) | STS$M_INHIB_MSG);

	if ( l_verify )
		sys$exit(yadcx$putmsg(YADCX$_VERIFIED, 0, 1, l_crc) | STS$M_INHIB_MSG);

	l_sts = s_restore(&l_kit, l_vbn, &l_hdr, l_out);
	yadcx$close(&l_kit, 0);

	sys$exit(l_sts | STS$M_INHIB_MSG);
	return	SS$_NORMAL;
}
