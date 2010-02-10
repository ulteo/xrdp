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
#include "seamlessrdpshell.h"
#include "xutils.h"

/* external function declaration */
extern char** environ;

static pthread_mutex_t mutex;
static int message_id;
static int sock;
static Display* display;
static Window_list window_list;
struct log_conf *l_config;
/*****************************************************************************/
int
error_handler(Display* display, XErrorEvent* error)
{
  char text[256];
  XGetErrorText(display, error->error_code, text, 255);
  printf("seamlessrdpshell[error_handler]: xrdp-chansrv: error [%s]\n", text);
  return 0;
}

/*****************************************************************************/
int
send_message(char* data, int data_len)
{
	if(user_channel_send(sock, data, data_len)<0)
	{
		perror("Error");
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
  printf("seamlessrdpshell[handler]: a processus has ended\n");
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

	for(i=0 ; i < strlen(application) ; i++){
		if(application[i] == '\n')
			application[i] = '\0';
	}

	printf("exec application %s\n",application);
	if ( pid < 0 ) {
		perror ( "echec du fork");
		exit(-1);
	}
	/* nous somme dans le processus père */
	if ( pid > 0 ){
		char* buffer = malloc(1024);
       /* l'application est crée */
		printf("je suis le pere\n");
		printf("pid de l'application : %i\n", pid);
		free(buffer);
		return 0;
	}
	int status;
	/* nous sommes dans le processus fils */
	/*TODO : trouver le path*/
	printf("je suis le fils\n");
	sprintf(path,"/usr/bin/%s",application);
	printf("path : '%s'\n",path);
	printf("application : '%s'\n",application);
	execle( path, application, (char*)0, environ);
	wait(&status);
	exit(0);
}


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

	for(i=0; i < window_list.item_count; i++){
		witem = &window_list.list[i];
		XGetGeometry(display,witem->window_id,&root,&x,&y,&width, &height,&border,&depth );
		printf("GEOM OUT: %i,%i,%i,%i,%i,%i\n",x,y,width,height,border,depth);

		win_in = get_in_window(display, witem->window_id);

		if(is_good_window(display, witem->window_id)==0){
			proper_win = witem->window_id;
			printf("%i is a good window\n",(int)proper_win);
		}
		else{
			if(is_good_window(display, win_in) == 0){
				proper_win = win_in;
				printf("%i is a good window\n",(int)proper_win);
			}
			else{
				printf("No good window\n");
				return;
			}

		}


		get_window_name(display,proper_win,&name);

		get_window_type(display,proper_win,&type);
		get_window_pid(display, proper_win, &pid);
		get_parent_window(display, proper_win, &parent_id);

		if(type == XInternAtom(display, "_NET_WM_STATE_MODAL", False)){
			flags = SEAMLESSRDP_CREATE_MODAL;
			printf("c'est une fenetre modal\n");
		}



		if(type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False)){
			flags = SEAMLESSRDP_CREATE_TOPMOST;
		}

		if(parent_id == 0 && type != XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False)){
				printf("Enable to create the window\n");
				flags = SEAMLESSRDP_CREATE_TOPMOST;
				parent_id = -1;
		}

		if(win_in == 0){
			win_in = witem->window_id;
		}

		buffer=malloc(1024);
		window_id = malloc(11);

		sprintf(window_id, "0x%08x",(int)witem->window_id);
		sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", message_id,window_id,pid,(int)parent_id,flags );
		printf("%s",buffer);
		send_message(buffer, strlen(buffer));

		printf("app.title : %s\n", name);
		sprintf(buffer, "TITLE,%i,%s,%s,0x%08x\n", message_id,window_id,name,0 );
		printf("%s",buffer);
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, x,y,width,height,0 );
		printf("%s",buffer);
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,0,0 );
		printf("%s",buffer);
		send_message(buffer, strlen(buffer));

		free(buffer);
		free(window_id);
	}
}




