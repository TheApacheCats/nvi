/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 8.19 1994/03/08 19:41:29 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:41:29 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

static int bcorrect __P((SCR *, EXF *, VICMDARG *, u_int));
static int fcorrect __P((SCR *, EXF *, VICMDARG *, u_int));
static int getptrn __P((SCR *, EXF *, int, char **));
static int search __P((SCR *, EXF *, VICMDARG *, char *, int, enum direction));

/*
 * v_searchn -- n
 *	Repeat last search.
 */
int
v_searchn(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (search(sp, ep, vp, NULL, SEARCH_MSG, sp->searchdir));
}

/*
 * v_searchN -- N
 *	Reverse last search.
 */
int
v_searchN(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	enum direction dir;

	switch (dir) {
	case BACKWARD:
		dir = FORWARD;
		break;
	case FORWARD:
	default:			/* NOTSET handled in search(). */
		dir = BACKWARD;
		break;
	}
	return (search(sp, ep, vp, NULL, SEARCH_MSG, dir));
}

/*
 * v_searchb -- [count]?RE[? offset]
 *	Search backward.
 */
int
v_searchb(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	char *ptrn;

	if (F_ISSET(vp, VC_ISDOT))
		ptrn = NULL;
	else {
		if (getptrn(sp, ep, '?', &ptrn))
			return (1);
		if (ptrn == NULL)
			return (0);
	}
	return (search(sp, ep, vp, ptrn,
	    SEARCH_MSG | SEARCH_PARSE | SEARCH_SET | SEARCH_TERM, BACKWARD));
}

/*
 * v_searchf -- [count]/RE[/ offset]
 *	Search forward.
 */
int
v_searchf(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	char *ptrn;

	if (F_ISSET(vp, VC_ISDOT))
		ptrn = NULL;
	else {
		if (getptrn(sp, ep, '/', &ptrn))
			return (1);
		if (ptrn == NULL)
			return (0);
	}
	return (search(sp, ep, vp, ptrn,
	    SEARCH_MSG | SEARCH_PARSE | SEARCH_SET | SEARCH_TERM, FORWARD));
}

/*
 * v_searchw -- [count]^A
 *	Search for the word under the cursor.
 */
int
v_searchw(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	size_t blen, len;
	int rval;
	char *bp;

	len = vp->kbuflen + sizeof(RE_WSTART) + sizeof(RE_WSTOP);
	GET_SPACE_RET(sp, bp, blen, len);
	(void)snprintf(bp, blen, "%s%s%s", RE_WSTART, vp->keyword, RE_WSTOP);
		
	rval = search(sp, ep, vp, bp, SEARCH_MSG, FORWARD);

	FREE_SPACE(sp, bp, blen);
	return (rval);
}

static int
search(sp, ep, vp, ptrn, flags, dir)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	int flags;
	char *ptrn;
	enum direction dir;
{
	if (ISMOTION(vp))
		flags |= SEARCH_EOL;
	switch (dir) {
	case BACKWARD:
		if (b_search(sp, ep,
		    &vp->m_start, &vp->m_stop, ptrn, NULL, &flags))
			return (1);
		/* Non-motion commands move to the end of the range. */
		if (!ISMOTION(vp))
			vp->m_final = vp->m_stop;
		else if (bcorrect(sp, ep, vp, flags))
			return (1);
		break;
	case FORWARD:
		if (f_search(sp, ep,
		    &vp->m_start, &vp->m_stop, ptrn, NULL, &flags))
			return (1);
		/* Non-motion commands move to the end of the range. */
		if (!ISMOTION(vp))
			vp->m_final = vp->m_stop;
		else if (fcorrect(sp, ep, vp, flags))
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERR, "No previous search pattern.");
		return (1);
	default:
		abort();
	}
	return (0);
}

/*
 * getptrn --
 *	Get the search pattern.
 */
