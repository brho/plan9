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
 * All the code assumes outofmemoryexits = 1.
 */

enum
{
	Lru = 0,
	Freeze,
	Write,
	Nfsops,
};

Fsys *fs;
uvlong maxfsz;

vlong fsoptime[Nfsops];
ulong nfsopcalls[Nfsops];

static char* fsopname[] =
{
[Lru]		"lru",
[Freeze]	"freeze",
[Write]		"write",
};

char statstext[Statsbufsz], *statsp;

void
quiescent(int y)
{
	if(y == No)
		xrwlock(&fs->quiescence, Rd);
	else
		xrwunlock(&fs->quiescence, Rd);
}

uvlong
fsdiskfree(void)
{
	uvlong nfree;

	xqlock(fs);
	nfree = fs->super->d.ndfree;
	nfree += (fs->limit - fs->super->d.eaddr)/Dblksz;
	xqunlock(fs);
	return nfree;
}

static char*
fsstats(char *s, char *e, int clr, int verb)
{
	int i;

	s = seprint(s, e, "mblks:\t%4ulld nblk %4ulld nablk %4ulld mused %4ulld mfree\n",
		fs->nblk, fs->nablk, fs->nmused, fs->nmfree);
	s = seprint(s, e, "lists:\t%4uld clean %#4uld dirty %#4uld refs %4uld total\n",
		fs->clean.n, fs->dirty.n, fs->refs.n,
		fs->clean.n + fs->dirty.n + fs->refs.n);
	s = seprint(s, e, "dblks:\t %4ulld nblk %4ulld nfree (%ulld list + %ulld rem)\n",
		fs->limit/Dblksz - 1, fsdiskfree(), fs->super->d.ndfree,
		(fs->limit - fs->super->d.eaddr)/Dblksz);
	s = seprint(s, e, "paths:\t%4uld alloc %4uld free (%4uld bytes)\n",
		pathalloc.nalloc, pathalloc.nfree, pathalloc.elsz);
	s = seprint(s, e, "mfs:\t%4uld alloc %4uld free (%4uld bytes)\n",
		mfalloc.nalloc, mfalloc.nfree, mfalloc.elsz);

	if(verb == 0)
		return s;
	s = seprint(s, e, "nmelts:\t%d\n", fs->nmelts);
	s = seprint(s, e, "nindirs:\t");
	for(i = 0; i < nelem(fs->nindirs); i++){
		s = seprint(s, e, "%d ", fs->nindirs[i]);
		if(clr)
			fs->nindirs[i] = 0;
	}
	s = seprint(s, e, "\n");
	s = seprint(s, e, "\n");
	s = seprint(s, e, "Fsysmem:\t%uld\n", Fsysmem);
	s = seprint(s, e, "Mzerofree:\t%d\tMminfree:\t%d\tMmaxfree:\t%d\n",
		Mzerofree, Mminfree, Mmaxfree);
	s = seprint(s, e, "Dzerofree:\t%d\tDminfree:\t%d\tDmaxfree:\t%d\n",
		Dzerofree, Dminfree, Dmaxfree);
	s = seprint(s, e, "Mmaxdirtypcent:\t%d\n", Mmaxdirtypcent);
	s = seprint(s, e, "Dblksz:  \t%uld\n", Dblksz);
	s = seprint(s, e, "Mblksz:  \t%ud\n", sizeof(Memblk));
	s = seprint(s, e, "Dminattrsz:\t%uld\n", Dminattrsz);
	s = seprint(s, e, "Nblkgrpsz:\t%uld\n", Nblkgrpsz);
	s = seprint(s, e, "Dblkdatasz:\t%d\n", Dblkdatasz);
	s = seprint(s, e, "Embedsz:\t%d\n", Embedsz);
	s = seprint(s, e, "Dentryperblk:\t%d\n", Dblkdatasz/Daddrsz);
	s = seprint(s, e, "Dptrperblk:\t%d\n\n", Dptrperblk);

	for(i = 0; i < nelem(nfsopcalls); i++){
		if(nfsopcalls[i] == 0)
			s = seprint(s, e, "%s:\t0 calls\t0 µs\n", fsopname[i]);
		else
			s = seprint(s, e, "%s:\t%uld calls\t%ulld µs\n", fsopname[i],
				nfsopcalls[i], (fsoptime[i]/nfsopcalls[i])/1000);
		if(clr){
			nfsopcalls[i] = 0;
			fsoptime[i] = 0;
		}
	}
	return s;
}

