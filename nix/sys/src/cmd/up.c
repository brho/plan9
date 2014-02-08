/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/*
 * File update tool.
 * Report actions needed to make dst be like src
 */

#include <u.h>
#include <libc.h>
#include <bio.h>

enum
{
	Rc = 0,
	Chg,
	Sts,
};

static int dogids, douids;
static char *fcparg = "-x";
static int omode = Rc;
static char *droot;
static Biobuf bout;
static int usemtime;

static int
realerror(void)
{
	char buf[128];

	rerrstr(buf, sizeof buf);
	return strstr(buf, "does not exist") == 0;
}

static int
samemeta(Dir *sd, Dir *dd)
{
	if(dd->mtime > sd->mtime)
		return -1;
	return sd->length == sd->length && dd->mtime == sd->mtime;
}

static int
samecontent(char *f1, char *f2)
{
	Waitmsg *m;

	if(fork() == 0){
		execl("/bin/cmp", "cmp", "-s", f1, f2, nil);
		sysfatal("exec /bin/cmp: %r");
	}
	m = wait();
	if(m->msg[0] == 0){
		free(m);
		return 1;
	}else{
		free(m);
		return 0;
	}
}

static Dir*
findname(char *name, Dir *d, int nd)
{
	int i;

	for(i = 0; i < nd; i++)
		if(d[i].name != nil && strcmp(d[i].name, name) == 0)
			return &d[i];
	return nil;
}

static int update(Dir*, char*, Dir*, char*);

static int
updatedir(char *src, char *dst)
{
	int sfd, dfd, nsd, ndd, r, i;
	Dir *sd, *dd, *csd, *cdd;
	char *csrc, *cdst;

	sfd = open(src, OREAD);
	if(sfd < 0){
		fprint(2, "%s: %s: %r\n", argv0, src);
		return -1;
	}
	dfd = open(dst, OREAD);
	if(dfd < 0){
		close(sfd);
		fprint(2, "%s: %s: %r\n", argv0, dst);
		return -1;
	}
	sd = nil;
	dd = nil;
	nsd = dirreadall(sfd, &sd);
	ndd = dirreadall(dfd, &dd);
	close(sfd);
	close(dfd);
	if(nsd < 0){
		fprint(2, "%s: %s: %r\n", argv0, src);
		goto fail;
	}
	if(ndd < 0){
		fprint(2, "%s: %s: %r\n", argv0, dst);
		goto fail;
	}

	for(i = 0; i < nsd; i++){
		csd = &sd[i];
		cdd = findname(csd->name, dd, ndd);
		csrc = smprint("%s/%s", src, csd->name);
		cdst = smprint("%s/%s", dst, csd->name);
		r = update(csd, csrc, cdd, cdst);
		free(csrc);
		free(cdst);
		if(r < 0)
			goto fail;
		if(cdd != nil)
			cdd->name = nil;
	}

	for(i = 0; i < ndd; i++)
		if(dd[i].name != nil){
			csrc = smprint("%s/%s", src, dd[i].name);
			cdst = smprint("%s/%s", dst, dd[i].name);
			r = update(nil, csrc, &dd[i], cdst);
			free(csrc);
			free(cdst);
			if(r < 0)
				goto fail;
		}

	free(sd);
	free(dd);
	return 0;
fail:
	free(sd);
	free(dd);
	return -1;
}

static int
samemode(Dir *d1, Dir *d2)
{
	long m1, m2;

	m1 = d1->mode & (0777|DMAPPEND|DMEXCL|DMTMP);
	m2 = d2->mode & (0777|DMAPPEND|DMEXCL|DMTMP);
	return m1 == m2;
}

static void
updatemode(char *f, long mode)
{
	if(omode == Sts)
		exits(f);
	Bprint(&bout, "chmod 0%lo %s\n", mode&(0777|DMAPPEND|DMEXCL|DMTMP), f);
}

static void
updategid(char *f, char *gid)
{
	if(omode == Sts)
		exits(f);
	Bprint(&bout, "chgrp %s %s\n", gid, f);
}

static void
updateuid(char *f, char *uid)
{
	if(omode == Sts)
		exits(f);
	Bprint(&bout, "chgrp -o %s %s\n", uid, f);
}

