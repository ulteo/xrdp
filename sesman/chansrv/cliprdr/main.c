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
#include "uni_rdp.h"

/* external function declaration */
extern char** environ;

static Display* display;
static int cliprdr_channel;
struct log_config* l_config;
static Window wclip;
static int running;
pthread_cond_t reply_cond;
pthread_mutex_t mutex;

/* Atoms of the two X selections we're dealing with: CLIPBOARD (explicit-copy) and PRIMARY (selection-copy) */
static Atom clipboard_atom;
static Atom primary_atom;
static Atom targets_atom;
static Atom timestamp_atom;
static Atom format_string_atom;
static Atom format_utf8_string_atom;
static Atom format_unicode_atom;
static Atom xrdp_clipboard;
static Atom targets[MAX_TARGETS];
static int num_targets;
static char* clipboard_data = 0;
static int clipboard_size;

/*****************************************************************************/
void APP_CC
cliprdr_wait_reply()
{

  if (pthread_cond_wait(&reply_cond, &mutex) != 0) {
    perror("pthread_cond_timedwait() error");
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_cliprdr[cliprdr_wait_reply]: "
				"pthread_mutex_lock()");
    return;
  }
}

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
void cliprdr_send_capability()
{
	/* this message is ignored by rdp applet */
	struct stream* s;

	make_stream(s);
	init_stream(s,1024);

	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_send_capability]:");
	/* clip header */
	out_uint16_le(s, CB_CLIP_CAPS);                        /* msg type */
	out_uint16_le(s, 0x00);                                /* msg flag */
	out_uint32_le(s, 0);                                   /* msg size */
	/* we only support one capability for now */
	out_uint16_le(s, 1);                                   /* cCapabilitiesSets */
	out_uint8s(s, 16);                                     /* pad */
	/* CLIPRDR_CAPS_SET */
	out_uint16_le(s, CB_CAPSTYPE_GENERAL);                 /* capabilitySetType */
	out_uint16_le(s, 92);                                  /* lengthCapability */
	out_uint32_le(s, CB_CAPS_VERSION_1);                   /* version */
	out_uint32_le(s, 0);                                   /* general flags */


	s_mark_end(s);
	cliprdr_send(s);
	free_stream(s);

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
void cliprdr_send_format_list()
{
	struct stream* s;

	make_stream(s);
	init_stream(s,1024);

	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_send_format_list_response]:");
	/* clip header */
	out_uint16_le(s, CB_FORMAT_LIST);                      /* msg type */
	out_uint16_le(s, 0);                                   /* msg flag */
	out_uint32_le(s, 72);                                  /* msg size */
	/* we only send one format */
	out_uint32_le(s, CF_TEXT);                             /* Format Id */
	out_uint8s(s, 32);
	out_uint32_le(s, CF_UNICODETEXT);                      /* Format Id */
	out_uint8s(s, 32);

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
void cliprdr_send_data_request()
{
	struct stream* s;

	make_stream(s);
	init_stream(s,1024);

	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_send_data_request]:");
	/* clip header */
	out_uint16_le(s, CB_FORMAT_DATA_REQUEST);              /* msg type */
	out_uint16_le(s, 0);                                   /* msg flag */
	out_uint32_le(s, 1);                                   /* msg size */
	out_uint32_le(s, CF_UNICODETEXT);                      /* we want CF_UNICODE */

	s_mark_end(s);
	cliprdr_send(s);
	free_stream(s);

}

/*****************************************************************************/
void cliprdr_send_data(int request_type)
{
	struct stream* s;
	int uni_clipboard_len = (clipboard_size+1)*2;
	int packet_len = uni_clipboard_len + 12;
	char* temp;

	make_stream(s);
	init_stream(s,packet_len);

	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_send_data]:");
	/* clip header */
	out_uint16_le(s, CB_FORMAT_DATA_RESPONSE);             /* msg type */
	out_uint16_le(s, 0);                                   /* msg flag */
	out_uint32_le(s, uni_clipboard_len);                   /* msg size */
	temp = s->p;
	uni_rdp_out_str(s, clipboard_data, uni_clipboard_len);


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
		log_message(l_config, LOG_LEVEL_WARNING, "vchannel_cliprdr[cliprdr_process_format_list]: "
				"Long format list is not yet supported");
		return 1;
	}

	/* short format announce */
	format_number = size / 36;
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_process_format_list]: "
			"%i formats announced", format_number);

	s->p = s->end;

	cliprdr_send_format_list_response();
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_process_format_list]: "
			"get the clipboard");
	XSetSelectionOwner(display, clipboard_atom, wclip, CurrentTime);
	if (XGetSelectionOwner(display, clipboard_atom) != wclip)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "vchannel_cliprdr[cliprdr_process_format_list]: "
				"Unable to set clipboard owner");
	}
	return 0;
}


