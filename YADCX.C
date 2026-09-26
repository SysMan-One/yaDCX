#define	__NEW_STARLET	1

#include	<string.h>
#include	<rms.h>
#include	<efndef.h>
#include	<stsdef.h>
#include	<ssdef.h>
#include	<starlet.h>

#include	"YADCX.H"

#define	__MODULE__	"YADCX"
#define	__IDENT__	"X-01-00ECO02"
#define	__REV__		"01.00.02"

/*
**++
**  FACILITY:	yaDCX -- Yet Another Data Compression/eXpansion kit builder
**
**  MODULE DESCRIPTION:
**
**	The kit builder, head module of the project.  It glues together the
**	stub image YADCX_SFX.EXE, a kit header and a payload file (normally a
**	BACKUP saveset) into a single self-extracting image:
**
**		$ YADCX :== $dev:[dir]YADCX.EXE
**		$ YADCX DX_V29.BCK			build DX_V29.EXE here
**		$ YADCX DX_V29.BCK /OUTPUT=[.KITS]	build [.KITS]DX_V29.EXE
**		$ YADCX DX_V29.BCK /NAME=DX.BCK		store another name in the kit
**		$ YADCX DX_V29.BCK /STUB=[OTHER]	take [OTHER]YADCX_SFX.EXE
**		$ YADCX /LIST DX_V29.EXE		show and check a kit
**
**	Without /STUB the stub is YADCX_SFX.EXE from the directory of the
**	builder image itself.  The payload is copied block for block with its
**	CRC-32 and its own RMS record attributes (FAT) recorded in the header.
**
**  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  26-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	26-SEP-2026	CLD	X-01-00: initial version, replaces MKCAT.C of
**				[LAISHEV.YADX_KIT].
**
**	26-SEP-2026	CLD	X-01-00ECO01: OPENIN/OPENOUT show the RMS STV
**				(yadcx$putmsg_rms).
**
**	26-SEP-2026	CLD	X-01-00ECO02: kit release with CONTRIBUTING.md,
**				AGENTS.md, tools/mk_aaareadme.py; no code change.
**--
*/

