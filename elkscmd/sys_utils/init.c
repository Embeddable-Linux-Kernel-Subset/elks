/*
 * init  A System-V init Clone.
 *
 * Usage: /bin/init
 *       init [0123456]
 *
 * 2001-08-23  Harry Kalogirou
 *	Many bug fixes. In general made it finaly work.
 *	Also corrected the C formating.
 *
 * 1999-11-07  mario.frasca@home.ict.nl
 *
 *  Copyright 1999 Mario Frasca
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmp.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>
#include <memory.h>
#include <errno.h>
#include <time.h>

#define USE_UTMP	0	/* =1 to use /var/run/utmp*/
#define DEBUG		0	/* =1 for debug messages*/

#if !DEBUG
#define debug(...)
#define debug2(...)
#endif

#define FLAG_RESPAWN   1
#define FLAG_WAIT      2
#define RUNLEVELS     12
#define BUFSIZE      256
#define INITTAB      "/etc/inittab"
#define INITLVL      "/etc/initlvl"
#define SHELL        "/bin/sh"
#define GETTY        "/bin/getty"
#define DEVTTY       "/dev/tty1"
#define MAXCHILD     8

/* 'hashed' strings */
#define RESPAWN      362
#define WAIT         122
#define ONCE          62
#define BOOT         147
#define BOOTWAIT     465
#define POWERFAIL    403
#define POWERFAILNOW 951
#define POWERWAIT    577
#define POWEROKWAIT  829
#define CTRLALTDEL   504
#define OFF           39
#define ONDEMAND     240
#define INITDEFAULT  707
#define SYSINIT      398
#define KBREQUEST    622

/*
  each of the entries from inittab corresponds to a child, each of which:
  has a unique 2 chars identifier.
  is allowed to run in some run-levels.
  might need to be waited for completion when spawned.
  might be running or not (pid different from zero).
  might have to been respawned.
  was respawned at a certain point in time.

  for each running child, we keep the pid assigned to a given id.
  We take this information each time from the INITTAB file.
*/
struct tabentry {
	char id[3];
	pid_t pid;
};

static char *initname;			/* full path to this program*/
static struct tabentry children[MAXCHILD], *nextchild=children, *thisOne;
static char runlevel;
static char prevRunlevel;
#if USE_UTMP
static struct utmp utentry;
#endif

/* Print an error message and die*/
static void fatalmsg(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "%s: ", initname);
	vfprintf(stderr, msg, args);
	va_end(args);
	exit(1);
}

#if DEBUG
static void debug(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "%s: ", initname);
	vfprintf(stderr, msg, args);
	va_end(args);
}

static void debug2(const char *str)
{
	fprintf(stderr, "%s", str);
}
#endif

void parseLine(const char* line, void func())
{
	char *a[4];
	int k = 0;
	char buf[256], *p;

	strcpy(buf, line);
	a[k++] = p = buf;
	while (*p <= ' ') {
		if (*p == 0) return;
		p++;
	}
	if (*p == '#') return;
	while (k < 4) {
		/* looking for the k-th ':' */
		while (*p && *p != ':') p++;
		*p = 0;
		a[k++] = ++p;
	}
	func(a);
}

void scanFile(void func())
{
	int f, left;
	char buf[BUFSIZE], *line, *next;

	f = open(INITTAB, O_RDONLY);
	if (-1 == f) fatalmsg("Missing %s\n", INITTAB);

	left = read(f, buf, BUFSIZE);
	line = strtok(buf, "\n");
	next = strtok(NULL, "\n");

	while (1) {
		if (!next) {
			if (line == buf) goto out;
			memmove(buf, line, left);
			left += read(f, buf+left, BUFSIZE-left);
			line = buf;
			next = strtok(buf, "\n");
		}
		else {
			parseLine(line, func);

			left -= next-line;
			line = next;
			next = strtok(NULL, "\n");
		}
	}
out:
	close(f);
}

/* returns a pointer to the child or NULL */
struct tabentry *matchPid(pid_t pid)
{
	struct tabentry *i=nextchild;
	while (i!=children)
		if ((--i)->pid == pid) return i;
	return 0;
}

