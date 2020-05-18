/*
 * This file is part of the ELKS TCP/IP stack
 *
 * (C) 2001 Harry Kalogirou (harkal@rainbow.cs.unipi.gr)
 *
 * Major debugging by Greg Haerr May 2020
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "slip.h"
#include "tcp.h"
#include "tcp_output.h"
#include "tcp_cb.h"
#include "tcpdev.h"
#include "timer.h"
#include <linuxmt/arpa/inet.h>
#include "ip.h"
#include "icmp.h"
#include "netconf.h"
#include "deveth.h"
#include "arp.h"

ipaddr_t local_ip;
ipaddr_t gateway_ip;
ipaddr_t netmask_ip;

char deveth[] = "/dev/eth";

static int intfd;	/* interface fd*/

void ktcp_run(void)
{
    fd_set fdset;
    struct timeval timeint, *tv;
    int count;
extern int tcp_timeruse;
extern int cbs_in_time_wait;
extern int cbs_in_user_timeout;

    while (1) {
	//if (tcp_timeruse > 0 || tcpcb_need_push > 0) {
	if (tcp_timeruse > 0 || tcpcb_need_push > 0 || cbs_in_time_wait > 0 || cbs_in_user_timeout > 0) {

	    timeint.tv_sec  = 1;
	    timeint.tv_usec = 0;
	    tv = &timeint;
	} else tv = NULL;

	FD_ZERO(&fdset);
	FD_SET(intfd, &fdset);
	FD_SET(tcpdevfd, &fdset);
	count = select(intfd > tcpdevfd ? intfd + 1 : tcpdevfd + 1, &fdset, NULL, NULL, tv);
	if (count < 0)
		return;	//FIXME

	Now = timer_get_time();

	/* expire timeouts and push data*/
	tcp_update();

	/* process received packets*/
	if (FD_ISSET(intfd, &fdset)) {
		if (eth_device)
			deveth_process();
		else slip_process();
	}

	/* process application socket actions*/
	if (FD_ISSET(tcpdevfd, &fdset))
		tcpdev_process();

	/* check for retransmit needed*/
	if (tcp_timeruse > 0)
		tcp_retrans();

	tcpcb_printall();
    }
}

int main(int argc,char **argv)
{
    int daemon = 0;
    speed_t baudrate = 0;
    char *progname = argv[0];

    if (argc > 1 && !strcmp("-b", argv[1])) {
	daemon = 1;
	argc--;
	argv++;
    }
    if (argc > 1 && !strcmp("-s", argv[1])) {
	if (argc <= 2) goto usage;
	baudrate = atol(argv[2]);
	argc -= 2;
	argv += 2;
    }
    if (argc < 3) {
usage:
	printf("Usage: %s [-b] [-s baud] local_ip [interface] [gateway] [netmask]\n", progname);
	exit(3);
    }

    local_ip = in_aton(argv[1]);

    if (argc > 3) gateway_ip = in_aton(argv[3]);
    else gateway_ip = in_aton("10.0.2.2"); /* use dhcp when implemented */

    if (argc > 4) netmask_ip = in_aton(argv[4]);
    else netmask_ip = in_aton("255.255.255.0");

    printf("ktcp: ip %s, ", in_ntoa(local_ip));
    printf("gateway %s, ", in_ntoa(gateway_ip));
    printf("netmask %s\n", in_ntoa(netmask_ip));

    /* must exit() in next two stages on error to reset kernel tcpdev_inuse to 0*/
    if ((tcpdevfd = tcpdev_init("/dev/tcpdev")) < 0)
	exit(1);

    if (strcmp(argv[2],deveth) == 0) {
	if ((intfd = deveth_init(deveth)) < 0)
	    exit(1);
	eth_device = 1;
    } else {
	if ((intfd = slip_init(argv[2], baudrate)) < 0)
	    exit(2);
    }

    /* become daemon now that tcpdev_inuse race condition over*/
    if (daemon) {
	int fd;
	if (fork())
	    exit(0);
	close(0);
	/* redirect messages to console*/
	fd = open("/dev/console", O_WRONLY);
	dup2(fd, 1);		/* required for printf output*/
	dup2(fd, 2);
    }

    arp_init ();
    ip_init();
    icmp_init();
    tcp_init();
    netconf_init();

    ktcp_run();

    exit(0);
}

unsigned long in_aton(const char *str)
{
    unsigned long l = 0;
    unsigned int val;
    int i;

    for (i = 0; i < 4; i++) {
	l <<= 8;
	if (*str != '\0') {
	    val = 0;
	    while (*str != '\0' && *str != '.') {
		val *= 10;
		val += *str++ - '0';
	    }
	    l |= val;
	    if (*str != '\0')
		str++;
	}
    }
    return htonl(l);
}

char *
in_ntoa(ipaddr_t in)
{
    register unsigned char *p;
    static char b[18];

    p = (unsigned char *)&in;
    sprintf(b, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
    return b;
}
