/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 8.22 1993/11/28 18:55:03 bostic Exp $ (Berkeley) $Date: 1993/11/28 18:55:03 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_version -- :version
 *	Display the program version.
 */
int
ex_version(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	static time_t then = 754530892;

	(void)ex_printf(EXCOOKIE,
"Version 0.87, %sThe CSRG, University of California, Berkeley.\n",
	    ctime(&then));
	return (0);
}
