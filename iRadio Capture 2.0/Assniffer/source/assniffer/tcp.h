/*
    assniffer - tcp.h (TCP connection analysis)
    Copyright (C) 2004 Cockos Incorporated

    assniffer is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    assniffer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with assniffer; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _ASSNIFF_TCP_H_
#define _ASSNIFF_TCP_H_

// IP/TCP header format structures
#pragma pack(push, 1)

typedef struct
{
  unsigned char IHL:4, version:4;
  unsigned char ToS;
  unsigned short total_len;
  unsigned short ident;
  unsigned short fragoffs:12, flags:4;
  unsigned char ttl;
  unsigned char proto;
  unsigned short hdr_checksum;
  unsigned int source_addr;
  unsigned int dest_addr;
  unsigned int padding:8, options:24;
} IP_header;

#define	TH_FIN	1
#define	TH_SYN	2

typedef struct
{
	unsigned short	source_port;
	unsigned short	dest_port;
	unsigned int    sequence;
  unsigned int    ack_number;
  unsigned char th_x2:4;
  unsigned char th_offs:4;
  unsigned char th_flags;
	unsigned short th_win;
	unsigned short th_sum;
	unsigned short th_urp;
} TCP_header;

#pragma pack(pop)



void handle_tcp(IP_header *iphdr, TCP_header *tcphdr, unsigned char *payload, int payload_len);


#endif//_ASSNIFF_TCP_H_

