/*
* Small jpeg decoder library (Internal header)
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

#ifndef __TINYJPEG_INTERNAL_H_
#define __TINYJPEG_INTERNAL_H_

#define SANITY_CHECK 1

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define TINYJPEG_FLAGS_MJPEG_TABLE	(1<<1)

#define HUFFMAN_BITS_SIZE  256
#define HUFFMAN_HASH_NBITS 9
#define HUFFMAN_HASH_SIZE  (1UL<<HUFFMAN_HASH_NBITS)
#define HUFFMAN_HASH_MASK  (HUFFMAN_HASH_SIZE-1)

#define HUFFMAN_TABLES	   4
#define COMPONENTS	   3

#define cY  0
#define cCb 1
#define cCr 2

#define BLACK_Y 0
#define BLACK_U 127
#define BLACK_V 127

#if DEBUG
#define trace(fmt, args...) do { \
  fprintf(stderr, fmt, ## args); \
  fflush(stderr); \
} while(0)
#else
#define trace(fmt, args...) do { } while (0)
#endif
#define error(fmt, args...) do { \
  snprintf(error_string, sizeof(error_string), fmt, ## args); \
  return -1; \
} while(0)

#define be16_to_cpu(x) (((x)[0]<<8)|(x)[1])

enum std_markers {
  DQT  = 0xDB, /* Define Quantization Table */
  SOF  = 0xC0, /* Start of Frame (size information) */
  DHT  = 0xC4, /* Huffman Table */
  SOI  = 0xD8, /* Start of Image */
  SOS  = 0xDA, /* Start of Scan */
  RST  = 0xD0, /* Reset Marker d0 -> .. */
  RST7 = 0xD7, /* Reset Marker .. -> d7 */
  EOI  = 0xD9, /* End of Image */
  DRI  = 0xDD, /* Define Restart Interval */
  APP0 = 0xE0,
};

typedef struct huffman_table
{
  /* Fast look up table, using HUFFMAN_HASH_NBITS bits we can have directly the symbol,
    * if the symbol is <0, then we need to look into the tree table */
  short int lookup[HUFFMAN_HASH_SIZE];
  /* code size: give the number of bits of a symbol is encoded */
  unsigned char code_size[HUFFMAN_HASH_SIZE];
  /* some place to store value that is not encoded in the lookup table */
  uint16_t slowtable[16-HUFFMAN_HASH_NBITS][256];
}huffman_table_t;

typedef struct component
{
  unsigned int Hfactor;
  unsigned int Vfactor;
  struct huffman_table *AC_table;
  struct huffman_table *DC_table;
  short int previous_DC;	/* Previous DC coefficient */
#if SANITY_CHECK
  unsigned int cid;
#endif
} component_t;

typedef struct jdec_task
{
  int id;
  int mcus_posx, mcus_posy;
  const unsigned char *stream_begin, *stream_end;
  unsigned int stream_length;

  const unsigned char *stream;	/* Pointer to the current stream */
  unsigned int reservoir, nbits_in_reservoir;
} jdec_task_t;

typedef struct idct_data{
  short int DCT_Y[4][64];		/* DCT coef */
  short int DCT_C[2][64];		/* DCT coef */
} idct_data_t;

typedef struct yuv_data{
  uint8_t Y[64*4], Cr[64], Cb[64];
} yuv_data_t;

typedef struct jpeg_parse_context
{
  uint8_t *components[COMPONENTS];
  int width, height;	/* Size of the image */
  int mcus_in_width, mcus_in_height;
  int mcus_posx, mcus_posy;
  unsigned int flags;

  const unsigned char *stream_begin, *stream_end;
  unsigned int stream_length;

  const unsigned char *stream;	/* Pointer to the current stream */
  unsigned int reservoir, nbits_in_reservoir;

  struct component component_infos[COMPONENTS];
  float Q_tables[COMPONENTS][64];	/* quantization tables */
  int q[COMPONENTS];	/* quantization tables idx for each color component*/
  struct huffman_table HTDC[HUFFMAN_TABLES];	/* DC huffman tables   */
  struct huffman_table HTAC[HUFFMAN_TABLES];	/* AC huffman tables   */
  int default_huffman_table_initialized;

  int restart_interval;
  int ntasks;
  int last_rst_marker_seen;		/* Rst marker is incremented each time */
} jpeg_parse_context_t;

typedef struct huffman_context {
  struct component component_infos[COMPONENTS];
  struct huffman_table HTDC[HUFFMAN_TABLES];	/* DC huffman tables   */
  struct huffman_table HTAC[HUFFMAN_TABLES];	/* AC huffman tables   */
  int default_huffman_table_initialized;
  int restart_interval;
  int ntasks;
  int mcus_in_width;
  int mcus_in_height;
  struct jtask_queue *jtask_q;
  struct idct_queue *idct_q;
} huffman_context_t;

typedef struct idct_context {
  float Q_tables[COMPONENTS][64];	/* possible quantization tables */
  int q[COMPONENTS];	/* quantization tables idx for each color component*/
  int width, height;
  int restart_interval;
  int ntasks;

  struct idct_queue *idct_q;
  struct cc_queue *cc_q;
} idct_context_t;

typedef struct cc_context {
  int width, height;
  int restart_interval;
  unsigned char* base;
  unsigned char* rgb_data;
  int mcus_in_width;
  int mcus_in_height;
  int ntasks;

  struct cc_queue *cc_q;
  struct write_list *write_l;
} cc_context;

typedef struct jpeg_decode_context {
  int width, height;
  int mcus_in_width, mcus_in_height;
  int restart_interval;
  int ntasks;

  struct huffman_context *hc;
  struct idct_context *ic;
  struct cc_context *cc;

  struct jdec_task hdata; //huffman task
  struct idct_data idata;
  struct yuv_data yuvdata;
} jpeg_decode_context_t;

typedef struct write_context {
  FILE *F;
  int width, height;
  int restart_interval;
  unsigned char* rgb_data;
  unsigned char* base;
  struct write_list *write_l;
} write_context_t;


/** !!! Note !!!
* (Threading) data structures introduced in the the reference solution.
* Used for parallelizing both with markers and without markers. Can be
* used by students but not required.
*

typedef struct jtask_queue {
  struct jdec_task *queue;
  int head;
  int tail;
  int free;
  int nelem;
  int ntasks;
  int popped;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} jtask_queue_t;

typedef struct idct_task{
  int id;
  struct idct_data *mcus;
  int size;
  int mcus_posx, mcus_posy;
} idct_task_t;

typedef struct idct_queue {
  struct idct_task *queue;
  int head;
  int tail;
  int free;
  int nelem;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} idct_queue_t;

typedef struct cc_task{
  int id;
  struct yuv_data *mcus;
  int size;
  unsigned int mcus_posx, mcus_posy;
} cc_task_t;

typedef struct cc_queue {
    struct cc_task *queue;
    int head;
    int tail;
    int free;
    int nelem;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} cc_queue_t;

typedef struct write_list {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int *list;
  int next;
  int nelem;
  int count;
} write_list_t;

*/

#endif

