/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_msg.c,v 10.27 1995/11/05 13:12:06 bostic Exp $ (Berkeley) $Date: 1995/11/05 13:12:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "vi.h"

#define	SCROLL_EXWAIT	0x01		/* User must enter cont. char or :. */
#define	SCROLL_QUIT	0x02		/* User can enter 'q' to interrupt. */
#define	SCROLL_WAIT	0x04		/* User must enter cont. char. */

static void	vs_divider __P((SCR *));
static void	vs_output __P((SCR *, mtype_t, const char *, int));
static void	vs_msgsave __P((SCR *, mtype_t, char *, size_t));
static void	vs_scroll __P((SCR *, CHAR_T *, u_int));

/*
 * vs_busy --
 *	Display, update or clear a busy message.
 *
 * This routine is the default editor interface for vi busy messages.  It
 * implements a standard strategy of stealing lines from the bottom of the
 * vi text screen.  Screens using an alternate method of displaying busy
 * messages, e.g. X11 clock icons, should set their scr_busy function to the
 * correct function before calling the main editor routine.
 *
 * PUBLIC: void vs_busy __P((SCR *, const char *, busy_t));
 */
void
vs_busy(sp, msg, btype)
	SCR *sp;
	const char *msg;
	busy_t btype;
{
	GS *gp;
	VI_PRIVATE *vip;
	static const char flagc[] = "|/-|-\\";
	struct timeval tv;
	size_t len, notused;
	const char *p;

	/* Ex doesn't display busy messages. */
	if (F_ISSET(sp, S_EX))
		return;

	gp = sp->gp;
	vip = VIP(sp);

	/*
	 * Most of this routine is to deal with the screen sharing real estate
	 * between the normal edit messages and the busy messages.  Logically,
	 * all that's needed is something that puts up a message, periodically
	 * updates it, and then goes away.
	 */
	switch (btype) {
	case BUSY_ON:
		++vip->busy_ref;
		if (vip->totalcount != 0 || vip->busy_ref != 1)
			break;

		/* Initialize state for updates. */
		vip->busy_ch = 0;
		(void)gettimeofday(&vip->busy_tv, NULL);

		/* Save the current cursor. */
		(void)gp->scr_cursor(sp, &vip->busy_oldy, &vip->busy_oldx);

		/* Display the busy message. */
		p = msg_cat(sp, msg, &len);
		(void)gp->scr_move(sp, LASTLINE(sp), 0);
		(void)gp->scr_addstr(sp, p, len);
		(void)gp->scr_cursor(sp, &notused, &vip->busy_fx);
		(void)gp->scr_clrtoeol(sp);
		(void)gp->scr_move(sp, LASTLINE(sp), vip->busy_fx);
		break;
	case BUSY_OFF:
		if (vip->busy_ref == 0)
			break;
		--vip->busy_ref;

		/*
		 * If the line isn't in use for another purpose, clear it.
		 * Always return to the original position.
		 */
		if (vip->totalcount == 0 && vip->busy_ref == 0) {
			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			(void)gp->scr_clrtoeol(sp);
		}
		(void)gp->scr_move(sp, vip->busy_oldy, vip->busy_oldx);
		break;
	case BUSY_UPDATE:
		if (vip->totalcount != 0 || vip->busy_ref == 0)
			break;

		/* Update no more than every 1/4 of a second. */
		(void)gettimeofday(&tv, NULL);
		if (((tv.tv_sec - vip->busy_tv.tv_sec) * 1000000 +
		    (tv.tv_usec - vip->busy_tv.tv_usec)) < 4000)
			return;

		/* Display the update. */
		if (vip->busy_ch == sizeof(flagc))
			vip->busy_ch = 0;
		(void)gp->scr_move(sp, LASTLINE(sp), vip->busy_fx);
		(void)gp->scr_addstr(sp, flagc + vip->busy_ch++, 1);
		(void)gp->scr_move(sp, LASTLINE(sp), vip->busy_fx);
		break;
	}
	(void)gp->scr_refresh(sp, 0);
}