char*
updatestats(int clr, int verb)
{
	static QLock statslk;

	if(clr)
		warn("clearing stats");
	xqlock(&statslk);
	statsp = statstext;
	*statsp = 0;
	statsp = fsstats(statsp, statstext+sizeof statstext, clr, verb);
	statsp = ninestats(statsp, statstext+sizeof statstext, clr, verb);
	statsp = ixstats(statsp, statstext+sizeof statstext, clr, verb);
	xqunlock(&statslk);
	return statstext;
}

int
isro(Memblk *f)
{
	return f == fs->archive || f == fs->root || f == fs->cons || f == fs->stats;
}

/*
 * NO LOCKS. debug only
 *
 */
void
fsdump(int full, int disktoo)
{
	int i, n, x;
	Memblk *b;
	daddrt a;
	extern int fullfiledumps;

	x = fullfiledumps;
	fullfiledumps = full;
	nodebug();
	if(fs != nil){
		fprint(2, "\n\nfsys '%s' limit %#ullx super m%#p root m%#p:\n",
			fs->dev, fs->limit, fs->super, fs->root);
		fprint(2, "%H\n", fs->super);
		dfdump(fs->root, disktoo);
		mlistdump("refs", &fs->refs);
		if(1){
			n = 0;
			fprint(2, "hash:");
			for(i = 0; i < nelem(fs->fhash); i++)
				for(b = fs->fhash[i].b; b != nil; b = b->next){
					if(n++ % 5 == 0)
						fprint(2, "\n\t");
					fprint(2, "d%#010ullx ", EP(b->addr));
				}
			fprint(2, "\n");
		}
	}
	if(fs->super->d.free != 0){
		fprint(2, "free:");
		i = 0;
		for(a = fs->super->d.free; a != 0; a = dbgetref(a)){
			if(i++ % 5 == 0)
				fprint(2, "\n\t");
			fprint(2, "d%#010ullx ", EP(a));
		}
		fprint(2, "\n");
	}
	mlistdump("mru", &fs->clean);
	mlistdump("dirty", &fs->dirty);
	fprint(2, "%s\n", updatestats(0, 1));
	fullfiledumps = x;
	debug();
}

static daddrt
disksize(int fd)
{
	Dir *d;
	daddrt sz;

	d = dirfstat(fd);
	if(d == nil)
		return 0;
	sz = d->length;
	free(d);
	return sz;
}

/*
 * To preserve coherency, blocks written are always frozen.
 * DBref blocks with RCs and the free block list require some care:
 *
 * On disk, the super block indicates that even (odd) DBref blocks are active.
 * On memory, the super selects even (odd) refs (we read refs from there.)
 * To sync...
 * 1. we make a frozen super to indicate that odd (even) DBrefs are active.
 * 2. we write odd (even) DBref blocks.
 * 3. the frozen super is written, indicating that odd (even) refs are in use.
 *    (The disk is coherent now, pretending to use odd (even) refs).
 * 4. The memory super is udpated to select odd (even) DBref blocks.
 *    (from now on, we are loading refs from odd (even) blocks.
 * 5. we update even (odd) DBref blocks, so we can get back to 1.
 *    with even/odd swapped.
 *
 */

static void
freezesuperrefs(void)
{
	Memblk *b, *rb;

	b = mballocz(fs->super->addr, 0);
	xqlock(fs);
	b->type = fs->super->type;
	b->d = fs->super->d;
	b->d.oddrefs = !fs->super->d.oddrefs;
	assert(fs->fzsuper == nil);
	fs->fzsuper = b;
	b->frozen = 1;
	b->dirty = 1;	/* so it's written */
	xqlock(&fs->refs);
	for(rb = fs->refs.hd; rb != nil; rb = rb->lnext){
		rb->frozen = 1;
		rb->changed = rb->dirty;
	}
	xqunlock(&fs->refs);
	xqunlock(fs);
}

static Memblk*
readsuper(void)
{
	Memblk *super;
	Dsuperdata *d1, *d2;

	if(catcherror()){
		error("not a creepy disk: %r");
		error(nil);
	}
	fs->super = dbget(DBsuper, Dblksz);
	super = fs->super;
	if(super->d.magic != MAGIC)
		error("bad magic number");
	d1 = &fs->super->d.Dsuperdata;
	d2 = &fs->super->d.dup;
	if(memcmp(d1, d2, sizeof(Dsuperdata)) != 0){
		warn("partially written superblock, using old.");
		if(fs->super->d.dup.epoch < fs->super->d.epoch)
			fs->super->d.Dsuperdata = fs->super->d.dup;
	}
	if(super->d.dblksz != Dblksz)
		error("bad Dblksz");
	if(super->d.nblkgrpsz != Nblkgrpsz)
		error("bad Nblkgrpsz");
	if(super->d.dminattrsz != Dminattrsz)
		error("bad Dminattrsz");
	if(super->d.ndptr != Ndptr)
		error("bad ndptr");
	if(super->d.niptr != Niptr)
		error("bad niptr");
	if(super->d.embedsz != Embedsz)
		error("bad Embedsz");
	if(super->d.dptrperblk != Dptrperblk)
		error("bad Dptrperblk");

	noerror();
	return super;
}

