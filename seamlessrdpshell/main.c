/**
 * Copyright (C) 2008 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2009 2010
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

#include <log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include <vchannel.h>
#include <os_calls.h>
#include <file.h>
#include "seamlessrdpshell.h"
#include "xutils.h"

/* external function declaration */
extern char** environ;

static pthread_mutex_t mutex;
static int message_id;
static Display* display;
static Window_list window_list;
static int seamrdp_channel;
struct log_config* l_config;

/*****************************************************************************/
int
error_handler(Display* display, XErrorEvent* error)
{
  char text[256];
  XGetErrorText(display, error->error_code, text, 255);
  log_message(l_config, LOG_LEVEL_DEBUG, "XHook[error_handler]: "
  		" Error [%s]", text);
  return 0;
}

/*****************************************************************************/
int
send_message(char* data, int data_len)
{
	struct stream* s;
	make_stream(s);
	init_stream(s, data_len+1);
	out_uint8p(s, data, data_len+1);
	s_mark_end(s);
	if(vchannel_send(seamrdp_channel, s->data, data_len+1) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[send_message]: "
				"Unable to send message");
		return 1;
	}
	message_id++;
	return 0;
}

/*****************************************************************************/
void
handler(int sig)
{
	int pid, statut;
	pid = waitpid(-1, &statut, 0);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[handler]: "
  		"A processus has ended");
  return;
}

/*****************************************************************************/
int
spawn_app(char* application)
{
	char* path = malloc(256);
	int pid;

	pid = fork ();
	int i;

	for(i=0 ; i < strlen(application) ; i++)
	{
		if(application[i] == '\n')
			application[i] = '\0';
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
			"Exec application %s",application);
	if ( pid < 0 )
	{
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[spawn_app]: "
				"Failed to fork [%s]",strerror(errno));
		g_exit(-1);
	}
	if ( pid > 0 )
	{
		char* buffer = malloc(1024);
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
				"I 'm the processus parent");
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
				"child pid : %i", pid);
		free(buffer);
		return 0;
	}
	int status;
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
			"I 'm the processus child");
	sprintf(path,"/usr/bin/%s",application);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
			" Path : '%s'",path);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
			" Application name: '%s'",application);
	execle( path, application, (char*)0, environ);
	wait(&status);
	exit(0);
}

/*****************************************************************************/
void synchronize(){
	char* buffer;
	char* window_id;
	int i;
	int x,y;
	unsigned int width,height,border,depth;
	Window root;
	Window_item* witem;
	Window parent_id;
	Window win_in;
	Window proper_win = 0;
	Atom type;
	unsigned char *name;
	int flags = 0;
	int pid;

	name = '\0';
	buffer = malloc(1024);
	sprintf(buffer, "HELLO,%i,0x%08x\n", 0, 0);
	send_message(buffer, strlen(buffer));
	free(buffer);

	for(i=0; i < window_list.item_count; i++)
	{
		witem = &window_list.list[i];
		XGetGeometry(display,witem->window_id,&root,&x,&y,&width, &height,&border,&depth );
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
				"GEOM OUT: %i,%i,%i,%i,%i,%i", x, y, width, height, border, depth);

		win_in = get_in_window(display, witem->window_id);

		if(is_good_window(display, witem->window_id)==0)
		{
			proper_win = witem->window_id;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
					"%i is a good window", (int)proper_win);
		}
		else
		{
			if(is_good_window(display, win_in) == 0)
			{
				proper_win = win_in;
				log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
						"%i is a good window", (int)proper_win);
			}
			else
			{
				log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
						"No good window");
				return;
			}
		}

		get_window_name(display,proper_win,&name);
		get_window_type(display,proper_win,&type);
		get_window_pid(display, proper_win, &pid);
		get_parent_window(display, proper_win, &parent_id);

		if(type == XInternAtom(display, "_NET_WM_STATE_MODAL", False))
		{
			flags = SEAMLESSRDP_CREATE_MODAL;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
					"%i is a modal windows", proper_win);
		}

		if(type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False))
		{
			flags = SEAMLESSRDP_CREATE_TOPMOST;
		}

		if(parent_id == 0 && type != XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False))
		{
				flags = SEAMLESSRDP_CREATE_TOPMOST;
				parent_id = -1;
		}

		if(win_in == 0)
		{
			win_in = witem->window_id;
		}

		buffer=malloc(1024);
		window_id = malloc(11);

		sprintf(window_id, "0x%08x",(int)witem->window_id);
		sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", message_id,window_id,pid,(int)parent_id,flags );
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "TITLE,%i,%s,%s,0x%08x\n", message_id,window_id,name,0 );
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, x,y,width,height,0 );
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,0,0 );
		send_message(buffer, strlen(buffer));

		free(buffer);
		free(window_id);
	}
}

