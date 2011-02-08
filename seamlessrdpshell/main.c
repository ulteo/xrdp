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
extern char **environ;

static pthread_mutex_t mutex;
static pthread_mutex_t send_mutex;
static int message_id;
static Display *display;
static Window_list window_list;
static int seamrdp_channel;
struct log_config *l_config;

void check_window_name(Window_item *witem);

/*****************************************************************************/
int error_handler(Display * display, XErrorEvent * error)
{
	char text[256];
	char major_opcode[256];
	char minor_opcode[256];
	XGetErrorText(display, error->error_code, text, 255);
	XGetErrorText(display, error->request_code, major_opcode, 255);
	XGetErrorText(display, error->minor_code, minor_opcode, 255);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[error_handler]: "
		    " Error [%s] Major opcode [%s] Minor opcode [%s] Type: %i ResourceId: %lu",
		    text, major_opcode, minor_opcode, error->type,
		    error->resourceid);
	return 0;
}

/*****************************************************************************/
int send_message(char *data, int data_len)
{
	pthread_mutex_lock(&send_mutex);
	struct stream *s;
	make_stream(s);
	init_stream(s, data_len + 1);
	out_uint8p(s, data, data_len + 1);
	s_mark_end(s);
	if (vchannel_send(seamrdp_channel, s->data, data_len + 1) < 0) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[send_message]: "
			    "Unable to send message");
		pthread_mutex_unlock(&send_mutex);
		return 1;
	}
	message_id++;
	pthread_mutex_unlock(&send_mutex);
	return 0;
}

/*****************************************************************************/
int send_ack(int id)
{
	int ret;
	char* buffer = g_malloc(1024, 1);

	log_message(l_config, LOG_LEVEL_INFO, "XHook[sendAck]: "
		    "Sending ack for message %i", id);

	sprintf(buffer, "ACK,%i,%i\n", message_id, id);
	ret = send_message(buffer, strlen(buffer));
	g_free(buffer);

	return ret;
}

/*****************************************************************************/
void handler(int sig)
{
	int pid, statut;
	pid = waitpid(-1, &statut, 0);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[handler]: "
		    "A processus has ended");
	return;
}

/*****************************************************************************/
void get_app_args(const char *cmdline, struct list *args)
{
	char *args_string = g_strdup(cmdline);
	char *pos_args_string = args_string;
	char *pos = strchr(pos_args_string, ' ');

	while (g_strlen(pos_args_string) != 0) {
		if (pos != NULL) {
			*pos = '\0';
		}
		if (g_strlen(pos_args_string) != 0) {
			list_add_item(args, (long)g_strdup(pos_args_string));
		}
		if (pos == NULL) {
			g_free(args_string);
			return;
		}
		pos_args_string += pos - pos_args_string + 1;
		if (pos[1] == '\"') {
			pos = strchr(pos + 2, '\"');
			if (pos == NULL) {
				g_free(args_string);
				return;
			}
			pos++;
		} else {
			pos = strchr(pos + 1, ' ');
		}
	}
	g_free(args_string);
}

/*****************************************************************************/
int get_app_path(const char *application_name, char *application_path)
{
	char *path_env_var = getenv("PATH");
	char *pos_path_env_var = path_env_var;
	char *pos = strchr(pos_path_env_var, ':');
	while (g_strlen(pos_path_env_var) != 0) {
		if (pos != NULL) {
			*pos = '\0';
		}
		g_snprintf(application_path, 256, "%s/%s", pos_path_env_var,
			   application_name);
		g_printf("test path : %s\n", application_path);
		if (g_file_exist(application_path)) {
			return 0;
		}
		if (pos == NULL) {
			return 1;
		}
		pos_path_env_var += pos - pos_path_env_var + 1;
		*pos = ':';
		pos = strchr(pos + 1, ':');
	}
	return 0;
}

/*****************************************************************************/
int spawn_app(char *cmdline)
{
	char *path = g_malloc(256, 1);
	char *application = NULL;
	struct list *args;
	int status = 0;
	char *pos = NULL;
	int pid = 0;
	int i = 0;

	for (i = 0; i < strlen(cmdline); i++) {
		if (cmdline[i] == '\n')
			cmdline[i] = '\0';
	}
	application = g_strdup(cmdline);
	pos = strchr(application, ' ');
	if (pos != NULL) {
		*pos = '\0';
	}
	log_message(l_config, LOG_LEVEL_INFO, "XHook[spawn_app]: "
		    "Exec app %s", application);

	if (get_app_path(application, path) == 1) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[spawn_app]: "
			    "Unable to find the application %s", application);
		return 1;
	}
	args = list_create();
	args->auto_free = 1;
	get_app_args(cmdline, args);

	g_free(application);

	log_message(l_config, LOG_LEVEL_INFO, "XHook[spawn_app]: "
		    "with arguments %s", cmdline);

	pid = fork();
	if (pid < 0) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[spawn_app]: "
			    "Failed to fork [%s]", strerror(errno));
		g_exit(-1);
	}
	if (pid > 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
			    "Child pid : %i", pid);
		return 0;
	}
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
		    "Child processus");

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
		    " Path : '%s'", path);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[spawn_app]: "
		    " Application name: '%s'", cmdline);
	execvp(path, (char **)args->items);
	list_delete(args);
	wait(&status);
	exit(0);
}

