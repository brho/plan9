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
#include <error.h>
#include <mp.h>
#include <libsec.h>
#include <thread.h>

/*
 * Archer: archive creepy file trees into a creepy worm.
 */

#define dprint(...)	if(!debug){}else fprint(2, __VA_ARGS__)

enum
{
	Errstack = 128,
	Stack = 128*1024,
};

typedef struct Path Path;
typedef struct Idx Idx;

typedef u64int daddrt;

struct Idx
{
	uchar	sha[SHA1dlen];
	daddrt	addr;
};

struct Path
{
	char *name;
	Path *up;
	int fd;
	Dir *d, *nd;
	Idx;
};

#pragma	varargck	type	"P"	Path*
#pragma	varargck	type	"H"	uchar*
#pragma	varargck	type	"I"	uchar*

static int debug;
int mainstacksize = Stack;
static int consfd;
static int dontprune;
static char *idxdir;

static int
pathfmt(Fmt *fmt)
{
	Path *p;
	
	p = va_arg(fmt->args, Path*);
	if(p == nil)
		return fmtprint(fmt, "none");
	if(p->up == nil)
		return fmtprint(fmt, "%s", p->name);
	return fmtprint(fmt, "%P/%s", p->up, p->name);
}

static int
shafmt(Fmt *fmt)
{
	uchar *p;
	int i;

	p = va_arg(fmt->args, uchar*);
	if(p == nil)
		return fmtprint(fmt, "[]");
	fmtprint(fmt, "[");
	for(i = 0; i < SHA1dlen; i++)
		fmtprint(fmt, "%02x", p[i]);
	return fmtprint(fmt, "]");
}

static int
idxfmt(Fmt *fmt)
{
	uchar *p;
	int i;

	p = va_arg(fmt->args, uchar*);
	for(i = 0; i < SHA1dlen - 1; i++)
		fmtprint(fmt, "%02x/", p[i]);
	return 0;
}

static Path*
newpath(char *dir)
{
	Path *p;

	p = malloc(sizeof *p);
	p->name = dir;
	p->up = nil;
	p->fd = open(p->name, OREAD);
	if(p->fd < 0){
		free(p);
		error("open: %r");
	}
	p->d = dirfstat(p->fd);
	p->nd = p->d;
	if(p->d == nil){
		free(p);
		close(p->fd);
		error("stat: %r");
	}
	return p;
}

static Path*
walkpath(Path *up, char *name, Dir *d)
{
	Path *p;

	p = mallocz(sizeof *p, 1);
	p->name = name;
	p->nd = nil;
	p->d = d;
	p->up = up;
	p->fd = -1;
	return p;
}

static void
pathsha(Path *p)
{
	uchar buf[8*1024];
	long nr;
	DigestState *s;

	s = sha1((uchar*)"creepy", 6, nil, nil);
	for(;;){
		nr = read(p->fd, buf, sizeof buf);
		if(nr <= 0){
			sha1((uchar*)"creepy", 6, p->sha, s);
			if(nr < 0)
				error("%P: read: %r", p);
			break;
		}
		sha1(buf, nr, nil, s);
	}
	seek(p->fd, 0, 0);
	dprint("sha: %P: %H\n", p, p->sha);
	
}

static void
openpath(Path *p, int perm, int mode)
{
	char *s;

	dprint("openpath %P %s\n", p, mode == OREAD?"rd":"wr");
	assert(mode == OREAD || mode == OWRITE);
	s = smprint("%P", p);
	if(p->d == nil){
		if(mode == OWRITE)
			p->fd = create(s, perm, mode);
	}else{
		if(mode == OWRITE)
			mode |= OTRUNC;
		p->fd = open(s, mode);
	}
	free(s);
	if(p->fd < 0)
		error("%P: %r", p);
	if(mode == OREAD && p->d != nil)
		pathsha(p);
}

static void
copy(Path *dp, Path *sp)
{
	char buf[8*1024];
	long nr;
	Dir *d;

	dprint("copy %P\n", sp);
	for(;;){
		nr = read(sp->fd, buf, sizeof buf);
		if(nr < 0)
			error("%P: read: %r", sp);
		if(nr == 0)
			break;
		if(write(dp->fd, buf, nr) != nr)
			error("%P: write: %r", dp);
	}
	d = dirfstat(dp->fd);
	if(d == nil)
		error("%P: stat: %r", dp);
	dp->addr = d->qid.path;
	free(d);
}

