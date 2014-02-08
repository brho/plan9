/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/*
 * '9': 9p
 * 'N': mblk/dblk alloc/free chdentry, drefs
 * 'D': disk
 * 'E': fids
 * 'F': slices, indirects, dirnth
 * 'K': reclaim
 * 'M': mblk/dblk gets puts
 * 'P': procs
 * 'R': block read
 * 'W': block write
 * 'X': ix
 * 'd': general debug
 * 'O': lru blocks out
 * 'Z': policy
 */
#define d9print(...)	if(!dbg['9']){}else fprint(2, __VA_ARGS__)
#define dNprint(...)	if(!dbg['N']){}else fprint(2, __VA_ARGS__)
#define dEprint(...)	if(!dbg['E']){}else fprint(2, __VA_ARGS__)
#define dFprint(...)	if(!dbg['F']){}else fprint(2, __VA_ARGS__)
#define dKprint(...)	if(!dbg['K']){}else fprint(2, __VA_ARGS__)
#define dMprint(...)	if(!dbg['M']){}else fprint(2, __VA_ARGS__)
#define dPprint(...)	if(!dbg['P']){}else fprint(2, __VA_ARGS__)
#define dRprint(...)	if(!dbg['R']){}else fprint(2, __VA_ARGS__)
#define dWprint(...)	if(!dbg['W']){}else fprint(2, __VA_ARGS__)
#define dXprint(...)	if(!dbg['X']){}else fprint(2, __VA_ARGS__)
#define dOprint(...)	if(!dbg['O']){}else fprint(2, __VA_ARGS__)
#define dZprint(...)	if(!dbg['Z']){}else fprint(2, __VA_ARGS__)
#define dprint(...)	if(!dbg['d']){}else fprint(2, __VA_ARGS__)
extern char dbg[256];
