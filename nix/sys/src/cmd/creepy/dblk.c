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
 * disk blocks, built upon memory blocks provided by mblk.c
 * see dk.h
 */

int swreaderr, swwriteerr;

static void
okaddr(daddrt addr)
{
	if((addr&Fakeaddr) == 0 && (addr < Dblksz || addr >= fs->limit))
		warnerror("bad address d%#010ullx", addr);
}

static void
okdiskaddr(daddrt addr)
{
	if((addr&Fakeaddr) != 0  || addr < Dblksz || addr >= fs->limit)
		fatal("okdiskaddr %#ullx", addr);
}

void
dbclear(u64int tag, daddrt addr)
{
	static Diskblk d;
	static QLock lk;

	dWprint("dbclear d%#ullx\n", addr);
	xqlock(&lk);
	d.tag = tag;
	if(pwrite(fs->fd, &d, sizeof d, addr) != Dblksz){
		xqunlock(&lk);
		warnerror("dbclear: d%#ullx: %r", addr);
	}
	xqunlock(&lk);
}

void
meltedref(Memblk *rb)
{
	if(canqlock(&fs->refs))
		fatal("meltedref rlk");
	if(rb->frozen){
		dWprint("melted ref dirty=%d\n", rb->dirty);
		dbwrite(rb);
		rb->frozen = 0;
	}
}

static daddrt
newblkaddr(void)
{
	daddrt addr, naddr;

	xqlock(fs);
	if(catcherror()){
		xqunlock(fs);
		error(nil);
	}
Again:
	if(fs->super == nil)
		addr = Dblksz;
	else if(fs->super->d.free != 0){
		addr = fs->super->d.free;
		okdiskaddr(addr);
		/*
		 * Caution: can't acquire new locks while holding the fs lock,
		 * but dbgetref may allocate blocks.
		 */
		xqunlock(fs);
		if(catcherror()){
			xqlock(fs);	/* restore the default in this fn. */
			error(nil);
		}
		naddr = dbgetref(addr);	/* acquires locks */
		if(naddr != 0)
			okdiskaddr(naddr);
		noerror();
		xqlock(fs);
		if(addr != fs->super->d.free){
			/* had a race */
			goto Again;
		}
		fs->super->d.free = naddr;
		fs->super->d.ndfree--;
		changed(fs->super);
	}else if(fs->super->d.eaddr < fs->limit){
		addr = fs->super->d.eaddr;
		fs->super->d.eaddr += Dblksz;
		changed(fs->super);
		/*
		 * ref blocks are allocated and initialized on demand,
		 * and they must be zeroed before used.
		 * do this holding the lock so others find everything
		 * initialized.
		 */
		if(((addr-Dblk0addr)/Dblksz)%Nblkgrpsz == 0){
			dprint("new ref blk addr = d%#ullx\n", addr);
			/* on-demand fs initialization */
			dbclear(TAG(DBref, 0, addr), addr);
			dbclear(TAG(DBref, 0, addr+Dblksz), addr+Dblksz);
			dbclear(TAG(DBref, 0, addr+2*Dblksz), addr+2*Dblksz);
			addr += 3*Dblksz;
			fs->super->d.eaddr += 3*Dblksz;
			if(fs->super->d.eaddr > fs->limit)
				sysfatal("disk is full");
		}
	}else{
		addr = 0;
		/* preserve backward compatibility with fossil */
		sysfatal("disk is full");
	}

	noerror();
	xqunlock(fs);
	okaddr(addr);
	dNprint("newblkaddr = d%#ullx\n", addr);
	return addr;
}

daddrt
addrofref(daddrt refaddr, int idx)
{
	return refaddr + idx*Dblksz;
}

daddrt
refaddr(daddrt addr, int *idx)
{
	daddrt bno, refaddr;

	addr -= Dblk0addr;
	bno = addr/Dblksz;
	*idx = bno%Nblkgrpsz;
	refaddr = Dblk0addr + bno/Nblkgrpsz * Nblkgrpsz * Dblksz;
	return refaddr;
}

