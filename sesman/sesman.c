/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file sesman.c
 * @brief Main program file
 * @author Jay Sorg
 *
 */

#include "sesman.h"
#include "thread_calls.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>


int g_sck;
int g_pid;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct config_sesman* g_cfg; /* defined in config.h */

#define XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
tbus g_term_event = 0;
tbus g_sync_event = 0;

extern int g_thread_sck; /* in thread.c */

/******************************************************************************/
/**
 *
 * @brief Starts sesman main loop
 *
 */
static void DEFAULT_CC
sesman_main_loop(void)
{
  int in_sck;
  int error;
  int robjs_count;
  int cont;
  tbus sck_obj;
  tbus robjs[8];

  /*main program loop*/
  log_message(&(g_cfg->log), LOG_LEVEL_INFO, "listening...");
  g_sck = g_tcp_socket();
  g_tcp_set_non_blocking(g_sck);
  error = scp_tcp_bind(g_sck, g_cfg->listen_address, g_cfg->listen_port);
  if (error == 0)
  {
    error = g_tcp_listen(g_sck);
    if (error == 0)
    {
      sck_obj = g_create_wait_obj_from_socket(g_sck, 0);
      cont = 1;
      while (cont)
      {
        /* build the wait obj list */
        robjs_count = 0;
        robjs[robjs_count++] = sck_obj;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sync_event;
        /* wait */
        if (g_obj_wait(robjs, robjs_count, 0, 0, -1) != 0)
        {
          /* error, should not get here */
          g_sleep(100);
        }
        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
          break;
        }
        if (g_is_wait_obj_set(g_sync_event)) /* sync */
        {
          g_reset_wait_obj(g_sync_event);
          session_sync_start();
        }
        if (g_is_wait_obj_set(sck_obj)) /* incomming connection */
        {
          in_sck = g_tcp_accept(g_sck);
          if ((in_sck == -1) && g_tcp_last_error_would_block(g_sck))
          {
            /* should not get here */
            g_sleep(100);
          }
          else if (in_sck == -1)
          {
            /* error, should not get here */
            break;
          }
          else
          {
            /* we've got a connection, so we pass it to scp code */
            LOG_DBG(&(g_cfg->log), "new connection");
            thread_scp_start(in_sck);
            /* todo, do we have to wait here ? */
          }
        }
      }
      g_delete_wait_obj_from_socket(sck_obj);
    }
    else
    {
      log_message(&(g_cfg->log), LOG_LEVEL_ERROR, "listen error %d (%s)",
                  g_get_errno(), g_get_strerror());
    }
  }
  else
  {
    log_message(&(g_cfg->log), LOG_LEVEL_ERROR, "bind error on "
                "port '%s': %d (%s)", g_cfg->listen_port,
                g_get_errno(), g_get_strerror());
  }
  g_tcp_close(g_sck);
}



