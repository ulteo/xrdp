/**
 * Copyright (C) 2009-2011 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com>
 * Author Thomas MOUTON <thomas@ulteo.com> 2010-2011
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
#include <log.h>
#include <stdlib.h>

extern struct log_config *l_config;

static char *gravity_strs[] = { "ForgetGravity",
	"NorthWestGravity",
	"NorthGravity",
	"NorthEastGravity",
	"WestGravity",
	"CenterGravity",
	"EastGravity",
	"SouthWestGravity",
	"SouthGravity",
	"SouthEastGravity",
	"StaticGravity"
};

static char *wmstate_strs[] = {
	"WithdrawnState",
	"NormalState",
	"UnknownState",
	"IconicState"
};

static Display *g_display;

static Atom g_atom_net_wm_state = None;
static Atom g_atom_net_wm_state_maximized_horz = None;
static Atom g_atom_net_wm_state_maximized_vert = None;
static Atom g_atom_net_wm_state_hidden = None;
static Atom g_atom_net_wm_state_modal = None;

static Atom g_atom_net_wm_window_type = None;
static Atom g_atom_net_wm_window_type_normal = None;
static Atom g_atom_net_wm_window_type_splash = None;

static Atom g_atom_wm_state = None;
static Atom g_atom_wm_change_state = None;

void initializeXUtils(Display *dpy) {
	g_display = dpy;

	g_atom_net_wm_state = XInternAtom(g_display, "_NET_WM_STATE", False);
	g_atom_net_wm_state_maximized_horz = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	g_atom_net_wm_state_maximized_vert = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	g_atom_net_wm_state_hidden = XInternAtom(g_display, "_NET_WM_STATE_HIDDEN", False);
	g_atom_net_wm_state_modal = XInternAtom(g_display, "_NET_WM_STATE_MODAL", False);

	g_atom_net_wm_window_type = XInternAtom(g_display, "_NET_WM_WINDOW_TYPE", False);
	g_atom_net_wm_window_type_normal = XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	g_atom_net_wm_window_type_splash = XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_SPLASH", False);

	g_atom_wm_state = XInternAtom(g_display, "WM_STATE", False);
	g_atom_wm_change_state = XInternAtom(g_display, "WM_CHANGE_STATE", False);
}

/*****************************************************************************/
const char *gravityToStr(int gravity)
{
	if (gravity >= (sizeof(gravity_strs) / sizeof(gravity_strs[0])))
		return "BadGravity";

	return gravity_strs[gravity];
}

/*****************************************************************************/
const char *wmstateToStr(int wmstate)
{
	if (wmstate >= (sizeof(wmstate_strs) / sizeof(wmstate_strs[0])))
		return wmstate_strs[2];

	return wmstate_strs[wmstate];
}

/*****************************************************************************/
int hex2int(const char *hexa_string)
{
	int ret = 0, t = 0, n = 0;
	const char *c = hexa_string + 2;

	while (*c && (n < 16)) {
		if ((*c >= '0') && (*c <= '9'))
			t = (*c - '0');
		else if ((*c >= 'A') && (*c <= 'F'))
			t = (*c - 'A' + 10);
		else if ((*c >= 'a') && (*c <= 'f'))
			t = (*c - 'a' + 10);
		else
			break;
		n++;
		ret *= 16;
		ret += t;
		c++;
		if (n >= 8)
			break;
	}
	return ret;
}

/*****************************************************************************/
int
get_property(Display * display, Window w, const char *property,
	     unsigned long *nitems, unsigned char **data)
{
	Atom actual_type;
	int actual_format;
	unsigned long bytes;
	int status;

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_property]: "
		    "Get property for window 0x%08lx : %s", w, property);
	status = XGetWindowProperty(display,
				    w,
				    XInternAtom(display, property, True),
				    0,
				    (~0L),
				    False,
				    AnyPropertyType,
				    &actual_type,
				    &actual_format, nitems, &bytes, data);

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_property]: "
		    "Get property for window 0x%08lx : %s return status: %i nitems: %lu ", w, property, status, *nitems);

	if (status != Success) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_property]: "
			    "Unable to get atom %s for window 0x%08lx", property, w);
		return 1;
	}
	return 0;
}