int exec_app(char* application, char* window_id){
	char* path = malloc(256);
	int pid;


	pid = fork ();
	int i;

	for(i=0 ; i < strlen(application) ; i++){
		if(application[i] == '\n')
			application[i] = '\0';
	}


	printf("exec application %s\n",application);
	if ( pid < 0 ) {
		perror ( "echec du fork");
		exit(-1);
	}
	/* nous somme dans le processus père */
	if ( pid > 0 ){
		char* buffer = malloc(1024);
       /* l'application est crée */
		printf("je suis le pere\n");
		printf("pid de l'application : %i\n", pid);

		sprintf(buffer, "APP_ID,%i,%s,%i\n", message_id, window_id, pid );
		printf("%s",buffer);
		send_message(buffer, strlen(buffer));
		free(buffer);
		return 0;
	}
	int status;
	/* nous sommes dans le processus fils */
	/*TODO : trouver le path*/
	printf("je suis le fils\n");
	sprintf(path,"/usr/bin/%s",application);
	printf("path : '%s'\n",path);
	printf("application : '%s'\n",application);
	execle( path, application, (char*)0, environ);
	/*execle( "/usr/bin/gedit", "gedit", (char*)0, environ);*/
	wait(&status);
	exit(0);
}


int get_next_token(char** message, char** token){
	char *temp;
	if (*message[0] == '\0'){
		return 0;
	}
	*token = *message;
	temp = strchr(*message, ',');
	if(temp == 0)
		temp = strchr(*message, '\0');

	*temp = '\0';
	/*printf("get_next_token : %s\n", *token);*/

	*message+= strlen(*message)+1;
	/*printf("suite : %s\n", *message);*/
	return 1;

}

/*****************************************************************************/
int move_window2(XEvent* ev){
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
    printf("xrdp-chansrv: window (%i) remove during the operation", (int)ev->xconfigure.window);
    return 0;
  }
  win_in = get_in_window(display, witem->window_id);
  if(win_in == 0)
  {
    printf( "xrdp-chansrv: unknow window : %i", (int)witem->window_id);
    return 0;
  }
  if(witem->state == SEAMLESSRDP_MINIMIZED)
  {
    printf( "xrdp-chansrv: the window %i is minimized", (int)witem->window_id);
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
    printf( "xrdp-chansrv: cannot move window at the same position");
  }
  if(width !=ev->xconfigure.width || height != ev->xconfigure.height){
    XResizeWindow(display, witem->window_id , ev->xconfigure.width,ev->xconfigure.height);
  }
  else
  {
    printf( "xrdp-chansrv: cannot resize the window at the same dimension");
  }
  return 1;
}





int change_state(Window w, int state ){
	Window_item *witem;
	Window_get(window_list, w, witem);
	printf("change state \n");

	if(witem == 0){
		printf("unknow window\n");
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





void process_message(char* buffer){
	char* token1, *token2, *token3, *token4, *token5, *token6, *token7 ;
	char* temp;
	temp = buffer;
	char* buffer2 = malloc(1024);
	XEvent ev;


	printf("Seamlessrdpshell :: message to process : %s\n",buffer);
	if(get_next_token(&temp, &token1)){
		/*printf("\t %s token1 = %s\n", temp, token1);*/
	}
	if(get_next_token(&temp, &token2)){
			/*printf("\t token2 = %s\n", token2);*/
		}
	if(get_next_token(&temp, &token3)){
			/*printf("\t token3 = %s\n", token3);*/
		}

	if(get_next_token(&temp, &token4)){
			/*printf("\t token4 = %s\n", token4);*/
		}
	if(get_next_token(&temp, &token5)){
		/*printf("\t %s token5 = %s\n", temp, token5);*/
	}
	if(get_next_token(&temp, &token6)){
		/*printf("\t %s token6 = %s\n", temp, token6);*/
	}
	if(get_next_token(&temp, &token7)){
		/*printf("\t %s token7 = %s\n", temp, token7);*/
	}

	/* message process */
	if(strcmp("START_APP", token1)==0 && strlen(token4)>0){
		exec_app(token4, token3);

		free(buffer2);
		return;
	}
	/* message process */
	if(strcmp("SPAWN", token1)==0 && strlen(token3)>0){
		spawn_app(token3);

		free(buffer2);
		return;
	}

	if(strcmp("STATE", token1)==0 ){
		int state = hex2int(token4);
		Window w = (Window)hex2int(token3);

		printf("state : %i for the window %i\n",state, (int)w);
		change_state(w, state);

		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));

		free(buffer2);
		return;
	}

	if(strcmp("POSITION", token1)==0 ){
		ev.xconfigure.window = (Window) hex2int(token3);
		ev.xconfigure.x = atoi(token4);
		ev.xconfigure.y = atoi(token5);
		ev.xconfigure.width = atoi(token6);
		ev.xconfigure.height = atoi(token7);

		pthread_mutex_lock(&mutex);
		move_window2( &ev);
		pthread_mutex_unlock(&mutex);

		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);

		send_message(buffer2, strlen(buffer2));

		free(buffer2);
		return;
		}

	if(strcmp("FOCUS", token1)==0 ){
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);

		send_message(buffer2, strlen(buffer2));

		free(buffer2);
		return;
	}
	if(strcmp("ZCHANGE", token1)==0 ){

		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);

		send_message(buffer2, strlen(buffer2));

		free(buffer2);
		return;
	}

	if(strcmp("ACK", token1)==0 ){
		free(buffer2);
		return;
	}
	if(strcmp("SYNC", token1)==0 ){
		synchronize();
		free(buffer2);
		return;
	}


	printf("MESSAGE INCONNU : %s\n",token1);

}