/*****************************************************************************/
void synchronize()
{
	char *buffer;
	char *window_id;
	int i;
	int x, y;
	unsigned int width, height, border, depth;
	Window root;
	Window_item *witem;
	Window parent_id;
	Window win_in;
	Window proper_win = 0;
	Atom type;
	int flags = 0;
	int pid;

	for (i = 0; i < window_list.item_count; i++) {
		witem = &window_list.list[i];
		XGetGeometry(display, witem->window_id, &root, &x, &y, &width,
			     &height, &border, &depth);
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
			    "GEOM OUT: %i,%i,%i,%i,%i,%i", x, y, width, height,
			    border, depth);

		win_in = get_in_window(display, witem->window_id);

		if (is_good_window(display, witem->window_id) == 0) {
			proper_win = witem->window_id;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[synchronize]: "
				    "0x%08lx is a good window", proper_win);
		} else {
			if (is_good_window(display, win_in) == 0) {
				proper_win = win_in;
				log_message(l_config, LOG_LEVEL_DEBUG,
					    "XHook[synchronize]: "
					    "0x%08lx is a good window",
					    proper_win);
			} else {
				log_message(l_config, LOG_LEVEL_ERROR, "XHook[synchronize]: "
					    "No good window");
				return;
			}
		}

		get_window_type(display, proper_win, &type);
		get_window_pid(display, proper_win, &pid);
		get_parent_window(display, proper_win, &parent_id);

		if (type == XInternAtom(display, "_NET_WM_STATE_MODAL", False)) {
			flags = SEAMLESSRDP_CREATE_MODAL;
			log_message(l_config, LOG_LEVEL_INFO, "XHook[synchronize]: "
				    "0x%08lx is a modal windows", proper_win);
		}

		if (type ==
		    XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
				False)) {
			flags = SEAMLESSRDP_CREATE_TOPMOST;
		}

		if (parent_id == 0
		    && type != XInternAtom(display,
					   "_NET_WM_WINDOW_TYPE_NORMAL",
					   False)) {
			flags = SEAMLESSRDP_CREATE_TOPMOST;
			parent_id = -1;
		}

		if (win_in == 0) {
			win_in = witem->window_id;
		}

		buffer = g_malloc(1024, 1);
		window_id = g_malloc(11, 1);

		sprintf(window_id, "0x%08x", (int)witem->window_id);
		sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", message_id,
			window_id, pid, (int)parent_id, flags);
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n",
			message_id, window_id, x, y, width, height, 0);
		send_message(buffer, strlen(buffer));

		sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,
			window_id, 0, 0);
		send_message(buffer, strlen(buffer));

		g_free(buffer);
		g_free(window_id);
	}
}

/*****************************************************************************/
int get_next_token(char **message, char **token)
{
	char *temp;
	if (*message[0] == '\0') {
		return 0;
	}
	*token = *message;
	temp = strchr(*message, ',');
	if (temp == 0) {
		temp = strchr(*message, '\0');
	}

	*temp = '\0';
	*message += strlen(*message) + 1;
	return 1;

}

/*****************************************************************************/
int process_move_action(XEvent * ev)
{
	Window_item *witem;
	Window root;
	int x;
	int y;
	unsigned int width, height, border, depth;
	short flag = 0;
	XWindowAttributes attr;

	Window_get(window_list, ev->xconfigure.window, witem);
	if (witem == 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[process_move_action]: "
			    "Window (0x%08lx) remove during the operation", ev->xconfigure.window);
		return 0;
	}

	if (witem->state == SEAMLESSRDP_MINIMIZED) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[process_move_action]: "
			    "The window 0x%08lx is minimized", witem->window_id);
		return 1;
	}

	XGetWindowAttributes(display, witem->window_id, &attr);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[process_move_action]: "
		    "Wnd 0x%08lx gravity: %s", witem->window_id, gravityToStr(attr.win_gravity));

	if (attr.win_gravity != StaticGravity) {
		ev->xconfigure.x -= attr.x;
		ev->xconfigure.y -= attr.y;
	}

	XGetGeometry(display, witem->win_out, &root, &x, &y, &width, &height,
		     &border, &depth);

	if (x != ev->xconfigure.x || y != ev->xconfigure.y)
		flag += 1;
	if (width != ev->xconfigure.width || height != ev->xconfigure.height)
		flag += 2;

	switch (flag) {
	case 1:
		XMoveWindow(display, witem->win_out, ev->xconfigure.x,
			    ev->xconfigure.y);
		break;
	case 2:
		XResizeWindow(display, witem->win_out, ev->xconfigure.width,
			      ev->xconfigure.height);
		break;
	case 3:
		XMoveResizeWindow(display, witem->win_out, ev->xconfigure.x,
				  ev->xconfigure.y, ev->xconfigure.width,
				  ev->xconfigure.height);
		break;
	}

	XFlush(display);
	return 1;
}

