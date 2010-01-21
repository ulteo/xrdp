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

#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "devredir.h"
#include "printer_dev.h"

extern int g_rdpdr_chan_id; /* in chansrv.c */
extern struct log_config log_conf;
static int g_devredir_up = 0;
static tbus g_x_wait_obj = 0;
static char hostname[256];
static int deviceId;
static int use_unicode;
static int vers_major;
static int vers_minor;
static int client_id;
static int supported_operation[6] = {0};
struct device device_list[128] = {0};
int device_count = 0;

/*****************************************************************************/
int APP_CC
dev_redir_send(struct stream* s){
  int rv;
  int size = (int)(s->end - s->data);

  rv = send_channel_data(g_rdpdr_chan_id, s->data, size);
  if (rv != 0)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel: enable to send message");
  }
  rv = log_message(&log_conf, LOG_LEVEL_DEBUG, "xrdp-devredir: send message: ");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG, s->data, size );

  return rv;
}




/* Converts a windows path to a unix path */
/*void
convert_to_unix_filename(char *filename)
{
	char *p;

	while ((p = strchr(filename, '\\')))
	{
		*p = '/';
	}
}
*/








/*****************************************************************************/
int APP_CC
dev_redir_init(void)
{
  struct stream* s;

  log_message(&log_conf, LOG_LEVEL_INFO, "init");
  g_devredir_up = 1;
  /*server announce*/
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_SERVER_ANNOUNCE);
  out_uint16_le(s, 0x1);
  out_uint16_le(s, 0x0c);
  out_uint32_le(s, 0x1);
  s_mark_end(s);
  dev_redir_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_deinit(void)
{
	int i;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "deinit");
  for(i=0 ; i<device_count ; i++)
  {
  	switch(device_list[i].device_type)
  	{
			case PRINTER_DEVICE:
				log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_deinit]:"
						" remove printer with the id %i", device_list[i].device_type);
				printer_dev_del(device_list[i].device_id);

			default:
				log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_deinit]:"
						" unknow device type: %i", device_list[i].device_type);
				break;
  	}
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
  int component;
  int packetId;
  int result;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: data received:");
  log_hexdump(&log_conf, LOG_LEVEL_DEBUG, s->p, length);
  in_uint16_le(s, component);
  in_uint16_le(s, packetId);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: component=0x%04x packetId=0x%04x", component, packetId);
  if ( component == RDPDR_CTYP_CORE )
  {
    switch (packetId)
    {
    case PAKID_CORE_CLIENTID_CONFIRM :
      return dev_redir_clientID_confirm(s);

    case PAKID_CORE_CLIENT_NAME :
      return dev_redir_client_name(s);

    case PAKID_CORE_CLIENT_CAPABILITY:
      return dev_redir_client_capability(s);

    case PAKID_CORE_DEVICELIST_ANNOUNCE:
      return dev_redir_devicelist_announce(s);
      break;

    default:
      log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_data_in]: unknown message");
      return 1;
    }
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;

  if ((!g_devredir_up) || (objs == 0) || (count == 0))
  {
    return 0;
  }
  lcount = *count;
  objs[lcount] = g_x_wait_obj;
  lcount++;
  *count = lcount;
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_check_wait_objs(void)
{
  if (!g_devredir_up)
  {
    return 0;
  }
  log_message(&log_conf, LOG_LEVEL_INFO, "rdpdr channel[dev_redir_check_wait_objs]: check wait object");
  return 0;
}


