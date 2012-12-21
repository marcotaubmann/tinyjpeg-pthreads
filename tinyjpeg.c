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
