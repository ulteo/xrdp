/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechevalier <david@ulteo.com>
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include <list.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "seamrdp.h"


static int g_x_socket = 0;
static int g_seamrdp_up = 0;
static int g_last_seamrdp_size = 0;
static int seamrdp_mess_id = 0;
static int connected = 0;
static char* g_last_seamrdp_data = 0;
static char username[256];
static tbus g_x_wait_obj = 0;
static Display* g_display = 0;
static Window_list window_list;
static struct stream* g_ins = 0;
static char* display_name;

extern int g_seamrdp_chan_id; /* in chansrv.c */
extern struct log_config log_conf;

/*****************************************************************************/
int DEFAULT_CC
seamrdp_error_handler(Display* dis, XErrorEvent* xer)
{
  char text[256];
  XGetErrorText(dis, xer->error_code, text, 255);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: error [%s]", text);
  return 0;
}

/*****************************************************************************/
/* The X server had an internal error.  This is the last function called.
   Do any cleanup that needs to be done on exit, like removing temporary files.
   Don't worry about memory leaks */
int DEFAULT_CC
seamrdp_fatal_handler(Display* dis)
{
  log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: fatal error, exiting");
  main_cleanup();
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
seamrdp_send(char* mess, int size){
  struct stream* s;
  int rv;

  make_stream(s);
  init_stream(s, size);
  out_uint8p(s, mess, size);
  s_mark_end(s);
  size = (int)(s->end - s->data);

  rv = send_channel_data(g_seamrdp_chan_id, s->data, size);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: message sended: %s\n", mess);
  seamrdp_mess_id++;
  free_stream(s);
  return rv;
}

/*****************************************************************************/
/* returns error */
int APP_CC
seamrdp_init(void)
{
  char buf[1025];
  char xrdp_username_path[256];
  int size;
  int rv;
  int fd;

  log_message(&log_conf, LOG_LEVEL_INFO, "xrdp-chansrv: in seamrdp_init");
  if (g_seamrdp_up)
  {
    log_message(&log_conf, LOG_LEVEL_INFO, "xrdp-chansrv: channel already initialised -> disconnected session");
    return 0;
  }
  XInitThreads();

  seamrdp_deinit();
  rv = 0;
  /* setting the error handlers can cause problem when shutting down
     chansrv on some xlibs */
  XSetErrorHandler(seamrdp_error_handler);
  XSetIOErrorHandler(seamrdp_fatal_handler);

  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: opening display");
  g_display = XOpenDisplay(0);
  if (g_display == 0)
  {
	  log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: XOpenDisplay failed");
    rv = 1;
  }
  if (rv == 0)
  {
    g_x_socket = XConnectionNumber(g_display);
    if (g_x_socket == 0)
    {
      log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: XConnectionNumber failed");
      rv = 2;
    }
    g_x_wait_obj = g_create_wait_obj_from_socket(g_x_socket, 0);
  }
  display_name = g_getenv("XRDP_SESSVC_DISPLAY");
  if(g_strlen(display_name) == 0)
  {
    log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: failed to get environement variable XRDP_SESSVC_DISPLAY");
	rv = 3;
  }
  g_sprintf(xrdp_username_path,"/tmp/xrdp_user_%s",display_name);
  fd = g_file_open(xrdp_username_path);
  if(fd == 0)
  {
    log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: failed to get environement variable XRDP_SESSVC_DISPLAY");
	rv = 4;
  }
  size = g_file_read(fd,username,255);
  g_file_close(fd);
  username[size]='\0';

  if(connected == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: session already exist");
	Window_list_init();
  }
  connected = 1;
  if (rv == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: subscribe to SubstructureNotifyMask events");
	XSelectInput(g_display, DefaultRootWindow(g_display), SubstructureNotifyMask);
  }
  if (rv == 0)
  {
    size = snprintf(buf, sizeof(buf) - 1, "HELLO,%i,0x%08x\n", seamrdp_mess_id, 0);
    rv = seamrdp_send(buf,size);
    if (rv != 0)
    {
      log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: send_channel_data failed ");
      rv = 5;
    }
  }
  if (rv == 0)
  {
    g_seamrdp_up = 1;
    make_stream(g_ins);
    init_stream(g_ins, 8192)
  }
  else
  {
    log_message(&log_conf, LOG_LEVEL_ALWAYS, "xrdp-chansrv: seamrdp_init: error on exit");
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
seamrdp_deinit(void)
{
  if (g_x_wait_obj != 0)
  {
    g_delete_wait_obj_from_socket(g_x_wait_obj);
    g_x_wait_obj = 0;
  }

  g_x_socket = 0;
  g_free(g_last_seamrdp_data);
  g_last_seamrdp_data = 0;
  g_last_seamrdp_size = 0;
  free_stream(g_ins);
  g_ins = 0;
  if (g_display != 0)
  {
    XCloseDisplay(g_display);
    g_display = 0;
  }
  g_seamrdp_up = 0;
  return 0;
}


/*****************************************************************************/
void DEFAULT_CC
seamrdp_move_window(Window w, int x, int y, int width, int height)
{
  char* window_id;
  char* buffer;
  int size;

  Window win_in = get_in_window(g_display, w);
  Window_item* witem;
  Window_get(w, witem);
  if(witem == 0)
  {
    Window_get(win_in, witem);
    if(witem == 0)
    {
      log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window (%i) remove during the operation", (int)w);
      return;
    }
  }

  window_id = g_malloc(11,0);
  buffer = g_malloc(1024,0);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window (%i) move to [%i, %i, %i, %i]", (int)witem->window_id, x, y, width, height);
  if(witem->state == SEAMLESSRDP_MAXIMIZED)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window state : MAXIMIZED");
    sprintf(window_id, "0x%08x",(int)witem->window_id);
    size = sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", seamrdp_mess_id,window_id, -4, -4, width, height, 0 );
  }
  else
  {
    sprintf(window_id, "0x%08x",(int)witem->window_id);
    size = sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", seamrdp_mess_id,window_id, x, y, width, height, 0 );
  }
  seamrdp_send(buffer,size+1);
  g_free(window_id);
  g_free(buffer);
}

/*****************************************************************************/
int DEFAULT_CC
seamrdp_spawn_app(char* application)
{
  char* path = g_malloc(256,0);
  struct list* app_param;
  int i;
  int pid;

  for(i=0 ; i < g_strlen(application) ; i++)
  {
    if(application[i] == '\n'){
      application[i] = '\0';
    }
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: execute application '%s' for the user '%s'", application, username);
  pid = fork ();

  if( pid < 0 )
  {
    perror ( "failed to create processus");
    g_exit(-1);
  }
  if( pid > 0 )
  {
    return 0;
  }
  int status;

  app_param = list_create();
  app_param->auto_free = 1;
  list_add_item(app_param, (long)g_strdup("/bin/bash"));
  list_add_item(app_param, (long)g_strdup("startapp"));
  list_add_item(app_param, (long)g_strdup(username));
  list_add_item(app_param, (long)g_strdup(application));
  list_add_item(app_param, (long)g_strdup(display_name));
  list_add_item(app_param, 0);
  g_execvp("/bin/bash", ((char**)app_param->items));
  wait(&status);
  g_exit(0);
}


/*****************************************************************************/
int DEFAULT_CC
seamrdp_exec_app(char* application, char* window_id)
{
  struct list* app_param;
  char* path = g_malloc(256,0);
  char* buf;
  int pid = fork ();
  int i;
  int size;

  for(i=0 ; i < g_strlen(application) ; i++){
    if(application[i] == '\n')
    {
      application[i] = '\0';
    }
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: execute application '%s' for the user '%s'", application, username);
  if ( pid < 0 ) {
    perror ( "failed to create processus");
    g_exit(-1);
  }
  if( pid > 0 )
  {
    buf = g_malloc(1024,0);
    size = sprintf(buf, "APP_ID,%i,%s,%i\n", seamrdp_mess_id, window_id, pid );
    seamrdp_send(buf, size);
    g_free(buf);
    g_free(path);
    return 0;
  }
  int status;
  app_param = list_create();
  app_param->auto_free = 1;
  list_add_item(app_param, (long)g_strdup("/bin/bash"));
  list_add_item(app_param, (long)g_strdup("startapp"));
  list_add_item(app_param, (long)g_strdup(username));
  list_add_item(app_param, (long)g_strdup(application));
  list_add_item(app_param, (long)g_strdup(display_name));
  list_add_item(app_param, 0);
  g_execvp("/bin/bash", ((char**)app_param->items));
  wait(&status);
  g_free(path);
  g_exit(0);
}

/*****************************************************************************/
int APP_CC
get_next_token(char** message, char** token)
{
  char *temp;
  if(*message[0] == '\0')
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
  *message+= g_strlen(*message)+1;
  return 1;
}


/*****************************************************************************/
int APP_CC
get_icon(Window win_in, Window win_out)
{
  #if __WORDSIZE==64
    return 0;
  #endif

  #if __WORDSIZE==64
  long int *data = NULL;
  #else
  int *data = NULL;
  #endif

  int i;
  int k;
  int message_id;
  int height;
  int width;
  int count;
  int message_length;
  int pixel;
  char a;
  char r;
  char g;
  char b;
  unsigned long nitems;
  char* buffer;
  char* buffer_pos;
  char* window_id;
  int size;

  if(get_property(g_display, win_in, "_NET_WM_ICON", &nitems, (unsigned char*)&data) != Success)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: no icon for the window '%i'", (int)win_out);
    return 1;
  }
  if(nitems < 16*16+2)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: no proper icon for the window '%i'", (int)win_out);
    return 1;
  }
  buffer = g_malloc(1024,0);
  window_id = g_malloc(11,0);
  sprintf(window_id, "0x%08x",(int)win_out);
  //analyzing of _NET_WM_ICON atom content
  k=0;
  while( k < nitems )
  {
    message_length = 0;
    message_id = 0;
    width = data[k];
    height = data[k+1];
    if(width>32)
    {
      k+= height*width;
      continue;
    }

    k+=2;
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: new icon finded '%iX%i'", width, height);
    buffer_pos = buffer;
    count = sprintf(buffer, "SETICON,%i,%s,%i,%s,%i,%i,", seamrdp_mess_id,window_id,message_id,"RGBA",width,height );
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
        size = sprintf(buffer_pos,"\n");
        seamrdp_send(buffer,message_length+1);
        message_id++;
        buffer_pos = buffer;
        message_length = 0;
        count = sprintf(buffer, "SETICON,%i,%s,%i,%s,%i,%i,", seamrdp_mess_id, window_id,message_id,"RGBA",width,height );
        buffer_pos += count;
        message_length += count;
      }
    }
    k+= height*width;
  }
  g_free(buffer);
  g_free(window_id);
  return 0;
}

