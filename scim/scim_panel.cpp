/**
 * Copyright (C) 2013 Ulteo SAS
 * http://www.ulteo.com
 * Author Alexandre CONFIANT-LATOUR <a.confiant@ulteo.com> 2013
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

#define Uses_C_STDIO
#define Uses_C_STDLIB
#define Uses_SCIM_LOOKUP_TABLE
#define Uses_SCIM_SOCKET
#define Uses_SCIM_TRANSACTION
#define Uses_SCIM_TRANS_COMMANDS
#define Uses_SCIM_CONFIG
#define Uses_SCIM_CONFIG_MODULE
#define Uses_SCIM_DEBUG
#define Uses_SCIM_HELPER
#define Uses_SCIM_HELPER_MODULE
#define Uses_SCIM_PANEL_AGENT
#include <scim.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

#define XRDP_SOCKET_TIMEOUT 100

/* Declarations picked from os_calls.h.
   Needed because direct include of this file
   makes symbol collision problems with glib */
extern "C" {
	void g_printf(const char* format, ...);
	int g_tcp_send(int sck, const void* ptr, int len, int flags);
	int g_tcp_recv(int sck, void* ptr, int len, int flags);
	int g_tcp_can_send(int sck, int millis);
	int g_tcp_can_recv(int sck, int millis);
	int g_tcp_last_error_would_block(int sck);
	int g_create_unix_socket(const char *socket_filename);
	int g_tcp_accept(int sck);
	int g_file_close(int fd);
}

using namespace scim;

static PanelAgent *_panel_agent = 0;
static GThread *_panel_agent_thread = 0;
static ConfigModule *_config_module = 0;
static ConfigPointer _config;
static int listen_sock, data_sock;
static GThread *_xrdp_scim_server_thread = 0;
static char imeState, imeStateLast;

static void slot_update_factory_info(const PanelFactoryInfo &info) { } 
static void slot_show_help(const String &help) { }
static void slot_show_factory_menu(const std::vector <PanelFactoryInfo> &factories) { }
static void slot_update_spot_location(int x, int y) { }
static void slot_update_preedit_string(const String &str, const AttributeList &attrs) { }
static void slot_update_aux_string(const String &str, const AttributeList &attrs) { }
static void slot_update_lookup_table(const LookupTable &table) { }
static void slot_register_properties(const PropertyList &props) { }
static void slot_update_property(const Property &prop) { }
static void slot_register_helper_properties(int id, const PropertyList &props) { }
static void slot_update_helper_property(int id, const Property &prop) { }
static void slot_register_helper(int id, const HelperInfo &helper) { }
static void noop_v(void) { }
static void noop_i(int) { }

static int data_recv(int socket, char* data, int len) {
	int rcvd;

	while (len > 0) { 
		rcvd = g_tcp_recv(socket, data, len, 0);

		if (rcvd == -1) { 
			if (g_tcp_last_error_would_block(socket)) {
				/* not an error, must retry */
				g_tcp_can_recv(socket, 10);
			} else {
				/* error, stop */
				return -1;
			}
		} else if (rcvd == 0) {
			/* disconnection, stop */
			return 0;
		} else {
			/* got data */
			data += rcvd;
			len -= rcvd;
		}
	}
	return 1;
}

static int data_send(int socket, char* data, int len) {
	int sent;

	while (len > 0) { 
		sent = g_tcp_send(socket, data, len, 0);

		if (sent == -1) { 
			if (g_tcp_last_error_would_block(socket)) {
				/* not an error, must retry */
				g_tcp_can_send(socket, 10);
			} else {
				/* error, stop */
				return -1;
			}
		} else if (sent == 0) {
			/* disconnection, stop */
			return 0;
		} else {
			/* got data */
			data += sent;
			len -= sent;
		}
	}
	return 1;
}

static gpointer panel_agent_thread_func(gpointer data) {
	if (!_panel_agent->run()) {
		fprintf(stderr, "Failed to run Panel.\n");
	}

	gdk_threads_enter();
	gtk_main_quit();
	gdk_threads_leave();
	g_thread_exit(NULL);
	return ((gpointer) NULL);
}

static bool run_panel_agent(void) {
	_panel_agent_thread = NULL;
	if (_panel_agent && _panel_agent->valid()) {
		_panel_agent_thread = g_thread_create(panel_agent_thread_func, NULL, TRUE, NULL);
	}
	return (_panel_agent_thread != NULL);
}

static void server_process_loop(int data_sock) {
	/* Initial values */
	imeStateLast = imeState = 0;

	/* Server mainloop */
	while (true) {
		int status;
		unsigned int unicode;

		if (g_tcp_can_recv(data_sock, XRDP_SOCKET_TIMEOUT)) {
			status = data_recv(data_sock, (char*)(&unicode), sizeof(uint32));

			if (status == -1) {
				/* got an error */
				fprintf(stderr, "Connection error\n");
				return;
			}

			if (status == 0) {
				/* disconnection */
				fprintf(stderr, "Disconnected\n");
				return;
			}

			/* Dirty hack Used to send an unicode symbol without a keypress */
			_panel_agent->select_candidate(unicode);
		}

		/* read ImeState spool */
		if (imeState != imeStateLast) {

			/* send it */
			status = data_send(data_sock, &imeState, 1);
			imeStateLast = imeState;

			if (status == -1) {
				/* got an error */
				fprintf(stderr, "Connection error\n");
				return;
			}

			if (status == 0) {
				/* disconnection */
				fprintf(stderr, "Disconnected\n");
				return;
			}
		}
	}
}

