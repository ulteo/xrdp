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
  log_message(&(g_cfg->log), LOG_LEVEL_INFO, "sesman[sesman_main_loop]: "
				"listening...");
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
          	log_message(&(g_cfg->log), LOG_LEVEL_INFO, "sesman[sesman_main_loop]: "
									"new connection");
            thread_scp_start(in_sck);
            /* todo, do we have to wait here ? */
          }
        }
      }
      g_delete_wait_obj_from_socket(sck_obj);
    }
    else
    {
    	log_message(&(g_cfg->log), LOG_LEVEL_ERROR, "sesman[sesman_main_loop]: "
						"listen error %d (%s)",
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
		log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_get_xpath]: "
				"error in xmlXPathNewContext");
		return 1;
	}
	xpathObj = xmlXPathEvalExpression((xmlChar*) xpath, context);
	xmlXPathFreeContext(context);
	nodeset = xpathObj->nodesetval;
	if(xmlXPathNodeSetIsEmpty(nodeset))
	{
		xmlXPathFreeObject(xpathObj);
		log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_get_xpath]: "
				"no result");
		return 1;
	}
	keyword = xmlNodeListGetString(doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
	if( keyword == 0)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_get_xpath]: "
				"Unable to get keyword");
		return 1;
	}
	g_strcpy(value, (const char*)keyword);
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
	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[xml_send_info]: "
			"data send : %s\n",xmlbuff);
	make_stream(s);
	init_stream(s, 1024);
	out_uint32_be(s,buff_size);
	out_uint8p(s, xmlbuff, buff_size)
	size = s->p - s->data;
	if (g_tcp_can_send(client, 10))
	{
		buff_size = g_tcp_send(client, s->data, size, 0);
	}
	else
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[xml_send_info]: "
				"Unable to send xml response: %s, cause: %s", xmlbuff, strerror(g_get_errno()));
	}
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

	doc = xmlNewDoc(xmlCharStrdup("1.0"));
	if (doc == NULL)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_send_error]: "
				"Unable to create the document");
		return 0;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, xmlCharStrdup("error"));
	xmlNodeSetContent(node, xmlCharStrdup(message));
	xmlDocSetRootElement(doc, node);


	xmlDocDumpFormatMemory(doc, &xmlbuff, &buff_size, 1);
	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_send_error]: "
			"data send : %s",xmlbuff);

	make_stream(s);
	init_stream(s, 1024);
	out_uint32_be(s,buff_size);
	out_uint8p(s, xmlbuff, buff_size)
	size = s->p - s->data;
	if (g_tcp_can_send(client, 10))
	{
		buff_size = g_tcp_send(client, s->data, size, 0);
	}
	else
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[xml_send_error]: "
				"Unable to send xml response: %s, cause: %s", xmlbuff, strerror(g_get_errno()));
	}
  free_stream(s);
	xmlFree(xmlbuff);
	return buff_size;
}

/************************************************************************/
int DEFAULT_CC
xml_send_success(int client, char* message)
{
	xmlChar* xmlbuff;
	xmlDocPtr doc;
	xmlNodePtr node;
	struct stream* s;
	int buff_size, size;

	doc = xmlNewDoc(xmlCharStrdup("1.0"));
	if (doc == NULL)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_send_success]: "
				"Unable to create the document\n");
		return 0;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, xmlCharStrdup("response"));
	xmlNodeSetContent(node, xmlCharStrdup(message));
	xmlDocSetRootElement(doc, node);

	xmlDocDumpFormatMemory(doc, &xmlbuff, &buff_size, 1);
	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_send_success]: "
			"data send : %s\n",xmlbuff);

	make_stream(s);
	init_stream(s, 1024);
	out_uint32_be(s,buff_size);
	out_uint8p(s, xmlbuff, buff_size)
	size = s->p - s->data;
	if (g_tcp_can_send(client, 10))
	{
		buff_size = g_tcp_send(client, s->data, size, 0);
	}
	else
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[xml_send_success]: "
				"Unable to send xml response: %s, cause: %s", xmlbuff, strerror(g_get_errno()));
	}
  free_stream(s);
	xmlFree(xmlbuff);
	return buff_size;
}