int
get_property_value(Window wnd, char *propname, long max_length,
		   unsigned long *nitems_return, unsigned char **prop_return, int nowarn)
{
	int result;
	Atom property;
	Atom actual_type_return;
	int actual_format_return;
	unsigned long bytes_after_return;

	property = XInternAtom(display, propname, True);
	if (property == None)
	{
		fprintf(stderr, "Atom %s does not exist\n", propname);
		return (-1);
	}

	result = XGetWindowProperty(display, wnd, property, 0,	/* long_offset */
				    max_length,	/* long_length */
				    False,	/* delete */
				    AnyPropertyType,	/* req_type */
				    &actual_type_return,
				    &actual_format_return,
				    nitems_return, &bytes_after_return, prop_return);

	if (result != Success)
	{
		fprintf(stderr, "XGetWindowProperty failed\n");
		return (-1);
	}

	if (actual_type_return == None || actual_format_return == 0)
	{
		if (!nowarn)
			fprintf(stderr, "Window is missing property %s\n", propname);
		return (-1);
	}

	if (bytes_after_return)
	{
		fprintf(stderr, "%s is too big for me\n", propname);
		return (-1);
	}

	if (actual_format_return != 32)
	{
		fprintf(stderr, "%s has bad format\n", propname);
		return (-1);
	}

	return (0);
}










int is_modal_window(Window w, Atom* atom){
	unsigned char* data;
	Atom     actual_type;
	int      actual_format;
	unsigned long     nitems;
	unsigned long     bytes;
	int status;
	int i;
	Atom modal = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);

    status = XGetWindowProperty(
    		display,
            w,
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
		printf("Enable to get atom _NET_WM_STATE\n");
		return False;
    }
    if(data == 0){
    	printf("no windows state : \n");
    	return False;
    }

    printf("state size : %i\n",(int)nitems);

    for(i=0 ; i<nitems ; i++){
		*atom = (Atom) \
			( \
			  (*((unsigned char*) data+0) << 0) | \
			  (*((unsigned char*) data + 1) << 8) | \
			  (*((unsigned char*) data+ 2) << 16) | \
			  (*((unsigned char*) data+ 3) << 24) \
			); \
		printf("Atom state : %i\n",(int)*atom);
		printf("windows state : %s\n",XGetAtomName(display, *atom));
		if(*atom == modal ){
			return True;
		}
    	data+=sizeof(Atom);
    }
    return False;
}

Window get_real_window(Window w){
	unsigned char* data;
	Window root;
	Window parent;
	Window *children;
	unsigned int nchildren;
	Atom     actual_type;
	int      actual_format;
	unsigned long     nitems;
	unsigned long     bytes;
	int status;

    status = XGetWindowProperty(
    		display,
    		w,
            XInternAtom(display, "_NET_WM_PID", True),
            0,
            (~0L),
            False,
            AnyPropertyType,
            &actual_type,
            &actual_format,
            &nitems,
            &bytes,
            &data);

    if((status == 0) && (data != 0)){
    	printf("real window : %i\n",(int)w);
		return w;
    }



	if (!XQueryTree(display, w, &root, &parent, &children, &nchildren) || nchildren == 0){
		printf("No child windows\n");
		return 0;
	}

    status = XGetWindowProperty(
    		display,
    		children[nchildren-1],
            XInternAtom(display, "_NET_WM_PID", True),
            0,
            (~0L),
            False,
            AnyPropertyType,
            &actual_type,
            &actual_format,
            &nitems,
            &bytes,
            &data);

    if((status != 0) || (data == 0)){
		printf("%i did not contain the right information\n", (int)children[nchildren-1]);
		return 0;
    }

	printf("real window : %i\n",(int)children[nchildren-1]);
	return children[nchildren-1];
}


