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
 * fs checks
 */
void
checktag(u64int tag, uint type, daddrt addr)
{
		
	if((tag|DFdir) != TAG(type, DFdir, addr)){
		if(type == DBref)
			if((tag|DFdir) == TAG(type, DFdir, addr+Dblksz))
				return;	/* odd refs */
		warn("bad tag: %#ullx  %#ullx\n",
			tag, TAG(type, 0, addr));
		error("bad tag");
	}
}

static int
validaddr(daddrt addr)
{
	if(addr&Fakeaddr)
		return 0;
	if(addr == 0)
		return 1;
	return addr >= Dblk0addr && addr < fs->super->d.eaddr;
}

void
checkblk(Memblk *b)
{
	int i;
	daddrt eaddr, *de;
	long doff, sz;

	checktag(b->d.tag, b->type, b->addr);
	switch(b->type){
	case DBfree:
		warnerror("free block on disk");
		break;
	case DBref:
		eaddr = fs->super->d.eaddr;
		for(i = 0; i < Drefperblk; i++)
			if(b->d.ref[i] >= eaddr)
				warnerror("ref out of range");
		break;
	case DBsuper:
		if(b->d.magic != MAGIC)
			warnerror("super: magic");
		if(b->d.eaddr >= fs->limit || b->d.eaddr < Dblk0addr)
			warnerror("super: eaddr out of range");
		if(b->d.free >= b->d.eaddr || (b->d.free && b->d.free < Dblk0addr))
			warnerror("super: free out of range");
		if(b->d.root >= b->d.eaddr || b->d.root < Dblk0addr)
			warnerror("super: root out of range");
		break;
	case DBattr:
		if(!validaddr(b->d.next))
			warnerror("attr: next out of range");
		break;
	case DBdata:
		if(DBDIR(b) == 0)
			break;
		for(i = 0; i < Dblkdatasz/Daddrsz; i++)
			if(!validaddr(b->d.ptr[i]))
				warnerror("dentry out of range");
		break;
	case DBfile:
		if(!validaddr(b->d.aptr))
			warnerror("file: attr out of range");
		for(i = 0; i < nelem(b->d.dptr); i++)
			if(!validaddr(b->d.dptr[i]))
				warnerror("file: dptr out of range");
		for(i = 0; i < nelem(b->d.iptr); i++)
			if(!validaddr(b->d.iptr[i]))
				warnerror("file: iptr out of range");
		if(DBDIR(b) != 0){
			doff = embedattrsz(b);
			if(doff > Embedsz)
				warnerror("file: wrong attr size");
			sz = Embedsz-doff;
			de = (daddrt*)(b->d.embed+doff);
			for(i = 0; i < sz/Daddrsz; i++)
				if(!validaddr(de[i]))
					warnerror("file: dentry out of range");
		}
		break;
	default:
		if(b->type < DBptr0 || b->type >= DBptr0 + Niptr)
			warnerror("unknown block type");
		for(i = 0; i < Dptrperblk; i++)
			if(!validaddr(b->d.ptr[i]))
				warnerror("ptr: address out of range");
	}
}

static uvlong
clearrefs(int disktoo)
{
	Memblk *b;
	daddrt addr, eaddr;
	uvlong nhash;
	int i;

	nhash = 0;
	for(i = 0; i < nelem(fs->fhash); i++)
		for(b = fs->fhash[i].b; b != nil; b = b->next){
			nhash++;
			b->d.cnt = 0;
		}
	if(disktoo == 0)
		return nhash;

	eaddr = fs->super->d.eaddr;
	for(addr = Dblk0addr+2*Dblksz; addr < eaddr; addr += Dblksz*Nblkgrpsz){
		if(catcherror()){
			warn("clearrefs: %r");
			return 1;
		}
		b = dbget(DBref, addr);
		memset(b->d.data, 0, Dblkdatasz);
		mbput(b);
		noerror();
	}
	return nhash;
}

static int
mbcounted(Memblk *b)
{
	if(b == nil)
		return 0;
	if(b < fs->blk || b >= fs->blk + fs->nablk)
		fatal("mbcountref: m%#p not in global array", b);
	if(b->ref != b->d.cnt){
		warn("check: m%#p: found %ulld != ref %ud\n%H",
			b, b->d.cnt, b->ref, b);
		return 1;
	}
	return 0;
}

daddrt
dbcounted(daddrt addr)
{
	Memblk *rb;
	daddrt n, raddr;
	int i;

	raddr = refaddr(addr, &i) + 2*Dblksz;
	if(catcherror()){
		warn("dbcounted: %r");
		return 1;
	}
	rb = dbget(DBref, raddr);
	noerror();
	n = rb->d.ref[i];
	mbput(rb);
	return n;
}