/*****************************************************************************/
int APP_CC
seamrdp_create_window(Window win_out)
{
  char* window_id = g_malloc(11,0);
  char* buffer = g_malloc(1024,0);
  int x;
  int y;
  unsigned int width,height,border,depth;
  Window root;
  XWindowAttributes attributes;
  Window_item* witem;
  Window parent_id;
  Window win_in;
  Window proper_win = 0;
  Atom type;
  unsigned char *name = '\0';
  int flags = SEAMLESSRDP_CREATE_MODAL;
  int pid, size;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: map of the window : %i", win_out);
  Window_get(win_out, witem);
  if(witem != 0)
  {
    sprintf(window_id, "0x%08x",(int)win_out);
    size = sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", seamrdp_mess_id,window_id,0,0 );
    seamrdp_send(buffer, size);
    g_free(buffer);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: the window %i already exist", win_out);
    g_free(window_id);
    return;
  }
  win_in = get_in_window(g_display, win_out);

  if(is_good_window(g_display, win_out)==0)
  {
    proper_win = win_out;
  }
  else
  {
    if(is_good_window(g_display, win_in) == 0)
    {
      proper_win = win_in;
    }
    else
    {
      log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: no good window");
      g_free(window_id);
      g_free(buffer);
      return;
    }
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: %i is a good window", proper_win);
  XGetWindowAttributes(g_display,win_out, &attributes);
  if(attributes.class == 2)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window %i has not the right attributes", proper_win);
    g_free(window_id);
    g_free(buffer);
    return;
  }

  get_window_name(g_display, proper_win, &name);
  get_window_type(g_display, proper_win, &type);
  get_window_pid(g_display, proper_win, &pid);
  get_parent_window(g_display, proper_win, &parent_id);

  if(parent_id != 0)
  {
    parent_id = -1;
  }
  if(type == XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_POPUP_MENU",False))
  {
    parent_id = -1;
  }
  if(parent_id == 0 && type == XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_NORMAL", False))
  {
    flags = SEAMLESSRDP_NORMAL;
  }
  if(win_in == 0)
  {
    win_in = win_out;
  }

  sprintf(window_id, "0x%08x",(int)win_out);
  size = sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", seamrdp_mess_id,window_id,pid,(int)parent_id,flags );
  seamrdp_send(buffer, size);

  size = sprintf(buffer, "TITLE,%i,%s,%s,0x%08x\n", seamrdp_mess_id,window_id,name,0 );
  seamrdp_send(buffer, size);

  if( parent_id == 0 )
  {
    get_icon( win_in, win_out);
  }

  size = sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", seamrdp_mess_id,window_id, attributes.x,attributes.y,attributes.width,attributes.height,0 );
  seamrdp_send(buffer, size);

  size = sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", seamrdp_mess_id,window_id,0,0 );
  seamrdp_send(buffer, size);

  g_free(window_id);
  g_free(buffer);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: add window [%i] to the window list", win_out);
  Window_add(win_out);
  Window_dump();
}



/*****************************************************************************/
int APP_CC
move_window2(XEvent* ev)
{
  Window_item *witem;
  Window win_in;
  Status status;
  Window root;
  int x;
  int y;
  unsigned int width,height,border,depth;
  char* temp = g_malloc(1024,0);
  Window_get(ev->xconfigure.window , witem);
  if(witem == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window (%i) remove during the operation", (int)ev->xconfigure.window);
    return 0;
  }
  win_in = get_in_window(g_display, witem->window_id);
  if(win_in == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: unknow window : %i", (int)witem->window_id);
    return 0;
  }
  if(witem->state == SEAMLESSRDP_MINIMIZED)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: the window %i is minimized", (int)witem->window_id);
    return 1;
  }
  XGetGeometry(g_display,witem->window_id,&root,&x,&y,&width, &height,&border,&depth );
  if(x !=ev->xconfigure.x || y != ev->xconfigure.y)
  {
    int x_decal,y_decal;
    unsigned int width2,height2,border2,depth2;
    XGetGeometry(g_display,win_in,&root,&x_decal,&y_decal,&width2, &height2,&border2,&depth2 );
    status = XMoveWindow(g_display, witem->window_id , ev->xconfigure.x-x_decal,ev->xconfigure.y-y_decal);
  }
  else
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: cannot move window at the same position");
  }
  if(width !=ev->xconfigure.width || height != ev->xconfigure.height){
    XResizeWindow(g_display, witem->window_id , ev->xconfigure.width,ev->xconfigure.height);
  }
  else
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: cannot resize the window at the same dimension");
  }
  g_free(temp);
  return 1;
}