static void
closepath(Path *p)
{
	dprint("closepath %P\n", p);
	if(p->fd >= 0)
		close(p->fd);
	free(p->nd);
	free(p);
}

static void
wlink(Path *old, Path *new)
{
	char *s;
	int r;

	dprint("link %#ullx %P\n", old->addr, new);
	s = smprint("link %#ullx %P\n", old->addr, new);
	r = write(consfd, s, strlen(s));
	free(s);
	if(r < 0)
		error("link %P: %r", new);
}

static void
wunlink(Path *p)
{
	char *s;
	int r;

	dprint("unlink %P\n", p);
	s = smprint("unlink %P\n", p);
	r = write(consfd, s, strlen(s));
	free(s);
	if(r < 0)
		error("unlink %P\n", p);
}

static int
getidx(Path *p)
{
	char *ifn;
	int fd;
	Idx x;

	ifn = smprint("%s/%I", idxdir, p->sha);
	dprint("getidx %s...", ifn);
	fd = open(ifn, ORDWR);
	free(ifn);
	if(fd < 0){
		dprint("no\n");
		return -1;
	}
	seek(fd, p->sha[sizeof p->sha - 1]*sizeof x, 0);
	if(read(fd, &x, sizeof x) != sizeof x){
		close(fd);
		dprint("no\n");
		return -1;
	}
	close(fd);
	p->Idx = x;
	dprint(" d%#010ullx\n", p->addr);
	return 0;
}

static void
setidx(Path *p)
{
	char *ifn, *s, *e;
	int i, fd;
	ulong off;

	ifn = smprint("%s/%I", idxdir, p->sha);
	dprint("setidx %s d%#010ullx...\n", ifn, p->addr);
	if(catcherror()){
		free(ifn);
		error(nil);
	}
	s = ifn + strlen(idxdir);
	fd = -1;
	for(i = 0; i < SHA1dlen-1; i++){
		e = s + i*3;
		assert(*e == '/');
		*e = 0;
		if(i < SHA1dlen-2){
			if(access(ifn, AEXIST) < 0){
				dprint("\tcreate d: %s\n", ifn);
				fd = create(ifn, OREAD, 0770|DMDIR);
				if(fd < 0)
					error("%s: %r", ifn);
				close(fd);
			}
		}else{
			fd = open(ifn, OWRITE);
			if(fd < 0){
				dprint("\tcreate f: %s\n", ifn);
				fd = create(ifn, OWRITE, 0660);
			}
			if(fd < 0)
				error("%s: %r", ifn);
		}
		*e = '/';
	}
	off = sizeof(Idx) * (uint)p->sha[SHA1dlen-1];
	seek(fd, off, 0);
	if(write(fd, &p->Idx, sizeof p->Idx) != sizeof p->Idx){
		close(fd);
		error("%s: write: %r", ifn);
	}
	close(fd);
	free(ifn);
	noerror();
}

static int
archived(Path *p1, Path *p2)
{
	if(p1->d == nil || p2->d == nil)
		return 0;
	return p1->d->mtime == p2->d->mtime && p1->d->length == p2->d->length;
}

static int
metachanges(Path *p1, Path *p2)
{
	if((p1->d == nil && p2->d != nil) || (p1->d != nil && p2->d == nil))
		return 1;
	if(!archived(p1, p2))
		return 1;
	if(strcmp(p1->d->uid, p2->d->uid) != 0)
		return 1;
	if(strcmp(p1->d->gid, p2->d->gid) != 0)
		return 1;
	if(strcmp(p1->d->muid, p2->d->muid) != 0)
		return 1;
	return p1->d->mode != p2->d->mode;
}

static Dir*
match(Dir *d, Dir *ds, int nds)
{
	int i;

	for(i = 0; i < nds; i++)
		if(strcmp(d->name, ds[i].name) == 0)
			return &ds[i];
	return nil;
}

