/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	"$Id: vi_mextern.h,v 8.3 1996/12/18 10:27:19 bostic Exp $ (Berkeley) $Date: 1996/12/18 10:27:19 $";
 */

/*
 * Globals, the list of names exposed to the outside world by the vi Motif
 * widget library.
 *
 * Applications using the Motif vi widget code will have to initialize these
 * or the library code will fail.
 */
extern char *vi_progname;			/* Program name. */
extern int   vi_ofd;				/* Output file descriptor. */

/*
 * RAZ -- anyway we can get the library to do this setup?
 */
#ifdef __STDC__
Widget	vi_create_editor(String, Widget, void (*)(void));
Widget	vi_create_menubar(Widget);  
void	vi_input_func(XtPointer, int *, XtInputId *);
int	vi_run(int, char *[], int *, int *, pid_t *);
int	vi_send(char *, IP_BUF *);
int	vi_translate(char *, size_t *);
#else
Widget	vi_create_editor();
Widget	vi_create_menubar();
void	vi_input_func();
void	vi_run();
int	vi_send();
int	vi_translate();
#endif