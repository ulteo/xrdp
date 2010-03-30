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

#ifndef DISK_DEV_H_
#define DISK_DEV_H_

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <vchannel.h>
#include <os_calls.h>
#include <xrdp_constants.h>
#include <file.h>
#include "log.h"
#include "arch.h"
#include "os_calls.h"
#include "parse.h"

struct disk_device
{
	int device_id;
	char dir_name[256];
};

int DEFAULT_CC
fuse_process();
int DEFAULT_CC
disk_dev_add(struct stream* s, int device_data_length, int device_id, char* dos_name);






//int DEFAULT_CC
//printer_dev_get_next_job();
//int APP_CC
//in_unistr(struct stream* s, char *string, int str_size, int in_len);
//
//
//#define RDPDR_PRINTER_ANNOUNCE_FLAG_ASCII							0x00000001
//#define RDPDR_PRINTER_ANNOUNCE_FLAG_DEFAULTPRINTER		0x00000002
//#define RDPDR_PRINTER_ANNOUNCE_FLAG_NETWORKPRINTER		0x00000004
//#define RDPDR_PRINTER_ANNOUNCE_FLAG_TSPRINTER					0x00000008
//#define RDPDR_PRINTER_ANNOUNCE_FLAG_XPSFORMAT					0x00000010
//
//#define EVENT_SIZE  (sizeof (struct inotify_event))
//#define BUF_LEN     (1024 * (EVENT_SIZE + 16))
//
//
//struct printer_device
//{
//	int device_id;
//	char printer_name[256];
//	int watch;
//};
//
//
//int DEFAULT_CC
//printer_dev_delete_job(char* jobs);
//int APP_CC
//printer_dev_del(int device_id);
//int APP_CC
//printer_dev_add(struct stream* s, int device_data_length,
//								int device_id, char* dos_name);
//int APP_CC
//printer_dev_init_printer_socket( char* printer_name);
//int APP_CC
//printer_dev_get_printer_socket();

#endif /* PRINTER_DEV_H_ */



