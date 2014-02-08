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
 * memory blocks.
 * see dk.h
 */

/*
 * For simplicity, functions in mblk.c do not raise errors.
 * (debug dump functions may be an exception).
 */

Alloc mfalloc =
{
	.elsz = sizeof(Mfile),
	.zeroing = 1,
};

char*
tname(int t)
{
	static char*nms[] = {
	[DBfree]	"DBfree",
	[DBsuper]	"DBsuper",
	[DBref]		"DBref",
	[DBdata]	"DBdata",
	[DBattr]	"DBattr",
	[DBfile]		"DBfile",
	[DBptr0]	"DBptr0",
	[DBptr0+1]	"DBptr1",
	[DBptr0+2]	"DBptr2",
	[DBptr0+3]	"DBptr3",
	[DBptr0+4]	"DBptr4",
	[DBptr0+5]	"DBptr5",
	[DBptr0+6]	"DBptr6",
	};

	if(t < 0 || t >= nelem(nms))
		return "BADTYPE";
	return nms[t];
}

int fullfiledumps = 0;

/*
 * NO LOCKS. debug only
 */
static void
fmttab(Fmt *fmt, int t, int c)
{
	while(t-- > 0)
		fmtprint(fmt, "%c   ", c?'.':' ');
}
int mbtab;
static void
fmtptr(Fmt *fmt, int type, daddrt addr, char *tag, int n)
{
	Memblk *b;

	if(addr == 0)
		return;
	b = mbget(type, addr, Dontmk);
	if(b == nil){
		fmttab(fmt, mbtab, 0);
		fmtprint(fmt, "%s[%d] = d%#010ullx <unloaded>\n", tag, n, addr);
	}else{
		fmtprint(fmt, "%H", b);
		mbput(b);
	}
}
static void
dumpdirdata(Fmt *fmt, Memblk *b)
{
	long doff;
	daddrt *p;
	int i;

	if(b->d.length == 0 || DBDIR(b) == 0)
		return;
	doff = embedattrsz(b);
	if(doff < Embedsz){
		fmttab(fmt, mbtab, 0);
		p = (daddrt*)(b->d.embed+doff);
		for(i = 0; i < 5 && (uchar*)p < b->d.embed+Embedsz - Daddrsz; i++)
			fmtprint(fmt, "%sd%#010ullx", i?" ":"data: ", EP(*p++));
		fmtprint(fmt, "\n");
	}
}

int
mbfmt(Fmt *fmt)
{
	Memblk *b;
	int i, n;

	b = va_arg(fmt->args, Memblk*);
	if(b == nil)
		return fmtprint(fmt, "<nil>\n");
	nodebug();
	fmttab(fmt, mbtab, b->type == DBfile);

	fmtprint(fmt, "%s", tname(b->type));
	if(b->type == DBfile && b->mf != nil)
		fmtprint(fmt, " '%s'", b->mf->name);
	if(b->frozen)
		fmtprint(fmt, " FZ");
	if(b->dirty)
		fmtprint(fmt, " DT");
	if(DBDIR(b))
		fmtprint(fmt, " DIR");
	fmtprint(fmt, " m%#p d%#010ullx", b, EP(b->addr));
	fmtprint(fmt, " r=%d", b->ref);
	switch(b->type){
	case DBfree:
		fmtprint(fmt, "\n");
		break;
	case DBdata:
	case DBattr:
		fmtprint(fmt, " dr=%ulld\n", dbgetref(b->addr));
		break;
	case DBref:
		fmtprint(fmt, " next m%#p", b->lnext);
		for(i = n = 0; i < Drefperblk; i++)
			if(b->d.ref[i]){
				if(n++%3 == 0){
					fmtprint(fmt, "\n");
					fmttab(fmt, mbtab, 0);
				}
				fmtprint(fmt, "  ");
				fmtprint(fmt, "[%02d]d%#010ullx=%#ullx",
					i, addrofref(b->addr, i), b->d.ref[i]);
			}
		if(n == 0 || --n%4 != 0)
			fmtprint(fmt, "\n");
		break;
	case DBfile:
		fmtprint(fmt, " dr=%ulld", dbgetref(b->addr));
		if(b->mf == nil){
			fmtprint(fmt, "  no mfile\n");
			break;
		}
		fmtprint(fmt, "  nr%d nw%d\n", b->mf->readers, b->mf->writer);
		if(0)
			fmtprint(fmt, "  asz %#ullx aptr %#ullx",
				b->d.asize, b->d.aptr);
		fmttab(fmt, mbtab, 0);
		fmtprint(fmt, "    %M '%s' len %ulld ndents %ulld melted m%#p\n",
			(ulong)b->d.mode, usrname(b->d.uid),
			b->d.length, b->d.ndents, b->mf->melted);
		if(0){
			fmttab(fmt, mbtab, 0);
			fmtprint(fmt, "  id %#ullx mode %M mt %#ullx"
				" '%s'\n",
				EP(b->d.id), (ulong)b->d.mode,
				EP(b->d.mtime), b->mf->uid);
		}
		mbtab++;
		if(DBDIR(b))
			dumpdirdata(fmt, b);
		for(i = 0; i < nelem(b->d.dptr); i++)
			fmtptr(fmt, DBdata, b->d.dptr[i], "d", i);
		for(i = 0; i < nelem(b->d.iptr); i++)
			fmtptr(fmt, DBptr0+i, b->d.iptr[i], "i", i);
		mbtab--;
		break;
	case DBsuper:
		fmtprint(fmt, "\n");
		fmttab(fmt, mbtab, 0);
		fmtprint(fmt, "  free d%#010ullx eaddr d%#010ullx root d%#010ullx %s refs\n",
			b->d.free, b->d.eaddr, b->d.root,
			b->d.oddrefs?"odd":"even");
		break;
	default:
		if(b->type < DBptr0 || b->type >= DBptr0+Niptr){
			fmtprint(fmt, "<bad type %d>", b->type);
			break;
		}
		fmtprint(fmt, " dr=%ulld\n", dbgetref(b->addr));
		mbtab++;
		if(fullfiledumps)
			for(i = 0; i < Dptrperblk; i++)
				fmtptr(fmt, b->type-1, b->d.ptr[i], "p", i);
		mbtab--;
		break;
	}
	debug();
	return 0;
}