/*****************************************************************************/
Window get_in_window(Display * display, Window w)
{
	Window root;
	Window parent;
	Window *children;
	unsigned int nchildren;

	if (!XQueryTree(display, w, &root, &parent, &children, &nchildren)
	    || nchildren == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG,
			    "XHook[get_in_window]: " "Window 0x%08lx has no child window", w);
		return 0;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_in_window]: "
		    "In window : 0x%08lx Out window: 0x%08lx", children[nchildren - 1], w);
	return children[nchildren - 1];
}

static int get_atoms_from_atom(Display *display, Window wnd, Atom atom, Atom **return_atoms, unsigned long *nitems) {
	char *atom_name;
	unsigned char *data;
	int status;
	int i;

	if (atom == None)
		return -1;

	if (! (atom_name = XGetAtomName(display, atom))) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_atoms_from_atom]: "
			    "Atom does not exist on display %s", XDisplayString(display));
		return -2;
	}
	
	status = get_property(display, wnd, atom_name, nitems, &data);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_atoms_from_atom]: "
			    "Window(0x%08lx): Failed to get atom %s", wnd, atom_name);
		return -3;
	}
	
	if (*nitems == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_atoms_from_atom]: "
			    "Window 0x%08lx: atom %s is empty", wnd, atom_name);
		return 0;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_wm_protocols]: "
			    "Window 0x%08lx: atom %s contains %ul atom(s)", wnd, atom_name, *nitems);

	*return_atoms = malloc(sizeof(Atom) * *nitems);
	if (! (*return_atoms)) {
		log_message(l_config, LOG_LEVEL_WARNING, "XHook[get_wm_protocols]: "
			    "Failed to allocate memory(%i bytes)", (sizeof(Atom) * *nitems));
		return -4;
	}

	for (i = 0; i < *nitems; i++) {
		(*return_atoms)[i] = (Atom)
		    ((*((unsigned char *)data + 0) << 0) |
		     (*((unsigned char *)data + 1) << 8) |
		     (*((unsigned char *)data + 2) << 16) |
		     (*((unsigned char *)data + 3) << 24)
		    );
		data += 4;

		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_wm_protocols]: "
			    "Window 0x%08lx: atom %s contains atom: %ul", wnd, atom_name, (*return_atoms)[i]);
	}

	return 0;
}

int is_good_window(Display * display, Window w)
{
	unsigned char *data;
	unsigned long nitems;
	int status;

	if (is_splash_window(display, w))
		return 0;

	status = get_property(display, w, "WM_HINTS", &nitems, &data);
	if ((status != 0) || nitems == 0 || (data == 0)) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[is_good_window]: "
			    "Window 0x%08lx did not contain the right information", w);
		return 1;
	}
	return 0;
}

/*****************************************************************************/
int get_window_name(Display * display, Window w, unsigned char **name)
{
	unsigned long nitems;
	int status;
	unsigned char * p = NULL;

	status = get_property(display, w, "_NET_WM_NAME", &nitems, name);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_name]: "
			    "Window 0x%08lx: Unable to get atom _NET_WM_NAME", w);

		status = get_property(display, w, "WM_NAME", &nitems, name);
		if (status != 0) {
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_name]: "
				    "Window 0x%08lx: Unable to get atom WM_NAME", w);
			return False;
		}
		if (nitems == 0 || name == 0) {
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_name]: "
				    "Window 0x%08lx has no name in atom WM_NAME", w);
			return False;
		}
	}
	else if (nitems == 0 || name == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_name]: "
			    "Window 0x%08lx has no name in atom _NET_WM_NAME", w);
		return False;
	}
	
	p = *name;

	while (*p != '\0') {
		if ((*p < 0x20 && *p > 0) || *p == ',')
			*p = '_';
		p++;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_name]: "
		    "Window(0x%08lx) name : %s\n", w, *name);
	return True;
}

static int get_net_wm_state(Display * display, Window w, Atom ** atoms, unsigned long *nitems)
{
	int status;

	status = get_atoms_from_atom(display, w, g_atom_net_wm_state, atoms, nitems);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_net_wm_state]: "
			    "Unable to get window(0x%08lx) states", w);
		return 1;
	}

	return 0;
}

