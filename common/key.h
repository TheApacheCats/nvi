/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.8 1992/10/10 13:58:11 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:58:11 $
 */

#define	K_CR		1
#define	K_ESCAPE	2
#define	K_NL		3
#define	K_VERASE	4
#define	K_VKILL		5
#define	K_VLNEXT	6
#define	K_VWERASE	7
extern u_char special[];			/* Special characters. */

#define	GB_BS		0x01		/* Backspace erases past command. */
#define	GB_ESC		0x02		/* Escape executes command. */
#define	GB_NL		0x04		/* Leave the newline on. */
#define	GB_NLECHO	0x08		/* Echo the newline. */
#define	GB_OFF		0x10		/* Leave first buffer char empty. */

#define	ISQ(off)	qb[(off)]
#define	QINIT		bzero(qb, cblen);
#define	QSET(off)	qb[(off)] = 1

extern u_long cblen;			/* Buffer lengths. */
extern u_char *qb;			/* Quote buffer. */

extern u_char *atkeybuf;		/* Base of shared at buffer. */
extern u_char *atkeyp;			/* Next char of shared at buffer. */
extern u_long atkeybuflen;		/* Remaining keys in shared @ buffer. */

int	gb __P((int, u_char **, size_t *, u_int));
int	gb_inc __P((void));
void	gb_init __P((void));

/*
 * Vi makes the screen ready for ex to print, but there are special ways that
 * information gets displayed.   The output overwrites some ex commands, so
 * in that case we erase the command line, outputting a '\r' to guarantee the
 * first column.  (We could theoretically lose if the command line has already
 * wrapped, but this should only result in additional characters being sent to
 * the terminal.)  All other initial output lines are preceded by a '\n'.
 */
/* Start the sequence. */
#define	EX_PRSTART(overwrite) {						\
	if (mode == MODE_VI) {						\
		if (ex_prstate == PR_NONE) {				\
			if (overwrite) {				\
				while (ex_prerase--) {			\
					(void)putchar('\b');		\
					(void)putchar(' ');		\
					(void)putchar('\b');		\
				}					\
				(void)putchar('\r');			\
				ex_prstate = PR_STARTED;		\
			} else {					\
				(void)putchar('\n');			\
				ex_prstate = PR_PRINTED;		\
			}						\
		}  else if (ex_prstate == PR_STARTED) {			\
			(void)putchar('\n');				\
			ex_prstate = PR_PRINTED;			\
		}							\
	} else								\
		(void)putchar('\n');					\
}

/* Print a newline. */
#define	EX_PRNEWLINE {							\
	(void)putchar('\n');						\
	ex_prstate = PR_PRINTED;					\
}

/* Print a trailing newline, if necessary. */
#define	EX_PRTRAIL {							\
	if (mode != MODE_VI || ex_prstate == PR_PRINTED)		\
		(void)putchar('\n');					\
}