/*
 * Blocks are kept in a hash while loaded, to look them up.
 * When in the hash, they fall into exactly one of this cases:
 *	- a super block or a fake mem block (e.g., cons, /), unlinked
 *	- a ref block, linked in the fs->refs list
 *	- a clean block, linked in the fs clean list
 *	- a dirty block, linked in the fs dirty list.
 *
 * The policy function (eg fslru) keeps the lock on the list while
 * releasing blocks from the hash. This implies locking in the wrong order.
 * The "ispolicy" argument in some functions here indicates that the
 * call is from the policy function.
 */

void
ismelted(Memblk *b)
{
	if(b->frozen)
		fatal("frozen at pc %#p", getcallerpc(&b));
}

void
munlink(List *l, Memblk *b, int ispolicy)
{
	if(!ispolicy)
		xqlock(l);
	if(b->lprev != nil)
		b->lprev->lnext = b->lnext;
	else
		l->hd = b->lnext;
	if(b->lnext != nil)
		b->lnext->lprev = b->lprev;
	else
		l->tl = b->lprev;
	b->lnext = nil;
	b->lprev = nil;
	l->n--;
	if(!ispolicy)
		xqunlock(l);
	b->unlinkpc = getcallerpc(&l);
}

static void
mlink(List *l, Memblk *b)
{
	assert(b->lnext == nil && b->lprev == nil);
	xqlock(l);
	b->lnext = l->hd;
	if(l->hd != nil)
		l->hd->lprev = b;
	else
		l->tl = b;
	l->hd = b;
	l->n++;
	xqunlock(l);
}

static void
mlinklast(List *l, Memblk *b)
{
	xqlock(l);
	b->lprev = l->tl;
	if(l->tl != nil)
		l->tl->lnext = b;
	else
		l->hd = b;
	l->tl = b;
	l->n++;
	xqunlock(l);
}

List
mfilter(List *bl, int(*f)(Memblk*))
{
	Memblk *b, *bnext;
	List wl;

	memset(&wl, 0, sizeof wl);
	xqlock(bl);
	for(b = bl->hd; b != nil; b = bnext){
		bnext = b->lnext;
		if(f(b)){
			munlink(bl, b, 1);
			mlinklast(&wl, b);
		}
	}
	xqunlock(bl);
	return wl;
}

void
mlistdump(char *tag, List *l)
{
	Memblk *b;
	int i;

	fprint(2, "%s:", tag);
	i = 0;
	for(b = l->hd; b != nil; b = b->lnext){
		if(i++ % 5 == 0)
			fprint(2, "\n\t");
		fprint(2, "d%#010ullx ", EP(b->addr));
	}
	fprint(2, "\n");
}