/*
 * Return /archive/yyyy/mmdd melted and wlocked, create it if needed.
 * Clear the arch addr in the super if a new archive should be taken.
 */
static Path*
currentarch(void)
{
	Path *p;
	Memblk *f, *pf;
	char yname[30], dname[30], *names[2];
	Tm *tm;
	int i;

	tm = localtime(time(nil));
	seprint(yname, yname+sizeof yname, "%04d", tm->year + 1900);
	seprint(dname, dname+sizeof dname, "%02d%02d", tm->mon + 1, tm->mday);
	names[0] = yname;
	names[1] = dname;

	p = newpath(fs->root);
	addelem(&p, fs->archive);
	for(i = 0; i < nelem(names); i++){
		if(catcherror())
			break;
		pf = p->f[p->nf-1];
		rwlock(pf, Rd);
		if(catcherror()){
			rwunlock(pf, Rd);
			error(nil);
		}
		f = dfwalk(pf, names[i]);
		addelem(&p, f);
		mbput(f);
		noerror();
		rwunlock(pf, Rd);
		noerror();
	}
	meltedpath(&p, p->nf, 0);
	if(catcherror()){
		rwunlock(p->f[p->nf-1], Wr);
		error(nil);
	}
	/* 0:/ 1:archive 2:yyyy 3:mmdd */
	for(i = p->nf-1; i < 3; i++){
		f = dfcreate(p->f[i], names[i-1], p->f[i]->d.uid, p->f[i]->d.mode);
		rwlock(f, Wr);
		rwunlock(p->f[i], Wr);
		addelem(&p, f);
		mbput(f);
	}
	noerror();

	return p;
}

static void
updateroot(Memblk *nf)
{
	if(fs->super->d.root != nf->addr){
		fs->archive = nf;
		incref(nf);
		fs->super->d.root = nf->addr;
		changed(fs->super);
	}
}

/*
 * Freeze the file tree, keeping active as a new melted file
 * that refers to frozen children now in the archive.
 * returns the just frozen tree or nil
 *
 * NB: This may be called from fsfmt(), with a melted archive,
 * which violates the invariant that archive is always frozen, leading
 * to a violation on the expected number of references to it (fsfmt leaks it).
 */
