// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2008-2010 Jason Rogers (dovoto)
// Copyright (C) 2008-2010 Dave Murphy (WinterMute)

#include <nds/arm9/cache.h>
#include <nds/arm9/sprite.h>
#include <nds/arm9/trig_lut.h>
#include <nds/dma.h>
#include <nds/interrupts.h>

SpriteEntry OamMemorySub[128];
SpriteEntry OamMemory[128];

OamState oamMain =
{
    .gfxOffsetStep = -1,
    .firstFree = 0,
    .allocBufferSize = 32,
    .allocBuffer = NULL,
    .oamMemory = OamMemory,
    .spriteMapping = SpriteMapping_1D_128
};

OamState oamSub =
{
    .gfxOffsetStep = -1,
    .firstFree = 0,
    .allocBufferSize = 32,
    .allocBuffer = NULL,
    .oamMemory = OamMemorySub,
    .spriteMapping = SpriteMapping_1D_128
};

void oamInit(OamState *oam, SpriteMapping mapping, bool extPalette)
{
    int extPaletteFlag = extPalette ? DISPLAY_SPR_EXT_PALETTE : 0;

    oam->gfxOffsetStep = (mapping & 3) + 5;

    oam->spriteMapping = mapping;

    dmaFillWords(0, oam->oamMemory, sizeof(OamMemory));

    for (int i = 0; i < 128; i++)
        oam->oamMemory[i].isHidden = true;

    for (int i = 0; i < 32; i++)
    {
        oam->oamRotationMemory[i].hdx = (1 << 8);
        oam->oamRotationMemory[i].vdy = (1 << 8);
    }

    swiWaitForVBlank();

    DC_FlushRange(oam->oamMemory, sizeof(OamMemory));

    if (oam == &oamMain)
    {
        dmaCopy(oam->oamMemory, OAM, sizeof(OamMemory));

        REG_DISPCNT &= ~DISPLAY_SPRITE_ATTR_MASK;
        REG_DISPCNT |= DISPLAY_SPR_ACTIVE | (mapping & 0xffffff0) | extPaletteFlag;
    }
    else
    {
        dmaCopy(oam->oamMemory, OAM_SUB, sizeof(OamMemory));

        REG_DISPCNT_SUB &= ~DISPLAY_SPRITE_ATTR_MASK;
        REG_DISPCNT_SUB |= DISPLAY_SPR_ACTIVE | (mapping & 0xffffff0) | extPaletteFlag;
    }

    oamAllocReset(oam);
}

void oamDisable(OamState *oam)
{
    if (oam == &oamMain)
        REG_DISPCNT &= ~DISPLAY_SPR_ACTIVE;
    else
        REG_DISPCNT_SUB &= ~DISPLAY_SPR_ACTIVE;
}

void oamEnable(OamState *oam)
{
    if (oam == &oamMain)
        REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
    else
        REG_DISPCNT_SUB |= DISPLAY_SPR_ACTIVE;
}

u16 *oamGetGfxPtr(OamState *oam, int gfxOffsetIndex)
{
    if (gfxOffsetIndex < 0)
        return NULL;

    if (oam == &oamMain)
        return &SPRITE_GFX[(gfxOffsetIndex << oam->gfxOffsetStep) >> 1];
    else
        return &SPRITE_GFX_SUB[(gfxOffsetIndex << oam->gfxOffsetStep) >> 1];
}

void oamClear(OamState *oam, int start, int count)
{
    int i = 0;

    if (count == 0)
    {
        count = 128;
        start = 0;
    }

    for (i = start; i < count + start; i++)
        oam->oamMemory[i].attribute[0] = ATTR0_DISABLED;
}

unsigned int oamGfxPtrToOffset(OamState *oam, const void *offset)
{
    if (oam->spriteMapping & DISPLAY_SPR_1D)
    {
        return ((u32)offset & 0xFFFFF) >> oam->gfxOffsetStep;
    }
    else
    {
        u32 size = (oam->spriteMapping & DISPLAY_SPR_2D_BMP_256);

        u32 toffset = (((u32)offset) & 0xFFFFF) >> 1;

        if (size == DISPLAY_SPR_2D_BMP_256)
        {
            u32 x = (toffset & 0xFF);
            u32 y = (toffset >> (8 + 3));

            return (x >> 3) | (y << 5);
        }
        else
        {
            u32 x = (toffset & 0x7F);
            u32 y = (toffset >> (7 + 3));

            return (x >> 3) | (y << 4);
        }
    }
}