/*****************************************************************************/
int process_destroy_action(Window wnd)
{
	Window_item *witem = NULL;

	Window_get(window_list, wnd, witem);
	if (witem == 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[process_destroy_action]: "
			    "Window (0x%08lx) already removed", wnd);
		return 0;
	}

	XDestroyWindow(display, wnd);
	XFlush(display);

	return 0;
}

/*****************************************************************************/
int process_focus_action(Window wnd)
{
	Window_item *witem = NULL;

	Window_get(window_list, wnd, witem);
	if (witem == 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[process_focus_action]: "
			    "Window (0x%08lx) no longer exists", wnd);
		return 0;
	}

	log_message(l_config, LOG_LEVEL_INFO, "XHook[process_focus_action]: "
		    "Setting focus in window (0x%08lx)", wnd);

	XMapRaised(display, wnd);

	XSetInputFocus(display, wnd, RevertToParent, CurrentTime);
	XFlush(display);

	return 0;
}

/*****************************************************************************/
int change_state(Window w, int state, int recv_msg_id)
{
	Window_item *witem;
	Window_get(window_list, w, witem);

	if (witem == 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[change_state]: "
			    "Unknow window 0x%08lx", w);
		return 0;
	}

	log_message(l_config, LOG_LEVEL_INFO, "XHook[change_state]: "
		    "Change window 0x%08lx state from %i to %i", witem->window_id, witem->state, state);

	if (state == witem->state && witem->waiting_state != NULL) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[change_state]: "
			    "Wnd 0x%08lx already has the state %i", witem->window_id, state);
		send_ack(witem->waiting_state->ack_id);
		witem->waiting_state = NULL;
		return 0;
	}

	if (state == SEAMLESSRDP_NORMAL) {
		Window win_in;
		win_in = get_in_window(display, w);
		if (witem->state == SEAMLESSRDP_MINIMIZED) {
			XMapWindow(display, win_in);
			XFlush(display);
		}
	}

	if (state == SEAMLESSRDP_MAXIMIZED) {
		if (witem->waiting_state) {
			log_message(l_config, LOG_LEVEL_INFO, "XHook[change_state]: "
				    "Window 0x%08lx is already waiting for state change (ack_id: %i state: %i)", witem->window_id, witem->waiting_state->ack_id, witem->waiting_state->state);
			return -1;
		}

		witem->waiting_state = g_malloc(sizeof(StateOrder), 1);

		witem->waiting_state->ack_id = recv_msg_id;
		witem->waiting_state->state = SEAMLESSRDP_MAXIMIZED;
		set_wm_state(display, witem->window_id, STATE_MAXIMIZED_BOTH);
	}
	return 0;
}

/*****************************************************************************/
void process_message(char *buffer)
{
	char *token1, *token2, *token3, *token4, *token5, *token6, *token7;
	char *temp;
	temp = buffer;
	char *buffer2 = g_malloc(1024, 1);
	XEvent ev;
	int recv_msg_id;

	log_message(l_config, LOG_LEVEL_INFO, "XHook[process_message]: "
		    "Message to process : %s", buffer);
	get_next_token(&temp, &token1);
	get_next_token(&temp, &token2);
	get_next_token(&temp, &token3);
	get_next_token(&temp, &token4);
	get_next_token(&temp, &token5);
	get_next_token(&temp, &token6);
	get_next_token(&temp, &token7);

	recv_msg_id = atoi(token2);
	if (recv_msg_id == 0 && strcmp(token2, "0") != 0) {
		log_message(l_config, LOG_LEVEL_WARNING, "XHook[process_message]: "
			    "Received a bad message id : %s", token2);
		return;
	}

	/* message process */
	if (strcmp("SPAWN", token1) == 0 && strlen(token3) > 0) {
		spawn_app(token3);
		g_free(buffer2);
		return;
	}
	if (strcmp("STATE", token1) == 0) {
		int state = hex2int(token4);
		Window w = (Window) hex2int(token3);

		change_state(w, state, recv_msg_id);
		return;
	}

	if (strcmp("POSITION", token1) == 0) {
		ev.xconfigure.window = (Window) hex2int(token3);
		ev.xconfigure.x = atoi(token4);
		ev.xconfigure.y = atoi(token5);
		ev.xconfigure.width = atoi(token6);
		ev.xconfigure.height = atoi(token7);

		pthread_mutex_lock(&mutex);
		process_move_action(&ev);
		pthread_mutex_unlock(&mutex);
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		g_free(buffer2);
		return;
	}

	if (strcmp("DESTROY", token1) == 0) {
		Window wnd = (Window) hex2int(token3);
		g_free(buffer2);
		process_destroy_action(wnd);
		return;
	}

	if (strcmp("FOCUS", token1) == 0) {
		Window wnd = (Window) hex2int(token3);
		process_focus_action(wnd);

		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		g_free(buffer2);
		return;
	}
	if (strcmp("ZCHANGE", token1) == 0) {
		sprintf(buffer2, "ACK,%i,%s\n", message_id, token2);
		send_message(buffer2, strlen(buffer2));
		g_free(buffer2);
		return;
	}

	if (strcmp("ACK", token1) == 0) {
		g_free(buffer2);
		return;
	}
	if (strcmp("SYNC", token1) == 0) {
		synchronize();
		g_free(buffer2);
		return;
	}
	log_message(l_config, LOG_LEVEL_WARNING, "XHook[process_message]: "
		    "Invalid message : %s\n", token1);

}

