/*
 * This file is part of the ELKS TCP/IP stack
 *
 * (C) 2001 Harry Kalogirou (harkal@rainbow.cs.unipi.gr)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

/*
 * TODO : IP fragmentation and reassemply of fragmented IP packets
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "ip.h"
#include "icmp.h"
#include "slip.h"
#include "tcp.h"
#include "tcpdev.h"
#include <linuxmt/arpa/inet.h>
#include "deveth.h"
#include "arp.h"

#if 0
#define IP_VERSION(s)	((s)->version_ihl>>4&0xf)
#define IP_IHL(s)	((s)->version_ihl&0xf)
#define IP_FLAGS(s)	((s)->frag_off>>13)
#endif

//#define USE_ASM

static char ipbuf[TCPDEV_BUFSIZE];

int ip_init(void)
{
    return 0;
}

#ifndef USE_ASM
__u16 ip_calc_chksum(char *data, int len)
{
    __u32 sum = 0;
    int i;
    __u16 *p = (__u16 *) data;

    len >>= 1;

    for (i=0; i < len ; i++){
	sum += *p++;
    }

    return ~((sum & 0xffff) + ((sum >> 16) & 0xffff));
}
#else
#asm
/*__u16 ip_calc_chksum(char *data, int len)*/
	.text
	.globl _ip_calc_chksum
_ip_calc_chksum:

	push	bp
	mov	bp,sp
	push	di

	mov	cx, 6[bp]
	shr	cx, 1
	shr	cx, 1
	mov	di, 4[bp]
	xor	ax, ax
loop1:
	adc	ax, [di]
	inc di
	inc di
	adc	ax, [di]
	inc di
	inc di

        loop	loop1

	adc	ax, 0
	not	ax

	pop di
	pop bp

	ret
#endasm


#endif

void ip_print(struct iphdr_s *head)
{
#if 0
    int i;
    __u8 *addr;

    debug_ip("Version/IHL: %d/%d ",IP_VERSION(head),IP_IHL(head));
    debug_ip("TypeOfService/Length: %d/%d\n",head->tos,ntohs(head->tot_len));
    debug_ip("Id/flags/fragment offset: %d/%d ",head->id,head->frag_off);
    debug_ip("ttl: %d ",head->ttl);
    debug_ip("Protocol: %d\n",head->protocol);

    addr = (__u8 *)&head->saddr;
    debug_ip("saddr : %d.%d.%d.%d ",addr[0],addr[1],addr[2],addr[3]);
    addr = (__u8 *)&head->daddr;
    debug_ip("daddr : %d.%d.%d.%d ",addr[0],addr[1],addr[2],addr[3]);

    debug_ip("check sum = %d\n",ip_calc_chksum(head, 4 * IP_IHL(head)));

    addr = (__u8 *)head + 4 * IP_IHL(head);
    for ( i = 0 ; i < ntohs(head->tot_len) - 20 ; i++ )
	debug_ip("%x ",addr[i]);

    debug_ip("\n");
#endif
}

void ip_recvpacket(char *packet,int size)
{
    struct iphdr_s *iphdr;
    __u8 *data;

    iphdr = (struct iphdr_s *)packet;

    debug_ip("IP: Got packet size %d\n",size);
    ip_print(iphdr);

    if (IP_VERSION(iphdr) != 4){
        debug_ip("IP : Bad IP version\n");
	return;
    }

    if (IP_IHL(iphdr) < 5){
        debug_ip("IP : Bad IHL\n");
	return;
    }

    data = packet + 4 * IP_IHL(iphdr);

    switch (iphdr->protocol) {
    case PROTO_ICMP:
        debug_ip("IP: icmp packet\n");
	icmp_process(iphdr, data);
	break;

    case PROTO_TCP:
        debug_ip("IP: tcp packet\n");
	tcp_process(iphdr);
	break;
    }
}

void ip_sendpacket(char *packet,int len,struct addr_pair *apair)
{
    struct iphdr_s *iph = (struct iphdr_s *)ipbuf;
    __u16 tlen;
    char llbuf[15];
    struct ip_ll *ipll = (struct ip_ll *)llbuf;
    ipaddr_t ip_addr;
    eth_addr_t eth_addr;

    unsigned char *addr = (unsigned char *) &apair->daddr;
    debug_ip("IPSEND daddr: %d.%d.%d.%d\n", addr [0], addr [1], addr [2], addr [3]);

    if (apair->daddr == local_ip || apair->daddr == 0x0100007f) //FIXME
	goto local;

    if (dev->type == 1) {  /* Ethernet */
debug_ip("ETH ROUTE\n");
        /* Is this the best place for the IP routing to happen ? */
        /* I think no, because actual sending interface is coming from the routing */

        if ((local_ip & netmask_ip) != (apair->daddr & netmask_ip))
            /* Not on the same local network */
            /* Route to the gateway as local destination */
            ip_addr = gateway_ip;
        else
            /* On the same local network */
            /* Route to the local destination */
            ip_addr = apair->daddr;

        /* The ARP transaction should occur before sending the IP packet */
        /* So this part should be moved upward in the IP protocol automaton */
        /* to avoid this dangerous unlimited try again loop */

        while (arp_cache_get (ip_addr, eth_addr))
            /* MAC not in cache */
            /* Issue an ARP request to get it */
            /* Until issue jbruchon#67 fixed, we block until ARP reply */
            arp_request (ip_addr);

    /*link layer*/

    /* The Ethernet header should be built by the Ethernet module */
    /* So this part should be moved downward */

    memcpy(ipll->ll_eth_dest, eth_addr, 6);
    memcpy(ipll->ll_eth_src,eth_local_addr, 6);
    ipll->ll_type_len=0x08; /*=0x0800 bigendian*/
    } //if (dev->type == 1)

local:
    /*ip layer*/
    iph->version_ihl	= 0x45;

    tlen = 4 * IP_IHL(iph);

    iph->tos 		= 0;
    iph->tot_len	= htons(tlen + len);
    iph->id		= 0;
    iph->frag_off 	= 0;
    iph->ttl		= 64;
    iph->protocol	= apair->protocol;

    if (dev->type == 1) {
      iph->daddr		= apair->daddr;
      iph->saddr		= local_ip;
//iph->saddr		= apair->saddr;
    } else {
      iph->daddr		= apair->daddr;
      iph->saddr		= apair->saddr;
    }

    iph->check		= 0;
    iph->check		= ip_calc_chksum((char *)iph, tlen);

    ip_print(iph);

    memcpy(&ipbuf[tlen], packet, len);

    /* "route"  127.0.0.1*/
    if (iph->daddr == local_ip || iph->daddr == 0x0100007f) {
	debug_ip("ip: route localhost\n");
	ip_recvpacket(ipbuf, tlen + len);
	return;
    }

    if (dev->type == 1) { /*add link layer*/
      memmove(&ipbuf[14],ipbuf, TCPDEV_BUFSIZE-14);
      memcpy(ipbuf,llbuf,14);
    }
    if (dev->type == 1)
	deveth_send(ipbuf, tlen + len + 14);  /* add link layer length */
    else
	slip_send(ipbuf, tlen + len);
}