static Memblk*
fsfreeze(void)
{
	Path *p;
	Memblk *na, *oa, *arch, *oarch;
	char name[50];
	vlong t0;

	dZprint("freezing fs...\n");
	if(fs->profile)
		t0 = fstime(nsec());
	xqlock(&fs->fzlk);
	if(fs->fzsuper != nil){
		/*
		 * we did freeze/reclaim and are still writing, can't freeze now.
		 */
		xqunlock(&fs->fzlk);
		return nil;
	}
	xrwlock(&fs->quiescence, Wr);	/* not really required */
	nfsopcalls[Freeze]++;
	if(catcherror()){
		/*
		 * There was an error during freeze.
		 * It's better not to continue to prevent disk corruption.
		 * The user is expected to restart from the last frozen
		 * version of the tree.
		 */
		fatal("freeze: %r");
	}

	/* 1. Move active into /archive/yyyy/mmdd/<epochnb>.
	 * We must add an extra disk ref to keep archive alive after melting
	 * it within currentarch() because "/" is a fake and there's no old
	 * frozen copy for "/" (keeping such ref).
	 *
	 * Dbput will unlink the block from the hash and move its address
	 * into the free list. However, we still have a mem ref from fs->archive
	 * and perhaps more from user paths, which must be advanced, so we can't
	 * release the reference on archive just yet.
	 * We will do the mbput corresponding to fs->archive after
	 * advancing all fids, so their archive moves to the new one.
	 */
	arch = fs->archive;
	dbincref(arch->addr);
	p = currentarch();
	updateroot(p->f[1]);
	oarch = arch;
	dbput(arch);

	arch = p->f[p->nf-1];
	oa = fs->active;
	rwlock(oa, Wr);
	seprint(name, name+sizeof(name), "%ulld", oa->d.mtime);
	wname(oa, name);
	dfchdentry(arch, 0, oa->addr);

	/* 2. Freeze it, plus any melted blocks in /active due to
	 * the link of the new archived tree.
	 */
	oa->d.mtime = fstime(0);
	oa->d.atime = fstime(0);
	rwunlock(oa, Wr);
	changed(oa);
	dffreeze(oa);
	rwunlock(arch, Wr);
	dffreeze(fs->archive);

	/* 2. Freeze the on-disk reference counters
	 * and the state of the super-block.
	 * After doing so, the state to be written on the disk is
	 * coherent and corresponds to now.
	 */
	dprint("freezing refs...\n");
	freezesuperrefs();

	/* 3. Make a new active and replace the old one.
	 * defer the release of the old active until all fids are melted
	 * (see similar discussion in 1).
	 */
	na = dbdup(oa);
	rwlock(na, Wr);
	na->d.id = fstime(0);
	wname(na, "active");
	fs->active = na;
	rwunlock(na, Wr);
	rwlock(fs->root, Wr);
	dfchdentry(fs->root, oa->addr, na->addr);
	rwunlock(fs->root, Wr);
	assert(oa->ref > 1);	/* release fs->active */

	/* 4. Advance pahts in fids to their most recent melted files,
	 * to release refs to old frozen files, and to the now gone old
	 * "/archive".
	 */
	meltfids();
	mbput(oa);
	mbput(oarch);
	if(fs->profile)
		fsoptime[Freeze] += nsec() - t0;
	noerror();
	xrwunlock(&fs->quiescence, Wr);
	xqunlock(&fs->fzlk);
	putpath(p);

	dZprint("fs frozen\n");
	return na;
}

static long
writerefs(void)
{
	Memblk *rb;
	long n;

	n = 0;
	xqlock(&fs->refs);
	for(rb = fs->refs.hd; rb != nil; rb = rb->lnext){
		if((rb->addr - Dblk0addr)/Dblksz % Nblkgrpsz == 2){
			/* It's a fake DBref block used for checks: ignore. */
			rb->frozen = rb->dirty = 0;
			continue;
		}
		if(rb->dirty && rb->frozen)
			n++;
		meltedref(rb);
	}
	xqunlock(&fs->refs);
	return n;
}

static int
mustwrite(Memblk *b)
{
	return b->frozen != 0;
}

/*
 * Written blocks become mru, perhaps we should
 * consider keeping their location in the clean list, at the
 * expense of visiting them while scanning for blocks to move out.
 * We write only (dirty) blocks that are frozen or part of the "/archive" file.
 */
static long
writedata(void)
{
	Memblk *b;
	long nw;
	List dl;

	nw = 0;
	dl = mfilter(&fs->dirty, mustwrite);
	while((b = dl.hd) != nil){
		munlink(&dl, b, 1);
		assert(b->dirty);
		if((b->addr&Fakeaddr) != 0)
			fatal("write data on fake address");
		dbwrite(b);
		nw++;
	}
	return nw;
}

static void
writezsuper(void)
{
	if(canqlock(&fs->fzlk))
		fatal("writezsuper: lock");
	assert(fs->fzsuper != nil);
	fs->fzsuper->d.epoch = fstime(0);
	fs->fzsuper->d.dup = fs->fzsuper->d.Dsuperdata;
	dbwrite(fs->fzsuper);
	dprint("writezsuper: %H\n", fs->fzsuper);
	mbput(fs->fzsuper);
	fs->fzsuper = nil;
}

static void
syncref(daddrt addr)
{
	static Memblk b;

	b.addr = addr;
	b.type = DBref;
	dbread(&b);
	if(fs->super->d.oddrefs == 0) /* then the old ones are odd */
		addr += Dblksz;
	dWprint("syncref d%#010ullx at d%#010ullx\n", b.addr, addr);
	if(pwrite(fs->fd, &b.d, sizeof b.d, addr) != sizeof b.d)
		error("syncref: write: %r");
}

static void
syncrefs(void)
{
	Memblk *rb;

	fs->super->d.oddrefs = !fs->super->d.oddrefs;
	xqlock(&fs->refs);
	rb = fs->refs.hd;
	xqunlock(&fs->refs);
	for(; rb != nil; rb = rb->lnext){
		if(rb->changed)
			syncref(rb->addr);
		rb->changed = 0;
	}
}