/*
 * vs_update --
 *	Update a command.
 *
 * PUBLIC: void vs_update __P((SCR *, char *, char *));
 */
void
vs_update(sp, m1, m2)
	SCR *sp;
	char *m1, *m2;
{
	GS *gp;
	size_t len, mlen, oldy, oldx;

	if (!F_ISSET(sp, S_SCREEN_READY))
		return;

	/*
	 * This routine displays a message on the bottom line of the screen,
	 * without updating any of the command structues that would keep it
	 * there for any period of time, i.e. it is overwritten immediately.
	 *
	 * It's used by the ex read and ! commands when the user's command is
	 * expanded, and by the ex substitution confirmation prompt.
	 */
	if (F_ISSET(sp, S_EX)) {
		(void)ex_printf(sp,
		    "%s\n", m1 == NULL? "" : m1, m2 == NULL ? "" : m2);
		(void)ex_fflush(sp);
	}

	gp = sp->gp;
	(void)gp->scr_cursor(sp, &oldy, &oldx);
	(void)gp->scr_move(sp, LASTLINE(sp), 0);
	(void)gp->scr_clrtoeol(sp);

	/*
	 * XXX
	 * Don't let long file names screw up the screen.
	 */
	if (m1 != NULL) {
		mlen = len = strlen(m1);
		if (len > sp->cols - 2)
			mlen = len = sp->cols - 2;
		(void)gp->scr_addstr(sp, m1, mlen);
	}
	if (m2 != NULL) {
		mlen = strlen(m2);
		if (len + mlen > sp->cols - 2)
			mlen = (sp->cols - 2) - len;
		(void)gp->scr_addstr(sp, m2, mlen);
	}

	(void)gp->scr_move(sp, oldy, oldx);
	(void)sp->gp->scr_refresh(sp, 0);
}

/*
 * vs_msg --
 *	Display ex output or error messages for the screen.
 *
 * This routine is the default editor interface for all ex output, and all ex
 * and vi error/informational messages.  It implements the standard strategy
 * of stealing lines from the bottom of the vi text screen.  Screens using an
 * alternate method of displaying messages, e.g. dialog boxes, should set their
 * scr_msg function to the correct function before calling the editor.
 *
 * PUBLIC: void vs_msg __P((SCR *, mtype_t, char *, size_t));
 */
