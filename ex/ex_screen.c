/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_screen.c,v 5.2 1993/04/06 11:37:22 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:37:22 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

int
ex_split(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;

	/* Get a new file. */
	if ((tep = file_get(sp, ep,
	    cmdp->argc ? (char *)cmdp->argv[0] : NULL, 1)) == NULL)
		return (1);
	
	return (sp->split(sp, tep));
}