daddrt
mbcountref(Memblk *b)
{
	daddrt old;

	if(b == nil)
		return 0;
	if(b < fs->blk || b >= fs->blk + fs->nablk)
		fatal("mbcountref: m%#p not in global array", b);
	old = b->d.cnt++;
	if(old == 0 && b->type == DBfile)
		mbcountref(b->mf->melted);
	return old;
}

u64int
dbcountref(daddrt addr)
{
	Memblk *rb;
	daddrt n, raddr;
	int i;

	raddr = refaddr(addr, &i) + 2*Dblksz;
	if(catcherror()){
		warn("dbcountref: %r");
		return 1;
	}
	rb = dbget(DBref, raddr);
	noerror();
	n = rb->d.ref[i]++;
	mbput(rb);
	return n;
}

static int
bcountrefs(Memblk *b, void*)
{
	if(dbcountref(b->addr) != 0)	/* already counted; prune */
		return -1;
	return 0;
}

static int
dbcountfree(daddrt addr, int oktohash)
{
	Memblk *rb;
	daddrt n, raddr;
	int i;

	if(!oktohash && mbhashed(addr)){
		warn("check: d%#010ullx: free block in use", addr);
		return 1;
	}
	raddr = refaddr(addr, &i) + 2*Dblksz;
	if(catcherror()){
		warn("dbcountref: %r");
		return 1;
	}
	rb = dbget(DBref, raddr);
	noerror();
	n = rb->d.ref[i];
	if(n != 0){
		warn("check: d%#010ullx: double free", addr);
		mbput(rb);
		return 1;
	}
	rb->d.ref[i] = ~0;
	mbput(rb);
	return 0;
}

static long dfcountrefs(Memblk*);

static int
fcountref(Memblk *, daddrt *de, void *a)
{
	Memblk *b;
	long *nfails;

	nfails = a;
	if(*de == 0)
		return 0;

	if(catcherror()){
		warn("check: d%#010ullx %r", *de);
		(*nfails)++;
	}else{
		b = dbget(DBfile, *de);
		(*nfails) += dfcountrefs(b);
		noerror();
		mbput(b);
	}
	return 0;
}

static long
dfcountrefs(Memblk *f)
{
	int i;
	long nfails;

	nfails = 0;
	isfile(f);
	if((f->addr&Fakeaddr) == 0 && f->addr >= fs->limit){
		warn("check: '%s' d%#010ullx: out of range", f->mf->name, f->addr);
		return 1;
	}
	if((f->addr&Fakeaddr) == 0)
		if(dbcountref(f->addr) != 0)		/* already visited */
			return 0;		/* skip children */
	rwlock(f, Rd);
	if(catcherror()){
		warn("check: '%s' d%#010ullx: data: %r", f->mf->name, f->addr);
		rwunlock(f, Rd);
		return 1;
	}
	for(i = 0; i < nelem(f->d.dptr); i++)
		ptrmap(f->d.dptr[i], 0, bcountrefs, nil, Disk);
	for(i = 0; i < nelem(f->d.iptr); i++)
		ptrmap(f->d.iptr[i], i+1, bcountrefs, nil, Disk);
	if(DBDIR(f))
		dfdirmap(f, fcountref, &nfails, Rd);
	noerror();
	rwunlock(f, Rd);
	return nfails;
}

static long
fscheckrefs(void)
{
	long nfails;
	int i;
	Memblk *b;

	dprint("mblk refs...\n");
	clearrefs(Mem);
	mbcountref(fs->super);
	mbcountref(fs->root);
	mbcountref(fs->active);
	mbcountref(fs->archive);
	mbcountref(fs->cons);
	mbcountref(fs->stats);
	mbcountref(fs->fzsuper);
	countfidrefs();
	for(i = 0; i < nelem(fs->fhash); i++)
		for(b = fs->fhash[i].b; b != nil; b = b->next)
			mbcountref(b);

	nfails = 0;
	for(i = 0; i < nelem(fs->fhash); i++)
		for(b = fs->fhash[i].b; b != nil; b = b->next)
			nfails += mbcounted(b);
	nfails += mbcounted(fs->super);
	nfails += mbcounted(fs->root);
	nfails += mbcounted(fs->active);
	nfails += mbcounted(fs->archive);
	nfails += mbcounted(fs->cons);
	nfails += mbcounted(fs->stats);
	nfails += mbcounted(fs->fzsuper);

	if(nfails > 0 && dbg['D']){
		dprint("fscheckrefs: %ld fails. sleeping\n", nfails);
		fsdump(1, 0);
		while(1)sleep(5000);
	}
	return nfails;
}

