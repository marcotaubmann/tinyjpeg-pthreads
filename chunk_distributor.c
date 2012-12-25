#include "chunk_distributor.h"
#include <stdlib.h>


void chunk_distributor_init (chunk_distributor_t *c, int nr_elements) {
	c->next_element = 0;
	c->last_element = nr_elements - 1;

	c->mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(c->mutex, NULL);
}

int min(int a, int b) {
	return (a<b)?a:b;
}

/*
	return NULL when there are no more chunks
*/
chunk_t * chunk_distributor_get_next_chunk(chunk_distributor_t *c){

	chunk_t *chunk;

	pthread_mutex_lock(c->mutex);

	if (c->next_element > c->last_element ){
		chunk =  (chunk_t *) NULL;
	} else {
		chunk = (chunk_t *) malloc(sizeof(chunk_t));
		chunk->first_element = c->next_element;
		chunk->last_element = min(c->last_element, c->next_element + CHUNKSIZE - 1);

		c->next_element = chunk->last_element + 1;
	}

	pthread_mutex_unlock(c->mutex);

	return chunk;
}

void chunk_distributor_destroy(chunk_distributor_t *c){
	pthread_mutex_destroy(c->mutex);
	free(c);
}
