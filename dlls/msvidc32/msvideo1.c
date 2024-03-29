/*
 * Microsoft Video-1 Decoder
 * Copyright (C) 2003 the ffmpeg project
 *
 * Portions Copyright (C) 2004 Mike McCormack for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * @file msvideo1.c
 * Microsoft Video-1 Decoder by Mike Melanson (melanson@pcisys.net)
 * For more information about the MS Video-1 format, visit:
 *   http://www.pcisys.net/~melanson/codecs/
 *
 * This decoder outputs either PAL8 or RGB555 data, depending on the
 * whether a RGB palette was passed through palctrl;
 * if it's present, then the data is PAL8; RGB555 otherwise.
 */

#include <stdarg.h> 
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h" 
#include "commdlg.h"
#include "vfw.h"
 
#include "mmsystem.h"
 
#include "wine/debug.h"
 
WINE_DEFAULT_DEBUG_CHANNEL(msvidc32); 

#define CRAM_MAGIC mmioFOURCC('C', 'R', 'A', 'M')
#define MSVC_MAGIC mmioFOURCC('M', 'S', 'V', 'C')
#define WHAM_MAGIC mmioFOURCC('W', 'H', 'A', 'M')

#define PALETTE_COUNT 256
#define LE_16(x)  ((((uint8_t*)(x))[1] << 8) | ((uint8_t*)(x))[0])

/* FIXME - check the stream size */
#define CHECK_STREAM_PTR(n) \
  if ((stream_ptr + n) > buf_size ) { \
    WARN("stream_ptr out of bounds (%d >= %d)\n", \
      stream_ptr + n, buf_size); \
    return; \
  }

typedef BYTE uint8_t;

typedef struct Msvideo1Context {
    DWORD dwMagic;
    int mode_8bit;  /* if it's not 8-bit, it's 16-bit */
} Msvideo1Context;

static void 
msvideo1_decode_8bit( int width, int height, unsigned char *buf, int buf_size,
                      unsigned char *pixels, int stride)
{
    int block_ptr, pixel_ptr;
    int total_blocks;
    int pixel_x, pixel_y;  /* pixel width and height iterators */
    int block_x, block_y;  /* block width and height iterators */
    int blocks_wide, blocks_high;  /* width and height in 4x4 blocks */
    int block_inc;
    int row_dec;

    /* decoding parameters */
    int stream_ptr;
    unsigned char byte_a, byte_b;
    unsigned short flags;
    int skip_blocks;
    unsigned char colors[8];

    stream_ptr = 0;
    skip_blocks = 0;
    blocks_wide = width / 4;
    blocks_high = height / 4;
    total_blocks = blocks_wide * blocks_high;
    block_inc = 4;
    row_dec = stride + 4;

    for (block_y = blocks_high; block_y > 0; block_y--) {
        block_ptr = ((block_y * 4) - 1) * stride;
        for (block_x = blocks_wide; block_x > 0; block_x--) {
            /* check if this block should be skipped */
            if (skip_blocks) {
                block_ptr += block_inc;
                skip_blocks--;
                total_blocks--;
                continue;
            }

            pixel_ptr = block_ptr;

            /* get the next two bytes in the encoded data stream */
            CHECK_STREAM_PTR(2);
            byte_a = buf[stream_ptr++];
            byte_b = buf[stream_ptr++];

            /* check if the decode is finished */
            if ((byte_a == 0) && (byte_b == 0) && (total_blocks == 0))
                return;
            else if ((byte_b & 0xFC) == 0x84) {
                /* skip code, but don't count the current block */
                skip_blocks = ((byte_b - 0x84) << 8) + byte_a - 1;
            } else if (byte_b < 0x80) {
                /* 2-color encoding */
                flags = (byte_b << 8) | byte_a;

                CHECK_STREAM_PTR(2);
                colors[0] = buf[stream_ptr++];
                colors[1] = buf[stream_ptr++];

                for (pixel_y = 0; pixel_y < 4; pixel_y++) {
                    for (pixel_x = 0; pixel_x < 4; pixel_x++, flags >>= 1)
                    {
#if ORIGINAL
                        pixels[pixel_ptr++] = colors[(flags & 0x1) ^ 1];
#else
                        pixels[width*(height-(pixel_ptr/width)-1) + 
                               pixel_ptr%width] = 
                               colors[(flags & 0x1) ^ 1];
                        pixel_ptr++;
#endif
                    }
                    pixel_ptr -= row_dec;
                }
            } else if (byte_b >= 0x90) {
                /* 8-color encoding */
                flags = (byte_b << 8) | byte_a;

                CHECK_STREAM_PTR(8);
                memcpy(colors, &buf[stream_ptr], 8);
                stream_ptr += 8;

                for (pixel_y = 0; pixel_y < 4; pixel_y++) {
                    for (pixel_x = 0; pixel_x < 4; pixel_x++, flags >>= 1)
                    {
#if ORIGINAL
                        pixels[pixel_ptr++] = 
                            colors[((pixel_y & 0x2) << 1) + 
                                (pixel_x & 0x2) + ((flags & 0x1) ^ 1)];
#else
                        pixels[width*(height-(pixel_ptr/width)-1) + 
                               pixel_ptr%width] = 
                            colors[((pixel_y & 0x2) << 1) + 
                                (pixel_x & 0x2) + ((flags & 0x1) ^ 1)];
                        pixel_ptr++;
#endif
                    }
                    pixel_ptr -= row_dec;
                }
            } else {
                /* 1-color encoding */
                colors[0] = byte_a;

                for (pixel_y = 0; pixel_y < 4; pixel_y++) {
                    for (pixel_x = 0; pixel_x < 4; pixel_x++)
                    {
#if ORIGINAL
                        pixels[pixel_ptr++] = colors[0];
#else
                        pixels[width*(height-(pixel_ptr/width)-1) + 
                               pixel_ptr%width] = colors[0];
                        pixel_ptr++;
#endif
                    }
                    pixel_ptr -= row_dec;
                }
            }

            block_ptr += block_inc;
            total_blocks--;
        }
    }
}

