/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2010
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
#include "cliprdr.h"
#include "xutils.h"

/* external function declaration */
extern char** environ;

static pthread_mutex_t mutex;
static int message_id;
static Display* display;
static int cliprdr_channel;
struct log_config* l_config;
static Window wclip;
static int running;


/* Atoms of the two X selections we're dealing with: CLIPBOARD (explicit-copy) and PRIMARY (selection-copy) */
static Atom clipboard_atom, primary_atom;


/*****************************************************************************/
int
error_handler(Display* display, XErrorEvent* error)
{
  char text[256];
  XGetErrorText(display, error->error_code, text, 255);
  log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[error_handler]: "
  		" Error [%s]", text);
  return 0;
}

/*****************************************************************************/
int APP_CC
cliprdr_send(struct stream* s){
  int rv;
  int length;

	length = (int)(s->end - s->data);

  rv = vchannel_send(cliprdr_channel, s->data, length);
  if (rv != 0)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "vchannel_cliprdr[cliprdr_send]: "
    		"Enable to send message");
  }
  return rv;
}

/*****************************************************************************/
void
handler(int sig)
{
	int pid, statut;
	pid = waitpid(-1, &statut, 0);
	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[handler]: "
  		"A processus has ended");
  return;
}

/*****************************************************************************/
void cliprdr_send_ready()
{
	struct stream* s;

	make_stream(s);
	init_stream(s,1024);

	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_send_ready]:");
	/* clip header */
	out_uint16_le(s, CB_MONITOR_READY);                    /* msg type */
	out_uint16_le(s, 0x00);                                /* msg flag */
	out_uint32_le(s, 0);                                   /* msg size */

	s_mark_end(s);
	cliprdr_send(s);
	free_stream(s);

}

/*****************************************************************************/
void cliprdr_send_format_list_response()
{
	struct stream* s;

	make_stream(s);
	init_stream(s,1024);

	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_send_format_list_response]:");
	/* clip header */
	out_uint16_le(s, CB_FORMAT_LIST_RESPONSE);             /* msg type */
	out_uint16_le(s, CB_RESPONSE_OK);                      /* msg flag */
	out_uint32_le(s, 0);                                   /* msg size */

	s_mark_end(s);
	cliprdr_send(s);
	free_stream(s);

}




/*****************************************************************************/
int cliprdr_process_format_list(struct stream* s, int msg_flags, int size)
{
	int format_number;

	/* long format announce */
	if (msg_flags == CB_ASCII_NAMES)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[cliprdr_process_format_list]: "
				"Long format list is not yet supported");
		return 1;
	}

	/* short format announce */
	format_number = size / 36;
	log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[cliprdr_process_format_list]: "
			"%i formats announced", format_number);

	s->p = s->end;

	cliprdr_send_format_list_response();
	return 0;
}


/*****************************************************************************/
void cliprdr_process_message(struct stream* s){
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[process_message]: "
			"New message for clipchannel");
	int msg_type;
	int msg_flags;
	int msg_size;

	in_uint16_le(s, msg_type);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[process_message]: "
			"Message type : %04x", msg_type);
	in_uint16_le(s, msg_flags);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[process_message]: "
			"Message flags : %04x", msg_flags);
	in_uint32_le(s, msg_size);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[process_message]: "
			"Message size : %i", msg_size);

	switch (msg_type)
	{
	case CB_FORMAT_LIST :
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[process_message]: "
				"Client capabilities announce");
		cliprdr_process_format_list(s, msg_flags, msg_size);
		break;
	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[process_message]: "
				"Unknow message type : %i", msg_type);

	}
}

/*****************************************************************************/
static void
cliprdr_get_clip_data()
{
	Window clipboard_owner;
	Atom type;
	int format;
	unsigned long len, bytes_left;
	unsigned char *data;

	clipboard_owner = XGetSelectionOwner(display, clipboard_atom);
	if(clipboard_owner!=0)
	{
		XConvertSelection(display,clipboard_atom,XA_STRING,XA_PRIMARY,clipboard_owner,CurrentTime);
		XGetWindowProperty(display,clipboard_owner,primary_atom,0,10000000L,0,XA_STRING,&type,&format,&len,&bytes_left,&data);
		//get_property(display, primary_owner,"TARGETS", &nitems, (unsigned char**)&data);
		printf("clipboard data : %s\n", data);
	}

}