SpriteSize oamDimensionsToSize(int width, int height)
{
    if (width == 8)
    {
        if (height == 8)
            return SpriteSize_8x8;
        else if (height == 16)
            return SpriteSize_8x16;
        else if (height == 32)
            return SpriteSize_8x32;
    }
    else if (width == 16)
    {
        if (height == 8)
            return SpriteSize_16x8;
        else if (height == 16)
            return SpriteSize_16x16;
        else if (height == 32)
            return SpriteSize_16x32;
    }
    else if (width == 32)
    {
        if (height == 8)
            return SpriteSize_32x8;
        else if (height == 16)
            return SpriteSize_32x16;
        else if (height == 32)
            return SpriteSize_32x32;
        else if (height == 64)
            return SpriteSize_32x64;
    }
    else if (width == 64)
    {
        if (height == 32)
            return SpriteSize_64x32;
        else if (height == 64)
            return SpriteSize_64x64;
    }

    return SpriteSize_Invalid;
}

void oamSet(OamState *oam, int id, int x, int y, int priority, int palette_alpha,
            SpriteSize size, SpriteColorFormat format, const void *gfxOffset,
            int affineIndex, bool sizeDouble, bool hide, bool hflip, bool vflip,
            bool mosaic)
{
    if (hide)
    {
        oam->oamMemory[id].attribute[0] = ATTR0_DISABLED;
        return;
    }

    oam->oamMemory[id].shape = SPRITE_SIZE_SHAPE(size);
    oam->oamMemory[id].size = SPRITE_SIZE_SIZE(size);
    oam->oamMemory[id].x = x;
    oam->oamMemory[id].y = y;
    oam->oamMemory[id].priority = priority;
    oam->oamMemory[id].hFlip = hflip;
    oam->oamMemory[id].vFlip = vflip;
    oam->oamMemory[id].isMosaic = mosaic;
    oam->oamMemory[id].gfxIndex = oamGfxPtrToOffset(oam, gfxOffset);

    if (affineIndex >= 0 && affineIndex < 32)
    {
        oam->oamMemory[id].rotationIndex = affineIndex;
        oam->oamMemory[id].isSizeDouble = sizeDouble;
        oam->oamMemory[id].isRotateScale = true;
    }
    else
    {
        oam->oamMemory[id].isSizeDouble = false;
        oam->oamMemory[id].isRotateScale = false;
    }

    if (format == SpriteColorFormat_Bmp)
    {
        oam->oamMemory[id].blendMode = OBJMODE_BITMAP;
        oam->oamMemory[id].colorMode = 0;
        oam->oamMemory[id].alpha = palette_alpha;
    }
    else
    {
        oam->oamMemory[id].blendMode = OBJMODE_NORMAL;
        // SpriteColorFormat is identical to ObjColMode except for
        // SpriteColorFormat_Bmp, checked for above
        oam->oamMemory[id].colorMode = (ObjColMode)format;
        oam->oamMemory[id].palette = palette_alpha;
    }
}

void oamSetGfx(OamState *oam, int id, SpriteSize size, SpriteColorFormat format,
               const void *gfxOffset)
{
    sassert(oam == &oamMain || oam == &oamSub,
            "oamSetGfx() oam must be &oamMain or &oamSub");
    sassert(id >= 0 && id < SPRITE_COUNT,
            "oamSetGfx() index is out of bounds, must be 0-127");

    oam->oamMemory[id].shape    = (ObjShape)SPRITE_SIZE_SHAPE(size);
    oam->oamMemory[id].size     = (ObjSize)SPRITE_SIZE_SIZE(size);
    oam->oamMemory[id].gfxIndex = oamGfxPtrToOffset(oam, gfxOffset);

    if (format == SpriteColorFormat_Bmp)
    {
        oam->oamMemory[id].blendMode = OBJMODE_BITMAP;
        oam->oamMemory[id].colorMode = 0;
    }
    else
    {
        oam->oamMemory[id].blendMode = OBJMODE_NORMAL;
        // SpriteColorFormat is identical to ObjColMode except for
        // SpriteColorFormat_Bmp, checked for above
        oam->oamMemory[id].colorMode = (ObjColMode)format;
    }
}

void oamUpdate(OamState *oam)
{
    DC_FlushRange(oam->oamMemory, sizeof(OamMemory));

    if (oam == &oamMain)
        dmaCopy(oam->oamMemory, OAM, sizeof(OamMemory));
    else
        dmaCopy(oam->oamMemory, OAM_SUB, sizeof(OamMemory));
}

void oamRotateScale(OamState *oam, int rotId, int angle, int sx, int sy)
{
    sassert(rotId >= 0 && rotId < 32,
            "oamRotateScale() rotId is out of bounds, must be 0-31");

    int ss = sinLerp(angle);
    int cc = cosLerp(angle);

    oam->oamRotationMemory[rotId].hdx = (cc * sx) >> 12;
    oam->oamRotationMemory[rotId].vdx = (-ss * sx) >> 12;
    oam->oamRotationMemory[rotId].hdy = (ss * sy) >> 12;
    oam->oamRotationMemory[rotId].vdy = (cc * sy) >> 12;
}