/*
 * Write any dirty frozen state after a freeze.
 * Only this function and initialization routines (i.e., super, refs)
 * may lead to writes.
 */
static void
fswrite(void)
{
	vlong t0;
	long nr, nb;

	dZprint("writing fs...\n");
	if(fs->profile)
		t0 = fstime(nsec());
	xqlock(&fs->fzlk);
	nfsopcalls[Write]++;
	if(fs->fzsuper == nil)
		fatal("can't fswrite if we didn't fsfreeze");
	if(catcherror()){
		fsoptime[Write] += nsec() - t0;
		xqunlock(&fs->fzlk);
		error(nil);
	}
	nr = writerefs();
	nb = writedata();
	writezsuper();
	nb++;
	syncrefs();
	noerror();
	if(fs->profile)
		fsoptime[Write] += fstime(nsec()) - t0;
	fs->wtime = fstime(nsec());
	xqunlock(&fs->fzlk);
	dZprint("fs written (2*%ld refs %ld data)\n", nr, nb);
	if(fs->halt){
		warn("halted. exiting.");
		threadexitsall(nil);
	}
}

static void
fsinit(char *dev, int nblk)
{
	uvlong fact, i;
	void *p;
	char *c, *e;

	/* this is an invariant that must hold for directories */
	assert(Embedsz % Daddrsz == 0);
	maxfsz = Ndptr*Dblkdatasz;
	fact = 1;
	for(i = 0; i < Niptr; i++){
		maxfsz += Dptrperblk * fact;
		fact *= Dptrperblk;
	}

	fs = mallocz(sizeof *fs, 1);
	fs->dev = strdup(dev);
	fs->fd = open(dev, ORDWR);
	if(fs->fd < 0)
		fatal("can't open disk: %r");

	fs->nablk = Fsysmem / sizeof(Memblk);
	if(nblk > 0 && nblk < fs->nablk)
		fs->nablk = nblk;
	fs->limit = disksize(fs->fd);
	fs->ndblk = fs->limit/Dblksz;
	fs->limit = fs->ndblk*Dblksz;
	if(fs->limit < 10*Dblksz)
		fatal("buy a larger disk");
	if(fs->nablk > fs->ndblk){
		warn("using %ulld blocks and not %ulld (small disk)",
			fs->ndblk, fs->nablk);
		fs->nablk = fs->ndblk;
	}
	p = malloc(fs->nablk * sizeof fs->blk[0]);
	fs->blk = p;
	warn("prepaging...");
	c = p;
	e = c + fs->nablk * sizeof fs->blk[0];
	for(; c < e; c += 4096)
		*c = 0;		/* prepage it */
	fstime(nsec());
	dprint("fsys '%s' init\n", fs->dev);
}

void
fssync(void)
{
	if(fsfreeze())
		fswrite();
}

static int
confirm(char *msg)
{
	char buf[100];
	int n;

	fprint(2, "%s [y/n]: ", msg);
	n = read(0, buf, sizeof buf - 1);
	if(n <= 0)
		return 0;
	if(buf[0] == 'y')
		return 1;
	return 0;
}

enum
{
	Fossiloff = 128*1024,
	Fossilmagic = 0xffae7637,
};

static void
dontoverwrite(daddrt addr)
{
	Dsuperdata d;
	static char buf[BIT64SZ];

	if(pread(fs->fd, &d, sizeof d, addr + sizeof(Diskblkhdr)) != sizeof d)
		return;
	if(d.magic == MAGIC)
		if(!confirm("disk has a creepy fs: continue?")){
			warn("aborting");
			threadexitsall("no");
		}else
			return;

	if(pread(fs->fd, buf, sizeof buf, Fossiloff) != sizeof buf)
		return;
	if(GBIT32(buf) == Fossilmagic)
		if(!confirm("disk has a fossil fs: continue?")){
			warn("aborting");
			threadexitsall("no");
		}else{
			memset(buf, 0, sizeof buf);
			pwrite(fs->fd, buf, sizeof buf, Fossiloff);
		}
}

/*
 * / is only in memory. It's `on-disk' address is Noaddr.
 *
 * /archive is the root on disk.
 * /active is allocated on disk, but not on disk. It will be linked into
 * /archive as a child in the future.
 */
