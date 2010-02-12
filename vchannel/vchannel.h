/**
 * Copyright (C) 2009 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com>
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


#ifndef VCHANNEL_H_
#define VCHANNEL_H_

#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include "log.h"
#include "parse.h"
#include "list.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#define VCHANNEL_SOCKET_PATH	"/var/spool/xrdp/xrdp_user_channel_socket"
#define ERROR		-1
#define VCHANNEL_OPEN_RETRY_ATTEMPT		12

#define VCHAN_CFG_LOGGING						"Logging"
#define VCHAN_CFG_LOG_FILE					"LogDir"
#define VCHAN_CFG_LOG_LEVEL					"LogLevel"
#define VCHAN_CFG_LOG_ENABLE_SYSLOG	"EnableSyslog"
#define VCHAN_CFG_LOG_SYSLOG_LEVEL	"SyslogLevel"


typedef struct{
	struct log_config* log_conf;
	char name[9];
	int sock;
} Vchannel;

int APP_CC
vchannel_open(Vchannel *channel);
int APP_CC
vchannel_send(Vchannel *channel, struct stream *s);
int APP_CC
vchannel_receive(Vchannel *channel, struct stream *s);
int APP_CC
vchannel_close(Vchannel *channel);
int APP_CC
vchannel_read_logging_conf(struct log_config* log_conf, const char* chan_name);

#endif /* VCHANNEL_H_ */