void
vs_msg(sp, mtype, line, len)
	SCR *sp;
	mtype_t mtype;
	char *line;
	size_t len;
{
	GS *gp;
	VI_PRIVATE *vip;
	size_t cols, oldx, oldy, padding;
	const char *e, *s, *t;

	/* If no text is supplied, flush any buffered output. */
	if (line == NULL) {
		if (F_ISSET(sp, S_EX))
			(void)fflush(stdout);
		return;
	}

	/* If the screen isn't ready to display messages, save it. */
	if (!F_ISSET(sp, S_SCREEN_READY)) {
		(void)vs_msgsave(sp, mtype, line, len);
		return;
	}

	/* Ring the bell if it's scheduled. */
	gp = sp->gp;
	if (F_ISSET(gp, G_BELLSCHED)) {
		F_CLR(gp, G_BELLSCHED);
		(void)gp->scr_bell(sp);
	}

	/*
	 * Display ex output or error messages for the screen.  If the tty
	 * has not yet entered into ex canonical mode, do so.  Set a flag so
	 * that we know it happened for when we enter (or reenter) vi mode.
	 *
	 * XXX
	 * THIS IS WAY WRONG -- WE HAVE NO WAY TO RETURN THE ERROR!!!
	 */
	if (F_ISSET(sp, S_EX)) {
		if (!F_ISSET(sp, S_SCREEN_READY) &&
		    gp->scr_screen(sp, S_EX))
			return;
		F_SET(sp, S_EX_WROTE | S_SCREEN_READY);
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 1);
		(void)printf("%.*s", (int)len, line);
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 0);
		return;
	}

	/* Save the cursor position. */
	(void)gp->scr_cursor(sp, &oldy, &oldx);

	/*
	 * If the message type is changing, terminate any previous message
	 * and move to a new line.
	 */
	vip = VIP(sp);
	if (mtype != vip->mtype) {
		if (vip->lcontinue != 0) {
			if (vip->mtype == M_NONE)
				vs_output(sp, vip->mtype, "\n", 1);
			else
				vs_output(sp, vip->mtype, ".\n", 2);
			vip->lcontinue = 0;
		}
		vip->mtype = mtype;
	}

	/* If it's an ex output message, just write it out. */
	if (mtype == M_NONE) {
		vs_output(sp, mtype, line, len);
		goto ret;
	}

	/*
	 * Need up to three padding characters normally; a semi-colon and
	 * two separating spaces.  If only a single line on the screen, add
	 * some more for the trailing continuation message.
	 *
	 * XXX
	 * Assume that a semi-colon takes up a single column on the screen.
	 */
	if (IS_ONELINE(sp))
		(void)msg_cmsg(sp, CMSG_CONT_S, &padding);
	else
		padding = 0;
	padding += 3;

	/*
	 * If a message won't fit on a single line, try to split on a <blank>.
	 * If a subsequent message fits on the same line, write a separator
	 * and output it.  Otherwise, put out a newline.
	 *
	 * XXX
	 * There are almost certainly pathological cases that will break this
	 * code.
	 */
	if (vip->lcontinue != 0)
		if (len + vip->lcontinue + padding >= sp->cols)
			vs_output(sp, mtype, ".\n", 2);
		else  {
			vs_output(sp, mtype, ";", 1);
			vs_output(sp, M_NONE, "  ", 2);
		}
	for (cols = sp->cols - padding, s = line; len > 0; s = t) {
		for (; isblank(*s) && --len != 0; ++s);
		if (len == 0)
			break;
		if (len > cols) {
			for (e = s + cols; e > s && !isblank(*e); --e);
			if (e == s)
				 e = t = s + cols;
			else
				for (t = e; isblank(e[-1]); --e);
		} else {
			e = s + len;
			/*
			 * XXX:
			 * If t isn't initialized for "s = t", len will be
			 * equal to 0.  Shut the freakin' compiler up.
			 */
			t = s;
		}
		len -= e - s;
		if ((e - s) > 1 && s[(e - s) - 1] == '.')
			--e;
		vs_output(sp, mtype, s, e - s);
		if (len != 0)
			vs_output(sp, M_NONE, "\n", 1);
		if (INTERRUPTED(sp))
			break;
	}

ret:	/* Restore the cursor position. */
	(void)gp->scr_move(sp, oldy, oldx);

	/* Refresh the screen. */
	(void)gp->scr_refresh(sp, 0);
}

/*
 * vs_output --
 *	Output the text to the screen.
 */
