#define	__NEW_STARLET	1

#include	<string.h>
#include	<rms.h>
#include	<efndef.h>
#include	<stsdef.h>
#include	<ssdef.h>
#include	<starlet.h>
#include	<str$routines.h>
#include	<lib$routines.h>

#include	"YADCX.H"

#define	__MODULE__	"YADCX"
#define	__IDENT__	"X-02-00ECO01"
#define	__REV__		"02.00.01"

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
**
**	26-SEP-2026	CLD	X-01-01: all strings by descriptor, no .ASCIZ/strlen;
**				/NAME checked with STR$FIND_FIRST_IN_SET.
**
**	26-SEP-2026	CLD	X-01-01ECO01: product version for the IOSBDEF change
**				in YADCX_COMMON.C; no change here.
**
**	26-SEP-2026	CLD	X-02-00: /COMPRESSION=DCX (default) or NONE, kit
**				format 2, exact allocation by a sizing pass, NONE
**				when DCX gives no gain; /LIST expands and checks the
**				original CRC.
**
**	28-SEP-2026	CLD	X-02-00ECO01: product version for KITNAME in
**				YADCX_COMMON.C; no change here.
**--
*/

static char	s_buf[YADCX$K_IOBLKS * YADCX$K_BLKSZ];	/* Copy buffer					*/

static	$DESCRIPTOR(s_q_verb, "YADCX");			/* Verb in YADCX_CLI.CLD			*/
static	$DESCRIPTOR(s_q_file, "FILE");
static	$DESCRIPTOR(s_q_list, "LIST");
static	$DESCRIPTOR(s_q_output, "OUTPUT");
static	$DESCRIPTOR(s_q_stub, "STUB");
static	$DESCRIPTOR(s_q_name, "NAME");
static	$DESCRIPTOR(s_q_cnone, "COMPRESSION.NONE");
static	$DESCRIPTOR(s_sfxname, "YADCX_SFX.EXE");		/* The stub, default name			*/
static	$DESCRIPTOR(s_exe, ".EXE");
static	$DESCRIPTOR(s_badchars, ":[]<>;");		/* Not allowed in /NAME				*/
static	$DESCRIPTOR(s_jpi, "$GETJPIW JPI$_IMAGNAME");
static	$DESCRIPTOR(s_n_passes, "the two compression passes (sizes differ)");


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
	if ( a_sts == YADCX$_BADMETH )
		return	yadcx$putmsg(a_sts, 0, 1, a_hdr->method);
	if ( a_sts == YADCX$_NOHEADER || a_sts == YADCX$_BADHDR )
		return	yadcx$putmsg(a_sts, 0, 1, &a_file->rsadsc);

	return	yadcx$putmsg(YADCX$_READERR, a_sts, 1, &a_file->rsadsc);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	YADCX/LIST: show what a kit carries and check it: the CRC-32 of the
