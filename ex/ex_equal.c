/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 8.3 1993/10/03 15:23:29 bostic Exp $ (Berkeley) $Date: 1993/10/03 15:23:29 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_equal -- :address =
 *	Print out the line number matching the specified address, or the
 *	last line number in the file if no address specified.
 */
int
ex_equal(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	if (F_ISSET(cmdp, E_ADDRDEF)) {
		if (file_lline(sp, ep, &lno))
			return (1);
		(void)fprintf(sp->stdfp, "%ld\n", lno);
	} else
		(void)fprintf(sp->stdfp, "%ld\n", cmdp->addr1.lno);
	return (0);
}
