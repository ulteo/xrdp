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

#include <vchannel.h>
#include <os_calls.h>
#include <xrdp_constants.h>
#include <file.h>
#include "rdpdr.h"
#include "defines.h"


struct log_config	*l_config;
static char hostname[256];
static int use_unicode;
static int vers_major;
static int vers_minor;
static int client_id;
static int supported_operation[6] = {0};
struct device device_list[128];
int device_count = 0;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
char username[256];
static int printer_sock;
//static int disk_sock;
static int rdpdr_sock;

/*****************************************************************************/
int APP_CC
rdpdr_send(struct stream* s){
  int rv;
  int length = (int)(s->end - s->data);

  rv = vchannel_send(rdpdr_sock, s->data, length);
  if (rv != 0)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_send]: "
    		"Enable to send message");
  }
/*  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_send]: "
				"send message: ");
  log_hexdump(l_config, LOG_LEVEL_DEBUG, (unsigned char*)s->data, length );
*/
  return rv;
}

/*****************************************************************************/
int APP_CC
rdpdr_transmit(int sock, int type, char* mess, int length)
{
  struct stream* header;
  int rv;
  int temp_size = 0;
  char* p;
  int total_length;
  if (sock < 1)
  {
    log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[rdpdr_transmit]: "
    		"The socket is not open");
    return 1;
  }
  total_length = length;
  while (temp_size < total_length)
  {
  	length = MIN(1600, total_length - temp_size);
  	make_stream(header);
  	init_stream(header, 9);
  	out_uint8(header, type);
  	out_uint32_be(header, length);
  	out_uint32_be(header, total_length);
  	s_mark_end(header);
  	rv = g_tcp_send(sock, header->data, 9, 0);
  	log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "vchannel_rdpdr[rdpdr_transmit]: "
  			"Header sended: %s", header->data);
  	log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)header->data, 9);
  	if (rv != 9)
  	{
  		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_transmit]: "
  				"error while sending the header");
  		free_stream(header);
  		return 1;
  	}
  	free_stream(header);
  	p = mess+temp_size;
  	rv = g_tcp_send(sock, p, length, 0);
  	log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "vchannel_rdpdr[rdpdr_transmit]: "
  			"Message sended: ");
  	log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)mess, 1600);
  	if (rv != 1600)
  	{
  		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_transmit]: "
  				"error while sending the message: %s", mess);
  		return 1;
  	}
  	temp_size+=1600;
  }
	return 0;
}


/*****************************************************************************/
int APP_CC
rdpdr_in_unistr(struct stream* s, char *string, int str_size, int in_len)
{
#ifdef HAVE_ICONV
  size_t ibl = in_len, obl = str_size - 1;
  char *pin = (char *) s->p, *pout = string;
  static iconv_t iconv_h = (iconv_t) - 1;

  if (g_iconv_works)
  {
    if (iconv_h == (iconv_t) - 1)
    {
      if ((iconv_h = iconv_open(g_codepage, WINDOWS_CODEPAGE)) == (iconv_t) - 1)
      {
        log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[rdpdr_in_unistr]: iconv_open[%s -> %s] fail %p",
					WINDOWS_CODEPAGE, g_codepage, iconv_h);
        g_iconv_works = False;
        return rdp_in_unistr(s, string, str_size, in_len);
      }
    }

    if (iconv(iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
    {
      if (errno == E2BIG)
      {
        log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[rdpdr_in_unistr]: "
							"server sent an unexpectedly long string, truncating");
      }
      else
      {
        iconv_close(iconv_h);
        iconv_h = (iconv_t) - 1;
        log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[rdpdr_in_unistr]: "
							"iconv fail, errno %d\n", errno);
        g_iconv_works = False;
        return rdpdr_in_unistr(s, string, str_size, in_len);
      }
    }

    /* we must update the location of the current STREAM for future reads of s->p */
    s->p += in_len;
    *pout = 0;
    return pout - string;
  }
  else
#endif
  {
    int i = 0;
    int len = in_len / 2;
    int rem = 0;

    if (len > str_size - 1)
    {
      log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr[rdpdr_in_unistr]: "
						"server sent an unexpectedly long string, truncating");
      len = str_size - 1;
      rem = in_len - 2 * len;
    }
    while (i < len)
    {
      in_uint8a(s, &string[i++], 1);
      in_uint8s(s, 1);
    }
    in_uint8s(s, rem);
    string[len] = 0;
    return len;
  }
}

