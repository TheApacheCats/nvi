/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 8.10 1994/08/17 14:35:56 bostic Exp $ (Berkeley) $Date: 1994/08/17 14:35:56 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_mark -- m[a-z]
 *	Set a mark.
 */
int
v_mark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark_set(sp, ep, vp->character, &vp->m_start, 1));
}

enum which {BMARK, FMARK};
static int mark __P((SCR *, EXF *, VICMDARG *, enum which));


/*
 * v_bmark -- `['`a-z]
 *	Move to a mark.
 *
 * Moves to a mark, setting both row and column.
 *
 * !!!
 * Although not commonly known, the "'`" and "'`" forms are historically
 * valid.  The behavior is determined by the first character, so "`'" is
 * the same as "``".  Remember this fact -- you'll be amazed at how many
 * people don't know it and will be delighted that you are able to tell
 * them.
 */
int
v_bmark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark(sp, ep, vp, BMARK));
}

/*
 * v_fmark -- '['`a-z]
 *	Move to a mark.
 *
 * Move to the first nonblank character of the line containing the mark.
 */
int
v_fmark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark(sp, ep, vp, FMARK));
}

static int
mark(sp, ep, vp, cmd)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	enum which cmd;
{
	MARK m;
	size_t len;
	enum direction dir;

	if (mark_get(sp, ep, vp->character, &vp->m_stop))
		return (1);

	/* Forward marks move to the first non-blank. */
	if (cmd == FMARK) {
		vp->m_stop.cno = 0;
		if (nonblank(sp, ep, vp->m_stop.lno, &vp->m_stop.cno))
			return (1);
	}

	/* Non-motion commands move to the end of the range. */
	if (!ISMOTION(vp)) {
		vp->m_final = vp->m_stop;
		return (0);
	}

	/*
	 * !!!
	 * If a motion component, the cursor has to move.
	 */
	if (vp->m_stop.lno == vp->m_start.lno &&
	    vp->m_stop.cno == vp->m_start.cno) {
		v_nomove(sp);
		return (1);
	}

	/*
	 * If the motion is in the reverse direction, switch the start and
	 * stop MARK's so that it's in a forward direction.  (There's no
	 * reason for this other than to make the tests below easier.  The
	 * code in vi.c:vi() would have done the switch.)  Both forward
	 * and backward motions can happen for either kind of mark command.
	 */
	if (vp->m_start.lno > vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno > vp->m_stop.cno) {
		dir = BACKWARD;
		m = vp->m_start;
		vp->m_start = vp->m_stop;
		vp->m_stop = m;
	} else
		dir = FORWARD;

	/*
	 * BACKWARD:
	 *	VC_D commands move to the end of the range.  VC_Y stays at
	 *	the start unless the end of the range is on a different line,
	 *	when it moves to the end of the range.  Ignore VC_C and
	 *	VC_DEF.
	 *
	 * FORWARD:
	 *	VC_D and VC_Y commands don't move.  Ignore VC_C and VC_DEF.
	 */
	if (dir == BACKWARD)
		if (F_ISSET(vp, VC_D) ||
		    F_ISSET(vp, VC_Y) && vp->m_start.lno != vp->m_stop.lno)
			vp->m_final = vp->m_start;
		else
			vp->m_final = vp->m_stop;
	else
		vp->m_final = vp->m_start;

	if (cmd == FMARK)
		return (0);

	/*
	 * Forward marks are always line oriented, and it's set in the
	 * vcmd.c table.  Backward marks that start and stop at column
	 * 0 of the line are also line mode commands. 
	 */
	if (vp->m_start.cno == 0 && vp->m_stop.cno == 0)
		F_SET(vp, VM_LMODE);

	/*
	 * BMARK'S that move backward and start at column 0, or move forward
	 * and end at column 0 are corrected to the last column of the previous
	 * line.  Else, adjust the starting/ending point to the character
	 * before the current one (this is safe because we know the command had
	 * to move to succeed).
	 */
	if (vp->m_start.lno < vp->m_stop.lno && vp->m_stop.cno == 0) {
		if (file_gline(sp, ep, --vp->m_stop.lno, &len) == NULL) {
			GETLINE_ERR(sp, vp->m_stop.lno);
			return (1);
		}
		vp->m_stop.cno = len ? len - 1 : 0;
	} else
		--vp->m_stop.cno;

	return (0);
}