int get_icon(Window win_in, Window win_out ){
	int i, k, message_id, height, width, count, message_length, pixel;
	char a,r,g,b;
	unsigned long nitems;
	int *data = NULL;
	char* buffer = malloc(1024);
	char* buffer_pos = buffer;
	char* window_id = malloc(11);

	if(get_property(display, win_in, "_NET_WM_ICON", &nitems, (unsigned char**)&data) != Success){
		printf("No icon for the application %i\n",(int)win_out);
		return 1;
	}


	if(nitems < 16*16+2){
		printf("No proper icon for the application %i\n",(int)win_out);
		return 1;
	}


	sprintf(window_id, "0x%08x",(int)win_out);
	//analyzing of _NET_WM_ICON atom content
	k=0;
	while( k < nitems ) {
		printf("on en est au %i\n",k);
		message_length = 0;
		message_id = 0;
		width = data[k];
		height = data[k+1];
		k+=2;

		printf("new Icon : %i X %i\n",width, height);

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

			if(message_length >1000 || i == width * height -1){
				sprintf(buffer_pos,"\n");
				send_message(buffer, strlen(buffer));
				printf("%s",buffer);

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


	printf("Seamlessrdpshell :: map d'une fenetre : %i\n",(int)win_out);
	Window_get(window_list, win_out, witem);
	if(witem != 0){
		sprintf(window_id, "0x%08x",(int)win_out);
		sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,0,0 );
		printf("%s",buffer);
		send_message(buffer, strlen(buffer));
		printf("%i already exist\n",(int)win_out);
		free(window_id);
		free(buffer);
		return;
	}
	XGetGeometry(display,win_out,&root,&x,&y,&width, &height,&border,&depth );
	printf("GEOM OUT: %i,%i,%i,%i,%i,%i\n",x,y,width,height,border,depth);


	win_in = get_in_window(display, win_out);
	XGetGeometry(display,win_in,&root,&x,&y,&width, &height,&border,&depth );
	printf("GEOM IN: %i,%i,%i,%i,%i,%i\n",x,y,width,height,border,depth);

	if(is_good_window(display, win_out)==0){
		proper_win = win_out;
		printf("%i is a good window\n",(int)proper_win);
	}
	else{
		if(is_good_window(display, win_in) == 0){
			proper_win = win_in;
			printf("%i is a good window\n",(int)proper_win);
		}
		else{
			printf("No good window\n");
			free(window_id);
			free(buffer);
			return;
		}

	}


	XGetWindowAttributes(display,win_out, &attributes);
	if(attributes.class == 2){
		printf("bad attributes : %i\n",(int)proper_win);
		free(window_id);
		free(buffer);

		return;
	}



	get_window_name(display,proper_win,&name);

	get_window_type(display,proper_win,&type);
	get_window_pid(display, proper_win, &pid);
	get_parent_window(display, proper_win, &parent_id);


	if(type == XInternAtom(display, "_NET_WM_STATE_MODAL", False)){
		flags = SEAMLESSRDP_CREATE_MODAL;
		printf("c'est une fenetre modal\n");
	}



	if(type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False)){
		flags = SEAMLESSRDP_CREATE_TOPMOST;
	}

	if(parent_id == 0 && type != XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False)){
			printf("Enable to create the window\n");
			flags = SEAMLESSRDP_CREATE_TOPMOST;
			parent_id = -1;
	}

	if(win_in == 0){
		win_in = win_out;
	}
	printf("class : %i\n",attributes.class);

	sprintf(window_id, "0x%08x",(int)win_out);
	sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", message_id,window_id,pid,(int)parent_id,flags );

	printf("%s",buffer);
	send_message(buffer, strlen(buffer));
	printf("app.title : %s\n", name);

	sprintf(buffer, "TITLE,%i,%s,%s,0x%08x\n", message_id,window_id,name,0 );
	printf("%s",buffer);
	send_message(buffer, strlen(buffer));
	get_icon( win_in, win_out);

	sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, attributes.x,attributes.y,attributes.width,attributes.height,0 );
	printf("%s",buffer);
	send_message(buffer, strlen(buffer));
	sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,0,0 );
	printf("%s",buffer);
	send_message(buffer, strlen(buffer));
	free(window_id);
	free(buffer);
	Window_add(window_list,win_out);
	Window_dump(window_list);

}