/*
 * db*ref() functions update the on-disk reference counters.
 * memory blocks use Memblk.Ref instead. Beware.
 */
static daddrt
dbaddref(daddrt addr, int delta, int set, Memblk **rbp, int *ip)
{
	Memblk *rb;
	daddrt raddr, ref;
	int i;

	if(addr == 0)
		return 0;
	if(addr&Fakeaddr)	/* root and ctl files don't count */
		return 0;

	raddr = refaddr(addr, &i);
	rb = dbget(DBref, raddr);

	xqlock(&fs->refs);
	if(catcherror()){
		mbput(rb);
		xqunlock(&fs->refs);
		debug();
		error(nil);
	}
	if(delta != 0 || set != 0){
		if(delta != 0){
			if(rb->d.ref[i] >= Dblksz)
				fatal("dbaddref: d%#010ullx in free list", rb->d.ref[i]);
			if(rb->d.ref[i] == 0)
				fatal("dbaddref: d%#010ullx double free", rb->d.ref[i]);
		}
		meltedref(rb);
		if(set)
			rb->d.ref[i] = set;
		else
			rb->d.ref[i] += delta;
		rb->dirty = 1;
	}
	ref = rb->d.ref[i];
	noerror();
	xqunlock(&fs->refs);
	if(rbp == nil)
		mbput(rb);
	else
		*rbp = rb;
	if(ip != nil)
		*ip = i;
	return ref;
}

daddrt
dbgetref(daddrt addr)
{
	if(fs->worm)
		return 6ULL;
	return dbaddref(addr, 0, 0, nil, nil);
}

void
dbsetref(daddrt addr, int ref)
{
	daddrt n;

	if(fs->worm)
		return;
	n = dbaddref(addr, 0, ref, nil, nil);
	dNprint("dbsetref %#010ullx -> %ulld\tpc %#p\n", addr, n, getcallerpc(&addr));
}

daddrt
dbincref(daddrt addr)
{
	daddrt n;

	if(fs->worm)
		return 6ULL;
	n = dbaddref(addr, +1, 0, nil, nil);
	dNprint("dbincref %#010ullx -> %ulld\tpc %#p\n", addr, n, getcallerpc(&addr));
	return n;
}

static daddrt
dbdecref(daddrt addr, Memblk **rb, int *idx)
{
	daddrt n;

	if(fs->worm)
		return 6ULL;
	n = dbaddref(addr, -1, 0, rb, idx);
	dNprint("dbdecref %#010ullx -> %ulld\tpc %#p\n", addr, n, getcallerpc(&addr));
	return n;
}

static void
nodoublefree(daddrt addr)
{
	daddrt a;

	if(addr == 0)
		return;
	for(a = fs->super->d.free; a != 0; a = dbgetref(a))
		if(a == addr)
			fatal("double free for addr d%#ullx", addr);
}

static long xdbput(Memblk *b, int type, daddrt addr, int isdir);

static long
dropdentries(void *p, int n)
{
	int i;
	daddrt *d;
	long tot;

	tot = 0;
	d = p;
	for(i = 0; i < n; i++)
		if(d[i] != 0)
			tot += xdbput(nil, DBfile, d[i], 0);
	return tot;
}

/*
 * Drop a on-disk reference.
 * When no references are left, the block is unlinked from the hash
 * (and its hash ref released), and disk references to blocks pointed to by
 * this blocks are also decremented (and perhaps such blocks released).
 *
 * More complex than needed, because we don't want to read a data block
 * just to release a reference to it, unless it's a data block for a directory.
 *
 * b may be nil if type and addr are given, for recursive calls.
 */

