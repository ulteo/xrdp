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

#ifndef USER_CHANNEL_H
#define USER_CHANNEL_H

#include "arch.h"
#include "parse.h"


struct user_channel
{
	int channel_id;
	char channel_name[9];
	int channel_socket;
};

int APP_CC
user_channel_do_up();

int APP_CC
user_channel_init(char* channel_name, int channel_id);
int APP_CC
user_channel_deinit(void);
int APP_CC
user_channel_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length);
int APP_CC
user_channel_get_wait_objs(tbus* objs, int* count, int* timeout);
int APP_CC
user_channel_check_wait_objs(void);

#endif
