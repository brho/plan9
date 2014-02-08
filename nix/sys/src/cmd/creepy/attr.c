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
 * Attribute handling
 *
 * BUG: we only support the predefined attributes.
 *
 */

typedef struct Adef Adef;

struct Adef
{
	char*	name;
	int	sz;
	long	(*wattr)(Memblk*, char*);
	long	(*rattr)(Memblk*, char*, long);
};

long wname(Memblk*, char*);
static long ratime(Memblk*, char*, long);
static long rgid(Memblk*, char*, long);
static long rid(Memblk*, char*, long);
static long rlength(Memblk*, char*, long);
static long rmode(Memblk*, char*, long);
static long rmtime(Memblk*, char*, long);
static long rmuid(Memblk*, char*, long);
static long rname(Memblk*, char*, long);
static long rstar(Memblk*, char*, long);
static long ruid(Memblk*, char*, long);
static long watime(Memblk*, char*);
static long wgid(Memblk*, char*);
static long wid(Memblk*, char*);
static long wlength(Memblk*, char*);
static long wmode(Memblk*, char*);
static long wmtime(Memblk*, char*);
static long wmuid(Memblk*, char*);
static long wuid(Memblk*, char*);

static Adef adef[] =
{
	{"name", 0, wname, rname},
	{"id",	BIT64SZ, nil, rid},
	{"atime", BIT64SZ, watime, ratime},
	{"mtime", BIT64SZ, wmtime, rmtime},
	{"length", BIT64SZ, wlength, rlength},
	{"uid", 0, wuid, ruid},
	{"gid", 0, wgid, rgid},
	{"muid", 0, wuid, ruid},
	{"mode", BIT64SZ, wmode, rmode},
	{"*", 0, nil, rstar},
};

/*
 * Return size for attributes embedded in file.
 * At least Dminattrsz bytes are reserved in the file block,
 * at most Embedsz.
 * Size is rounded to the size of an address.
 */
ulong
embedattrsz(Memblk *f)
{
	ulong sz;

	sz = f->d.asize;
	sz = ROUNDUP(sz, BIT64SZ);
	if(sz < Dminattrsz)
		sz = Dminattrsz;
	else
		sz = Embedsz;
	return sz;
}

void
gmeta(Memblk *f, void *buf, ulong nbuf)
{
	char *p;
	int i;

	f->mf->uid = usrname(f->d.uid);
	f->mf->gid = usrname(f->d.gid);
	f->mf->muid = usrname(f->d.muid);
	p = buf;
	for(i = 0; i < nbuf; i++)
		if(p[i] == 0)
			break;
	if(i == nbuf)
		error("corrupt meta");
	f->mf->name = buf;
	
}

static ulong
metasize(Memblk *f)
{
	return strlen(f->mf->name) + 1;
}

/*
 * Pack the metadata into buf.
 * pointers in meta are changed to refer to the packed data.
 * Return pointer past the packed metadata.
 * The caller is responsible for ensuring that metadata fits in buf.
 */
ulong
pmeta(void *buf, ulong nbuf, Memblk *f)
{
	char *p, *e;
	ulong sz;

	sz = metasize(f);
	if(sz > nbuf)
		error("attributes are too long");
	p = buf;
	if(f->mf->name != p)
		e = strecpy(p, p+nbuf, f->mf->name);
	else
		e = p + strlen(p);
	e++;
	assert(e-p <= sz);	/* can be <, to leave room for growing */
	f->mf->name = p;
	return sz;
}

long 
wname(Memblk *f, char *val)
{
	char *old;
	ulong maxsz;

	old = f->mf->name;
	f->mf->name = val;
	maxsz = embedattrsz(f);
	if(metasize(f) > maxsz){
		f->mf->name = old;
		warnerror("no room to grow metadata");
	}
	pmeta(f->d.embed, maxsz, f);
	return strlen(val)+1;
}

static long
ru64int(uvlong v, char *buf, long n)
{
	char s[30], *p;

	p = seprint(s, s+sizeof s, "%#018ullx", v);
	if((p-s)+1 > n)
		error("buffer too short");
	strecpy(buf, buf+n, s);
	return (p-s)+1;
}

static long
rstr(char *s, char *buf, long len)
{
	long l;

	l = strlen(s) + 1;
	if(l > len)
		error("buffer too short");
	strcpy(buf, s);
	return l;
}

static long 
rname(Memblk *f, char *buf, long len)
{
	return rstr(f->mf->name, buf, len);
}

static long 
ruid(Memblk *f, char *buf, long len)
{
	return rstr(f->mf->uid, buf, len);
}

static u64int
chkusr(char *buf)
{
	int id;

	id = usrid(buf);
	if(id < 0)
		error("unknown user '%s'", buf);
	return id;
}

static long 
wuid(Memblk *f, char *buf)
{
	f->d.uid = chkusr(buf);
	return strlen(buf)+1;
}

static long 
rgid(Memblk *f, char *buf, long len)
{
	return rstr(f->mf->gid, buf, len);
}

static long 
wgid(Memblk *f, char *buf)
{
	f->d.gid = chkusr(buf);
	return strlen(buf)+1;
}

static long 
rmuid(Memblk *f, char *buf, long len)
{
	return rstr(f->mf->muid, buf, len);
}

static long 
wmuid(Memblk *f, char *buf)
{
	f->d.muid = chkusr(buf);
	return strlen(buf)+1;
}