static int get_wm_state(Display* display, Window wnd) {
	Atom *wm_states;
	unsigned long nitems;
	int status;

	status = get_atoms_from_atom(display, wnd, g_atom_wm_state, &wm_states, &nitems);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_wm_state]: "
			    "Unable to get window(0x%08lx) WM states", wnd);
		return -1;
	}
	if (nitems == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_wm_state]: "
			    "Window 0x%08lx has no WM state", wnd);
		return -2;
	}

	return wm_states[0];
}

int get_window_state(Display * display, Window wnd) {
	Atom *states;
	unsigned long nstates;
	int i;
	int state = STATE_NORMAL;
	int wmstate;

	wmstate = get_wm_state(display, wnd);
	switch (wmstate) {
		case IconicState:
		case NormalState:
			break;
		default:
			return -1;
	}

	if (get_net_wm_state(display, wnd, &states, &nstates) != 0)
		return -2;

	if (nstates == 0) {
		if (wmstate == NormalState)
			return state;
		else
			return -3;
	}

	for (i = 0; i < nstates; i++) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_state]: "
			    "%iWindow 0x%08lx has state: %s", i, wnd, XGetAtomName(display, states[i]));

		if (states[i] == g_atom_net_wm_state_hidden && wmstate == IconicState)
			state |= STATE_ICONIFIED;
		else if (states[i] == g_atom_net_wm_state_maximized_horz && wmstate == NormalState)
			state |= STATE_MAXIMIZED_HORIZ;
		else if (states[i] == g_atom_net_wm_state_maximized_vert && wmstate == NormalState)
			state |= STATE_MAXIMIZED_VERT;
	}

	return state;
}

static void change_wm_state(Display* display, Window wnd, int wmstate) {
	int old_wmstate;
	XEvent xev;

	switch (wmstate) {
		case IconicState:
		case NormalState:
			break;
		default:
			log_message(l_config, LOG_LEVEL_ERROR, "XHook[change_wm_state]: "
				    "Unknown state %i", wmstate);
			return;
	}

	old_wmstate = get_wm_state(display, wnd);

	// see http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.4
	
	if (old_wmstate == IconicState && wmstate == NormalState) {
		XMapWindow(display, wnd);
		XFlush(display);

		return;
	}
	else if (old_wmstate == NormalState && wmstate == IconicState) {
		if (g_atom_wm_change_state == None) {
			log_message(l_config, LOG_LEVEL_ERROR, "XHook[change_wm_state]: "
				"Window 0x%08lx: Unable to change WM state from %s(%i) to %s(%i): the atom WM_CHANGE_STATE does not exist", wnd, wmstateToStr(old_wmstate), old_wmstate, wmstateToStr(wmstate), wmstate);
			return;
		}

		xev.type = ClientMessage;
		xev.xclient.window = wnd;
		xev.xclient.message_type = g_atom_wm_change_state;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = wmstate;
		XSendEvent(display, DefaultRootWindow(display), False, SubstructureNotifyMask|SubstructureRedirectMask, &xev);
	}

	XFlush(display);
}

static void remove_net_wm_state(Display* display, Window wnd, Atom state) {
	XEvent xev;

	if (state == None) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[remove_net_wm_state]: "
			    "No state to remove");
		return;
	}

	if (g_atom_net_wm_state == None) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[remove_net_wm_state]: "
			"Unable to remove state %s of window 0x%08lx: the atom _NET_WM_STATE does not exist", XGetAtomName(display, state), wnd);
		return;
	}

	xev.type = ClientMessage;
	xev.xclient.window = wnd;
	xev.xclient.message_type = g_atom_net_wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = state;
	xev.xclient.data.l[2] = 0;
	xev.xclient.data.l[3] = 2;
	xev.xclient.data.l[4] = 0;
	XSendEvent(display, DefaultRootWindow(display), False, SubstructureNotifyMask|SubstructureRedirectMask, &xev);
	XFlush(display);
}

