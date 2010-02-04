/**
 * Copyright (C) 2009 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/


#ifndef USER_CHANNEL_H_
#define USER_CHANNEL_H_

#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>


int
user_channel_connect(const char* chan_name);
int
user_channel_send(int sock, char* data, int size);
int
user_channel_receive(int sock, char* data);
int
user_channel_close(int sock);

#define USER_CHANNEL_SOCKET_PATH	"/var/spool/xrdp/xrdp_user_channel_socket"
#define ERROR		-1

typedef struct{
	unsigned char* 	data;
	unsigned char* 	pointer;
	int size;
} Message;



#define out32(s, v) \
{ \
  *((s)->pointer) = (unsigned char)((v) >> 24); \
  (s)->pointer++; \
  *((s)->pointer) = (unsigned char)((v) >> 16); \
  (s)->pointer++; \
  *((s)->pointer) = (unsigned char)((v) >> 8); \
  (s)->pointer++; \
  *((s)->pointer) = (unsigned char)((v) >> 0); \
  (s)->pointer++; \
  s->size += 4; \
}


#define out16(s, v) \
{ \
  *((s)->pointer) = (unsigned char)((v) >> 8); \
  (s)->pointer++; \
  *((s)->pointer) = (unsigned char)((v) >> 0); \
  (s)->pointer++; \
  s->size += 4; \
}

#define out8(s, v, n) \
{ \
  memcpy((s)->pointer, (v), (n)); \
  (s)->pointer += (n); \
  s->size += n; \
}



#define init_message(s, v) \
{ \
  (s)->data = (unsigned char*)malloc(v); \
  (s)->pointer = (s)->data; \
  s->size = 0; \
}

/******************************************************************************/
#define free_message(s) \
{ \
  if ((s) != 0) \
  { \
    free((s)->data); \
  } \
  free((s)); \
} \


#define in32(s, v) \
{ \
  (v) = (unsigned int) \
    ( \
      (*((unsigned char*)((s)->pointer + 0)) << 24) | \
      (*((unsigned char*)((s)->pointer + 1)) << 16) | \
      (*((unsigned char*)((s)->pointer + 2)) << 8) | \
      (*((unsigned char*)((s)->pointer + 3)) << 0) \
    ); \
  (s)->pointer += 4; \
}



#define in16(s, v) \
{ \
  (v) = (unsigned short) \
    ( \
      (*((unsigned char*)((s)->pointer + 0)) << 8) | \
      (*((unsigned char*)((s)->pointer + 1)) << 0) \
    ); \
  (s)->pointer += 2; \
}

#define in8(s, v, n) \
{ \
  memcpy((v), (s)->pointer, (n)); \
  (s)->pointer += (n); \
}


#endif /* USER_CHANNEL_H_ */
