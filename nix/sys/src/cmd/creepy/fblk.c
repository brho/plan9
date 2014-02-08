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
 * File blocks.
 * see dk.h
 */

Path*
walkpath(Memblk *f, char *elems[], int nelems)
{
	int i;
	Memblk *nf;
	Path *p;

	p = newpath(f);
	if(catcherror()){
		putpath(p);
		error(nil);
	}
	isfile(f);
	for(i = 0; i < nelems; i++){
		if((f->d.mode&DMDIR) == 0)
			error("not a directory");
		rwlock(f, Rd);
		if(catcherror()){
			rwunlock(f, Rd);
			error("walk: %r");
		}
		nf = dfwalk(f, elems[i]);
		rwunlock(f, Rd);
		addelem(&p, nf);
		mbput(nf);
		f = nf;
		USED(&f);	/* in case of error() */
		noerror();
	}
	noerror();
	return p;
}

Path*
walkto(char *a, char **lastp)
{
	char *els[Npathels];
	int nels, n;

	n = strlen(a);
	nels = gettokens(a, els, nelem(els), "/");
	if(nels < 1)
		error("invalid path");
	if(lastp != nil){
		*lastp = a + n - strlen(els[nels-1]);
		return walkpath(fs->root, els, nels-1);
	}else
		return walkpath(fs->root, els, nels);
}

void
rwlock(Memblk *f, int iswr)
{
	xrwlock(f->mf, iswr);
}

void
rwunlock(Memblk *f, int iswr)
{
	xrwunlock(f->mf, iswr);
}

void
isfile(Memblk *f)
{
	if((f->d.mode&DMDIR) != 0)
		assert(DBDIR(f));
	else
		assert(!DBDIR(f));
	if(f->type != DBfile || f->mf == nil)
		fatal("isfile: not a file at pc %#p", getcallerpc(&f));
}

void
isrwlocked(Memblk *f, int iswr)
{
	if(f->type != DBfile || f->mf == nil)
		fatal("isrwlocked: not a file  at pc %#p", getcallerpc(&f));
	if((iswr && canrlock(f->mf)) || (!iswr && canwlock(f->mf)))
		fatal("is%clocked at pc %#p", iswr?'w':'r', getcallerpc(&f));
}

static void
isdir(Memblk *f)
{
	if((f->d.mode&DMDIR) != 0)
		assert(DBDIR(f));
	else
		assert(!DBDIR(f));
	if(f->type != DBfile || f->mf == nil)
		fatal("isdir: not a file at pc %#p", getcallerpc(&f));
	if((f->d.mode&DMDIR) == 0)
		fatal("isdir: not a dir at pc %#p", getcallerpc(&f));
}

/* for dfblk only */
static Memblk*
getmelted(uint type, uint dbit, daddrt *addrp, int *chg)
{
	Memblk *b, *nb;

	*chg = 0;
	if(*addrp == 0){
		b = dballocz(type, dbit, 1);
		*addrp = b->addr;
		*chg = 1;
		return b;
	}

	b = dbget(type, *addrp);
	assert(DBDIR(b) == dbit);
	nb = nil;
	if(!b->frozen)
		return b;
	if(catcherror()){
		mbput(b);
		mbput(nb);
		error(nil);
	}
	nb = dbdup(b);
	USED(&nb);		/* for error() */
	*addrp = nb->addr;
	*chg = 1;
	dbput(b);
	noerror();
	mbput(b);
	return nb;
}

/*
 * Get a file data block, perhaps allocating it on demand
 * if mkit. The file must be r/wlocked and melted if mkit.
 *
 * Adds disk refs for dir entries copied during melts and
 * considers that /archive is always melted.
 *
 * Read-ahead is not considered here. The file only records
 * the last accessed block number, to help the caller do RA.
 *
 */
