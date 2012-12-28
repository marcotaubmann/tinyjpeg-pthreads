/*
* Small jpeg decoder library
*
* Copyright (c) 2006, Luc Saillard <luc@saillard.org>
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* - Redistributions of source code must retain the above copyright notice,
*  this list of conditions and the following disclaimer.
*
* - Redistributions in binary form must reproduce the above copyright notice,
*  this list of conditions and the following disclaimer in the documentation
*  and/or other materials provided with the distribution.
*
* - Neither the name of the author nor the names of its contributors may be
*  used to endorse or promote products derived from this software without
*  specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "tinyjpeg.h"
#include "tinyjpeg-internal.h"

enum element_type {DATA, FINISH};

// --------------------------------------------------------
// struct definitions
// --------------------------------------------------------

struct idct_data_buffer_element {
	enum element_type type;
	struct idct_data idata;
	struct idct_data_buffer_element *next;
};

struct idct_data_buffer {
	int elements;
	struct idct_data_buffer_element *first;
	struct idct_data_buffer_element *last;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
};

// --------------------------------------------------------
struct yuv_data_buffer_element {
	enum element_type type;
	struct yuv_data yuvdata;
	struct yuv_data_buffer_element *next;
};

struct yuv_data_buffer {
	int elements;
	struct yuv_data_buffer_element *first;
	struct yuv_data_buffer_element *last;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
};

// --------------------------------------------------------
struct huffman_thread_parameters {
	int id;
	struct jpeg_decode_context *jdc;
	int mcus_posy;
	int mcus_posx;
	int mcus_in_height;
  	struct huffman_context *hc;
	struct jdec_task *jtask;
	struct idct_data_buffer *ibuffer;
};

struct idct_thread_parameters {
	int id;
	struct idct_context *ic;
	struct idct_data_buffer *ibuffer;
	struct yuv_data_buffer *yuvbuffer;
};

struct convert_thread_parameters {
	int id;
	struct cc_context *cc;
	int bytes_per_mcu;
	int mcus_posx;
	int mcus_posy;
	struct jpeg_decode_context *jdc;
	int bytes_per_blocklines;
	struct yuv_data_buffer *yuvbuffer;
};
// --------------------------------------------------------
// Function definitions
// --------------------------------------------------------

static void exitmessage(const char *message)
{
  printf("%s\n", message);
  exit(0);
}
// --------------------------------------------------------
void idct_data_buffer_init(struct idct_data_buffer *b){
	b->elements = 0;
	b->first = NULL;
	b->last = NULL;

	b->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (b->mutex == NULL)
		exitmessage("Not enough memory to alloc new mutex for idct buffer\n");
	pthread_mutex_init(b->mutex, NULL);

	b->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (b->cond == NULL)
		exitmessage("Not enough memory to alloc new cond for idct buffer\n");
	pthread_cond_init(b->cond, NULL);
}

void idct_data_buffer_destroy(struct idct_data_buffer *b){
	pthread_mutex_destroy(b->mutex);
	pthread_cond_destroy(b->cond);
	free(b);
}

void idct_data_buffer_add(
	struct idct_data_buffer *b,
	struct idct_data_buffer_element *e )
{
	e->next = NULL;
	pthread_mutex_lock(b->mutex);
	
	if(b->elements == 0){
		b->first = e;
		b->last = e;
	} else {
		b->last->next = e;
		b->last = e;
	}
	b->elements++;

	pthread_cond_signal(b->cond);
	pthread_mutex_unlock(b->mutex);
}

struct idct_data_buffer_element* idct_data_buffer_extract(struct idct_data_buffer *b){
	struct idct_data_buffer_element *e;

	pthread_mutex_lock(b->mutex);
	while (b->elements == 0) {
		pthread_cond_wait(b->cond, b->mutex);
	}

	e = b->first;
	if(b->elements == 1) {
		b->first = NULL;
		b->last = NULL;
	} else {
		b->first = b->first->next;
	}
	e->next = NULL;
	b->elements--;

	pthread_mutex_unlock(b->mutex);

	return e;
}

// --------------------------------------------------------
void yuv_data_buffer_init(struct yuv_data_buffer *b) {
	b->elements = 0;
	b->first = NULL;
	b->last = NULL;

	b->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (b->mutex == NULL)
		exitmessage("Not enough memory to alloc new mutex for yuv buffer\n");
	pthread_mutex_init(b->mutex, NULL);

	b->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (b->cond == NULL)
		exitmessage("Not enough memory to alloc new cond for yuv buffer\n");
	pthread_cond_init(b->cond, NULL);
}

void yuv_data_buffer_destroy(struct yuv_data_buffer *b){
	pthread_mutex_destroy(b->mutex);
	pthread_cond_destroy(b->cond);
	free(b);
}

void yuv_data_buffer_add(
	struct yuv_data_buffer *b,
	struct yuv_data_buffer_element *e )
{
	e->next = NULL;
	pthread_mutex_lock(b->mutex);
	
	if(b->elements == 0){
		b->first = e;
		b->last = e;
	} else {
		b->last->next = e;
		b->last = e;
	}
	b->elements++;

	pthread_cond_signal(b->cond);
	pthread_mutex_unlock(b->mutex);
}

struct yuv_data_buffer_element* yuv_data_buffer_extract(struct yuv_data_buffer *b){
	struct yuv_data_buffer_element *e;

	pthread_mutex_lock(b->mutex);
	while (b->elements == 0) {
		pthread_cond_wait(b->cond, b->mutex);
	}

	e = b->first;
	if(b->elements == 1) {
		b->first = NULL;
		b->last = NULL;
	} else {
		b->first = b->first->next;
	}
	b->elements--;

	pthread_mutex_unlock(b->mutex);

	e->next = NULL;
	return e;
}

// --------------------------------------------------------
void *huffman_thread (void *arg) {
	struct huffman_thread_parameters *p = (struct huffman_thread_parameters*)arg;
	int j;
	struct idct_data_buffer_element *e;

	for (j=0; j<p->jdc->restart_interval && p->mcus_posy< p->mcus_in_height; j++) {
	  e = (struct idct_data_buffer_element *)malloc(sizeof(struct idct_data_buffer_element));
	  if (e == NULL)
    		exitmessage("Not enough memory to alloc new idata buffer element\n");
	  e->type = DATA;
          process_huffman_mcu(p->hc, p->jtask, &(e->idata));
	  idct_data_buffer_add(p->ibuffer, e);
      
          p->mcus_posx++;
          if (p->mcus_posx >= p->jdc->mcus_in_width){
            p->mcus_posy++;
            p->mcus_posx = 0;
          }
        }

	e = (struct idct_data_buffer_element *)malloc(sizeof(struct idct_data_buffer_element));
	if (e == NULL)
    		exitmessage("Not enough memory to alloc new idata buffer element\n");
	e->type = FINISH;
	idct_data_buffer_add(p->ibuffer, e);
	
	return NULL;
}

// --------------------------------------------------------
void *idct_thread(void *arg){
	struct idct_thread_parameters *p = (struct idct_thread_parameters*)arg;
	struct idct_data_buffer_element *idct_element;
	struct yuv_data_buffer_element *yuv_element;
	int extract_next;

	do {
		idct_element = idct_data_buffer_extract(p->ibuffer);
		if (idct_element == NULL)
			exitmessage("idct_data_buffer returned empty element\n");
		
		yuv_element = (struct yuv_data_buffer_element *)malloc(sizeof(struct yuv_data_buffer_element));
		if (yuv_element == NULL)
			exitmessage("Not enough memory to alloc new yuvdata buffer element\n");
		yuv_element->type = idct_element->type;
		if (idct_element->type == DATA){
			idct_mcu(p->ic, &(idct_element->idata), &(yuv_element->yuvdata));
			extract_next = 1;
		} else {
			extract_next = 0;
		}
		yuv_data_buffer_add(p->yuvbuffer, yuv_element);

		free(idct_element);
	} while (extract_next);

	return NULL;
}

// --------------------------------------------------------
void *convert_thread(void*arg){
	struct convert_thread_parameters *p = (struct convert_thread_parameters*)arg;
	struct yuv_data_buffer_element *yuv_element;
	int extract_next;

	do {
		yuv_element = yuv_data_buffer_extract(p->yuvbuffer);
		if (yuv_element == NULL)
			exitmessage("yuv_data_buffer returned empty element\n");

		if(yuv_element->type == DATA) {
			convert_yuv_bgr(p->cc, &(yuv_element->yuvdata));

			p->cc->rgb_data += p->bytes_per_mcu;
			p->mcus_posx++;
			if (p->mcus_posx >= p->jdc->mcus_in_width){
				p->mcus_posy++;
				p->mcus_posx = 0;
				p->cc->rgb_data += (p->bytes_per_blocklines - p->jdc->width*3);
			}
			extract_next = 1;
		} else {
			extract_next = 0;
		}
		free(yuv_element);
	} while (extract_next);

	return NULL;
}
// --------------------------------------------------------
void decode_jpeg_task_pipeline(struct jpeg_decode_context *jdc, struct jdec_task *jtask){
  struct huffman_context *hc = jdc->hc;
  struct idct_context *ic = jdc->ic;
  struct cc_context *cc = jdc->cc;

  int i;
  int mcus_posx=0;
  int mcus_posy=0;
  unsigned int bytes_per_blocklines= jdc->width *3*16;
  unsigned int bytes_per_mcu = 3*16;

  struct idct_data_buffer *ibuffer;
  struct yuv_data_buffer *yuvbuffer;
  struct huffman_thread_parameters hp;
  struct idct_thread_parameters ip;
  struct convert_thread_parameters cp;
  pthread_t huf_thread;
  pthread_t idc_thread;
  pthread_t con_thread;

  for (i=0; i<COMPONENTS; i++)
    hc->component_infos[i].previous_DC = 0;

  cc->rgb_data = cc->base + jtask->mcus_posy * bytes_per_blocklines + jtask->mcus_posx * bytes_per_mcu;
  mcus_posx = jtask->mcus_posx;
  mcus_posy = jtask->mcus_posy;

  ibuffer = (struct idct_data_buffer *)malloc(sizeof(struct idct_data_buffer));
  if (ibuffer == NULL) exitmessage("Not enough memory to alloc new ibuffer\n");
  idct_data_buffer_init(ibuffer);

  hp.id = 0;
  hp.jdc = jdc;
  hp.mcus_posx = mcus_posx;
  hp.mcus_posy = mcus_posy;
  hp.mcus_in_height = cc->mcus_in_height;
  hp.hc = hc;
  hp.jtask = jtask;
  hp.ibuffer = ibuffer;
  pthread_create(&huf_thread, NULL, huffman_thread, &hp);

  yuvbuffer = (struct yuv_data_buffer *)malloc(sizeof(struct yuv_data_buffer));
  if (yuvbuffer == NULL) exitmessage("Not enough memory to alloc new yuvbuffer\n");
  yuv_data_buffer_init(yuvbuffer);

  ip.id = 1;
  ip.ic = ic;
  ip.ibuffer = ibuffer;
  ip.yuvbuffer = yuvbuffer;
  pthread_create(&idc_thread, NULL, idct_thread, &ip);

  cp.id = 2;
  cp.cc = cc;
  cp.bytes_per_mcu = bytes_per_mcu;
  cp.mcus_posx = mcus_posx;
  cp.mcus_posy = mcus_posy;
  cp.jdc = jdc;
  cp.bytes_per_blocklines = bytes_per_blocklines;
  cp.yuvbuffer = yuvbuffer;
  pthread_create(&con_thread, NULL, convert_thread, &cp);

  pthread_join(huf_thread, NULL);
  pthread_join(idc_thread, NULL);
  pthread_join(con_thread, NULL);

}

// --------------------------------------------------------
// original stuff
// --------------------------------------------------------

/* Global variable to return the last error found while deconding */
char error_string[256];

