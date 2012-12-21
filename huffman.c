/*
* huffman.c
*
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tinyjpeg-internal.h"

static const unsigned char zigzag[64] =
{
  0,  1,  5,  6, 14, 15, 27, 28,
  2,  4,  7, 13, 16, 26, 29, 42,
  3,  8, 12, 17, 25, 30, 41, 43,
  9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63
};

/* Global variable to return the last error found while decoding */
extern char error_string[256];


/*
* 4 functions to manage the stream
*
*  fill_nbits: put at least nbits in the reservoir of bits.
*              But convert any 0xff,0x00 into 0xff
*  get_nbits: read nbits from the stream, and put it in result,
*             bits is removed from the stream and the reservoir is filled
*             automaticaly. The result is signed according to the number of
*             bits.
*  look_nbits: read nbits from the stream without marking as read.
*  skip_nbits: read nbits from the stream but do not return the result.
*
* stream: current pointer in the jpeg data (read bytes per bytes)
* nbits_in_reservoir: number of bits filled into the reservoir
* reservoir: register that contains bits information. Only nbits_in_reservoir
*            is valid.
*                          nbits_in_reservoir
*                        <--    17 bits    -->
*            Ex: 0000 0000 1010 0000 1111 0000   <== reservoir
*                        ^
*                        bit 1
*            To get two bits from this example
*                 result = (reservoir >> 15) & 3
*
*/

#define fill_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
  while (nbits_in_reservoir<nbits_wanted) \
    { \
      unsigned char c; \
      if (stream >= hdata->stream_end) \
        return -1; \
      c = *stream++; \
      reservoir <<= 8; \
      if (c == 0xff && *stream == 0x00) \
        stream++; \
      reservoir |= c; \
      nbits_in_reservoir+=8; \
    } \
}  while(0);


/* Signed version !!!! */
#define get_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do { \
  fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
  result = ((reservoir)>>(nbits_in_reservoir-(nbits_wanted))); \
  nbits_in_reservoir -= (nbits_wanted);  \
  reservoir &= ((1U<<nbits_in_reservoir)-1); \
  if ((unsigned int)result < (1UL<<((nbits_wanted)-1))) \
      result += (0xFFFFFFFFUL<<(nbits_wanted))+1; \
}  while(0);

#define look_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do { \
  fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
  result = ((reservoir)>>(nbits_in_reservoir-(nbits_wanted))); \
}  while(0);

/* To speed up the decoding, we assume that the reservoir has enough bits */
#define skip_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
  nbits_in_reservoir -= (nbits_wanted); \
  reservoir &= ((1U<<nbits_in_reservoir)-1); \
}  while(0);

/**
* Get the next (valid) huffman code in the stream.
*
* To speedup the procedure, we look HUFFMAN_HASH_NBITS bits and the code is
* lower than HUFFMAN_HASH_NBITS we have automaticaly the length of the code
* and the value by using two lookup table.
* Else if the value is not found, just search (linear) into an array for each
* bits is the code is present.
*
* If the code is not present for any reason, -1 is return.
*/
static int get_next_huffman_code(struct jdec_task *hdata, struct huffman_table *huffman_table)
{
  int value, hcode;
  unsigned int extra_nbits, nbits;
  uint16_t *slowtable;


  look_nbits(hdata->reservoir, hdata->nbits_in_reservoir, hdata->stream, HUFFMAN_HASH_NBITS, hcode);

  value = huffman_table->lookup[hcode];
  if (value >= 0)
  {
    unsigned int code_size = huffman_table->code_size[value];
    skip_nbits(hdata->reservoir, hdata->nbits_in_reservoir, hdata->stream, code_size);
    return value;
  }

  /* Decode more bits each time ... */
  for (extra_nbits=0; extra_nbits<16-HUFFMAN_HASH_NBITS; extra_nbits++)
  {
    nbits = HUFFMAN_HASH_NBITS + 1 + extra_nbits;

    look_nbits(hdata->reservoir, hdata->nbits_in_reservoir, hdata->stream, nbits, hcode);
    slowtable = huffman_table->slowtable[extra_nbits];
    /* Search if the code is in this array */
    while (slowtable[0]) {
      if (slowtable[0] == hcode) {
        skip_nbits(hdata->reservoir, hdata->nbits_in_reservoir, hdata->stream, nbits);
        return slowtable[1];
      }
      slowtable+=2;
    }
  }
  return 0;
}