static Memblk*
dfblk(Memblk *f, ulong bno, int mkit)
{
	ulong prev, nblks;
	int i, idx, nindir, type, chg;
	Memblk *b, *pb;
	daddrt *addrp;

	if(mkit)
		ismelted(f);

	if(bno != f->mf->lastbno){
		f->mf->sequential = (!mkit && bno == f->mf->lastbno + 1);
		f->mf->lastbno = bno;
	}

	/*
	 * bno: block # relative to the the block we are looking at.
	 * prev: # of blocks before the current one.
	 */
	prev = 0;
	chg = 0;

	/*
	 * Direct block?
	 */
	if(bno < nelem(f->d.dptr)){
		if(mkit)
			b = getmelted(DBdata, DBDIR(f), &f->d.dptr[bno], &chg);
		else
			b = dbget(DBdata, f->d.dptr[bno]);
		if(chg)
			changed(f);
		return b;
	}
	bno -= nelem(f->d.dptr);
	prev += nelem(f->d.dptr);

	/*
	 * Indirect block
	 * nblks: # of data blocks addressed by the block we look at.
	 */
	nblks = Dptrperblk;
	for(i = 0; i < nelem(f->d.iptr); i++){
		if(bno < nblks)
			break;
		bno -= nblks;
		prev += nblks;
		nblks *= Dptrperblk;
	}
	if(i == nelem(f->d.iptr))
		error("offset exceeds file capacity");
	ainc(&fs->nindirs[i]);
	type = DBptr0+i;
	dFprint("dfblk: indirect %s nblks %uld (ppb %ud) bno %uld\n",
		tname(type), nblks, Dptrperblk, bno);

	addrp = &f->d.iptr[i];
	if(mkit)
		b = getmelted(type, DBDIR(f), addrp, &chg);
	else
		b = dbget(type, *addrp);
	if(chg)
		changed(f);
	pb = b;
	if(catcherror()){
		mbput(pb);
		error(nil);
	}

	/* at the loop header:
	 * 	pb: parent of b
	 * 	b: DBptr block we are looking at.
	 * 	addrp: ptr to b within fb.
	 * 	nblks: # of data blocks addressed by b
	 */
	for(nindir = i+1; nindir >= 0; nindir--){
		chg = 0;
		dFprint("indir %s d%#ullx nblks %uld ptrperblk %d bno %uld\n\n",
			tname(DBdata+nindir), *addrp, nblks, Dptrperblk, bno);
		idx = 0;
		if(nindir > 0){
			nblks /= Dptrperblk;
			idx = bno/nblks;
		}
		if(*addrp == 0 && !mkit){
			/* hole */
			warn("HOLE");
			b = nil;
		}else{
			assert(type >= DBdata);
			if(mkit)
				b = getmelted(type, DBDIR(f), addrp, &chg);
			else
				b = dbget(type, *addrp);
			if(chg)
				changed(pb);
			addrp = &b->d.ptr[idx];
		}
		mbput(pb);
		pb = b;
		USED(&b);	/* force to memory in case of error */
		USED(&pb);	/* force to memory in case of error */
		bno -= idx * nblks;
		prev +=  idx * nblks;
		type--;
	}
	noerror();
	return b;
}

/*
 * Remove [bno:bend) file data blocks.
 * The file must be r/wlocked and melted.
 */
void
dfdropblks(Memblk *f, ulong bno, ulong bend)
{
	Memblk *b;

	isrwlocked(f, Wr);
	ismelted(f);
	assert(!DBDIR(f));

	dprint("dfdropblks: could remove d%#ullx[%uld:%uld]\n",
		f->addr, bno, bend);
	/*
	 * Instead of releasing the references on the data blocks,
	 * considering that the file might grow again, we keep them.
	 * Consider recompiling again and again and...
	 *
	 * The length has been adjusted and data won't be returned
	 * before overwritten.
	 *
	 * We only have to zero the data, because the file might
	 * grow using holes and the holes must read as zero, and also
	 * because directories assume all data blocks are initialized.
	 */
	for(; bno < bend; bno++){
		if(catcherror())
			continue;
		b = dfblk(f, bno, Dontmk);
		noerror();
		memset(b->d.data, 0, Dblkdatasz);
		changed(b);
		mbput(b);
	}
}

/*
 * block # for the given offset (first block in file is 0).
 * embedded data accounts also as block #0.
 * If boffp is not nil it returns the offset within that block
 * for the given offset.
 */
ulong
dfbno(Memblk *f, uvlong off, ulong *boffp)
{
	ulong doff, dlen;

	doff = embedattrsz(f);
	dlen = Embedsz - doff;
	if(off < dlen){
		*boffp = doff + off;
		return 0;
	}
	off -= dlen;
	if(boffp != nil)
		*boffp = off%Dblkdatasz;
	return off/Dblkdatasz;
}

/*
 * Return a block slice for data in f.
 * The slice returned is resized to keep in a single block.
 * If there's a hole in the file, Blksl.data == nil && Blksl.len > 0.
 *
 * If mkit, the data block (and any pointer block crossed)
 * is allocated/melted if needed, but the file length is NOT updated.
 *
 * The file must be r/wlocked by the caller, and melted if mkit.
 * The block is returned referenced but unlocked,
 * (it's still protected by the file lock.)
 */