static void
msvideo1_decode_16bit( int width, int height, unsigned char *buf, int buf_size,
                       unsigned short *pixels, int stride)
{
    int block_ptr, pixel_ptr;
    int total_blocks;
    int pixel_x, pixel_y;  /* pixel width and height iterators */
    int block_x, block_y;  /* block width and height iterators */
    int blocks_wide, blocks_high;  /* width and height in 4x4 blocks */
    int block_inc;
    int row_dec;

    /* decoding parameters */
    int stream_ptr;
    unsigned char byte_a, byte_b;
    unsigned short flags;
    int skip_blocks;
    unsigned short colors[8];

    stream_ptr = 0;
    skip_blocks = 0;
    blocks_wide = width / 4;
    blocks_high = height / 4;
    total_blocks = blocks_wide * blocks_high;
    block_inc = 4;
    row_dec = stride + 4;

    for (block_y = blocks_high; block_y > 0; block_y--) {
        block_ptr = ((block_y * 4) - 1) * stride;
        for (block_x = blocks_wide; block_x > 0; block_x--) {
            /* check if this block should be skipped */
            if (skip_blocks) {
                block_ptr += block_inc;
                skip_blocks--;
                total_blocks--;
                continue;
            }

            pixel_ptr = block_ptr;

            /* get the next two bytes in the encoded data stream */
            CHECK_STREAM_PTR(2);
            byte_a = buf[stream_ptr++];
            byte_b = buf[stream_ptr++];

            /* check if the decode is finished */
            if ((byte_a == 0) && (byte_b == 0) && (total_blocks == 0)) {
                return;
            } else if ((byte_b & 0xFC) == 0x84) {
                /* skip code, but don't count the current block */
                skip_blocks = ((byte_b - 0x84) << 8) + byte_a - 1;
            } else if (byte_b < 0x80) {
                /* 2- or 8-color encoding modes */
                flags = (byte_b << 8) | byte_a;

                CHECK_STREAM_PTR(4);
                colors[0] = LE_16(&buf[stream_ptr]);
                stream_ptr += 2;
                colors[1] = LE_16(&buf[stream_ptr]);
                stream_ptr += 2;

                if (colors[0] & 0x8000) {
                    /* 8-color encoding */
                    CHECK_STREAM_PTR(12);
                    colors[2] = LE_16(&buf[stream_ptr]);
                    stream_ptr += 2;
                    colors[3] = LE_16(&buf[stream_ptr]);
                    stream_ptr += 2;
                    colors[4] = LE_16(&buf[stream_ptr]);
                    stream_ptr += 2;
                    colors[5] = LE_16(&buf[stream_ptr]);
                    stream_ptr += 2;
                    colors[6] = LE_16(&buf[stream_ptr]);
                    stream_ptr += 2;
                    colors[7] = LE_16(&buf[stream_ptr]);
                    stream_ptr += 2;

                    for (pixel_y = 0; pixel_y < 4; pixel_y++) {
                        for (pixel_x = 0; pixel_x < 4; pixel_x++, flags >>= 1)
                            pixels[pixel_ptr++] = 
                                colors[((pixel_y & 0x2) << 1) + 
                                    (pixel_x & 0x2) + ((flags & 0x1) ^ 1)];
                        pixel_ptr -= row_dec;
                    }
                } else {
                    /* 2-color encoding */
                    for (pixel_y = 0; pixel_y < 4; pixel_y++) {
                        for (pixel_x = 0; pixel_x < 4; pixel_x++, flags >>= 1)
                            pixels[pixel_ptr++] = colors[(flags & 0x1) ^ 1];
                        pixel_ptr -= row_dec;
                    }
                }
            } else {
                /* otherwise, it's a 1-color block */
                colors[0] = (byte_b << 8) | byte_a;

                for (pixel_y = 0; pixel_y < 4; pixel_y++) {
                    for (pixel_x = 0; pixel_x < 4; pixel_x++)
                        pixels[pixel_ptr++] = colors[0];
                    pixel_ptr -= row_dec;
                }
            }

            block_ptr += block_inc;
            total_blocks--;
        }
    }
}