/*****************************************************************************/
int get_icon(Window wnd)
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
	char a, r, g, b;
	unsigned long nitems;
	char *buffer = g_malloc(1024, 1);
	char *buffer_pos = buffer;
	char *window_id = g_malloc(11, 1);

	if (get_property
	    (display, wnd, "_NET_WM_ICON", &nitems,
	     (unsigned char **)&data) != Success) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_icon]: "
			    "No icon for the window 0x%08lx", wnd);
		return 1;
	}
	if (nitems < 16 * 16 + 2) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_icon]: "
			    "No proper icon for the window 0x%08lx", wnd);
		return 1;
	}
	sprintf(window_id, "0x%08lx", wnd);
	//analyzing of _NET_WM_ICON atom content
	k = 0;
	while (k < nitems) {
		message_length = 0;
		message_id = 0;
		width = data[k];
		height = data[k + 1];
		k += 2;

		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_icon]: "
			    "new Icon : %i X %i\n", width, height);
		if (width > 32) {
			k += height * width;
			continue;
		}
		buffer_pos = buffer;
		count =
		    sprintf(buffer, "SETICON,%i,%s,%i,%s,%i,%i,", message_id,
			    window_id, message_id, "RGBA", width, height);

		buffer_pos += count;
		message_length += count;

		for (i = 0; i < width * height; i++) {
			a = ((data[k + i] >> 24));
			r = ((data[k + i] >> 16));
			g = ((data[k + i] >> 8));
			b = ((data[k + i] >> 0));

			pixel = (r << 24) + (g << 16) + (b << 8) + (a << 0);

			count = sprintf(buffer_pos, "%08x", pixel);
			buffer_pos += count;
			message_length += count;

			if (message_length > 1000 || i == width * height - 1) {
				count = sprintf(buffer_pos, "\n");
				send_message(buffer, message_length);

				message_id++;
				buffer_pos = buffer;
				message_length = 0;
				count =
				    sprintf(buffer,
					    "SETICON,%i,%s,%i,%s,%i,%i,",
					    message_id, window_id, message_id,
					    "RGBA", width, height);
				buffer_pos += count;
				message_length += count;
			}
		}
		k += height * width;
	}
	return 0;

}

/*****************************************************************************/
int is_window_resizable(Display * display, Window w)
{
	unsigned char *data;
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long bytes;
	Atom atom = 0;
	int i;
	int status;

	status = XGetWindowProperty(display,
				    w,
				    XInternAtom(display,
						"_NET_WM_ALLOWED_ACTIONS",
						True), 0, (~0L), False,
				    AnyPropertyType, &actual_type,
				    &actual_format, &nitems, &bytes, &data);

	if (status != 0) {
		return 0;
	}
	if (nitems == 0) {
		return 0;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[is_window_resizable]: "
		    "%i action allowed founded", (int)nitems);
	for (i = 0; i < nitems; i++) {
		atom = (Atom)
		    ((*((unsigned char *)data + 0) << 0) |
		     (*((unsigned char *)data + 1) << 8) |
		     (*((unsigned char *)data + 2) << 16) |
		     (*((unsigned char *)data + 3) << 24)
		    );
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[is_window_resizable]: "
			    "Atom state : %i", (int)atom);
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[is_window_resizable]: "
			    "Windows state : %s[%i]\n", XGetAtomName(display, atom),
			    (int)atom);
		if (atom ==
		    XInternAtom(display, "_NET_WM_ACTION_MAXIMIZE_HORZ",
				True)) {
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[is_window_resizable]: "
				    "window 0x%08lx is resizable", w);
			return 0;
		}
		if (atom ==
		    XInternAtom(display, "_NET_WM_ACTION_MAXIMIZE_VERT",
				True)) {
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[is_window_resizable]: "
				    "window 0x%08lx is resizable", w);
			return 0;
		}
		data += sizeof(Atom);
	}
	return 1;
}