Blksl
dfslice(Memblk *f, ulong len, uvlong off, int iswr)
{
	Blksl sl;
	ulong boff, doff, dlen, bno;

	memset(&sl, 0, sizeof sl);

	dFprint("slice m%#p[%#ullx:+%#ulx]%c...\n",f, off, len, iswr?'w':'r');
	if(iswr)
		ismelted(f);
	else
		if(off >= f->d.length)
			goto done;

	doff = embedattrsz(f);
	dlen = Embedsz - doff;

	if(off < dlen){
		sl.b = f;
		incref(f);
		sl.data = f->d.embed + doff + off;
		sl.len = dlen - off;
	}else{
		bno = (off-dlen) / Dblkdatasz;
		boff = (off-dlen) % Dblkdatasz;
		sl.b = dfblk(f, bno, iswr);
		if(iswr)
			ismelted(sl.b);
		if(sl.b != nil)
			sl.data = sl.b->d.data + boff;
		sl.len = Dblkdatasz - boff;
	}

	if(sl.len > len)
		sl.len = len;
	if(off + sl.len > f->d.length)
		if(!iswr)
			sl.len = f->d.length - off;
		/* else the file size will be updated by the caller */
done:
	if(sl.b == nil)
		dFprint("slice m%#p[%#ullx:+%#ulx]%c -> 0[%#ulx]\n",
			f, off, len, iswr?'w':'r', sl.len);
	else
		dFprint("slice m%#p[%#ullx:+%#ulx]%c -> m%#p:%#uld[%#ulx]\n",
			f, off, len, iswr?'w':'r',
			sl.b, (uchar*)sl.data - sl.b->d.data, sl.len);
	assert(sl.b ==  nil || sl.b->ref > 1);
	return sl;
}


uvlong
dfdirmap(Memblk *d, Dirf dirf, void *arg, int iswr)
{
	Blksl sl;
	daddrt *de;
	uvlong off;
	int i;

	isdir(d);
	assert(d->d.length/Daddrsz >= d->d.ndents);
	if(iswr){
		isrwlocked(d, iswr);
		ismelted(d);
	}
	off = 0;
	for(;;){
		sl = dfslice(d, Dblkdatasz, off, iswr);
		if(sl.len == 0)
			break;
		if(sl.b == nil)
			continue;
		if(catcherror()){
			mbput(sl.b);
			error(nil);
		}
		de = sl.data;
		for(i = 0; i < sl.len/Daddrsz; i++)
			if(dirf(sl.b, &de[i], arg) < 0){
				noerror();
				mbput(sl.b);
				return off + i*Daddrsz;
			}
		off += sl.len;
		noerror();
		mbput(sl.b);
	}
	return Noaddr;
}

static int
chdentryf(Memblk *b, daddrt *de, void *p)
{
	daddrt *addrs, addr, naddr;

	addrs = p;
	addr = addrs[0];
	naddr = addrs[1];
	if(*de != addr)
		return 0;	/* continue searching */

	if(naddr != addr){
		*de = naddr;
		changed(b);
	}
	return -1;		/* found: stop */
}

/*
 * Find a dir entry for addr (perhaps 0 == avail) and change it to
 * naddr. If iswr, the entry is allocated if needed and the blocks
 * melted on demand.
 * Return the offset for the entry in the file or Noaddr
 * Does not adjust disk refs.
 */
uvlong
dfchdentry(Memblk *d, daddrt addr, daddrt naddr)
{
	uvlong off;
	daddrt addrs[2] = {addr, naddr};

	dNprint("dfchdentry d%#010ullx -> d%#010ullx\nin %H\n", addr, naddr, d);
	off = dfdirmap(d, chdentryf, addrs, Wr);
	if(addr == 0 && naddr != 0){
		if(d->d.length < off+Daddrsz)
			d->d.length = off+Daddrsz;
		d->d.ndents++;
		changed(d);
	}else if(addr != 0 && naddr == 0){
		d->d.ndents--;
		changed(d);
	}
	return off;
}

typedef
struct Walkarg
{
	char *name;
	Memblk *f;
} Walkarg;