/*****************************************************************************/
int cliprdr_process_data_request(struct stream* s, int msg_flags, int size)
{
	int request_type = 0;
	if (clipboard_data == 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "vchannel_cliprdr[cliprdr_process_data_request]: "
				"No clipboard data");
		return 1;
	}

	in_uint32_le(s, request_type);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_process_data_request]: "
			"Request for data of type %i", request_type);
	cliprdr_send_data(request_type);
	return 0;
}

/*****************************************************************************/
int cliprdr_process_data_request_response(struct stream* s, int msg_flags, int size)
{
	if (msg_flags == CB_RESPONSE_FAIL)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "vchannel_cliprdr[cliprdr_process_data_request_response]: "
				"Unable to get clipboard data");
		return 1;
	}
	log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)s->p, size);
	if (XGetSelectionOwner(display, clipboard_atom) != wclip)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "vchannel_cliprdr[cliprdr_process_data_request_response]: "
				"Xrpd is not the owner of the selection");
		return 1;
	}
	if (clipboard_data != 0)
	{
		g_free(clipboard_data);
	}
	clipboard_data = g_malloc(size, 1);
	clipboard_size = uni_rdp_in_str(s, clipboard_data, size, size);

	return 0;
}


/*****************************************************************************/
void cliprdr_process_message(struct stream* s){
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
			"New message for clipchannel");
	int msg_type;
	int msg_flags;
	int msg_size;

	in_uint16_le(s, msg_type);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
			"Message type : %04x", msg_type);
	in_uint16_le(s, msg_flags);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
			"Message flags : %04x", msg_flags);
	in_uint32_le(s, msg_size);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
			"Message size : %i", msg_size);

	switch (msg_type)
	{
	case CB_FORMAT_LIST :
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
				"Client format list announce");
		cliprdr_process_format_list(s, msg_flags, msg_size);
		//cliprdr_send_format_list();
		break;

	case CB_FORMAT_DATA_RESPONSE :
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
				"Client data request response");
		cliprdr_process_data_request_response(s, msg_flags, msg_size);
		pthread_cond_signal(&reply_cond);

		break;
	case CB_FORMAT_LIST_RESPONSE :
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
				"Client format list response");
		break;

	case CB_FORMAT_DATA_REQUEST :
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
				"Client data request");
		cliprdr_process_data_request(s, msg_flags, msg_size);
		break;

	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[process_message]: "
				"Unknow message type : %i", msg_type);

	}
}

/*****************************************************************************/
static void
cliprdr_clear_selection(XEvent* e)
{
	Window newOwner = XGetSelectionOwner(e->xselectionclear.display, clipboard_atom);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[clear_selection]: "
			"New windows owner : %i", (int)newOwner);

	XConvertSelection(e->xselectionclear.display, clipboard_atom, XA_STRING, xrdp_clipboard, wclip, CurrentTime);
  XSync (e->xselectionclear.display, False);
}

/*****************************************************************************/
static void
cliprdr_get_clipboard(XEvent* e)
{
	Atom type;
	unsigned long len, bytes_left, dummy;
	int format, result;
	unsigned char *data;


	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_get_clipboard]: "
			"New owner %i", e->xselection.requestor);

	XGetWindowProperty (e->xselection.display,
											e->xselection.requestor,
											e->xselection.property,
											0, 0,
											False,
											AnyPropertyType,
											&type,
											&format,
											&len, &bytes_left,
											&data);

	// DATA is There
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_get_clipboard]: "
			"Data type : %s\n", XGetAtomName(e->xselection.display, e->xselection.property));

	if (bytes_left > 0)
	{
		result = XGetWindowProperty (	e->xselection.display,
																	e->xselection.requestor,
																	e->xselection.property,
																	0, bytes_left,
																	0,
																	XA_STRING,
																	&type,
																	&format,
																	&len, &dummy, &data);
		if (result == Success)
		{
			log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "vchannel_cliprdr[cliprdr_get_clipboard]: "
					"New data in clipboard: %s", data);

			if (clipboard_data == 0){
				log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_get_clipboard]: "
						"Update internal clipboard");
				clipboard_data = g_malloc(bytes_left+4, 1);
				g_memcpy(clipboard_data, data, bytes_left);
				clipboard_size = bytes_left;
			}
			else
			{
				g_free(clipboard_data);
				clipboard_data = g_malloc(bytes_left+4, 1);
				g_memcpy(clipboard_data, data, bytes_left);
				clipboard_size = bytes_left;
				log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_get_clipboard]: "
						"Send format list");
				cliprdr_send_format_list();
			}

		}
		else
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_get_clipboard]: "
					"Failed to get clipboard content");
		}
		XFree (data);
	}
	XSetSelectionOwner(e->xselection.display, clipboard_atom, wclip, CurrentTime);
}

