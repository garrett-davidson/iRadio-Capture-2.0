/*
    assniffer - sniff.cpp (entry point and capture initialization code)
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "platform.h"

#include "pcap.h"

#include "tcp.h"
#include "http.h" // for config items only

void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data);
int g_config_pcap_file=0;

static void usage()
{
  printf("Usage:\n"
         "\tassniffer output_directory [-r|-d pcap file|deviceindex] [flags (see below)] [-f \"pcap filter\"]\n"
         "  -nopromisc disables promiscuous mode\n"
         "  -nosubdirs does not create subdirectories, uses filename for whole url\n"
         "  -splitbyclient create subdirs by client IP\n"
				 "  Default MIME extension replaces URL's extension, or:\n"
         "     -nomime does not make extensions based on MIME type\n"
         "     -addmime makes MIME extensions appended to URL\n"
         "  Default write all MIME types, or:\n"
				 "     -mimetype \"type\" will analyze only \"type\" MIME (text, application, image, video, ...)\n" 
				 "  -debugfn adds debug info to filenames\n"
         "  -cookie logs all cookie values to a \"cookies.txt\" file\n"
	 "\n");
  exit(1);
}


int main(int argc, char **argv)
{
  int dev_index=0;
  char *pcap_file;
	int promisc=1;
	char *pcap_filter="port 80 || port 8080";
	struct bpf_program fp;
	bpf_u_int32 mask;
	bpf_u_int32 net;
	g_config_mimetype="all";
	
  printf("*******************************************************************************\n");
  printf("assniffer v0.2 alpha, Copyright (C) 2004-2006, Cockos Incorporated\n");
  printf("\t- Patches by Julien Bernard (julien.bernard@gmail.com)\n");
	printf("*******************************************************************************\n");
  printf("\n");

  if (argc < 2) usage();
  g_config_leadpath=argv[1];
	
  // parse command line
  int i;
  for (i=2; i < argc; i ++)
  {
		if (!stricmp(argv[i],"-r"))
		{
		  if (++i >= argc||!(pcap_file=argv[i])) usage();
			g_config_pcap_file=1;
		}
		else if (!stricmp(argv[i],"-d"))
    {
      if (++i >= argc||!(dev_index=atoi(argv[i]))) usage();
    	g_config_pcap_file=0;
		}
		else if (!stricmp(argv[i],"-f"))
		{
			if (++i >= argc||!(pcap_filter=argv[i])) usage();
		}
    else if (!stricmp(argv[i],"-nosubdirs"))
    {
      g_config_subdirs=0;
    }
    else if (!stricmp(argv[i],"-nomime"))
    {
      g_config_usemime=0;
    }
    else if (!stricmp(argv[i],"-addmime"))
    {
      g_config_usemime=1;
    }
    else if (!stricmp(argv[i],"-debugfn"))
    {
      g_config_debugfn=1;
    }
    else if (!stricmp(argv[i],"-nopromisc"))
    {
      promisc=0;
    }
		else if (!stricmp(argv[i],"-splitbyclient"))
		{
		 g_config_splitbyclient=1;
		}
		else if (!stricmp(argv[i],"-mimetype"))
		{
		 if (++i >= argc||!(g_config_mimetype=argv[i])) usage();
		}
		else if (!stricmp(argv[i],"-cookie"))
		{
		 g_config_cookie=1;
		}
    else usage();
  }

	pcap_if_t *alldevs;
	pcap_if_t *d;
	pcap_t *adhandle;
	char errbuf[PCAP_ERRBUF_SIZE];
	
if(g_config_pcap_file == 1) 
{
	if ((adhandle= pcap_open_offline(pcap_file, errbuf)) == NULL)
	{
			printf("Error in pcap file: %s\n", errbuf);
			exit(1);
  }
}
else 
{	
	/* Retrieve the device list */
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		printf("Error in pcap_findalldevs: %s\n", errbuf);
		exit(1);
	}
		
	if(!alldevs)
	{
		printf("\nNo interfaces found! Make sure pcap/Winpcap is installed.\n");
		return -1;
	}


	if (dev_index <= 0)
	{
    int i;

	  /* Print the list */
	  for(i=0,d=alldevs; d; d=d->next)
	  {
		  printf("%d. %s", ++i, d->name);
		  if (d->description) printf(" (%s)\n", d->description);
		  else printf(" (No description available)\n");
	  }
		printf("Enter the interface number (1-%d):",i);
		scanf("%d", &dev_index);
	}
	
	/* Jump to the selected adapter */
	for(d=alldevs, i=0; d && i < dev_index-1 ;d=d->next, i++);

	if (pcap_lookupnet(d->name, &net, &mask, errbuf) == -1) {
							 fprintf(stderr, "Can't get netmask for device %s\n", d->name);
							 		 net = 0;
									 		 mask = 0;
											 	 }
  
	/* Open the adapter */
	if ( !d || (adhandle= pcap_open_live(d->name,	// name of the device
							 65536,		// portion of the packet to capture. 
							 // 65536 grants that the whole packet will be captured on all the MACs.
							 promisc,			// promiscuous mode
							 1000,		// read timeout
							 errbuf		// error buffer
							 ) ) == NULL)
	{
		printf("\nUnable to open the adapter!\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
	
	printf("\nlistening on %s...\n", d->description);

	/* At this point, we don't need any more the device list. Free it */
	pcap_freealldevs(alldevs);
}
	
	if (pcap_compile(adhandle, &fp, pcap_filter, 0, net) == -1) {
		 fprintf(stderr, "Couldn't parse filter %s: %s\n", pcap_filter, pcap_geterr(adhandle));
		 return(2);
	 }
	 if (pcap_setfilter(adhandle, &fp) == -1) {
		 fprintf(stderr, "Couldn't install filter %s: %s\n", pcap_filter, pcap_geterr(adhandle));
		 return(2);
	 }	

	/* start the capture */
	pcap_loop(adhandle, 0, packet_handler, NULL);
	
	return 0;
}



#define ETHLEN 14 // length of ethernet packet headers


/* Callback function invoked by libpcap for every incoming packet */
void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data)
{
//  if (header->caplen < ETHLEN || *(short *)(pkt_data+ETHLEN-2) != 0x8) return; // not IP

  int pkt_len = header->caplen - ETHLEN;
  IP_header *ip_hdr = (IP_header *) (pkt_data + ETHLEN);
  TCP_header *tcp_hdr = (TCP_header *) ((char *)ip_hdr +ip_hdr->IHL*4);
  unsigned char *tcp_payload;

  int ip_len = ntohs(ip_hdr->total_len);

  if (pkt_len < ip_len) return; // incomplete packet

  pkt_len = ip_len - ip_hdr->IHL * 4;

//  printf("IHL=%d,version=%d\n",ip_hdr->IHL,ip_hdr->version);
  if (ip_hdr->version != 4 || ip_hdr->IHL < 5 || pkt_len <= 0) return; // invalid IP header
  if (ip_hdr->proto != 0x6) return; // not TCP

  pkt_len -= tcp_hdr->th_offs*4;
  if (tcp_hdr->th_offs < 5 || pkt_len < 0) return;

  tcp_payload = ((unsigned char *) tcp_hdr) + tcp_hdr->th_offs*4;

  int dport = ntohs(tcp_hdr->dest_port);
  int sport = ntohs(tcp_hdr->source_port);

        handle_tcp(ip_hdr,tcp_hdr,tcp_payload, pkt_len);
}