int get_state(Window w){
	unsigned char* data;
	Atom     actual_type;
	int      actual_format;
	unsigned long     nitems;
	unsigned long     bytes;
	Atom atom = 0;
	int i;
	int status;
	Window_item* witem;
	Window real_window = w;/*get_real_window(process,w);*/
	int state = SEAMLESSRDP_NORMAL;
	Window_get(window_list, w, witem);
	if(w == 0){
		printf("c'est zero\n");
		return 0;
	}

	printf("get _NET_WM_STATE\n");
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

    printf("count of state : %i\n",(int)nitems);


    if(status != 0){
		printf("Enable to get atom _NET_WM_STATE\n");
		return 0;
    }
    if(nitems == 0){
    	printf("no windows state : \n");
    	return 0;
    }

    printf("%i state found\n",(int)nitems);


    for(i = 0; i<nitems ; i++){
		atom = (Atom) \
			( \
			  (*((unsigned char*) data+0) << 0) | \
			  (*((unsigned char*) data + 1) << 8) | \
			  (*((unsigned char*) data+ 2) << 16) | \
			  (*((unsigned char*) data+ 3) << 24) \
			); \
		printf("Atom state : %i\n",(int)atom);
		printf("windows state : %s[%i]\n",XGetAtomName(display, atom),(int)atom);
	    if(atom == XInternAtom(display, "_NET_WM_STATE_HIDDEN", True)){
	        	state =SEAMLESSRDP_MINIMIZED;
	        	return state;
	    }
	    if(atom == XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True) || atom == XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True)){
	    	state =SEAMLESSRDP_MAXIMIZED;
	    }
		data+=sizeof(Atom);

    }

	printf("STATE for %i : %s\n",(int)real_window, XGetAtomName(display, atom));

    return state;
}




void destroy_window(Window w){
	char* window_id;
	char* buffer;
	Window_item *witem;
	Window_get(window_list,w, witem);
	if(witem->state == SEAMLESSRDP_MINIMIZED){
		printf("enable to destroy a minimize window \n");
		return ;
	}

	window_id = malloc(11);
	buffer = malloc(1024);

	printf("Seamlessrdpshell :: destruction d'une fenetre : %i\n",(int)w);
	sprintf(window_id, "0x%08x",(int)w);
	sprintf(buffer, "DESTROY,%i,%s,%08x\n", message_id,window_id,0 );
	printf("\t %s\n",buffer);
	send_message(buffer, strlen(buffer));
	Window_del(window_list, w);
	free(window_id);
	free(buffer);

}



void move_window(Window w, int x, int y, int width, int height){
	char* window_id;
	char* buffer;

	Window win_in = get_in_window(display, w);
	Window_item* witem;

	Window_get(window_list, w, witem);
	if(witem == 0){
		Window_get(window_list,win_in, witem);
		if(witem == 0){
			printf("unknowed window\n");
			return;
		}
	}
	window_id = malloc(11);
	buffer = malloc(1024);

	printf("move_window : state = %i\n",witem->state);
	if(witem->state == SEAMLESSRDP_MAXIMIZED){
		printf("move_window : state = maximized\n");
		printf("windows id : %i\n",(int)witem->window_id);
		sprintf(window_id, "0x%08x",(int)witem->window_id);
	    sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, -4, -4, width, height, 0 );
	    printf("%s",buffer);
	}
	else{

		printf("windows id : %i\n",(int)witem->window_id);
		sprintf(window_id, "0x%08x",(int)witem->window_id);
		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,window_id, x, y, width, height, 0 );
		printf("%s",buffer);
	}
	send_message(buffer, strlen(buffer));
	free(window_id);
	free(buffer);

}




void check_window_state(){
	char* window_id;
	char* buffer;

	Window win;
	Window win_in;
	Window_item *witem;
	Window proper_win = 0;
	printf("check windows states \n");
	int i;
	int state;
	int count = window_list.item_count;
	for(i=0 ; i < count; i++){
		witem = &window_list.list[i];
		win = window_list.list[i].window_id;
		win_in = get_in_window(display, win);
		if(is_good_window(display, win) == 0){
			proper_win = win;
		}
		if(is_good_window(display, win_in) == 0){
			proper_win = win_in;
		}
		if(proper_win == 0){
			printf("No good win\n");
			return;
		}


		state = get_state( proper_win);

		printf("check_window_state : %i\n",state);



		if (state != window_list.list[i].state){
			printf("\tstate for window %i has change for : %i\n",(int)window_list.list[i].window_id, state);
			window_list.list[i].state = state;
			window_id = malloc(11);
			buffer = malloc(1024);

			sprintf(window_id, "0x%08x",(int)win);
			sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,window_id,state,0 );
			printf("%s",buffer);

			send_message(buffer, strlen(buffer));
			free(window_id);
			free(buffer);
		}
	}
}



