/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 8.19 1993/12/02 19:54:33 bostic Exp $ (Berkeley) $Date: 1993/12/02 19:54:33 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "sex/sex_screen.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *
 * Pass the rest of the line after the ! character to the program named by
 * the O_SHELL option.
 *
 * Historical vi did NOT do shell expansion on the arguments before passing
 * them, only file name expansion.  This means that the O_SHELL program got
 * "$t" as an argument if that is what the user entered.  Also, there's a
 * special expansion done for the bang command.  Any exclamation points in
 * the user's argument are replaced by the last, expanded ! command.
 *
 * There's some fairly amazing slop in this routine to make the different
 * ways of getting here display the right things.  It took a long time to
 * get it right (wrong?), so be careful.
 */
int
ex_bang(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	enum filtertype ftype;
	ARGS *ap;
	EX_PRIVATE *exp;
	MARK rm;
	recno_t lno;
	size_t blen;
	int rval;
	char *bp, *msg;

	
	if (cmdp->argv[0]->len == 0) {
		msgq(sp, M_ERR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}
	ap = cmdp->argv[0];

	/* Swap commands. */
	exp = EXP(sp);
	if (exp->lastbcomm != NULL)
		FREE(exp->lastbcomm, strlen(exp->lastbcomm) + 1);
	if ((exp->lastbcomm = strdup(ap->bp)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}

	/*
	 * If the command was modified by the expansion, we redisplay it.
	 * Redisplaying it in vi mode is tricky, and handled separately
	 * in each case below.  If we're in ex mode, it's easy, so we just
	 * do it here.
	 */
	bp = NULL;
	if (F_ISSET(cmdp, E_MODIFY)) {
		if (IN_EX_MODE(sp)) {
			(void)ex_printf(EXCOOKIE, "!%s\n", ap->bp);
			(void)ex_fflush(EXCOOKIE);
		}
		/*
		 * Vi: Display the command if modified.  Historic vi displayed
		 * the command if it was modified due to file name and/or bang
		 * expansion.  If piping lines, it was immediately overwritten
		 * by any error or line change reporting.  We don't the user to
		 * have to page through the responses, so we only post it until
		 * it's erased by something else.  Otherwise, pass it on to the
		 * ex_exec_proc routine to display after the screen has been
		 * cleaned up.
		 */
		if (IN_VI_MODE(sp)) {
			GET_SPACE(sp, bp, blen, ap->len + 2);
			bp[0] = '!';
			memmove(bp + 1, ap->bp, ap->len + 1);
		}
	}

	/*
	 * If addresses were specified, pipe lines from the file through
	 * the command.
	 */
	if (cmdp->addrcnt != 0) {
		if (bp != NULL) {
			(void)sp->s_busy(sp, bp);
			FREE_SPACE(sp, bp, blen);
		}
		/*
		 * !!!
		 * Historical vi permitted "!!" in an empty file.  When it
		 * happens, we get called with two addresses of 1,1 and a
		 * bad attitude.
		 */
		ftype = FILTER;
		if (cmdp->addr1.lno == 1 && cmdp->addr2.lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno == 0) {
				cmdp->addr1.lno = cmdp->addr2.lno = 0;
				ftype = FILTER_READ;
			}
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, ap->bp, ftype))
			return (1);
		sp->lno = rm.lno;
		F_SET(sp, S_AUTOPRINT);
		return (0);
	}

	/*
	 * If no addresses were specified, run the command.  If the file
	 * has been modified and autowrite is set, write the file back.
	 * If the file has been modified, autowrite is not set and the
	 * warn option is set, tell the user about the file.
	 */
	msg = "\n";
	if (F_ISSET(ep, F_MODIFIED))
		if (O_ISSET(sp, O_AUTOWRITE)) {
			if (file_write(sp, ep, NULL, NULL, NULL, FS_ALL)) {
				rval = 1;
				goto ret;
			}
		} else if (O_ISSET(sp, O_WARN))
			if (IN_VI_MODE(sp) && F_ISSET(cmdp, E_MODIFY))
				msg = "\nFile modified since last write.\n";
			else
				msg = "File modified since last write.\n";

	/* Run the command. */
	rval = ex_exec_proc(sp, ap->bp, bp, msg);

	/* Ex terminates with a bang. */
	if (IN_EX_MODE(sp))
		(void)write(STDOUT_FILENO, "!\n", 2);

	/* Vi requires user permission to continue. */
	if (IN_VI_MODE(sp))
		F_SET(sp, S_CONTINUE);

	/* Free the extra space. */
ret:	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);

	return (rval);
}
