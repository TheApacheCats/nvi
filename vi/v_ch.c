/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ch.c,v 5.13 1992/10/10 13:59:34 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:59:34 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

enum csearchdir { NOTSET, FSEARCH, fSEARCH, TSEARCH, tSEARCH };
static enum csearchdir lastdir;
static int lastkey;

#define	NOPREV {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("No previous F, f, T or t search.");		\
	return (1);							\
}

#define	NOTFOUND {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("Character not found.");				\
	return (1);							\
}

/*
 * v_chrepeat -- [count];
 *	Repeat the last F, f, T or t search.
 */
int
v_chrepeat(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	vp->character = lastkey;

	switch(lastdir) {
	case NOTSET:
		NOPREV;
	case FSEARCH:
		return (v_chF(vp, fm, tm, rp));
	case fSEARCH:
		return (v_chf(vp, fm, tm, rp));
	case TSEARCH:
		return (v_chT(vp, fm, tm, rp));
	case tSEARCH:
		return (v_cht(vp, fm, tm, rp));
	}
	/* NOTREACHED */
}

/*
 * v_chrrepeat -- [count],
 *	Repeat the last F, f, T or t search in the reverse direction.
 */
int
v_chrrepeat(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;
	enum csearchdir savedir;

	vp->character = lastkey;
	savedir = lastdir;

	switch(lastdir) {
	case NOTSET:
		NOPREV;
	case FSEARCH:
		rval = v_chf(vp, fm, tm, rp);
		break;
	case fSEARCH:
		rval = v_chF(vp, fm, tm, rp);
		break;
	case TSEARCH:
		rval = v_cht(vp, fm, tm, rp);
		break;
	case tSEARCH:
		rval = v_chT(vp, fm, tm, rp);
		break;
	}
	lastdir = savedir;
	return (rval);
}

/*
 * v_cht -- [count]tc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor to its left.
 */
int
v_cht(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	if (!(rval = v_chf(vp, fm, tm, rp)))
		--rp->cno;
	lastdir = tSEARCH;
	return (rval);
}
	
/*
 * v_chf -- [count]fc
 *	Search forward in the line for the next occurrence of the character.
 */
int
v_chf(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register u_char *ep, *p;
	size_t len;
	u_long cnt;
	u_char *sp;

	lastdir = fSEARCH;
	lastkey = key = vp->character;

	EGETLINE(p, fm->lno, len);
	if (len == 0)
		NOTFOUND;

	sp = p;
	ep = p + len;
	p += fm->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (++p < ep && *p != key);
		if (p == ep)
			NOTFOUND;
	}
	rp->lno = fm->lno;
	rp->cno = p - sp;
	return (0);
}

/*
 * v_chT -- [count]Tc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor to its right.
 */
int
v_chT(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	if (!(rval = v_chF(vp, fm, tm, rp)))
		++rp->cno;
	lastdir = TSEARCH;
	return (0);
}

/*
 * v_chF -- [count]Fc
 *	Search backward in the line for the next occurrence of the character.
 */
int
v_chF(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register u_char *p, *ep;
	size_t len;
	u_long cnt;

	lastdir = FSEARCH;
	lastkey = key = vp->character;

	EGETLINE(p, fm->lno, len);
	if (len == 0)
		NOTFOUND;

	ep = p - 1;
	p += fm->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (--p > ep && *p != key);
		if (p == ep)
			NOTFOUND;
	}
	rp->lno = fm->lno;
	rp->cno = (p - ep) - 1;
	return (0);
}
