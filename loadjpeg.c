/*
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <pthread.h>

#include "tinyjpeg.h"

#include "timeutil.h"
#include "chunk_distributor.h"

#define NTHREADS 8


struct decode_thread_parameters {
	int id;
	chunk_distributor_t *cd;
  	struct jdec_task *jtasks;
  	struct jpeg_decode_context **jdcs;
};

void *decode_thread(void *arg){
	int i;
	chunk_t *c;
	struct decode_thread_parameters *p = (struct decode_thread_parameters*)arg;

	while ((c = chunk_distributor_get_next_chunk(p->cd)) != NULL){
		//printf("thread=%d feteched chunk. first=%d, last=%d\n", p->id, c->first_element, c->last_element);
		for(i=c->first_element; i<=c->last_element; i++) {
			decode_jpeg_task((p->jdcs[i]), &(p->jtasks[i]));
		}
	}

	return NULL;
}

static void exitmessage(const char *message)
{
  printf("%s\n", message);
  exit(0);
}

static int filesize(FILE *fp)
{
  long pos;
  fseek(fp, 0, SEEK_END);
  pos = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  return pos;
}

static struct write_context *create_write_context(struct jpeg_parse_context *jpc, const char *outfilename, uint8_t *rgb_data){
  char temp[1024];

  struct write_context *wc = malloc(sizeof(struct write_context));

  wc->width = jpc->width;
  wc->height = jpc->height;
  wc->restart_interval = jpc->restart_interval;

  snprintf(temp, sizeof(temp), "%s", outfilename);
  wc->F = fopen(temp, "wb");
  wc->base = rgb_data;
  wc->rgb_data = rgb_data;

  return wc;
}

static void destroy_write_context(struct write_context *wc){
  fclose(wc->F);
  free(wc);
}

/**
* Save a buffer in 24bits Targa format
* (BGR byte order)
*/
void write_tga_header(struct write_context *wc){
  unsigned char targaheader[18];
  memset(targaheader,0,sizeof(targaheader));

  targaheader[12] = (unsigned char) (wc->width & 0xFF);
  targaheader[13] = (unsigned char) (wc->width >> 8);
  targaheader[14] = (unsigned char) (wc->height & 0xFF);
  targaheader[15] = (unsigned char) (wc->height >> 8);
  targaheader[17] = 0x20;    // Top-down, non-interlaced
  targaheader[2]  = 2;       // image type = uncompressed RGB
  targaheader[16] = 24;

  fwrite(targaheader, sizeof(targaheader), 1, wc->F);
}

void write_next_mcu_line(struct write_context *wc){
  int bytes_per_mcu_line = wc->width*3*16;

  fwrite(wc->rgb_data, 1, bytes_per_mcu_line, wc->F);
  wc->rgb_data += bytes_per_mcu_line;
}

