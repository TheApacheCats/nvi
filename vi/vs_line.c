/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_line.c,v 10.12 1996/02/28 15:46:15 bostic Exp $ (Berkeley) $Date: 1996/02/28 15:46:15 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"
#include "vi.h"

#ifdef VISIBLE_TAB_CHARS
#define	TABCH	'-'
#else
#define	TABCH	' '
#endif

/*
 * vs_line --
 *	Update one line on the screen.
 *
 * PUBLIC: int vs_line __P((SCR *, SMAP *, size_t *, size_t *));
 */
int
vs_line(sp, smp, yp, xp)
	SCR *sp;
	SMAP *smp;
	size_t *xp, *yp;
{
	CHAR_T *kp;
	GS *gp;
	SMAP *tsmp;
	size_t chlen, cols_per_screen, cno_cnt, len, scno, skip_screens;
	size_t offset_in_char, offset_in_line, nlen, oldy, oldx;
	int ch, dne, is_cached, is_partial, is_tab;
	int list_tab, list_dollar;
	char *p, *cbp, *ecbp, cbuf[128];

#if defined(DEBUG) && 0
	TRACE(sp, "vs_line: row %u: line: %u off: %u\n",
	    smp - HMAP, smp->lno, smp->off);
#endif
	/*
	 * If ex modifies the screen after ex output is already on the screen,
	 * don't touch it -- we'll get scrolling wrong, at best.
	 */
	if (!F_ISSET(sp, S_INPUT_INFO) && VIP(sp)->totalcount > 1)
		return (0);
	if (F_ISSET(sp, S_SCR_EXWROTE) && smp - HMAP != LASTLINE(sp))
		return (0);

	/*
	 * Assume that, if the cache entry for the line is filled in, the
	 * line is already on the screen, and all we need to do is return
	 * the cursor position.  If the calling routine doesn't need the
	 * cursor position, we can just return.
	 */
	is_cached = SMAP_CACHE(smp);
	if (yp == NULL && is_cached)
		return (0);

	/*
	 * A nasty side effect of this routine is that it returns the screen
	 * position for the "current" character.  Not pretty, but this is the
	 * only routine that really knows what's out there.
	 *
	 * Move to the line.  This routine can be called by vs_sm_position(),
	 * which uses it to fill in the cache entry so it can figure out what
	 * the real contents of the screen are.  Because of this, we have to
	 * return to whereever we started from.
	 */
	gp = sp->gp;
	(void)gp->scr_cursor(sp, &oldy, &oldx);
	(void)gp->scr_move(sp, smp - HMAP, 0);

	/* Get the line. */
	dne = db_get(sp, smp->lno, 0, &p, &len);

	/*
	 * Special case if we're printing the info/mode line.  Skip printing
	 * the leading number, as well as other minor setup.  If painting the
	 * line between two screens, it's always in reverse video.  The only
	 * time this code paints the mode line is when the user is entering
	 * text for a ":" command, so we can put the code here instead of
	 * dealing with the empty line logic below.  This is a kludge, but it's
	 * pretty much confined to this module.
	 *
	 * Set the number of screens to skip until a character is displayed.
	 * Left-right screens are special, because we don't bother building
	 * a buffer to be skipped over.
	 *
	 * Set the number of columns for this screen.
	 */
	cols_per_screen = sp->cols;
	list_tab = O_ISSET(sp, O_LIST);
	if (F_ISSET(sp, S_INPUT_INFO)) {
		list_dollar = 0;
		if (O_ISSET(sp, O_LEFTRIGHT))
			skip_screens = 0;
		else
			skip_screens = smp->off - 1;
	} else {
		list_dollar = list_tab;
		skip_screens = smp->off - 1;

		/*
		 * If O_NUMBER is set and it's line number 1 or the line exists
		 * and this is the first screen of a folding line or any left-
		 * right line, display the line number.
		 */
		if (O_ISSET(sp, O_NUMBER)) {
			cols_per_screen -= O_NUMBER_LENGTH;
			if ((smp->lno == 1 || !dne) && skip_screens == 0) {
				nlen = snprintf(cbuf,
				    sizeof(cbuf), O_NUMBER_FMT, smp->lno);
				(void)gp->scr_addstr(sp, cbuf, nlen);
			}
		}
	}

	/*
	 * Special case non-existent lines and the first line of an empty
	 * file.  In both cases, the cursor position is 0, but corrected
	 * for the O_NUMBER field if it was displayed.
	 */
	if (dne || len == 0) {
		/* Fill in the cursor. */
		if (yp != NULL && smp->lno == sp->lno) {
			*yp = smp - HMAP;
			*xp = sp->cols - cols_per_screen;
		}

		/* If the line is on the screen, quit. */
		if (is_cached)
			goto ret1;

		/* Set line cacheing information. */
		smp->c_sboff = smp->c_eboff = 0;
		smp->c_scoff = smp->c_eclen = 0;

		/* Lots of special cases for empty lines. */
		if (skip_screens == 0)
			if (dne) {
				if (smp->lno == 1) {
					if (list_dollar) {
						ch = '$';
						goto empty;
					}
				} else {
					ch = '~';
					goto empty;
				}
			} else
				if (list_dollar) {
					ch = '$';
empty:					(void)gp->scr_addstr(sp,
					    KEY_NAME(sp, ch), KEY_LEN(sp, ch));
				}

		(void)gp->scr_clrtoeol(sp);
		(void)gp->scr_move(sp, oldy, oldx);
		return (0);
	}

	/*
	 * If we wrote a line that's this or a previous one, we can do this
	 * much more quickly -- we cached the starting and ending positions
	 * of that line.  The way it works is we keep information about the
	 * lines displayed in the SMAP.  If we're painting the screen in
	 * the forward, this saves us from reformatting the physical line for
	 * every line on the screen.  This wins big on binary files with 10K
	 * lines.
	 *
	 * Test for the first screen of the line, then the current screen line,
	 * then the line behind us, then do the hard work.  Note, it doesn't
	 * do us any good to have a line in front of us -- it would be really
	 * hard to try and figure out tabs in the reverse direction, i.e. how
	 * many spaces a tab takes up in the reverse direction depends on
	 * what characters preceded it.
	 */
	if (smp->off == 1) {
		smp->c_sboff = offset_in_line = 0;
		smp->c_scoff = offset_in_char = 0;
		p = &p[offset_in_line];
	} else if (is_cached) {
		offset_in_line = smp->c_sboff;
		offset_in_char = smp->c_scoff;
		p = &p[offset_in_line];
		if (skip_screens != 0)
			cols_per_screen = sp->cols;
	} else if (smp != HMAP &&
	    SMAP_CACHE(tsmp = smp - 1) && tsmp->lno == smp->lno) {
		if (tsmp->c_eclen != tsmp->c_ecsize) {
			offset_in_line = tsmp->c_eboff;
			offset_in_char = tsmp->c_eclen;
		} else {
			offset_in_line = tsmp->c_eboff + 1;
			offset_in_char = 0;
		}

		/* Put starting info for this line in the cache. */
		smp->c_sboff = offset_in_line;
		smp->c_scoff = offset_in_char;
		p = &p[offset_in_line];
		if (skip_screens != 0)
			cols_per_screen = sp->cols;
	} else {
		offset_in_line = 0;
		offset_in_char = 0;

		/* This is the loop that skips through screens. */
		if (skip_screens == 0) {
			smp->c_sboff = offset_in_line;
			smp->c_scoff = offset_in_char;
		} else for (scno = 0; offset_in_line < len; ++offset_in_line) {
			scno += chlen =
			    (ch = *(u_char *)p++) == '\t' && !list_tab ?
			    TAB_OFF(scno) : KEY_LEN(sp, ch);
			if (scno < cols_per_screen)
				continue;
			scno -= cols_per_screen;

			/*
			 * Reset the cols_per_screen to second and subsequent
			 * line length.
			 */
			cols_per_screen = sp->cols;

			/*
			 * If crossed the last skipped screen boundary, start
			 * displaying the characters.
			 */
			if (--skip_screens)
				continue;

			/* Put starting info for this line in the cache. */
			if (scno) {
				smp->c_sboff = offset_in_line;
				smp->c_scoff = offset_in_char = chlen - scno;
				--p;
			} else {
				smp->c_sboff = ++offset_in_line;
				smp->c_scoff = 0;
			}
			break;
		}
	}

	/*
	 * Set the number of characters to skip before reaching the cursor
	 * character.  Offset by 1 and use 0 as a flag value.  Vs_line is
	 * called repeatedly with a valid pointer to a cursor position.
	 * Don't fill anything in unless it's the right line and the right
	 * character, and the right part of the character...
	 */
	if (yp == NULL ||
	    smp->lno != sp->lno || sp->cno < offset_in_line ||
	    offset_in_line + cols_per_screen < sp->cno) {
		cno_cnt = 0;
		/* If the line is on the screen, quit. */
		if (is_cached)
			goto ret1;
	} else
		cno_cnt = (sp->cno - offset_in_line) + 1;

	/* This is the loop that actually displays characters. */
	ecbp = (cbp = cbuf) + sizeof(cbuf) - 1;
	for (is_partial = 0, scno = 0;
	    offset_in_line < len; ++offset_in_line, offset_in_char = 0) {
		if ((ch = *(u_char *)p++) == '\t' && !list_tab) {
			scno += chlen = TAB_OFF(scno) - offset_in_char;
			is_tab = 1;
		} else {
			scno += chlen = KEY_LEN(sp, ch) - offset_in_char;
			is_tab = 0;
		}

		/*
		 * Only display up to the right-hand column.  Set a flag if
		 * the entire character wasn't displayed for use in setting
		 * the cursor.  If reached the end of the line, set the cache
		 * info for the screen.  Don't worry about there not being
		 * characters to display on the next screen, its lno/off won't
		 * match up in that case.
		 */
		if (scno >= cols_per_screen) {
			smp->c_ecsize = chlen;
			chlen -= scno - cols_per_screen;
			smp->c_eclen = chlen;
			smp->c_eboff = offset_in_line;
			if (scno > cols_per_screen)
				is_partial = 1;

			/* Terminate the loop. */
			offset_in_line = len;
		}

		/*
		 * If the caller wants the cursor value, and this was the
		 * cursor character, set the value.  There are two ways to
		 * put the cursor on a character -- if it's normal display
		 * mode, it goes on the last column of the character.  If
		 * it's input mode, it goes on the first.  In normal mode,
		 * set the cursor only if the entire character was displayed.
		 */
		if (cno_cnt &&
		    --cno_cnt == 0 && (F_ISSET(sp, S_INPUT) || !is_partial)) {
			*yp = smp - HMAP;
			if (F_ISSET(sp, S_INPUT))
				*xp = scno - chlen;
			else
				*xp = scno - 1;
			if (O_ISSET(sp, O_NUMBER) &&
			    !F_ISSET(sp, S_INPUT_INFO) && smp->off == 1)
				*xp += O_NUMBER_LENGTH;

			/* If the line is on the screen, quit. */
			if (is_cached)
				goto ret2;
		}

		/* If the line is on the screen, don't display anything. */
		if (is_cached)
			continue;

#define	FLUSH {								\
	*cbp = '\0';							\
	(void)gp->scr_addstr(sp, cbuf, cbp - cbuf);			\
	cbp = cbuf;							\
}
		/*
		 * Display the character.  We do tab expansion here because
		 * the screen interface doesn't have any way to set the tab
		 * length.  Note, it's theoretically possible for chlen to
		 * be larger than cbuf, if the user set a impossibly large
		 * tabstop.
		 */
		if (is_tab)
			while (chlen--) {
				if (cbp >= ecbp)
					FLUSH;
				*cbp++ = TABCH;
			}
		else {
			if (cbp + chlen >= ecbp)
				FLUSH;
			for (kp = KEY_NAME(sp, ch) + offset_in_char; chlen--;)
				*cbp++ = *kp++;
		}
	}

	if (scno < cols_per_screen) {
		/* If didn't paint the whole line, update the cache. */
		smp->c_ecsize = smp->c_eclen = KEY_LEN(sp, ch);
		smp->c_eboff = len - 1;

		/*
		 * If not the info/mode line, and O_LIST set, and at the
		 * end of the line, and the line ended on this screen,
		 * add a trailing $.
		 */
		if (list_dollar) {
			++scno;

			chlen = KEY_LEN(sp, '$');
			if (cbp + chlen >= ecbp)
				FLUSH;
			for (kp = KEY_NAME(sp, '$'); chlen--;)
				*cbp++ = *kp++;
		}

		/* If still didn't paint the whole line, clear the rest. */
		if (scno < cols_per_screen)
			(void)gp->scr_clrtoeol(sp);
	}

