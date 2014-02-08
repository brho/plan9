/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include <u.h>
#include <libc.h>



uvlong
rdpmc(int index)
{
	int fd, n, core;
	char name[16+2+1];	/* 0x0000000000000000\0 */

	core = getcoreno(nil);

	snprint(name, sizeof(name), "/dev/core%4.4d/ctr%2.2ud", core, index);

	fd = open(name, OREAD);
	if (fd < 0)
		return 0xcafebabe;
	n = read(fd, name, sizeof(name) - 1);
	if (n < 0)
		return 0xcafebabe;
	close(fd);
	name[n] = '\0';
	return atoi(name);
}