/*****************************************************************************/
int get_next_token(char** message, char** token){
	char *temp;
	if (*message[0] == '\0')
	{
		return 0;
	}
	*token = *message;
	temp = strchr(*message, ',');
	if(temp == 0)
	{
		temp = strchr(*message, '\0');
	}

	*temp = '\0';
	*message+= strlen(*message)+1;
	return 1;

}

/*****************************************************************************/
int process_move_action(XEvent* ev){
  Window_item *witem;
  Window win_in;
  Status status;
  Window root;
  int x;
  int y;
  unsigned int width,height,border,depth;
	Window_get(window_list, ev->xconfigure.window, witem);
  if(witem == 0)
  {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_move_action]: "
				"Window (%i) remove during the operation", (int)ev->xconfigure.window);
    return 0;
  }
  win_in = get_in_window(display, witem->window_id);
  if(win_in == 0)
  {
  	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_move_action]: "
  			"Unknow window : %i", (int)witem->window_id);
    return 0;
  }
  if(witem->state == SEAMLESSRDP_MINIMIZED)
  {
  	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_move_action]: "
  			"The window %i is minimized", (int)witem->window_id);
    return 1;
  }
  XGetGeometry(display,witem->window_id,&root,&x,&y,&width, &height,&border,&depth );
  if(x !=ev->xconfigure.x || y != ev->xconfigure.y)
  {
    int x_decal,y_decal;
    unsigned int width2,height2,border2,depth2;
    XGetGeometry(display,win_in,&root,&x_decal,&y_decal,&width2, &height2,&border2,&depth2 );
    status = XMoveWindow(display, witem->window_id , ev->xconfigure.x-x_decal,ev->xconfigure.y-y_decal);
  }
  else
  {
  	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_move_action]: "
  			"Cannot move window at the same position");
  }
  if(width !=ev->xconfigure.width || height != ev->xconfigure.height){
    XResizeWindow(display, witem->window_id , ev->xconfigure.width,ev->xconfigure.height);
  }
  else
  {
  	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_move_action]: "
  			"Cannot resize the window at the same dimension");
  }
  return 1;
}

/*****************************************************************************/
int change_state(Window w, int state ){
	Window_item *witem;
	Window_get(window_list, w, witem);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[change_state]: "
			"Change state ");

	if(witem == 0){
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[change_state]: "
				"Unknow window");
		return 0;
	}

	if(state == witem->state)
		return 0;

	if(state==SEAMLESSRDP_NORMAL){
		Window win_in;
		win_in = get_in_window(display, w);
		if( witem->state == SEAMLESSRDP_MINIMIZED){
			XMapWindow(display, win_in);
		}
	}
	return 0;
}

/*****************************************************************************/
void process_message(char* buffer){
	char* token1, *token2, *token3, *token4, *token5, *token6, *token7 ;
	char* temp;
	temp = buffer;
	char* buffer2 = malloc(1024);
	XEvent ev;

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_message]: "
			"Message to process : %s",buffer);
	get_next_token(&temp, &token1);
	get_next_token(&temp, &token2);
	get_next_token(&temp, &token3);
	get_next_token(&temp, &token4);
	get_next_token(&temp, &token5);
	get_next_token(&temp, &token6);
	get_next_token(&temp, &token7);
	/* message process */
	if(strcmp("SPAWN", token1)==0 && strlen(token3)>0)
	{
		spawn_app(token3);
		free(buffer2);
		return;
	}
	if(strcmp("STATE", token1)==0 )
	{
		int state = hex2int(token4);
		Window w = (Window)hex2int(token3);

		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_message]: "
				"State : %i for the window %i",state, (int)w);
		change_state(w, state);
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		free(buffer2);
		return;
	}

	if(strcmp("POSITION", token1)==0 )
	{
		ev.xconfigure.window = (Window) hex2int(token3);
		ev.xconfigure.x = atoi(token4);
		ev.xconfigure.y = atoi(token5);
		ev.xconfigure.width = atoi(token6);
		ev.xconfigure.height = atoi(token7);

		pthread_mutex_lock(&mutex);
		process_move_action( &ev);
		pthread_mutex_unlock(&mutex);
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		free(buffer2);
		return;
		}

	if(strcmp("FOCUS", token1)==0 )
	{
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		free(buffer2);
		return;
	}
	if(strcmp("ZCHANGE", token1)==0 )
	{
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		free(buffer2);
		return;
	}

	if(strcmp("ACK", token1)==0 )
	{
		free(buffer2);
		return;
	}
	if(strcmp("SYNC", token1)==0 )
	{
		synchronize();
		free(buffer2);
		return;
	}
	log_message(l_config, LOG_LEVEL_WARNING, "XHook[process_message]: "
			"Invalid message : %s\n",token1);

}