/* returns a pointer to the child or NULL */
struct tabentry *matchId(const char *id)
{
	struct tabentry *i = nextchild;
	while (i!=children)
		if (!strcmp((--i)->id, id)) return i;
	return 0;
}


/* appends child information in the array */
void appendChild (const char *id, pid_t pid)
{
	if (MAXCHILD == nextchild-children)
		fatalmsg("too many children\n");
	memcpy(nextchild->id, id, 2);
	nextchild->id[2] = 0;
	nextchild->pid = pid;
	nextchild++;
}

/* removes child information from the array */
void removeChild(struct tabentry *pos)
{
	if (pos-children >= nextchild-children)
		fatalmsg("nonexistent child\n");
	memcpy(pos, --nextchild, sizeof (struct tabentry));
}

void doSleep(int sec)
{
/*
 struct timeval tv;

 tv.tv_sec = sec;
 tv.tv_usec = 0;

 while (select(0, NULL, NULL, NULL, &tv) < 0 && errno == EINTR)
  ;
 */
}

pid_t respawn(const char **a)
{
    int pid;
    char *argv[5], buf[128];
    int fd;
    char *devtty;

    if (a[3] == NULL) return 1;
    debug("spawning '%s'\n", a[3]);

    pid = fork();
    if (-1 == pid) fatalmsg("No fork\n");

    if (0 == pid) {
	setsid();
#if DEBUG
	close(0);
	close(1);
	close(2);
#endif
	strcpy(buf, a[3]);
	if (!strncmp(buf, GETTY, sizeof GETTY -1)) {
	    devtty = strchr(buf, ' ');

	    if (!devtty) fatalmsg("Bad getty line\n");
	    *(devtty++) = 0;
	    if ((fd = open(devtty, O_RDWR)) < 0) fatalmsg("Can't open %s\n", devtty);

	    dup2(fd ,STDIN_FILENO);
	    dup2(fd ,STDOUT_FILENO);
	    dup2(fd ,STDERR_FILENO);

	    argv[0] = GETTY;
	    argv[1] = devtty;
	    argv[2] = NULL;
	    execv(argv[0], argv);
	}
	else
	{
	    if ((fd = open(DEVTTY, O_RDWR)) < 0) fatalmsg("Can't open %s\n", DEVTTY);

	    dup2(fd ,STDIN_FILENO);
	    dup2(fd ,STDOUT_FILENO);
	    dup2(fd ,STDERR_FILENO);

	    argv[0] = SHELL;
	    argv[1] = "-e";
	    argv[2] = buf;
	    argv[3] = strtok(buf, " ");
	    argv[4] = NULL;
		execv(argv[0], argv);
	}

	fatalmsg("exec failed: %s\n", argv[0]);
    }
    debug("process owns %d\n", pid);

/* here I must do something about utmp */
    return pid;
}

int hash(const char *string)
{
	const char *p;
	int result = 0, i=1;

	p = string;
	while (*p)
		result += ((*p++)-'a')*(i++);

	return result;
}

void passOne(const char **a)
{
	pid_t pid;

	switch (hash(a[2])) {
	case INITDEFAULT:
		runlevel = a[1][0];
		break;

	case SYSINIT:
		pid = respawn(a);
		while (pid != wait(NULL));
		break;

	default:
	/* ignore */
		;
	}
}

void getRunlevel(char **a)
{
	if (INITDEFAULT == hash(a[2])) runlevel = a[1][0];
}

void exitRunlevel(char **a)
{
	debug("%s:%s:%s:%s ", a[0], a[1], a[2], a[3]);

	if (a[1][0] && !strchr(a[1], runlevel)) {
		struct tabentry *child;

		debug2("stop ");

		/* if running, terminate it gently */
		child = matchId(a[0]);
		if (!child) {
			debug2("not running\n");
			return;
		}
		if (!child->pid) {
			debug2("not running\n");
			return;
		}
		kill(child->pid, SIGTERM);

		doSleep(2);                  /* give it the time */

		/* if still running, kill it right away */
		child = matchId(a[0]);
		if (!child) return;
		if (!child->pid) return;
		kill(child->pid, SIGKILL);
	}
	debug2("\n");
}

