/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 9.2 1995/01/11 15:58:25 bostic Exp $ (Berkeley) $Date: 1995/01/11 15:58:25 $";
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
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "../vi/vcmd.h"
#include "excmd.h"
#include "../ex/tag.h"

/*
 * screen_init --
 *	Do the default initialization of an SCR structure.
 */
int
screen_init(orig, spp)
	SCR *orig, **spp;
{
	SCR *sp;
	size_t len;

	*spp = NULL;
	CALLOC_RET(orig, sp, SCR *, 1, sizeof(SCR));
	*spp = sp;

/* INITIALIZED AT SCREEN CREATE. */
	sp->gp = __global_list;			/* All ref the GS structure. */

	LIST_INIT(&sp->msgq);
	CIRCLEQ_INIT(&sp->frefq);

	sp->ccnt = 2;				/* Anything > 1 */

	sp->stdfp = stdout;			/* Start off at the terminal. */

	FD_ZERO(&sp->rdfd);

	/*
	 * XXX
	 * sp->defscroll is initialized by the opts_init() code because
	 * we don't have the option information yet.
	 */

	sp->tiqp = &sp->__tiq;
	CIRCLEQ_INIT(&sp->__tiq);

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig == NULL) {
		sp->searchdir = NOTSET;
	} else {
		if (orig->alt_name != NULL &&
		    (sp->alt_name = strdup(orig->alt_name)) == NULL)
			goto mem;

		/* Retain all searching/substitution information. */
		if (F_ISSET(orig, S_SRE_SET)) {
			F_SET(sp, S_SRE_SET);
			sp->sre = orig->sre;
		}
		if (F_ISSET(orig, S_SUBRE_SET)) {
			F_SET(sp, S_SUBRE_SET);
			sp->subre = orig->subre;
		}
		sp->searchdir = orig->searchdir == NOTSET ? NOTSET : FORWARD;

		if (orig->repl_len) {
			MALLOC(sp, sp->repl, char *, orig->repl_len);
			if (sp->repl == NULL)
				goto mem;
			sp->repl_len = orig->repl_len;
			memmove(sp->repl, orig->repl, orig->repl_len);
		}
		if (orig->newl_len) {
			len = orig->newl_len * sizeof(size_t);
			MALLOC(sp, sp->newl, size_t *, len);
			if (sp->newl == NULL) {
mem:				msgq(orig, M_SYSERR, NULL);
				goto err;
			}
			sp->newl_len = orig->newl_len;
			sp->newl_cnt = orig->newl_cnt;
			memmove(sp->newl, orig->newl, len);
		}

		sp->saved_vi_mode = orig->saved_vi_mode;

		if (opts_copy(orig, sp))
			goto err;

		sp->s_bell		= orig->s_bell;
		sp->s_bg		= orig->s_bg;
		sp->s_busy		= orig->s_busy;
		sp->s_change		= orig->s_change;
		sp->s_clear		= orig->s_clear;
		sp->s_colpos		= orig->s_colpos;
		sp->s_column		= orig->s_column;
		sp->s_confirm		= orig->s_confirm;
		sp->s_crel		= orig->s_crel;
		sp->s_edit		= orig->s_edit;
		sp->s_end		= orig->s_end;
		sp->s_ex_cmd		= orig->s_ex_cmd;
		sp->s_ex_run		= orig->s_ex_run;
		sp->s_ex_write		= orig->s_ex_write;
		sp->s_fg		= orig->s_fg;
		sp->s_fill		= orig->s_fill;
		sp->s_get		= orig->s_get;
		sp->s_key_read		= orig->s_key_read;
		sp->s_fmap		= orig->s_fmap;
		sp->s_position		= orig->s_position;
		sp->s_rabs		= orig->s_rabs;
		sp->s_rcm		= orig->s_rcm;
		sp->s_refresh		= orig->s_refresh;
		sp->s_scroll		= orig->s_scroll;
		sp->s_split		= orig->s_split;
		sp->s_suspend		= orig->s_suspend;

		F_SET(sp, F_ISSET(orig, S_SCREENS));
	}

	if (xaw_screen_copy(orig, sp))		/* Init S_VI_XAW screen. */
		goto err;
	if (svi_screen_copy(orig, sp))		/* Init S_VI_CURSES screen. */
		goto err;
	if (sex_screen_copy(orig, sp))		/* Init S_EX screen. */
		goto err;
	if (v_screen_copy(orig, sp))		/* Init vi. */
		goto err;
	if (ex_screen_copy(orig, sp))		/* Init ex. */
		goto err;

	*spp = sp;
	return (0);