void
fsfmt(char *dev, int force)
{
	Memblk *super;
	int uid;

	fsinit(dev, Mmaxfree);	/* enough # of blocks for fmt */

	if(catcherror())
		fatal("fsfmt: error: %r");

	fs->super = dballocz(DBsuper, DFreg, 1);

	if(!force)
		dontoverwrite(fs->super->addr);
	super = fs->super;
	super->d.magic = MAGIC;
	super->d.eaddr = fs->super->addr + Dblksz;
	super->d.dblksz = Dblksz;
	super->d.nblkgrpsz = Nblkgrpsz;
	super->d.dminattrsz = Dminattrsz;
	super->d.ndptr = Ndptr;
	super->d.niptr = Niptr;
	super->d.embedsz = Embedsz;
	super->d.dptrperblk = Dptrperblk;
	uid = usrid(getuser());
	fs->root = dfcreate(nil, "", uid, DMDIR|0555);
	rwlock(fs->root, Wr);
	fs->active = dfcreate(fs->root, "active", uid, DMDIR|0775);
	fs->archive = dfcreate(fs->root, "archive", uid, DMDIR|0555);
	rwunlock(fs->root, Wr);
	super->d.root = fs->archive->addr;
	fssync();

	noerror();
}

void
timeproc(void*)
{
	threadsetname("timeproc");
	for(;;){
		sleep(1);
		fstime(nsec());
	}
}

/*
 * If there are dirty blocks, call the policy once per Syncival.
 */
void
fssyncproc(void*)
{
	threadsetname("syncer");
	errinit(Errstack);
	for(;;){
		sleep(Syncival*1000);
		fspolicy(Post);
	}
}

typedef struct Parg
{
	int last;
	Memblk *which;
} Parg;
enum{First = 0, Last = 1};

static int
pickf(Memblk*, daddrt *de, void *a)
{
	Parg *pa;
	Memblk *c;

	pa = a;
	if(*de == 0)
		return 0;
	c = dbget(DBfile, *de);
	if(pa->which == nil || (pa->last && pa->which->d.mtime < c->d.mtime) ||
	  (!pa->last && pa->which->d.mtime > c->d.mtime)){
		mbput(pa->which);
		pa->which = c;
		incref(c);
	}
	mbput(c);
	return 0;
}

/*
 * Return the first or last children, for selecting archives.
 */
static Memblk*
pickchild(Memblk *f, int last)
{
	Parg pa;

	pa.which = nil;
	pa.last = last;
	rwlock(f, Rd);
	if(catcherror()){
		rwunlock(f, Rd);
		mbput(pa.which);
		error(nil);
	}
	dfdirmap(f, pickf, &pa, Rd);
	noerror();
	rwunlock(f, Rd);
	return pa.which;
}

static Path*
pickvictim(void)
{
	Path *p;
	int i;

	if(fs->archive->d.ndents == 0)
		return  nil;

	p = newpath(fs->root);
	if(catcherror()){
		putpath(p);
		return nil;
	}
	addelem(&p, fs->archive);

	/* yyyy mmdd epoch */
	for(i = 0; i < 3 && p->f[p->nf-1]->d.ndents > 0; i++){
		addelem(&p, pickchild(p->f[p->nf-1], First));
		mbput(p->f[p->nf-1]);
	}
	for(i = 1; i < p->nf; i++)
		if(p->f[i]->d.ndents > 1)
			break;

	if(i == p->nf){		/* last snap; nothing to reclaim */
		putpath(p);
		p = nil;
	}

	noerror();
	return p;
}

/*
 * One process per file system, so consume all the memory
 * for the cache.
 * To open more file systems, use more processes!
 */
