/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#include <stdlib.h>
#include <errno.h>
#include <euc.h>
#include "japanese.h"
#include "jfp_iconv_unicode.h"

#define	JFP_J2U_ICONV_X0213
#include "jfp_jis_to_ucs2.h"

void *
_icv_open(void)
{
	return (_icv_open_unicode((size_t)0));
}

void
_icv_close(void *cd)
{
	_icv_close_unicode(cd);
	return;
}

size_t
_icv_iconv(void *cd, char **inbuf, size_t *inbytesleft,
				char **outbuf, size_t *outbytesleft)
{
	unsigned int	u32;		/* UTF-32 */
	unsigned short	e16;		/* 16-bit EUC */
	unsigned char	ic1, ic2, ic3;	/* 1st, 2nd, and 3rd bytes of a char */
	size_t		rv = (size_t)0;	/* return value of this function */

	unsigned char	*ip;
        size_t		ileft;
	char		*op;
        size_t		oleft;

	/*
	 * If inbuf and/or *inbuf are NULL, reset conversion descriptor
	 * and put escape sequence if needed.
	 */
	if ((inbuf == NULL) || (*inbuf == NULL)) {
		_icv_reset_unicode(cd);
		return ((size_t)0);
	}

	ip = (unsigned char *)*inbuf;
	ileft = *inbytesleft;
	op = *outbuf;
	oleft = *outbytesleft;

	while (ileft != 0) {
		NGET(ic1, "never fail here"); /* get 1st byte */

		if (ISASC(ic1)) { /* CS0; 1 byte */
			u32 = (unsigned int)_jfp_tbl_jisx0201roman_to_ucs2[ic1];
			PUTU(u32, "CS0");
		} else if (ISCS1(ic1)) { /* JIS X 0213 plane 1; 2 bytes */
			NGET(ic2, "CS1-2");
			if (ISCS1(ic2)) { /* 2nd byte check passed */
				e16 = (ic1 << 8) | ic2;
				u32 = (unsigned int)_jfp_tbl_jisx0208_to_ucs2[
					(ic1 - 0xa1) * 94 + (ic2 - 0xa1)];
				if (IFHISUR(u32)) {
					u32 = _jfp_lookup_x0213_nonbmp(
						e16, u32);
					PUTU(u32, "CS1->NONBMP");
				} else if (u32 == 0xffff) {
					/* need to compose */
					unsigned int	u32_2;
					u32 = _jfp_lookup_x0213_compose(
						e16, &u32_2);
					PUTU(u32, "CS1->CP1");
					PUTU(u32_2, "CS1->CP2");
				} else {
					PUTU(u32, "CS1->BMP");
				}
			} else { /* 2nd byte check failed */
				RETERROR(EILSEQ, "CS1-2")
			}
		} else if (ic1 == SS2) { /* JIS X 0201 Kana; 2 bytes */
			NGET(ic2, "CS2-2");
			if (ISCS2(ic2)) { /* 2nd byte check passed */
				u32 = (unsigned int)
				_jfp_tbl_jisx0201kana_to_ucs2[ic2 - 0xa1];
				PUTU(u32, "CS2->Kana");
			} else { /* 2nd byte check failed */
				RETERROR(EILSEQ, "CS2-2")
			}
		} else if (ic1 == SS3) { /* JIS X 0213 plane 2; 3 bytes */
			NGET(ic2, "CS3-2");
			if (ISCS3(ic2)) { /* 2nd byte check passed */
				NGET(ic3, "CS3-3");
				if (ISCS3(ic3)) { /* 3rd byte check passed */
					e16 = (ic2 << 8) | (ic3 & 0x7f);
					u32 = (unsigned int)
					_jfp_tbl_jisx0213p2_to_ucs2[
					(ic2 - 0xa1) * 94 + (ic3 - 0xa1)];
					if (IFHISUR(u32)) {
						u32 = _jfp_lookup_x0213_nonbmp(
						e16, u32);
						PUTU(u32, "CS3->NONBMP");
					} else {
						PUTU(u32, "CS3->BMP");
					}
				} else { /* 3rd byte check failed */
					RETERROR(EILSEQ, "CS3-3")
				}
			} else { /* 2nd byte check failed */
				RETERROR(EILSEQ, "CS3-2")
			}
		} else if (ISC1CTRLEUC(ic1)) { /* C1 control; 1 byte */
			u32 = ic1;
			PUTU(u32, "E2BIG C1CTRL");
		} else { /* 1st byte check failed */
			RETERROR(EILSEQ, "CS?-1")
		}

		/*
		 * One character successfully converted so update
		 * values outside of this function's stack.
		 */
		*inbuf = (char *)ip;
		*inbytesleft = ileft;
		*outbuf = op;
		*outbytesleft = oleft;
	}

ret:
	DEBUGPRINTERROR

	/*
	 * Return value for successful return is not defined by XPG
	 * so return same as *inbytesleft as existing codes do.
	 */
	return ((rv == (size_t)-1) ? rv : *inbytesleft);
}