err:	screen_end(sp);
	return (1);
}

/*
 * screen_end --
 *	Release a screen.
 */
int
screen_end(sp)
	SCR *sp;
{
	int rval;

	rval = 0;
	if (xaw_screen_end(sp))			/* End S_VI_XAW screen. */
		rval = 1;
	if (svi_screen_end(sp))			/* End S_VI_CURSES screen. */
		rval = 1;
	if (sex_screen_end(sp))			/* End S_EX screen. */
		rval = 1;
	if (v_screen_end(sp))			/* End vi. */
		rval = 1;
	if (ex_screen_end(sp))			/* End ex. */
		rval = 1;

	/* Free FREF's. */
	{ FREF *frp;
		while ((frp = sp->frefq.cqh_first) != (FREF *)&sp->frefq) {
			CIRCLEQ_REMOVE(&sp->frefq, frp, q);
			if (frp->name != NULL)
				free(frp->name);
			if (frp->tname != NULL)
				free(frp->tname);
			FREE(frp, sizeof(FREF));
		}
	}

	/* Free file names. */
	{ char **ap;
		if (!F_ISSET(sp, S_ARGNOFREE) && sp->argv != NULL) {
			for (ap = sp->argv; *ap != NULL; ++ap)
				free(*ap);
			free(sp->argv);
		}
	}

	/* Free any text input. */
	text_lfree(&sp->__tiq);

	/* Free any script information. */
	if (F_ISSET(sp, S_SCRIPT))
		sscr_end(sp);

	/* Free alternate file name. */
	if (sp->alt_name != NULL)
		free(sp->alt_name);

	/* Free up search information. */
	if (sp->repl != NULL)
		FREE(sp->repl, sp->repl_len);
	if (sp->newl != NULL)
		FREE(sp->newl, sp->newl_len);

	/* Free all the options */
	opts_free(sp);

	/*
	 * Free the message chain last, so previous failures have a place
	 * to put messages.  Copy messages to (in order) a related screen,
	 * any screen, the global area.
	 */
	{ SCR *c_sp; MSG *mp, *next;
		if ((c_sp = sp->q.cqe_prev) != (void *)&sp->gp->dq) {
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else if ((c_sp = sp->q.cqe_next) != (void *)&sp->gp->dq) {
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else if ((c_sp =
		    sp->gp->hq.cqh_first) != (void *)&sp->gp->hq) {
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else {
			c_sp = NULL;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(sp->gp, G_BELLSCHED);
		}

		for (mp = sp->msgq.lh_first; mp != NULL; mp = next) {
			if (!F_ISSET(mp, M_EMPTY))
				msg_app(sp->gp, c_sp,
				    mp->flags & M_INV_VIDEO, mp->mbuf, mp->len);
			next = mp->q.le_next;
			if (mp->mbuf != NULL)
				FREE(mp->mbuf, mp->blen);
			FREE(mp, sizeof(MSG));
		}
	}

	/* Remove the screen from the displayed queue. */
	SIGBLOCK(sp->gp);
	CIRCLEQ_REMOVE(&sp->gp->dq, sp, q);
	SIGUNBLOCK(sp->gp);

	/* Free the screen itself. */
	FREE(sp, sizeof(SCR));

	return (rval);
}