/*****************************************************************************/
static void
cliprdr_process_selection_request(XEvent* e)
{
	XSelectionRequestEvent *req;
	XEvent respond;
	req=&(e->xselectionrequest);
	if (req->target == format_utf8_string_atom)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_process_selection_request]: "
				"Update data with %s", clipboard_data);
		cliprdr_send_data_request();
		cliprdr_wait_reply();
		XChangeProperty (req->display,
			req->requestor,
			req->property,
			format_utf8_string_atom,
			8,
			PropModeReplace,
			(unsigned char*) clipboard_data,
			clipboard_size);
		respond.xselection.property=req->property;
	}
	else if (req->target == targets_atom)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[cliprdr_process_selection_request]: "
				"Targets : '%s'", XGetAtomName(req->display, req->property));
		XChangeProperty (req->display, req->requestor, req->property, XA_ATOM, 32, PropModeReplace, (unsigned char*)targets, 1);
		respond.xselection.property=req->property;
	}
	else // Strings only please
	{
		printf ("No String %i\n",
			(int)req->target);
		respond.xselection.property= None;
	}
	respond.xselection.type= SelectionNotify;
	respond.xselection.display= req->display;
	respond.xselection.requestor= req->requestor;
	respond.xselection.selection=req->selection;
	respond.xselection.target= req->target;
	respond.xselection.time = req->time;
	XSendEvent (req->display, req->requestor,0,0,&respond);
	XFlush (req->display);
}



/*****************************************************************************/
void *thread_Xvent_process (void * arg)
{
	Window root_windows;
	XEvent ev;

  primary_atom = XInternAtom(display, "PRIMARY", False);
	clipboard_atom = XInternAtom(display, "CLIPBOARD", False);
	targets_atom = XInternAtom(display, "TARGETS", False);
	timestamp_atom = XInternAtom(display, "TIMESTAMP", False);
	format_string_atom = XInternAtom(display, "STRING", False);
	format_utf8_string_atom = XInternAtom(display, "UTF8_STRING", False);
	format_unicode_atom = XInternAtom(display, "text/unicode", False);
	xrdp_clipboard = XInternAtom(display,"XRDP_CLIPBOARD", False);


	num_targets = 0;
	targets[num_targets++] = format_utf8_string_atom;

	root_windows = DefaultRootWindow(display);
	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
				"Windows root ID : %i", (int)root_windows);
	wclip = XCreateSimpleWindow(display, root_windows,1,1,1,1,1,1,0);

	XSelectInput(display, wclip, PropertyChangeMask);
	log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
				"Begin the event loop ");

	while (running) {
		XNextEvent (display, &ev);
		switch (ev.type)
		{

		case SelectionClear :
			log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
						"XSelectionClearEvent");
			cliprdr_clear_selection(&ev);
			break;

		case SelectionRequest :
			log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
						"XSelectionRequestEvent");
			cliprdr_process_selection_request(&ev);
			break;

		case SelectionNotify :
			log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
						"SelectionNotify");
			cliprdr_get_clipboard(&ev);
			break;

		default:
			log_message(l_config, LOG_LEVEL_DEBUG, "cliprdr[thread_Xvent_process]: "
						"event type :  %i", ev.type);
			break;
		}
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
	cliprdr_send_capability();

	while(running){
		make_stream(s);
		init_stream(s, 1600);

		rv = vchannel_receive(cliprdr_channel, s->data, &length, &total_length);
		if( rv == ERROR )
		{
			log_message(l_config, LOG_LEVEL_ERROR, "vchannel_cliprdr[thread_vchannel_process]: "
					"Invalid message");
			vchannel_close(cliprdr_channel);
			pthread_exit ((void*)1);
		}
		switch(rv)
		{
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "vchannel_cliprdr[thread_vchannel_process]: "
					"Invalid message");
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[thread_vchannel_process]: "
					"Status connected");
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_cliprdr[thread_vchannel_process]: "
					"Status disconnected");
			running = 0;
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

	pthread_cond_init(&reply_cond, NULL);
	pthread_mutex_init(&mutex, NULL);

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

	running = 1;

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