/*****************************************************************************/
void create_window(Window win_out)
{
	char *window_id = g_malloc(11, 1);
	char *buffer = g_malloc(1024, 1);
	int x, y;
	unsigned int width, height, border, depth;
	Window root;
	XWindowAttributes attributes;
	Window_item *witem = NULL;
	Window parent_id;
	Window win_in;
	Window proper_win = 0;
	Atom type;
	int flags = 0;
	int pid;
	Atom *states;
	unsigned long nstates;
	int i;

	log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
		    "Creation of the window : 0x%08lx", win_out);
	Window_get(window_list, win_out, witem);
	if (witem != 0) {
		sprintf(window_id, "0x%08x", (int)win_out);
		sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id,
			window_id, 0, 0);
		send_message(buffer, strlen(buffer));
		log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
			    "0x%08lx already exist", win_out);
		g_free(window_id);
		g_free(buffer);
		return;
	}
	XGetGeometry(display, win_out, &root, &x, &y, &width, &height, &border,
		     &depth);
	log_message(l_config, LOG_LEVEL_INFO,
		    "XHook[create_window]: " "GEOM OUT: %i,%i,%i,%i,%i,%i", x,
		    y, width, height, border, depth);

	win_in = get_in_window(display, win_out);
	XGetGeometry(display, win_in, &root, &x, &y, &width, &height, &border,
		     &depth);
	log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
		    "GEOM IN: %i,%i,%i,%i,%i,%i\n", x,
		    y, width, height, border, depth);

	if (is_good_window(display, win_out) == 0) {
		proper_win = win_out;
		log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
			    "0x%08lx is a good window", proper_win);
	} else {
		if (is_good_window(display, win_in) == 0) {
			proper_win = win_in;
			log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
				    "0x%08lx is a good window", proper_win);
		} else {
			log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
				    "No good window");
			g_free(window_id);
			g_free(buffer);
			return;
		}
	}
	XGetWindowAttributes(display, win_out, &attributes);
	if (attributes.class == InputOnly) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
			    "Bad attributes : 0x%08lx", proper_win);
		g_free(window_id);
		g_free(buffer);
		return;
	}
	get_window_type(display, proper_win, &type);
	get_window_pid(display, proper_win, &pid);
	get_parent_window(display, proper_win, &parent_id);

	if (parent_id != 0) {
		Window_get(window_list, parent_id, witem);
		if (witem == 0) {
			log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
				    "Found a parent window (0x%08lx) for the window 0x%08lx, but the windows list does not contain it",
				    parent_id, proper_win);
			parent_id = 0;
		}
		witem = NULL;
	}

	if (win_in == 0) {
		win_in = win_out;
	}
	flags = SEAMLESSRDP_NORMAL;
	log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
		    "Windows type : %s", XGetAtomName(display, type));
	if (type ==
	    XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False)
	    || type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY",
				   False)
	    || type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False)
	    || type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU",
				   False)) {
		flags = SEAMLESS_CREATE_POPUP;
	}
	if (type == XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False))
		flags = SEAMLESS_CREATE_POPUP | SEAMLESS_CREATE_TOOLTIP;

	if (is_splash_window(display, proper_win)) {
		flags = SEAMLESS_CREATE_POPUP;
		parent_id = -1L;
	}

	if (flags & SEAMLESS_CREATE_POPUP) {
		if (parent_id == 0 || (flags & SEAMLESS_CREATE_TOOLTIP))
			parent_id = -1;
	}

	if (get_window_state(display, proper_win, &states, &nstates) == 0) {
		for (i = 0; i < nstates; i++) {
			if (states[i] == XInternAtom(display, "_NET_WM_STATE_MODAL", False)) {
				flags |= SEAMLESSRDP_CREATE_MODAL;
				log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
					    "0x%08lx is a modal windows", proper_win);
			}
		}
	}

	if (is_window_resizable(display, proper_win) == 1) {
		flags |= SEAMLESS_CREATE_FIXEDSIZE;
		log_message(l_config, LOG_LEVEL_INFO, "XHook[create_window]: "
			    "Windows 0x%08lx is not resizable", proper_win);
	}

	sprintf(window_id, "0x%08x", (int)proper_win);
	sprintf(buffer, "CREATE,%i,%s,%i,0x%08x,0x%08x\n", message_id,
		window_id, pid, (int)parent_id, flags);
	send_message(buffer, strlen(buffer));
	
	if (!(flags & SEAMLESS_CREATE_POPUP)) {
		get_icon(proper_win);
	}

	sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n", message_id,
		window_id, attributes.x, attributes.y, attributes.width,
		attributes.height, 0);
	send_message(buffer, strlen(buffer));
	sprintf(buffer, "STATE,%i,%s,0x%08x,0x%08x\n", message_id, window_id, 0,
		0);
	send_message(buffer, strlen(buffer));
	g_free(window_id);
	g_free(buffer);
	Window_add(window_list, proper_win, win_out);
	//Window_dump(window_list);

	Window_get(window_list, proper_win, witem);
	check_window_name(witem);
}