/*****************************************************************************/
int APP_CC
get_state(Window w)
{
  unsigned char* data;
  unsigned long     nitems;
  Atom atom = 0;
  int i;
  int status;
  Window_item* witem;
  Window real_window = w;
  int state = SEAMLESSRDP_NORMAL;
  Window_get(w,witem);
  if(w == 0)
  {
    return 0;
  }

  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: get state for window %i", (int)w);
  status = get_property(g_display, w, "_NET_WM_STATE", &nitems, &data);
  if(status != 0)
  {
    return 0;
  }
  if(nitems == 0)
  {
    return 0;
  }
  for(i = 0; i<nitems ; i++)
  {
    atom = (Atom) \
			( \
			  (*((unsigned char*) data+0) << 0) | \
			  (*((unsigned char*) data + 1) << 8) | \
			  (*((unsigned char*) data+ 2) << 16) | \
			  (*((unsigned char*) data+ 3) << 24) \
			); \
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window state for %i : %s", (int)w, XGetAtomName(g_display, atom));
    if(atom == XInternAtom(g_display, "_NET_WM_STATE_HIDDEN", True))
    {
      state =SEAMLESSRDP_MINIMIZED;
      return state;
    }
    if(atom == XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", True) ||
       atom == XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", True))
    {
      state =SEAMLESSRDP_MAXIMIZED;
    }
    data+=sizeof(Atom);
  }
  return state;
}

/*****************************************************************************/
void APP_CC
check_window_state()
{
  char* window_id;
  char* buffer;
  Window win;
  Window win_in;
  Window_item *witem;
  Window proper_win = 0;
  int i, size;
  int state;
  int count = window_list.item_count;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: ckeck window state");
  for(i=0 ; i < count; i++)
  {
    witem = &window_list.list[i];
    win = window_list.list[i].window_id;
    win_in = get_in_window(g_display, win);
    if(is_good_window(g_display, win) == 0)
    {
      proper_win = win;
    }
    if(is_good_window(g_display, win_in) == 0)
    {
      proper_win = win_in;
    }
    if(proper_win == 0)
    {
      return;
    }
    state = get_state(proper_win);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: state for the window %i : %i", (int)window_list.list[i].window_id, state);

    if (state != window_list.list[i].state)
    {
      log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: state for window %i has change for : %i",(int)window_list.list[i].window_id, state);
      window_list.list[i].state = state;
      window_id = g_malloc(11, 0);
      buffer = g_malloc(1024, 0);

      sprintf(window_id, "0x%08x",(int)win);
      size = sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", seamrdp_mess_id, window_id, state, 0 );
      seamrdp_send(buffer, size);
      g_free(window_id);
      g_free(buffer);
    }
  }
}


/*****************************************************************************/
int change_state(Window w, int state )
{
  Window_item *witem;
  Window_get(w, witem);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: change state ");
  if(witem == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: unknow window");
    return 0;
  }
  if(state == witem->state)
  {
    return 0;
  }
  if(state==SEAMLESSRDP_NORMAL)
  {
    Window win_in;
    win_in = get_in_window(g_display,w);
    if( witem->state == SEAMLESSRDP_MINIMIZED)
    {
      XMapWindow(g_display, win_in);
      XFlush(g_display);
    }
  }
  return 0;
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
  int pid, size;

  name = '\0';
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: synchronise");
  Window_dump();
  for(i=0; i < window_list.item_count; i++)
  {
    witem = &window_list.list[i];
    XGetGeometry(g_display,witem->window_id,&root,&x,&y,&width, &height,&border,&depth );
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: GEOM OUT: %i,%i,%i,%i,%i,%i\n",x,y,width,height,border,depth);
    win_in = get_in_window(g_display, witem->window_id);

    if(is_good_window(g_display, witem->window_id)==0)
    {
      proper_win = witem->window_id;
      log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: %i is a good window\n",(int)proper_win);
    }
    else
    {
      if(is_good_window(g_display, win_in) == 0)
      {
        proper_win = win_in;
        log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: %i is a good window\n",(int)proper_win);
      }
      else
      {
        log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: No good window\n");
        return;
      }
    }

    get_window_name(g_display, proper_win,&name);
    get_window_type(g_display, proper_win,&type);
    get_window_pid(g_display, proper_win, &pid);
    get_parent_window(g_display, proper_win, &parent_id);

    if(type == XInternAtom(g_display, "_NET_WM_STATE_MODAL", False))
    {
      flags = SEAMLESSRDP_CREATE_MODAL;
    }

    if(type == XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False))
    {
      flags = SEAMLESSRDP_CREATE_TOPMOST;
    }

    if(parent_id == 0 && type != XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_NORMAL", False))
    {
      log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: Enable to create the window");
      flags = SEAMLESSRDP_CREATE_TOPMOST;
      parent_id = -1;
    }

    if(win_in == 0)
    {
      win_in = witem->window_id;
    }

    buffer=g_malloc(1024,0);
    window_id = g_malloc(11,0);

    sprintf(window_id, "0x%08x",(int)witem->window_id);
    size = sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", seamrdp_mess_id,window_id,pid,(int)parent_id,flags );

    seamrdp_send(buffer,size);

    size = sprintf(buffer, "TITLE,%i,%s,%s,0x%08x\n", seamrdp_mess_id,window_id,name,0 );
    seamrdp_send(buffer,size);

    sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", seamrdp_mess_id,window_id, x,y,width,height,0 );
    seamrdp_send(buffer,size);

    size = sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", seamrdp_mess_id,window_id,0,0 );
    seamrdp_send(buffer,size);

    g_free(buffer);
    g_free(window_id);
  }
}



/*****************************************************************************/
void destroy_window(Window w){
  char* window_id;
  char* buffer;
  int size;
  Window_item *witem;
  Window_get(w, witem);
  if(witem == 0)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window (%i) remove during the operation", (int)w);
    return ;
  }
  if(witem->state == SEAMLESSRDP_MINIMIZED)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: enable to destroy a minimize window ");
    return ;
  }

  window_id = g_malloc(11,0);
  buffer = g_malloc(1024,0);

  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: window destroy : %i\n",(int)w);
  sprintf(window_id, "0x%08x",(int)w);
  size = sprintf(buffer, "DESTROY,%i,%s,%08x\n", seamrdp_mess_id,window_id,0 );
  seamrdp_send(buffer, size);
  Window_del(w);
  g_free(window_id);
  g_free(buffer);
}