static void add_net_wm_state(Display* display, Window wnd, Atom state1, Atom state2) {
	XEvent xev;

	if (state1 == None) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[add_net_wm_state]: "
			    "No state to add");
		return;
	}

	if (g_atom_net_wm_state == None) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[add_net_wm_state]: "
			"Unable to add state(s) to window 0x%08lx: the atom _NET_WM_STATE does not exist", wnd);
		return;
	}

	xev.type = ClientMessage;
	xev.xclient.window = wnd;
	xev.xclient.message_type = g_atom_net_wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
	xev.xclient.data.l[1] = state1;
	xev.xclient.data.l[2] = state2;
	xev.xclient.data.l[3] = 2;
	xev.xclient.data.l[4] = 0;
	XSendEvent(display, DefaultRootWindow(display), False, SubstructureNotifyMask|SubstructureRedirectMask, &xev);
	XFlush(display);
}

/*****************************************************************************/
int
set_window_state(Display* display, Window wnd, int state) {
	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[set_window_state]: "
		    "State request: window 0x%08lx state %d", wnd, state);

	if (g_atom_net_wm_state == None) {
		log_message(l_config, LOG_LEVEL_ERROR, "XHook[set_window_state]: "
			    "Unable to change state of window 0x%08lx to %d: the atom _NET_WM_STATE does not exist", wnd, state);
		return -1;
	}

	switch (state) {
		case STATE_MAXIMIZED_BOTH:
			remove_net_wm_state(display, wnd, g_atom_net_wm_state_hidden);

			change_wm_state(display, wnd, NormalState);

			add_net_wm_state(display, wnd, g_atom_net_wm_state_maximized_horz, g_atom_net_wm_state_maximized_vert);
			break;
		case STATE_NORMAL:
			remove_net_wm_state(display, wnd, g_atom_net_wm_state_hidden);
			remove_net_wm_state(display, wnd, g_atom_net_wm_state_maximized_horz);
			remove_net_wm_state(display, wnd, g_atom_net_wm_state_maximized_vert);

			change_wm_state(display, wnd, NormalState);
			break;
		case STATE_ICONIFIED:
			change_wm_state(display, wnd, IconicState);
			
			add_net_wm_state(display, wnd, g_atom_net_wm_state_hidden, None);
			break;
		default:
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[set_window_state]: "
				    "Unable to change state of window 0x%08lx to %d: The state %d is not supported", wnd, state, state);
			return -1;
	}

	return state;
}

int is_splash_window(Display * display, Window w) {
	Atom type;
	get_window_type(display, w, &type);
	if (type == g_atom_net_wm_window_type_splash)
		return 1;

	return 0;
}

int is_modal_window(Display * display, Window w) {
	Atom *states;
	unsigned long nstates;
	int i;
	
	if (get_net_wm_state(display, w, &states, &nstates) == 0) {
		for (i = 0; i < nstates; i++) {
			if (states[i] == XInternAtom(display, "_NET_WM_STATE_MODAL", False))
				return 1;
		}
	}

	return 0;
}

int get_window_type(Display * display, Window w, Atom * atom)
{
	Atom *types;
	unsigned long nitems;
	int status;

	status = get_atoms_from_atom(display, w, g_atom_net_wm_window_type, &types, &nitems);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_type]: "
			    "Unable to get window(0x%08lx) type", w);
		return 1;
	}
	if (nitems == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_type]: "
			    "Window 0x%08lx has no type", w);
		*atom = None;
	}
	else {
		*atom = types[0];
	}

	return 0;
}

/*****************************************************************************/
int get_window_pid(Display * display, Window w, int *pid)
{
	Atom *types;
	unsigned long nitems;
	int status;

	status = get_atoms_from_atom(display, w, g_atom_net_wm_window_type, &types, &nitems);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_pid]: "
			    "Unable to get window(0x%08lx) pid", w);
		return False;
	}
	if (nitems == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_window_pid]: "
			    "Window 0x%08lx has no pid", w);
		return False;
	}

	*pid = (int) types[0];

	return True;
}

/*****************************************************************************/
int get_parent_window(Display * display, Window w, Window * parent)
{
	Atom *types;
	unsigned long nitems;
	int status;

	status = get_atoms_from_atom(display, w, g_atom_net_wm_window_type, &types, &nitems);
	if (status != 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_parent_window]: "
			    "Unable to get parent of window 0x%08lx", w);
		return False;
	}
	if (nitems == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "XHook[get_parent_window]: "
			    "No parent window for window 0x%08lx", w);
		return False;
	}

	*parent = (Window) types[0];

	return True;
}