/*****************************************************************************/
int get_state(Window_item * witem)
{
	Atom *states;
	Atom atom_net_wm_state_hidden;
	Atom atom_net_wm_state_maximized_horz;
	Atom atom_net_wm_state_maximized_vert;
	unsigned long nstates;
	int i;
	int state = STATE_NORMAL;
	int state_seamless = SEAMLESSRDP_NORMAL;

	if (witem == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_state]: "
			    "No window item\n");
		return -1;
	}

	if (get_window_state(display, witem->win_out, &states, &nstates) != 0) {
		if (get_window_state(display, witem->window_id, &states, &nstates) != 0)
			return -1;
	}

	atom_net_wm_state_hidden = XInternAtom(display, "_NET_WM_STATE_HIDDEN", True);
	atom_net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);
	atom_net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True);

	for (i = 0; i < nstates; i++) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_state]: "
			    "State: %s", XGetAtomName(display, states[i]));
		
		if (states[i] == atom_net_wm_state_hidden)
			state |= STATE_ICONIFIED;
		else if (states[i] == atom_net_wm_state_maximized_horz)
			state |= STATE_MAXIMIZED_HORIZ;
		else if (states[i] == atom_net_wm_state_maximized_vert)
			state |= STATE_MAXIMIZED_VERT;
	}

	switch (state) {
		case STATE_ICONIFIED:
			state_seamless = SEAMLESSRDP_MINIMIZED;
			break;
		case STATE_MAXIMIZED_BOTH:
			state_seamless = SEAMLESSRDP_MAXIMIZED;
			break;
		case STATE_MAXIMIZED_HORIZ:
		case STATE_MAXIMIZED_VERT:
		case STATE_NORMAL:
			state_seamless = SEAMLESSRDP_NORMAL;
			break;
		default:
			log_message(l_config, LOG_LEVEL_WARNING, "XHook[get_state]: "
				    "Window 0x%08lx has an unknown state", witem->window_id);
			return -1;
	}

	return state_seamless;
}

/*****************************************************************************/
void destroy_window(Window w)
{
	char *window_id;
	char *buffer;
	Window_item *witem;
	Window_get(window_list, w, witem);
	if (witem->state == SEAMLESSRDP_MINIMIZED) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[destroy]: "
			    "Unable to destroy a minimize window ");
		return;
	}
	window_id = g_malloc(11, 1);
	buffer = g_malloc(1024, 1);
	log_message(l_config, LOG_LEVEL_INFO, "XHook[destroy]: "
		    "Destroy of window: 0x%08lx", w);
	sprintf(window_id, "0x%08x", (int)w);
	sprintf(buffer, "DESTROY,%i,%s,%08x\n", message_id, window_id, 0);

	send_message(buffer, strlen(buffer));
	Window_del(window_list, w);
	g_free(window_id);
	g_free(buffer);

}

/*****************************************************************************/
void move_window(Window w, int x, int y, int width, int height)
{
	char *window_id;
	char *buffer;

	Window_item *witem;

	Window_get(window_list, w, witem);
	if (witem == 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[move_window]: "
			    "Unknowed window 0x%08lx", w);
		return;
	}

	if (witem->state == SEAMLESSRDP_MAXIMIZED) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[move_window]: "
			    "Window 0x%08lx is maximized. Do not update client window size (x: %d y: %d width: %d height: %d)", witem->window_id, x, y, width, height);
		return;
	}

	window_id = g_malloc(11, 1);
	buffer = g_malloc(1024, 1);

	log_message(l_config, LOG_LEVEL_INFO, "XHook[move_window]: "
		    "Windows id : 0x%08lx State: %i", witem->window_id,
		    witem->state);
	if (witem->state == SEAMLESSRDP_MAXIMIZED) {
		sprintf(window_id, "0x%08x", (int)witem->window_id);
		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n",
			message_id, window_id, -4, -4, width, height, 0);
	} else {
		sprintf(window_id, "0x%08x", (int)witem->window_id);
		sprintf(buffer, "POSITION,%i,%s,%i,%i,%i,%i,0x%08x\n",
			message_id, window_id, x, y, width, height, 0);
	}
	send_message(buffer, strlen(buffer));
	g_free(window_id);
	g_free(buffer);
}

