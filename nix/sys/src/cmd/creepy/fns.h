/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

extern Path*	addelem(Path **pp, Memblk *f);
extern daddrt	addrofref(daddrt refaddr, int idx);
extern void	afree(Alloc *a, void *nd);
extern int	allowed(int uid);
extern int	allowed(int);
extern int	allowed(int);
extern void*	anew(Alloc *a);
extern void	changed(Memblk *b);
extern void	checkblk(Memblk *b);
extern void	checktag(u64int tag, uint type, daddrt addr);
extern char*	cliworker9p(void *v, void**aux);
extern Path*	clonepath(Path *p);
extern void	consinit(void);
extern void	consprint(char *fmt, ...);
extern void	consprintclients(void);
extern long	consread(char *buf, long count);
extern long	conswrite(char *ubuf, long count);
extern void	countfidrefs(void);
extern void	countfidrefs(void);
extern void	countfidrefs(void);
extern Memblk*	dballocz(uint type, int dbit, int zeroit);
extern void	dbclear(u64int tag, daddrt addr);
extern daddrt	dbcounted(daddrt addr);
extern u64int	dbcountref(daddrt addr);
extern Memblk*	dbdup(Memblk *b);
extern Memblk*	dbget(uint type, daddrt addr);
extern daddrt	dbgetref(daddrt addr);
extern daddrt	dbincref(daddrt addr);
extern long	dbput(Memblk *b);
extern long	dbread(Memblk *b);
extern void	dbsetref(daddrt addr, int ref);
extern long	dbwrite(Memblk *b);
extern void	debug(void);
extern void	dfaccessok(Memblk *f, int uid, int bits);
extern ulong	dfbno(Memblk *f, uvlong off, ulong *boffp);
extern void	dfcattr(Memblk *f, int op, char *name, char *val);
extern void	dfchanged(Path *p, int muid);
extern uvlong	dfchdentry(Memblk *d, daddrt addr, daddrt naddr);
extern Memblk*	dfcreate(Memblk *parent, char *name, int uid, ulong mode);
extern uvlong	dfdirmap(Memblk *d, Dirf dirf, void *arg, int iswr);
extern void	dfdropblks(Memblk *f, ulong bno, ulong bend);
extern void	dfdump(Memblk *f, int isdisk);
extern long	dffreeze(Memblk *f);
extern void	dflast(Memblk **fp, int iswr);
extern Memblk*	dfmelt(Memblk *parent, Memblk **fp);
extern ulong	dfpread(Memblk *f, void *a, ulong count, uvlong off);
extern ulong	dfpwrite(Memblk *f, void *a, ulong count, uvlong *off);
extern long	dfrattr(Memblk *f, char *name, char *val, long count);
extern void	dfremove(Memblk *p, Memblk *f);
extern Blksl	dfslice(Memblk *f, ulong len, uvlong off, int iswr);
extern void	dfused(Path *p);
extern Memblk*	dfwalk(Memblk *d, char *name);
extern long	dfwattr(Memblk *f, char *name, char *val);
extern Path*	dropelem(Path **pp);
extern void	dumpfids(void);
extern void	dumplockstats(void);
extern ulong	embedattrsz(Memblk *f);
extern void	fatal(char *fmt, ...);
extern void	fidattach(Fid *fid, char *aname, char *uname);
extern Fid*	fidclone(Cli *cli, Fid *fid, int no);
extern void	fidclose(Fid *fid);
extern void	fidcreate(Fid *fid, char *name, int mode, ulong perm);
extern int	fidfmt(Fmt *fmt);
extern void	fidopen(Fid *fid, int mode);
extern long	fidread(Fid *fid, void *data, ulong count, vlong offset, Packmeta pack);
extern void	fidremove(Fid *fid);
extern void	fidwalk(Fid *fid, char *wname);
extern long	fidwrite(Fid *fid, void *data, ulong count, uvlong *offset);
extern void	freerpc(Rpc *rpc);
extern int	fscheck(void);
extern uvlong	fsdiskfree(void);
extern void	fsdump(int full, int disktoo);
extern void	fsfmt(char *dev, int force);
extern int	fsfull(void);
extern int	fslru(void);
extern uvlong	fsmemfree(void);
extern void	fsopen(char *dev, int worm, int canwr);
extern void	fspolicy(int);
extern int	fsreclaim(void);
extern void	fssync(void);
extern void	fssyncproc(void*);
extern uvlong	fstime(uvlong t);
extern Fid*	getfid(Cli* cli, int no);
extern void	gmeta(Memblk *f, void *buf, ulong nbuf);
extern void	isfile(Memblk *f);
extern void	ismelted(Memblk *b);
extern int	isro(Memblk *f);
extern void	isrwlocked(Memblk *f, int iswr);
extern int	ixcallfmt(Fmt *fmt);
extern uint	ixpack(IXcall *f, uchar *ap, uint nap);
extern uint	ixpackedsize(IXcall *f);
extern char*	ixstats(char *s, char *e, int clr, int verb);
extern char*	ixstats(char *s, char*, int, int);
extern char*	ixstats(char *s, char*, int, int);
extern uint	ixunpack(uchar *ap, uint nap, IXcall *f);
extern Path*	lastpath(Path **pp, int nth);
extern int	leader(int gid, int lead);
extern void	listen9pix(char *addr,  char* (*cliworker)(void *arg, void **aux));
extern void	lockstats(int on);
extern Memblk*	mballocz(daddrt addr, int zeroit);
extern daddrt	mbcountref(Memblk *b);
extern int	mbfmt(Fmt *fmt);
extern Memblk*	mbget(int type, daddrt addr, int mkit);
extern Memblk*	mbhash(Memblk *b);
extern int	mbhashed(daddrt addr);
extern void	mbput(Memblk *b);
extern int	mbunhash(Memblk *b, int isreclaim);
extern void	mbunused(Memblk *b);
extern Path*	meltedpath(Path **pp, int nth, int user);
extern void	meltedref(Memblk *rb);
extern void	meltfids(void);
extern void	meltfids(void);
extern void	meltfids(void);
extern int	member(int uid, int member);
extern int	member(int uid, int member);
extern int	member(int uid, int member);
extern List	mfilter(List *bl, int(*f)(Memblk*));
extern void	mlistdump(char *tag, List *l);
extern void	munlink(List *l, Memblk *b, int isreclaim);
extern Fid*	newfid(Cli* cli, int no);
extern Path*	newpath(Memblk *root);
extern Rpc*	newrpc(void);
extern char*	ninestats(char *s, char *e, int clr, int verb);
extern char*	ninestats(char *s, char*, int, int);
extern char*	ninestats(char *s, char*, int, int);
extern void	nodebug(void);
extern void	ownpath(Path **pp);
extern int	pathfmt(Fmt *fmt);
extern ulong	pmeta(void *buf, ulong nbuf, Memblk *f);
extern int	ptrmap(daddrt addr, int nind, Blkf f, void *a, int isdisk);
extern void	putcli(Cli *cli);
extern void	putfid(Fid *fid);
extern void	putpath(Path *p);
extern void	quiescent(int y);
extern void	rahead(Memblk *f, uvlong offset);
extern daddrt	refaddr(daddrt addr, int *idx);
extern void	replied(Rpc *rpc);
extern void	rlsedebug(int r);
extern int	rpcfmt(Fmt *fmt);
extern void	rwlock(Memblk *f, int iswr);
extern void	rwunlock(Memblk *f, int iswr);
extern void	rwusers(Memblk *uf);
extern void	rwusers(Memblk*);
extern void	rwusers(Memblk*);
extern int	setdebug(void);
extern void	setfiduid(Fid *fid, char *uname);
extern void	srv9pix(char *srv, char* (*cliworker)(void *arg, void **aux));
extern void	timeproc(void*);
extern char*	tname(int t);
extern char*	updatestats(int clr, int verb);
extern int	usrfmt(Fmt *fmt);
extern int	usrid(char *n);
extern int	usrid(char*);
extern int	usrid(char*);
extern char*	usrname(int uid);
extern char*	usrname(int);
extern char*	usrname(int);
extern Path*	walkpath(Memblk *f, char *elems[], int nelems);
extern Path*	walkto(char *a, char **lastp);
extern void	warn(char *fmt, ...);
extern void	warnerror(char *fmt, ...);
extern int	writedenied(int uid);
extern void	written(Memblk *b);
extern void	wstatint(Memblk *f, char *name, u64int v);
extern int	xcanqlock(QLock *q);
extern void	xqlock(QLock *q);
extern void	xqunlock(QLock *q);
extern void	xrwlock(RWLock *rw, int iswr);
extern void	xrwunlock(RWLock *rw, int iswr);
extern long 	wname(Memblk *f, char *val);