static void
mbused(Memblk *b)
{
	if(b->dirty != 0 || (b->addr&Fakeaddr) != 0)
		return;
	switch(b->type){
	case DBref:
	case DBsuper:
		break;
	default:
		munlink(&fs->clean, b, 0);
		mlink(&fs->clean, b);
	}
}

void
mbunused(Memblk *b)
{
	if(b->dirty || (b->addr&Fakeaddr) != 0)		/* not on the clean list */
		return;
	if(b->type == DBsuper || b->type == DBref)	/* idem */
		return;
	munlink(&fs->clean, b, 0);
	mlinklast(&fs->clean, b);
}

void
changed(Memblk *b)
{
	if(b->type != DBsuper)
		ismelted(b);
	if(b->dirty || (b->addr&Fakeaddr) != 0)
		return;
	lock(&b->dirtylk);
	if(b->dirty){
		unlock(&b->dirtylk);
		return;
	}
	switch(b->type){
	case DBsuper:
	case DBref:
		b->dirty = 1;
		break;
	default:
		assert(b->dirty == 0);
		munlink(&fs->clean, b, 0);
		b->dirty = 1;
		mlink(&fs->dirty, b);
	}
	unlock(&b->dirtylk);
}

void
written(Memblk *b)
{
	lock(&b->dirtylk);
	assert(b->dirty != 0);
	switch(b->type){
	case DBsuper:
	case DBref:
		b->dirty = 0;
		unlock(&b->dirtylk);
		break;
	default:
		/*
		 * data blocks are removed from the dirty list,
		 * then written. They are not on the list while
		 * being written.
		 */
		assert(b->lprev == nil && b->lnext == nil);
		b->dirty = 0;
		unlock(&b->dirtylk);

		/*
		 * heuristic: frozen files that have a melted version
		 * are usually no longer useful.
		 */
		if(b->type == DBfile && b->mf->melted != nil)
			mlinklast(&fs->clean, b);
		else
			mlink(&fs->clean, b);
	}
}

static void
linkblock(Memblk *b)
{
	if((b->addr&Fakeaddr) != 0 || b->type == DBsuper)
		return;
	if(b->type == DBref)
		mlink(&fs->refs, b);
	else{
		assert(b->dirty == 0);
		mlink(&fs->clean, b);
	}
}

static void
unlinkblock(Memblk *b, int ispolicy)
{
	if((b->addr&Fakeaddr) != 0)
		return;
	switch(b->type){
	case DBref:
		fatal("unlinkblock: DBref");
	case DBsuper:
		fatal("unlinkblock: DBsuper");
	}

	if(b->dirty){
		assert(!ispolicy);
		munlink(&fs->dirty, b, 0);
	}else
		munlink(&fs->clean, b, ispolicy);
	b->unlinkpc = getcallerpc(&b);
}

/*
 * hashing a block also implies placing it in the refs/clean/dirty lists.
 * mbget has also the guts of mbhash, for new blocks.
 */
Memblk*
mbhash(Memblk *b)
{
	Memblk **h;
	uint hv;

	hv = b->addr%nelem(fs->fhash);
	xqlock(&fs->fhash[hv]);
	for(h = &fs->fhash[hv].b; *h != nil; h = &(*h)->next)
		if((*h)->addr == b->addr){
			warn("mbhash: dup blocks:");
			warn("b=> %H*h=> %H", b, *h);
			fatal("mbhash: dup");
		}
	*h = b;
	if(b->next != nil)
		fatal("mbhash: next");
	incref(b);
	linkblock(b);
	xqunlock(&fs->fhash[hv]);
	return b;
}

/*
 * unhashing a block also implies removing it from the refs/clean/dirty lists.
 * 
 */
int
mbunhash(Memblk *b, int ispolicy)
{
	Memblk **h;
	uint hv;

	if(b->type == DBref)
		fatal("mbunhash: DBref");

	hv = b->addr%nelem(fs->fhash);
	if(ispolicy){
		if(!xcanqlock(&fs->fhash[hv]))
			return 0;
	}else
		xqlock(&fs->fhash[hv]);
	for(h = &fs->fhash[hv].b; *h != nil; h = &(*h)->next)
		if((*h)->addr == b->addr){
			if(*h != b)
				fatal("mbunhash: dup");
			*h = b->next;
			b->next = nil;
			unlinkblock(b, ispolicy);
			b->unlinkpc = getcallerpc(&b);
			xqunlock(&fs->fhash[hv]);
			mbput(b);
			return 1;
		}
	fatal("mbunhash: not found");
	return 0;
}

