/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include "std.h"
#include "dat.h"

/*
ulong conftaggen;
int
canusekey(Fsstate *fss, Key *k)
{
	int i;

	if(_strfindattr(k->attr, "confirm")){
		for(i=0; i<fss->nconf; i++)
			if(fss->conf[i].key == k)
				return fss->conf[i].canuse;
		if(fss->nconf%16 == 0)
			fss->conf = erealloc(fss->conf, (fss->nconf+16)*(sizeof(fss->conf[0])));
		fss->conf[fss->nconf].key = k;
		k->ref++;
		fss->conf[fss->nconf].canuse = -1;
		fss->conf[fss->nconf].tag = conftaggen++;
		fss->nconf++;
		return -1;
	}
	return 1;
}
*/

#ifdef UNDEF
#endif

/*
char*
phasename(Fsstate *fss, int phase, char *tmp)
{
	char *name;

	if(fss->phase == Broken)
		name = "Broken";
	else if(phase == Established)
		name = "Established";
	else if(phase == Notstarted)
		name = "Notstarted";
	else if(phase < 0 || phase >= fss->maxphase
	|| (name = fss->phasename[phase]) == nil){
		sprint(tmp, "%d", phase);
		name = tmp;
	}
	return name;
}
*/

#ifdef UNDEF
void
disablekey(Key *k)
{
	Attr *a;

	if(sflag)	/* not on servers */
		return;
	for(a=k->attr; a; a=a->next){
		if(a->type==AttrNameval && strcmp(a->name, "disabled") == 0)
			return;
		if(a->next == nil)
			break;
	}
	if(a)
		a->next = _mkattr(AttrNameval, "disabled", "by.factotum", nil);
	else
		k->attr = _mkattr(AttrNameval, "disabled", "by.factotum", nil);	/* not reached: always a proto attribute */
}
#endif
