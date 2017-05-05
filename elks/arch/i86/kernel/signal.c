/*
 *  linux/arch/i386/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Modified for ELKS 1998-1999 Al Riddoch
 */

#include <linuxmt/types.h>
#include <linuxmt/config.h>
#include <linuxmt/sched.h>
#include <linuxmt/mm.h>
#include <linuxmt/kernel.h>
#include <linuxmt/signal.h>
#include <linuxmt/errno.h>
#include <linuxmt/wait.h>
#include <linuxmt/debug.h>

#include <arch/segment.h>

int do_signal(void)
{
    register __ptask currentp = current;
    register __sighandler_t *sah;
    unsigned signr;
    sigset_t mask;

    signr = 1;
    mask = (sigset_t)1;
    while ((currentp->signal &= (((sigset_t)1 << NSIG) - 1))) {
	if (currentp->signal & mask) {
	    currentp->signal &= ~mask;

	    debug2("Process %d has signal %d.\n", currentp->pid, signr);
	    sah = &currentp->sig.action[signr].sa_handler;
	    if ((*sah == SIG_IGN) || (currentp->pid == 1))	/* Ignore */
		continue;
	    if (*sah == SIG_DFL) {				/* Default */
		if (mask &					/* Stop */
			(SM_SIGSTOP | SM_SIGTSTP | SM_SIGTTIN | SM_SIGTTOU)) {
		    currentp->state = TASK_STOPPED;
		    /* Let the parent know */
		    currentp->p_parent->child_lastend = currentp->pid;
		    currentp->p_parent->lastend_status = (int) signr;
		    schedule();
		}
		else if (!(mask &			/* Core or Terminate */
			(SM_SIGCONT | SM_SIGCHLD | SM_SIGWINCH | SM_SIGURG))) {
#if 0
		    if (mask &					/* Core */
		      (SM_SIGQUIT|SM_SIGILL|SM_SIGABRT|SM_SIGFPE|SM_SIGSEGV|SM_SIGTRAP))
			dump_core();
#endif
		    do_exit((int) signr);			/* Terminate */
		}
		/* else	Ignore */
	    }
	    else {						/* Set handler */
		debug1("Setting up return stack for sig handler %x.\n",(unsigned)(*sah));
		debug1("Stack at %x\n", currentp->t_regs.sp);
		arch_setup_sighandler_stack(currentp, *sah, signr);
		debug1("Stack at %x\n", currentp->t_regs.sp);
		*sah = SIG_DFL;
		currentp->signal = 0;

		return 1;
	    }
	}
	signr++;
	mask <<= 1;
    }
    return 0;
}