/************************************************************************/
int DEFAULT_CC
xml_get_xpath(xmlDocPtr doc, char* xpath, char* value)
{
  xmlXPathObjectPtr xpathObj;
	xmlXPathContextPtr context;
	xmlNodeSetPtr nodeset;
	xmlChar *keyword;

	context = xmlXPathNewContext(doc);
	if (context == NULL)
	{
		printf("Error in xmlXPathNewContext\n");
		return 1;
	}
	xpathObj = xmlXPathEvalExpression((xmlChar*) xpath, context);
	xmlXPathFreeContext(context);
	nodeset = xpathObj->nodesetval;
	if(xmlXPathNodeSetIsEmpty(nodeset))
	{
		xmlXPathFreeObject(xpathObj);
    printf("No result\n");
		return 1;
	}
	keyword = xmlNodeListGetString(doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
	if( g_strlen(keyword) == 0)
	{
		printf("enable to get keyword\n");
		return 1;
	}
	g_strcpy(value,keyword);
	xmlFree(keyword);
	return 0;
}

/************************************************************************/
int DEFAULT_CC
xml_send_info(int client, xmlDocPtr doc)
{
	xmlChar* xmlbuff;
	int buff_size, size;
	struct stream* s;

	xmlDocDumpFormatMemory(doc, &xmlbuff, &buff_size, 1);
	printf("data send : %s\n",xmlbuff);
	make_stream(s);
	init_stream(s, 1024);
	out_uint32_be(s,buff_size);
	out_uint8p(s, xmlbuff, buff_size)
	size = s->p - s->data;
	buff_size = g_tcp_send(client, s->data, size, 0);
	free_stream(s);
	xmlFree(xmlbuff);
	return buff_size;
}

/************************************************************************/
int DEFAULT_CC
xml_send_error(int client, const char* message)
{
	xmlChar* xmlbuff;
	xmlDocPtr doc;
	xmlNodePtr node;
	struct stream* s;
	int buff_size, size;

	doc = xmlNewDoc("1.0");
	if (doc == NULL)
	{
		printf("enable to create the document\n");
		return 0;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, "error");
	xmlNodeSetContent(node, message);
	xmlDocSetRootElement(doc, node);


	xmlDocDumpFormatMemory(doc, &xmlbuff, &buff_size, 1);
	printf("data send : %s\n",xmlbuff);

	make_stream(s);
	init_stream(s, 1024);
	out_uint32_be(s,buff_size);
	out_uint8p(s, xmlbuff, buff_size)
	size = s->p - s->data;
	buff_size = g_tcp_send(client, s->data, size, 0);
	free_stream(s);
	xmlFree(xmlbuff);
	return buff_size;
}

/************************************************************************/
int DEFAULT_CC
xml_receive_message(int client, xmlDocPtr* doc)
{
  struct stream* s;
  int data_length;
  int size;

  make_stream(s);
	init_stream(s, 1024);
	g_tcp_recv(client, s->data, sizeof(int), 0);
	in_uint32_be(s,data_length);
  printf("data_length : %i\n", data_length);
  g_tcp_recv(client, s->data, data_length, 0);
  printf("data : %s\n",s->data);
  *doc = xmlReadMemory(s->data, data_length, "noname.xml", NULL, 0);
  free_stream(s);
  return data_length;
}


/************************************************************************/
int DEFAULT_CC
send_sessions(int client)
{
  struct session_item* sess;
  xmlNodePtr node, node2, node3;
  xmlDocPtr doc;
  xmlAttrPtr attr;
  xmlChar *xmlbuff;
  int buffersize, count, i;

	printf("request of sessions list\n");
	sess = (struct session_item*)session_list_session(&count);
	printf("%i count sessions\n",count);
	doc = xmlNewDoc("1.0");
	if (doc ==NULL)
	{
		printf("enable to create the document\n");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, "response");
	node2 = xmlNewNode(NULL, "sessions");
	xmlAddChild(node,node2);
	char prop[128];
	for ( i=0 ; i<count ; i++)
	{
		sprintf(prop, "%i", sess[i].pid);
		node3 = xmlNewNode(NULL, "session");
		xmlSetProp(node3, "id", prop );
		xmlSetProp(node3, "username", sess[i].name );
		xmlSetProp(node3, "status", session_get_status_string(sess[i].status) );
		xmlAddChild(node2, node3);
	}
	xmlAddChild(node, node2);
	xmlDocSetRootElement(doc, node);
	xml_send_info(client, doc);
	xmlFreeDoc(doc);
	return 0;
}

/************************************************************************/
int DEFAULT_CC
send_session(int client, int session_id)
{
  struct session_item* sess = 0;
  xmlNodePtr node, node2;
  xmlDocPtr doc;
  xmlAttrPtr attr;
  xmlChar *xmlbuff;
  int buffersize, count, i;

	printf("request of sessions list\n");
	sess = session_get_bypid(session_id);
  if( sess == NULL)
  {
    printf("The session %i did not exist\n",session_id);
    xml_send_error(client, "the session id of the request did not exist");
    close(client);
    pthread_exit( (void*) 1);
  }
	doc = xmlNewDoc("1.0");
	if (doc ==NULL)
	{
		printf("enable to create the document\n");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, "response");
	char prop[128];
	sprintf(prop, "%i", sess[0].pid);
	node2 = xmlNewNode(NULL, "session");
	xmlSetProp(node2, "id", prop );
	xmlSetProp(node2, "username", sess[0].name );
	xmlSetProp(node2, "status", session_get_status_string(sess[0].status) );
	xmlAddChild(node, node2);
	xmlDocSetRootElement(doc, node);
	xml_send_info(client, doc);
	xmlFreeDoc(doc);
	return 0;
}


/************************************************************************/
int DEFAULT_CC
send_logoff(int client, int session_id)
{
  struct session_item* sess = 0;
  xmlNodePtr node, node2;
  xmlDocPtr doc;
  xmlAttrPtr attr;
  xmlChar *xmlbuff;
  int buffersize, count, i;

	printf("request of sessions list\n");
	sess = session_get_bypid(session_id);
  if( sess == NULL)
  {
    printf("The session %i did not exist\n",session_id);
    xml_send_error(client, "the session id of the request did not exist");
    close(client);
    pthread_exit( (void*) 1);
  }
  session_kill(sess->pid);
  session_destroy(sess->name);
	doc = xmlNewDoc("1.0");
	if (doc ==NULL)
	{
		printf("enable to create the document\n");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, "response");
	node2 = xmlNewNode(NULL, "session");
	char prop[128];
	sprintf(prop, "%i", sess[0].pid);
	node2 = xmlNewNode(NULL, "session");
	xmlSetProp(node2, "id", prop );
	xmlSetProp(node2, "username", sess[0].name );
	xmlSetProp(node2, "status", "CLOSED" );
	xmlAddChild(node, node2);
	xmlDocSetRootElement(doc, node);
	xml_send_info(client, doc);
	xmlFreeDoc(doc);
	return 0;
}


/************************************************************************/
THREAD_RV THREAD_CC
process_request(int client)
{
  int size, session_id;
  int data_length;
  char request_type[128];
  char request_action[128];
  char session_id_string[12];
  xmlDocPtr doc;

  xmlInitParser();
  xml_receive_message(client, &doc);
  if (xml_get_xpath(doc, "/request/@type", request_type) == 1)
  {
  	printf("Enable to get the request type\n");
  	xml_send_error(client, "Enable to get the request type");
  	pthread_exit((void*) 1);
  }
  if (xml_get_xpath(doc, "/request/@action", request_action) == 1)
  {
  	printf("Enable to get the request action\n");
  	xml_send_error(client, "Enable to get the request type");
  	pthread_exit((void*) 1);
  }
  printf("request_type : '%s' \n",request_type);
  printf("request_action : '%s' \n",request_action);

  if( g_strcmp(request_type, "sessions") == 0)
  {
    if( g_strcmp(request_action, "list") != 0)
    {
    	xml_send_error(client, "For session request type only"
    			"the list action is supported");
    	close(client);
    	pthread_exit((void*) 1);
    }
    send_sessions(client);
    xmlFreeDoc(doc);
  	printf("fin\n");
    close(client);
  	pthread_exit((void*) 0);
  }
  if( g_strcmp(request_type, "session") == 0)
  {
    if (xml_get_xpath(doc, "/request/@id", (char**)&session_id_string) == 1)
    {
    	printf("Enable to get the request session id\n");
    	xml_send_error(client, "Enable to get the request session id");
      close(client);
    	pthread_exit((void*) 1);
    }
    session_id = g_atoi(session_id_string);
    if(session_id == 0)
    {
      printf("%i is not a numeric value\n", session_id);
      xml_send_error(client, "enable to convert the session id");
      close(client);
      pthread_exit((void*) 1);
    }
    if( g_strcmp(request_action, "status") == 0)
    {
    	send_session(client, session_id);
    	close(client);
    	pthread_exit((void*) 0);
    }
    if( g_strcmp(request_action, "logoff") == 0)
    {
    	send_logoff(client, session_id);
    	close(client);
    	pthread_exit((void*) 0);
    }
  	xml_send_error(client, "Unknown message for session");
  	close(client);
  	pthread_exit((void*) 1);
  }
  if( g_strcmp(request_type, "internal") == 0)
  {
  	char username[256];
  	if( g_strcmp(request_action, "disconnect") == 0)
  	{
      if (xml_get_xpath(doc, "/request/@username", (char**)&username) == 1)
      {
      	printf("Enable to get the username\n");
      	xml_send_error(client, "Enable to get the username");
        close(client);
      	pthread_exit((void*) 1);
      }
  		session_update_status_by_user(username, SESMAN_SESSION_STATUS_DISCONNECTED);
    	close(client);
    	pthread_exit((void*) 0);
  	}
  	xml_send_error(client, "Unknown message for internal");
  	close(client);
  	pthread_exit((void*) 1);
  }
	xml_send_error(client, "Unknown message");
	close(client);
	pthread_exit((void*) 1);
}


THREAD_RV THREAD_CC
admin_thread(void* param)
{
  int client;
  int server = g_create_unix_socket("/tmp/management");
  while(1)
  {
    printf("wait connection\n");
    int client = g_wait_connection(server);
    tc_thread_create((void*)process_request, (void*)client);
  }
}

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int fd;
  int error;
  int daemon = 1;
  int pid;
  char pid_s[8];
  char text[256];
  char pid_file[256];

  g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);
  if (1 == argc)
  {
    /* no options on command line. normal startup */
    g_printf("starting sesman...\n");
    daemon = 1;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--nodaemon")) ||
                           (0 == g_strcasecmp(argv[1], "-n")) ||
                           (0 == g_strcasecmp(argv[1], "-ns"))))
  {
    /* starts sesman not daemonized */
    g_printf("starting sesman in foregroud...\n");
    daemon = 0;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--help")) ||
                           (0 == g_strcasecmp(argv[1], "-h"))))
  {
    /* help screen */
    g_printf("sesman - xrdp session manager\n\n");
    g_printf("usage: sesman [command]\n\n");
    g_printf("command can be one of the following:\n");
    g_printf("-n, -ns, --nodaemon  starts sesman in foreground\n");
    g_printf("-k, --kill           kills running sesman\n");
    g_printf("-h, --help           shows this help\n");
    g_printf("if no command is specified, sesman is started in background");
    g_exit(0);
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--kill")) ||
                           (0 == g_strcasecmp(argv[1], "-k"))))
  {
    /* killing running sesman */
    /* check if sesman is running */
    if (!g_file_exist(pid_file))
    {
      g_printf("sesman is not running (pid file not found - %s)\n", pid_file);
      g_exit(1);
    }

    fd = g_file_open(pid_file);

    if (-1 == fd)
    {
      g_printf("error opening pid file[%s]: %s\n", pid_file, g_get_strerror());
      return 1;
    }

    error = g_file_read(fd, pid_s, 7);
    if (-1 == error)
    {
      g_printf("error reading pid file: %s\n", g_get_strerror());
      g_file_close(fd);
      g_exit(error);
    }
    g_file_close(fd);
    pid = g_atoi(pid_s);

    error = g_sigterm(pid);
    if (0 != error)
    {
      g_printf("error killing sesman: %s\n", g_get_strerror());
    }
    else
    {
      g_file_delete(pid_file);
    }

    g_exit(error);
  }
  else
  {
    /* there's something strange on the command line */
    g_printf("sesman - xrdp session manager\n\n");
    g_printf("error: invalid command line\n");
    g_printf("usage: sesman [ --nodaemon | --kill | --help ]\n");
    g_exit(1);
  }

  if (g_file_exist(pid_file))
  {
    g_printf("sesman is already running.\n");
    g_printf("if it's not running, try removing ");
    g_printf(pid_file);
    g_printf("\n");
    g_exit(1);
  }

  /* reading config */
  g_cfg = g_malloc(sizeof(struct config_sesman), 1);
  if (0 == g_cfg)
  {
    g_printf("error creating config: quitting.\n");
    g_exit(1);
  }
  g_cfg->log.fd = -1; /* don't use logging before reading its config */
  if (0 != config_read(g_cfg))
  {
    g_printf("error reading config: %s\nquitting.\n", g_get_strerror());
    g_exit(1);
  }

  /* starting logging subsystem */
  error = log_start(&(g_cfg->log));

  if (error != LOG_STARTUP_OK)
  {
    switch (error)
    {
      case LOG_ERROR_MALLOC:
        g_printf("error on malloc. cannot start logging. quitting.\n");
        break;
      case LOG_ERROR_FILE_OPEN:
        g_printf("error opening log file [%s]. quitting.\n", g_cfg->log.log_file);
        break;
    }
    g_exit(1);
  }

  /* libscp initialization */
  scp_init(&(g_cfg->log));

  if (daemon)
  {
    /* start of daemonizing code */
    g_pid = g_fork();

    if (0 != g_pid)
    {
      g_exit(0);
    }

    g_file_close(0);
    g_file_close(1);
    g_file_close(2);

    g_file_open("/dev/null");
    g_file_open("/dev/null");
    g_file_open("/dev/null");
  }

  /* initializing locks */
  lock_init();

  /* signal handling */
  g_pid = g_getpid();
  /* old style signal handling is now managed synchronously by a
   * separate thread. uncomment this block if you need old style
   * signal handling and comment out thread_sighandler_start()
   * going back to old style for the time being
   * problem with the sigaddset functions in sig.c - jts */