static void
vs_output(sp, mtype, line, llen)
	SCR *sp;
	mtype_t mtype;
	const char *line;
	int llen;
{
	GS *gp;
	VI_PRIVATE *vip;
	size_t notused;
	int len, rlen, tlen;
	const char *p, *t;

	gp = sp->gp;
	vip = VIP(sp);
	for (p = line, rlen = llen; llen > 0;) {
		/* Get the next physical line. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/*
		 * The max is sp->cols characters, and we may have already
		 * written part of the line.
		 */
		if (len + vip->lcontinue > sp->cols)
			len = sp->cols - vip->lcontinue;

		/*
		 * If the first line output, do nothing.  If the second line
		 * output, draw the divider line.  If drew a full screen, we
		 * remove the divider line.  If it's a continuation line, move
		 * to the continuation point, else, move the screen up.
		 */
		if (vip->lcontinue == 0) {
			if (!IS_ONELINE(sp)) {
				if (vip->totalcount == 1) {
					(void)gp->scr_move(sp,
					    LASTLINE(sp) - 1, 0);
					(void)gp->scr_clrtoeol(sp);
					(void)vs_divider(sp);
					F_SET(vip, VIP_DIVIDER);
					++vip->totalcount;
					++vip->linecount;
				}
				if (vip->totalcount == sp->t_maxrows &&
				    F_ISSET(vip, VIP_DIVIDER)) {
					--vip->totalcount;
					--vip->linecount;
					F_CLR(vip, VIP_DIVIDER);
				}
			}
			if (vip->totalcount != 0)
				vs_scroll(sp, NULL, SCROLL_QUIT);

			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			++vip->totalcount;
			++vip->linecount;

			if (INTERRUPTED(sp)) {
				vip->lcontinue =
				    vip->linecount = vip->totalcount = 0;
				break;
			}
		} else
			(void)gp->scr_move(sp, LASTLINE(sp), vip->lcontinue);

		/* Display the line, doing character translation. */
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 1);
		for (t = line, tlen = len; tlen--; ++t)
			(void)gp->scr_addstr(sp,
			    KEY_NAME(sp, *t), KEY_LEN(sp, *t));
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 0);

		/* Clear the rest of the line. */
		(void)gp->scr_clrtoeol(sp);

		/* If we loop, it's a new line. */
		vip->lcontinue = 0;

		/* Reset for the next line. */
		line += len;
		llen -= len;
		if (p != NULL) {
			++line;
			--llen;
		}
	}

	/* Set up next continuation line. */
	if (p == NULL)
		gp->scr_cursor(sp, &notused, &vip->lcontinue);
}

/*
 * vs_ex_wrchk --
 *	Check and wait for ex.
 *
 * PUBLIC: int vs_ex_wrchk __P((SCR *));
 */
int
vs_ex_wrchk(sp)
	SCR *sp;
{
	const char *p;
	EVENT ev;
	size_t len;

	/*
	 * Ex sets S_EX_WROTE depending on whether or not the ex command
	 * should be waited for.
	 *
	 * XXX
	 * We're ignoring most errors or illegal events.
	 */
	if (F_ISSET(sp, S_EX_WROTE)) {
		p = msg_cmsg(sp, CMSG_CONT, &len);
		(void)write(STDOUT_FILENO, p, len);
		do {
			if (v_event_get(sp, &ev, 0, 0))
				return (1);
		} while (ev.e_event != E_CHARACTER);
		F_CLR(sp, S_EX_WROTE);
	}
	return (0);
}

/*
 * vs_ex_resolve --
 *	Deal with ex message output.
 *
 * This routine is called when exiting a colon command to resolve any ex
 * output that may have occurred.  We may be in ex mode when we get here,
 * so return as necessary.
 *
 * PUBLIC: int vs_ex_resolve __P((SCR *, int *));
 */
int
vs_ex_resolve(sp, continuep)
	SCR *sp;
	int *continuep;
{
	CHAR_T ch;
	EVENT ev;
	GS *gp;
	VI_PRIVATE *vip;

	/* Don't continue by default. */
	if (continuep != NULL)
		*continuep = 0;

	/* Report on line modifications. */
	msgq_rpt(sp);

	/* Flush ex messages. */
	(void)ex_fflush(sp);

	/* If in vi and there's 0 or 1 lines of output, simply continue. */
	vip = VIP(sp);
	if (F_ISSET(sp, S_VI) && vip->totalcount < 2)
		return (0);

	/* If we switched into ex mode, return into vi mode. */
	gp = sp->gp;
	if (F_ISSET(sp, S_EX)) {
		/* Check to see if we have to wait for ex. */
		if (vs_ex_wrchk(sp))
			return (1);
		if (gp->scr_screen(sp, S_VI))
			return (1);
		F_SET(sp, S_SCR_REDRAW);
	} else {
		/* Put up the return-to-continue message and wait. */
		vs_scroll(sp,
		    &ch, continuep == NULL ? SCROLL_WAIT : SCROLL_EXWAIT);
		if (continuep != NULL && ch == ':') {
			*continuep = 1;
			return (0);
		}

		/* If ex changed the underlying screen, redraw from scratch. */
		if (F_ISSET(vip, VIP_N_REDRAW))
			F_SET(sp, S_SCR_REDRAW);
	}

	if (F_ISSET(sp, S_SCR_REDRAW)) {
		if (vs_refresh(sp))
			return (1);

		/* Reset the count of overwriting lines. */
		vip->linecount = vip->lcontinue = vip->totalcount = 0;
	} else {
		/* Set up the redraw of the overwritten lines. */
		ev.e_event = E_REPAINT;
		ev.e_flno = vip->totalcount >=
		    sp->rows ? 1 : sp->rows - vip->totalcount;
		ev.e_tlno = sp->rows;

		/* Reset the count of overwriting lines. */
		vip->linecount = vip->lcontinue = vip->totalcount = 0;

		/* Redraw. */
		(void)vs_repaint(sp, &ev);
	}
	return (0);
}

