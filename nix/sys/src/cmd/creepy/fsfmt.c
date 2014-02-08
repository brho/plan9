/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include "all.h"

int
usrid(char*)
{
	return 3;
}

char*
usrname(int)
{
	return "sys";
}

int
member(int uid, int member)
{
	return uid == member;
}

int
allowed(int)
{
	return 1;
}

void
meltfids(void)
{
}

void
rwusers(Memblk*)
{
}

char*
ninestats(char *s, char*, int, int)
{
	return s;
}

char*
ixstats(char *s, char*, int, int)
{
	return s;
}

void
countfidrefs(void)
{
}

static void
usage(void)
{
	fprint(2, "usage: %s [-DFLAGS] [-vy] [disk]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *dev;
	int verb, force;

	dev = "disk";
	verb = force = 0;
	ARGBEGIN{
	case 'v':
		verb = 1;
		break;
	case 'y':
		force = 1;
		break;
	default:
		if((ARGC() >= 'A' && ARGC() <= 'Z') || ARGC() == '9'){
			dbg['d'] = 1;
			dbg[ARGC()] = 1;
			fatalaborts = 1;
		}else
			usage();
	}ARGEND;
	if(argc == 1)
		dev = argv[0];
	else if(argc > 0)
		usage();
	fmtinstall('P', pathfmt);
	fmtinstall('H', mbfmt);
	fmtinstall('M', dirmodefmt);
	errinit(Errstack);
	if(catcherror())
		fatal("error: %r");
	fsfmt(dev, force);
	if(verb)
		fsdump(0, Mem);
	else
		print("%lld %ldK blocks\n", fs->ndblk, Dblksz/1024);
	noerror();
	exits(nil);
}