/*****************************************************************************/
int
get_icon(Window win_in, Window win_out )
{
#if __WORDSIZE==64
	return 0;
#endif

#if __WORDSIZE==64
	long int *data = NULL;
#else
	int *data = NULL;
#endif
	int i, k, message_id, height, width, count, message_length, pixel;
	char a,r,g,b;
	unsigned long nitems;
	char* buffer = g_malloc(1024, 1);
	char* buffer_pos = buffer;
	char* window_id = g_malloc(11, 1);

	if(get_property(display, win_in, "_NET_WM_ICON", &nitems, (unsigned char*)&data) != Success)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_icon]: "
					"No icon for the application %i",(int)win_out);
		return 1;
	}
	if(nitems < 16*16+2)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_icon]: "
					"No proper icon for the application %i",(int)win_out);
		return 1;
	}
	sprintf(window_id, "0x%08x",(int)win_out);
	//analyzing of _NET_WM_ICON atom content
	k=0;
	while( k < nitems )
	{
		message_length = 0;
		message_id = 0;
		width = data[k];
		height = data[k+1];
		k+=2;

		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_icon]: "
				"new Icon : %i X %i\n",width, height);

		buffer_pos = buffer;
		count = sprintf(buffer, "SETICON,%i,%s,%i,%s,%i,%i,", message_id,window_id,message_id,"RGBA",width,height );

		buffer_pos += count;
		message_length += count;

		for (i = 0; i < width * height; i++)
		{
			a = ((data[k+i] >> 24));
			r = ((data[k+i] >> 16));
			g = ((data[k+i] >> 8));
			b = ((data[k+i] >> 0));

			pixel = (r<<24)+(g<<16)+(b<<8)+(a<<0);

			count = sprintf(buffer_pos,"%08x",pixel);
			buffer_pos += count;
			message_length += count;

			if(message_length >1000 || i == width * height -1)
			{
				sprintf(buffer_pos,"\n");
				send_message(buffer, strlen(buffer));

				message_id++;
				buffer_pos = buffer;
				message_length = 0;
				count = sprintf(buffer, "SETICON,%i,%s,%i,%s,%i,%i,", message_id,window_id,message_id,"RGBA",width,height );
				buffer_pos += count;
				message_length += count;
			}
		}
		k+= height*width;
	}
	return 0;

}

