/*
 *  linux/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linuxmt/debug.h>

#include <linuxmt/types.h>
#include <linuxmt/sched.h>
#include <linuxmt/kernel.h>
#include <linuxmt/signal.h>
#include <linuxmt/errno.h>
#include <linuxmt/wait.h>
#include <linuxmt/mm.h>

#include <arch/segment.h>

static void generate(sig_t sig, sigset_t msksig, register struct task_struct *p)
{
    register __sighandler_t sa;

    sa = p->sig.action[sig - 1].sa_handler;
    if ((sa == SIG_IGN) || ((sa == SIG_DFL) && (msksig &
	    (SM_SIGCONT | SM_SIGCHLD | SM_SIGWINCH | SM_SIGURG)))) {
	if (!(msksig & SM_SIGCHLD)) debug_sig("SIGNAL ignoring %d pid %d\n", sig, p->pid);
	return;
    }
    debug_sig("SIGNAL generate sig %d %x pid %d\n", sig, msksig, p->pid);
    p->signal |= msksig;
    if ((p->state == TASK_INTERRUPTIBLE) /* && (p->signal & ~p->blocked) */ ) {
	debug_sig("SIGNAL wakeup pid %d\n", p->pid);
	wake_up_process(p);
    }
}

int send_sig(sig_t sig, register struct task_struct *p, int priv)
{
    register __ptask currentp = current;
    sigset_t msksig;

    if (sig != SIGCHLD) debug_sig("SIGNAL send_sig %d pid %d.\n", sig, p->pid);
    if (!priv && ((sig != SIGCONT) || (currentp->session != p->session)) &&
	(currentp->euid ^ p->suid) && (currentp->euid ^ p->uid) &&
	(currentp->uid ^ p->suid) && (currentp->uid ^ p->uid) && !suser())
	return -EPERM;
    msksig = (((sigset_t)1) << (sig - 1));
    if (msksig & (SM_SIGKILL | SM_SIGCONT)) {
	if (p->state == TASK_STOPPED)
	    wake_up_process(p);
	p->signal &= ~(SM_SIGSTOP | SM_SIGTSTP | SM_SIGTTIN | SM_SIGTTOU);
    }
    if (msksig & (SM_SIGSTOP | SM_SIGTSTP | SM_SIGTTIN | SM_SIGTTOU))
	p->signal &= ~SM_SIGCONT;
    /* Actually generate the signal */
    generate(sig, msksig, p);
    return 0;
}

int kill_pg(pid_t pgrp, sig_t sig, int priv)
{
    register struct task_struct *p;
    int err = -ESRCH;

    if (!pgrp) {
	debug_sig("SIGNAL kill_pg 0 ignored\n");
	return 0;
    }
    debug_sig("SIGNAL kill_pg %d\n", pgrp);
    for_each_task(p) {
	if (p->pgrp == pgrp) {
		if (p->state < TASK_ZOMBIE)
		    err = send_sig(sig, p, priv);
		else debug_sig("SIGNAL skip kill_pg pgrp %d pid %d state %d\n", pgrp, p->pid, p->state);
	}
    }
    return err;
}

int kill_process(pid_t pid, sig_t sig, int priv)
{
    register struct task_struct *p;

    debug_sig("SIGNAL kill sig %d pid %d\n", sig, pid);
    for_each_task(p)
	if (p->pid == pid)
	    return send_sig(sig, p, 0);
    return -ESRCH;
}

int sys_kill(pid_t pid, sig_t sig)
{
    register __ptask pcurrent = current;
    register struct task_struct *p;
    int count, err, retval;

    if ((unsigned int)(sig - 1) > (NSIG-1))
	return -EINVAL;

    count = retval = 0;
    if (pid == -1) {
	for_each_task(p)
	    if (p->pid > 1 && p != pcurrent) {
		count++;
		if ((err = send_sig(sig, p, 0)) != -EPERM)
		    retval = err;
	    }
	return (count ? retval : -ESRCH);
    }
    if (pid < 1)
	return kill_pg((!pid ? pcurrent->pgrp : -pid), sig, 0);
    return kill_process(pid, sig, 0);
}

int sys_signal(int signr, __sighandler_t handler)
{
    debug_sig("SIGNAL sys_signal %d action %x pid %d\n", signr, handler, current->pid);
    if (((unsigned int)signr > NSIG) || signr == SIGKILL || signr == SIGSTOP)
	return -EINVAL;
    current->sig.action[signr - 1].sa_handler = handler;
    return 0;
}