/************************************************************************/
int DEFAULT_CC
xml_send_key_value(int client, char* username, char* key, char* value)
{
  xmlNodePtr node, node2;
  xmlDocPtr doc;

  doc = xmlNewDoc(xmlCharStrdup("1.0"));
	if (doc ==NULL)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[xml_send_key_value]: "
				"Unable to create the document");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, xmlCharStrdup("response"));
	node2 = xmlNewNode(NULL, xmlCharStrdup("user_conf"));
	xmlSetProp(node2, xmlCharStrdup("key"), xmlCharStrdup(key) );
	xmlSetProp(node2, xmlCharStrdup("username"), xmlCharStrdup(username) );
	xmlSetProp(node2, xmlCharStrdup("value"), xmlCharStrdup(value) );
	xmlAddChild(node, node2);
	xmlDocSetRootElement(doc, node);
	xml_send_info(client, doc);
	xmlFreeDoc(doc);
	return 0;
}


/************************************************************************/
int DEFAULT_CC
xml_receive_message(int client, xmlDocPtr* doc)
{
  struct stream* s;
  int data_length;
	int res = 0;
  make_stream(s);
	init_stream(s, 1024);

	if (g_tcp_can_recv(client, 10))
	{
		res= g_tcp_recv(client, s->data, sizeof(int), 0);
	}
	else
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG, "sesman[xml_receive_message]: "
				"Unable to receive xml message, cause %s", strerror(g_get_errno()));
		return 1;
	}

	if (res != sizeof(int))
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG, "sesman[xml_received_message]: "
				"Unable to read size header with error %s", strerror(g_get_errno()));
		return 1;
	}
	in_uint32_be(s,data_length);
	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[xml_received_message]: "
			"data_length : %i", data_length);
  g_tcp_recv(client, s->data, data_length, 0);
  s->data[data_length] = 0;
  log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[xml_received_message]: "
			"data : %s",s->data);
  *doc = xmlReadMemory(s->data, data_length, "noname.xml", NULL, 0);
  free_stream(s);
  return 0;
}


