/*
 * seamlessrdpshell.h
 *
 *  Created on: 1 déc. 2009
 *      Author: david
 */


#ifndef SEAMLESSRDPSHELL_H_
#define SEAMLESSRDPSHELL_H_

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <stdio.h>


/* socket constant */
#define ERROR	-1
#define	SOCKET_ERRNO	errno
#define	ERRNO		errno

/* seamlessrdpshell constant */
#define SEAMLESSRDP_CREATE_MODAL 		1
#define SEAMLESSRDP_CREATE_TOPMOST 		2
#define SEAMLESSRDP_NOTYETMAPPED		-1
#define SEAMLESSRDP_NORMAL				0
#define SEAMLESSRDP_MAXIMIZED			2
#define SEAMLESSRDP_MINIMIZED			1
#define SEAMLESSRDP_VERY_MAXIMIZED		3


/* Xlib constant  */
#define _NET_WM_STATE_REMOVE        	0    /* remove/unset property */
#define _NET_WM_STATE_ADD           	1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        	2    /* toggle property  */





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


/* windows list macro */
#define Window_list_init(window_list){\
		window_list.item_count = 0;\
}\

#define Window_add(window_list, window){\
	int count = window_list.item_count;\
	Window_item* temp;\
	Window_get(window_list, window,temp);\
	if(temp == 0){\
		window_list.list[count].state = SEAMLESSRDP_NORMAL;\
		window_list.list[count].window_id = window;\
		window_list.list[count].normal_x = -1;\
		window_list.item_count++;\
	};\
}\


#define Window_get(window_list, window, window_item){\
	int i;\
	int count = window_list.item_count;\
	window_item =0;\
	for(i=0 ; i < count; i++){\
		if(window_list.list[i].window_id == window){\
			window_item = &window_list.list[i];\
			break;\
		}\
	}\
}\



#define Window_del(window_list, window){\
	int count = window_list.item_count;\
	Window_item* temp;\
	Window_get(window_list, window,temp);\
	if(temp != 0){\
		temp->window_id = window_list.list[count-1].window_id;\
		temp->state = window_list.list[count-1].state;\
		window_list.item_count--;\
	}\
}\

#define Window_dump(window_list){\
	int i;\
	int count = window_list.item_count;\
	printf("\tdump list of %i elements\n",count);\
	for(i=0 ; i < count; i++){\
		printf("\t elem %i :: state->%i || iconify->%i \n", \
				(int) window_list.list[i].window_id, \
				window_list.list[i].state, \
				window_list.list[i].iconify);\
	}\
}\


#endif /* SEAMLESSRDPSHELL_H_ */
