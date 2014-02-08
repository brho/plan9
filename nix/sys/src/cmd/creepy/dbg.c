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
#include <thread.h>

char dbg[256];
static char sdbg[256];
static Ref nodbg;

void
nodebug(void)
{
	incref(&nodbg);
	if(nodbg.ref == 1)
		memmove(sdbg, dbg, sizeof dbg);
	memset(dbg, 0, sizeof dbg);
}

void
debug(void)
{
	if(decref(&nodbg) == 0)
		memmove(dbg, sdbg, sizeof dbg);
}

int
setdebug(void)
{
	int r;

	r = nodbg.ref;
	if(r > 0)
		memmove(dbg, sdbg, sizeof dbg);
	return r;
}

void
rlsedebug(int r)
{
	nodbg.ref = r;
	if(r > 0)
		memset(dbg, 0, sizeof dbg);
}

