/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 9.5 1994/12/03 11:28:32 bostic Exp $ (Berkeley) $Date: 1994/12/03 11:28:32 $";
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

static LMARK *mark_find __P((SCR *, ARG_CHAR_T));

/*
 * Marks are maintained in a key sorted doubly linked list.  We can't
 * use arrays because we have no idea how big an index key could be.
 * The underlying assumption is that users don't have more than, say,
 * 10 marks at any one time, so this will be is fast enough.
 *
 * Marks are fixed, and modifications to the line don't update the mark's
 * position in the line.  This can be hard.  If you add text to the line,
 * place a mark in that text, undo the addition and use ` to move to the
 * mark, the location will have disappeared.  It's tempting to try to adjust
 * the mark with the changes in the line, but this is hard to do, especially
 * if we've given the line to v_ntext.c:v_ntext() for editing.  Historic vi
 * would move to the first non-blank on the line when the mark location was
 * past the end of the line.  This can be complicated by deleting to a mark
 * that has disappeared using the ` command.  Historic vi vi treated this as
 * a line-mode motion and deleted the line.  This implementation complains to
 * the user.
 *
 * In historic vi, marks returned if the operation was undone, unless the
 * mark had been subsequently reset.  Tricky.  This is hard to start with,
 * but in the presence of repeated undo it gets nasty.  When a line is
 * deleted, we delete (and log) any marks on that line.  An undo will create
 * the mark.  Any mark creations are noted as to whether the user created
 * it or if it was created by an undo.  The former cannot be reset by another
 * undo, but the latter may.
 *
 * All of these routines translate ABSMARK2 to ABSMARK1.  Setting either of
 * the absolute mark locations sets both, so that "m'" and "m`" work like
 * they, ah, for lack of a better word, "should".
 */

/*
 * mark_init --
 *	Set up the marks.
 */
int
mark_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * !!!
	 * ep MAY NOT BE THE SAME AS sp->ep, DON'T USE THE LATTER.
	 *
	 * Set up the marks.
	 */
	LIST_INIT(&ep->marks);
	return (0);
}

/*
 * mark_end --
 *	Free up the marks.
 */
int
mark_end(sp, ep)
	SCR *sp;
	EXF *ep;
{
	LMARK *lmp;

	/*
	 * !!!
	 * ep MAY NOT BE THE SAME AS sp->ep, DON'T USE THE LATTER.
	 */
	while ((lmp = ep->marks.lh_first) != NULL) {
		LIST_REMOVE(lmp, q);
		FREE(lmp, sizeof(LMARK));
	}
	return (0);
}

/*
 * mark_get --
 *	Get the location referenced by a mark.
 */
int
mark_get(sp, key, mp)
	SCR *sp;
	ARG_CHAR_T key;
	MARK *mp;
{
	LMARK *lmp;
	size_t len;

	if (key == ABSMARK2)
		key = ABSMARK1;

	lmp = mark_find(sp, key);
	if (lmp == NULL || lmp->name != key) {
		msgq(sp, M_BERR, "049|Mark %s: not set", KEY_NAME(sp, key));
                return (1);
	}
	if (F_ISSET(lmp, MARK_DELETED)) {
		msgq(sp, M_BERR,
		    "050|Mark %s: the line was deleted", KEY_NAME(sp, key));
                return (1);
	}

	/*
	 * !!!
	 * The absolute mark is initialized to lno 1/cno 0, and historically
	 * you could use it in an empty file.  Make such a mark always work.
	 */
	if ((lmp->lno != 1 || lmp->cno != 0) &&
	    (file_gline(sp, lmp->lno, &len) == NULL ||
	    lmp->cno > len || lmp->cno == len && len != 0)) {
		msgq(sp, M_BERR,
		    "051|Mark %s: cursor position no longer exists",
		    KEY_NAME(sp, key));
		return (1);
	}
	mp->lno = lmp->lno;
	mp->cno = lmp->cno;
	return (0);
}

/*
 * mark_set --
 *	Set the location referenced by a mark.
 */
int
mark_set(sp, key, value, userset)
	SCR *sp;
	ARG_CHAR_T key;
	MARK *value;
	int userset;
{
	LMARK *lmp, *lmt;

	if (key == ABSMARK2)
		key = ABSMARK1;

	/*
	 * The rules are simple.  If the user is setting a mark (if it's a
	 * new mark this is always true), it always happens.  If not, it's
	 * an undo, and we set it if it's not already set or if it was set
	 * by a previous undo.
	 */
	lmp = mark_find(sp, key);
	if (lmp == NULL || lmp->name != key) {
		MALLOC_RET(sp, lmt, LMARK *, sizeof(LMARK));
		if (lmp == NULL) {
			LIST_INSERT_HEAD(&sp->ep->marks, lmt, q);
		} else
			LIST_INSERT_AFTER(lmp, lmt, q);
		lmp = lmt;
	} else if (!userset &&
	    !F_ISSET(lmp, MARK_DELETED) && F_ISSET(lmp, MARK_USERSET))
		return (0);

	lmp->lno = value->lno;
	lmp->cno = value->cno;
	lmp->name = key;
	lmp->flags = userset ? MARK_USERSET : 0;
	return (0);
}

/*
 * mark_find --
 *	Find the requested mark, or, the slot immediately before
 *	where it would go.
 */
static LMARK *
mark_find(sp, key)
	SCR *sp;
	ARG_CHAR_T key;
{
	LMARK *lmp, *lastlmp;

	/*
	 * Return the requested mark or the slot immediately before
	 * where it should go.
	 */
	for (lastlmp = NULL, lmp = sp->ep->marks.lh_first;
	    lmp != NULL; lastlmp = lmp, lmp = lmp->q.le_next)
		if (lmp->name >= key)
			return (lmp->name == key ? lmp : lastlmp);
	return (lastlmp);
}

/*
 * mark_insdel --
 *	Update the marks based on an insertion or deletion.
 */
void
mark_insdel(sp, op, lno)
	SCR *sp;
	enum operation op;
	recno_t lno;
{
	LMARK *lmp;

	switch (op) {
	case LINE_APPEND:
		return;
	case LINE_DELETE:
		for (lmp = sp->ep->marks.lh_first;
		    lmp != NULL; lmp = lmp->q.le_next)
			if (lmp->lno >= lno)
				if (lmp->lno == lno) {
					F_SET(lmp, MARK_DELETED);
					(void)log_mark(sp, lmp);
				} else
					--lmp->lno;
		return;
	case LINE_INSERT:
		for (lmp = sp->ep->marks.lh_first;
		    lmp != NULL; lmp = lmp->q.le_next)
			if (lmp->lno >= lno)
				++lmp->lno;
		return;
	case LINE_RESET:
		return;
	}
	/* NOTREACHED */
}
