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
#include <seg.h>

enum
{
	Maxname = 80,
	Extra = 20,
};

void*
newseg(char *name, uvlong va, ulong len)
{
	static int once;
	int fd, cfd;
	char sname[Maxname+Extra], *s;
	void *p;

print("newseg %s %#llx %#lx\n", name, va, len);
	/* race, but ok */
	if(once++ == 0)
		if(bind("#g", "/mnt/seg", MREPL|MCREATE) < 0)
			return nil;
	s = seprint(sname, sname+Maxname, "/mnt/seg/%s", name);
	if(s == sname+Maxname){
		werrstr("name too long");
		return nil;
	}
	if(access(sname, AEXIST) < 0){
		if(va & (va-1)){
			werrstr("unusual virtual address");
			return nil;
		}
		fd = create(sname, OREAD, 0640|DMDIR);
		if(fd < 0)
			return nil;
		close(fd);
		strecpy(s, sname+sizeof sname, "/ctl");
		cfd = open(sname, OWRITE);
		*s = 0;
		if(cfd < 0)
			return nil;
		if(fprint(cfd, "addr %#llux %#lux\n", va, len) < 0){
			close(cfd);
print("newseg %s ctl failed %r\n", name);
			return nil;
		}
	}
	p = segattach(SG_CEXEC, name, (void*)va, len);
	if((uintptr)p == ~0)
		sysfatal("segattach: %s %#llx, %r", name, va);
	return p;
}

