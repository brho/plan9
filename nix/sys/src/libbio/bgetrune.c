/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include	<u.h>
#include	<libc.h>
#include	<bio.h>

long
Bgetrune(Biobufhdr *bp)
{
	int c, i;
	Rune rune;
	char str[UTFmax + 1];

	c = Bgetc(bp);
	if(c < Runeself) {		/* one char */
		bp->runesize = 1;
		return c;
	}
	str[0] = c;

	for(i=1;;) {
		c = Bgetc(bp);
		if(c < 0)
			return c;
		str[i++] = c;

		if(fullrune(str, i)) {
			bp->runesize = chartorune(&rune, str);
			while(i > bp->runesize) {
				Bungetc(bp);
				i--;
			}
			return rune;
		}
	}
}

int
Bungetrune(Biobufhdr *bp)
{

	if(bp->state == Bracteof)
		bp->state = Bractive;
	if(bp->state != Bractive)
		return Beof;
	bp->icount -= bp->runesize;
	bp->runesize = 0;
	return 1;
}