/************************************************************************/
int DEFAULT_CC
send_sessions(int client)
{
  struct session_item* sess;
  xmlNodePtr node, node2, node3;
  xmlDocPtr doc;
  int count, i;

  log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[send_sessions]: "
			"request for sessions list");
	sess = (struct session_item*)session_list_session(&count);
	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[send_sessions]: "
			"%i count sessions",count);
	doc = xmlNewDoc(xmlCharStrdup("1.0"));
	if (doc ==NULL)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[send_sessions]: "
				"Unable to create the document");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, xmlCharStrdup("response"));
	node2 = xmlNewNode(NULL, xmlCharStrdup("sessions"));
	xmlAddChild(node, node2);
	char prop[128];
	for ( i=0 ; i<count ; i++)
	{
		g_sprintf(prop, "%i", sess[i].display);
		node3 = xmlNewNode(NULL, xmlCharStrdup("session"));
		xmlSetProp(node3, xmlCharStrdup("id"), xmlCharStrdup(prop) );
		xmlSetProp(node3, xmlCharStrdup("username"), xmlCharStrdup(sess[i].name) );
		xmlSetProp(node3, xmlCharStrdup("status"), xmlCharStrdup(session_get_status_string(sess[i].status)) );
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

  log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[send_session]: "
			"request for session\n");
	sess = session_get_by_display(session_id);
  if( sess == NULL)
  {
  	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[send_session]: "
				"the session %i did not exist",session_id);
    xml_send_error(client, "the session id of the request did not exist");
    g_tcp_close(client);
    pthread_exit( (void*) 1);
  }
	doc = xmlNewDoc(xmlCharStrdup("1.0"));
	if (doc ==NULL)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[send_session]: "
				"Unable to create the document");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, xmlCharStrdup("response"));
	char prop[128];
	g_sprintf(prop, "%i", sess[0].display);
	node2 = xmlNewNode(NULL, xmlCharStrdup("session"));
	xmlSetProp(node2, xmlCharStrdup("id"), xmlCharStrdup(prop) );
	xmlSetProp(node2, xmlCharStrdup("username"), xmlCharStrdup(sess[0].name) );
	xmlSetProp(node2, xmlCharStrdup("status"), xmlCharStrdup(session_get_status_string(sess[0].status)) );
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
	char prop[128];
	int display;

	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG, "sesman[send_logoff]: "
			"request session %i logoff",session_id);
	sess = session_get_by_display(session_id);
  if( sess == NULL)
  {
    log_message(&(g_cfg->log), LOG_LEVEL_DEBUG, "sesman[send_logoff]: "
        "The session %i did not exist", session_id);
    xml_send_error(client, "the session id of the request did not exist");
    g_tcp_close(client);
    pthread_exit( (void*) 1);
  }

	session_update_status_by_user(sess->name, SESMAN_SESSION_STATUS_TO_DESTROY);
	doc = xmlNewDoc(xmlCharStrdup("1.0"));
	if (doc ==NULL)
	{
		log_message(&(g_cfg->log), LOG_LEVEL_DEBUG, "sesman[send_logoff]: "
				"Unable to create the document");
		return 1;
	}
	doc->encoding = xmlCharStrdup("UTF-8");
	node = xmlNewNode(NULL, xmlCharStrdup("response"));
	node2 = xmlNewNode(NULL, xmlCharStrdup("session"));
	sprintf(prop, "%i", display);
	node2 = xmlNewNode(NULL, xmlCharStrdup("session"));
	xmlSetProp(node2, xmlCharStrdup("id"), xmlCharStrdup(prop) );
	xmlSetProp(node2, xmlCharStrdup("username"), xmlCharStrdup(sess->name) );
	xmlSetProp(node2, xmlCharStrdup("status"), xmlCharStrdup("CLOSED") );
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
  int session_id;
  char request_type[128];
  char request_action[128];
  char session_id_string[12];
  xmlDocPtr doc;

  xmlInitParser();
  if (xml_receive_message(client, &doc) == 1)
  {
  	g_tcp_close(client);
  	pthread_exit((void*) 1);
  }
  if (xml_get_xpath(doc, "/request/@type", request_type) == 1)
  {
  	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[process_request]: "
  			"Unable to get the request type");
  	xml_send_error(client, "Unable to get the request type");
  	pthread_exit((void*) 1);
  }
  if (xml_get_xpath(doc, "/request/@action", request_action) == 1)
  {
  	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[process_request]: "
				"Unable to get the request action");
  	xml_send_error(client, "Unable to get the request type");
  	pthread_exit((void*) 1);
  }
  log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[process_request]: "
				"Request_type : '%s' ", request_type);
  log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[process_request]: "
				"Request_action : '%s' ", request_action);

  if( g_strcmp(request_type, "sessions") == 0)
  {
    if( g_strcmp(request_action, "list") != 0)
    {
    	xml_send_error(client, "For session request type only"
    			"the list action is supported");
    	g_tcp_close(client);
    	pthread_exit((void*) 1);
    }
    send_sessions(client);
    xmlFreeDoc(doc);
    g_tcp_close(client);
  	pthread_exit((void*) 0);
  }
  if( g_strcmp(request_type, "session") == 0)
  {
    if (xml_get_xpath(doc, "/request/@id", session_id_string) == 1)
    {
    	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
						"Unable to get the request session id");
    	xml_send_error(client, "Unable to get the request session id");
      g_tcp_close(client);
    	pthread_exit((void*) 1);
    }
    session_id = g_atoi(session_id_string);
    if(session_id == 0)
    {
    	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
						"%i is not a numeric value", session_id);
      xml_send_error(client, "Unable to convert the session id");
      g_tcp_close(client);
      pthread_exit((void*) 1);
    }
    if( g_strcmp(request_action, "status") == 0)
    {
    	send_session(client, session_id);
    	g_tcp_close(client);
    	pthread_exit((void*) 0);
    }
    if( g_strcmp(request_action, "logoff") == 0)
    {
    	send_logoff(client, session_id);
    	g_tcp_close(client);
    	pthread_exit((void*) 0);
    }
  	xml_send_error(client, "Unknown message for session");
  	g_tcp_close(client);
  	pthread_exit((void*) 1);
  }
  if( g_strcmp(request_type, "internal") == 0)
  {
  	char username[256];
  	if( g_strcmp(request_action, "disconnect") == 0)
  	{
      if (xml_get_xpath(doc, "/request/@username", username) == 1)
      {
      	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
							"Unable to get the username\n");
      	xml_send_error(client, "Unable to get the username");
      	g_tcp_close(client);
      	pthread_exit((void*) 1);
      }
  		session_update_status_by_user(username, SESMAN_SESSION_STATUS_DISCONNECTED);
  		g_tcp_close(client);
    	pthread_exit((void*) 0);
  	}
  	if( g_strcmp(request_action, "logoff") == 0)
  	{
      if (xml_get_xpath(doc, "/request/@username", username) == 1)
      {
      	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
							"Unable to get the username\n");
      	xml_send_error(client, "Unable to get the username");
      	g_tcp_close(client);
      	pthread_exit((void*) 1);
      }
  		session_update_status_by_user(username, SESMAN_SESSION_STATUS_TO_DESTROY);
  		g_tcp_close(client);
    	pthread_exit((void*) 0);
  	}
  	xml_send_error(client, "Unknown message for internal");
  	g_tcp_close(client);
  	pthread_exit((void*) 1);
  }
  if( g_strcmp(request_type, "user_conf") == 0)
  {
  	char username[256];
  	char key[128];
  	char value[256];
    if (xml_get_xpath(doc, "/request/@username", username) == 1)
    {
    	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
						"Unable to get the username\n");
    	xml_send_error(client, "Unable to get the username");
    	g_tcp_close(client);
    	pthread_exit((void*) 1);
    }
    if (xml_get_xpath(doc, "/request/@key", key) == 1)
    {
    	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
    			"Unable to get the key in the request");
    	xml_send_error(client, "Unable to get the key in the request");
    	g_tcp_close(client);
    	pthread_exit((void*) 1);
    }
  	if( g_strcmp(request_action, "set") == 0)
  	{
      if (xml_get_xpath(doc, "/request/@value", value) == 1)
      {
      	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
							"Unable to get the value int the request\n");
      	xml_send_error(client, "Unable to get the value in the request");
      	g_tcp_close(client);
      	pthread_exit((void*) 1);
      }
  		if (session_set_user_pref(username, key, value) == 0 )
  		{
  			xml_send_success(client, "SUCCESS");
  		}
  		else
  		{
  			xml_send_error(client, "Unable to set preference");
  		}
  		g_tcp_close(client);
    	pthread_exit((void*) 0);
  	}

  	if( g_strcmp(request_action, "get") == 0)
  	{
  		if(session_get_user_pref(username, key, value) == 0)
  		{
  			xml_send_key_value(client, username, key, value);
  		}
  		else
  		{
  			xml_send_error(client, "Unable to get preference");
  		}
  		g_tcp_close(client);
    	pthread_exit((void*) 0);
  	}
  	xml_send_error(client, "Unknown message for internal");
  	g_tcp_close(client);
  	pthread_exit((void*) 1);
  }
	xml_send_error(client, "Unknown message");
	g_tcp_close(client);
	pthread_exit((void*) 1);
}


