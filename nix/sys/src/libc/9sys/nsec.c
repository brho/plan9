/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include <u.h>
#include <libc.h>
#include <tos.h>

static uvlong order = 0x0001020304050607ULL;

static void
be2vlong(vlong *to, uchar *f)
{
	uchar *t, *o;
	int i;

	t = (uchar*)to;
	o = (uchar*)&order;
	for(i = 0; i < sizeof order; i++)
		t[o[i]] = f[i];
}

vlong
nsec(void)
{
	uchar b[8];
	vlong t;
	int f;

	f = open("/dev/bintime", OREAD);
	if(f < 0)
		return 0;
	t = 0;
	if(pread(f, b, sizeof b, 0) == sizeof b)
		be2vlong(&t, b);
	close(f);
	return t;
}
