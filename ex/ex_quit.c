/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 10.5 1996/02/26 14:22:37 bostic Exp $ (Berkeley) $Date: 1996/02/26 14:22:37 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_quit -- :quit[!]
 *	Quit.
 *
 * PUBLIC: int ex_quit __P((SCR *, EXCMD *));
 */
int
ex_quit(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	int force;

	force = FL_ISSET(cmdp->iflags, E_C_FORCE);

	/* Check for file modifications, or more files to edit. */
	if (file_m2(sp, force) || ex_ncheck(sp, force))
		return (1);

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}
