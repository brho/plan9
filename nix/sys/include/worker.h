/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#pragma	lib	"libworker.a"
#pragma src "/sys/src/libworker"

typedef char* (*Worker)(void *arg, void **aux);
int	getworker(Worker work, void *arg, Channel *rc);
void	workerdebug(int);

extern int (*workerthreadcreate)(void(*)(void*), void*, uint);