**	stored data, then, for a compressed kit, the payload is expanded in
**	memory and its own CRC-32 checked - VERIFIED means the same here and
**	in the stub's /VERIFY: the original payload would come back intact.
**
**  INPUTS:
**
**	a_name	-	kit file specification, descriptor
**
**  RETURNS:
**
**	YADCX$_VERIFIED or an error message code (already shown)
**--
*/
static unsigned int	s_list	(
		struct dsc$descriptor_s	*a_name
			)
{
unsigned int	l_sts;
uint32_t	l_vbn, l_crc;
YADCX$FILE	l_kit;
YADCX$HDR	l_hdr;

	if ( !(1 & (l_sts = yadcx$open(&l_kit, a_name, &s_exe))) )
		return	yadcx$putmsg_rms(YADCX$_OPENIN, &l_kit, a_name);

	if ( !(1 & (l_sts = yadcx$find_hdr(&l_kit, &l_vbn, &l_hdr))) )
		{
		yadcx$close(&l_kit, 0);
		return	s_hdr_err(l_sts, &l_kit, &l_hdr);
		}

	yadcx$show(&l_hdr);

	l_sts = yadcx$check(&l_kit, l_vbn, &l_hdr, &l_crc);

	if ( l_sts == YADCX$_TRUNCATED )
		l_sts = yadcx$putmsg(l_sts, 0, 2, l_vbn + l_hdr.sblocks, l_kit.blocks);
	else if ( l_sts == YADCX$_CRCERR )
		l_sts = yadcx$putmsg(l_sts, 0, 2, l_hdr.scrc, l_crc);
	else if ( !(1 & l_sts) )
		l_sts = yadcx$putmsg(YADCX$_READERR, l_sts, 1, &l_kit.rsadsc);
	else if ( l_hdr.method != YADCX$K_METHOD_NONE && (1 & (l_sts = yadcx$expand(&l_kit, l_vbn, &l_hdr, 0, &l_crc))) && l_crc != l_hdr.crc )
		l_sts = yadcx$putmsg(YADCX$_EXPCRC, 0, 2, l_hdr.crc, l_crc);

	yadcx$close(&l_kit, 0);

	if ( !(1 & l_sts) )
		return	l_sts;

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
**	a_name	-	the value, descriptor
**
**  RETURNS:
**
**	SS$_NORMAL or YADCX$_BADNAME
**--
*/
static unsigned int	s_check_name	(
		struct dsc$descriptor_s	*a_name
			)
{
	if ( !a_name->dsc$w_length || a_name->dsc$w_length > YADCX$K_NAMESZ )
		return	YADCX$_BADNAME;

	if ( str$find_first_in_set(a_name, &s_badchars) )	/* Position of a device/directory/version char	*/
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
**	a_payload -	payload file specification, descriptor
**	a_output -	/OUTPUT value, descriptor, empty - <payload name>.EXE here
**	a_stub	-	/STUB value, descriptor, empty - YADCX_SFX.EXE from the
**			builder image's directory
**	a_name	-	/NAME value, descriptor, empty - the payload's name.type
**	a_method -	YADCX$K_METHOD_DCX or _NONE (/COMPRESSION); DCX falls
**			back to NONE when it does not make the payload smaller
**
**  RETURNS:
**
**	YADCX$_BUILT or an error message code (already shown)
**--
*/
static unsigned int	s_build	(
		struct dsc$descriptor_s	*a_payload,
		struct dsc$descriptor_s	*a_output,
		struct dsc$descriptor_s	*a_stub,
		struct dsc$descriptor_s	*a_name,
		int		a_method
			)
{
unsigned int	l_sts;
int		l_len, l_rd, l_nogain = 0;
uint32_t	l_vbn, l_crc, l_s, l_p, l_d, l_mapsz = 0, l_scrc = 0, l_raws = 0, l_int, l_frac;
uint64_t	l_sbytes = 0, l_sbytes2 = 0;
void		*l_map = 0;
uint8_t		l_pfat[YADCX$K_FATSZ], l_sfat[YADCX$K_FATSZ];
char		l_selfbuf[NAM$C_MAXRSS], l_dnabuf[NAM$C_MAXRSS], l_txtbuf[32], *l_semi;
struct dsc$descriptor_s	l_self, l_dna, l_txt;
YADCX$FILE	l_pay, l_stub, l_kit;
YADCX$HDR	l_hdr, l_shdr;

	/*
	**  The payload: its size and its record attributes
	*/
	if ( !(1 & (l_sts = yadcx$open(&l_pay, a_payload, 0))) )
		return	yadcx$putmsg_rms(YADCX$_OPENIN, &l_pay, a_payload);

	if ( !l_pay.bytes )
		return	yadcx$putmsg(YADCX$_EMPTY, 0, 1, &l_pay.rsadsc);

	if ( !(1 & (l_sts = yadcx$get_fat(&l_pay, l_pfat))) )
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, &l_pay.rsadsc);

	l_p = l_pay.blocks;

	/*
	**  The name to store: /NAME or the payload's own name.type
	*/
	memset(&l_hdr, 0, sizeof(l_hdr));

	if ( a_name->dsc$w_length )
		{
		if ( !(1 & (l_sts = s_check_name(a_name))) )
			return	yadcx$putmsg(l_sts, 0, 0);
		l_len = a_name->dsc$w_length;
		memcpy(l_hdr.name, a_name->dsc$a_pointer, l_len);
		}
	else	{
		l_len = l_pay.nam.nam$b_name + l_pay.nam.nam$b_type;
		memcpy(l_hdr.name, l_pay.nam.nam$l_name, l_len);
		}
	l_hdr.namelen = (uint8_t) l_len;

	/*
	**  The stub: must be the bare one, without a kit header of its own
	*/
	if ( a_stub->dsc$w_length )
		l_sts = yadcx$open(&l_stub, a_stub, &s_sfxname);
	else	{
		$DSCINIT(l_self, l_selfbuf, sizeof(l_selfbuf));
		if ( !(1 & (l_sts = yadcx$self(&l_self))) )
			return	yadcx$putmsg(YADCX$_SYSERR, l_sts, 1, &s_jpi);
		if ( (l_semi = memchr(l_selfbuf, ';', l_self.dsc$w_length)) )	/* Cut the version off		*/
			l_self.dsc$w_length = (uint16_t) (l_semi - l_selfbuf);
		l_sts = yadcx$open(&l_stub, &s_sfxname, &l_self);
		}

	if ( !(1 & l_sts) )
		return	yadcx$putmsg_rms(YADCX$_OPENIN, &l_stub, a_stub->dsc$w_length ? a_stub : &s_sfxname);

	l_sts = yadcx$find_hdr(&l_stub, &l_vbn, &l_shdr);
	if ( 1 & l_sts || l_sts == YADCX$_BADHDR || l_sts == YADCX$_BADVER || l_sts == YADCX$_BADMETH )
		return	yadcx$putmsg(YADCX$_NESTED, 0, 1, &l_stub.rsadsc);
	if ( l_sts != YADCX$_NOHEADER )
		return	yadcx$putmsg(YADCX$_READERR, l_sts, 1, &l_stub.rsadsc);

	if ( !(1 & (l_sts = yadcx$get_fat(&l_stub, l_sfat))) )
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, &l_stub.rsadsc);

	l_s = l_stub.blocks;

	/*
	**  DCX: analysis, map, then a sizing pass, so the kit is allocated
	**  exactly; if the stored data would not be smaller than the payload,
	**  the payload is stored as is.
	*/
	l_d = l_p;
	if ( a_method == YADCX$K_METHOD_DCX )
		{
		if ( !(1 & (l_sts = yadcx$dcx_map(&l_pay, &l_map, &l_mapsz, &l_crc))) )
			return	l_sts;
		if ( !(1 & (l_sts = yadcx$compress(&l_pay, &l_map, l_mapsz, 0, 0, &l_sbytes, &l_scrc, &l_raws))) )
			return	l_sts;

		if ( l_sbytes >= l_pay.bytes )
			{
			lib$free_vm((int *) &l_mapsz, &l_map);
			a_method = YADCX$K_METHOD_NONE;
			l_nogain = 1;
			}
		else	l_d = (uint32_t) ((l_sbytes + YADCX$K_BLKSZ - 1) / YADCX$K_BLKSZ);
		}

	/*
	**  The kit: /OUTPUT or <payload name>.EXE in the current directory
	*/
	memcpy(l_dnabuf, l_pay.nam.nam$l_name, l_pay.nam.nam$b_name);
	memcpy(l_dnabuf + l_pay.nam.nam$b_name, s_exe.dsc$a_pointer, s_exe.dsc$w_length);
	$DSCINIT(l_dna, l_dnabuf, l_pay.nam.nam$b_name + s_exe.dsc$w_length);

	if ( !(1 & (l_sts = yadcx$create(&l_kit, a_output, &l_dna, l_s + 1 + l_d))) )
		return	yadcx$putmsg_rms(YADCX$_OPENOUT, &l_kit, a_output->dsc$w_length ? a_output : &l_dna);

	if ( !(1 & (l_sts = s_copy(&l_stub, &l_kit, 1, 1, l_s, 0, 0, &l_rd))) )
		{
		yadcx$abort(&l_kit);
		return	yadcx$putmsg(l_rd ? YADCX$_READERR : YADCX$_WRITEERR, l_sts, 1, l_rd ? &l_stub.rsadsc : &l_kit.rsadsc);
		}

	if ( a_method == YADCX$K_METHOD_DCX )
		{
		l_sts = yadcx$compress(&l_pay, &l_map, l_mapsz, &l_kit, l_s + 2, &l_sbytes2, &l_scrc, &l_raws);
		lib$free_vm((int *) &l_mapsz, &l_map);
		if ( !(1 & l_sts) )
			{
			yadcx$abort(&l_kit);
			return	l_sts;
			}
		if ( l_sbytes2 != l_sbytes )			/* The two passes must agree		*/
			{
			yadcx$abort(&l_kit);
			return	yadcx$putmsg(YADCX$_DCXERR, SS$_BUGCHECK, 1, &s_n_passes);
			}
		}
	else	{
		l_crc = 0xFFFFFFFF;
		if ( !(1 & (l_sts = s_copy(&l_pay, &l_kit, 1, l_s + 2, l_p, l_pay.bytes, &l_crc, &l_rd))) )
			{
			yadcx$abort(&l_kit);
			return	yadcx$putmsg(l_rd ? YADCX$_READERR : YADCX$_WRITEERR, l_sts, 1, l_rd ? &l_pay.rsadsc : &l_kit.rsadsc);
			}
		l_crc = ~l_crc;
		l_sbytes = l_pay.bytes;
		l_scrc = l_crc;
		}

	/*
	**  The header, its own CRC-32 goes last
	*/
	yadcx$marker(l_hdr.marker);
	l_hdr.version = YADCX$K_VERSION;
	l_hdr.hdrsz = sizeof(YADCX$HDR);
	l_hdr.crc = l_crc;
	l_hdr.bytes = l_pay.bytes;
	l_hdr.blocks = l_p;
	l_hdr.method = (uint16_t) a_method;
	l_hdr.reclen = (a_method == YADCX$K_METHOD_DCX) ? YADCX$K_RECLEN : 0;
	l_hdr.mapbytes = (a_method == YADCX$K_METHOD_DCX) ? l_mapsz : 0;
	l_hdr.sbytes = l_sbytes;
	l_hdr.sblocks = l_d;
	l_hdr.scrc = l_scrc;
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
		return	yadcx$putmsg(YADCX$_WRITEERR, l_sts, 1, &l_kit.rsadsc);
		}

	/*
	**  Close the kit with the stub's attributes (Fixed 512), the end of
	**  file right after the last block of the stored data.  FFBYTE is a word, it is
	**  not stored high word first like EFBLK.
	*/
	yadcx$fat_put(l_sfat, YADCX$K_FAT_EFBLK, l_s + 1 + l_d + 1);
	l_sfat[YADCX$K_FAT_FFBYTE] = l_sfat[YADCX$K_FAT_FFBYTE + 1] = 0;

	if ( !(1 & (l_sts = yadcx$close(&l_kit, l_sfat))) )
		{
		yadcx$abort(&l_kit);
		return	yadcx$putmsg(YADCX$_ATTRERR, l_sts, 1, &l_kit.rsadsc);
		}

	yadcx$close(&l_stub, 0);
	yadcx$close(&l_pay, 0);

	$DSCINIT(l_txt, l_txtbuf, sizeof(l_txtbuf));
	yadcx$fmt_uq(l_pay.bytes, &l_txt);
	yadcx$putmsg(YADCX$_PAYLOAD, 0, 4, &l_pay.rsadsc, &l_txt, l_p, l_hdr.crc);

	if ( a_method == YADCX$K_METHOD_DCX )
		{
		$DSCINIT(l_txt, l_txtbuf, sizeof(l_txtbuf));
		yadcx$fmt_uq(l_sbytes, &l_txt);
		yadcx$percent(l_sbytes, l_pay.bytes, &l_int, &l_frac);
		yadcx$putmsg(YADCX$_COMPRESSED, 0, 6, &l_txt, l_d, l_int, l_frac, l_mapsz, l_raws);
		}
	else if ( l_nogain )
		yadcx$putmsg(YADCX$_NOGAIN, 0, 1, &l_pay.rsadsc);

	return	yadcx$putmsg(YADCX$_BUILT, 0, 3, &l_kit.rsadsc, l_s, l_d);
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
int		l_given;
char		l_filebuf[NAM$C_MAXRSS], l_outbuf[NAM$C_MAXRSS], l_stubbuf[NAM$C_MAXRSS], l_namebuf[YADCX$K_NAMESZ + 1];
struct dsc$descriptor_s	l_file, l_output, l_stub, l_name;

	/*
	**  Buffer descriptors: the length is the size for CLI$GET_VALUE, then the
	**  length of the value; an absent qualifier leaves an empty string.  The
	**  /NAME buffer is one byte longer than allowed, so a too long value is
	**  seen and refused, not cut.
	*/
	$DSCINIT(l_file, l_filebuf, sizeof(l_filebuf));
	$DSCINIT(l_output, l_outbuf, 0);
	$DSCINIT(l_stub, l_stubbuf, 0);
	$DSCINIT(l_name, l_namebuf, 0);

	if ( !(1 & (l_sts = yadcx$cli_parse(&s_q_verb, 1, &l_given))) )
		sys$exit(l_sts | STS$M_INHIB_MSG);

	yadcx$cli_value(&s_q_file, &l_file);

	yadcx$crc_init();

	if ( yadcx$cli_present(&s_q_list) )
		sys$exit(s_list(&l_file) | STS$M_INHIB_MSG);

	if ( yadcx$cli_present(&s_q_output) )
		{
		l_output.dsc$w_length = sizeof(l_outbuf);
		yadcx$cli_value(&s_q_output, &l_output);
		}
	if ( yadcx$cli_present(&s_q_stub) )
		{
		l_stub.dsc$w_length = sizeof(l_stubbuf);
		yadcx$cli_value(&s_q_stub, &l_stub);
		}
	if ( yadcx$cli_present(&s_q_name) )
		{
		l_name.dsc$w_length = sizeof(l_namebuf);
		yadcx$cli_value(&s_q_name, &l_name);
		}

	sys$exit(s_build(&l_file, &l_output, &l_stub, &l_name, yadcx$cli_present(&s_q_cnone) ? YADCX$K_METHOD_NONE : YADCX$K_METHOD_DCX) | STS$M_INHIB_MSG);
	return	SS$_NORMAL;
}
