#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "tinyjpeg.h"
#include "tinyjpeg-internal.h"

extern char error_string[256];

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

/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
static const unsigned char bits_dc_luminance[17] =
{
  0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};
static const unsigned char val_dc_luminance[] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

static const unsigned char bits_dc_chrominance[17] =
{
  0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};
static const unsigned char val_dc_chrominance[] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

static const unsigned char bits_ac_luminance[17] =
{
  0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
};
static const unsigned char val_ac_luminance[] =
{
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

static const unsigned char bits_ac_chrominance[17] =
{
  0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77
};

static const unsigned char val_ac_chrominance[] =
{
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

static void print_SOF(const unsigned char *stream)
{
  int width, height, nr_components, precision;

  precision = stream[2];
  height = be16_to_cpu(stream+3);
  width  = be16_to_cpu(stream+5);
  nr_components = stream[7];

  trace("> SOF marker\n");
  trace("Size:%dx%d nr_components:%d precision:%d\n",
        width, height,
      nr_components,
      precision);
}


/*
* Takes two array of bits, and build the huffman table for size, and code
*
* lookup will return the symbol if the code is less or equal than HUFFMAN_HASH_NBITS.
* code_size will be used to known how many bits this symbol is encoded.
* slowtable will be used when the first lookup didn't give the result.
*/
static void build_huffman_table(const unsigned char *bits, const unsigned char *vals, struct huffman_table *table)
{
  unsigned int i, j, code, code_size, val, nbits;
  unsigned char huffsize[HUFFMAN_BITS_SIZE+1], *hz;
  unsigned int huffcode[HUFFMAN_BITS_SIZE+1], *hc;
  int next_free_entry;

  /*
  * Build a temp array
  *   huffsize[X] => numbers of bits to write vals[X]
  */
  hz = huffsize;
  for (i=1; i<=16; i++)
  {
    for (j=1; j<=bits[i]; j++)
      *hz++ = i;
  }
  *hz = 0;

  memset(table->lookup, 0xff, sizeof(table->lookup));
  for (i=0; i<(16-HUFFMAN_HASH_NBITS); i++)
    table->slowtable[i][0] = 0;

  /* Build a temp array
  *   huffcode[X] => code used to write vals[X]
  */
  code = 0;
  hc = huffcode;
  hz = huffsize;
  nbits = *hz;
  while (*hz)
  {
    while (*hz == nbits)
    {
      *hc++ = code++;
      hz++;
    }
    code <<= 1;
    nbits++;
  }

  /*
  * Build the lookup table, and the slowtable if needed.
  */
  next_free_entry = -1;
  for (i=0; huffsize[i]; i++)
  {
    val = vals[i];
    code = huffcode[i];
    code_size = huffsize[i];

    trace("val=%2.2x code=%8.8x codesize=%2.2d\n", val, code, code_size);

    table->code_size[val] = code_size;
    if (code_size <= HUFFMAN_HASH_NBITS)
    {
      /*
      * Good: val can be put in the lookup table, so fill all value of this
      * column with value val
      */
      int repeat = 1UL<<(HUFFMAN_HASH_NBITS - code_size);
      code <<= HUFFMAN_HASH_NBITS - code_size;
      while ( repeat-- )
        table->lookup[code++] = val;

    }
    else
    {
      /* Perhaps sorting the array will be an optimization */
      uint16_t *slowtable = table->slowtable[code_size-HUFFMAN_HASH_NBITS-1];
      while(slowtable[0])
        slowtable+=2;
      slowtable[0] = code;
      slowtable[1] = val;
      slowtable[2] = 0;
    }

  }
}

static void build_default_huffman_tables(struct jpeg_parse_context *jpc)
{
  if (   (jpc->flags & TINYJPEG_FLAGS_MJPEG_TABLE)
    && jpc->default_huffman_table_initialized)
    return;

  build_huffman_table(bits_dc_luminance, val_dc_luminance, &jpc->HTDC[0]);
  build_huffman_table(bits_ac_luminance, val_ac_luminance, &jpc->HTAC[0]);

  build_huffman_table(bits_dc_chrominance, val_dc_chrominance, &jpc->HTDC[1]);
  build_huffman_table(bits_ac_chrominance, val_ac_chrominance, &jpc->HTAC[1]);

  jpc->default_huffman_table_initialized = 1;
}

/*******************************************************************************
*
* JPEG/JFIF Parsing functions
*
* Note: only a small subset of the jpeg file format is supported. No markers,
* nor progressive stream is supported.
*
******************************************************************************/

static void build_quantization_table(float *qtable, const unsigned char *ref_table)
{
  /* Taken from libjpeg. Copyright Independent JPEG Group's LLM idct.
  * For float AA&N IDCT method, divisors are equal to quantization
  * coefficients scaled by scalefactor[row]*scalefactor[col], where
  *   scalefactor[0] = 1
  *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
  * We apply a further scale factor of 8.
  * What's actually stored is 1/divisor so that the inner loop can
  * use a multiplication rather than a division.
  */
  int i, j;
  static const double aanscalefactor[8] = {
    1.0, 1.387039845, 1.306562965, 1.175875602,
    1.0, 0.785694958, 0.541196100, 0.275899379
  };
  const unsigned char *zz = zigzag;

  for (i=0; i<8; i++) {
    for (j=0; j<8; j++) {
      *qtable++ = ref_table[*zz++] * aanscalefactor[i] * aanscalefactor[j];
    }
  }

}

static int parse_DQT(struct jpeg_parse_context *jpc, const unsigned char *stream)
{
  int qi;
  float *table;
  const unsigned char *dqt_block_end;

  trace("> DQT marker\n");
  dqt_block_end = stream + be16_to_cpu(stream);
  stream += 2;  /* Skip length */

  while (stream < dqt_block_end)
  {
    qi = *stream++;
#if SANITY_CHECK
    if (qi>>4)
      error("16 bits quantization table is not supported\n");
    if (qi>4)
      error("No more 4 quantization table is supported (got %d)\n", qi);
#endif
      table = jpc->Q_tables[qi];
      build_quantization_table(table, stream);
      stream += 64;
  }
  trace("< DQT marker\n");
  return 0;
}

static int parse_SOF(struct jpeg_parse_context *jpc, const unsigned char *stream)
{
  int i, width, height, nr_components, cid, sampling_factor;
  int Q_table;
  struct component *c;

  trace("> SOF marker\n");
  print_SOF(stream);

  height = be16_to_cpu(stream+3);
  width  = be16_to_cpu(stream+5);
  nr_components = stream[7];
#if SANITY_CHECK
  if (stream[2] != 8)
    error("Precision other than 8 is not supported\n");
  if (nr_components != 3)
    error("We only support YUV images\n");
  if (height%16)
    error("Height need to be a multiple of 16 (current height is %d)\n", height);
  if (width%16)
    error("Width need to be a multiple of 16 (current Width is %d)\n", width);
#endif
  stream += 8;
  for (i=0; i<nr_components; i++) {
    cid = *stream++;
    sampling_factor = *stream++;
    Q_table = *stream++;
    c = &jpc->component_infos[i];
#if SANITY_CHECK
    c->cid = cid;
    if (Q_table >= COMPONENTS)
      error("Bad Quantization table index (got %d, max allowed %d)\n", Q_table, COMPONENTS-1);
#endif
    c->Vfactor = sampling_factor&0xf;
    c->Hfactor = sampling_factor>>4;
    jpc->q[i] = Q_table;
    trace("Component:%d  factor:%dx%d  Quantization table:%d\n", cid, c->Hfactor, c->Hfactor, Q_table );

  }
  jpc->width = width;
  jpc->height = height;
  jpc->mcus_in_width = width/16;
  jpc->mcus_in_height = height/16;
// 	jpc->restart_interval = jpc->mcus_in_width * jpc->mcus_in_height;

  trace("< SOF marker\n");

  return 0;
}

static int parse_SOS(struct jpeg_parse_context *jpc, const unsigned char *stream)
{
  unsigned int i, cid, table;
  unsigned int nr_components = stream[2];

  trace("> SOS marker\n");

#if SANITY_CHECK
  if (nr_components != 3)
    error("We only support YCbCr image\n");
#endif

    stream += 3;
    for (i=0;i<nr_components;i++) {
      cid = *stream++;
      table = *stream++;
#if SANITY_CHECK
      if ((table&0xf)>=4)
        error("We do not support more than 2 AC Huffman table\n");
      if ((table>>4)>=4)
        error("We do not support more than 2 DC Huffman table\n");
      if (cid != jpc->component_infos[i].cid)
        error("SOS cid order (%d:%d) isn't compatible with the SOF marker (%d:%d)\n",
              i, cid, i, jpc->component_infos[i].cid);
        trace("ComponentId:%d  tableAC:%d tableDC:%d\n", cid, table&0xf, table>>4);
#endif
        jpc->component_infos[i].AC_table = &jpc->HTAC[table&0xf];
        jpc->component_infos[i].DC_table = &jpc->HTDC[table>>4];
    }
    jpc->stream = stream+3;
    trace("< SOS marker\n");
    return 0;
}

static int parse_DHT(struct jpeg_parse_context *jpc, const unsigned char *stream)
{
  unsigned int count, i;
  unsigned char huff_bits[17];
  int length, index;

  length = be16_to_cpu(stream) - 2;
  stream += 2;  /* Skip length */

  trace("> DHT marker (length=%d)\n", length);

  while (length>0) {
    index = *stream++;

    /* We need to calculate the number of bytes 'vals' will takes */
    huff_bits[0] = 0;
    count = 0;
    for (i=1; i<17; i++) {
      huff_bits[i] = *stream++;
      count += huff_bits[i];
    }
#if SANITY_CHECK
    if (count >= HUFFMAN_BITS_SIZE)
      error("No more than %d bytes is allowed to describe a huffman table", HUFFMAN_BITS_SIZE);
    if ( (index &0xf) >= HUFFMAN_TABLES)
      error("No more than %d Huffman tables is supported (got %d)\n", HUFFMAN_TABLES, index&0xf);
    trace("Huffman table %s[%d] length=%d\n", (index&0xf0)?"AC":"DC", index&0xf, count);
#endif

    if (index & 0xf0 )
      build_huffman_table(huff_bits, stream, &jpc->HTAC[index&0xf]);
    else
      build_huffman_table(huff_bits, stream, &jpc->HTDC[index&0xf]);

    length -= 1;
    length -= 16;
    length -= count;
    stream += count;
  }
  trace("< DHT marker\n");
  return 0;
}

static int parse_DRI(struct jpeg_parse_context *jpc, const unsigned char *stream)
{
  unsigned int length;

  trace("> DRI marker\n");

  length = be16_to_cpu(stream);

#if SANITY_CHECK
  if (length != 4)
    error("Length of DRI marker need to be 4\n");
#endif

  jpc->restart_interval = be16_to_cpu(stream+2);
// 	if (jpc->restart_interval == 0)
// 		jpc->restart_interval = jpc->mcus_in_width * jpc->mcus_in_height;

#if DEBUG
  trace("Restart interval = %d\n", jpc->restart_interval);
#endif

  trace("< DRI marker\n");

  return 0;
}

int parse_JFIF(struct jpeg_parse_context *jpc, const unsigned char *stream)
{
  int chuck_len;
  int marker;
  int sos_marker_found = 0;
  int dht_marker_found = 0;
  const unsigned char *next_chunck;

  /* Parse marker */
  while (!sos_marker_found)
  {
    if (*stream++ != 0xff) {
      trace("Bogus jpeg format\n");
      return -1;
    }
    /* Skip any padding ff byte (this is normal) */
    while (*stream == 0xff)
      stream++;

    marker = *stream++;
    chuck_len = be16_to_cpu(stream);
    next_chunck = stream + chuck_len;
    switch (marker)
    {
      case SOF:
        if (parse_SOF(jpc, stream) < 0)
          return -1;
        break;
      case DQT:
        if (parse_DQT(jpc, stream) < 0)
          return -1;
        break;
      case SOS:
        if (parse_SOS(jpc, stream) < 0)
          return -1;
        sos_marker_found = 1;
        break;
      case DHT:
        if (parse_DHT(jpc, stream) < 0)
          return -1;
        dht_marker_found = 1;
        break;
      case DRI:
        if (parse_DRI(jpc, stream) < 0)
          return -1;
        break;
      default:
        trace("> Unknown marker %2.2x\n", marker);
        break;
    }

    stream = next_chunck;
  }

  if (!dht_marker_found) {
    trace("No Huffman table loaded, using the default one\n");
    build_default_huffman_tables(jpc);
  }

  #ifdef SANITY_CHECK
  if (   (jpc->component_infos[cY].Hfactor < jpc->component_infos[cCb].Hfactor)
    || (jpc->component_infos[cY].Hfactor < jpc->component_infos[cCr].Hfactor))
    error("Horizontal sampling factor for Y should be greater than horitontal sampling factor for Cb or Cr\n");
  if (   (jpc->component_infos[cY].Vfactor < jpc->component_infos[cCb].Vfactor)
    || (jpc->component_infos[cY].Vfactor < jpc->component_infos[cCr].Vfactor))
    error("Vertical sampling factor for Y should be greater than vertical sampling factor for Cb or Cr\n");
  if (   (jpc->component_infos[cCb].Hfactor!=1)
    || (jpc->component_infos[cCr].Hfactor!=1)
    || (jpc->component_infos[cCb].Vfactor!=1)
    || (jpc->component_infos[cCr].Vfactor!=1))
    error("Sampling other than 1x1 for Cr and Cb is not supported");
  #endif

    return 0;
}

static void resync(struct jpeg_parse_context *jpc)
{
  jpc->reservoir = 0;
  jpc->nbits_in_reservoir = 0;
}

static int find_next_rst_marker(struct jpeg_parse_context *jpc)
{
  int rst_marker_found = 0;
  int marker;
  const unsigned char *stream = jpc->stream;

  /* Parse marker */
  while (!rst_marker_found)
  {
    while (*stream++ != 0xff)
    {
      if (stream >= jpc->stream_end)
        error("EOF while search for a RST marker.");
    }
    /* Skip any padding ff byte (this is normal) */
    while (*stream == 0xff)
      stream++;

    marker = *stream++;
    if ((RST+jpc->last_rst_marker_seen) == marker)
      rst_marker_found = 1;
    else if (marker >= RST && marker <= RST7)
      error("Wrong Reset marker found, abording");
    else if (marker == EOI)
      return 0;
  }
  trace("RST Marker %d found at offset %ld\n", jpc->last_rst_marker_seen, stream - jpc->stream_begin);

  jpc->stream = stream;
  jpc->last_rst_marker_seen++;
  jpc->last_rst_marker_seen &= 7;

  return 0;
}

/**
* Create a new JPEG decode task
*
*/
void create_jdec_task(struct jpeg_parse_context *jpc, struct jdec_task *task, int tasknum)
{
  resync(jpc);
  if (tasknum>0)
    find_next_rst_marker(jpc);

  task->id = tasknum;
  task->mcus_posx = (tasknum * jpc->restart_interval) % jpc->mcus_in_width;
  task->mcus_posy = (tasknum * jpc->restart_interval) / jpc->mcus_in_width;

  task->stream_begin = jpc->stream_begin;
  task->stream_end = jpc->stream_end;
  task->stream = jpc->stream;
  task->stream_length = jpc->stream_length;

  task->reservoir = jpc->reservoir;
  task->nbits_in_reservoir = jpc->nbits_in_reservoir;
}

/**
* Initialize the tinyjpeg object and prepare the decoding of the stream.
*
* Check if the jpeg can be decoded with this jpeg decoder.
* Fill some table used for preprocessing.
*/
int tinyjpeg_parse_context_header(struct jpeg_parse_context *jpc, const unsigned char *buf, unsigned int size)
{
  int ret;

  /* Identify the file */
  if ((buf[0] != 0xFF) || (buf[1] != SOI))
    error("Not a JPG file ?\n");

  jpc->stream_begin = buf+2;
  jpc->stream_length = size-2;
  jpc->stream_end = jpc->stream_begin + jpc->stream_length;

  ret = parse_JFIF(jpc, jpc->stream_begin);

  return ret;
}

struct jpeg_parse_context *create_jpeg_parse_context()
{
  return (struct jpeg_parse_context *) calloc(1, sizeof(struct jpeg_parse_context));
}

void destroy_jpeg_parse_context(struct jpeg_parse_context *jpc)
{
  free(jpc);
}

