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

#include "printer_dev.h"
#include <cups/cups.h>
//#include <cups/i18n.h> /* hardy problems */
#include <errno.h>
#include <zlib.h>

extern struct log_config log_conf;
extern char* username;
static struct printer_device printer_devices[128];
static printer_devices_count = 0;

/************************************************************************/
int				/* O - 0 if name is no good, 1 if name is good */
printer_dev_validate_name(const char *name)		/* I - Name to check */
{
  const char	*ptr;			/* Pointer into name */
  for (ptr = name; *ptr; ptr ++)
  {
    if (*ptr == '@')
    {
      break;
    }
    else
    {
    	if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 ||
    			*ptr == '/' || *ptr == '#')
    	{
    		return 0;
    	}
    }
  }
  return ((ptr - name) < 128);
}


/************************************************************************/
int
printer_dev_server_connect(http_t **http )
{
	if (! *http)
	{
		*http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());
		if (*http == NULL)
	  {
			return 1;
	  }
	}
	return 0;
}

/************************************************************************/
int
printer_dev_server_disconnect(http_t *http )
{
	if (!http)
	{
		httpClose(http);
	}
	return 0;

}

/************************************************************************/
int
printer_dev_add_printer(http_t* http, char* lp_name, char* device_uri)
{
  ipp_t		*request = NULL,		/* IPP Request */
					*response =NULL;		/* IPP Response */
  char		uri[HTTP_MAX_URI] = {0};	/* URI for printer/class */

  log_message(&log_conf, LOG_LEVEL_DEBUG, "printer_dev[printer_add]: "
  		"add_printer %s of type %s ", lp_name,device_uri);
  request = ippNewRequest(CUPS_ADD_MODIFY_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  if (device_uri[0] == '/')
  {
    snprintf(uri, sizeof(uri), "file://%s", device_uri);
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, uri);
  }
  else
  {
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, device_uri);
  }

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "printer_dev[printer_add]:"
    		" %s\n", cupsLastErrorString());
    return 1;
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "printer_dev[printer_add]:"
    		" %s\n", cupsLastErrorString());
    ippDelete(response);
    return 1;
  }
  else
  {
    ippDelete(response);
    return (0);
  }
}


/************************************************************************/
int
printer_dev_del_printer(http_t* http, char* lp_name)
{
	ipp_t		*request,		/* IPP Request */
					*response;		/* IPP Response */
	char		uri[HTTP_MAX_URI];	/* URI for printer/class */

	log_message(&log_conf, LOG_LEVEL_DEBUG, "printer_dev[printer_del] : "
			"delete_printer %s\n", lp_name);
	request = ippNewRequest(CUPS_DELETE_PRINTER);
	httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                 "localhost", 0, "/printers/%s", lp_name);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
             "printer-uri", NULL, uri);

	if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
	{
    log_message(&log_conf, LOG_LEVEL_ERROR, "printer_dev[printer_add]:"
    		" %s\n", cupsLastErrorString());
		return 1;
	}
	else if (response->request.status.status_code > IPP_OK_CONFLICT)
	{
    log_message(&log_conf, LOG_LEVEL_ERROR, "printer_dev[printer_add]:"
    		" %s\n", cupsLastErrorString());
    ippDelete(response);
    return 1;
	}
	else
	{
		ippDelete(response);
		return 0;
	}
}



/************************************************************************/
int
printer_dev_set_ppd(http_t *http, char* lp_name, char* ppd_file)
{
  ipp_t		*request,		/* IPP Request */
					*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */
  char		tempfile[1024];		/* Temporary filename */
  int		fd;			/* Temporary file */
  gzFile	*gz;			/* GZIP'd file */
  char		buffer[8192];		/* Copy buffer */
  int		bytes;			/* Bytes in buffer */


  printf("set_printer_file(%p, \"%s\", \"%s\")\n", http, lp_name, ppd_file);

 /*
  * See if the file is gzip'd; if so, unzip it to a temporary file and
  * send the uncompressed file.
  */

  if (!strcmp(ppd_file + strlen(ppd_file) - 3, ".gz"))
  {
   /*
    * Yes, the file is compressed; uncompress to a temp file...
    */

    if ((fd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      printf("ERROR: Unable to create temporary file");
      return (1);
    }

    if ((gz = gzopen(ppd_file, "rb")) == NULL)
    {
      printf("lpadmin: Unable to open file \"%s\": %s\n",ppd_file, strerror(errno));
      close(fd);
      unlink(tempfile);
      return (1);
    }

    while ((bytes = gzread(gz, buffer, sizeof(buffer))) > 0)
      write(fd, buffer, bytes);

    close(fd);
    gzclose(gz);

    ppd_file = tempfile;
  }

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  response = cupsDoFileRequest(http, request, "/admin/", ppd_file);
  ippDelete(response);

 /*
  * Remove the temporary file as needed...
  */

  if (ppd_file == tempfile)
    unlink(tempfile);

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    printf( "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}

/************************************************************************/
int
printer_dev_do_operation(http_t * http, int operation, char* lp_name)
{
	ipp_t		*request;		/* IPP Request */
	char		uri[HTTP_MAX_URI];	/* URI for printer/class */
	char* reason = NULL;

	printf("Do operation (%i => %p, \"%s\")\n", operation, http, lp_name);
  request = ippNewRequest(operation);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());


 	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                 "printer-state-message", NULL, reason);

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
  	printf("Operation failed: %s\n", ippErrorString(cupsLastError()));
  	return 1;
  }

  request = ippNewRequest(IPP_PURGE_JOBS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                 "printer-uri", NULL, uri);

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
	{
  	printf("%s\n", cupsLastErrorString());
  	return 1;
	}
  return 0;
}