/**
*
* Decode a single block that contains the DCT coefficients.
* The table coefficients is already dezigzaged at the end of the operation.
*
*/
static int process_Huffman_data_unit(struct huffman_context *hc, struct jdec_task *hdata, int component, short int *DCT_out)
{
  unsigned char j;
  unsigned int huff_code;
  int retcode;
  unsigned char size_val, count_0;

  struct component *c = &hc->component_infos[component];
  short int DCT[64];

  /* Initialize the DCT coef table */
  memset(DCT, 0, sizeof(DCT));

  /* DC coefficient decoding */
  retcode = get_next_huffman_code(hdata, c->DC_table);
  // End of stream
  if(retcode == -1)
    return -1;
  else
    huff_code = (unsigned int)retcode;
  if (huff_code) {
    get_nbits(hdata->reservoir, hdata->nbits_in_reservoir, hdata->stream, huff_code, DCT[0]);
    DCT[0] += c->previous_DC;
    c->previous_DC = DCT[0];
  } else {
    DCT[0] = c->previous_DC;
  }

  /* AC coefficient decoding */
  j = 1;
  while (j<64)
  {
    huff_code = get_next_huffman_code(hdata, c->AC_table);
    //trace("- %x\n", huff_code);

    size_val = huff_code & 0xF;
    count_0 = huff_code >> 4;

    if (size_val == 0)
    { /* RLE */
    if (count_0 == 0)
      break;	/* EOB found, go out */
      else if (count_0 == 0xF)
        j += 16;	/* skip 16 zeros */
    }
    else
    {
      j += count_0;	/* skip count_0 zeroes */
      if (j >= 64)
      {
        snprintf(error_string, sizeof(error_string), "Bad huffman data (buffer overflow)");
        break;
      }
      get_nbits(hdata->reservoir, hdata->nbits_in_reservoir, hdata->stream, size_val, DCT[j]);
      j++;
    }
  }

  for (j = 0; j < 64; j++)
    DCT_out[j] = DCT[zigzag[j]];
  return 0;
}

int process_huffman_mcu(struct huffman_context *hc, struct jdec_task *hdata, struct idct_data *idata){
  // Y
  int i;
  for (i=0; i<4; i++){
    if(process_Huffman_data_unit(hc, hdata, cY, idata->DCT_Y[i])){
      return -1;
    }
  }
  //Cb
  if(process_Huffman_data_unit(hc, hdata, cCb, idata->DCT_C[0])){
    return -1;
  }
  //Cr
  if(process_Huffman_data_unit(hc, hdata, cCr, idata->DCT_C[1])){
    return -1;
  }
  return 0;
}

struct huffman_context *create_huffman_context(struct jpeg_parse_context *jpc){
  struct huffman_context *hc = malloc(sizeof(struct huffman_context));

  memcpy(hc->component_infos, jpc->component_infos, sizeof(struct component)*COMPONENTS);
  memcpy(hc->HTDC, jpc->HTDC, sizeof(struct huffman_table));
  memcpy(hc->HTAC, jpc->HTAC, sizeof(struct huffman_table));
  hc->default_huffman_table_initialized = jpc->default_huffman_table_initialized;
  hc->restart_interval = jpc->restart_interval;
  hc->mcus_in_width = jpc->mcus_in_width;

  return hc;
}

void destroy_huffman_context(struct huffman_context *hc){
  free(hc);
}