/**
* Load one jpeg image, and decompress it, and save the result.
*/
int convert_one_image(const char *infilename, const char *outfilename)
{
  FILE *fp;
  struct jpeg_parse_context *jpc;
  struct jpeg_decode_context **jdcs;
  struct write_context* wc;

  struct jdec_task *jtasks;

  unsigned int length_of_file;
  int width, height;
  int mcus_in_width, mcus_in_height;
  unsigned char *buf;
  uint8_t *rgb_data;
  int i;
  int ntasks;
  int read;

  int nthreads = NTHREADS;
  int thread_ids[nthreads];
  pthread_t threads[nthreads];
  struct decode_thread_parameters dec_thr_pars[nthreads];
  chunk_distributor_t *cd;

  /* Load the Jpeg into memory */
  fp = fopen(infilename, "rb");
  if (fp == NULL)
    exitmessage("Cannot open filename\n");
  length_of_file = filesize(fp);
  buf = (unsigned char *)malloc(length_of_file + 4);
  if (buf == NULL)
    exitmessage("Not enough memory for loading file\n");
  read = fread(buf, length_of_file, 1, fp);
  fclose(fp);
  if (read == 0)
    exitmessage("File is empty\n");

  /* Decompress it */
  jpc = create_jpeg_parse_context();
  if (jpc == NULL)
    exitmessage("Not enough memory to alloc the structure need for decompressing\n");

  if (tinyjpeg_parse_context_header(jpc, buf, length_of_file)<0)
    exitmessage(tinyjpeg_get_errorstring());

  width = jpc->width;
  height = jpc->height;
  mcus_in_width = jpc->mcus_in_width;
  mcus_in_height = jpc->mcus_in_height;

  // RGB stuff
  rgb_data = (uint8_t *) malloc(width * height * 3);

  if (jpc->restart_interval){
    ntasks = (mcus_in_width * mcus_in_height / jpc->restart_interval)
        + !!((mcus_in_width * mcus_in_height) % jpc->restart_interval);
  }else{
    ntasks = 1;
    jpc->restart_interval = mcus_in_height * mcus_in_width;
  }
  wc = create_write_context(jpc, outfilename, rgb_data);

  printf("Decoding JPEG image...\n");

  jtasks = (struct jdec_task *)malloc(ntasks*sizeof(struct jdec_task));
  if (jtasks == NULL)
    exitmessage("Not enough memory  to alloc the structure need for create tasks\n");

  jdcs = (struct jpeg_decode_context **)malloc(ntasks*sizeof(struct jpeg_decode_context *));
  if (jdcs == NULL)
	  exitmessage("Not enough memory to alloc the structure need for all parallel jdcs\n");

  //create own task variable and own jdc for each task
  for(i=0; i<ntasks; i++) {
    create_jdec_task(jpc, &(jtasks[i]), i);
    jdcs[i] = create_jpeg_decode_context(jpc, rgb_data);
  }

  if (ntasks > 1) { // parallel by indepentent tasks
	  cd = (chunk_distributor_t *)malloc(sizeof(chunk_distributor_t));
	  if (cd == NULL)
	    exitmessage("Not enough memory to alloc the chunk distributor\n");

	  chunk_distributor_init (cd, ntasks);

	  // divide the tasks equally among the threads, but last thread has to do more if there is a rest
	  for (i=0; i<nthreads; i++){
		  thread_ids[i] = i;
		  dec_thr_pars[i].id = thread_ids[i];
		  dec_thr_pars[i].cd = cd;
		  dec_thr_pars[i].jtasks = jtasks;
		  dec_thr_pars[i].jdcs = jdcs;
		  pthread_create(&threads[i], NULL, decode_thread, &dec_thr_pars[i]);
	  }

	  for(i=0; i<nthreads;i++){
		pthread_join(threads[i], NULL);
	  }

  	  free(cd);

  } else { // parallel by pipeline
	decode_jpeg_task_pipeline(jdcs[0], &(jtasks[0]));
  }

  //file write could already start before the complete image is decoded
  write_tga_header(wc);
  for (i=0; i<mcus_in_height; i++){
    write_next_mcu_line(wc);
  }

  free(buf);
  free(rgb_data);

  destroy_jpeg_parse_context(jpc);
  for(i=0; i<ntasks; i++) {
    destroy_jpeg_decode_context(jdcs[i]);
  }
  free(jdcs);
  destroy_write_context(wc);

  return 0;
}

/*
*   Usage information.
*/
static void usage(void)
{
  fprintf(stderr, "Usage: loadjpeg [option] input_filename.jpeg output_filename.tga \n");
  fprintf(stderr, "option:\n");
  fprintf(stderr, "  --benchmark - Measure the timing of 1 conversion\n");
  exit(1);
}

/**
* Benchmark MAIN
*/
int main(int argc, char *argv[])
{
  char *output_filename, *input_filename;
  timer start, finish;
  long elapsed;
  int current_argument;
  int benchmark_mode = 0;

  if (argc < 3)
    usage();

  current_argument = 1;
  while (1)
  {
    if (strcmp(argv[current_argument], "--benchmark")==0)
      benchmark_mode = 1;
    else
      break;
    current_argument++;
  }

  if (argc < current_argument+2)
    usage();

  input_filename = argv[current_argument];
  output_filename = argv[current_argument+1];

  if(benchmark_mode)
    TIME(start);

  convert_one_image(input_filename, output_filename);

  if(benchmark_mode) {
    TIME(finish);
    elapsed = timevaldiff(&start, &finish);
    printf("Decoding finished in %.2fs (%ld ms)\n", (double)elapsed/1000, elapsed);
  }

  return 0;
}