/*****************************************************************************/
void *thread_Xvent_process (void * arg)
{
	Window root_windows;

  primary_atom = XInternAtom(display, "PRIMARY", False);
	clipboard_atom = XInternAtom(display, "CLIPBOARD", False);

	root_windows = DefaultRootWindow(display);
	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
				"Windows root ID : %i", (int)root_windows);
	wclip = XCreateSimpleWindow(display, root_windows,1,1,1,1,1,1,0);

	XSelectInput(display, wclip, PropertyChangeMask);
	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
				"Begin the event loop ");

	running = 1;
	while (running) {
		cliprdr_get_clip_data();
		g_sleep(1000);
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
			"Closing display");
	XCloseDisplay(display);
	pthread_exit (0);
}

/*****************************************************************************/
void *thread_vchannel_process (void * arg)
{
	struct stream* s = NULL;
	int rv;
	int length;
	int total_length;

	signal(SIGCHLD, handler);
	cliprdr_send_ready();
	while(1){
		make_stream(s);
		init_stream(s, 1600);

		rv = vchannel_receive(cliprdr_channel, s->data, &length, &total_length);
		if( rv == ERROR )
		{
			log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[thread_vchannel_process]: "
					"Invalid message");
			vchannel_close(cliprdr_channel);
			pthread_exit ((void*)1);
		}
		switch(rv)
		{
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[thread_vchannel_process]: "
					"Invalid message");
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[thread_vchannel_process]: "
					"Status connected");
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[thread_vchannel_process]: "
					"Status disconnected");
			break;
		default:
			cliprdr_process_message(s);
			break;
		}
		free_stream(s);
	}
	pthread_exit (0);
}

/*****************************************************************************/
int
cliprdr_init()
{
  char filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;
  int display_num;

  display_num = g_get_display_num_from_display(g_strdup(g_getenv("DISPLAY")));
	if(display_num == 0)
	{
		g_printf("cliprdr[cliprdr_init]: Display must be different of 0\n");
		return ERROR;
	}
	l_config = g_malloc(sizeof(struct log_config), 1);
	l_config->program_name = "cliprdr";
	l_config->log_file = 0;
	l_config->fd = 0;
	l_config->log_level = LOG_LEVEL_DEBUG;
	l_config->enable_syslog = 0;
	l_config->syslog_level = LOG_LEVEL_DEBUG;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(filename, 255, "%s/cliprdr.conf", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, CLIPRDR_CFG_GLOBAL, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, CLIPRDR_CFG_NAME))
      {
        if( g_strlen(value) > 1)
        {
        	l_config->program_name = (char*)g_strdup(value);
        }
      }
    }
  }
  if (file_by_name_read_section(filename, CLIPRDR_CFG_LOGGING, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, CLIPRDR_CFG_LOG_LEVEL))
      {
      	l_config->log_level = log_text2level(value);
      }
    }
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
	if (cliprdr_init() != LOG_STARTUP_OK)
	{
		g_printf("cliprdr[main]: Enable to init log system\n");
		g_free(l_config);
		return 1;
	}
	if (vchannel_init() == ERROR)
	{
		g_printf("cliprdr[main]: Enable to init channel system\n");
		g_free(l_config);
		return 1;
	}

	pthread_mutex_init(&mutex, NULL);
	message_id = 0;
	cliprdr_channel = vchannel_open("cliprdr");
	if( cliprdr_channel == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "cliprdr[main]: "
				"Error while connecting to vchannel provider");
		g_free(l_config);
		return 1;
	}

	XInitThreads();
	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[main]: "
			"Opening the default display : %s",getenv("DISPLAY"));

	if ((display = XOpenDisplay(0))== 0){
		log_message(l_config, LOG_LEVEL_ERROR, "cliprdr[main]: "
				"Unable to open the default display : %s ",getenv("DISPLAY"));
		g_free(l_config);
		return 1;
	}
	XSynchronize(display, 1);
	XSetErrorHandler(error_handler);


	if (pthread_create (&Xevent_thread, NULL, thread_Xvent_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "cliprdr[main]: "
				"Pthread_create error for thread : Xevent_thread");
		g_free(l_config);
		return 1;
	}
	if (pthread_create (&Vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "cliprdr[main]: "
				"Pthread_create error for thread : Vchannel_thread");
		g_free(l_config);
		return 1;
	}

	(void)pthread_join (Xevent_thread, &ret);
	//(void)pthread_join (Vchannel_thread, &ret);
	pthread_mutex_destroy(&mutex);
	g_free(l_config);
	return 0;
}