/*****************************************************************************/
int APP_CC
seamrdp_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
  int rv,size;
  char buffer[1024] ;
  char buf[1024];
  struct stream* ls;
  char* token1, *token2, *token3, *token4, *token5, *token6, *token7 ;
  char* temp;
  XEvent ev;
  size = s->end - s->p;
  g_strncpy(buf, (char*)s->p, size+1);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: seamrdp_data_in: chan_is %d "
      "chan_flags %d length %d total_length %d",
      chan_id, chan_flags, length, total_length);
  in_uint8a(s, s->end, length);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: seamrdp_data_in: chan_is %d "
      "message %s", chan_id, buf);


  temp = buf;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: message :  %s",buf);

  get_next_token(&temp, &token1);
  get_next_token(&temp, &token2);
  get_next_token(&temp, &token3);
  get_next_token(&temp, &token4);
  get_next_token(&temp, &token5);
  get_next_token(&temp, &token6);
  get_next_token(&temp, &token7);



  /* message process */
  if(g_strcmp("START_APP", token1)==0 && g_strlen(token4)>0){
	seamrdp_exec_app(token4, token3);
	rv = 0;
	XFlush(g_display);
	return rv;
  }

  if(g_strcmp("SPAWN", token1)==0 && g_strlen(token3)>0){
	seamrdp_spawn_app(token3);
	rv = 0;
	XFlush(g_display);
	return rv;
  }

  if(strcmp("STATE", token1)==0 ){
	int state = hex2int(token4);
	Window w = (Window)hex2int(token3);
	change_state(w, state);

	size = sprintf(buffer, "ACK,%i,%s\n", seamrdp_mess_id, token2);
	seamrdp_send(buffer, size+1);
	rv = 0;
	XFlush(g_display);
	return rv;
  }

  if(strcmp("POSITION", token1)==0 ){
	ev.xconfigure.window = (Window) hex2int(token3);
	ev.xconfigure.x = atoi(token4);
	ev.xconfigure.y = atoi(token5);
	ev.xconfigure.width = atoi(token6);
	ev.xconfigure.height = atoi(token7);
	move_window2(&ev);


	size = sprintf(buffer, "ACK,%d,%s\n", seamrdp_mess_id, token2);
	seamrdp_send(buffer,size);
	rv = 0;
	XFlush(g_display);
	return rv;
  }

  if(strcmp("FOCUS", token1)==0 ){
	size = sprintf(buffer, "ACK,%i,%s\n", seamrdp_mess_id, token2);
	seamrdp_send(buffer,size);

	rv = 0;
	XFlush(g_display);
	return rv;
  }
  if(strcmp("ZCHANGE", token1)==0 ){

	size = sprintf(buffer, "ACK,%i,%s\n", seamrdp_mess_id, token2);
	seamrdp_send(buffer,size+1);

	rv = 0;
	XFlush(g_display);
	return rv;
  }


  if(strcmp("ACK", token1)==0 ){
	rv = 0;
	XFlush(g_display);
	return rv;
  }

  if(strcmp("SYNC", token1)==0 ){
	synchronize();
	rv = 0;
	XFlush(g_display);
	return rv;
  }
  log_message(&log_conf, LOG_LEVEL_WARNING, "xrdp-chansrv: unknown command : %s\n",token1);

  rv = 0;
  XFlush(g_display);
  return rv;
}


