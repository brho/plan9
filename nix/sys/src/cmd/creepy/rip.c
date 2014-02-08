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
 * Requiescat in pace to all old files.
 * They deserve it.
 *
 * This is a WORM archive for creepy files.
 * It is actually a creepy 9pix that operates in worm mode:
 * - only "main" and "archive" are valid attach specs.
 * - only the owner can attach to "main"
 * - dbrefs are not used (all blocks are kept forever).
 * - there is no automatic sync proc.
 *
 * This is meant to be used by an archiving program that determines
 * which files changed from day to day and writes them to the archive
 * using the active tree of the worm, and calling sync() after that.
 *
 * XXX: modify sync so that in worm mode only ddmm[.n] dirs are kept
 * if the sync is due to a console request. (other syncs are ok to flush
 * changes to disk).
 *
 * XXX: The plan is that the archive can use an index file
 * /active/idx/n0/.../n15 to map sha1 -> address, such that
 * the archival program computes the index, looks if the file is already
 * kept here, and uses a new "link new old" ctl in that case, and
 * copies the file (and updates the index) otherwise.
 *
 * This permits the archival process to operate with multiple concurrent
 * processes archiving files in parallel (and computing hashes in parallel).
 * It's likely that's going to outperform fossil+venti.
 *
 * XXX: The owner must be always in allow mode for the active file tree.
 *
 * XXX: Change worm mode so that file->id value is the disk address.
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

	addr = "tcp!*!dump";
	srv = "dump";
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
	fsopen(dev, Worm, Wr);
	if(srv != nil)
		srv9pix(srv, cliworker9p);
	if(addr != nil)
		listen9pix(addr, cliworker9p);
	consinit();
	proccreate(timeproc, nil, Stack);
	noerror();
	threadexits(nil);
}

