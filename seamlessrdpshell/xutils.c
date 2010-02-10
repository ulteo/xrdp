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

#include "xutils.h"

/*****************************************************************************/
int
hex2int(const char* hexa_string)
{
  int ret = 0, t = 0, n = 0;
  const char *c = hexa_string+2;

  while(*c && (n < 16)) {
    if((*c >= '0') && (*c <= '9'))
    	t = (*c - '0');
    else if((*c >= 'A') && (*c <= 'F'))
    	t = (*c - 'A' + 10);
    else if((*c >= 'a') && (*c <= 'f'))
    	t = (*c - 'a' + 10);
    else break;
    n++;
    ret *= 16;
    ret += t;
    c++;
    if(n >= 8) break;
  }
  return ret;
}

/*****************************************************************************/
int
get_property( Display* display, Window w, const char* property, unsigned long *nitems, unsigned char** data)
{
  Atom actual_type;
  int actual_format;
  unsigned long bytes;
  int status;

  printf("seamlessrdpshell[get_property]: Get property : %s\n", property);
  status = XGetWindowProperty(display,
                              w,
                              XInternAtom(display, property, True),
                              0,
                              (~0L),
                              False,
                              AnyPropertyType,
                              &actual_type,
                              &actual_format,
                              nitems,
                              &bytes,
                              data);

  if(status != Success || *nitems==0){
  	printf("seamlessrdpshell[get_property]: Enable to get atom %s\n",property);
	return 1;
  }

  return 0;
}

/*****************************************************************************/
Window
get_in_window(Display* display,  Window w)
{
  Window root;
  Window parent;
  Window *children;
  unsigned int nchildren;

  if (!XQueryTree(display, w, &root, &parent, &children, &nchildren) || nchildren == 0){
  	printf("seamlessrdpshell[get_in_window]:  no child windows\n");
    return 0;
  }

  printf("seamlessrdpshell[get_in_window]: in window : %i\n",(int)children[nchildren-1]);
  return children[nchildren-1];
}


int is_good_window(Display* display,  Window w)
{
  unsigned char* data;
  unsigned long nitems;
  int status;

  status = get_property(display, w, "WM_HINTS", &nitems, &data);
  if((status != 0) || (data == 0))
  {
  	printf("seamlessrdpshell[is_good_window]: %i did not contain the right information\n", (int)w);
  }
  return status;
}

/*****************************************************************************/
int get_window_name(Display* display, Window w, unsigned char** name)
{
  unsigned long nitems;
  int status;
//TODO replace , par _
  status = get_property(display, w, "WM_NAME", &nitems, name);
  if(status != 0)
  {
  	printf("seamlessrdpshell[get_window_name]: enable to get atom WM_NAME\n");
    return False;
  }
  if(name == 0)
  {
  	printf("seamlessrdpshell[get_window_name]: no windows name in atom WM_NAME\n");
    return False;
  }
  printf("seamlessrdpshell[get_window_name]: windows name : %s\n",*name);
  return True;
}


int get_window_type(Display* display, Window w, Atom* atom)
{
  unsigned char* data;
  unsigned long nitems;
  int status;
  *atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL",False);

  status = get_property(display, w, "_NET_WM_WINDOW_TYPE", &nitems, &data);
  if(status != 0)
  {
  	printf("seamlessrdpshell[get_window_type]: xrdp-chansrv: enable to window type\n");
    return 1;
  }
  if(data == 0)
  {
  	printf("seamlessrdpshell[get_window_type]: no window type\n");
    return 0;
  }

  *atom = (Atom) \
   	    ( \
   	      (*((unsigned char*) data+0) << 0) | \
   	      (*((unsigned char*) data + 1) << 8) | \
   	      (*((unsigned char*) data+ 2) << 16) | \
   	      (*((unsigned char*) data+ 3) << 24) \
   	    );

  printf("seamlessrdpshell[get_window_type]: window type : %s\n", XGetAtomName(display, *atom));
  return 0;
}

/*****************************************************************************/
int get_window_pid(Display* display, Window w, int* pid){
  unsigned char* data;
  unsigned long     nitems;
  int status;

  status = get_property(display, w, "_NET_WM_PID", &nitems, &data);
  if(status != 0)
  {
  	printf("seamlessrdpshell[get_window_pid]: enable to get pid of window %i\n", (int)w);
    return False;
  }
  if(data == 0)
  {
  	printf("seamlessrdpshell[get_window_pid]: no pid for window %i\n", (int)w);
    return False;
  }
  *pid = (int) \
		( \
		  (*((unsigned char*) data+0) << 0) | \
		  (*((unsigned char*) data + 1) << 8) | \
		  (*((unsigned char*) data+ 2) << 16) | \
		  (*((unsigned char*) data+ 3) << 24) \
		); \
		printf("seamlessrdpshell[get_window_pid]: window pid: %i\n", *pid);
  return True;
}

/*****************************************************************************/
int get_parent_window(Display* display, Window w, Window* parent){
  unsigned char* data;
  unsigned long nitems;
  int status;


  status = get_property(display, w, "WM_TRANSIENT_FOR", &nitems, &data);
  if(status != 0)
  {
  	printf("seamlessrdpshell[get_parent_window]: Enable to get parent of window %i\n", (int)w);
    *parent = 0;
    return False;
  }
  if(data == 0)
  {
  	printf("seamlessrdpshell[get_parent_window]: no parent window for window %i\n", (int)w);
    *parent = 0;
    return False;
  }


  *parent = (Window) \
		( \
		  (*((unsigned char*) data+0) << 0) | \
		  (*((unsigned char*) data + 1) << 8) | \
		  (*((unsigned char*) data+ 2) << 16) | \
		  (*((unsigned char*) data+ 3) << 24) \
		); \
		printf("seamlessrdpshell[get_parent_window]: parent window for window %i : %i\n", (int)w, (int)parent);
  return True;
}