/*****************************************************************************/
/* returns error
   this is called to get any wait objects for the main loop
   timeout can be nil */
int APP_CC
seamrdp_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;

  if ((!g_seamrdp_up) || (objs == 0) || (count == 0))
  {
    return 0;
  }
  lcount = *count;
  objs[lcount] = g_x_wait_obj;
  lcount++;
  *count = lcount;
  return 0;
}

/*****************************************************************************/
int APP_CC
seamrdp_check_wait_objs(void)
{
  XEvent xevent;
  Window w;
  Window win_in;
  Window_item* witem;

  if (!g_seamrdp_up)
  {
    return 0;
  }
  int test = g_is_wait_obj_set(g_x_wait_obj);
  if (test)
  {
    if (XPending(g_display) < 1)
    {
      log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: seamrdp_check_wait_objs: sck closed");
      return 0;
    }
    while (XPending(g_display) > 0)
    {
      XNextEvent(g_display, &xevent);
      check_window_state();
      switch(xevent.type){
      case ConfigureNotify:
    	w = xevent.xconfigure.window;
      	seamrdp_move_window(w, xevent.xconfigure.x, xevent.xconfigure.y,
      			xevent.xconfigure.width, xevent.xconfigure.height);
      	break;

      case MapNotify:
      	w = xevent.xmap.window;
      	seamrdp_create_window(w);
      	break;

      case UnmapNotify:
      	w = xevent.xunmap.window;
      	Window_dump();

      	Window_get(w, witem);
      	if(witem == 0)
      	{
      	  win_in = get_in_window(g_display, w);
      	  Window_get(win_in, witem);
      	  if(witem == 0)
      	  {
            log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-chansrv: unknowed window\n");
            break;
      	  }
      	}
      	destroy_window(w);
      	break;

      default:
      	break;
      }
      XFlush(g_display);
    }
  }
  return 0;
}