static LRESULT
CRAM_DecompressQuery( Msvideo1Context *info, LPBITMAPINFO in, LPBITMAPINFO out )
{
    TRACE("ICM_DECOMPRESS_QUERY %p %p %p\n", info, in, out);

    if( (info==NULL) || (info->dwMagic!=CRAM_MAGIC) )
        return ICERR_BADPARAM;

    TRACE("planes = %d\n", in->bmiHeader.biPlanes );
    TRACE("bpp    = %d\n", in->bmiHeader.biBitCount );
    TRACE("height = %ld\n", in->bmiHeader.biHeight );
    TRACE("width  = %ld\n", in->bmiHeader.biWidth );
    TRACE("compr  = %lx\n", in->bmiHeader.biCompression );

    if( ( in->bmiHeader.biCompression != CRAM_MAGIC ) &&
        ( in->bmiHeader.biCompression != MSVC_MAGIC ) &&
        ( in->bmiHeader.biCompression != WHAM_MAGIC ) )
        return ICERR_UNSUPPORTED;

    if( ( in->bmiHeader.biBitCount != 16 ) &&
        ( in->bmiHeader.biBitCount != 8 ) )
    {
        TRACE("can't do %d bpp\n", in->bmiHeader.biBitCount );
        return ICERR_UNSUPPORTED;
    }

    /* output must be same dimensions as input */
    if( out )
    {
        if( in->bmiHeader.biBitCount != out->bmiHeader.biBitCount )
            return ICERR_UNSUPPORTED;
        if( in->bmiHeader.biPlanes != out->bmiHeader.biPlanes )
            return ICERR_UNSUPPORTED;
        if( in->bmiHeader.biHeight != out->bmiHeader.biHeight )
            return ICERR_UNSUPPORTED;
        if( in->bmiHeader.biWidth != out->bmiHeader.biWidth )
            return ICERR_UNSUPPORTED;
    }

    TRACE("OK!\n");

    return ICERR_OK;
}

static LRESULT 
CRAM_DecompressGetFormat( Msvideo1Context *info, LPBITMAPINFO in, LPBITMAPINFO out )
{
    DWORD size;

    TRACE("ICM_DECOMPRESS_GETFORMAT %p %p %p\n", info, in, out);

    if( (info==NULL) || (info->dwMagic!=CRAM_MAGIC) )
        return ICERR_BADPARAM;

    size = in->bmiHeader.biSize;
    if (in->bmiHeader.biBitCount <= 8)
        size += in->bmiHeader.biClrUsed * sizeof(RGBQUAD);

    if( out )
    {
        memcpy( out, in, size );
        out->bmiHeader.biCompression = BI_RGB;
        out->bmiHeader.biSizeImage = in->bmiHeader.biHeight
                                   * in->bmiHeader.biWidth *4;
        return ICERR_OK;
    }

    return size;
}

LRESULT CRAM_DecompressBegin( Msvideo1Context *info, LPBITMAPINFO in, LPBITMAPINFO out )
{
    TRACE("ICM_DECOMPRESS_BEGIN %p %p %p\n", info, in, out);

    if( (info==NULL) || (info->dwMagic!=CRAM_MAGIC) )
        return ICERR_BADPARAM;

    TRACE("bitmap is %d bpp\n", in->bmiHeader.biBitCount);
    if( in->bmiHeader.biBitCount == 8 )
        info->mode_8bit = 1;
    else if( in->bmiHeader.biBitCount == 16 )
        info->mode_8bit = 0;
    else
    {
        ERR("Bad output format\n");
        return ICERR_BADPARAM;
    }

    return ICERR_OK;
}

