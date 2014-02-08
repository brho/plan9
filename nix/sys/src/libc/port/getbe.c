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

static uvlong border = 0x0001020304050607ull;
static uvlong lorder = 0x0706050403020100ull;

uvlong
getle(uchar *t, int w)
{
	uint i;
	uvlong r;

	r = 0;
	for(i = w; i != 0; )
		r = r<<8 | t[--i];
	return r;
}

void
putle(uchar *t, uvlong r, int w)
{
	uchar *o, *f;
	uint i;

	f = (uchar*)&r;
	o = (uchar*)&lorder;
	for(i = 0; i < w; i++)
		t[o[i]] = f[i];
}

uvlong
getbe(uchar *t, int w)
{
	uint i;
	uvlong r;

	r = 0;
	for(i = 0; i < w; i++)
		r = r<<8 | t[i];
	return r;
}

void
putbe(uchar *t, uvlong r, int w)
{
	uchar *o, *f;
	uint i;

	f = (uchar*)&r;
	o = (uchar*)&border + (sizeof border-w);
	for(i = 0; i < w; i++)
		t[i] = f[o[i]];
}