static int
getptrn(sp, ep, prompt, storep)
	SCR *sp;
	EXF *ep;
	int prompt;
	char **storep;
{
	TEXT *tp;

	if (sp->s_get(sp, ep, &sp->tiq, prompt,
	    TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT) != INP_OK)
		return (1);

	/* Len is 0 if backspaced over the prompt, 1 if only CR entered. */
	tp = sp->tiq.cqh_first;
	if (tp->len == 0)
		*storep = NULL;
	else
		*storep = tp->lb;
	return (0);
}

/*
 * !!!
 * Historically, commands didn't affect the line searched to if the motion
 * command was a search and the pattern match was the start or end of the
 * line.  There were some special cases, however, concerning search to the
 * start of end of a line.
 *
 * Vi was not, however, consistent, and it was fairly easy to confuse it.
 * For example, given the two lines:
 *
 *	abcdefghi
 *	ABCDEFGHI
 *
 * placing the cursor on the 'A' and doing y?$ would so confuse it that 'h'
 * 'k' and put would no longer work correctly.  In any case, we try to do
 * the right thing, but it's not going to exactly match historic practice.
 */

/*
 * bcorrect --
 *	Handle command with a backward search as the motion.
 */
static int
bcorrect(sp, ep, vp, flags)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	u_int flags;
{
	size_t len;

	/*
	 * VC_D commands move to the end of the range.  VC_Y stays at the
	 * start unless the end of the range is on a different line, when
	 * it moves to the end of the range.  Ignore VC_C and VC_S.
	 */
	if (F_ISSET(vp, VC_D) ||
	    F_ISSET(vp, VC_Y) && vp->m_start.lno != vp->m_stop.lno)
		vp->m_final = vp->m_stop;
	else
		vp->m_final = vp->m_start;

	/*
	 * !!!
	 * Correct backward searches which start at column 0 to be the last
	 * column of the previous line.  Otherwise, adjust the starting point
	 * to the character before the current one.
	 *
	 * Backward searches become line mode operations if they start
	 * at column 0 and end at column 0 of another line.
	 */
	if (vp->m_start.lno > vp->m_stop.lno && vp->m_start.cno == 0) {
		if (file_gline(sp, ep, --vp->m_start.lno, &len) == NULL) {
			GETLINE_ERR(sp, vp->m_stop.lno);
			return (1);
		}
		if (vp->m_stop.cno == 0)
			F_SET(vp, VM_LMODE);
		vp->m_start.cno = len ? len - 1 : 0;
	} else
		--vp->m_start.cno;

	/*
	 * !!!
	 * Commands become line mode operations if there was a delta
	 * specified to the search pattern.
	 */
	if (LF_ISSET(SEARCH_DELTA)) {
		F_SET(vp, VM_LMODE);
		return (0);
	}
	return (0);
}

/*
 * fcorrect --
 *	Handle command with a forward search as the motion.
 */
static int
fcorrect(sp, ep, vp, flags)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	u_int flags;
{
	size_t len;

	/* VC_D and VC_Y commands stay at the start.  Ignore VC_C and VC_S. */
	vp->m_final = vp->m_start;

	/*
	 * !!!
	 * Correct forward searches which end at column 0 to be the last
	 * column of the previous line.  Otherwise, adjust the ending
	 * point to the character before the current one.
	 *
	 * Forward searches become line mode operations if they start
	 * at column 0 and end at column 0 of another line.
	 */
	if (vp->m_start.lno < vp->m_stop.lno && vp->m_stop.cno == 0) {
		if (file_gline(sp, ep, --vp->m_stop.lno, &len) == NULL) {
			GETLINE_ERR(sp, vp->m_stop.lno);
			return (1);
		}
		if (vp->m_start.cno == 0)
			F_SET(vp, VM_LMODE);
		vp->m_stop.cno = len ? len - 1 : 0;
	} else
		--vp->m_stop.cno;

	/*
	 * !!!
	 * Commands become line mode operations if there was a delta
	 * specified to the search pattern.
	 */
	if (LF_ISSET(SEARCH_DELTA)) {
		F_SET(vp, VM_LMODE);
		return (0);
	}

	return (0);
}