void
fsopen(char *dev, int worm, int canwr)
{
	Memblk *arch, *last, *c;
	int uid;

	if(catcherror())
		fatal("fsopen: error: %r");

	fsinit(dev, 0);
	readsuper();
	fs->worm = worm;
	fs->mode = canwr;
	uid = usrid("sys");
	xqlock(&fs->fzlk);
	fs->root = dfcreate(nil, "", uid, DMDIR|0555);
	arch = dbget(DBfile, fs->super->d.root);
	fs->archive = arch;
	rwlock(fs->root, Wr);
	rwlock(arch, Wr);
	dfchdentry(fs->root, 0, arch->addr);
	rwunlock(arch, Wr);
	rwunlock(fs->root, Wr);

	last = pickchild(arch, Last);		/* yyyy */
	if(last != nil){
		c = pickchild(last, Last);	/* mmdd */
		mbput(last);
		last = c;
	}
	if(last != nil){
		c = pickchild(last, Last);	/* epoch */
		mbput(last);
		last = c;
	}
	rwlock(fs->root, Wr);
	if(last != nil){
		rwlock(last, Rd);
		fs->active = dbdup(last);
		rwunlock(last, Rd);
		mbput(last->mf->melted);	/* could keep it, but no need */
		last->mf->melted = nil;
		wname(fs->active, "active");
		fs->active->d.id = fstime(nsec());
		rwlock(fs->active, Wr);
		dfchdentry(fs->root, 0, fs->active->addr);
		rwunlock(fs->active, Wr);
		mbput(last);
	}else
		fs->active = dfcreate(fs->root, "active", uid, DMDIR|0775);

	fs->cons = dfcreate(nil, "cons", uid, DMEXCL|0660);
	fs->cons->d.gid = usrid("adm");
	fs->cons->mf->gid = "adm";
	changed(fs->cons);
	fs->stats = dfcreate(nil, "stats", uid, 0664);
	rwunlock(fs->root, Wr);
	fs->consc = chancreate(sizeof(char*), 256);
	xqunlock(&fs->fzlk);

	noerror();

	/*
	 * Try to load the /active/users file, if any,
	 * but ignore errors. We already have a default table loaded
	 * and may operate using it.
	 */
	if(!catcherror()){
		c = dfwalk(fs->active, "users");
		rwlock(c, Wr);
		if(catcherror()){
			rwunlock(c, Wr);
			mbput(c);
			error(nil);
		}
		rwusers(c);
		noerror();
		rwunlock(c, Wr);
		mbput(c);
		noerror();
		fs->cons->d.uid = usrid(getuser());
		fs->cons->mf->uid = getuser();
	}
	fs->wtime = fstime(nsec());
}

uvlong
fsmemfree(void)
{
	uvlong nfree;

	xqlock(fs);
	nfree = fs->nablk - fs->nblk;
	nfree += fs->nmfree;
	xqunlock(fs);
	return nfree;
}

/*
 * Check if we are low on memory and move some blocks out in that case.
 * This does not acquire locks on blocks, so it's safe to call it while
 * keeping some files/blocks locked.
 */
int
fslru(void)
{
	Memblk *b, *bprev;
	vlong t0;
	int x;
	long target, tot, n, ign;

	x = setdebug();
	dZprint("fslru: low on memory %ulld free %d min\n", fsmemfree(), Mminfree);
	tot = ign = 0;
	do{
		target = Mmaxfree - fsmemfree();
		t0 = nsec();
		xqlock(&fs->clean);
		nfsopcalls[Lru]++;
		if(catcherror()){
			fsoptime[Lru] += t0 - nsec();
			xqunlock(&fs->clean);
			warn("fslru: %r");
			break;
		}
		n = 0;
		for(b = fs->clean.tl; b != nil && target > 0; b = bprev){
			bprev = b->lprev;
			if(b->dirty)
				fatal("fslru: dirty block on clean\n");
			switch(b->type){
			case DBfree:
				/* can happen. but, does it? */
				fatal("fslru: DBfree on clean\n");
			case DBsuper:
			case DBref:
				fatal("fslru: type %d found on clean\n", b->type);
			case DBfile:
				if(b == fs->root || b == fs->active || b == fs->archive){
					ign++;
					continue;
				}
				break;
			}
			if(b->ref > 1){
				ign++;
				continue;
			}
			/*
			 * Blocks here have one ref because of the hash table,
			 * which means they are are not used.
			 * We release the hash ref to let them go.
			 * bprev can't move while we put b.
			 */
			dOprint("fslru: out: m%#p d%#010ullx\n", b, b->addr);
			if(mbunhash(b, 1)){
				n++;
				tot++;
				target--;
			}
		}
		noerror();
		fsoptime[Lru] += t0 - nsec();
		xqunlock(&fs->clean);
	}while(n > 0 && target > 0);
	if(tot == 0){
		warn("low on mem (0 out; %uld ignored)", ign);
		tot = -1;
	}else
		dZprint("fslru: %uld out %uld ignored %ulld free %d min %d max\n",
			tot, ign, fsmemfree(), Mminfree, Mmaxfree);
	rlsedebug(x);
	return tot;
}

int
fsfull(void)
{
	if(fsdiskfree() > Dzerofree)
		return 0;

	if(1){
		warn("file system full");
		fsdump(0, Mem);
		fatal("aborting");
	}
	return 1;
}