/*****************************************************************************/
void check_window_state(Window_item *witem)
{
	char *buffer = NULL;
	int state = -1;

	if (! witem)
		return;

	state = get_state(witem);
	if (state < 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[check_window_state]: "
			    "Failed to check window 0x%08lx state", witem->window_id);
		return;
	}

	if (state == witem->state)
		return;

	log_message(l_config, LOG_LEVEL_INFO, "XHook[check_window_state]: "
		    "Window 0x%08lx state has change : %i", witem->window_id, state);

	witem->state = state;

	if (witem->waiting_state) {
		send_ack(witem->waiting_state->ack_id);
		g_free(witem->waiting_state);
		witem->waiting_state = NULL;

		return;
	}

	buffer = g_malloc(1024, True);
	g_sprintf(buffer, "STATE,%i,0x%08lx,0x%08x,0x%08x\n", message_id, witem->window_id, state, 0);
	send_message(buffer, g_strlen(buffer));
	g_free(buffer);
}

/*****************************************************************************/
void check_window_name(Window_item *witem)
{
	char *buffer = NULL;
	unsigned char * name = NULL;

	if (! witem)
		return;

	if (! get_window_name(display, witem->window_id, &name)) {
		if (! get_window_name(display, witem->win_out, &name))
			return;
	}

	if (! witem->name || g_strlen((char *) name) != g_strlen((char *) witem->name) || g_strcmp((char *) name, (char *) witem->name) != 0) {
		log_message(l_config, LOG_LEVEL_INFO, "XHook[check_window_name]: "
			    "Window 0x%08lx name has changed : %s", witem->window_id, name);

		witem->name = g_strdup((char *) name);

		buffer = g_malloc(1024, True);
		g_sprintf(buffer, "TITLE,%i,0x%08lx,%s,0x%08x\n", message_id, witem->window_id, name, 0);
		send_message(buffer, g_strlen(buffer));
		g_free(buffer);
	}
}

/*****************************************************************************/
void check_windows() {
	Window_item *witem = NULL;
	int i = 0;
	int count = 0;

	log_message(l_config, LOG_LEVEL_INFO, "XHook[check_windows]: "
		    "Check windows changes");

	count = window_list.item_count;
	for (i = 0; i < count; i++) {
		witem = &window_list.list[i];

		check_window_name(witem);
		check_window_state(witem);
	}
}

/*****************************************************************************/
void *thread_Xvent_process(void *arg)
{
	Window w;
	Window root_windows;
	Window_item *witem;

	root_windows = DefaultRootWindow(display);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
		    "Windows root ID : 0x%08lx", root_windows);

	XSelectInput(display, root_windows, SubstructureNotifyMask);
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
		    "Begin the event loop ");

	while (1) {
		XEvent ev;
		XNextEvent(display, &ev);
		pthread_mutex_lock(&mutex);

		check_windows();

		switch (ev.type) {

		case ConfigureNotify:
			w = ev.xconfigure.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				    "Window move : 0x%08lx x:%i y:%i w:%i h:%i", w, ev.xconfigure.x, ev.xconfigure.y, ev.xconfigure.width, ev.xconfigure.height);
			move_window(w, ev.xconfigure.x, ev.xconfigure.y,
				    ev.xconfigure.width, ev.xconfigure.height);
			break;

		case ReparentNotify:
			w = ev.xreparent.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				    "Window reparented : 0x%08lx parent:0x%08lx x:%d y:%d", w, ev.xreparent.parent, ev.xreparent.x, ev.xreparent.y);
			if (is_splash_window(display, w))
				create_window(ev.xreparent.parent);
			break;

		case MapNotify:
			w = ev.xmap.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				    "Window 0x%08lx mapped", w);
			create_window(w);
			break;

		case DestroyNotify:
			w = ev.xdestroywindow.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				    "Destroy of the window: 0x%08lx", w);
			break;

		case UnmapNotify:
			w = ev.xunmap.window;
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
				    "Unmap of the window: 0x%08lx", w);
			//Window_dump(window_list);

			Window_get(window_list, w, witem);
			if (witem == 0) {
				log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
					    "Unknowed window\n");
				break;
			}

			if (witem->state == SEAMLESSRDP_MINIMIZED) {
				log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_Xvent_process]: "
					    "Window 0x%08lx is iconified\n",
					    witem->window_id);
				break;
			}

			destroy_window(witem->window_id);
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
	pthread_exit(0);
}

