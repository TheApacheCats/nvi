/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 8.17 1994/05/02 13:51:33 bostic Exp $ (Berkeley) $Date: 1994/05/02 13:51:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../sex/sex_screen.h"

enum which {APPEND, CHANGE, INSERT};

static int aci __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_append -- :[line] a[ppend][!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, APPEND));
}

/*
 * ex_change -- :[line[,line]] c[hange][!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, CHANGE));
}

/*
 * ex_insert -- :[line] i[nsert][!]
 *	Insert one or more lines of new text before the specified line,
 *	or the current line if no address is specified.
 */
int
ex_insert(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, INSERT));
}

static int
aci(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	TEXTH *sv_tiqp, tiq;
	TEXT *tp;
	struct termios t;
	recno_t cnt;
	u_int flags;
	int rval;

	rval = 0;

	/*
	 * Set input flags; the ! flag turns off autoindent for append,
	 * change and insert.
	 */
	LF_INIT(TXT_DOTTERM | TXT_NLECHO);
	if (!F_ISSET(cmdp, E_FORCE) && O_ISSET(sp, O_AUTOINDENT))
		LF_SET(TXT_AUTOINDENT);
	if (O_ISSET(sp, O_BEAUTIFY))
		LF_SET(TXT_BEAUTIFY);

	/* Input is interruptible. */
	F_SET(sp, S_INTERRUPTIBLE);

	/*
	 * If this code is called by vi, the screen TEXTH structure (sp->tiqp)
	 * may already be in use, e.g. ":append|s/abc/ABC/" would fail as we're
	 * only halfway through the line when the append code fires.  Use the
	 * local structure instead.
	 *
	 * If this code is called by vi, we want to reset the terminal and use
	 * ex's s_get() routine.  It actually works fine if we use vi's s_get()
	 * routine, but it doesn't look as nice.  Maybe if we had a separate
	 * window or something, but getting a line at a time looks awkward.
	 */
	if (IN_VI_MODE(sp)) {
		memset(&tiq, 0, sizeof(TEXTH));
		CIRCLEQ_INIT(&tiq);
		sv_tiqp = sp->tiqp;
		sp->tiqp = &tiq;

		if (F_ISSET(sp->gp, G_STDIN_TTY))
			SEX_RAW(t);
		(void)write(STDOUT_FILENO, "\n", 1);
		LF_SET(TXT_NLECHO);

	}

	if (sex_get(sp, ep, sp->tiqp, 0, flags) != INP_OK)
		goto err;
	
	/*
	 * If doing a change, replace lines for as long as possible.
	 * Then, append more lines or delete remaining lines.  Inserts
	 * are the same as appends to the previous line.
	 */
	m = cmdp->addr1;
	if (cmd == INSERT) {
		--m.lno;
		cmd = APPEND;
	}

	tp = sp->tiqp->cqh_first;
	if (cmd == CHANGE)
		for (;; tp = tp->q.cqe_next) {
			if (m.lno > cmdp->addr2.lno) {
				cmd = APPEND;
				--m.lno;
				break;
			}
			if (tp == (TEXT *)sp->tiqp) {
				for (cnt =
				    (cmdp->addr2.lno - m.lno) + 1; cnt--;)
					if (file_dline(sp, ep, m.lno))
						goto err;
				goto done;
			}
			if (file_sline(sp, ep, m.lno, tp->lb, tp->len))
				goto err;
			sp->lno = m.lno++;
		}

	if (cmd == APPEND)
		for (; tp != (TEXT *)sp->tiqp; tp = tp->q.cqe_next) {
			if (file_aline(sp, ep, 1, m.lno, tp->lb, tp->len)) {
err:				rval = 1;
				goto done;
			}
			sp->lno = ++m.lno;
		}

done:	if (IN_VI_MODE(sp)) {
		sp->tiqp = sv_tiqp;
		text_lfree(&tiq);

		/* Reset the terminal state. */
		if (F_ISSET(sp->gp, G_STDIN_TTY)) {
			if (SEX_NORAW(t))
				rval = 1;
			F_SET(sp, S_REFRESH);
		}
	}
	return (rval);
}
