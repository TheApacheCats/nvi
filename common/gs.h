/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.16 1993/11/07 13:43:50 bostic Exp $ (Berkeley) $Date: 1993/11/07 13:43:50 $
 */

typedef struct _gs {
	struct list_entry screens;	/* Linked list of SCR structures. */
	
	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */
	struct termios
		 s5_curses_botch;	/* System V curses workaround. */

	struct _msg	*msgp;		/* User message list. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Size of temporary buffer. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

/* INFORMATION SHARED BY ALL SCREENS. */
	struct _ibuf	*key;		/* Key input buffer. */
	struct _ibuf	*tty;		/* Tty input buffer. */

	struct list_entry cutq;		/* Cut buffers. */

#define	MAX_BIT_SEQ	128		/* Max + 1 fast check character. */
	struct list_entry seqq;		/* Linked list of maps, abbrevs. */
	bitstr_t bit_decl(seqb, MAX_BIT_SEQ);

#define	G_BELLSCHED	0x0001		/* Bell scheduled. */
#define	G_ISFROMTTY	0x0002		/* Reading from a tty. */
#define	G_RECOVER_SET	0x0004		/* Recover system initialized. */
#define	G_SETMODE	0x0008		/* Tty mode changed. */
#define	G_SIGALRM	0x0010		/* SIGALRM arrived. */
#define	G_SIGHUP	0x0020		/* SIGHUP arrived. */
#define	G_SIGTERM	0x0040		/* SIGTERM arrived. */
#define	G_SIGWINCH	0x0080		/* SIGWINCH arrived. */
#define	G_SLEEPING	0x0100		/* Asleep (die on signal). */
#define	G_SNAPSHOT	0x0200		/* Always snapshot files. */
#define	G_TMP_INUSE	0x0400		/* Temporary buffer in use. */
	u_int	 flags;
} GS;

extern GS *__global_list;		/* List of screens. */