/************************************************************************/
int
printer_dev_get_restricted_user(http_t* http, char* lp_name, char* user_list)
{
  ipp_t	*request;		/* IPP Request */
	ipp_t	*response;
  char uri[HTTP_MAX_URI];	/* URI for printer/class */
  ipp_attribute_t *attr;
  int i=0;
  char* user_p = user_list;
  int size = 0;

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", lp_name);

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  response = cupsDoRequest(http, request, "/admin/");
  attr = ippFindAttribute(response, "requesting-user-name-allowed", IPP_TAG_NAME);
  if (attr == 0 || attr->num_values == 0)
  {
  	return 0;
  }
  for(i=0 ; i < attr->num_values ; i++)
  {
  	size = sprintf(user_p, "%s,",attr->values[i].string.text);
  	user_p+= size;
  }
  printf("requesting-user-name-allowed : %s\n", user_list);
  return attr->num_values;
}

/************************************************************************/
int
printer_dev_restrict_user(http_t* http, char* lp_name, char* user)
{
	int num_options = 0;
  cups_option_t	*options;
  ipp_t	*request;		/* IPP Request */
  char uri[HTTP_MAX_URI];	/* URI for printer/class */
  char user_buffer[1024] = {0};

  /*
  * Add the options...
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                  "localhost", 0, "/printers/%s", lp_name);

  request = ippNewRequest(CUPS_ADD_MODIFY_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);

  if( printer_dev_get_restricted_user(http, lp_name, user_buffer) == 0)
  {
  	sprintf(user_buffer, "%s", user);
  }
  else
  {
  	sprintf(user_buffer, "%s%s", user_buffer, user);
  }
  num_options = cupsAddOption("requesting-user-name-denied",
	                          "all", num_options, &options);
  num_options = cupsAddOption("requesting-user-name-allowed",
                          user_buffer, num_options,&options);
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_PRINTER);
  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    printf("lpadmin: %s\n", cupsLastErrorString());
    return 1;
  }
  else
  {
    return 0;
  }
}

int APP_CC
printer_dev_add(struct stream* s, int device_data_length,
								int device_id, char* dos_name)
{
  int flags;
  int ignored;
  int pnp_name_len;
  int printer_name_len;
  int driver_name_len;
  int cached_field_name;
  char pnp_name[256] = {0};			/* ignored */
  char driver_name[256] = {0};
  char printer_name[256] = {0};
	http_t *http = NULL;

  in_uint32_le(s, flags);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
		  "flags = %i", flags);
  in_uint32_le(s, ignored);
  in_uint32_le(s, pnp_name_len);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
		  "pnp_name_len = %i", pnp_name_len);
  in_uint32_le(s, driver_name_len);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
		  "driver_name_len = %i", driver_name_len);
  in_uint32_le(s, printer_name_len);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
		  "print_name_len = %i", printer_name_len);
  in_uint32_le(s, cached_field_name);
  if(pnp_name_len != 0)
  {
    in_unistr(s, pnp_name, sizeof(pnp_name), pnp_name_len);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
  		  "pnp_name = %s", pnp_name);
  }
  if(driver_name_len != 0)
  {
    in_unistr(s, driver_name, sizeof(driver_name), driver_name_len);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
		  "driver_name = %s", driver_name);
  }
  if(printer_name_len != 0)
  {
    in_unistr(s, printer_name, sizeof(printer_name), printer_name_len);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[printer_dev_add]: "
		  "printer name = %s", printer_name);
  }

  log_message(&log_conf, LOG_LEVEL_DEBUG, "try to connect to cups server");
	if (printer_dev_server_connect(&http) == 1)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "enable to connect to printer server\n");
		return 1;
	}
	if( printer_dev_add_printer(http, printer_name, DEVICE_URI) !=0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "failed to add printer\n");
		printer_dev_server_disconnect(http);
		return 1;
	}
	if(printer_dev_set_ppd(http,printer_name, PPD_FILE) !=0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "failed to set ppd file\n");
		printer_dev_server_disconnect(http);
		return 1;
	}
	log_message(&log_conf, LOG_LEVEL_DEBUG, "Succed to add printer\n");
	printer_dev_do_operation(http, IPP_RESUME_PRINTER, printer_name);
	printer_dev_do_operation(http, CUPS_ACCEPT_JOBS, printer_name);
	printer_dev_restrict_user(http, printer_name, username);
	printer_dev_server_disconnect(http);
	printer_devices[printer_devices_count].device_id = device_id;
	g_strcpy(printer_devices[printer_devices_count].printer_name, printer_name);
  return 0;
}

/************************************************************************/
int APP_CC
printer_dev_get_printer(int device_id)
{
	int i;
	for (i=0 ; i< printer_devices_count ; i++)
	{
		if(device_id == printer_devices[i].device_id)
		{
			return i;
		}
	}
	return -1;
}
/************************************************************************/
int APP_CC
printer_dev_del(int device_id)
{
	int printer_index;
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
			"printer remove : %i\n",device_id);
	printer_index = printer_dev_get_printer(device_id);
	if (printer_index == -1)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[printer_dev_del]:"
				"the printer %i did not exist\n",device_id);
		return 1;
	}
	/* get user of printer */
	/* remove user of list */
	/* update */
	/* remove it if on user exist */
}