static long
xdbput(Memblk *b, int type, daddrt addr, int isdir)
{
	daddrt ref;
	Memblk *mb, *rb;
	int i, idx, sz;
	uvlong doff;
	long tot;

	if(b == nil && addr == 0)
		return 0;
	if(fs->worm)
		return 1;
	okdiskaddr(addr);
	ref = dbgetref(addr);
	if(ref > 2*Dblksz)
		fatal("dbput: d%#010ullx: double free", addr);

	ref = dbdecref(addr, &rb, &idx);
	if(ref != 0){
		dKprint("dbput: d%#010ullx dr %#ullx type %s\n",
			addr, ref, tname(type));
		mbput(rb);
		return 0;
	}
	/*
	 * Gone from disk, be sure it's also gone from memory.
	 */
	if(catcherror()){
		mbput(rb);
		error(nil);
	}
	mb = b;
	if(mb == nil){
		if(isdir || type != DBdata)
			mb = dbget(type, addr);
		else
			mb = mbget(type, addr, Dontmk);
	}

	dKprint("dbput: free: %H\n", mb);
	tot = 1;
	if(mb != nil){
		isdir |= DBDIR(mb);
		assert(type == mb->type && addr == mb->addr && mb->ref > 1);
		mbunhash(mb, 0);
	}
	if(catcherror()){
		if(mb != b)
			mbput(mb);
		error(nil);
	}
	switch(type){
	case DBsuper:
	case DBref:
		fatal("dbput: super or ref");
	case DBdata:
		if(isdir)
			tot += dropdentries(mb->d.data, Dblkdatasz/Daddrsz);
		break;
	case DBattr:
		break;
	case DBfile:
		if(isdir)
			assert(mb->d.mode&DMDIR);
		else
			assert((mb->d.mode&DMDIR) == 0);
		tot += xdbput(nil, DBattr, mb->d.aptr, 0);
		for(i = 0; i < nelem(mb->d.dptr); i++){
			tot += xdbput(nil, DBdata, mb->d.dptr[i], isdir);
			mb->d.dptr[i] = 0;
		}
		for(i = 0; i < nelem(mb->d.iptr); i++){
			tot += xdbput(nil, DBptr0+i, mb->d.iptr[i], isdir);
			mb->d.iptr[i] = 0;
		}
		if(isdir){
			doff = embedattrsz(mb);
			sz = Embedsz-doff;
			tot += dropdentries(mb->d.embed+doff, sz/Daddrsz);
		}
		break;
	default:
		if(type < DBptr0 || type >= DBptr0+Niptr)
			fatal("dbput: type %d", type);
		for(i = 0; i < Dptrperblk; i++){
			tot += xdbput(nil, mb->type-1, mb->d.ptr[i], isdir);
			mb->d.ptr[i] = 0;
		}
	}
	noerror();

	if(mb != b)
		mbput(mb);

	if(dbg['d'])
		assert(mbget(type, addr, Dontmk) == nil);

	if(dbg['K'])
		nodoublefree(addr);
	xqlock(fs);
	xqlock(&fs->refs);
	rb->d.ref[idx] = fs->super->d.free;
	fs->super->d.free = addr;
	fs->super->d.ndfree++;
	xqunlock(&fs->refs);
	xqunlock(fs);
	noerror();
	mbput(rb);

	return tot;
}

long
dbput(Memblk *b)
{
	if(b == nil)
		return 0;
	return xdbput(b, b->type, b->addr, DBDIR(b));
}

static daddrt
newfakeaddr(void)
{
	static daddrt addr = ~0;
	daddrt n;

	xqlock(fs);
	addr -= Dblksz;
	n = addr;
	xqunlock(fs);
	return n|Fakeaddr;
}

Memblk*
dballocz(uint type, int dbit, int zeroit)
{
	Memblk *b;
	daddrt addr;
	int ctl;

	ctl = type == DBctl;
	if(ctl){
		type = DBfile;
		addr = newfakeaddr();
	}else
		addr = newblkaddr();
	b = mballocz(addr, zeroit);
	b->d.tag = TAG(type, dbit, b->addr);
	b->type = type;
	if(catcherror()){
		mbput(b);
		debug();
		error(nil);
	}
	if((addr&Fakeaddr) == 0 && addr >= Dblk0addr)
		dbsetref(addr, 1);
	if(type == DBfile)
		b->mf = anew(&mfalloc);
	b = mbhash(b);
	changed(b);
	noerror();
	dNprint("dballoc %s -> %H\n", tname(type), b);
	return b;
}