/*****************************************************************************/
int APP_CC
rdpdr_get_socket_from_device_id(int device_id)
{
	int i;
	int device_type;
	for(i=0 ; i<device_count ; i++)
	{
		if(device_list[i].device_id == device_id)
		{
			device_type = device_list[i].device_type;
			switch(device_type)
			{
			case RDPDR_DTYP_PRINT:
				return printer_sock;

				default:
				  log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_get_socket_from_device_id]: "
								"Unknow device type : %02x", device_type);
				return -1;
			}
		}
	}
	return -1;
}

/*****************************************************************************/
int APP_CC
rdpdr_send_init(void)
{
  struct stream* s;

  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_send_init]: "
				"Send init message");
  /*server announce*/
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_SERVER_ANNOUNCE);
  out_uint16_le(s, 0x1);
  out_uint16_le(s, 0x0c);
  out_uint32_le(s, 0x1);
  s_mark_end(s);
  rdpdr_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_confirm_clientID_request()
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_send_capability_request]: Send Server Client ID Confirm Request");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */

  s_mark_end(s);
  rdpdr_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_clientID_confirm(struct stream* s)
{
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_process_message]: new message: PAKID_CORE_CLIENTID_CONFIRM");
    in_uint16_le(s, vers_major);
    in_uint32_le(s, vers_minor);
    in_uint32_le(s, client_id);
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_process_message]: version : %i:%i, client_id : %i", vers_major, vers_minor, client_id);
    return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_client_name(struct stream* s)
{
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_process_message]: new message: PAKID_CORE_CLIENT_NAME");
  int hostname_size;
  in_uint32_le(s, use_unicode);
  in_uint32_le(s, hostname_size);   /* flag not use */
  in_uint32_le(s, hostname_size);
  if (hostname_size < 1)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr_channel[rdpdr_process_message]: no hostname specified");
    return 1;
  }
  if (use_unicode == 1)
  {
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_process_message]: unicode is used");
    rdpdr_in_unistr(s, hostname, sizeof(hostname), hostname_size);
  }
  else
  {
    in_uint8a(s, hostname, hostname_size);
  }
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_process_message]: hostname : '%s'",hostname);
  if (g_strlen(hostname) >0)
  {
    rdpdr_confirm_clientID_request();
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_client_capability(struct stream* s)
{
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: new message: PAKID_CORE_CLIENT_CAPABILITY");
  int capability_count, ignored, temp, general_capability_version, rdp_version, i;

  in_uint16_le(s, capability_count);
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: capability number : %i", capability_count);
  if (capability_count == 0 )
  {
    log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr_channel[rdpdr_client_capability]: No capability ");
    return 1;
  }
  in_uint16_le(s, ignored);
  /* GENERAL_CAPS_SET */
  in_uint16_le(s, temp);		/* capability type */
  if (temp != CAP_GENERAL_TYPE )
  {
    log_message(l_config, LOG_LEVEL_ERROR,
    		"vchannel_rdpdr_channel[rdpdr_client_capability]: malformed message (normaly  CAP_GENERAL_TYPE)");
    return 1;
  }
  in_uint16_le(s, ignored);		/* capability length */
  in_uint32_le(s, general_capability_version);		/* general capability version */
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
		  "general capability version = %i ",general_capability_version);
  if (general_capability_version != GENERAL_CAPABILITY_VERSION_01 &&
		  general_capability_version != GENERAL_CAPABILITY_VERSION_02  )
  {
    log_message(l_config, LOG_LEVEL_ERROR,
    		"vchannel_rdpdr_channel[rdpdr_client_capability]: malformed message "
    		"(normaly general_capability_version = [GENERAL_CAPABILITY_VERSION_01|GENERAL_CAPABILITY_VERSION_02])");
    return 1;
  }
  /* Capability message */
  in_uint32_le(s, ignored);		/* OS type */
  in_uint32_le(s, ignored);		/* OS version */
  in_uint16_le(s, ignored);		/* major version */
  in_uint16_le(s, rdp_version);	/* minor version */
  log_message(l_config, LOG_LEVEL_DEBUG,
      		"vchannel_rdpdr_channel[rdpdr_client_capability]: RDP version = %i ",rdp_version);
  if(rdp_version == RDP6)
  {
    log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
    		"only RDP5 is supported");
  }
  in_uint16_le(s, temp);	/* oiCode1 */
  if (temp != RDPDR_IRP_MJ_ALL)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
	      		"client did not support all the IRP operation");
    return 1;
  }
  in_uint16_le(s, ignored);			/* oiCode2(unused) */
  in_uint32_le(s, ignored);			/* extendedPDU(unused) */
  in_uint32_le(s, ignored);			/* extraFlags1 */
  in_uint32_le(s, ignored);			/* extraFlags2 */
  in_uint32_le(s, ignored);			/* SpecialTypeDeviceCap (device redirected before logon (smartcard/com port */

  for( i=1 ; i<capability_count ; i++ )
  {
    in_uint16_le(s, temp);	/* capability type */
    switch (temp)
    {
    case CAP_DRIVE_TYPE:
      log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
    		  "CAP_DRIVE_TYPE supported by the client");
      supported_operation[temp]=1;
      break;
    case CAP_PORT_TYPE:
      log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
    		  "CAP_PORT_TYPE supported by the client but not by the server");
      supported_operation[temp]=1;
      break;
    case CAP_PRINTER_TYPE:
      log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
    		  "CAP_PRINTER_TYPE supported by the client");
      supported_operation[temp]=1;
      break;
    case CAP_SMARTCARD_TYPE:
      log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_client_capability]: "
    		  "CAP_SMARTCARD_TYPE supported by the client but not by the server");
      supported_operation[temp]=1;
      break;
    default:
      log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_client_capability]: "
      		  "invalid capability type : %i", temp);
      break;
    }
    in_uint16_le(s, ignored);	/* capability length */
    in_uint32_le(s, ignored);	/* capability version */
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_devicelist_announce(struct stream* s)
{
  int device_list_count, device_id, device_type, device_data_length;
  int i;
  char dos_name[9] = {0};

  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rpdr_devicelist_announce]: "
  		"	new message: PAKID_CORE_DEVICELIST_ANNOUNCE");
  in_uint32_le(s, device_list_count);	/* DeviceCount */
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rpdr_devicelist_announce]: "
		  "%i device(s) declared", device_list_count);
  /* device list */
  for( i=0 ; i<device_list_count ; i++)
  {
    in_uint32_le(s, device_type);
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rpdr_devicelist_announce]: "
    		"device type: %i", device_type);
    in_uint32_le(s, device_id);
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rpdr_devicelist_announce]: "
  		  "device id: %i", device_id);

    in_uint8a(s,dos_name,8)
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rpdr_devicelist_announce]: "
  		  "dos name: '%s'", dos_name);
    in_uint32_le(s, device_data_length);
    log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rpdr_devicelist_announce]: "
  		  "data length: %i", device_data_length);

    device_list[device_count].device_id = device_id;
    device_list[device_count].device_type = device_type;
    device_count++;
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_process_message(struct stream* s, int length, int total_length)
{
  int component;
  int packetId;
  int result = 0;
  int device_id;
  int device_sock;
  struct stream* packet;

  if(length != total_length)
  {
  	log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "vchannel_rdpdr[rdpdr_process_message]: "
  			"packet is fragmented");
  	if(is_fragmented_packet == 0)
  	{
  		log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "vchannel_rdpdr[rdpdr_process_message]: "
  				"packet is fragmented : first part");
  		is_fragmented_packet = 1;
  		fragment_size = length;
  		make_stream(splitted_packet);
  		init_stream(splitted_packet, total_length);
  		g_memcpy(splitted_packet->p,s->p, length );
  		log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)s->p, length);
  		return 0;
  	}
  	else
  	{
  		g_memcpy(splitted_packet->p+fragment_size, s->p, length );
  		fragment_size += length;
  		if (fragment_size == total_length )
  		{
    		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_message]: "
    				"packet is fragmented : last part");
  			packet = splitted_packet;
  		}
  		else
  		{
    		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_message]: "
    				"packet is fragmented : next part");
  			return 0;
  		}
  	}
  }
  else
  {
  	packet = s;
  }
  log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "vchannel_rdpdr_channel[rdpdr_process_message]: data received:");
  log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)packet->p, total_length);
  in_uint16_le(packet, component);
  in_uint16_le(packet, packetId);
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr_channel[rdpdr_process_message]: component=0x%04x packetId=0x%04x", component, packetId);
  if ( component == RDPDR_CTYP_CORE )
  {
    switch (packetId)
    {
    case PAKID_CORE_CLIENTID_CONFIRM :
      result = rdpdr_clientID_confirm(packet);
      break;

    case PAKID_CORE_CLIENT_NAME :
    	result =  rdpdr_client_name(packet);
    	break;

    case PAKID_CORE_CLIENT_CAPABILITY:
    	result = rdpdr_client_capability(packet);
    	break;

    case PAKID_CORE_DEVICELIST_ANNOUNCE:
    	rdpdr_devicelist_announce(packet);
    	result = rdpdr_transmit(printer_sock, DATA_MESSAGE, packet->data, total_length);
      break;
    case PAKID_CORE_DEVICE_IOCOMPLETION:
    	in_uint32_le(packet, device_id);
    	device_sock = rdpdr_get_socket_from_device_id(device_id);
    	if (device_sock == -1)
    	{
        log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr_channel[rdpdr_process_message]: "
        		"Unknow device type, failed to redirect message");
        break;
    	}
    	result = rdpdr_transmit(printer_sock, DATA_MESSAGE, packet->data, total_length);
    	break;
    default:
      log_message(l_config, LOG_LEVEL_WARNING, "vchannel_rdpdr_channel[rdpdr_process_message]: "
      		"Unknown message %02x",packetId);
      result = 1;
    }
    if(is_fragmented_packet == 1)
    {
    	is_fragmented_packet = 0;
    	fragment_size = 0;
    	free_stream(packet);
    }

  }
  return result;
}