/*****************************************************************************/
void create_window(Window win_out){
	char* window_id = malloc(11);;
	char* buffer = malloc(1024);
	int x,y;
	unsigned int width,height,border,depth;
	Window root;
	XWindowAttributes attributes;
	Window_item* witem;
	Window parent_id;
	Window win_in;
	Window proper_win = 0;
	Atom type;
	unsigned char *name;
	int flags = 0;
	int pid;

	name = '\0';


	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
			"Creation of the window : %i",(int)win_out);
	Window_get(window_list, win_out, witem);
	if(witem != 0)
	{
		sprintf(window_id, "0x%08x",(int)win_out);
		sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,0,0 );
		send_message(buffer, strlen(buffer));
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
				"%i already exist",(int)win_out);
		free(window_id);
		free(buffer);
		return;
	}
	XGetGeometry(display,win_out,&root,&x,&y,&width, &height,&border,&depth );
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
				"GEOM OUT: %i,%i,%i,%i,%i,%i", x, y, width, height, border, depth);


	win_in = get_in_window(display, win_out);
	XGetGeometry(display,win_in,&root,&x,&y,&width, &height,&border,&depth );
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
				"GEOM IN: %i,%i,%i,%i,%i,%i\n", x, y, width, height, border, depth);

	if(is_good_window(display, win_out)==0)
	{
		proper_win = win_out;
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
					"%i is a good window", (int)proper_win);
	}
	else
	{
		if(is_good_window(display, win_in) == 0)
		{
			proper_win = win_in;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
						"%i is a good window", (int)proper_win);
		}
		else
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
						"No good window");
			free(window_id);
			free(buffer);
			return;
		}
	}
	XGetWindowAttributes(display,win_out, &attributes);
	if(attributes.class == 2)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
					"Bad attributes : %i",(int)proper_win);
		free(window_id);
		free(buffer);
		return;
	}
	get_window_name(display,proper_win,&name);
	get_window_type(display,proper_win,&type);
	get_window_pid(display, proper_win, &pid);
	get_parent_window(display, proper_win, &parent_id);

  if(parent_id != 0)
  {
    parent_id = -1;
  }
  if(type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU",False))
  {
    parent_id = -1;
  }
  if(parent_id == 0 && type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False))
  {
    flags = SEAMLESSRDP_NORMAL;
  }
  if(win_in == 0)
  {
    win_in = win_out;
  }

  sprintf(window_id, "0x%08x",(int)win_out);
  sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", message_id, window_id,pid,(int)parent_id,flags );
	send_message(buffer, strlen(buffer));
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[create_window]: "
			"Application title : %s", name);

	sprintf(buffer, "TITLE,%i,%s,%s,0x%08x\n", message_id,window_id,name,0 );
	send_message(buffer, strlen(buffer));
	get_icon( win_in, win_out);

	sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, attributes.x,attributes.y,attributes.width,attributes.height,0 );
	send_message(buffer, strlen(buffer));
	sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,0,0 );
	send_message(buffer, strlen(buffer));
	free(window_id);
	free(buffer);
	Window_add(window_list,win_out);
	//Window_dump(window_list);
}

/*****************************************************************************/
int get_state(Window w)
{
	unsigned char* data;
	Atom     actual_type;
	int      actual_format;
	unsigned long     nitems;
	unsigned long     bytes;
	Atom atom = 0;
	int i;
	int status;
	Window_item* witem;
	Window real_window = w;

	int state = SEAMLESSRDP_NORMAL;
	Window_get(window_list, w, witem);
	if(w == 0)
	{
		return 0;
	}

	status = XGetWindowProperty(
			display,
			real_window,
			XInternAtom(display, "_NET_WM_STATE", True),
			0,
			(~0L),
			False,
			AnyPropertyType,
			&actual_type,
			&actual_format,
			&nitems,
			&bytes,
			&data);

    if(status != 0){
		return 0;
    }
    if(nitems == 0){
    	return 0;
    }

  	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_state]: "
					"%i state founded",(int)nitems);
    for(i = 0; i<nitems ; i++)
    {
    	atom = (Atom) \
    			( \
    					(*((unsigned char*) data+0) << 0) | \
    					(*((unsigned char*) data + 1) << 8) | \
    					(*((unsigned char*) data+ 2) << 16) | \
    					(*((unsigned char*) data+ 3) << 24) \
    			); \
    	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_state]: "
					"Atom state : %i",(int)atom);
    	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_state]: "
    					"Windows state : %s[%i]\n",XGetAtomName(display, atom),(int)atom);
	    if(atom == XInternAtom(display, "_NET_WM_STATE_HIDDEN", True))
	    {
	    	state =SEAMLESSRDP_MINIMIZED;
	    	return state;
	    }
	    if( atom == XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True) ||
	    		atom == XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True))
	    {
	    	state =SEAMLESSRDP_MAXIMIZED;
	    }
		data+=sizeof(Atom);

    }

    log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_state]: "
					"State for %i : %s",(int)real_window, XGetAtomName(display, atom));

    return state;
}

/*****************************************************************************/
void destroy_window(Window w)
{
	char* window_id;
	char* buffer;
	Window_item *witem;
	Window_get(window_list,w, witem);
	if(witem->state == SEAMLESSRDP_MINIMIZED)
	{
    log_message(l_config, LOG_LEVEL_DEBUG, "XHook[destroy]: "
    		"Unable to destroy a minimize window ");
		return ;
	}
	window_id = malloc(11);
	buffer = malloc(1024);
  log_message(l_config, LOG_LEVEL_DEBUG, "XHook[destroy]: "
				"Destroy of window: %i",(int)w);
	sprintf(window_id, "0x%08x",(int)w);
	sprintf(buffer, "DESTROY,%i,%s,%08x\n", message_id,window_id,0 );

	send_message(buffer, strlen(buffer));
	Window_del(window_list, w);
	free(window_id);
	free(buffer);

}


