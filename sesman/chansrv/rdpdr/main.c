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


static pthread_mutex_t mutex;
Vchannel rdpdr_chan;
struct log_config* l_config;


/*****************************************************************************/
void process_message(struct stream* s)
{
	return;
}

/*****************************************************************************/
void *thread_spool_process (void * arg)
{
	while (1) {
		pthread_mutex_lock(&mutex);

		pthread_mutex_unlock(&mutex);
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_spool_process]: "
			"Finished spool process");
	pthread_exit (0);
}

/*****************************************************************************/
void *thread_vchannel_process (void * arg)
{
	char* buffer = malloc(1024);
	struct stream* s = NULL;
	int rv;

	while(1){
		make_stream(s);
		rv = vchannel_receive(&rdpdr_chan, s);
		if( rv == ERROR )
		{
			continue;
		}
		switch(rv)
		{
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "XHook[thread_vchannel_process]: "
					"Invalid message");
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_vchannel_process]: "
					"Status connected");
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "XHook[thread_vchannel_process]: "
					"Status disconnected");
			break;
		default:
			process_message(s);
			break;
		}
		free_stream(s);
	}
	pthread_exit (0);
}

/*****************************************************************************/
int main(int argc, char** argv, char** environ)
{
	pthread_t spool_thread;
	pthread_t vchannel_thread;
	void *ret;
	l_config = g_malloc(sizeof(struct log_config), 1);
	vchannel_read_logging_conf(l_config, "vchannel_rdpdr");
	log_start(l_config);
	rdpdr_chan.log_conf = l_config;
	g_strncpy(rdpdr_chan.name, "rdpdr", 9);

	pthread_mutex_init(&mutex, NULL);
	if(vchannel_open(&rdpdr_chan) == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[main]: "
				"Error while connecting to vchannel provider");
		return 1;
	}

	if (pthread_create (&spool_thread, NULL, thread_spool_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[main]: "
				"Pthread_create error for thread : spool_thread");
		return 1;
	}
	if (pthread_create (&vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "vchannel_rdpdr[main]: "
				"Pthread_create error for thread : vchannel_thread");
		return 1;
	}

	(void)pthread_join (spool_thread, &ret);
	(void)pthread_join (vchannel_thread, &ret);
	pthread_mutex_destroy(&mutex);

	return 0;
}
