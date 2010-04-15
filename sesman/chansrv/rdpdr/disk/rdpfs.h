/**
 * Copyright (C) 2008 Ulteo SAS
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

#ifndef RDPFS_H_
#define RDPFS_H_

#include "rdpdr.h"
#include "log.h"
#include "vchannel.h"

struct volume_info
{
	int creation_time_low;
	int creation_time_high;
	int serial;
	char label[256];
	char fs_type[256];
	long f_bfree;
	long f_bsize;
	long f_blocks;
	int f_namelen;
	int f_namemax;
};

struct fs_info
{
	int create_access_time;
	int last_access_time;
	int last_write_time;
	int last_change_time;
	int file_attributes;
	long file_size;
	long allocation_size;    /* number of occuped cluster */
	int nlink;
	int detele_request;
	int is_dir;
	char filename[256];
};


struct request_response
{
	int Request_type;
	int Request_param;
	struct volume_info volume_inf;
	struct fs_info fs_inf;
	unsigned char buffer[1024];
	int request_status;
} ;




int APP_CC
rdpfs_send(struct stream* s);
void APP_CC
rdpfs_query_volume_information(int completion_id, int device_id, int information, const char* query );
void APP_CC
rdpfs_query_information(int completion_id, int device_id, int information, const char* query );
int APP_CC
rdpfs_create(int device_id, int desired_access, int shared_access, int creation_disposition, int flags, const char* path);
void APP_CC
rdpfs_query_volume_information(int completion_id, int device_id, int information, const char* query );
void APP_CC
rdpfs_request_close(int completion_id, int device_id);


#endif /* RDPFS_H_ */