static char	s_buf[YADCX$K_IOBLKS * YADCX$K_BLKSZ];	/* Copy buffer					*/


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Show the message for a status of yadcx$find_hdr() on a kit.
**
**  INPUTS:
**
**	a_sts	-	status of yadcx$find_hdr()
**	a_file	-	the file scanned
**	a_hdr	-	the header found (for BADVER)
**
**  RETURNS:
**
**	the message code shown
**--
*/
static unsigned int	s_hdr_err	(
		unsigned int	a_sts,
		YADCX$FILE	*a_file,
		YADCX$HDR	*a_hdr
			)
{
	if ( a_sts == YADCX$_BADVER )
		return	yadcx$putmsg(a_sts, 0, 2, a_hdr->version, YADCX$K_VERSION);
	if ( a_sts == YADCX$_NOHEADER || a_sts == YADCX$_BADHDR )
		return	yadcx$putmsg(a_sts, 0, 1, a_file->rsa);

	return	yadcx$putmsg(YADCX$_READERR, a_sts, 1, a_file->rsa);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	YADCX/LIST: show what a kit carries and check its payload CRC-32.
**
**  INPUTS:
**
**	a_name	-	kit file specification, .ASCIZ
**
**  RETURNS:
**
**	YADCX$_VERIFIED or an error message code (already shown)
**--
*/
static unsigned int	s_list	(
		char		*a_name
			)
{
unsigned int	l_sts;
uint32_t	l_vbn, l_crc;
YADCX$FILE	l_kit;
YADCX$HDR	l_hdr;

	if ( !(1 & (l_sts = yadcx$open(&l_kit, a_name, (int) strlen(a_name), ".EXE"))) )
		return	yadcx$putmsg_rms(YADCX$_OPENIN, &l_kit, a_name);

	if ( !(1 & (l_sts = yadcx$find_hdr(&l_kit, &l_vbn, &l_hdr))) )
		{
		yadcx$close(&l_kit, 0);
		return	s_hdr_err(l_sts, &l_kit, &l_hdr);
		}

	yadcx$show(&l_hdr);

	l_sts = yadcx$check(&l_kit, l_vbn, &l_hdr, &l_crc);
	yadcx$close(&l_kit, 0);

	if ( l_sts == YADCX$_TRUNCATED )
		return	yadcx$putmsg(l_sts, 0, 2, l_vbn + l_hdr.blocks, l_kit.blocks);
	if ( l_sts == YADCX$_CRCERR )
		return	yadcx$putmsg(l_sts, 0, 2, l_hdr.crc, l_crc);
	if ( !(1 & l_sts) )
		return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, l_kit.rsa);

	return	yadcx$putmsg(YADCX$_VERIFIED, 0, 1, l_crc);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Check a /NAME value: a plain "name.type", nothing that would take the
**	restored file out of the directory it is restored to.
**
**  INPUTS:
**
**	a_name	-	the value
**	a_len	-	its length
**
**  RETURNS:
**
**	SS$_NORMAL or YADCX$_BADNAME
**--
*/
static unsigned int	s_check_name	(
		char		*a_name,
		int		a_len
			)
{
	if ( !a_len || a_len > YADCX$K_NAMESZ )
		return	YADCX$_BADNAME;

	for (int i = 0; i < a_len; i++)
		if ( strchr(":[]<>;", a_name[i]) )
			return	YADCX$_BADNAME;

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Copy blocks from one file to another through s_buf, optionally
**	running the first <a_bytes> bytes through the CRC.
**
**  INPUTS:
**
**	a_in	-	source file
**	a_out	-	destination file
**	a_invbn	-	first source VBN
**	a_outvbn -	first destination VBN
**	a_blocks -	number of blocks
**	a_bytes	-	number of bytes to run through the CRC
**
**  INPUTS/OUTPUTS:
**
**	a_crc	-	running CRC, NULL - no CRC
**
**  OUTPUTS:
**
**	a_rd	-	1 - a read failed, 0 - a write failed (on error)
**
**  RETURNS:
**
**	condition value of the I/O
**--
*/
static unsigned int	s_copy	(
		YADCX$FILE	*a_in,
		YADCX$FILE	*a_out,
		uint32_t	a_invbn,
		uint32_t	a_outvbn,
		uint32_t	a_blocks,
		uint64_t	a_bytes,
		uint32_t	*a_crc,
		int		*a_rd
			)
{
unsigned int	l_sts;
uint32_t	l_n, l_len;

	for (uint32_t l_done = 0; l_done < a_blocks; l_done += l_n)
		{
		l_n = a_blocks - l_done;
		if ( l_n > YADCX$K_IOBLKS )
			l_n = YADCX$K_IOBLKS;

		*a_rd = 1;
		if ( !(1 & (l_sts = yadcx$read(a_in, a_invbn + l_done, s_buf, l_n))) )
			return	l_sts;

		if ( a_crc )
			{
			l_len = (a_bytes < l_n * YADCX$K_BLKSZ) ? (uint32_t) a_bytes : l_n * YADCX$K_BLKSZ;
			yadcx$crc(a_crc, s_buf, l_len);
			a_bytes -= l_len;
			}

		*a_rd = 0;
		if ( !(1 & (l_sts = yadcx$write(a_out, a_outvbn + l_done, s_buf, l_n))) )
			return	l_sts;
		}

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Build a kit: stub blocks, header block, payload blocks.
**
**  INPUTS:
**
**	a_payload -	payload file specification, .ASCIZ
**	a_output -	/OUTPUT value, .ASCIZ, empty - <payload name>.EXE here
**	a_stub	-	/STUB value, .ASCIZ, empty - YADCX_SFX.EXE from the
**			builder image's directory
**	a_name	-	/NAME value, .ASCIZ, empty - the payload's name.type
**
**  RETURNS:
**
**	YADCX$_BUILT or an error message code (already shown)
**--
*/
static unsigned int	s_build	(
		char		*a_payload,
		char		*a_output,
		char		*a_stub,
		char		*a_name
			)
{
unsigned int	l_sts;
int		l_len, l_rd;
uint32_t	l_vbn, l_crc, l_s, l_p;
uint8_t		l_pfat[YADCX$K_FATSZ], l_sfat[YADCX$K_FATSZ];
char		l_self[NAM$C_MAXRSS + 1], l_dna[NAM$C_MAXRSS + 1], l_txt[32], *l_semi;
YADCX$FILE	l_pay, l_stub, l_kit;
YADCX$HDR	l_hdr, l_shdr;

	/*
	**  The payload: its size and its record attributes
	*/
	if ( !(1 & (l_sts = yadcx$open(&l_pay, a_payload, (int) strlen(a_payload), 0))) )
		return	yadcx$putmsg_rms(YADCX$_OPENIN, &l_pay, a_payload);

	if ( !l_pay.bytes )
		return	yadcx$putmsg(YADCX$_EMPTY, 0, 1, l_pay.rsa);

	if ( !(1 & (l_sts = yadcx$get_fat(&l_pay, l_pfat))) )
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, l_pay.rsa);

	l_p = l_pay.blocks;

	/*
	**  The name to store: /NAME or the payload's own name.type
	*/
	memset(&l_hdr, 0, sizeof(l_hdr));

	if ( *a_name )
		{
		l_len = (int) strlen(a_name);
		if ( !(1 & (l_sts = s_check_name(a_name, l_len))) )
			return	yadcx$putmsg(l_sts, 0, 0);
		memcpy(l_hdr.name, a_name, l_len);
		}
	else	{
		l_len = l_pay.nam.nam$b_name + l_pay.nam.nam$b_type;
		memcpy(l_hdr.name, l_pay.nam.nam$l_name, l_len);
		}
	l_hdr.namelen = (uint8_t) l_len;

	/*
	**  The stub: must be the bare one, without a kit header of its own
	*/
	if ( *a_stub )
		l_sts = yadcx$open(&l_stub, a_stub, (int) strlen(a_stub), "YADCX_SFX.EXE");
	else	{
		if ( !(1 & (l_sts = yadcx$self(l_self, sizeof(l_self), &l_len))) )
			return	yadcx$putmsg(YADCX$_SYSERR, l_sts, 1, "$GETJPIW JPI$_IMAGNAME");
		if ( (l_semi = strchr(l_self, ';')) )
			*l_semi = '\0';
		l_sts = yadcx$open(&l_stub, "YADCX_SFX.EXE", 13, l_self);
		}

	if ( !(1 & l_sts) )
		return	yadcx$putmsg_rms(YADCX$_OPENIN, &l_stub, *a_stub ? a_stub : "YADCX_SFX.EXE");

	l_sts = yadcx$find_hdr(&l_stub, &l_vbn, &l_shdr);
	if ( 1 & l_sts || l_sts == YADCX$_BADHDR || l_sts == YADCX$_BADVER )
		return	yadcx$putmsg(YADCX$_NESTED, 0, 1, l_stub.rsa);
	if ( l_sts != YADCX$_NOHEADER )
		return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, l_stub.rsa);

	if ( !(1 & (l_sts = yadcx$get_fat(&l_stub, l_sfat))) )
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, l_stub.rsa);

	l_s = l_stub.blocks;

	/*
	**  The kit: /OUTPUT or <payload name>.EXE in the current directory
	*/
	memcpy(l_dna, l_pay.nam.nam$l_name, l_pay.nam.nam$b_name);
	strcpy(l_dna + l_pay.nam.nam$b_name, ".EXE");

	if ( !(1 & (l_sts = yadcx$create(&l_kit, a_output, (int) strlen(a_output), l_dna, l_s + 1 + l_p))) )
		return	yadcx$putmsg_rms(YADCX$_OPENOUT, &l_kit, *a_output ? a_output : l_dna);

	if ( !(1 & (l_sts = s_copy(&l_stub, &l_kit, 1, 1, l_s, 0, 0, &l_rd))) )
		{
		yadcx$abort(&l_kit);
		return	yadcx$putmsg(l_rd ? YADCX$_READERR : YADCX$_WRITEERR, l_sts, 1, l_rd ? l_stub.rsa : l_kit.rsa);
		}

	l_crc = 0xFFFFFFFF;
	if ( !(1 & (l_sts = s_copy(&l_pay, &l_kit, 1, l_s + 2, l_p, l_pay.bytes, &l_crc, &l_rd))) )
		{
		yadcx$abort(&l_kit);
		return	yadcx$putmsg(l_rd ? YADCX$_READERR : YADCX$_WRITEERR, l_sts, 1, l_rd ? l_pay.rsa : l_kit.rsa);
		}

	/*
	**  The header, its own CRC-32 goes last
	*/
	yadcx$marker(l_hdr.marker);
	l_hdr.version = YADCX$K_VERSION;
	l_hdr.hdrsz = sizeof(YADCX$HDR);
	l_hdr.crc = ~l_crc;
	l_hdr.bytes = l_pay.bytes;
	l_hdr.blocks = l_p;
	memcpy(l_hdr.fat, l_pfat, YADCX$K_FATSZ);
	sys$gettim((void *) &l_hdr.built);
	l_hdr.ident[0] = (char) (sizeof(__IDENT__) - 1);
	memcpy(l_hdr.ident + 1, __IDENT__, sizeof(__IDENT__) - 1);

	l_crc = 0xFFFFFFFF;
	yadcx$crc(&l_crc, &l_hdr, (uint32_t) ((char *) &l_hdr.hdrcrc - (char *) &l_hdr));
	l_hdr.hdrcrc = ~l_crc;

	if ( !(1 & (l_sts = yadcx$write(&l_kit, l_s + 1, &l_hdr, 1))) )
		{
		yadcx$abort(&l_kit);
		return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, l_kit.rsa);
		}

	/*
	**  Close the kit with the stub's attributes (Fixed 512), the end of
	**  file right after the last payload block.  FFBYTE is a word, it is
	**  not stored high word first like EFBLK.
	*/
	yadcx$fat_put(l_sfat, YADCX$K_FAT_EFBLK, l_s + 1 + l_p + 1);
	l_sfat[YADCX$K_FAT_FFBYTE] = l_sfat[YADCX$K_FAT_FFBYTE + 1] = 0;

	if ( !(1 & (l_sts = yadcx$close(&l_kit, l_sfat))) )
		{
		yadcx$abort(&l_kit);
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, l_kit.rsa);
		}

	yadcx$close(&l_stub, 0);
	yadcx$close(&l_pay, 0);

	yadcx$fmt_uq(l_pay.bytes, l_txt, sizeof(l_txt));
	yadcx$putmsg(YADCX$_PAYLOAD, 0, 4, l_pay.rsa, l_txt, l_p, l_hdr.crc);

	return	yadcx$putmsg(YADCX$_BUILT, 0, 3, l_kit.rsa, l_s, l_p);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Main: parse the command line, then build or list a kit.
