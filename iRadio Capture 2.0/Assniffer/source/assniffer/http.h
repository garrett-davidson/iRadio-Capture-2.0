/*
    assniffer - http.h (HTTP connection analysis)
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


#ifndef _ASSNIFF_HTTP_H_
#define _ASSNIFF_HTTP_H_

extern int g_config_usemime; // 1=append, 2=replace extension
extern int g_config_debugfn;
extern int g_config_subdirs;
extern int g_config_splitbyclient;
extern char *g_config_mimetype;
extern char *g_config_leadpath;
extern int g_config_cookie;


class HTTP_context
{
public:
  HTTP_context(int concnt);
  ~HTTP_context();

  // returns bytes used
  int onRequestStream(unsigned char *payload, int payload_len);

  int isDone();

  // returns amount used, if any
  int onReplyData(unsigned char *payload, int payload_len, unsigned int m_src_ip);

  char m_mime_type[64];
	char m_mime_subtype[64];
  int m_fail,m_bytes_timeout_scanget,m_bytes_timeout_scanreply;
  int m_hasfoundget;
  int m_ischunked, m_chunkpos;
  int m_content_length,m_content_length_orig;
  FILE *m_fp;
  char m_fn[1024];
  char m_host[128];
  char m_cookie[4096];
  int m_hasfoundhttpreply;
  int m_is_gz;
  int m_done;
  int m_lookforhdr;
  int m_http_reply_code;
  int m_concnt;
  char finalfn[2048];
  char finalfn_by_client[2048];
	unsigned int m_src_ip;
	
  void doWrite(void *buf, int len, unsigned int m_src_ip);
};


#endif //_ASSNIFF_HTTP_H_