/*****************************************************************************/
void move_window(Window w, int x, int y, int width, int height)
{
	char* window_id;
	char* buffer;

	Window win_in = get_in_window(display, w);
	Window_item* witem;

	Window_get(window_list, w, witem);
	if(witem == 0)
	{
		Window_get(window_list,win_in, witem);
		if(witem == 0){
	    log_message(l_config, LOG_LEVEL_DEBUG, "XHook[move_window]: "
						"Unknowed window");
			return;
		}
	}
	window_id = malloc(11);
	buffer = malloc(1024);

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[move_window]: "
				"State: %i",witem->state);
	if(witem->state == SEAMLESSRDP_MAXIMIZED)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[move_window]: "
					"Windows id : %i", (int)witem->window_id);
		sprintf(window_id, "0x%08x",(int)witem->window_id);
		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, -4, -4, width, height, 0 );
	}
	else
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[move_window]: "
					"Windows id: %i",(int)witem->window_id);
		sprintf(window_id, "0x%08x",(int)witem->window_id);
		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, x, y, width, height, 0 );
	}
	send_message(buffer, strlen(buffer));
	free(window_id);
	free(buffer);
}

/*****************************************************************************/
void check_window_state()
{
	char* window_id;
	char* buffer;

	Window win;
	Window win_in;
	Window_item *witem;
	Window proper_win = 0;
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[check_window_state]: "
				"Check windows states");
	int i;
	int state;
	int count = window_list.item_count;
	for(i=0 ; i < count; i++)
	{
		witem = &window_list.list[i];
		win = window_list.list[i].window_id;
		win_in = get_in_window(display, win);
		if(is_good_window(display, win) == 0)
		{
			proper_win = win;
		}
		if(is_good_window(display, win_in) == 0)
		{
			proper_win = win_in;
		}
		if(proper_win == 0)
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[check_window_state]: "
						"No good win");
			return;
		}
		state = get_state( proper_win);
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[check_window_state]: "
					"State for %i: %i\n", (int)window_list.list[i].window_id, state);
		if (state != window_list.list[i].state)
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[check_window_state]: "
					"State for window %i has change for : %i", (int)window_list.list[i].window_id, state);
			window_list.list[i].state = state;
			window_id = malloc(11);
			buffer = malloc(1024);

			sprintf(window_id, "0x%08x",(int)win);
			sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,state,0 );
			send_message(buffer, strlen(buffer));
			free(window_id);
			free(buffer);
		}
	}
}

/*****************************************************************************/
void *thread_Xvent_process (void * arg)
{
	Window w;
	Window root_windows;
	Window_item* witem;
	Window win_in;

	root_windows = DefaultRootWindow(display);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				"Windows root ID : %i", (int)root_windows);

	XSelectInput(display, root_windows, SubstructureNotifyMask);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				"Begin the event loop ");

	while (1) {
		XEvent ev;
		XNextEvent (display, &ev);
		pthread_mutex_lock(&mutex);

		check_window_state();

		switch(ev.type){

		case ConfigureNotify:
			w = ev.xconfigure.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
						"Window move : %i",(int)w);
			move_window(w, ev.xconfigure.x,ev.xconfigure.y,ev.xconfigure.width,ev.xconfigure.height);
			break;

		case MapNotify:
			w = ev.xmap.window;
			create_window( w);
			break;

		case DestroyNotify:
			w = ev.xdestroywindow.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
						"Destroy of the window: %i",(int)w);
			break;

		case UnmapNotify:
			w = ev.xunmap.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
						"Unmap of the window: %i",(int)w);
			//Window_dump(window_list);

			Window_get(window_list,w, witem);
			if(witem == 0){
				win_in = get_in_window(display, w);
				Window_get(window_list,win_in, witem);
				if(witem == 0){
					log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
							"Unknowed window\n");
					break;
				}
			}
			destroy_window( w);
			break;

		default:
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
						"Event type [%i] ignored", ev.type);
			break;

		}
		pthread_mutex_unlock(&mutex);
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
			"Closing display");
	XCloseDisplay(display);
	pthread_exit (0);
}

