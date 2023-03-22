// SPDX-License-Identifier: Zlib
//
// Copyright (c) 2023 Adrian "asie" Siekierka

#ifndef NDS_ARM7_CAMERA_H__
#define NDS_ARM7_CAMERA_H__

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* low-level Aptina I2C register read/write functions */
u8 aptI2cWrite(u8 device, u16 reg, u16 data);
u16 aptI2cRead(u8 device, u16 reg);
// #define aptI2cWrite i2cWriteRegister16
// #define aptI2cRead i2cReadRegister16
void aptI2cWaitClearBits(u8 device, u16 reg, u16 mask);
void aptI2cWaitSetBits(u8 device, u16 reg, u16 mask);
void aptI2cClearBits(u8 device, u16 reg, u16 mask);
void aptI2cSetBits(u8 device, u16 reg, u16 mask);

/* low-level Aptina MCU register read/write functions */
u16 aptMcuRead(u8 device, u16 reg);
void aptMcuWrite(u8 device, u16 reg, u16 data);
void aptMcuWaitClearBits(u8 device, u16 reg, u16 mask);
void aptMcuWaitSetBits(u8 device, u16 reg, u16 mask);
void aptMcuClearBits(u8 device, u16 reg, u16 mask);
void aptMcuSetBits(u8 device, u16 reg, u16 mask);

/* high-level camera functions */
void aptCameraSetMode(u8 device, u8 mode);
void aptCameraInit(u8 device);
void aptCameraDeinit(u8 device);
void aptCameraActivate(u8 device);
void aptCameraDeactivate(u8 device);

/* camera FIFO handler */
void installCameraFIFO(void);

#ifdef __cplusplus
}
#endif

#endif // NDS_ARM7_CAMERA_H__