static char*
drel(char *n)
{
	static int l;

	if(l == 0)
		l = strlen(droot);
	if(n[l] == 0)
		return "/";
	return n + l;
}

static void
updatecp(char *src, char *dst, int isdir)
{
	if(omode == Sts)
		exits(dst);
	if(omode == Chg){
		Bprint(&bout, "c %s\n", drel(dst));
		return;
	}
	if(isdir)
		Bprint(&bout, "mkdir -p %s && dircp %s %s\n", dst, src, dst);
	else
		Bprint(&bout, "fcp -x %s %s\n", src, dst);
}

static void
updatenew(char *src, char *dst, int isdir)
{
	if(omode == Sts)
		exits(dst);
	if(omode == Chg){
		Bprint(&bout, "a %s\n", drel(dst));
		return;
	}
	if(isdir)
		Bprint(&bout, "mkdir -p %s && dircp %s %s\n", dst, src, dst);
	else
		Bprint(&bout, "fcp %s %s %s\n", fcparg, src, dst);
}

static void
updatedel(char *f)
{
	if(omode == Sts)
		exits(f);
	if(omode == Chg){
		Bprint(&bout, "d %s\n", drel(f));
		return;
	}
	Bprint(&bout, "rm -rf %s\n", f);
}

static void
updatestat(char *f, long, char *, char *)
{
	if(omode == Sts)
		exits(f);
	Bprint(&bout, "m %s\n", drel(f));
}

static int
update(Dir *sd, char *src, Dir *dd, char *dst)
{
	if(sd == nil && dd == nil)
		return 0;

	if(droot == nil)
		droot = dst;

	if(sd == nil && dd != nil){
		updatedel(dst);
		return 0;
	}

	if(sd != nil && dd == nil){
		updatenew(src, dst, sd->mode&DMDIR);
		return 0;
	}

	if((sd->mode&DMDIR) != (dd->mode&DMDIR)){
		updatedel(dst);
		updatenew(src, dst, sd->mode&DMDIR);
		return 0;
	}

	if((sd->mode&DMDIR) == 0){
		if(usemtime){
			switch(samemeta(sd, dd)){
			case 0:
				updatecp(src, dst, 0);
				return 0;
			case -1:
				Bflush(&bout);
				fprint(2, "%s is newer: ignored\n", dst);
				return 0;
			}
		}else
			if(samecontent(src, dst) == 0){
				updatecp(src, dst, 0);
				return 0;
			}
		if(omode == Chg &&
			(samemode(sd, dd) == 0 ||
			(dogids && strcmp(sd->gid, dd->gid) != 0) ||
			(douids && strcmp(sd->uid, dd->uid) != 0)) ){
			updatestat(dst, sd->mode, sd->uid, sd->gid);
			return 0;
		}
		if(samemode(sd, dd) == 0)
			updatemode(dst, sd->mode);
		if(dogids && strcmp(sd->gid, dd->gid) != 0)
			updategid(dst, sd->gid);
		if(douids && strcmp(sd->uid, dd->uid) != 0)
			updateuid(dst, sd->uid);
		return 0;
	}

	return updatedir(src, dst);
}

static void
usage(void)
{
	fprint(2, "usage: %s [-cutgs] src dst\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Dir *sd, *dd;

	ARGBEGIN{
	case 't':
		usemtime = 1;
		break;
	case 'c':
		omode = Chg;
		break;
	case 'u':
		dogids = 1;
		douids = 1;
		fcparg = "-gux";
		break;
	case 'g':
		dogids = 1;
		fcparg = "-gx";
		break;
	case 's':
		omode = Sts;
		break;
	default:
		usage();
	}ARGEND
	if(argc != 2)
		usage();
	outofmemoryexits(1);
	if(Binit(&bout, 1, OWRITE) < 0)
		sysfatal("Binit: %r");
	sd = dirstat(argv[0]);
	if(sd == nil && realerror())
		sysfatal("%s: %r", argv[0]);
	dd = dirstat(argv[1]);
	if(dd == nil && realerror())
		sysfatal("%s: %r", argv[1]);
	if(update(sd, argv[0], dd, argv[1]) < 0){
		Bterm(&bout);
		exits("errors");
	}
	Bterm(&bout);
	exits(nil);
}