static int
findname(Memblk*, daddrt *de, void *p)
{
	Walkarg *w;

	w = p;
	if(*de == 0)
		return 0;

	w->f = dbget(DBfile, *de);
	if(strcmp(w->f->mf->name, w->name) != 0){
		mbput(w->f);
		return 0;
	}

	/* found  */
	dprint("dfwalk '%s' -> %H\n", w->name, w->f);
	return -1;
}

/*
 * Walk to a child and return it referenced.
 */
Memblk*
dfwalk(Memblk *d, char *name)
{
	Walkarg w;

	if(strcmp(name, "..") == 0)
		fatal("dfwalk: '..'");
	w.name = name;
	w.f = nil;
	if(dfdirmap(d, findname, &w, Rd) == Noaddr)
		error("file not found");
	return w.f;
}

/*
 * Return the last version for *fp, rwlocked, be it frozen or melted.
 */
void
dflast(Memblk **fp, int iswr)
{
	Memblk *f;

	f = *fp;
	isfile(f);
	rwlock(f, iswr);
	while(f->mf->melted != nil){
		incref(f->mf->melted);
		*fp = f->mf->melted;
		rwunlock(f, iswr);
		mbput(f);
		f = *fp;
		rwlock(f, iswr);
		if(!f->frozen)
			return;
	}
}

/*
 * Return *fp melted, by melting it if needed, and wlocked.
 * The reference from the (already melted) parent is adjusted,
 * as are the memory and disk references for the old file *fp.
 *
 * The parent is wlocked by the caller and unlocked upon return.
 */
Memblk*
dfmelt(Memblk *parent, Memblk **fp)
{
	Memblk *of, *nf;

	ismelted(parent);
	isrwlocked(parent, Wr);
	dflast(fp, Wr);
	of = *fp;
	if(of->frozen == 0){
		rwunlock(parent, Wr);
		return of;
	}
	if(catcherror()){
		rwunlock(of, Wr);
		rwunlock(parent, Wr);
		error(nil);
	}
	nf = dbdup(of);
	noerror();

	rwlock(nf, Wr);
	rwunlock(of, Wr);
	if(catcherror()){
		rwunlock(nf, Wr);
		mbput(nf);
		error(nil);
	}
	dfchdentry(parent, of->addr, nf->addr);
	dbput(of);
	mbput(of);
	*fp = nf;
	noerror();
	rwunlock(parent, Wr);
	return nf;
}

void
dfused(Path *p)
{
	Memblk *f;

	f = p->f[p->nf-1];
	isfile(f);
	rwlock(f, Wr);
	f->d.atime = fstime(0);
	rwunlock(f, Wr);
}

/*
 * Report that a file has been modified.
 * Modification times propagate up to the root of the file tree.
 * But frozen files are never changed.
 */
void
dfchanged(Path *p, int muid)
{
	Memblk *f;
	u64int t;
	int i;

	t = fstime(0);
	for(i = 0; i < p->nf; i++){
		f = p->f[i];
		rwlock(f, Wr);
		if(f->frozen == 0)
			if(!catcherror()){
				f->d.mtime = t;
				f->d.atime = t;
				f->d.muid = muid;
				changed(f);
				noerror();
			}
		rwunlock(f, Wr);
	}
}

/*
 * May be called with null parent, for root and ctl files.
 * The first call with a null parent is root, all others are ctl
 * files linked at root.
 */
Memblk*
dfcreate(Memblk *parent, char *name, int uid, ulong mode)
{
	Memblk *nf;
	Mfile *m;
	int isctl;

	if(fsfull())
		error("file system full");
	isctl = parent == nil;
	if(parent == nil)
		parent = fs->root;

	if(parent != nil){
		dprint("dfcreate '%s' %M at\n%H\n", name, mode, parent);
		isdir(parent);
		isrwlocked(parent, Wr);
		ismelted(parent);
	}else
		dprint("dfcreate '%s' %M", name, mode);

	if(isctl)
		nf = dballocz(DBctl, (mode&DMDIR)?DFdir:0, 1);
	else
		nf = dballocz(DBfile, (mode&DMDIR)?DFdir:0, 1);

	if(catcherror()){
		mbput(nf);
		if(parent != nil)
			rwunlock(parent, Wr);
		error(nil);
	}

	m = nf->mf;
	nf->d.id = fstime(0);
	nf->d.mode = mode;
	nf->d.mtime = nf->d.id;
	nf->d.atime = nf->d.id;
	nf->d.length = 0;
	m->uid = usrname(uid);
	nf->d.uid = uid;
	m->gid = m->uid;
	nf->d.gid = nf->d.uid;
	m->muid = m->uid;
	nf->d.muid = nf->d.uid;
	m->name = name;
	nf->d.asize = pmeta(nf->d.embed, Embedsz, nf);
	changed(nf);

	if(parent != nil){
		m->gid = parent->mf->gid;
		nf->d.gid = parent->d.gid;
		dfchdentry(parent, 0, nf->addr);
	}
	noerror();
	dprint("dfcreate-> %H\n within %H\n", nf, parent);
	return nf;
}

