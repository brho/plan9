/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include "all.h"
/*
 * Creepy file server 9P and IX service.
 */

static void
usage(void)
{
	fprint(2, "usage: %s [-DFLAGS] [-a] [-A addr] [-S srv] disk\n", argv0);
	exits("usage");
}

int mainstacksize = Stack;

void
threadmain(int argc, char *argv[])
{
	char *addr, *dev, *srv;

	addr = "tcp!*!9fs";
	srv = "9pix";
	ARGBEGIN{
	case 'A':
		addr = EARGF(usage());
		break;
	case 'S':
		srv = EARGF(usage());
		break;
	case 'a':
		noauth = 1;
		break;
	default:
		if(ARGC() >= 'A' && ARGC() <= 'Z' || ARGC() == '9'){
			dbg[ARGC()] = 1;
			fatalaborts = 1;
		}else
			usage();
	}ARGEND;
	if(argc != 1)
		usage();
	dev = argv[0];
	if(dbg['d'])
		dbg['Z'] = 1;

	outofmemoryexits(1);
	workerthreadcreate = proccreate;
	fmtinstall('H', mbfmt);
	fmtinstall('M', dirmodefmt);
	fmtinstall('F', fcallfmt);
	fmtinstall('G', ixcallfmt);
	fmtinstall('X', fidfmt);
	fmtinstall('R', rpcfmt);
	fmtinstall('A', usrfmt);
	fmtinstall('P', pathfmt);

	errinit(Errstack);
	if(catcherror())
		fatal("uncatched error: %r");
	rfork(RFNAMEG|RFNOTEG);
	rwusers(nil);
	fsopen(dev, Normal, Wr);
	if(srv != nil)
		srv9pix(srv, cliworker9p);
	if(addr != nil)
		listen9pix(addr, cliworker9p);

	consinit();
	proccreate(timeproc, nil, Stack);
	proccreate(fssyncproc, nil, Stack);
	noerror();
	threadexits(nil);
}

