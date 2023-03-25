/*---------------------------------------------------------------------------------

	Camera control for the ARM9

	Copyright (C) 2023 Adrian "asie" Siekierka

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/

#include <stdio.h>
#include <nds.h>
#include <nds/fifomessages.h>

/* High-level functions */

static u8 activeDevice = CAMERA_NONE;

u8 cameraGetActive(void) {
	return activeDevice;
}

bool cameraInit(void) {
	if (REG_CAM_MCNT || (REG_SCFG_CLK & (SCFG_CLK_CAMERA_IF | SCFG_CLK_CAMERA_EXT))) {
		cameraDeinit();
	}

	REG_SCFG_CLK |= SCFG_CLK_CAMERA_IF;
	REG_CAM_MCNT = 0;
	swiDelay(30);
	REG_SCFG_CLK |= SCFG_CLK_CAMERA_EXT;
	swiDelay(30);
	REG_CAM_MCNT |= BIT(1) | BIT(5);
	swiDelay(8200);
	REG_SCFG_CLK &= ~SCFG_CLK_CAMERA_EXT;
	REG_CAM_CNT &= ~CAM_CNT_TRANSFER_ENABLE;
	REG_CAM_CNT |= CAM_CNT_TRANSFER_FLUSH;
	REG_CAM_CNT = (REG_CAM_CNT & ~(0x0300)) | 0x0200;
	REG_CAM_CNT |= 0x0400;
	REG_CAM_CNT |= CAM_CNT_IRQ;
	REG_SCFG_CLK |= SCFG_CLK_CAMERA_EXT;
	swiDelay(20);

	fifoMutexAcquire(FIFO_CAMERA);
	fifoSendValue32(FIFO_CAMERA, CAMERA_CMD_FIFO(CAMERA_CMD_INIT, 0));
	fifoWaitValueAsync32(FIFO_CAMERA);
	u32 result = fifoGetValue32(FIFO_CAMERA);
	fifoMutexRelease(FIFO_CAMERA);

	REG_SCFG_CLK &= ~SCFG_CLK_CAMERA_EXT;
	REG_SCFG_CLK |= SCFG_CLK_CAMERA_EXT;
	swiDelay(20);

	return result == I2CREG_APT_CHIP_VERSION_MT9V113;
}

bool cameraDeinit(void) {
	if (REG_CAM_MCNT & BIT(5)) {
		fifoMutexAcquire(FIFO_CAMERA);
		fifoSendValue32(FIFO_CAMERA, CAMERA_CMD_FIFO(CAMERA_CMD_DEINIT, 0));
		fifoWaitValue32(FIFO_CAMERA);
		fifoGetValue32(FIFO_CAMERA);
		fifoMutexRelease(FIFO_CAMERA);
	}

	if (!(REG_CAM_MCNT || (REG_SCFG_CLK & (SCFG_CLK_CAMERA_IF | SCFG_CLK_CAMERA_EXT)))) {
		return false;
	}

	REG_CAM_CNT &= ~0x8F00;
	REG_CAM_CNT |= CAM_CNT_TRANSFER_FLUSH;
	REG_SCFG_CLK &= ~SCFG_CLK_CAMERA_EXT;
	swiDelay(30);
	REG_CAM_MCNT = 0;
	REG_SCFG_CLK &= ~SCFG_CLK_CAMERA_IF;
	swiDelay(30);

	return true;
}

bool cameraSelect(u8 device) {
	fifoMutexAcquire(FIFO_CAMERA);
	fifoSendValue32(FIFO_CAMERA, CAMERA_CMD_FIFO(CAMERA_CMD_SELECT, device));
	fifoWaitValue32(FIFO_CAMERA);
	bool result = fifoGetValue32(FIFO_CAMERA);
	fifoMutexRelease(FIFO_CAMERA);
	if (result) {
		activeDevice = device;
		return true;
	} else {
		return false;
	}
}

bool cameraStartTransfer(u16 *buffer, u8 captureMode, u8 ndmaId) {
	bool preview = captureMode == MCUREG_APT_SEQ_CMD_PREVIEW;
	if (!preview && captureMode != MCUREG_APT_SEQ_CMD_CAPTURE) {
		return false;
	}

	if (cameraTransferActive()) {
		cameraStopTransfer();
	}

	fifoMutexAcquire(FIFO_CAMERA);
	fifoSendValue32(FIFO_CAMERA, CAMERA_CMD_FIFO(CAMERA_CMD_SEND_SEQ_CMD, captureMode));
	fifoWaitValue32(FIFO_CAMERA);
	fifoGetValue32(FIFO_CAMERA);
	fifoMutexRelease(FIFO_CAMERA);

	REG_CAM_CNT &= ~0x200F;
	if (preview) {
		REG_CAM_CNT |= CAM_CNT_FORMAT_RGB | CAM_CNT_SCANLINES(4);
	} else {
		REG_CAM_CNT |= CAM_CNT_FORMAT_YUV | CAM_CNT_SCANLINES(1);
	}
	REG_CAM_CNT |= CAM_CNT_TRANSFER_FLUSH;
	REG_CAM_CNT |= CAM_CNT_TRANSFER_ENABLE;

	NDMA_SRC(ndmaId)     = (u32) &REG_CAM_DATA;
	NDMA_DEST(ndmaId)    = (u32) buffer;
	NDMA_LENGTH(ndmaId)  = (preview ? (256 * 192) : (640 * 480)) >> 1;
	NDMA_BLENGTH(ndmaId) = preview ? 512 : 320;
	NDMA_BDELAY(ndmaId)  = 2;
	NDMA_CR(ndmaId)      = NDMA_SRC_FIX | NDMA_BLOCK_SCALER(4) | NDMA_START_CAMERA | NDMA_ENABLE;
	return true;
}

/* Low-level functions */

static u16 cameraLLCall(int type, u8 device, u16 reg, u16 value) {
	FifoMessage msg;
	msg.type = type;
	msg.aptRegParams.device = device;
	msg.aptRegParams.reg = reg;
	msg.aptRegParams.value = value;

	fifoMutexAcquire(FIFO_CAMERA);
	fifoSendDatamsg(FIFO_CAMERA, sizeof(msg), (u8*) &msg);
	fifoWaitValue32(FIFO_CAMERA);
	u16 result = fifoGetValue32(FIFO_CAMERA);
	fifoMutexRelease(FIFO_CAMERA);
	return result;
}

u16 cameraI2cRead(u8 device, u16 reg) {
	return cameraLLCall(CAMERA_APT_READ_I2C, device, reg, 0);
}

u16 cameraI2cWrite(u8 device, u16 reg, u16 value) {
	return cameraLLCall(CAMERA_APT_WRITE_I2C, device, reg, value);
}

u16 cameraMcuRead(u8 device, u16 reg) {
	return cameraLLCall(CAMERA_APT_READ_MCU, device, reg, 0);
}

u16 cameraMcuWrite(u8 device, u16 reg, u16 value) {
	return cameraLLCall(CAMERA_APT_WRITE_MCU, device, reg, value);
}