static void
dfcountfree(void)
{
	daddrt addr;

	dprint("list...\n");
	addr = fs->super->d.free;
	while(addr != 0){
		if(addr < Dblksz){
			warn("check: d%#010ullx in free list", addr);
			break;
		}
		if(addr >fs->limit){
			warn("check: d%#010ullx: free overflow", addr);
			break;
		}
		dbcountfree(addr, 0);
		addr = dbgetref(addr);
	}
	/* DBref blocks */
	dprint("refs...\n");
	for(addr = Dblk0addr; addr < fs->super->d.eaddr; addr += Dblksz*Nblkgrpsz){
		dbcountfree(addr, 1);		/* even DBref */
		dbcountfree(addr+Dblksz, 1);	/* odd DBref */
		dbcountfree(addr+2*Dblksz, 1);	/* check DBref */
	}
}

static uvlong
mleaks(void)
{
	uvlong nblk, nfails, n;
	Memblk *p;

	dprint("mblk leaks...\n");
	nfails = 0;
	if(fs->nblk != fs->nmused + fs->nmfree){
		warn("block leaks: %ulld blks != %ulld used + %ulld free",
			fs->nblk, fs->nmused, fs->nmfree);
		nfails++;
	}
	nblk = fs->clean.n + fs->dirty.n + fs->refs.n;
	nblk++; /* super */
	nblk++; /* cons */
	nblk++; /* root */
	nblk++; /* stats */
	if(nblk != fs->nmused){
		warn("check: %ulld blocks linked != %ulld blocks used",
			nblk, fs->nmused);
		fs->super->unlinkpc = 0;
		fs->root->unlinkpc = 0;
		fs->cons->unlinkpc = 0;
		fs->stats->unlinkpc = 0;
		for(p = fs->free; p != nil; p = p->next)
			p->unlinkpc = 0;
		for(p = fs->dirty.hd; p != nil; p = p->lnext)
			p->unlinkpc = 0;
		for(p = fs->clean.hd; p != nil; p = p->lnext)
			p->unlinkpc = 0;
		for(p = fs->refs.hd; p != nil; p = p->lnext)
			p->unlinkpc = 0;
		for(n = 0; n < fs->nblk; n++)
			if(fs->blk[n].unlinkpc != 0){
				warn("check: block unlinked at %#p:\n%H",
					fs->blk[n].unlinkpc, &fs->blk[n]);
				nfails++;
			}
	}
	return nfails;
}

static uvlong
dleaks(void)
{
	daddrt n, addr, c;
	long nfails;

	dprint("dblk leaks...\n");
	clearrefs(Disk);
	nfails = dfcountrefs(fs->root);
	dfcountfree();

	for(addr = Dblk0addr; addr < fs->super->d.eaddr; addr += Dblksz){
		c = dbcounted(addr);
		if(c == 0){
			warn("check: d%#010ullx: leak", addr);
			nfails++;
			continue;
		}
		if(addr < Dblk0addr || c == ~0)
			continue;
		n = dbgetref(addr);
		if(n != c){
			warn("check: d%#010ullx: found %ulld != ref %ulld",
				addr, c, n);
			nfails++;
		}
	}
	return nfails;
}
/*
 * Failed checks are reported but not fixed (but for leaked blocks).
 * The user is expected to format the partition and restore contents from venti.
 * We might easily remove the dir entries for corrupt files, and restore
 */
int
fscheck(void)
{
	long nfails;

	xqlock(&fs->fzlk);
	xrwlock(&fs->quiescence, Wr);
	nfails = 0;
	if(catcherror()){
		xrwunlock(&fs->quiescence, Wr);
		xqunlock(&fs->fzlk);
		warn("check: %r");
		nfails++;
		return nfails;
	}

	warn("check...");

	nfails += mleaks();
	nfails += fscheckrefs();
	if(nfails == 0)
		nfails += dleaks();

	xrwunlock(&fs->quiescence, Wr);
	xqunlock(&fs->fzlk);
	noerror();
	if(nfails > 0 && dbg['D']){
		dprint("fscheck: %ld fails. sleeping\n", nfails);
		fsdump(0, 1);
		while(1)sleep(5000);
	}
	if(nfails)
		warn("check fails");
	else
		warn("check passes");
	return nfails;
}

