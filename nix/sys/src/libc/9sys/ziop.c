/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include	<u.h>
#include	<libc.h>

int
ziop(int fd[2])
{
	if(bind("#∏", "/mnt/zp", MREPL|MCREATE) < 0)
		return -1;
	fd[0] = open("/mnt/zp/data", ORDWR);
	if(fd[0] < 0)
		return -1;
	fd[1] = open("/mnt/zp/data1", ORDWR);
	if(fd[1] < 0){
		close(fd[0]);
		return -1;
	}
	return 0;
}
