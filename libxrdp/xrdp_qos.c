/**
 * Copyright (C) 2013 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2013
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
#include "libxrdp.h"


struct xrdp_qos* DEFAULT_CC
xrdp_qos_create(struct xrdp_session* session)
{
  struct xrdp_qos* result = g_malloc(sizeof(struct xrdp_qos), true);

  if (!session)
  {
    printf("Failed to initialize qos object, [session: %x]\n", (unsigned int)session);
    return NULL;
  }

  if (result == NULL)
  {
    printf("Failed to initialize qos object, unable to allocate object\n");
    return NULL;
  }

  result->session = session;
  result->spooled_packet = list_create();
  result->spooled_packet->auto_free = true;

  return result;
}

void DEFAULT_CC
xrdp_qos_delete(struct xrdp_qos* qos)
{
  list_delete(qos->spooled_packet);
  g_free(qos);
}

static void APP_CC
xrdp_qos_stream_copy(struct spacket* packet, struct stream* src)
{
  int size = src->end - src->data;
  make_stream(packet->data);
  init_stream(packet->data, size);
  struct stream* dst = packet->data;

  g_memcpy(dst->data, src->data, (src->end-src->data));
  dst->size = size;

  dst->channel_hdr = dst->data + (src->channel_hdr - src->data);
  dst->end = dst->data + (src->end - src->data);
  dst->iso_hdr = dst->data + (src->iso_hdr - src->data);
  dst->mcs_hdr = dst->data + (src->mcs_hdr - src->data);
  dst->sec_hdr = dst->data + (src->sec_hdr - src->data);
  dst->next_packet = dst->data + (src->next_packet - src->data);
  dst->p = dst->data + (src->p - src->data);
  dst->rdp_hdr = dst->data + (src->rdp_hdr - src->data);
}


int APP_CC
xrdp_qos_spool(struct xrdp_qos* self, struct stream* s, int data_pdu_type, int packet_type)
{
  DEBUG(("    in xrdp_qos_spool, gota send %d bytes", (src->end - src->data)));
  struct spacket* p = g_malloc(sizeof(struct spacket), true);

  xrdp_qos_stream_copy(p, s);
  p->update_type = data_pdu_type;
  p->packet_type = packet_type;

  list_add_item(self->spooled_packet, (tbus)p);

  DEBUG(("    out xrdp_qos_spool, sent %d bytes ok", (src->end - src->data)));

  return 0;
}