static void
mbfree(Memblk *b)
{
	Mfile *mf;

	if(b == nil)
		return;
	dNprint("mbfree m%#p d%#010ullx\n", b, b->addr);
	if(b->ref > 0)
		fatal("mbfree: d%#010ullx has %d refs\n", b->addr, b->ref);
	if(b->type == DBfree)
		fatal("mbfree: d%#010ullx double free:\n", b->addr);
	if(b->next != nil)
		fatal("mbfree: d%#010ullx has next\n", b->addr);
	if(b->lnext != nil || b->lprev != nil)
		fatal("mbfree: d%#010ullx has lnext/lprev\n", b->addr);

	/* this could panic, but errors reading a block might cause it */
	if(b->type == DBref)
		warn("free of DBref. i/o errors?");

	if(b->mf != nil){
		mf = b->mf;
		b->mf = nil;
		mbput(mf->melted);
		assert(mf->writer == 0 && mf->readers == 0);
		afree(&mfalloc, mf);
	}

	xqlock(fs);
	fs->nmused--;
	fs->nmfree++;
	b->next = fs->free;
	fs->free = b;
	xqunlock(fs);
}

Memblk*
mballocz(daddrt addr, int zeroit)
{
	Memblk *b;
	static int nwait;

	for(;;){
		xqlock(fs);
		if(fs->free != nil){
			b = fs->free;
			fs->free = b->next;
			fs->nmfree--;
			b->next = nil;
			break;
		}
		if(fs->nblk < fs->nablk){
			b = &fs->blk[fs->nblk++];
			break;
		}
		xqunlock(fs);
		if((nwait++ % 60) == 0)
			warn("out of memory blocks. waiting");
		sleep(1000);
	}
	fs->nmused++;
	xqunlock(fs);

	if(zeroit)
		memset(b, 0, sizeof *b);
	else
		memset(&b->Meminfo, 0, sizeof b->Meminfo);

	b->addr = addr;
	b->ref = 1;
	dNprint("mballocz %H", b);
	return b;
}

int
mbhashed(daddrt addr)
{
	Memblk *b;
	uint hv;

	hv = addr%nelem(fs->fhash);
	xqlock(&fs->fhash[hv]);
	for(b = fs->fhash[hv].b; b != nil; b = b->next)
		if(b->addr == addr)
			break;
	xqunlock(&fs->fhash[hv]);
	return b != nil;
}

Memblk*
mbget(int type, daddrt addr, int mkit)
{
	Memblk *b;
	uint hv;

	if(catcherror())
		fatal("mbget: %r");
	hv = addr%nelem(fs->fhash);
	xqlock(&fs->fhash[hv]);
	for(b = fs->fhash[hv].b; b != nil; b = b->next)
		if(b->addr == addr){
			checktag(b->d.tag, type, addr);
			incref(b);
			break;
		}
	if(mkit)
		if(b == nil){
			b = mballocz(addr, 0);
			b->loading = 1;
			b->type = type;
			b->d.tag = TAG(type, 0, addr);
			/* mbhash() it, without releasing the locks */
			b->next = fs->fhash[hv].b;
			fs->fhash[hv].b = b;
			incref(b);
			linkblock(b);
			xqlock(&b->newlk);	/* make others wait for it */
		}else if(b->loading){
			xqunlock(&fs->fhash[hv]);
			xqlock(&b->newlk);	/* wait for it */
			xqunlock(&b->newlk);
			if(b->loading){
				mbput(b);
				dprint("mbget %#ullx -> i/o error\n", addr);
				return nil;	/* i/o error reading it */
			}
			dMprint("mbget %#010ullx -> waited for m%#p\n", addr, b);
			noerror();
			return b;
		}
	xqunlock(&fs->fhash[hv]);
	if(b != nil)
		mbused(b);
	dMprint("mbget %#010ullx -> m%#p\n", addr, b);
	noerror();
	return b;
}

void
mbput(Memblk *b)
{
	if(b == nil)
		return;
	dMprint("mbput m%#p d%#010ullx pc=%#p\n", b, b->addr, getcallerpc(&b));
	if(decref(b) == 0)
		mbfree(b);
}