static uvlong
chku64int(char *buf)
{
	u64int v;
	char *r;

	v = strtoull(buf, &r, 0);
	if(r == nil || r == buf || *r != 0)
		error("not a number");
	return v;
}

static long 
wid(Memblk *f, char *buf)
{
	f->d.id = chku64int(buf);
	return strlen(buf)+1;
}

static long 
rid(Memblk *f, char *buf, long n)
{
	return ru64int(f->d.id, buf, n);
}

static long 
watime(Memblk *f, char *buf)
{
	f->d.atime = chku64int(buf);
	return strlen(buf)+1;
}

static long 
ratime(Memblk *f, char *buf, long n)
{
	return ru64int(f->d.atime, buf, n);
}

static long 
wmode(Memblk *f, char *buf)
{
	f->d.mode = chku64int(buf) | (f->d.mode&DMUSERS);
	return strlen(buf)+1;
}

static long 
rmode(Memblk *f, char *buf, long n)
{
	return ru64int(f->d.mode&~DMUSERS, buf, n);
}

static long 
wmtime(Memblk *f, char *buf)
{
	f->d.mtime = chku64int(buf);
	return strlen(buf)+1;
}

static long 
rmtime(Memblk *f, char *buf, long n)
{
	return ru64int(f->d.mtime, buf, n);
}

static uvlong
resized(Memblk *f, uvlong sz)
{
	ulong boff, bno, bend, doff;

	if(f->d.mode&DMDIR)
		error("can't resize a directory");

	if(sz > maxfsz)
		error("max file size exceeded");
	if(sz >= f->d.length)
		return sz;
	bno = dfbno(f, sz, &boff);
	if(boff > 0)
		bno++;
	bend = dfbno(f, sz, &boff);
	if(boff > 0)
		bend++;
	doff = embedattrsz(f);
	if(doff < Embedsz)
		memset(f->d.embed+doff, 0, Embedsz-doff);
	dfdropblks(f, bno, bend);
	return sz;
}

static long 
wlength(Memblk *f, char *buf)
{
	f->d.length = chku64int(buf);
	resized(f, f->d.length);
	return strlen(buf)+1;
}

static long 
rlength(Memblk *f, char *buf, long n)
{
	return ru64int(f->d.length, buf, n);
}

static long 
rstar(Memblk *, char *buf, long len)
{
	char *s, *e;
	int i;

	s = buf;
	e = s + len;
	for(i = 0; i < nelem(adef); i++)
		if(*adef[i].name != '*')
			s = seprint(s, e, "%s ", adef[i].name);
	if(s > buf)
		*--s = 0;
	return s - (char*)buf;
}

long
dfwattr(Memblk *f, char *name, char *val)
{
	int i;
	long tot;

	isfile(f);
	ismelted(f);
	isrwlocked(f, Wr);
	if(fsdiskfree() < Dzerofree)
		error("disk full");

	for(i = 0; i < nelem(adef); i++)
		if(strcmp(adef[i].name, name) == 0)
			break;
	if(i == nelem(adef))
		error("bug: user defined attributes not yet implemented");
	if(adef[i].wattr == nil)
		error("can't write %s", name);
	tot = adef[i].wattr(f, val);
	changed(f);
	return tot;
}

long
dfrattr(Memblk *f, char *name, char *val, long count)
{
	int i;
	long tot;

	isfile(f);
	isrwlocked(f, Rd);
	for(i = 0; i < nelem(adef); i++)
		if(strcmp(adef[i].name, name) == 0)
			break;
	if(i == nelem(adef))
		error("no such attribute");
	if(adef[i].sz != 0 && count < adef[i].sz)
		error("buffer too short for attribute");

	tot = adef[i].rattr(f, val, count);
	return tot;
}

static void
cstring(Memblk*, int op, char *v1, char *v2)
{
	int v;

	v = strcmp(v1, v2);
	switch(op){
	case CEQ:
		if(v != 0)
			error("false");
		break;
	case CGE:
		if(v < 0)
			error("false");
		break;
	case CGT:
		if(v <= 0)
			error("false");
		break;
	case CLE:
		if(v > 0)
			error("false");
		break;
	case CLT:
		if(v >= 0)
			error("false");
	case CNE:
		if(v == 0)
			error("false");
		break;
	}
}

/*
 * cond on attribute value
 */
void
dfcattr(Memblk *f, int op, char *name, char *val)
{
	int i;
	char buf[128];

	isfile(f);
	isrwlocked(f, Rd);

	dfrattr(f, name, buf, sizeof buf);

	for(i = 0; i < nelem(adef); i++)
		if(strcmp(adef[i].name, name) == 0)
			break;
	if(i == nelem(adef))
		error("no such attribute");
	cstring(f, op, buf, val);
}

/*
 * Does not check if the user can't write because of the "write"
 * user.
 * Does check if the user is allowed in config mode.
 */
void
dfaccessok(Memblk *f, int uid, int bits)
{
	uint mode;

	if(allowed(uid))
		return;

	bits &= 3;

	mode = f->d.mode &0777;

	if((mode&bits) == bits)
		return;
	mode >>= 3;
	
	if(member(f->d.gid, uid) && (mode&bits) == bits)
		return;
	mode >>= 3;
	if(f->d.uid == uid && (mode&bits) == bits)
		return;
	error("permission denied");
}
