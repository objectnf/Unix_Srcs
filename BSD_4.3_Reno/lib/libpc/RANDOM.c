/*-
 * Copyright (c) 1979 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)RANDOM.c	1.5 (Berkeley) 4/9/90";
#endif /* not lint */

#include "h00vars.h"

extern long RAND();

double
RANDOM()
{
	double d;
	long l;

	/*
	 * calculate (1103515245 * seed) mod 2^31-1
	 */
	d = 1103515245.0 * _seed / 2147483647.0;
	l = d;
	d = d - l;
	_seed = d * 2147483647.0;
	/*
	 * want a value in the range 0..1
	 */
	return(d);
}
