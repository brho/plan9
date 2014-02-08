/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

typedef struct Fid Fid;
typedef struct Rpc Rpc;
typedef struct Shortrpc Shortrpc;
typedef struct Largerpc Largerpc;
typedef struct Cli Cli;

enum
{
	Maxmdata = 8*KiB,
	Minmdata = 128,

	QTCACHE = 0x02,	/* experiment */
};

/*
 * One reference kept because of existence and another per req using it.
 */
struct Fid
{
	Fid	*next;		/* in alloc or free list */
	Fid	*prev;		/* in alloc list */
	Fid	*hnext;		/* in hash */
	Ref;
	QLock;
	Cli*	cli;		/* no is local to a client */
	int	no;
	Path*	p;
	int	omode;		/* -1 if closed */
	int	rclose;
	int	archived;
	int	cflags;		/* OCERR|OCEND */
	int	consopen;	/* for flush. has /cons open? */
	int	uid;

	uvlong	loff;		/* last offset, for dir reads */
	int	lidx;		/* next dir entry index to read */
	char*	buf;		/* for statsread() */

	int	afd;		/* for afids */
	int	authok;		/* for afids */
	AuthRpc *rpc;		/* for afids */
};

struct Rpc
{
	Rpc	*next;		/* in client or free list */
	Cli	*cli;
	Fid	*fid;
	union{
		Fcall	t;
		IXcall	xt;
	};
	union{
		Fcall	r;
		IXcall	xr;
	};

	/* these are for ix */
	Rpc*	rpc0;		/* where to get fid, c, closed, flushed */
	u16int	chan;		/* channel # (ix) */
	Channel* c;		/* to worker (ix) */
	int	closed;		/* got last rpc in chan */

	vlong	t0;
	int	flushed;
	uchar	data[1];
};

/*
 * 9p, and ix if carrying data.
 */
struct Largerpc
{
	Rpc;
	uchar	buf[IOHDRSZ+Maxmdata];
};

/*
 * ix requests that do not carry much data.
 */
struct Shortrpc
{
	Rpc;
	uchar	buf[IOHDRSZ+Minmdata];
};

struct Cli
{
	Cli *next;		/* in list of clients or free list*/
	Ref;
	int fd;
	int cfd;
	char *addr;
	int uid;
	ulong msize;

	QLock wlk;	/* lock for writing replies to the client */
	uchar wdata[IOHDRSZ+Maxmdata];

	QLock rpclk;
	ulong nrpcs;	/* should we limit the max # per client? */
	Rpc *rpcs;
};

typedef ulong (*Packmeta)(Memblk*, uchar*, int);

#pragma	varargck	type	"X"	Fid*
#pragma	varargck	type	"R"	Rpc*

extern vlong calltime[];
extern ulong ncalls[];
extern char *callname[];
extern Alloc fidalloc, rpcalloc, clialloc;
extern int noauth;