void *thread_Xvent_process (void * arg)
{
	Window w;
	Window root_windows;
	Window_item* witem;
	Window win_in;

	root_windows = DefaultRootWindow(display);
	printf("windows root ID : %i\n",(int)root_windows);


    XSelectInput(display, root_windows, SubstructureNotifyMask|PropertyChangeMask);
    printf("Seamlessrdpshell :: begin the event loop \n");

    while (1) {
        XEvent ev;
        XNextEvent (display, &ev);
    	pthread_mutex_lock(&mutex);


    	check_window_state();

    	switch(ev.type){

    	case ConfigureNotify:
        	w = ev.xconfigure.window;
        	printf("Seamlessrdpshell :: move d'une fenetre : %i\n",(int)w);


        	move_window(w, ev.xconfigure.x,ev.xconfigure.y,ev.xconfigure.width,ev.xconfigure.height);

        	break;


        case MapNotify:
        	w = ev.xmap.window;
        	create_window( w);
        	break;

        case DestroyNotify:
        	w = ev.xdestroywindow.window;
        	printf("Seamlessrdpshell :: destroy d'une fenetre : %i\n",(int)w);
        	break;

        case UnmapNotify:
        	w = ev.xunmap.window;
        	printf("Seamlessrdpshell :: unmap d'une fenetre : %i\n",(int)w);
        	Window_dump(window_list);

        	Window_get(window_list,w, witem);
        	if(witem == 0){
        		win_in = get_in_window(display, w);
        		Window_get(window_list,win_in, witem);
        		if(witem == 0){
        			printf("unknowed window\n");
        			break;
        		}
        	}

        	printf("\t destroy window\n");
        	destroy_window( w);
        	break;

        default:
        	printf("type : %i\n", ev.type);
        	break;

        }
    	pthread_mutex_unlock(&mutex);
      }
		XCloseDisplay(display);


	pthread_exit (0);
}






void *thread_vchannel_process (void * arg)
{
	char* buffer = malloc(1024);

	signal(SIGCHLD, handler);
	printf("sending HELLO to %i\n",sock);
	sprintf(buffer, "HELLO,%i,0x%08x\n", 0, 0);
	send_message(buffer, strlen(buffer));
	while(1){
		if(user_channel_receive(sock, buffer ) == ERROR){
			perror("Error while receiving data\n");
			sleep(3);
		}
		printf("\t CHANNEL_MSG\n");
		printf("\t data : %s\n",buffer);
		process_message(buffer);
	}
	pthread_exit (0);
}



int main(int argc, char** argv, char** environ)
{
	vchannel_read_logging_conf(&l_config);
	log_start(&l_config);
	Window_list_init(window_list);
	pthread_t Xevent_thread, Vchannel_thread;
	void *ret;
	int count = 12;

	pthread_mutex_init(&mutex, NULL);
	message_id = 0;

	printf("seamlessrdpshell[main]: socket creation\n");

	while( count != 0)
	{
		sock = user_channel_connect("seamrdp");
		if(sock == ERROR)
		{
			perror("seamlessrdpshell[main]: retry connection\n");
			sleep(1);
			count--;
		}
		else
		{
			break;
		}
	}
	if(sock == ERROR)
	{
		perror("seamlessrdpshell[main]: "
				"error while connecting to vchannel service\n");
		return 1;
	}
	XInitThreads();
	printf("seamlessrdpshell[main]"
			"opening the default display : %s\n",getenv("DISPLAY"));

	if ((display = XOpenDisplay(0))== 0){
		printf("seamlessrdpshell[main]: "
				"unable to open the default display : %s \n",getenv("DISPLAY"));
		pthread_exit(0);
	}
	XSynchronize(display, 1);
	XSetErrorHandler(error_handler);


	if (pthread_create (&Xevent_thread, NULL, thread_Xvent_process, (void*)0) < 0)
	{
		fprintf (stderr, "seamlessrdpshell[main]: "
				"pthread_create error for thread : Xevent_thread\n");
		exit (1);
	}
	if (pthread_create (&Vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		fprintf (stderr, "seamlessrdpshell[main]: "
				"pthread_create error for thread : Vchannel_thread\n");
		exit (1);
	}

	(void)pthread_join (Xevent_thread, &ret);
	(void)pthread_join (Vchannel_thread, &ret);
	pthread_mutex_destroy(&mutex);

	return 0;
}