void
dfremove(Memblk *p, Memblk *f)
{
	vlong n;

	/* funny as it seems, we may need extra blocks to melt */
	if(fsfull())
		error("file system full");

	isrwlocked(f, Wr);
	isrwlocked(p, Wr);
	ismelted(p);
	if(DBDIR(f) != 0 && f->d.ndents > 0)
		error("directory not empty");
	incref(p);
	if(catcherror()){
		mbput(p);
		error(nil);
	}
	dfchdentry(p, f->addr, 0);
	/* shouldn't fail now. it's unlinked */

	if(p->d.ndents == 0 && p->d.length > 0){	/* all gone, make it public */
		p->d.length = 0;
		changed(p);
	}

	noerror();
	rwunlock(f, Wr);
	if(!catcherror()){
		n = dbput(f);
		noerror();
	}
	mbput(f);
	mbput(p);
}

/*
 * It's ok if a is nil, for reading ahead.
 */
ulong
dfpread(Memblk *f, void *a, ulong count, uvlong off)
{
	Blksl sl;
	ulong tot;
	char *p;

	p = a;
	isrwlocked(f, Rd);
	for(tot = 0; tot < count; tot += sl.len){
		sl = dfslice(f, count-tot, off+tot, Rd);
		if(sl.len == 0){
			assert(sl.b == nil);
			break;
		}
		if(sl.data == nil){
			memset(p+tot, 0, sl.len);
			assert(sl.b == nil);
			continue;
		}
		if(p != nil)
			memmove(p+tot, sl.data, sl.len);
		mbput(sl.b);
	}
	return tot;
}

ulong
dfpwrite(Memblk *f, void *a, ulong count, uvlong *off)
{
	Blksl sl;
	ulong tot;
	char *p;

	if(fsfull())
		error("file system full");

	isrwlocked(f, Wr);
	ismelted(f);
	p = a;
	if(f->d.mode&DMAPPEND)
		*off = f->d.length;
	for(tot = 0; tot < count;){
		sl = dfslice(f, count-tot, *off+tot, Wr);
		if(sl.len == 0 || sl.data == nil)
			fatal("dfpwrite: bug");
		memmove(sl.data, p+tot, sl.len);
		changed(sl.b);
		mbput(sl.b);
		tot += sl.len;
		if(*off+tot > f->d.length){
			f->d.length = *off+tot;
			changed(f);
		}
	}
	return tot;
}

int
ptrmap(daddrt addr, int nind, Blkf f, void *a, int isdisk)
{
	int i;
	Memblk *b;
	long tot;

	if(addr == 0)
		return 0;
	if(isdisk)
		b = dbget(DBdata+nind, addr);
	else{
		b = mbget(DBdata+nind, addr, Dontmk);
		if(b == nil)
			return 0;	/* on disk */
	}
	if(catcherror()){
		mbput(b);
		error(nil);
	}
	tot = 0;
	if(f == nil || f(b, a) == 0){
		tot++;
		if(nind > 0){
			for(i = 0; i < Dptrperblk; i++)
				tot += ptrmap(b->d.ptr[i], nind-1, f, a, isdisk);
		}
	}
	noerror();
	mbput(b);
	return tot;
}

static int
dumpf(Memblk*, daddrt *de, void *p)
{
	int isdisk;
	Memblk *f;

	if(*de == 0)
		return 0;

	isdisk = *(int*)p;
	if(isdisk)
		f = dbget(DBfile, *de);
	else
		f = mbget(DBfile, *de, Dontmk);
	if(f != nil){
		if(catcherror()){
			mbput(f);
			error(nil);
		}
		dfdump(f, isdisk);
		noerror();
		mbput(f);
	}
	return 0;
}

