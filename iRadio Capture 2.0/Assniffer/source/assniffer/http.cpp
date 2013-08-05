/*
    assniffer - http.cpp (HTTP connection analysis)
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
#include "http.h"

#ifndef WIN32 
    #include <arpa/inet.h> 
#else 
    #include <winsock2.h> 
#endif 

// configurable items of http.cpp
int g_config_usemime=2; // 1=append, 2=replace extension
int g_config_debugfn=0;
int g_config_subdirs=1;
int g_config_splitbyclient=0;
char *g_config_leadpath=NULL;
char *g_config_mimetype="all";
int g_config_cookie=0;

// some utility functions
char* long2ip(unsigned int longip)
{
	 struct in_addr x;
	 x.s_addr = (longip);
	 return inet_ntoa(x);
}

#ifdef _WIN32
char *skip_root(char *path)
{
  char *p = CharNext(path);
  char *p2 = CharNext(p);

  if (*path && *p == ':' && *p2=='\\')
  {
    return CharNext(p2);
  }
  else if (*path == '\\' && *p == '\\')
  {
    // skip host and share name
    int x = 2;
    while (x--)
    {
      while (!IS_DIR_CHAR(*p2))
      {
        if (!*p2)
          return NULL;
        p2 = CharNext(p2);
      }
      p2 = CharNext(p2);
    }

    return p2;
  }
  else
    return NULL;
}
#endif


void url_decode(char *in, char *out, int maxlen)
{
  while (*in && maxlen>1)
  {
    if (*in == '+') 
    {
      in++;
      *out++=' ';
    }
	  else if (*in == '%' && in[1] != '%' && in[1])
	  {
		  int a=0;
		  int b=0;
		  for ( b = 0; b < 2; b ++)
		  {
			  int r=in[1+b];
			  if (r>='0'&&r<='9') r-='0';
			  else if (r>='a'&&r<='z') r-='a'-10;
			  else if (r>='A'&&r<='Z') r-='A'-10;
			  else break;
			  a*=16;
			  a+=r;
		  }
		  if (b < 2) *out++=*in++;
		  else { *out++=a; in += 3;}
	  }
	  else *out++=*in++;
	  maxlen--;
  }
  *out=0;
}




HTTP_context::HTTP_context(int concnt)
{
  m_mime_type[0]=0;
	m_mime_subtype[0]=0;
  m_concnt=concnt;
  m_fail=0;
  m_bytes_timeout_scanget = 256;
  m_bytes_timeout_scanreply = 256;
  m_http_reply_code=0;
  m_hasfoundget=0;
  m_done=0;
  m_fp=0;
  m_fn[0]=0;
  strcpy(m_host,"unknown");
  m_hasfoundhttpreply=0;
  m_is_gz=0;
  m_ischunked=0;
  m_chunkpos=0;
  m_content_length=-1;
  m_content_length_orig=-1;
  m_src_ip=0;
  m_cookie[0]=0;
}

HTTP_context::~HTTP_context()
{
  if (m_fp) 
  {
    fclose(m_fp);
    if (m_is_gz)
    {
      char buf[8192];
      sprintf(buf,"gzip -df \"%s\"",finalfn);
      system(buf);
    }
  }
}

  // returns bytes used
int HTTP_context::onRequestStream(unsigned char *payload, int payload_len)
{
  if (m_hasfoundget) return 0; // if we've already parsed a get, we're done

  int startpos;
  for (startpos = 0; startpos <= payload_len-4; startpos++)
  {
    if (!memcmp(payload+startpos,"GET ",4)) break;
  }

  m_bytes_timeout_scanget -= startpos;
  if (m_bytes_timeout_scanget < 0)
  {
    m_fail=1;
    return 0;
  }
  
  if (startpos > payload_len-4) 
  {
    return startpos;
  }

  int endptr=startpos;
  while (endptr <= payload_len-4)
  {
    if (!memcmp(payload+endptr,"\r\n\r\n",4))
    {
      m_hasfoundget++;
      break;
    }
    endptr++;
  }
  if (!m_hasfoundget) 
  {
    return startpos; // wait for full request        
  }
  endptr+=4;

  // startpos is start of get block, endptr is end, both offsets
  int l=startpos+4;
  int haddot=0;
  while (l < endptr && payload[l] != '\n' && payload[l] != '\r' && payload[l] != ' ' && payload[l]) 
  {
    l++;
  }

  char buf[512];
  {
    char obuf[512];
    lstrcpyn(obuf,(char*)payload+startpos+4,min(l-startpos-4+1,sizeof(obuf)));
    // urldecode obuf to buf
    url_decode(obuf,buf,sizeof(buf)-1);
  }

  printf("get_request:%s\n",buf);

  {
    char *p=buf;
    int allowslashes=!!g_config_subdirs;
    while (*p) 
    {
      if (*p == '?' || *p == '&' || *p == ';' || *p == '=')
      {
        allowslashes=0;
      }

      if (*p == '/' || *p == '\\')
      {
        if (allowslashes) *p='/';
        else *p='_';
      }
      else if (*p == '?' || *p == '*' || *p == '|' || *p == ':' || *p == '<' || *p == '>' || *p == '\"' || *p == '\'')
        *p = '_';

      p++;
    }
    p=buf;
    while (*p == ' ') p++;
    if (!*p || !p[1]) 
    {
      strcpy(m_fn,"_index.html");
      if (g_config_subdirs) m_fn[0] = '/';
    }
    else lstrcpyn(m_fn,p,256);
  }

  int x;
  for (x = startpos+4; x < endptr-4; x ++)
  {
    if (!strnicmp((char*)payload+x,"\nHost:",6))
    {
      x+=6;
      while (payload[x] == ' ' && x < endptr) x++;
      int sx=x;
      while (payload[x] != '\r' && payload[x] != '\n' && x < endptr) x++;
      if (!strnicmp((char*)payload+sx,"www.",4)) sx+=4;
      lstrcpyn(m_host,(char*)payload+sx,min(x-sx+1,sizeof(m_host)));
      {
        char *p=m_host;
        while (*p)
        {
          if (*p == '/' || *p == '\\' || *p == '?' || *p == '*' || *p == '|' || *p == ':')
          {
            *p='_';
          }
          p++;
        }
      }
      //printf("host:|%s|\n",m_host);
    } 
    else if (g_config_cookie && !strnicmp((char*)payload+x,"\nCookie:",8))
    {
      x+=8;
      while (payload[x] == ' ' && x < endptr) x++;
      int sx=x;
      while (payload[x] != '\r' && payload[x] != '\n' && x < endptr) x++;
      int l = min(x-sx+1,sizeof(m_cookie)-1);
      lstrcpyn(m_cookie,(char*)payload+sx,l);
      m_cookie[l]=0;
    }
  }
  return endptr;
}

int HTTP_context::isDone() { return m_done; }

// returns amount used, if any
int HTTP_context::onReplyData(unsigned char *payload, int payload_len, unsigned int m_src_ip) 
{
  if (!m_hasfoundget || m_done) return 0;

  unsigned char *payload_orig=payload;

  if (!m_hasfoundhttpreply)
  {
    int hdrstart;
    for (hdrstart = 0; hdrstart <= payload_len-4; hdrstart++)
    {
      if (!memcmp(payload+hdrstart,"HTTP",4)) break;
    }
    m_bytes_timeout_scanreply -= hdrstart;
    if (m_bytes_timeout_scanreply < 0) 
    {
      m_fail=1;
      return 0;
    }
    if (hdrstart > payload_len-4) 
    {
      return hdrstart; // no header
    }

    int hdrend;
    for (hdrend = hdrstart; hdrend <= payload_len-4; hdrend ++)
    {
      if (!memcmp(payload+hdrend,"\r\n\r\n",4)) break;
    }
    if (hdrend > payload_len-4)
    {
      return hdrstart; // no end yet
    }

    hdrend+=4;
    m_hasfoundhttpreply=1;
    //printf("got HTTP reply\n");

    int x;
    for (x = hdrstart; x < hdrend-4; x ++)
    {
      if (payload[x] == '\r' || payload[x] == '\n') break;
    }
    {
      char tmp[512];
      lstrcpyn(tmp,(char*)payload+hdrstart,min(x-hdrstart+1,sizeof(tmp)));
      char *p=tmp;
      while (*p != ' ' && *p) p++;
      while (*p == ' ') p++;
      m_http_reply_code=atoi(p);
      printf("http_reply_code=%d (%s)\n",m_http_reply_code,tmp);
    }


    int keep_alive=0;
    for (x = hdrstart; x < hdrend-4; x ++)
    {
      if (!strnicmp((char*)payload+x,"\nTransfer-Encoding:",19))
      {
        x+=19;
        while (payload[x] == ' ' && x < payload_len) x++;
        if (!strnicmp((char*)payload+x,"chunked",7))
        {         
          m_ischunked=1;
          m_chunkpos=0;
        }
      }
      else if (!strnicmp((char*)payload+x,"\nContent-Type:",14))
      {
        x+=14;
        while (payload[x] == ' ' && x < payload_len) x++;
        int sx=x;
				while (payload[x] != '/' && payload[x] != '\r' && payload[x] != '\n' && x < payload_len) x++;
        if (x < payload_len && payload[x] == '/')
        {
					lstrcpyn(m_mime_type,(char*)payload+sx,min(sizeof(m_mime_type),1+x-sx));
          int sx=++x;
          while (payload[x] != '\r' && payload[x] != '\n' && x < payload_len) x++;
          if ( x > sx+2 && !strnicmp((char*)payload+sx,"x-",2)) sx+=2; // skip x-

          lstrcpyn(m_mime_subtype,(char*)payload+sx,min(sizeof(m_mime_subtype),1+x-sx));
        }
      }
      else if (!strnicmp((char*)payload+x,"\nContent-Encoding:",18))
      {
        x+=18;
        while (payload[x] == ' ' && x < payload_len) x++;
        int sx=x;
        if (!strnicmp((char*)payload+sx,"gzip",4)) m_is_gz=1;
      }
      else if (!strnicmp((char*)payload+x,"\nContent-Length:",16))
      {
        x+=16;
        while (payload[x] == ' ' && x < payload_len) x++;
        int len=atoi((char*)payload+x);
        //printf("Got Content-Length of %d\n",len);
        m_content_length_orig = m_content_length = len;
      }
      else if (m_content_length_orig < 0 && !strnicmp((char*)payload+x,"\nConnection:",12))
      {
        x+=12;
        while (payload[x] == ' ' && x < payload_len) x++;
        if (!strnicmp((char*)payload+x,"keep-alive",10)) keep_alive=1;
      }
    }
    if (keep_alive && !m_ischunked && m_content_length_orig < 0)
      m_content_length_orig=m_content_length=0;

    //printf("got response, keep_alive=%d, content_length=%d, chunked=%d\n",keep_alive, m_content_length_orig, m_ischunked);

    return hdrend; // done with header, let the rest get parsed the next time around
  }

  if (payload_len <= 0) return payload_len;


  // at this point, handle the different data types


  if (m_ischunked)
  {
    if (m_chunkpos > 0)
    {
      int l=min(m_chunkpos,payload_len);
      m_chunkpos-=l;

      doWrite(payload,l,m_src_ip);

      return l;
    }

    char buf[32];
    int x=0;
    while (x < payload_len && (payload[x] == '\r' || payload[x] == '\n')) x++;
    int sx=x;
    while (x < payload_len && payload[x] != '\r' && payload[x] != '\n') x++;
    lstrcpyn(buf,(char*)payload+sx,min(x-sx+1,sizeof(buf)));            
    char *p;
//      printf("reading chunk from buf: |%s|\n",buf);
    m_chunkpos = strtol(buf,&p,16);
    if (x < payload_len && payload[x]=='\r') x++;
    if (x < payload_len && payload[x]=='\n') x++;

    if (!m_chunkpos) // we're done
    {
      m_done=1;
    }
    return x;
  }
  else if (m_content_length_orig >= 0)
  {
    int l=min(m_content_length,payload_len);
    if (l > 0)
    {
      m_content_length-=l;
      doWrite(payload,l,m_src_ip);
    }
    if (m_content_length <= 0)
    {
      m_done=1;
    }
    return l;
  }
  else
  {
		doWrite(payload,payload_len,m_src_ip);
    return payload_len; // woot, in non-chunked non-content length mode we just write it all
  }
}

void HTTP_context::doWrite(void *buf, int len,unsigned int m_src_ip)
{
  if(m_cookie[0] && g_config_cookie)
  {
    char fn[MAX_PATH*2];
    sprintf(fn,"%s/cookies.txt", g_config_leadpath); 
    FILE *fh = fopen(fn,"ab");
    if(fh)
    {
      char *p = m_cookie;
      int l = strlen(m_cookie);
      while(1)
      {
        if(p>=p+l) break;
        while(*p==' ') p++;
        char *name = p;
        char *d = p;
        while(*d!='=' && *d) d++;
        if(!*d) break;
        *d++ = 0;
        char *val = d;
        while(*d!=';' && *d) d++;
        *d++ = 0;
        fprintf(fh, "%s %s %s %s\r\n", m_host, m_fn, name, val);
        p = d;
      }
      fclose(fh);
    }
    m_cookie[0] = 0;
  }
  if (len < 1) return;
  if (m_http_reply_code < 200 || m_http_reply_code >= 300) return;

	if (stricmp(g_config_mimetype,"all") && stricmp(m_mime_type,g_config_mimetype)) return;
	
  if (!m_fp && m_fn[0]) 
  {
    if (g_config_usemime &&
//          stricmp(m_mime_subtype,"octet-stream") &&
//        stricmp(m_mime_subtype,"unknown") &&
//          stricmp(m_mime_subtype,"binary") &&
        stricmp(m_mime_subtype,"misc") &&
        strlen(m_mime_subtype) < 5) // the strlen really removes a lot of the above
    {
      if (m_mime_subtype[0])
      {
        if (!stricmp(m_mime_subtype,"jpeg")) strcpy(m_mime_subtype,"jpg");
        {
          char *p=m_mime_subtype;
          int c=8;
          while (*p && c-- && *p != '?' && *p != '*' && *p != '&' && *p != '/' && *p != '\\' && *p != '|' && *p != ':' && *p != ' ' && *p != ';') p++;
          *p=0;
        }
      }  
      if (m_mime_subtype[0])
      {
        if (g_config_usemime>1) {
          char *p=m_fn;
          while (*p) p++;
          int c=16;
          while (*p != '/' && *p != '?' && *p != '&' && *p != '.' && p > m_fn && c-- > 0) p--;
          if (*p == '.') *p=0; // remove extension
        }
        strcat(m_fn,".");
        strcat(m_fn,m_mime_subtype);
      }
    }

		if (g_config_debugfn)
      sprintf(finalfn,"%s/" 
          "%s"
          "(%02d%s%s)" // woot
          "%s%s",
		    g_config_leadpath,
        m_host,
        m_concnt,m_ischunked?"ch":"",m_is_gz?"gz":"",
        m_fn,m_is_gz ? ".gz" : "");
    else
		  if (g_config_splitbyclient)
      	sprintf(finalfn,"%s/%s/" 
          "%s"
          "%s%s",
					g_config_leadpath,
					long2ip(m_src_ip),
        	m_host,
        	m_fn,m_is_gz ? ".gz" : "");
			else
				sprintf(finalfn,"%s/"
          "%s"
          "%s%s",
          g_config_leadpath,
          m_host,
          m_fn,m_is_gz ? ".gz" : "");

    {
      char *p=finalfn;
      while ((p=strstr(p,".."))) p[1]='_';
      p=finalfn+2;
      while ((p=strstr(p,"\\\\"))) p[1]='_';
      p=finalfn;
      while ((p=strstr(p,"//"))) p[1]='_';
      p=finalfn;
      if (p[1] == ':') p+=2;
      while (*p)
      {
        if (*p == '?' || *p == '*' || *p == '|' || *p == ':' || *p == '<' || *p == '>' || *p == '\"' || *p == '\'') *p = '_';      
        if (!isprint(*p))
        {
          *p=0;
          break;
        }
        p++;
      }
    }

#ifdef _WIN32
    {
      //fix '\' chars
      char *p = finalfn;
      while(p = strstr(p, "/"))
        *p = '\\';
      //windows doesn't like full pathnames > 256chars =/
      if(strlen(finalfn)>256)
      {
        //cut down some characters from the biggest part of the fullpath
        //int lens[256]={0,};
        char *p = finalfn;
        char *p2 = finalfn;
        int l = 0;
        char *big = finalfn;
        int bigl = 0;
        while(1)
        {
          if(!*p || *p==':' || *p=='\\')
          {
            if(l>bigl)
            {
              big = p2;
              bigl = l;
            }
            if(!*p) break;
            l = 0;
            p2 = p+1;
          }
          else
            l++;
          p++;
        }
        int tocut = strlen(finalfn) - 256;
        if(big && bigl>= tocut)
        {
          //cut in the middle so we keep the beginning+file extension
          strcpy(big+(bigl/2)-(tocut/2), big+(bigl/2)+tocut);
          printf("Warning: filename >256 chars truncated\n", finalfn);
        }
        else
        {
          //revert to simple trimming
          printf("Warning: filename too long with no room to trim properly: '%s'\n", finalfn);
          finalfn[256] = 0;
        }
      }
    }
#endif

    if (g_config_subdirs || g_config_splitbyclient)
    {
      char *p=finalfn;
      if (*p) 
      {
#ifdef _WIN32
        p = skip_root(finalfn);
#else
        p=finalfn;
#endif
				if (p) for (;;)
        {
          while (!IS_DIR_CHAR(*p) && *p) p=CharNext(p);
          if (!*p) break;

          char c=*p;
          *p=0;
#ifdef _WIN32
          CreateDirectory(finalfn,NULL);
#else
	  mkdir(finalfn,0644);
#endif
          *p++ = c;
        }
      }
    } 

    if (finalfn[0] && IS_DIR_CHAR(finalfn[strlen(finalfn)-1])) strcat(finalfn,"index.html");

    m_fp = fopen(finalfn,"wb");
    if (!m_fp)
    {
      printf("Error creating '%s'\n",finalfn);
    }
  }    
  if (m_fp)
  {
    fwrite(buf,len,1,m_fp);
    fflush(m_fp);
  }
}