THREAD_RV THREAD_CC
admin_thread(void* param)
{
  int server = g_create_unix_socket(MANAGEMENT_SOCKET_NAME);
	g_chmod_hex(MANAGEMENT_SOCKET_NAME, 0xFFFF);
  while(1)
  {
  	log_message(&(g_cfg->log), LOG_LEVEL_DEBUG_PLUS, "sesman[admin_thread]: "
					"wait connection");
    int client = g_wait_connection(server);
    if (client < 0)
    {
    	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "sesman[process_request]: "
							"Unable to get client from management socket [%s]", strerror(g_get_errno()));
    }
    else
    {
    	tc_thread_create((void*)process_request, (void*)client);
    }
  }
}

THREAD_RV THREAD_CC
monit_thread(void* param)
{
  while(1)
  {
  	g_sleep(2000);
  	session_monit();
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

  if(g_is_root() != 0){
  	g_printf("Error, xrdp-sesman service must be start with root privilege\n");
  	return 0;
  }


  g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);
  if (1 == argc)
  {
    /* no options on command line. normal startup */
  	g_printf("starting sesman...");
    daemon = 1;
  }
  else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--nodaemon")) ||
                           (0 == g_strcasecmp(argv[1], "-n")) ||
                           (0 == g_strcasecmp(argv[1], "-ns"))))
  {
    /* starts sesman not daemonized */
  	g_printf("starting sesman in foregroud...");
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
    if (g_daemonize(pid_file) == 0)
    {
      g_writeln("problem daemonize");
      g_exit(1);
    }
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
  //g_signal_child_stop(sig_sesman_session_end); /* SIGCHLD */
#endif
#if 0
  thread_sighandler_start();
#endif

  /* start program main loop */
  log_message(&(g_cfg->log), LOG_LEVEL_ALWAYS,
              "starting sesman with pid %d", g_pid);

  /* make sure the /tmp/.X11-unix directory exist */
  if (!g_directory_exist("/tmp/.X11-unix"))
  {
    g_create_dir("/tmp/.X11-unix");
    g_chmod_hex("/tmp/.X11-unix", 0x1777);
  }

  if (!g_directory_exist(XRDP_SOCKET_PATH))
  {
    g_create_dir(XRDP_SOCKET_PATH);
    g_chmod_hex(XRDP_SOCKET_PATH, 0x1777);
  }

  g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_term", g_pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_sync", g_pid);
  g_sync_event = g_create_wait_obj(text);

  tc_thread_create(admin_thread, 0);
  tc_thread_create(monit_thread, 0);
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