ret2:	if (cbp > cbuf)
		FLUSH;

ret1:	(void)gp->scr_move(sp, oldy, oldx);
	return (0);
}

/*
 * vs_number --
 *	Repaint the numbers on all the lines.
 *
 * PUBLIC: int vs_number __P((SCR *));
 */
int
vs_number(sp)
	SCR *sp;
{
	GS *gp;
	SMAP *smp;
	VI_PRIVATE *vip;
	size_t len, oldy, oldx;
	int exist;
	char nbuf[10];

	/* No reason to do anything if we're in input mode on the info line. */
	if (F_ISSET(sp, S_INPUT_INFO))
		return (0);

	/*
	 * Try and avoid getting the last line in the file, by getting the
	 * line after the last line in the screen -- if it exists, we know
	 * we have to to number all the lines in the screen.  Get the one
	 * after the last instead of the last, so that the info line doesn't
	 * fool us.
	 *
	 * If that test fails, we have to check each line for existence.
	 *
	 * XXX
	 * The problem is that file_lline will lie, and tell us that the
	 * info line is the last line in the file.
	 */
	exist = db_exist(sp, TMAP->lno + 1);

	gp = sp->gp;
	vip = VIP(sp);
	(void)gp->scr_cursor(sp, &oldy, &oldx);

	for (smp = HMAP; smp <= TMAP; ++smp) {
		if (smp->off != 1)
			continue;
		if (smp->lno != 1 && !exist && !db_exist(sp, smp->lno))
			break;
		(void)gp->scr_move(sp, smp - HMAP, 0);
		len = snprintf(nbuf, sizeof(nbuf), O_NUMBER_FMT, smp->lno);
		(void)gp->scr_addstr(sp, nbuf, len);
	}
	(void)gp->scr_move(sp, oldy, oldx);
	return (0);
}