/*
 * BUG: these should ensure that all integers are converted between
 * little endian (disk format) and the machine endianness.
 * We know the format of all blocks and the type of all file
 * attributes. Those are the integers to convert to fix the bug.
 */
static Memblk*
hosttodisk(Memblk *b)
{
	if(catcherror())
		fatal("hosttodisk: bad tag");
	checktag(b->d.tag, b->type, b->addr);
	noerror();
	incref(b);
	return b;
}

static void
disktohost(Memblk *b)
{
	static union
	{
		u64int i;
		uchar m[BIT64SZ];
	} u;

	u.i = 0x1122334455667788ULL;
	if(u.m[0] != 0x88)
		fatal("fix hosttodisk/disktohost for big endian");
	checkblk(b);
}

static int
isfakeref(daddrt addr)
{
	addr -= Dblk0addr;
	addr /= Dblksz;
	return (addr%Nblkgrpsz) == 2;
}

/*
 * Write the block a b->addr.
 * DBrefs are written at even (b->addr) or odd (b->addr+DBlksz)
 * reference blocks as indicated by the frozen super block to be written.
 */
long
dbwrite(Memblk *b)
{
	Memblk *nb;
	static int nw;
	daddrt addr;

	if(b->addr&Fakeaddr)
		fatal("dbwrite: fake addr %H", b);
	if(b->dirty == 0)
		return 0;
	addr = b->addr;
	/*
	 * super switches between even/odd DBref blocks, plus there's a
	 * fake DBref block used just for fscheck() counters.
	 */
	if(b->type == DBref){
		assert(fs->fzsuper != nil);
		if(fs->fzsuper->d.oddrefs && !isfakeref(b->addr))
			addr += Dblksz;
	}
	dWprint("dbwriting at d%#010ullx %H\n",addr, b);
	nb = hosttodisk(b);
	if(swwriteerr != 0 && ++nw > swwriteerr){
		written(b);	/* what can we do? */
		mbput(nb);
		warnerror("dbwrite: sw fault");
	}
	if(pwrite(fs->fd, &nb->d, sizeof nb->d, addr) != Dblksz){
		written(b);	/* what can we do? */
		mbput(nb);
		warnerror("dbwrite: d%#ullx: %r", b->addr);
	}
	written(b);
	mbput(nb);

	return Dblksz;
}

long
dbread(Memblk *b)
{
	static int nr;
	long tot, n;
	uchar *p;
	daddrt addr;

	if(b->addr&Fakeaddr)
		fatal("dbread: fake addr %H", b);
	p = b->d.ddata;
	addr = b->addr;
	/*
	 * super switches between even/odd DBref blocks, plus there's a
	 * fake DBref block used just for fscheck() counters.
	 */
	if(b->type == DBref && fs->super->d.oddrefs && !isfakeref(b->addr))
		addr += Dblksz;
	for(tot = 0; tot < Dblksz; tot += n){
		if(swreaderr != 0 && ++nr > swreaderr)
			warnerror("dbread: sw fault");
		n = pread(fs->fd, p+tot, Dblksz-tot, addr + tot);
		if(n == 0)
			werrstr("eof on disk file");
		if(n <= 0)
			warnerror("dbread: d%#ullx: %r", b->addr);
	}
	assert(tot == sizeof b->d && tot == Dblksz);

	dRprint("dbread from d%#010ullx tag %#ullx %H\n", addr, b->d.tag, b);
	disktohost(b);
	if(b->type != DBref)
		b->frozen = 1;

	return tot;
}