/*****************************************************************************/
int APP_CC
dev_redir_clientID_confirm(struct stream* s)
{
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: new message: PAKID_CORE_CLIENTID_CONFIRM");
    in_uint16_le(s, vers_major);
    in_uint32_le(s, vers_minor);
    in_uint32_le(s, client_id);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: version : %i:%i, client_id : %i", vers_major, vers_minor, client_id);
    return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_client_name(struct stream* s)
{
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: new message: PAKID_CORE_CLIENT_NAME");
  int hostname_size;
  in_uint32_le(s, use_unicode);
  in_uint32_le(s, hostname_size);   /* flag not use */
  in_uint32_le(s, hostname_size);
  if (hostname_size < 1)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_data_in]: no hostname specified");
    return 1;
  }
  if (use_unicode == 1)
  {
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: unicode is used");
    in_unistr(s, hostname, sizeof(hostname), hostname_size);
  }
  else
  {
    in_uint8a(s, hostname, hostname_size);
  }
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: hostname : '%s'",hostname);
  if (g_strlen(hostname) >0)
  {
    dev_redir_confirm_clientID_request();
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_client_capability(struct stream* s)
{
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: new message: PAKID_CORE_CLIENT_CAPABILITY");
  int capability_count, ignored, temp, general_capability_version, rdp_version, i;

  in_uint16_le(s, capability_count);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: capability number : %i", capability_count);
  if (capability_count == 0 )
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: No capability ");
    return 1;
  }
  in_uint16_le(s, ignored);
  /* GENERAL_CAPS_SET */
  in_uint16_le(s, temp);		/* capability type */
  if (temp != CAP_GENERAL_TYPE )
  {
    log_message(&log_conf, LOG_LEVEL_ERROR,
    		"rdpdr channel[dev_redir_client_capability]: malformed message (normaly  CAP_GENERAL_TYPE)");
    return 1;
  }
  in_uint16_le(s, ignored);		/* capability length */
  in_uint32_le(s, general_capability_version);		/* general capability version */
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
		  "general capability version = %i ",general_capability_version);
  if (general_capability_version != GENERAL_CAPABILITY_VERSION_01 &&
		  general_capability_version != GENERAL_CAPABILITY_VERSION_02  )
  {
    log_message(&log_conf, LOG_LEVEL_ERROR,
    		"rdpdr channel[dev_redir_client_capability]: malformed message "
    		"(normaly general_capability_version = [GENERAL_CAPABILITY_VERSION_01|GENERAL_CAPABILITY_VERSION_02])");
    return 1;
  }
  /* Capability message */
  in_uint32_le(s, ignored);		/* OS type */
  in_uint32_le(s, ignored);		/* OS version */
  in_uint16_le(s, ignored);		/* major version */
  in_uint16_le(s, rdp_version);	/* minor version */
  log_message(&log_conf, LOG_LEVEL_DEBUG,
      		"rdpdr channel[dev_redir_client_capability]: RDP version = %i ",rdp_version);
  if(rdp_version == RDP6)
  {
    log_message(&log_conf, LOG_LEVEL_WARNING, "rdpdr channel[dev_redir_client_capability]: "
    		"only RDP5 is supported");
  }
  in_uint16_le(s, temp);	/* oiCode1 */
  if (temp != RDPDR_IRP_MJ_ALL)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: "
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
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_DRIVE_TYPE supported by the client");
      supported_operation[temp]=1;
      break;
    case CAP_PORT_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_PORT_TYPE supported by the client but not by the server");
      supported_operation[temp]=1;
      break;
    case CAP_PRINTER_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_PRINTER_TYPE supported by the client");
      supported_operation[temp]=1;
      break;
    case CAP_SMARTCARD_TYPE:
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
    		  "CAP_SMARTCARD_TYPE supported by the client but not by the server");
      supported_operation[temp]=1;
      break;
    default:
      log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: "
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
dev_redir_devicelist_announce(struct stream* s)
{
  int device_count, device_id, device_type, device_data_length;
  int i, j;
  char dos_name[9] = {0};
  char* data;

  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_data_in]: new message: PAKID_CORE_DEVICELIST_ANNOUNCE");
  in_uint32_le(s, device_count);	/* DeviceCount */
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
		  "%i device(s) declared", device_count);
  /* device list */
  for( i=0 ; i<device_count ; i++)
  {
    in_uint32_le(s, device_type);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
  		  "device type: %i", device_type);
    in_uint32_le(s, device_id);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
  		  "device id: %i", device_id);

    in_uint8a(s,dos_name,8)
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
  		  "dos name: '%s'", dos_name);
    in_uint32_le(s, device_data_length);
    log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
  		  "data length: %i", device_data_length);

    switch(device_type)
    {
    case RDPDR_DTYP_PRINT :
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
      		  "Add printer device");
      if (printer_dev_add(s, device_data_length, device_id, dos_name) == 1)
      {
      	device_list[device_count].device_id = device_id;
      	device_list[device_count].device_type = PRINTER_DEVICE;
      	device_count++;
      	break;
      }
      log_message(&log_conf, LOG_LEVEL_ERROR, "rdpdr channel[dev_redir_client_capability]: "
      		  "Enable to add printer device");
      break;

    case RDPDR_DTYP_FILESYSTEM :
      log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_client_capability]: "
        	  "Add filesystem device");
      break;

    }
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_confirm_clientID_request()
{
  struct stream* s;
  log_message(&log_conf, LOG_LEVEL_DEBUG, "rdpdr channel[dev_redir_send_capability_request]: Send Server Client ID Confirm Request");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */

  s_mark_end(s);
  dev_redir_send(s);
  free_stream(s);
  return 0;
}