int
fsreclaim(void)
{
	Memblk *victim, *arch;
	long n, tot;
	Path *p;

	xqlock(&fs->fzlk);
	if(catcherror()){
		warn("reclaim: %r");
		xqunlock(&fs->fzlk);
		return 0;
	}
	warn("%ulld free: reclaiming...", fsdiskfree());
	if(fs->fzsuper != nil){
		/*
		 * we did freeze/reclaim and are still writing, can't reclaim now.
		 */
		noerror();
		xqunlock(&fs->fzlk);
		warn("write in progress. refusing to reclaim");
		return 0;
	}

	tot = 0;
	for(;;){

		/*
		 * The logic regarding references for reclaim is similar
		 * to that described in fsfreeze().
		 * Read that comment before this code.
		 */
		dprint("fsreclaim: reclaiming\n");
		p = pickvictim();
		if(p == nil){
			dprint("nothing to reclaim\n");
			break;
		}
		if(catcherror()){
			putpath(p);
			error(nil);
		}
		assert(p->nf > 2);
		victim = p->f[p->nf-1];
		warn("reclaiming '%s'", victim->mf->name);
		dprint("%H\n", victim);
		arch = fs->archive;
		dbincref(arch->addr);		/* see comment in fsfreeze() */
		meltedpath(&p, p->nf-1, 0);
		updateroot(p->f[1]);
		if(catcherror()){
			rwunlock(p->f[p->nf-2], Wr);
			error(nil);
		}
		dfchdentry(p->f[p->nf-2], victim->addr, 0);
		noerror();
		rwunlock(p->f[p->nf-2], Wr);
		n = dbput(victim);
		dbput(arch);
		mbput(arch);
		noerror();
		putpath(p);
		dffreeze(fs->archive);
		dprint("fsreclaim: %uld file%s reclaimed\n", n, n?"s":"");
		tot += n;

		if(fsdiskfree() > Dmaxfree){
			dprint("fsreclaim: %d free: done\n", Dmaxfree);
			break;
		}
	}
	if(tot == 0){
		warn("low on disk: 0 files reclaimed %ulld blocks free",
			fsdiskfree());
		tot = -1;
	}else
		warn("%uld file%s reclaimed %ulld blocks free",
			tot, tot?"s":"", fsdiskfree());
	noerror();
	xqunlock(&fs->fzlk);
	return tot;
}

static int
fsdirtypcent(void)
{
	long n, ndirty;

	n = fs->clean.n;
	ndirty = fs->dirty.n;

	return (ndirty*100)/(n + ndirty);
}

/*
 * Policy for memory and and disk block reclaiming.
 * Called from the sync proc from time to time and also AFTER each RPC.
 */
void
fspolicy(int when)
{
	int lomem, lodisk, hidirty, longago;

	switch(when){
	case Pre:
		if(fsmemfree() > Mzerofree && fsdiskfree() > Dzerofree)
			return;
		qlock(&fs->policy);
		break;
	case Post:
		if(!canqlock(&fs->policy))
			return;
		break;
	}
	if(catcherror()){
		qunlock(&fs->policy);
		warn("fspolicy: %r");
		return;
	}

	lomem = fsmemfree() < Mminfree;
	lodisk = fsdiskfree() < Dminfree;
	hidirty = fsdirtypcent() > Mmaxdirtypcent;
	longago = (fstime(nsec()) - fs->wtime)/NSPERSEC > Syncival;

	/* Ideal sequence for [lomem lodisk hidirty] might be:
	 * 111:	lru sync reclaim+sync lru
	 * 110:	lru reclaim+sync
	 * 101:	lru sync lru
	 * 100:	lru
	 * 011:	reclaim+sync
	 * 010:	reclaim+sync
	 * 001:	sync
	 * 000: -
	 * Plus: if we are still low on memory, after lru, try
	 * doing a sync to move blocks to the clean list, ie. fake  "hidirty".
	 */

	if(lomem || lodisk || hidirty || longago)
		dZprint("fspolicy: lomem=%d (%ulld) lodisk=%d (%ulld)"
			" hidirty=%d (%d%%) longago=%d\n",
			lomem, fsmemfree(), lodisk, fsdiskfree(),
			hidirty, fsdirtypcent(), longago);
	if(lomem){
		fslru();
		lomem = fsmemfree() < Mminfree;
		if(lomem)
			hidirty++;
	}
	if(lodisk)
		fsreclaim();

	if(lodisk || hidirty || (longago && fs->dirty.n != 0))
		fssync();
	if(lomem && hidirty)
		fslru();

	noerror();
	qunlock(&fs->policy);
}

uvlong
fstime(uvlong t)
{
	static Lock lk;
	static uvlong last;

	lock(&lk);
	if(t)
		fs->atime = t;
	t = fs->atime;
	if(t == last)
		fs->atime = ++t;
	last = t;
	unlock(&lk);
	return t;
}