/*****************************************************************************/
void *thread_vchannel_process (void * arg)
{
	char* buffer = malloc(1024);
	struct stream* s = NULL;
	int rv;
	int length;
	int total_length;

	signal(SIGCHLD, handler);
	sprintf(buffer, "HELLO,%i,0x%08x\n", 0, 0);
	send_message(buffer, strlen(buffer));
	while(1){
		make_stream(s);
		init_stream(s, 1024);

		rv = vchannel_receive(seamrdp_channel, s->data, &length, &total_length);
		if( rv == ERROR )
		{
			printf("close\n");
			vchannel_close(seamrdp_channel);
			pthread_exit ((void*)1);
		}
		switch(rv)
		{
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "XHook[thread_vchannel_process]: "
					"Invalid message");
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_vchannel_process]: "
					"Status connected");
			synchronize();
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_vchannel_process]: "
					"Status disconnected");
			break;
		default:
			s->data[length]=0;
			process_message(s->data);
			break;
		}
		free_stream(s);
	}
	pthread_exit (0);
}

int
XHook_init()
{
  char filename[256];
  char log_filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;
  int display_num;

  display_num = g_get_display_num_from_display(g_strdup(g_getenv("DISPLAY")));
	if(display_num == 0)
	{
		g_printf("XHook[XHook_init]: Display must be different of 0\n");
		return ERROR;
	}
	l_config = g_malloc(sizeof(struct log_config), 1);
	l_config->program_name = "XHook";
	l_config->log_file = 0;
	l_config->fd = 0;
	l_config->log_level = LOG_LEVEL_DEBUG;
	l_config->enable_syslog = 0;
	l_config->syslog_level = LOG_LEVEL_DEBUG;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(filename, 255, "%s/seamrdp.conf", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, XHOOK_CFG_GLOBAL, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, XHOOK_CFG_NAME))
      {
        if( g_strlen(value) > 1)
        {
        	l_config->program_name = (char*)g_strdup(value);
        }
      }
    }
  }
  if (file_by_name_read_section(filename, XHOOK_CFG_LOGGING, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_DIR))
      {
      	l_config->log_file = (char*)g_strdup(value);
      }
      if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_LEVEL))
      {
      	l_config->log_level = log_text2level(value);
      }
      if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_ENABLE_SYSLOG))
      {
      	l_config->enable_syslog = log_text2bool(value);
      }
      if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_SYSLOG_LEVEL))
      {
      	l_config->syslog_level = log_text2level(value);
      }
    }
  }
  if( g_strlen(l_config->log_file) > 1 && g_strlen(l_config->program_name) > 1)
  {
  	g_sprintf(log_filename, "%s/%i/%s.log",
  			l_config->log_file,	display_num, l_config->program_name);
  	l_config->log_file = (char*)g_strdup(log_filename);
  }
  list_delete(names);
  list_delete(values);

	if(log_start(l_config) != LOG_STARTUP_OK)
	{
		g_printf("vchannel[vchannel_init]: Unable to start log system\n");
		return ERROR;
	}
  else
  {
  	g_printf("vchannel[vchannel_init]: Invalid channel configuration file : %s\n", filename);
  	return LOG_STARTUP_OK;
  }
  return 0;
}

/*****************************************************************************/
int main(int argc, char** argv, char** environ)
{
	pthread_t Xevent_thread, Vchannel_thread;
	void *ret;
	l_config = g_malloc(sizeof(struct log_config), 1);
	if (XHook_init() != LOG_STARTUP_OK)
	{
		g_printf("XHook[main]: Enable to init log system\n");
		g_free(l_config);
		return 1;
	}
	if (vchannel_init() == ERROR)
	{
		g_printf("XHook[main]: Enable to init channel system\n");
		g_free(l_config);
		return 1;
	}

	Window_list_init(window_list);
	pthread_mutex_init(&mutex, NULL);
	message_id = 0;
	seamrdp_channel = vchannel_open("seamrdp");
	if( seamrdp_channel == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
				"Error while connecting to vchannel provider");
		g_free(l_config);
		return 1;
	}

	XInitThreads();
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[main]: "
			"Opening the default display : %s",getenv("DISPLAY"));

	if ((display = XOpenDisplay(0))== 0){
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
				"Unable to open the default display : %s ",getenv("DISPLAY"));
		g_free(l_config);
		return 1;
	}
	XSynchronize(display, 1);
	XSetErrorHandler(error_handler);


	if (pthread_create (&Xevent_thread, NULL, thread_Xvent_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
				"Pthread_create error for thread : Xevent_thread");
		g_free(l_config);
		return 1;
	}
	if (pthread_create (&Vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
				"Pthread_create error for thread : Vchannel_thread");
		g_free(l_config);
		return 1;
	}

	(void)pthread_join (Xevent_thread, &ret);
	(void)pthread_join (Vchannel_thread, &ret);
	pthread_mutex_destroy(&mutex);
	g_free(l_config);
	return 0;
}