**
**  IMPLICIT INPUTS:
**
**	the foreign command line
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
int		l_given, l_len;
char		l_file[NAM$C_MAXRSS + 1], l_output[NAM$C_MAXRSS + 1], l_stub[NAM$C_MAXRSS + 1], l_name[YADCX$K_NAMESZ + 2];

	l_output[0] = l_stub[0] = l_name[0] = '\0';

	if ( !(1 & (l_sts = yadcx$cli_parse("YADCX", 1, &l_given))) )
		sys$exit(l_sts | STS$M_INHIB_MSG);

	yadcx$cli_value("FILE", l_file, sizeof(l_file), &l_len);

	yadcx$crc_init();

	if ( yadcx$cli_present("LIST") )
		sys$exit(s_list(l_file) | STS$M_INHIB_MSG);

	if ( yadcx$cli_present("OUTPUT") )
		yadcx$cli_value("OUTPUT", l_output, sizeof(l_output), &l_len);
	if ( yadcx$cli_present("STUB") )
		yadcx$cli_value("STUB", l_stub, sizeof(l_stub), &l_len);
	if ( yadcx$cli_present("NAME") )
		yadcx$cli_value("NAME", l_name, sizeof(l_name), &l_len);

	sys$exit(s_build(l_file, l_output, l_stub, l_name) | STS$M_INHIB_MSG);
	return	SS$_NORMAL;
}