/*****************************************************************************/
int DEFAULT_CC
rdpdr_process_channel_opening(int client, char* device)
{
	struct stream* s;
  int data_length;
  int size;
  int type;

	make_stream(s);
	init_stream(s, 1024);

	size = g_tcp_recv(client, s->data, sizeof(int)+1, 0);

	if ( size < 1)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_channel_opening]: "
				"enable to get information on the opening channel");
		g_tcp_close(client);
		free_stream(s);
		return 0;
	}
	in_uint8(s, type);
	if(type != CHANNEL_OPEN)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_channel_opening]: "
				"Invalid operation type");
		free_stream(s);
		return 1;
	}
	in_uint32_be(s, data_length);
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_channel_opening]: "
			"Data_length : %i\n", data_length);
	size = g_tcp_recv(client, s->data, data_length, 0);
	s->data[data_length] = 0;
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_channel_opening]: "
			"Channel name : %s\n",s->data);

	if(g_strcmp(device, s->data) == 0)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_process_channel_opening]: "
				"New server connection for device %s ", device);
		free_stream(s);
		return 0;
	}
	log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_process_channel_opening]: "
				"Unable to open device program channel for %s", device);
	free_stream(s);
	return 1;
}

/*****************************************************************************/
int APP_CC
rdpdr_init_devices()
{
	int display_num;
	char socket_filename[256];
	int pid;
	int sock;
	char printer_program_path[256];

	display_num = g_get_display_num_from_display(g_getenv("DISPLAY"));
	if(display_num == 0)
	{
	  log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_init]: "
	  		"Unable to get display");
	  return 1;
	}
  log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_init]: "
  		"Activate all the device type");
	/* socket creation */
  g_sprintf(socket_filename, "/var/spool/xrdp/%i/vchannel_printer", display_num);
  sock = g_create_unix_socket(socket_filename);
	if (sock == 1)
	{
	  log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[rdpdr_init]: "
	  		"Unable create printer socket");
	  return 1;
	}
	/* printer launch */
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_init]: "
			"Try to launch the rdpdr device application 'printer' ");
	pid = g_fork();
	if(pid < 0)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_init]: "
				"Error while launching the channel application rdpdr_printer");
		return 1;
	}
	if( pid == 0)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_init]: "
				"Launching the channel application rdpdr_printer ");
		sprintf(printer_program_path, "%s/%s",XRDP_SBIN_PATH, "rdpdr_printer");
		g_execlp3(printer_program_path, "rdpdr_printer", username);
		log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_init]: "
				"The channel application rdpdr_printer ended");
		g_exit(0);
	}
	/* wait device */
	log_message(l_config, LOG_LEVEL_DEBUG, "vchannel_rdpdr[rdpdr_init]: "
			"Waiting printer program");
	printer_sock = g_wait_connection(sock);
	if(printer_sock < 0 )
	{
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_[rdpdr_init]: "
				"Error while waiting xrdp_printer");
		return 1;
	}
	if( rdpdr_process_channel_opening(printer_sock, "printer") == 1)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_[rdpdr_init]: "
						"Error while waiting channel to xrdp_printer");
		return 1;
	}
	return 0;
}

