/* $Id: module-rdp-sink.c 2043 2007-11-09 18:25:40Z lennart $ */

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechavalier <david@ulteo.com>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include "sound_channel.h"
#define BUFSIZE 1024

extern int completion_count;

void sound_process(const void *data, size_t size) {
    void *state = NULL;
    int write_type = 0;
    int packet_id=0;

    /* If something is connected to our monitor source, we have to
     * pass valid data to it */
    void *p;
    int current_size=0;
    printf("module-rdp[pa_sink_process]: "
    		"Send wave Frame \n");

    if (completion_count > 20)
    {
//      while (length > 0) {
//      	pa_sink_render(s, length, &chunk);
//      	p = pa_memblock_acquire(chunk.memblock);
//      	pa_memblock_release(chunk.memblock);
//      	pa_memblock_unref(chunk.memblock);
//      	pa_assert(chunk.length <= length);
//      	length -= chunk.length;
//      }

    	return;
    }
    int stamp = 0;
    vchannel_sound_send_wave_info(stamp, size, data);
}

/*****************************************************************************/
void *thread_sound_process (void * arg)
{
  int error;
	static const pa_sample_spec ss = {
      .format = PA_SAMPLE_S16LE,
      .rate = 44100,
      .channels = 2
  };
  pa_simple *s = NULL;

  /* Create the recording stream */
  if (!(s = pa_simple_new(NULL, "vchannel_rdpsnd", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
      fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
      goto finish;
  }
  printf("begining the main loop\n");
  for (;;) {
      uint8_t buf[BUFSIZE];

      /* Record some data ... */
      if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
          fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
          goto finish;
      }

      /* And write it to STDOUT */
      sound_process(buf, sizeof(buf));
  }
  finish:
      if (s)
          pa_simple_free(s);

	pthread_exit (0);
}



int main(int argc, char*argv[]) {
    /* The sample type to use */
		pthread_t vchannel_thread;
		pthread_t sound_thread;
    int ret = 1;

    init_channel();
    printf("module-rdp[pa_init]: "
    		"Create virtual channel thread\n");
  	if (pthread_create (&sound_thread, NULL, thread_sound_process, (void*)0) < 0)
  	{
  		printf( "rdpdr_printer[main]: "
  				"Pthread_create error for thread : spool_thread");
  		return 1;
  	}

    if (pthread_create (&vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
  	{
  		printf("rdpdr_printer[main]: "
  				"Pthread_create error for thread : vchannel_thread\n");
  		return 1;
  	}

  	(void)pthread_join (vchannel_thread, &ret);
  	(void)pthread_join (sound_thread, &ret);


  	return 0;
}
