/*******************************************************************************
*
* Colorspace conversion routine
*
* Note:
* YCbCr is defined per CCIR 601-1, except that Cb and Cr are
* normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
* The conversion equations to be implemented are therefore
*      R = Y                + 1.40200 * Cr
*      G = Y - 0.34414 * Cb - 0.71414 * Cr
*      B = Y + 1.77200 * Cb
*
******************************************************************************/

#include <stdlib.h>
#include "tinyjpeg-internal.h"

static unsigned char clamp(int i)
{
  if (i<0)
    return 0;
  else if (i>255)
    return 255;
  else
    return i;
}

#define SCALEBITS       10
#define ONE_HALF        (1UL << (SCALEBITS-1))
#define FIX(x)          ((int)((x) * (1UL<<SCALEBITS) + 0.5))

/**
*  YCrCb -> RGB24 (2x2)
*  .-------.
*  | 1 | 2 |
*  |---+---|
*  | 3 | 4 |
*  `-------'
*/
void convert_yuv_bgr(struct cc_context *cc, struct yuv_data *yuv)
{
  const unsigned char *Y, *Cb, *Cr;
  unsigned char *p, *p2;
  int i,j;
  int offset_to_next_row;

  p = cc->rgb_data;
  p2 = cc->rgb_data + cc->width*3;
  Y = yuv->Y;
  Cb = yuv->Cb;
  Cr = yuv->Cr;
  offset_to_next_row = (cc->width*3*2) - 16*3;

  for (i=0; i<8; i++) {
    for (j=0; j<8; j++) {

      int y, cb, cr;
      int add_r, add_g, add_b;
      int r, g , b;

      cb = *Cb++ - 128;
      cr = *Cr++ - 128;
      add_r = FIX(1.40200) * cr + ONE_HALF;
      add_g = - FIX(0.34414) * cb - FIX(0.71414) * cr + ONE_HALF;
      add_b = FIX(1.77200) * cb + ONE_HALF;

      y  = (*Y++) << SCALEBITS;
      r = (y + add_r) >> SCALEBITS;
      g = (y + add_g) >> SCALEBITS;
      b = (y + add_b) >> SCALEBITS;

      *p++ = clamp(b);
      *p++ = clamp(g);
      *p++ = clamp(r);

      y  = (*Y++) << SCALEBITS;
      r = (y + add_r) >> SCALEBITS;
      g = (y + add_g) >> SCALEBITS;
      b = (y + add_b) >> SCALEBITS;
      *p++ = clamp(b);
      *p++ = clamp(g);
      *p++ = clamp(r);

      y  = (Y[16-2]) << SCALEBITS;
      r = (y + add_r) >> SCALEBITS;
      g = (y + add_g) >> SCALEBITS;
      b = (y + add_b) >> SCALEBITS;
      *p2++ = clamp(b);
      *p2++ = clamp(g);
      *p2++ = clamp(r);

      y  = (Y[16-1]) << SCALEBITS;
      r = (y + add_r) >> SCALEBITS;
      g = (y + add_g) >> SCALEBITS;
      b = (y + add_b) >> SCALEBITS;
      *p2++ = clamp(b);
      *p2++ = clamp(g);
      *p2++ = clamp(r);
    }
    Y  += 16;
    p  += offset_to_next_row;
    p2 += offset_to_next_row;
  }
}

struct cc_context *create_cc_context(struct jpeg_parse_context *jpc, uint8_t *rgb_data){
  struct cc_context *cc = malloc(sizeof(struct cc_context));
  cc->width = jpc->width;
  cc->height = jpc->height;
  cc->restart_interval = jpc->restart_interval;
  cc->mcus_in_width = jpc->mcus_in_width;
  cc->mcus_in_height = jpc->mcus_in_height;
  cc->base = rgb_data;

  return cc;
}

void destroy_cc_context(struct cc_context *cc){
  free(cc);
}
