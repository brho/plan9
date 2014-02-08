/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/*
 * SSH RSA authentication.
 * 
 * Client protocol:
 *	read public key
 *		if you don't like it, read another, repeat
 *	write challenge
 *	read response
 * all numbers are hexadecimal biginits parsable with strtomp.
 */

#include "dat.h"

enum {
	CHavePub,
	CHaveResp,

	Maxphase,
};

static char *phasenames[] = {
[CHavePub]	"CHavePub",
[CHaveResp]	"CHaveResp",
};

struct State
{
	RSApriv *priv;
	mpint *resp;
	int off;
	Key *key;
};

static RSApriv*
readrsapriv(Key *k)
{
	char *a;
	RSApriv *priv;

	priv = rsaprivalloc();

	if((a=_strfindattr(k->attr, "ek"))==nil || (priv->pub.ek=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->attr, "n"))==nil || (priv->pub.n=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!p"))==nil || (priv->p=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!q"))==nil || (priv->q=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!kp"))==nil || (priv->kp=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!kq"))==nil || (priv->kq=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!c2"))==nil || (priv->c2=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!dk"))==nil || (priv->dk=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	return priv;

Error:
	rsaprivfree(priv);
	return nil;
}

static int
sshrsainit(Proto*, Fsstate *fss)
{
	int iscli;
	State *s;

	if((iscli = isclient(_strfindattr(fss->attr, "role"))) < 0)
		return failure(fss, nil);
	if(iscli==0)
		return failure(fss, "sshrsa server unimplemented");

	s = emalloc(sizeof *s);
	fss->phasename = phasenames;
	fss->maxphase = Maxphase;
	fss->phase = CHavePub;
	fss->ps = s;
	return RpcOk;
}

static int
sshrsaread(Fsstate *fss, void *va, uint *n)
{
	RSApriv *priv;
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");
	case CHavePub:
		if(s->key){
			closekey(s->key);
			s->key = nil;
		}
		if(findkey(&s->key, fss, fss->sysuser, 1, s->off, fss->attr, nil) != RpcOk)
			return failure(fss, nil);
		s->off++;
		priv = s->key->priv;
		*n = snprint(va, *n, "%B", priv->pub.n);
		return RpcOk;
	case CHaveResp:
		*n = snprint(va, *n, "%B", s->resp);
		fss->phase = Established;
		return RpcOk;
	}
}

static int
sshrsawrite(Fsstate *fss, void *va, uint)
{
	mpint *m;
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "write");
	case CHavePub:
		if(s->key == nil)
			return failure(fss, "no current key");
		switch(canusekey(fss, s->key)){
		case -1:
			return RpcConfirm;
		case 0:
			return failure(fss, "confirmation denied");
		case 1:
			break;
		}
		m = strtomp(va, nil, 16, nil);
		m = rsadecrypt(s->key->priv, m, m);
		s->resp = m;
		fss->phase = CHaveResp;
		return RpcOk;
	}
}

static void
sshrsaclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->key)
		closekey(s->key);
	if(s->resp)
		mpfree(s->resp);
	free(s);
}

static int
sshrsaaddkey(Key *k, int before)
{
	fmtinstall('B', mpfmt);

	if((k->priv = readrsapriv(k)) == nil){
		werrstr("malformed key data");
		return -1;
	}
	return replacekey(k, before);
}

static void
sshrsaclosekey(Key *k)
{
	rsaprivfree(k->priv);
}

Proto sshrsa = {
.name=	"sshrsa",
.init=		sshrsainit,
.write=	sshrsawrite,
.read=	sshrsaread,
.close=	sshrsaclose,
.addkey=	sshrsaaddkey,
.closekey=	sshrsaclosekey,
};