static gpointer xrdp_scim_server_thread_func(gpointer data) {
	int num;
	char name[250];
	char *p = getenv("DISPLAY");

	/* socket name */
	sscanf(p, ":%d", &num);
	g_snprintf(name, 250, "/var/spool/xrdp/xrdp_scim_socket_72%d", num);

	g_printf("Socket name : %s\n", name);

	/* Listen loop */
	while(true) {

		/* destroy file if it already exist */
		unlink(name);

		if ((listen_sock = g_create_unix_socket(name)) == 1) {
			/* error creating socket */
			fprintf(stderr, "Failed to create socket : %s\n", name);
			return ((gpointer) NULL);
		}

		g_printf("Waiting for incomming connection\n");

		if ((data_sock = g_tcp_accept(listen_sock)) == -1) {
			fprintf(stderr, "Failed to accept\n");
			return ((gpointer) NULL);
		}

		/* Process loop */
		server_process_loop(data_sock);
	}

	gdk_threads_enter();
	gtk_main_quit();
	gdk_threads_leave();
	g_thread_exit(NULL);
	return ((gpointer) NULL);
}

static bool run_xrdp_scim_server(void) {
	_xrdp_scim_server_thread = NULL;
	_xrdp_scim_server_thread = g_thread_create(xrdp_scim_server_thread_func, NULL, TRUE, NULL);
	return (_xrdp_scim_server_thread != NULL);
}

static void slot_turn_on(void) {
	imeState = 1;
	fprintf(stderr, "IME enabled\n");
}

static void slot_turn_off(void) {
	imeState = 0;
	fprintf(stderr, "IME disabled\n");
}

static void slot_transaction_start(void) {
	gdk_threads_enter();
}

static void slot_transaction_end(void) {
	gdk_threads_leave();
}

static bool initialize_panel_agent(const String &config, const String &display, bool resident) {
	_panel_agent = new PanelAgent();
	if (!_panel_agent->initialize(config, display, resident)) {
		return false;
	}

	_panel_agent->signal_connect_transaction_start(slot(slot_transaction_start));
	_panel_agent->signal_connect_transaction_end(slot(slot_transaction_end));
	_panel_agent->signal_connect_reload_config(slot(noop_v));
	_panel_agent->signal_connect_turn_on(slot(slot_turn_on));
	_panel_agent->signal_connect_turn_off(slot(slot_turn_off));
	_panel_agent->signal_connect_update_screen(slot(noop_i));
	_panel_agent->signal_connect_update_spot_location(slot(slot_update_spot_location));
	_panel_agent->signal_connect_update_factory_info(slot(slot_update_factory_info));
	_panel_agent->signal_connect_show_help(slot(slot_show_help));
	_panel_agent->signal_connect_show_factory_menu(slot(slot_show_factory_menu));
	_panel_agent->signal_connect_show_preedit_string(slot(noop_v));
	_panel_agent->signal_connect_show_aux_string(slot(noop_v));
	_panel_agent->signal_connect_show_lookup_table(slot(noop_v));
	_panel_agent->signal_connect_hide_preedit_string(slot(noop_v));
	_panel_agent->signal_connect_hide_aux_string(slot(noop_v));
	_panel_agent->signal_connect_hide_lookup_table(slot(noop_v));
	_panel_agent->signal_connect_update_preedit_string(slot(slot_update_preedit_string));
	_panel_agent->signal_connect_update_preedit_caret(slot(noop_i));
	_panel_agent->signal_connect_update_aux_string(slot(slot_update_aux_string));
	_panel_agent->signal_connect_update_lookup_table(slot(slot_update_lookup_table));
	_panel_agent->signal_connect_register_properties(slot(slot_register_properties));
	_panel_agent->signal_connect_update_property(slot(slot_update_property));
	_panel_agent->signal_connect_register_helper_properties(slot(slot_register_helper_properties));
	_panel_agent->signal_connect_update_helper_property(slot(slot_update_helper_property));
	_panel_agent->signal_connect_register_helper(slot(slot_register_helper));
	_panel_agent->signal_connect_remove_helper(slot(noop_i));
	_panel_agent->signal_connect_lock(slot(noop_v));
	_panel_agent->signal_connect_unlock(slot(noop_v));

	return true;
}

int main(int argc, char *argv []) {
	String config_name("simple");
	String display_name("");
	char *p;

	//load config module
	_config_module = new ConfigModule(config_name);

	if (!_config_module || !_config_module->valid()) {
		fprintf(stderr, "Can not load \"simple\" Config module.\n");
		return 1;
	}

	//create config instance
	_config = _config_module->create_config();

	if (_config.null()) {
		fprintf(stderr, "Failed to create Config instance from \"simple\" Config module.\n");
		return 1;
	}

	// init threads and gtk
	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(&argc, &argv);

	p = getenv("DISPLAY");
	display_name = String(p);

	if (!initialize_panel_agent(config_name, display_name, true)) {
		fprintf(stderr, "Failed to initialize Panel Agent!\n");
		return 1;
	}

	scim_daemon();

	if (!run_panel_agent()) {
		fprintf(stderr, "Failed to run Socket Server!\n");
		return 1;
	}

	if (!run_xrdp_scim_server()) {
		fprintf(stderr, "Failed to run Socket Server!\n");
		return 1;
	}

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	// Exiting...
	g_thread_join(_panel_agent_thread);
	_config.reset();
	g_file_close(data_sock);
	g_file_close(listen_sock);

	fprintf(stderr, "Successfully exited.\n");
	return 0;
}