const char *tinyjpeg_get_errorstring() {
  return error_string;
}

void decode_jpeg_task(struct jpeg_decode_context *jdc, struct jdec_task *jtask){
  struct huffman_context *hc = jdc->hc;
  struct idct_context *ic = jdc->ic;
  struct cc_context *cc = jdc->cc;

  struct idct_data *idata = &jdc->idata;
  struct yuv_data *yuvdata = &jdc->yuvdata;

  int i, j;
  int mcus_posx=0;
  int mcus_posy=0;
  unsigned int bytes_per_blocklines= jdc->width *3*16;
  unsigned int bytes_per_mcu = 3*16;

  for (i=0; i<COMPONENTS; i++)
    hc->component_infos[i].previous_DC = 0;

  cc->rgb_data = cc->base + jtask->mcus_posy * bytes_per_blocklines + jtask->mcus_posx * bytes_per_mcu;
  mcus_posx = jtask->mcus_posx;
  mcus_posy = jtask->mcus_posy;

  for (j=0; j<jdc->restart_interval && mcus_posy< cc->mcus_in_height; j++) {
    process_huffman_mcu(hc, jtask, idata);
    idct_mcu(ic, idata, yuvdata);
    convert_yuv_bgr(cc, yuvdata);

    cc->rgb_data += bytes_per_mcu;
    mcus_posx++;
    if (mcus_posx >= jdc->mcus_in_width){
      mcus_posy++;
      mcus_posx = 0;
      cc->rgb_data += (bytes_per_blocklines - jdc->width*3);
    }
  }

}

struct jpeg_decode_context *create_jpeg_decode_context(struct jpeg_parse_context *jpc, uint8_t *rgb_data){
  struct jpeg_decode_context *jdc = malloc(sizeof(struct jpeg_decode_context));
  jdc->width = jpc->width;
  jdc->height = jpc->height;
  jdc->restart_interval = jpc->restart_interval;
  jdc->mcus_in_width = jpc->mcus_in_width;
  jdc->mcus_in_height = jpc->mcus_in_height;

  jdc->hc = create_huffman_context(jpc);
  jdc->ic = create_idct_context(jpc);
  jdc->cc = create_cc_context(jpc, rgb_data);

  return jdc;
}

void destroy_jpeg_decode_context(struct jpeg_decode_context* jdc){
  destroy_huffman_context(jdc->hc);
  destroy_idct_context(jdc->ic);
  destroy_cc_context(jdc->cc);
  free(jdc);
}