void
dfdump(Memblk *f, int isdisk)
{
	int i;
	extern int mbtab;

	isfile(f);
	/* visit the blocks to fetch them if needed, so %H prints them. */
	for(i = 0; i < nelem(f->d.dptr); i++)
		ptrmap(f->d.dptr[i], 0, nil, nil, isdisk);
	for(i = 0; i < nelem(f->d.iptr); i++)
		ptrmap(f->d.iptr[i], i+1, nil, nil, isdisk);
	fprint(2, "%H\n", f);
	if(DBDIR(f) != 0){
		mbtab++;
		if(!catcherror()){
			dfdirmap(f, dumpf, &isdisk, Rd);
			noerror();
		}
		mbtab--;
	}
}

static void
freezeaddr(daddrt addr)
{
	Memblk *f;

	if(addr == 0)
		return;
	f = mbget(DBfile, addr, Dontmk);
	if(f == nil)			/* must be frozen */
		return;
	if(catcherror()){
		mbput(f);
		error(nil);
	}
	dffreeze(f);
	noerror();
	mbput(f);

}

static int
bfreeze(Memblk *b, void*)
{
	int i;

	if(b->frozen)
		return -1;
	b->frozen = 1;
	if(b->type == DBdata && DBDIR(b))
		for(i = 0; i < Dblkdatasz/Daddrsz; i++)
			freezeaddr(b->d.ptr[i]);
	return 0;
}

long
dffreeze(Memblk *f)
{
	int i;
	long tot;
	ulong doff;

	isfile(f);
	if(f->frozen)
		return 0;
	rwlock(f, Wr);
	if(catcherror()){
		rwunlock(f, Wr);
		error(nil);
	}
	f->frozen = 1;
	tot = 1;
	if(DBDIR(f))
		for(doff = embedattrsz(f); doff < Embedsz; doff += Daddrsz)
			freezeaddr(*(daddrt*)(f->d.embed+doff));
	for(i = 0; i < nelem(f->d.dptr); i++)
		tot += ptrmap(f->d.dptr[i], 0, bfreeze, nil, Mem);
	for(i = 0; i < nelem(f->d.iptr); i++)
		tot += ptrmap(f->d.iptr[i], i+1, bfreeze, nil, Mem);
	noerror();
	rwunlock(f, Wr);
	return tot;
}


/*
 * Caller walked down p, and now requires the nth element to be
 * melted, and wlocked for writing. (nth count starts at 1);
 * 
 * Return the path with the version of f that we must use,
 * locked for writing and melted.
 * References kept in the path are traded for the ones returned.
 *
 * Calls from user requests wait until /archive is melted.
 * Calls from fsfreeze(), fsreclaim(), etc. melt /archive.
 */
Path*
meltedpath(Path **pp, int nth, int user)
{
	int i;
	Memblk *f, **fp;
	Path *p;

	ownpath(pp);
	p = *pp;
	assert(nth >= 1 && p->nf >= nth && p->nf >= 2);
	assert(p->f[0] == fs->root);
	fp = &p->f[nth-1];

	/*
	 * 1. Optimistic: Try to get a loaded melted version for f.
	 */
	dflast(fp, Wr);
	f = *fp;
	if(!f->frozen)
		return p;
	ainc(&fs->nmelts);
	rwunlock(f, Wr);

	/*
	 * 2. Realistic:
	 * walk down the path, melting every frozen thing until we
	 * reach f. Keep wlocks so melted files are not frozen while we walk.
	 * /active is special, because it's only frozen temporarily while
	 * creating a frozen version of the tree. Instead of melting it,
	 * we should just wait for it if the call is from a user RPC.
	 * p[0] is /
	 * p[1] is /active or /archive
	 */
	if(!user){
		rwlock(p->f[0], Wr);
		i = 1;
	}else{
		for(;;){
			dflast(&p->f[1], Wr);
			if(p->f[1]->frozen == 0)
				break;
			rwunlock(p->f[1], Wr);
			yield();
		}
		i = 2;
	}
	for(; i < nth; i++)
		dfmelt(p->f[i-1], &p->f[i]);
	return p;
}

/*
 * Advance path to use the most recent version of each file.
 */
Path*
lastpath(Path **pp, int nth)
{
	Memblk *f;
	Path *p;
	int i;

	p = *pp;
	for(i = 0; i < nth; i++){
		f = p->f[i];
		if(f != nil && f->mf != nil && f->mf->melted != nil)
			break;
	}
	if(i == nth)
		return p;	/* all files have the last version */

	ownpath(pp);
	p = *pp;
	for(i = 0; i < nth; i++){
		dflast(&p->f[i], Rd);
		rwunlock(p->f[i], Rd);
	}
	return p;
}