static void archer(Path*, Path*);

static void
archdir(Path *cp, Path *wp)
{
	Dir *cds, *wds, *d;
	int ncds, nwds, i;
	Path *ccp, *cwp;

	if(dontprune == 0 && archived(cp, wp)){
		dprint("archdir: prune %P -> %P\n", cp, wp); 
		return;
	}
	ncds = dirreadall(cp->fd, &cds);
	if(ncds < 0)
		error("read %P: %r", cp);
	if(catcherror()){
		free(cds);
		error(nil);
	}
	nwds = dirreadall(wp->fd, &wds);
	if(nwds < 0)
		error("read %P: %r", wp);
	if(catcherror()){
		free(wds);
		error(nil);
	}

	for(i = 0; i < ncds; i++){
		ccp = walkpath(cp, cds[i].name, &cds[i]);
		d = match(&cds[i], wds, nwds);
		cwp = walkpath(wp, cds[i].name, d);
		if(!catcherror()){
			archer(ccp, cwp);
			noerror();
		}else
			fprint(2, "%P: %r", cwp);
		closepath(ccp);
		closepath(cwp);
		if(d != nil)
			d->name = nil;	/* visited */
	}
	for(i = 0; i < nwds; i++)
		if(wds[i].name != nil){
			d = match(&wds[i], cds, ncds);
			ccp = walkpath(cp, wds[i].name, d);
			cwp = walkpath(wp, wds[i].name, &wds[i]);
			if(!catcherror()){
				archer(ccp, cwp);
				noerror();
			}else
				fprint(2, "%P: %r", cwp);
			closepath(ccp);
			closepath(cwp);
		}
	noerror();
	noerror();
	free(cds);
	free(wds);
}

static void
archfile(Path *cp, Path *wp)
{
	if(wp->d != nil && archived(cp, wp)){
		dprint("archfile: prune %P -> %P\n", cp, wp); 
		return;
	}

	openpath(cp, cp->d->mode, OREAD);
	if(getidx(cp) == 0){
		wlink(cp, wp);
		return;
	}else{
		openpath(wp, cp->d->mode, OWRITE);
		copy(wp, cp);
		setidx(wp);
	}
}

static void
archer(Path *cp, Path *wp)
{
	dprint("archer %P -> %P\n", cp, wp);
	assert(cp->d != nil || wp->d != nil);
	if(wp->d != nil){
		if(cp->d == nil){
			wunlink(wp);
			return;
		}
		if((cp->d->mode&DMDIR) ^ (wp->d->mode&DMDIR))
			wunlink(wp);
	}
	if(cp->d->mode&DMDIR)
		archdir(cp, wp);
	else
		archfile(cp, wp);
	if(metachanges(cp, wp) && dirfwstat(wp->fd, cp->d) < 0){
		dprint("stat %P\n", wp);
		fprint(2, "%P: wstat: %r\n", wp);
	}
}

static void
usage(void)
{
	fprint(2, "usage: %s [-d] [-t] cdir wdir\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *cdir, *wdir, *wact, *wcons;
	Path *cp, *wp;

	dontprune = 0;
	ARGBEGIN{
	case 'd':
		debug = 1;
		break;
	case 't':
		dontprune = 1;
		break;
	default:
		usage();
	}ARGEND;
	if(argc != 2)
		usage();
	cdir = argv[0];
	wdir = argv[1];

	outofmemoryexits(1);
	errinit(Errstack);
	if(catcherror())
		sysfatal("uncatched error: %r");

	fmtinstall('H', shafmt);
	fmtinstall('I', idxfmt);
	fmtinstall('P', pathfmt);
	wact = smprint("%s/active", wdir);
	idxdir = smprint("%s/idx", wact);
	wcons = smprint("%s/cons", wdir);
	consfd = open(wcons, OWRITE);
	if(consfd < 0)
		sysfatal("cons: %r");
	cp = newpath(cdir);
	wp = newpath(wact);
	if(catcherror())
		sysfatal("%P: %r", wp);
	archer(cp, wp);
	noerror();
	closepath(cp);
	closepath(wp);
	noerror();
	threadexits(nil);
}