/*****************************************************************************/
int
rdpdr_init()
{
  char filename[256];
  char log_filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;
  int display_num;
  int res;

  display_num = g_get_display_num_from_display(g_strdup(g_getenv("DISPLAY")));
	if(display_num == 0)
	{
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
  g_snprintf(filename, 255, "%s/rdpdr.conf", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, RDPDR_CFG_GLOBAL, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, RDPDR_CFG_NAME))
      {
        if( g_strlen(value) > 1)
        {
        	l_config->program_name = (char*)g_strdup(value);
        }
      }
    }
  }
  if (file_by_name_read_section(filename, RDPDR_CFG_LOGGING, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_DIR))
      {
      	l_config->log_file = (char*)g_strdup(value);
      }
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_LEVEL))
      {
      	l_config->log_level = log_text2level(value);
      }
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_ENABLE_SYSLOG))
      {
      	l_config->enable_syslog = log_text2bool(value);
      }
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_SYSLOG_LEVEL))
      {
      	l_config->syslog_level = log_text2level(value);
      }
    }
  }
  if( g_strlen(l_config->log_file) > 1 && g_strlen(l_config->program_name) > 1)
  {
  	g_sprintf(log_filename, "%s/%i/%s.log",
  			l_config->log_file,	display_num, l_config->program_name);
  	l_config->log_file = (char*)g_strdup(log_filename);
  }
  list_delete(names);
  list_delete(values);
  res = log_start(l_config);

	if( res != LOG_STARTUP_OK)
	{
		g_printf("vchannel_rdpdr[rdpdr_init]: Unable to start log system [%i]\n", res);
		return res;
	}
  return LOG_STARTUP_OK;
}

