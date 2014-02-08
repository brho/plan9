/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#pragma	lib	"libtube.a"
#pragma	src	"/sys/src/libtube"

typedef struct Tube Tube;
typedef struct Talt Talt;

/*
 * Talt.op
 */
enum
{
	TSND = 0x6666,	/* weird numbers, for debugging */
	TRCV,
	TNBSND,
	TNBRCV,
	TNOP
};

struct Tube
{
	int	msz;	/* message size */
	int	tsz;	/* tube size (# of messages) */
	int	nmsg;	/* semaphore: # of messages in tube */
	int	nhole;	/* semaphore: # of free slots in tube */
	int	hd;
	int	tl;
};

struct Talt
{
	Tube*	t;
	void*	m;
	int	op;
};



extern void	freetube(Tube *t);
extern int	nbtrecv(Tube *t, void *p);
extern int	nbtsend(Tube *t, void *p);
extern Tube*	newtube(ulong msz, ulong n);
extern int	talt(Talt a[], int na);
extern void	trecv(Tube *t, void *p);
extern void	tsend(Tube *t, void *p);
extern Tube*	namedtube(char*,ulong,int, int);

extern int namedtubedebug;