/*
 * vs_resolve --
 *	Deal with message output.
 *
 * This routine is called from the main vi loop to periodically ensure that
 * the user has seen any messages that have been displayed.
 *
 * PUBLIC: int vs_resolve __P((SCR *));
 */
int
vs_resolve(sp)
	SCR *sp;
{
	EVENT ev;
	GS *gp;
	MSG *mp;
	VI_PRIVATE *vip;
	size_t oldy, oldx;
	int redraw;

	/* Save the cursor position. */
	gp = sp->gp;
	(void)gp->scr_cursor(sp, &oldy, &oldx);

	/* Ring the bell if it's scheduled. */
	if (F_ISSET(gp, G_BELLSCHED)) {
		F_CLR(gp, G_BELLSCHED);
		(void)gp->scr_bell(sp);
	}

	/* Flush any saved messages. */
	if (gp->msgq.lh_first != NULL && F_ISSET(sp, S_SCREEN_READY))
		while ((mp = gp->msgq.lh_first) != NULL) {
			gp->scr_msg(sp, mp->mtype, mp->buf, mp->len);
			LIST_REMOVE(mp, q);
			free(mp->buf);
			free(mp);
		}

	/* Display new file status line. */
	if (F_ISSET(sp, S_STATUS)) {
		F_CLR(sp, S_STATUS);
		msgq_status(sp, sp->lno, 0);
	}

	/* Report on line modifications. */
	msgq_rpt(sp);

	vip = VIP(sp);
	switch (vip->totalcount) {
	case 0:
		redraw = 0;
		break;
	case 1:
		redraw = 0;

		/* Skip the modeline if it's in use. */
		F_SET(vip, VIP_S_MODELINE);
		break;
	default:
		/*
		 * If >1 message line in use, prompt the user to continue and
		 * repaint overwritten lines.
		 */
		vs_scroll(sp, NULL, SCROLL_WAIT);

		ev.e_event = E_REPAINT;
		ev.e_flno = vip->totalcount >=
		    sp->rows ? 1 : sp->rows - vip->totalcount;
		ev.e_tlno = sp->rows;
		redraw = 1;
		break;
	}

	/* Reset the count of overwriting lines. */
	vip->linecount = vip->lcontinue = vip->totalcount = 0;

	/* Redraw. */
	if (redraw)
		(void)vs_repaint(sp, &ev);

	/* Restore the cursor position. */
	(void)gp->scr_move(sp, oldy, oldx);

	return (0);
}

/*
 * vs_scroll --
 *	Scroll the screen for output.
 */