#if 1
  g_signal_hang_up(sig_sesman_reload_cfg); /* SIGHUP  */
  g_signal_user_interrupt(sig_sesman_shutdown); /* SIGINT  */
  g_signal_kill(sig_sesman_shutdown); /* SIGKILL */
  g_signal_terminate(sig_sesman_shutdown); /* SIGTERM */
  g_signal_child_stop(sig_sesman_session_end); /* SIGCHLD */
#endif
#if 0
  thread_sighandler_start();
#endif
  if (daemon)
  {
    /* writing pid file */
    fd = g_file_open(pid_file);
    if (-1 == fd)
    {
      log_message(&(g_cfg->log), LOG_LEVEL_ERROR,
                  "error opening pid file[%s]: %s",
                  pid_file, g_get_strerror());
      log_end(&(g_cfg->log));
      g_exit(1);
    }
    g_sprintf(pid_s, "%d", g_pid);
    g_file_write(fd, pid_s, g_strlen(pid_s));
    g_file_close(fd);
  }

  /* start program main loop */
  log_message(&(g_cfg->log), LOG_LEVEL_ALWAYS,
              "starting sesman with pid %d", g_pid);

  /* make sure the /tmp/.X11-unix directory exist */
  if (!g_directory_exist("/tmp/.X11-unix"))
  {
    g_create_dir("/tmp/.X11-unix");
    g_chmod_hex("/tmp/.X11-unix", 0x1777);
  }

  g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_term", g_pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_sync", g_pid);
  g_sync_event = g_create_wait_obj(text);

  tc_thread_create(admin_thread, 0);
  sesman_main_loop();

  /* clean up PID file on exit */
  if (daemon)
  {
    g_file_delete(pid_file);
  }

  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_sync_event);

  if (!daemon)
  {
    log_end(&(g_cfg->log));
  }

  return 0;
}

