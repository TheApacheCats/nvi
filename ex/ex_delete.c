/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 10.7 1996/02/26 14:22:20 bostic Exp $ (Berkeley) $Date: 1996/02/26 14:22:20 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_delete: [line [,line]] d[elete] [buffer] [count] [flags]
 *
 *	Delete lines from the file.
 *
 * PUBLIC: int ex_delete __P((SCR *, EXCMD *));
 */
int
ex_delete(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	recno_t lno;

	NEEDFILE(sp, cmdp);

	/*
	 * !!!
	 * Historically, lines deleted in ex were not placed in the numeric
	 * buffers.  We follow historic practice so that we don't overwrite
	 * vi buffers accidentally.
	 */
	if (cut(sp,
	    FL_ISSET(cmdp->iflags, E_C_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE))
		return (1);

	/* Delete the lines. */
	if (delete(sp, &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Set the cursor to the line after the last line deleted. */
	sp->lno = cmdp->addr1.lno;

	/* Or the last line in the file if deleted to the end of the file. */
	if (db_last(sp, &lno))
		return (1);
	if (sp->lno > lno)
		sp->lno = lno;
	return (0);
}