static void
vs_scroll(sp, chp, flags)
	SCR *sp;
	CHAR_T *chp;
	u_int flags;
{
	EVENT ev;
	GS *gp;
	VI_PRIVATE *vip;
	size_t len;
	const char *p;

	gp = sp->gp;
	vip = VIP(sp);
	if (!IS_ONELINE(sp)) {
		/*
		 * Scroll the screen.  Instead of scrolling the entire screen,
		 * delete the line above the first line output so preserve the
		 * maximum amount of the screen.
		 */
		(void)gp->scr_move(sp, vip->totalcount <
		    sp->rows ? LASTLINE(sp) - vip->totalcount : 0, 0);
		(void)gp->scr_deleteln(sp);

		/* If there are screens below us, push them back into place. */
		if (sp->q.cqe_next != (void *)&sp->gp->dq) {
			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			(void)gp->scr_insertln(sp);
		}
	}

	/* If just displayed a full screen, wait. */
	if (vip->linecount == sp->t_maxrows ||
	    LF_ISSET(SCROLL_EXWAIT | SCROLL_WAIT)) {
		if (IS_ONELINE(sp))
			p = msg_cmsg(sp, CMSG_CONT_S, &len);
		else {
			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			p = msg_cmsg(sp,
			    LF_ISSET(SCROLL_QUIT) ? CMSG_CONT_Q :
			    LF_ISSET(SCROLL_EXWAIT) ? CMSG_CONT_EX : CMSG_CONT,
			    &len);
		}
		(void)gp->scr_addstr(sp, p, len);

		++vip->totalcount;
		vip->linecount = 0;

		(void)gp->scr_clrtoeol(sp);
		(void)gp->scr_refresh(sp, 0);

		/*
		 * Get a single character from the terminal.
		 *
		 * XXX
		 * We're ignoring any errors or illegal events.
		 */
		if (v_event_get(sp, &ev, 0, 0) || ev.e_event != E_CHARACTER) {
			if (chp != NULL)
				*chp = CH_QUIT;
			F_SET(gp, G_INTERRUPTED);
		} else {
			if (chp != NULL)
				*chp = ev.e_c;
			if (LF_ISSET(SCROLL_QUIT) && ev.e_c == CH_QUIT)
				F_SET(gp, G_INTERRUPTED);
		}
	}
}

/*
 * vs_divider --
 *	Draw a dividing line between the screen and the output.
 */
static void
vs_divider(sp)
	SCR *sp;
{
	GS *gp;
	size_t len;

#define	DIVIDESTR	"+=+=+=+=+=+=+=+"
	len =
	    sizeof(DIVIDESTR) - 1 > sp->cols ? sp->cols : sizeof(DIVIDESTR) - 1;
	gp = sp->gp;
	(void)gp->scr_attr(sp, SA_INVERSE, 1);
	(void)gp->scr_addstr(sp, DIVIDESTR, len);
	(void)gp->scr_attr(sp, SA_INVERSE, 0);
}

/*
 * vs_msgsave --
 *	Save a message for later display.
 */
static void
vs_msgsave(sp, mt, p, len)
	SCR *sp;
	mtype_t mt;
	char *p;
	size_t len;
{
	GS *gp;
	MSG *mp_c, *mp_n;

	/*
	 * We have to handle messages before we have any place to put them.
	 * If there's no screen support yet, allocate a msg structure, copy
	 * in the message, and queue it on the global structure.  If we can't
	 * allocate memory here, we're genuinely screwed, dump the message
	 * to stderr in the (probably) vain hope that someone will see it.
	 */
	CALLOC_GOTO(sp, mp_n, MSG *, 1, sizeof(MSG));
	MALLOC_GOTO(sp, mp_n->buf, char *, len);

	memmove(mp_n->buf, p, len);
	mp_n->len = len;
	mp_n->mtype = mt;

	gp = sp->gp;
	if ((mp_c = gp->msgq.lh_first) == NULL) {
		LIST_INSERT_HEAD(&gp->msgq, mp_n, q);
	} else {
		for (; mp_c->q.le_next != NULL; mp_c = mp_c->q.le_next);
		LIST_INSERT_AFTER(mp_c, mp_n, q);
	}
	return;

alloc_err:
	if (mp_n != NULL)
		free(mp_n);
	(void)fprintf(stderr, "%.*s\n", (int)len, p);
}