/*****************************************************************************/
int main(int argc, char** argv, char** environ)
{
	struct stream* s = NULL;
	int rv;
	int length;
	int total_length;

	l_config = g_malloc(sizeof(struct log_config), 1);
	if (argc != 2)
	{
		g_printf("Usage : vchannel_rdpdr USERNAME\n");
		return 1;
	}
	if ( g_getuser_info(argv[1], 0, 0, 0, 0, 0) == 1)
	{
		g_printf("The username '%s' did not exist\n", argv[1]);
		return 1;
	}
	g_sprintf(username, argv[1]);
	if (rdpdr_init() != LOG_STARTUP_OK)
	{
		g_printf("vchannel_rdpdr[main]: Enable to init log system\n");
		g_free(l_config);
		return 1;
	}
	if (vchannel_init() == ERROR)
	{
		g_printf("vchannel_rdpdr[main]: Enable to init channel system\n");
		g_free(l_config);
		return 1;
	}
	rdpdr_sock = vchannel_open("rdpdr");
	if(rdpdr_sock == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[main]: "
				"Error while connecting to vchannel provider");
		return 1;
	}
	if (rdpdr_init_devices() == 1)
	{
		g_printf("vchannel_rdpdr[main]: Enable to init device programs\n");
		return 1;
	}

	rdpdr_send_init();
	while(1){
		make_stream(s);
		init_stream(s, 1600);
		printf("socket : %i\n", rdpdr_sock);
		rv = vchannel_receive(rdpdr_sock, s->data, &length, &total_length);
		if( rv == ERROR )
		{
			free_stream(s);
			pthread_exit((void*)1);
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
			rdpdr_process_message(s, length, total_length);
			break;
		}
		free_stream(s);
	}
	return 0;
}
