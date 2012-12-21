/*
 * Small jpeg decoder library (header file)
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

#ifndef __JPEGDEC_H__
#define __JPEGDEC_H__

#include "tinyjpeg-internal.h"

//tinyjpeg-parse.c
struct jpeg_parse_context *create_jpeg_parse_context();
void destroy_jpeg_parse_context(struct jpeg_parse_context *jpc);
void create_jdec_task(struct jpeg_parse_context *jpc, struct jdec_task *task, int tasknum);
int tinyjpeg_parse_context_header(struct jpeg_parse_context *jpc, const unsigned char *buf, unsigned int size);

//conv_yuvbgr.c
void convert_yuv_bgr(struct cc_context *cc, struct yuv_data *yuv);
struct cc_context *create_cc_context(struct jpeg_parse_context *jpc, uint8_t *rgb_data);
void destroy_cc_context(struct cc_context *cc);

//jidctflt.c
void idct_mcu(struct idct_context *ic, struct idct_data *idata, struct yuv_data *yuvdata);
struct idct_context *create_idct_context(struct jpeg_parse_context *jpc);
void destroy_idct_context(struct idct_context *ic);

//huffman.c
int process_huffman_mcu(struct huffman_context *hc, struct jdec_task *hdata, struct idct_data *idata);
struct huffman_context *create_huffman_context(struct jpeg_parse_context *jpc);
void destroy_huffman_context(struct huffman_context *hc);

//tinyjpeg.c
const char *tinyjpeg_get_errorstring();
void decode_jpeg_task(struct jpeg_decode_context *jdc, struct jdec_task *jtask);
struct jpeg_decode_context *create_jpeg_decode_context(struct jpeg_parse_context *jpc, uint8_t *rgb_data);
void destroy_jpeg_decode_context(struct jpeg_decode_context* jdc);


//thread_queues.c
/**
 * Prototypes of multi-reader multi-writer thread-safe jtask_queue 
 * functions in the reference implementation.

void push_jtask(struct jtask_queue *jtask_q, struct jdec_task *task);
int pop_jtask(struct jtask_queue *jtask_q, struct jdec_task *task);
void init_jtask_queue(struct jtask_queue *jtask_q, int nelem, int ntasks);
void destroy_jtask_queue(struct jtask_queue *jtask_q);

*/

#endif
