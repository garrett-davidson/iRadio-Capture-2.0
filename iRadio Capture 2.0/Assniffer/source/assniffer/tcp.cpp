/*
    assniffer - tcp.cpp (TCP connection analysis)
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
#include "../WDL/ptrlist.h"
#include "../WDL/queue.h"
#include "http.h"
#include "tcp.h"

#define TWINDOW_SIZE 65536
#define CONNECTION_TIMEOUT 15 // seconds



class TCPCtx // monitors a TCP connection
{
public:
  TCPCtx(unsigned int src_ip, unsigned int dest_ip, unsigned short src_port, unsigned short dest_port);
  ~TCPCtx();
  void flushReplyBufs();
  void flushReqBufs();
  int onPacket(int dir, IP_header *iphdr, TCP_header *tcphdr, unsigned char *payload, int payload_len);

  HTTP_context *m_http;

  // if a reply comes in and it's replybuf_window_min, we just move 
  // replybuf_window_in up, and directly add the data.
  // otherwise, if it's between min and max, we add it in.
  // if it's >= max, we append it to replybuf, moving replybuf_window_max up,
  // and if we have to, we move min up, and copy that old data.
  // whenever we move up min, we should zero it out, too.
  // this shoudl be redone and we should track which pieces are missing, too.
  // oh well, TODO or something
  // also need to flush on timeout or on fins.

  WDL_Queue replybuf;
  unsigned char replybuf_window[TWINDOW_SIZE];
  unsigned int replybuf_window_min, replybuf_window_max;
  WDL_Queue reqbuf; 
  unsigned char reqbuf_window[TWINDOW_SIZE];
  unsigned int reqbuf_window_min, reqbuf_window_max;

  unsigned int m_src_ip, m_dest_ip;
  unsigned short m_src_port, m_dest_port;
  int m_had_fin;
  time_t m_last_time;
  int m_concnt;
};



TCPCtx::TCPCtx(unsigned int src_ip, unsigned int dest_ip, unsigned short src_port, unsigned short dest_port)
{
  m_concnt=0;
  m_src_ip=src_ip;
  m_dest_ip=dest_ip;
  m_src_port=src_port;
  m_dest_port=dest_port;
  m_had_fin=0;
  replybuf_window_min=replybuf_window_max=0;
	reqbuf_window_min=reqbuf_window_max=0;
  time(&m_last_time);
  m_http=0;
}

TCPCtx::~TCPCtx()
{
  if (m_http) delete m_http;
  m_http=0;
}

void TCPCtx::flushReplyBufs()
{
  if (replybuf_window_max > replybuf_window_min)
  {
    for (;replybuf_window_min < replybuf_window_max; replybuf_window_min++)
      replybuf.Add(&replybuf_window[replybuf_window_min & (TWINDOW_SIZE-1)],1);
  }
}

void TCPCtx::flushReqBufs()
{
  if (reqbuf_window_max > reqbuf_window_min)
  {
    for (;reqbuf_window_min < reqbuf_window_max; reqbuf_window_min++)
      reqbuf.Add(&reqbuf_window[reqbuf_window_min & (TWINDOW_SIZE-1)],1);
  }
}


int TCPCtx::onPacket(int dir, IP_header *iphdr, TCP_header *tcphdr, unsigned char *payload, int payload_len)
{
  if (tcphdr)
  {
    unsigned int seq=ntohl(tcphdr->sequence);
    if (tcphdr->th_flags & TH_SYN)
    {
      if (dir > 0) 
      {          
			  flushReqBufs();
		    reqbuf_window_min=reqbuf_window_max=seq+1;
      }
      else
      {
        //printf("got SYN from server, this should happen once per connection, you'd think (%u)\n",seq);
        flushReplyBufs();
        replybuf_window_min=replybuf_window_max=seq+1;
      }
    }

    if (payload_len>0)//else
    {
      if (dir > 0)
      {
        if (!reqbuf_window_min) 
        {
          reqbuf_window_min=reqbuf_window_max=seq;
        }

        if (seq == reqbuf_window_min)
        {
          //printf("got an in-order request payload (%u->%u %d bytes)\n",seq,seq+payload_len,payload_len);
          if (payload_len > 0) reqbuf.Add(payload,payload_len);
          reqbuf_window_min += payload_len;
          if (reqbuf_window_max < reqbuf_window_min) reqbuf_window_max=reqbuf_window_min;
        }
        else if (seq > reqbuf_window_min && seq+payload_len < reqbuf_window_max)
        {
          int x;
          for (x = 0; x < payload_len; x++)
            reqbuf_window[(seq+x) & (TWINDOW_SIZE-1)]=payload[x];
        }
        else if (seq > reqbuf_window_min && seq+payload_len >= reqbuf_window_max && seq < reqbuf_window_max + TWINDOW_SIZE)
        {
          if ((seq+payload_len)-reqbuf_window_min > TWINDOW_SIZE)
          {
            unsigned int endp = seq+payload_len - TWINDOW_SIZE;
            if (endp > reqbuf_window_min  + TWINDOW_SIZE) endp = reqbuf_window_min + TWINDOW_SIZE;

            for (;reqbuf_window_min < endp; reqbuf_window_min++)
              reqbuf.Add(&reqbuf_window[reqbuf_window_min & (TWINDOW_SIZE-1)],1);
          }

          int x;
          for (x = 0; x < payload_len; x ++)
          {
            reqbuf_window[(seq+x)&(TWINDOW_SIZE-1)]=payload[x];
          }
          reqbuf_window_max=seq+payload_len;
        }
      }
      else
      {
        if (!replybuf_window_min) 
        {
          //printf("replybuf_window_min=0, setting seq to %u\n",seq);
          replybuf_window_min=replybuf_window_max=seq;
        }

        if (seq == replybuf_window_min)
        {
          //printf("got an in-order reply payload (%u->%u %d bytes,  %dboo)\n",seq,seq+payload_len,payload_len,tcphdr->th_offs);
          if (payload_len > 0) replybuf.Add(payload,payload_len);
          replybuf_window_min += payload_len;
          if (replybuf_window_max < replybuf_window_min) replybuf_window_max=replybuf_window_min;
        }
        else if (seq > replybuf_window_min && seq+payload_len < replybuf_window_max)
        {
          //printf("this sux\n");
          int x;
          for (x = 0; x < payload_len; x++)
            replybuf_window[(seq+x) & (TWINDOW_SIZE-1)]=payload[x];
        }
        else if (seq > replybuf_window_min && seq+payload_len >= replybuf_window_max && seq < replybuf_window_max + TWINDOW_SIZE) // should use the window field of the TCP header here (and above), but oh well
        {
          //printf("this also sux (seq=%u, replybuf_window_min=%u)\n",seq,replybuf_window_min);
          if ((seq+payload_len)-replybuf_window_min > TWINDOW_SIZE)
          {
            unsigned int endp = seq+payload_len - TWINDOW_SIZE;
            if (endp > replybuf_window_min  + TWINDOW_SIZE) endp = replybuf_window_min + TWINDOW_SIZE;

            for (;replybuf_window_min < endp; replybuf_window_min++)
              replybuf.Add(&replybuf_window[replybuf_window_min & (TWINDOW_SIZE-1)],1);
          }

          int x;
          for (x = 0; x < payload_len; x ++)
          {
            replybuf_window[(seq+x)&(TWINDOW_SIZE-1)]=payload[x];
          }
          replybuf_window_max=seq+payload_len;
        }
      }
    }
    if (tcphdr->th_flags & TH_FIN)
    {
      m_had_fin |= dir>0 ? 1 : 2;
      if (dir < 0) flushReplyBufs();
      if (dir > 0) flushReqBufs();
    }
  }

  while (replybuf.Available() > 0)
  {
    int tryagain=0;

    if (!m_http) m_http = new HTTP_context(m_concnt++);

    if (!m_http->m_hasfoundget && reqbuf.Available()>0)
    {
      int l=m_http->onRequestStream((unsigned char *)reqbuf.Get(),reqbuf.Available());
      if (l>0)
      {
        reqbuf.Advance(l);
        reqbuf.Compact();
      }
      if (m_http->m_fail) return 1;
    }

    if (m_http->m_hasfoundget)
    {
      int l=m_http->onReplyData((unsigned char *)replybuf.Get(),replybuf.Available(),m_src_ip);       
      if (l > 0)
      {
        replybuf.Advance(l);
        replybuf.Compact();
        tryagain++;
      }
    }

    if (m_http->m_fail) return 1;

    if (m_http->isDone()) 
    {
      delete m_http;
      m_http=0;
      tryagain++;
    }

    if (!tryagain) break;
  }

  return (m_http && m_http->m_fail) || m_had_fin==3;
}





///////////////////////////////////////////////

static WDL_PtrList<TCPCtx> conlist;

void handle_tcp(IP_header *iphdr, TCP_header *tcphdr, unsigned char *payload, int payload_len)
{
  int x;

  time_t now;
  time(&now);

  for (x = 0; x < conlist.GetSize(); x ++)
  {
    TCPCtx *t=conlist.Get(x);
    if (now - t->m_last_time > CONNECTION_TIMEOUT)
    {
      t->flushReplyBufs();
      t->flushReqBufs();
      t->onPacket(-1,NULL,NULL,NULL,0);
      delete t;
      conlist.Delete(x--);
      //printf("Timed out connection\n");
    }
  }

  for (x = 0; x < conlist.GetSize(); x ++)
  {
    TCPCtx *t=conlist.Get(x);
    int dir=0;
    if (t->m_dest_ip == iphdr->dest_addr &&
        t->m_src_ip == iphdr->source_addr &&
        t->m_dest_port == tcphdr->dest_port &&
        t->m_src_port == tcphdr->source_port)
    {
      dir=1;
    }
    else  if (t->m_src_ip == iphdr->dest_addr &&
        t->m_dest_ip == iphdr->source_addr &&
        t->m_src_port == tcphdr->dest_port &&
        t->m_dest_port == tcphdr->source_port)
    {
      dir=-1;
    }


    if (dir)
    {
      //printf("got %d byte ethernet packet off wire (%d byte TCP payload)!\n",orig_len,payload_len);
      t->m_last_time = now;

      if (t->onPacket(dir,iphdr,tcphdr,payload,payload_len))
      {
        conlist.Delete(x);
        delete t;
    //    printf("Fin'd connection\n");
      }
      return;
    }
  }

  // woo, new connection
  if (tcphdr->th_flags & TH_SYN)
  {
    TCPCtx *ctx = new TCPCtx(iphdr->source_addr,iphdr->dest_addr,tcphdr->source_port,tcphdr->dest_port);
    ctx->reqbuf_window_min=ctx->reqbuf_window_max=ntohl(tcphdr->sequence)+1;
    conlist.Add(ctx);
  //  printf("Added connection (%d)\n",conlist.GetSize());
  }

}