LRESULT CRAM_Decompress( Msvideo1Context *info, ICDECOMPRESS *icd, DWORD size )
{
    LONG width, height, stride, sz;
    WORD bit_per_pixel;

    TRACE("ICM_DECOMPRESS %p %p %ld\n", info, icd, size);

    if( (info==NULL) || (info->dwMagic!=CRAM_MAGIC) )
        return ICERR_BADPARAM;

    /* FIXME: flags are ignored */

    width  = icd->lpbiInput->biWidth;
    height = icd->lpbiInput->biHeight;
    bit_per_pixel = icd->lpbiInput->biBitCount;
    stride = width*bit_per_pixel/8;
    sz = icd->lpbiInput->biSizeImage;

    if (info->mode_8bit)
    {
        msvideo1_decode_8bit( width, height, icd->lpInput, sz,
                              icd->lpOutput, stride);
    }
    else
    {
        msvideo1_decode_16bit( width, height, icd->lpInput, sz,
                               icd->lpOutput, stride);
    }

    return ICERR_OK;
}

LRESULT CRAM_DecompressEx( Msvideo1Context *info, ICDECOMPRESSEX *icd, DWORD size )
{
    LONG width, height, stride, sz;
    WORD bit_per_pixel;

    TRACE("ICM_DECOMPRESSEX %p %p %ld\n", info, icd, size);

    if( (info==NULL) || (info->dwMagic!=CRAM_MAGIC) )
        return ICERR_BADPARAM;

    /* FIXME: flags are ignored */

    width  = icd->lpbiSrc->biWidth;
    height = icd->lpbiSrc->biHeight;
    bit_per_pixel = icd->lpbiSrc->biBitCount;
    stride = width*bit_per_pixel/8;
    sz = icd->lpbiSrc->biSizeImage;

    if (info->mode_8bit)
    {
        msvideo1_decode_8bit( width, height, icd->lpSrc, sz, 
                             icd->lpDst, stride);
    }
    else
    {
        msvideo1_decode_16bit( width, height, icd->lpSrc, sz,
                              icd->lpDst, stride);
    }

    return ICERR_OK;
}

/***********************************************************************
 *		DriverProc (MSVIDC32.@)
 */
LRESULT WINAPI CRAM_DriverProc( DWORD dwDriverId, HDRVR hdrvr, UINT msg,
                                  LONG lParam1, LONG lParam2)
{
    Msvideo1Context *info = (Msvideo1Context *) dwDriverId;
    LRESULT r = 0;

    TRACE("%ld %p %d %ld %ld\n", dwDriverId, hdrvr, msg, lParam1, lParam2);

    switch( msg )
    {
    case DRV_LOAD:
        TRACE("Loaded\n");
        r = 1;
        break;

    case DRV_ENABLE:
        break;

    case DRV_OPEN:
        TRACE("Opened\n");
        info = HeapAlloc( GetProcessHeap(), 0, sizeof (Msvideo1Context) );
        if( info )
        {
            memset( info, 0, sizeof info );
            info->dwMagic = CRAM_MAGIC;
        }
        r = (LRESULT) info;
        break;

    case ICM_DECOMPRESS_QUERY:
        r = CRAM_DecompressQuery( info, (LPBITMAPINFO) lParam1,
                                       (LPBITMAPINFO) lParam2 );
        break;

    case ICM_DECOMPRESS_GET_FORMAT:
        r = CRAM_DecompressGetFormat( info, (LPBITMAPINFO) lParam1,
                                       (LPBITMAPINFO) lParam2 );
        break;

    case ICM_DECOMPRESS_GET_PALETTE:
        FIXME("ICM_DECOMPRESS_GET_PALETTE\n");
        break;

    case ICM_DECOMPRESSEX_QUERY:
        FIXME("ICM_DECOMPRESSEX_QUERY\n");
        break;

    case ICM_DECOMPRESS:
        r = CRAM_Decompress( info, (ICDECOMPRESS*) lParam1,
                                  (DWORD) lParam2 );
        break;

    case ICM_DECOMPRESS_BEGIN:
        r = CRAM_DecompressBegin( info, (LPBITMAPINFO) lParam1,
                                       (LPBITMAPINFO) lParam2 );
        break;

    case ICM_DECOMPRESSEX:
        r = CRAM_DecompressEx( info, (ICDECOMPRESSEX*) lParam1,
                                  (DWORD) lParam2 );
        break;

    case DRV_CLOSE:
        HeapFree( GetProcessHeap(), 0, info );
        break;

    case DRV_DISABLE:
        break;

    case DRV_FREE:
        break;

    default:
        FIXME("Unknown message: %04x %ld %ld\n", msg, lParam1, lParam2);
    }

    return r;
}