void enterRunlevel(const char **a)
{
	pid_t pid;

	debug("%s:%s:%s:%s ", a[0], a[1], a[2], a[3]);

	if (!a[1][0] || strchr(a[1], runlevel)) {
		int andWait=0;

		/* if not running, spawn it */
		if (!matchId(a[0])) {
			switch (hash(a[2])) {
			case WAIT:
				andWait = 1;
			case RESPAWN:
			case ONCE:
				pid = respawn(a);
				if (andWait) while (pid != wait(NULL));
				else appendChild(a[0], pid);
				break;
			default:
				debug2("discarded\n");
			}
		} else debug2("already running\n");
	}
}

void spawnThisOne(const char **a)
{
	if (!strncmp(a[0], thisOne->id, 2)) {
		switch (hash(a[2])) {
		case RESPAWN:
		case ONDEMAND:
			thisOne->pid = respawn(a);
			break;
		default:
			removeChild(thisOne);
		}
#if USE_UTMP
		strcpy(utentry.ut_line, strstr(a[3], "/tty")+1);
		time(&utentry.ut_time);
		utentry.ut_id[0] = a[0][0];
		utentry.ut_id[1] = a[0][1];
		utentry.ut_pid = thisOne->pid;
		setutent();
		pututline(&utentry);
		endutent();
#endif
	}
}

void handle_signal(int sig)
{
	debug("signaled\n");
	switch(sig) {
		case SIGHUP:
		/* got signaled by another instance of init, change runlevel! */
		{
			prevRunlevel = runlevel;
			scanFile(getRunlevel);

			if (runlevel != prevRunlevel) {
				/* -stop all running children not needed in new run-level */
				scanFile(exitRunlevel);
				/* -start all non running children needed in new run-level */
				scanFile(enterRunlevel);
			}
		}
		break;
	}
}

int main(int argc, char **argv)
{
	pid_t pid;

//	argv[0] = "init";
	initname = argv[0];
#if DEBUG
	int fd = open(DEVTTY, O_RDWR);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	debug("starting\n");
#endif

#if USE_UTMP
	memset(&utentry, 0, sizeof(struct utmp));
	utentry.ut_type = INIT_PROCESS;
	time(&utentry.ut_time);
	utentry.ut_id[0] = 'l';
	utentry.ut_id[1] = '1';
	utentry.ut_pid = getpid();
	strcpy(utentry.ut_user, "INIT");
	setutent();
	pututline(&utentry);
#endif
	/* am I the No.1 init? */
	if (getpid() == 1) {
		/*   signal(SIGALRM,  handle_signal); */
		signal(SIGHUP,   handle_signal);
		/*   signal(SIGINT,   handle_signal); */
		/*   signal(SIGCHLD,  handle_signal); */
		/*   signal(SIGPWR,   handle_signal); */
		/*   signal(SIGWINCH, handle_signal); */
		/*   signal(SIGUSR1,  handle_signal); */
		/*   signal(SIGSTOP,  handle_signal); */
		/*   signal(SIGTSTP,  handle_signal); */
		/*   signal(SIGCONT,  handle_signal); */
		/*   signal(SIGSEGV,  handle_signal); */
#if USE_UTMP
		setutent();
#endif
		/* get runlevel & spawn sysinit */
		debug("scan inittab pass 1\n");
		scanFile(passOne);

		/* spawn needed children */
		debug("scan inittab runlevel %c\n", runlevel);
		scanFile(enterRunlevel);
#if USE_UTMP
		endutent();
#endif
		/* wait for signals. */
		while (1) {
			pid = wait(NULL);
			if (pid == -1) continue;

			debug("wait got child %d\n", pid);
			thisOne = matchPid(pid);
			if (!thisOne) continue;

			debug("scan inittab spawn\n");
			scanFile(spawnThisOne);
		}
	} else {
		/* try to store new run-level into /etc/initrunlvl*/
		int f = open(INITLVL, O_WRONLY);
		if (f < 0)
			debug("write '%s' failed\n", INITLVL);
		write(f, argv[1], 1);
		close(f);

		debug("change to runlevel %c\n", argv[1][0]);

		/* signal the first init to switch run level*/
		kill(1, SIGHUP);
	}
	return 0;
}
