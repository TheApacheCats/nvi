/*-
 * Copyright (c) 2000
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

static void	   perr __P((char *, char *));

/*
 * gs_init --
 *	Create and partially initialize the GS structure.
 * PUBLIC: GS * gs_init __P((char*));
 */
GS *
gs_init(name)
	char *name;
{
	GS *gp;
	char *p;

	/* Figure out what our name is. */
	if ((p = strrchr(name, '/')) != NULL)
		name = p + 1;

	/* Allocate the global structure. */
	CALLOC_NOMSG(NULL, gp, GS *, 1, sizeof(GS));
	if (gp == NULL)
		perr(name, NULL);

	gp->progname = name;

	/* Common global structure initialization. */
	/* others will need to be copied from main.c */
	CIRCLEQ_INIT(&gp->dq);

	thread_init(gp);

	return (gp);
}

/*
 * gs_new_win
 *	Create new window
 * PUBLIC: WIN * gs_new_win __P((GS *gp));
 */

WIN *
gs_new_win(GS *gp)
{
	WIN *wp;

	CALLOC_NOMSG(NULL, wp, WIN *, 1, sizeof(*wp));
	if (!wp)
		return NULL;

	/* Common global structure initialization. */
	LIST_INIT(&wp->ecq);
	LIST_INSERT_HEAD(&wp->ecq, &wp->excmd, q);

	CIRCLEQ_INSERT_TAIL(&gp->dq, wp, q);
	CIRCLEQ_INIT(&wp->scrq);

	wp->gp = gp;

	return wp;
}

/*
 * win_end --
 *	Remove window.
 *
 * PUBLIC: int win_end __P((WIN *wp));
 */
int
win_end(WIN *wp)
{
	SCR *sp;

	CIRCLEQ_REMOVE(&wp->gp->dq, wp, q);

	while ((sp = wp->scrq.cqh_first) != (void *)&wp->scrq)
		(void)screen_end(sp);

#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	/* Free any temporary space. */
	if (wp->tmp_bp != NULL)
		free(wp->tmp_bp);
#endif

	return 0;
}

/*
 * gs_end --
 *	End the program, discarding screens and most of the global area.
 *
 * PUBLIC: void gs_end __P((GS *));
 */
void
gs_end(gp)
	GS *gp;
{
	MSGS *mp;
	SCR *sp;
	WIN *wp;

	/* If there are any remaining screens, kill them off. */
	if (gp->ccl_sp != NULL) {
		(void)file_end(gp->ccl_sp, NULL, 1);
		(void)screen_end(gp->ccl_sp);
	}
	while ((wp = gp->dq.cqh_first) != (void *)&gp->dq)
		(void)win_end(wp);
	while ((sp = gp->hq.cqh_first) != (void *)&gp->hq)
		(void)screen_end(sp);

#ifdef HAVE_PERL_INTERP
	perl_end(gp);
#endif

#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	{ FREF *frp;
		/* Free FREF's. */
		while ((frp = gp->frefq.cqh_first) != (FREF *)&gp->frefq) {
			CIRCLEQ_REMOVE(&gp->frefq, frp, q);
			if (frp->name != NULL)
				free(frp->name);
			if (frp->tname != NULL)
				free(frp->tname);
			free(frp);
		}
	}

	/* Free key input queue. */
	if (gp->i_event != NULL)
		free(gp->i_event);

	/* Free cut buffers. */
	cut_close(gp);

	/* Free map sequences. */
	seq_close(gp);

	/* Free default buffer storage. */
	(void)text_lfree(&gp->dcb_store.textq);

	/* Close message catalogs. */
	msg_close(gp);
#endif

	/* Ring the bell if scheduled. */
	if (F_ISSET(gp, G_BELLSCHED))
		(void)fprintf(stderr, "\07");		/* \a */

	/*
	 * Flush any remaining messages.  If a message is here, it's almost
	 * certainly the message about the event that killed us (although
	 * it's possible that the user is sourcing a file that exits from the
	 * editor).
	 */
	while ((mp = gp->msgq.lh_first) != NULL) {
		(void)fprintf(stderr, "%s%.*s",
		    mp->mtype == M_ERR ? "ex/vi: " : "", (int)mp->len, mp->buf);
		LIST_REMOVE(mp, q);
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
		free(mp->buf);
		free(mp);
#endif
	}

#if defined(TRACE)
	/* Close tracing file descriptor. */
	vtrace_end();
#endif
}


/*
 * perr --
 *	Print system error.
 */
static void
perr(name, msg)
	char *name, *msg;
{
	(void)fprintf(stderr, "%s:", name);
	if (msg != NULL)
		(void)fprintf(stderr, "%s:", msg);
	(void)fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
}