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


#ifndef SEAMLESSRDPSHELL_H_
#define SEAMLESSRDPSHELL_H_

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <stdio.h>
#include "arch.h"
#include "parse.h"

/* socket constant */
#define ERROR	-1
#define	SOCKET_ERRNO	errno
#define	ERRNO		errno

/* seamlessrdpshell constant */
#define SEAMLESSRDP_CREATE_MODAL 		1
#define SEAMLESSRDP_CREATE_TOPMOST 		2
#define SEAMLESSRDP_NOTYETMAPPED		-1
#define SEAMLESSRDP_NORMAL			0
#define SEAMLESSRDP_MAXIMIZED			2
#define SEAMLESSRDP_MINIMIZED			1
#define SEAMLESSRDP_VERY_MAXIMIZED		3


/* Xlib constant  */
#define _NET_WM_STATE_REMOVE        	0    /* remove/unset property */
#define _NET_WM_STATE_ADD           	1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        	2    /* toggle property  */

extern struct log_config log_conf;


typedef struct {
	int state;
	int lock;
	int iconify;
	Window window_id;
	Window parent;
	int normal_x;
	int normal_y;
	unsigned int normal_width;
	unsigned int normal_height;

} Window_item;



typedef struct{
	Window_item list[1024];
	int item_count;

} Window_list;


int APP_CC
seamrdp_init();
int APP_CC
seamrdp_deinit(void);
int APP_CC
seamrdp_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length);
int APP_CC
seamrdp_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
seamrdp_check_wait_objs(void);




#define Window_list_init(){\
		window_list.item_count = 0;\
}\

#define Window_add( window){\
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: windows add");\
  int count = window_list.item_count;\
  Window_item* temp;\
  Window_get(window,temp);\
  if(temp == 0){\
    window_list.list[count].state = SEAMLESSRDP_NORMAL;\
    window_list.list[count].window_id = window;\
    window_list.list[count].normal_x = -1;\
    window_list.item_count++;\
  };\
}\


#define Window_get(window, window_item){\
  int i;\
  int count = window_list.item_count;\
  window_item = 0;\
  for(i=0 ; i < count; i++){\
    if(window_list.list[i].window_id == window){\
      window_item = &window_list.list[i];\
      break;\
    }\
  }\
}\



#define Window_del(window){\
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: windows del");\
  int count = window_list.item_count;\
  Window_item* temp;\
  Window_get(window,temp);\
  if(temp != 0){\
    temp->window_id = window_list.list[count-1].window_id;\
    temp->state = window_list.list[count-1].state;\
    window_list.item_count--;\
  }\
}\

#define Window_dump(){\
  int i;\
  int count = window_list.item_count;\
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: dump list of %i elements\n",count);\
  for(i=0 ; i < count; i++){\
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: \t elem %i :: state->%i || iconify->%i \n",\
                 (int) window_list.list[i].window_id,\
                 window_list.list[i].state, \
                 window_list.list[i].iconify);\
  }\
}\


#endif /* SEAMLESSRDPSHELL_H_ */
