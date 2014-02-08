/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#pragma	lib	"liberror.a"
#pragma src "/sys/src/liberror"

typedef struct Error Error;

struct Error {
	int	nerr;
	int	nlabel;
	jmp_buf label[1];	/* of [stack] actually */
};

#pragma	varargck	argpos	error	1

void	errinit(int stack);
void	noerror(void);
int	errstacksize(void);
void	error(char* msg, ...);
#define	catcherror()	setjmp((*__ep)->label[(*__ep)->nerr++])
extern Error**	__ep;