Memblk*
dbget(uint type, daddrt addr)
{
	Memblk *b;

	dMprint("dbget %s d%#010ullx\n", tname(type), addr);
	okaddr(addr);
	b = mbget(type, addr, Mkit);
	if(b == nil)
		error("i/o error");
	if(b->loading == 0)
		return b;

	/* the file is new, must read it */
	if(catcherror()){
		xqunlock(&b->newlk);	/* awake those waiting for it */
		mbunhash(b, 0);		/* put our ref and the hash ref */
		mbput(b);
		error(nil);
	}
	dbread(b);
	checktag(b->d.tag, type, addr);
	assert(b->type == type);
	if(type == DBfile){
		assert(b->mf == nil);
		b->mf = anew(&mfalloc);
		gmeta(b, b->d.embed, Embedsz);
		if(b->d.mode&DMDIR)
			assert(DBDIR(b));
		else
			assert(!DBDIR(b));
	}
	b->loading = 0;
	noerror();
	xqunlock(&b->newlk);
	return b;
}

static void
dupdentries(void *p, int n)
{
	int i;
	daddrt *d;

	d = p;
	for(i = 0; i < n; i++)
		if(d[i] != 0){
			dNprint("add ref on dup d%#ullx\n", d[i]);
			dbincref(d[i]);
		}
}

/*
 * caller responsible for locking.
 * On errors we may leak disk blocks because of added references.
 * Isdir flags that the block belongs to a dir, so we could add references
 * to dir entries.
 */
Memblk*
dbdup(Memblk *b)
{
	Memblk *nb;
	int i;
	ulong doff, sz;

	nb = dballocz(b->type, DBDIR(b), 0);
	if(catcherror()){
		mbput(nb);
		error(nil);
	}
	switch(b->type){
	case DBfree:
	case DBref:
	case DBsuper:
	case DBattr:
		fatal("dbdup: %s", tname(b->type));
	case DBdata:
		memmove(nb->d.data, b->d.data, Dblkdatasz);
		if(DBDIR(b) != 0)
			dupdentries(b->d.data, Dblkdatasz/Daddrsz);
		break;
	case DBfile:
		if(!b->frozen)
			isrwlocked(b, Rd);
		nb->d.asize = b->d.asize;
		nb->d.aptr = b->d.aptr;
		nb->d.ndents = b->d.ndents;
		if(nb->d.aptr != 0)
			dbincref(b->d.aptr);
		for(i = 0; i < nelem(b->d.dptr); i++){
			nb->d.dptr[i] = b->d.dptr[i];
			if(nb->d.dptr[i] != 0)
				dbincref(b->d.dptr[i]);
		}
		for(i = 0; i < nelem(b->d.iptr); i++){
			nb->d.iptr[i] = b->d.iptr[i];
			if(nb->d.iptr[i] != 0)
				dbincref(b->d.iptr[i]);
		}
		nb->d.Dmeta = b->d.Dmeta;
		memmove(nb->d.embed, b->d.embed, Embedsz);
		gmeta(nb, nb->d.embed, Embedsz);
		if(DBDIR(b) != 0){
			doff = embedattrsz(nb);
			sz = Embedsz-doff;
			dupdentries(nb->d.embed+doff, sz/Daddrsz);
		}
		/*
		 * no race: caller takes care.
		 */
		if(b->frozen && b->mf->melted == nil){
			incref(nb);
			b->mf->melted = nb;
		}
		break;
	default:
		if(b->type < DBptr0 || b->type >= DBptr0 + Niptr)
			fatal("dbdup: bad type %d", b->type);
		for(i = 0; i < Dptrperblk; i++){
			nb->d.ptr[i] = b->d.ptr[i];
			if(nb->d.ptr[i] != 0)
				dbincref(nb->d.ptr[i]);
		}
	}
	changed(nb);
	noerror();

	/* when b is a frozen block, it's likely we won't use it more,
	 * because we now have a melted one.
	 * pretend it's the lru one.
	 */
	if(b->frozen)
		mbunused(b);

	return nb;
}

