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
#include <auth.h>

typedef struct Test Test;

struct Test
{
	char *name;
	int (*server)(Test*, AuthRpc*, int);
	int (*client)(Test*, int);
};

int
ai2status(AuthInfo *ai)
{
	if(ai == nil)
		return -1;
	auth_freeAI(ai);
	return 0;
}

int
proxyserver(Test *t, AuthRpc *rpc, int fd)
{
	char buf[1024];

	sprint(buf, "proto=%q role=server", t->name);
	return ai2status(fauth_proxy(fd, rpc, nil, buf));
}

int
proxyclient(Test *t, int fd)
{
	return ai2status(auth_proxy(fd, auth_getkey, "proto=%q role=client", t->name));
}

Test test[] =
{
	"apop",		proxyserver,		proxyclient,
	"cram",		proxyserver,		proxyclient,
	"p9sk1",		proxyserver,		proxyclient,
	"p9sk2",		proxyserver,		proxyclient,
	"p9any",		proxyserver,		proxyclient
};

void
usage(void)
{
	fprint(2, "usage: test [name]...\n");
	exits("usage");
}

void
runtest(AuthRpc *srpc, Test *t)
{
	int p[2], bad;
	Waitmsg *w;

	if(pipe(p) < 0)
		sysfatal("pipe: %r");

	print("%s...", t->name);

	switch(fork()){
	case -1:
		sysfatal("fork: %r");

	case 0:
		close(p[0]);
		if((*t->server)(t, srpc, p[1]) < 0){
			print("\n\tserver: %r");
			_exits("oops");
		}
		close(p[1]);
		_exits(nil);
	default:
		close(p[1]);
		if((*t->client)(t, p[0]) < 0){
			print("\n\tclient: %r");
			bad = 1;
		}
		close(p[0]);
		break;
	}
	w = wait();
	if(w->msg[0])
		bad = 1;
	print("\n");
}

void
main(int argc, char **argv)
{
	int i, j;
	int afd;
	AuthRpc *srpc;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	quotefmtinstall();
	afd = open("/n/kremvax/factotum/rpc", ORDWR);
	if(afd < 0)
		sysfatal("open /n/kremvax/factotum/rpc: %r");
	srpc = auth_allocrpc(afd);
	if(srpc == nil)
		sysfatal("auth_allocrpc: %r");

	if(argc == 0)
		for(i=0; i<nelem(test); i++)
			runtest(srpc, &test[i]);
	else
		for(i=0; i<argc; i++)
			for(j=0; j<nelem(test); j++)
				if(strcmp(argv[i], test[j].name) == 0)
					runtest(srpc, &test[j]);
	exits(nil);
}
