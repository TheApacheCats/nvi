/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: args.h,v 9.2 1995/01/11 15:57:56 bostic Exp $ (Berkeley) $Date: 1995/01/11 15:57:56 $
 */

/*
 * Structure for building "argc/argv" vector of arguments.
 *
 * !!!
 * All arguments are nul terminated as well as having an associated length.
 * The argument vector is NOT necessarily NULL terminated.  The proper way
 * to check the number of arguments is to use the argc value in the EXCMDARG
 * structure or to walk the array until an ARGS structure with a length of 0
 * is found.
 */
typedef struct _args {
	CHAR_T	*bp;		/* Argument. */
	size_t	 blen;		/* Buffer length. */
	size_t	 len;		/* Argument length. */

#define	A_ALLOCATED	0x01	/* If allocated space. */
	u_int8_t flags;
} ARGS;
