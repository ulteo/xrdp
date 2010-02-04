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

#include "user_channel.h"


/*****************************************************************************/
int
user_channel_connect(const char* chan_name)
{
	int sock;
	int len;
	struct sockaddr_un saun;
	char socket_filename[256];

	/* Create socket */
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("seamlessrdpshell[user_channel_connect] : Error creating socket: socket");
		return ERROR;
	}
	sprintf(socket_filename, "%s%s", USER_CHANNEL_SOCKET_PATH, (char*)getenv("DISPLAY"));
	/* Connect to server */
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, socket_filename);
	len = sizeof(saun.sun_family) + strlen(saun.sun_path);
	if (connect(sock, (struct sockaddr *) &saun, len) < 0)
	{
		/*perror("Error connecting to socket: connect");*/
		close(sock);
		return ERROR;
	}
	printf("seamlessrdpshell[user_channel_connect] : channel find\n");
	user_channel_send(sock, (char*)chan_name, strlen(chan_name));
	return sock;
}


/*****************************************************************************/
int
user_channel_send(int sock, char* data, int size)
{
	Message *msg = malloc(sizeof(Message));
	init_message(msg,size+4);
	out32(msg, size);
	out8(msg, data, size);
	if(write(sock, msg->data, msg->size) < msg->size)
	{
		perror("seamlessrdpshell[user_channel_connect] : "
				"error while writing data\n");
		return ERROR;
	}
	return msg->size;
}

/*****************************************************************************/
int
user_channel_receive(int sock, char* data)
{
	Message *msg = malloc(sizeof(Message));
	init_message(msg,1024);
	int nb_read = 0;
	int size;

	nb_read = read(sock, msg->data, 4);
	if( nb_read != 4)
	{
		perror("seamlessrdpshell[user_channel_connect] : Error while receiving data\n");
		free_message(msg);
		return ERROR;
	}
	in32(msg, size);
	printf("size of message : %i\n", size);
	nb_read = read( sock, data, size);
	if(nb_read != size){
		printf("seamlessrdpshell[user_channel_connect] : wrong format message\n");
		free_message(msg);
		return ERROR;
	}
	/*printf("%i octets read\n",nb_read);*/
	free_message(msg);
	return nb_read+12;
}

/*****************************************************************************/
int
user_channel_close(int sock)
{
	return close(sock);
}