/*****************************************************************************/
void *thread_vchannel_process(void *arg)
{
	char *buffer = g_malloc(1024, 1);
	struct stream *s = NULL;
	int rv;
	int length;
	int total_length;

	signal(SIGCHLD, handler);
	sprintf(buffer, "HELLO,%i,0x%08x\n", 0, 0);
	send_message(buffer, strlen(buffer));
	while (1) {
		make_stream(s);
		init_stream(s, 1600);

		rv = vchannel_receive(seamrdp_channel, s->data, &length,
				      &total_length);
		if (rv == ERROR) {
			log_message(l_config, LOG_LEVEL_ERROR, "XHook[thread_Xvent_process]: "
				    "Closing the connexion to the channel server");
			vchannel_close(seamrdp_channel);
			pthread_exit((void *)1);
		}
		switch (rv) {
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "XHook[thread_vchannel_process]: "
				    "Invalid message");
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_INFO, "XHook[thread_vchannel_process]: "
				    "Status connected");
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_INFO, "XHook[thread_vchannel_process]: "
				    "Status disconnected");
			break;
		default:
			s->data[length] = 0;
			process_message(s->data);
			break;
		}
		free_stream(s);
	}
	pthread_exit(0);
}

int XHook_init()
{
	char filename[256];
	char log_filename[256];
	struct list *names;
	struct list *values;
	char *name;
	char *value;
	int index;
	int display_num;
	int res;

	display_num =
	    g_get_display_num_from_display(g_strdup(g_getenv("DISPLAY")));
	if (display_num == 0) {
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
	if (file_by_name_read_section(filename, XHOOK_CFG_GLOBAL, names, values)
	    == 0) {
		for (index = 0; index < names->count; index++) {
			name = (char *)list_get_item(names, index);
			value = (char *)list_get_item(values, index);
			if (0 == g_strcasecmp(name, XHOOK_CFG_NAME)) {
				if (g_strlen(value) > 1) {
					l_config->program_name =
					    (char *)g_strdup(value);
				}
			}
		}
	}
	if (file_by_name_read_section
	    (filename, XHOOK_CFG_LOGGING, names, values) == 0) {
		for (index = 0; index < names->count; index++) {
			name = (char *)list_get_item(names, index);
			value = (char *)list_get_item(values, index);
			if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_DIR)) {
				l_config->log_file = (char *)g_strdup(value);
			}
			if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_LEVEL)) {
				l_config->log_level = log_text2level(value);
			}
			if (0 ==
			    g_strcasecmp(name, XHOOK_CFG_LOG_ENABLE_SYSLOG)) {
				l_config->enable_syslog = log_text2bool(value);
			}
			if (0 == g_strcasecmp(name, XHOOK_CFG_LOG_SYSLOG_LEVEL)) {
				l_config->syslog_level = log_text2level(value);
			}
		}
	}
	if (g_strlen(l_config->log_file) > 1
	    && g_strlen(l_config->program_name) > 1) {
		g_sprintf(log_filename, "%s/%i/%s.log", l_config->log_file,
			  display_num, l_config->program_name);
		l_config->log_file = (char *)g_strdup(log_filename);
	}
	list_delete(names);
	list_delete(values);
	res = log_start(l_config);

	if (res != LOG_STARTUP_OK) {
		g_printf
		    ("vchannel[vchannel_init]: Unable to start log system[%i]\n",
		     res);
		return res;
	} else {
		return LOG_STARTUP_OK;
	}
}

/*****************************************************************************/
int main(int argc, char **argv, char **environ)
{
	pthread_t Xevent_thread, Vchannel_thread;
	void *ret;
	l_config = g_malloc(sizeof(struct log_config), 1);
	if (XHook_init() != LOG_STARTUP_OK) {
		g_printf("XHook[main]: Unable to init log system\n");
		g_free(l_config);
		return 1;
	}
	if (vchannel_init() == ERROR) {
		g_printf("XHook[main]: Unable to init channel system\n");
		g_free(l_config);
		return 1;
	}

	Window_list_init(window_list);
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&send_mutex, NULL);
	message_id = 0;
	seamrdp_channel = vchannel_open("seamrdp");
	if (seamrdp_channel == ERROR) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
			    "Error while connecting to vchannel provider");
		g_free(l_config);
		return 1;
	}

	XInitThreads();
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[main]: "
		    "Opening the default display : %s", getenv("DISPLAY"));

	if ((display = XOpenDisplay(0)) == 0) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
			    "Unable to open the default display : %s ",
			    getenv("DISPLAY"));
		g_free(l_config);
		return 1;
	}
	XSetErrorHandler(error_handler);

	initializeXUtils(display);

	if (pthread_create
	    (&Xevent_thread, NULL, thread_Xvent_process, (void *)0) < 0) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
			    "Pthread_create error for thread : Xevent_thread");
		g_free(l_config);
		return 1;
	}
	if (pthread_create
	    (&Vchannel_thread, NULL, thread_vchannel_process, (void *)0) < 0) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[main]: "
			    "Pthread_create error for thread : Vchannel_thread");
		g_free(l_config);
		return 1;
	}

	(void)pthread_join(Xevent_thread, &ret);
	(void)pthread_join(Vchannel_thread, &ret);
	pthread_mutex_destroy(&mutex);
	g_free(l_config);
	return 0;
}
