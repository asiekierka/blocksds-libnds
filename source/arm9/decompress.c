// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2005 Jason Rogers (dovoto)

#include <stdlib.h>

#include <nds/arm9/decompress.h>
#include <nds/arm9/sassert.h>
#include <nds/bios.h>

static int decompress_get_header(uint8_t *source, uint16_t *dest, uint32_t arg)
{
    (void)dest;
    (void)arg;

    return *(uint32_t *)source;
}

static uint8_t decompress_read_8(uint8_t *source)
{
    return *source;
}

static uint16_t decompress_read_16(uint16_t *source)
{
    return *source;
}

static uint32_t decompress_read_32(uint32_t *source)
{
    return *source;
}

TDecompressionStream decomStream = {
    decompress_get_header,
    NULL, // The close callback can be omitted
    decompress_read_8,
    decompress_read_16,
    decompress_read_32
};

void decompress(const void *data, void *dst, DecompressType type)
{
    switch (type)
    {
        case LZ77Vram:
            swiDecompressLZSSVram(data, dst, 0, &decomStream);
            break;
        case LZ77:
            swiDecompressLZSSWram(data, dst);
            break;
        case HUFF:
            swiDecompressHuffman(data, dst, 0, &decomStream);
            break;
        case RLE:
            swiDecompressRLEWram(data, dst);
            break;
        case RLEVram:
            swiDecompressRLEVram(data, dst, 0, &decomStream);
            break;
        default:
            break;
    }
}

void decompressStream(const void *data, void *dst, DecompressType type,
                      getByteCallback readCB, getHeaderCallback getHeaderCB)
{
#ifdef ARM9
    sassert(type != LZ77 && type != RLE,
            "LZ77 and RLE do not support streaming, use Vram versions");
#endif

    sassert(type != HUFF, "HUFF not supported");

    TDecompressionStream decompresStream = {
        getHeaderCB,
        NULL,
        readCB,
        NULL,
        NULL // Only required for Huffman
    };

    switch (type)
    {
        case LZ77Vram:
            swiDecompressLZSSVram(data, dst, 0, &decompresStream);
            break;
        case HUFF:
            break;
        case RLEVram:
            swiDecompressRLEVram(data, dst, 0, &decompresStream);
            break;
        default:
            break;
    }
}
