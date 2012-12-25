#ifndef __CHUNK_DISTRIBUTOR_H_
#define __CHUNK_DISTRIBUTOR_H_

#define CHUNKSIZE 5

#include <pthread.h>

typedef struct chunk_distributor {
	int next_element ;
	int last_element;
	pthread_mutex_t *mutex;
} chunk_distributor_t;

typedef struct chunk {
	int first_element;
	int last_element;
} chunk_t;

// chunk_distributor.c
void chunk_distributor_init (chunk_distributor_t *c, int nr_elements);
chunk_t * chunk_distributor_get_next_chunk(chunk_distributor_t *c);
void chunk_distributor_destroy(chunk_distributor_t *c);

#endif
