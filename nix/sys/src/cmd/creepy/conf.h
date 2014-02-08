/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */



enum
{
	KiB = 1024UL,
	MiB = KiB * 1024UL,
	GiB = MiB * 1024UL,

#ifdef TESTING
	Incr = 1,
	Fsysmem = 800*KiB,	/* size for in-memory block array */

	/* disk parameters; don't change */
	Dblksz = 512UL,		/* disk block size */
	Ndptr = 2,		/* # of direct data pointers */
	Niptr = 2,		/* # of indirect data pointers */
#else
	Incr = 16,
	Fsysmem = 1*GiB+GiB/2UL,	/* size for in-memory block array */

	/* disk parameters; don't change */
	Dblksz = 8*KiB,		/* disk block size */
	Ndptr = 8,		/* # of direct data pointers */
	Niptr = 4,		/* # of indirect data pointers */

#endif

	Syncival = 5*60,		/* desired sync intervals (s) */
	Mmaxdirtypcent = 50,	/* Max % of blocks dirty in mem */
	Mminfree = 200,		/* # blocks when low on mem blocks */
	Dminfree = 2000,		/* # blocks when low on disk blocks */
	Dminattrsz = Dblksz/2,	/* min size for attributes */

	Nahead = 10 * Dblksz,	/* # of bytes to read ahead */

	/*
	 * Caution: Stack and Errstack also limit the max tree depth,
	 * because of recursive routines (in the worst case).
	 */
	Stack = 128*KiB,		/* stack size for threads */
	Errstack = 128,		/* max # of nested error labels */
	Npathels = 64,		/* max depth (only used by walkto) */

	Fhashsz = 7919,		/* size of file hash (plan9 has 35454 files). */
	Fidhashsz = 97,		/* size of the fid hash size */
	Uhashsz = 97,

	Rpcspercli = 20,		/* != 0 places a limit */

	Nlstats = 1009,		/* # of lock profiling entries */

	Mmaxfree = 10*Mminfree,		/* high on mem blocks */
	Dmaxfree = 2*Dminfree,		/* high on disk blocks */
	Mzerofree = 10,			/* out of memory blocks */
	Dzerofree = 10,			/* out of disk blocks */

	Unamesz = 20,
	Statsbufsz = 1024,

};

