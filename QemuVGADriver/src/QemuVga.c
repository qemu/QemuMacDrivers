#include "VideoDriverPrivate.h"
#include "VideoDriverPrototypes.h"
#include "DriverQDCalls.h"
#include "QemuVga.h"

/* List of supported modes */
struct vMode {
	UInt32	width;
	UInt32	height;
};

static struct vMode vModes[] =  {
	{ 640, 480 },
	{ 800, 600 },
	{ 1024, 768 },
	{ 1280, 1024 },
	{ 1600, 1200 },
	{ 1920, 1080 },
	{ 1920, 1200 },
	{ 0,0 }
};

static void VgaWriteB(UInt16 port, UInt8 val)
{
	UInt8 *ptr;
	
	ptr = (UInt8 *)((UInt32)GLOBAL.boardRegAddress + port + 0x400 - 0x3c0);
	*ptr = val;
	SynchronizeIO();
}

static UInt8 VgaReadB(UInt16 port)
{
	UInt8 *ptr, val;
	
	ptr = (UInt8 *)((UInt32)GLOBAL.boardRegAddress + port + 0x400 - 0x3c0);
	val = *ptr;
	SynchronizeIO();
	return val;
}

static void DispiWriteW(UInt16 reg, UInt16 val)
{
	UInt16 *ptr;
	
	ptr = (UInt16 *)((UInt32)GLOBAL.boardRegAddress + (reg << 1) + 0x500);
	*ptr = EndianSwap16Bit(val);
	SynchronizeIO();
}

static UInt16 DispiReadW(UInt16 reg)
{
	UInt16 *ptr, val;
	
	ptr = (UInt16 *)((UInt32)GLOBAL.boardRegAddress + (reg << 1) + 0x500);
	val = EndianSwap16Bit(*ptr);
	SynchronizeIO();
	return val;
}

static void ExtWriteL(UInt16 reg, UInt32 val)
{
	UInt32 *ptr;
	
	ptr = (UInt32 *)((UInt32)GLOBAL.boardRegAddress + (reg << 2) + 0x600);
	*ptr = EndianSwap32Bit(val);
	SynchronizeIO();
}

static UInt32 ExtReadL(UInt32 reg)
{
	UInt32 *ptr, val;
	
	ptr = (UInt32 *)((UInt32)GLOBAL.boardRegAddress + (reg << 2) + 0x600);
	val = EndianSwap32Bit(*ptr);
	SynchronizeIO();
	return val;
}

OSStatus QemuVga_Init(void)
{
	UInt16 id, i;
	UInt32 mem, width, height, depth;

	id = DispiReadW(VBE_DISPI_INDEX_ID);
	mem = DispiReadW(VBE_DISPI_INDEX_VIDEO_MEMORY_64K);
	mem <<= 16;
	lprintf("DISPI_ID=%04x VMEM=%d Mb\n", id, mem >> 20);
	if ((id & 0xfff0) != VBE_DISPI_ID0) {
		lprintf("Unsupported ID !\n");
		return controlErr;
	}
	if (mem > GLOBAL.boardFBMappedSize)
		mem = GLOBAL.boardFBMappedSize;
	GLOBAL.vramSize = mem;
	
	// XXX Add endian control regs

	width = DispiReadW(VBE_DISPI_INDEX_XRES);
	height = DispiReadW(VBE_DISPI_INDEX_YRES);
	depth = DispiReadW(VBE_DISPI_INDEX_BPP);
	lprintf("Current setting: %dx%dx%d\n", width, height, depth);

	GLOBAL.depth = GLOBAL.bootDepth = depth;
	for (i = 0; vModes[i].width; i++) {
		if (width == vModes[i].width && height == vModes[i].height)
			break;
	}
	if (!vModes[i].width) {
		lprintf("Not found in list ! using default.\n");
		i = 0;
	}
	GLOBAL.curMode = GLOBAL.bootMode = i;
	GLOBAL.numModes = sizeof(vModes) / sizeof(struct vMode) - 1;

	return noErr;
}

static OSStatus VBLTimerProc(void *p1, void *p2);

static OSStatus ScheduleVBLTimer(void)
{
    /* XXX HACK: Run timer at 20Hz */
	AbsoluteTime target = AddDurationToAbsolute(50, UpTime());
	
	return SetInterruptTimer(&target, VBLTimerProc, NULL, &GLOBAL.VBLTimerID);
}

static OSStatus VBLTimerProc(void *p1, void *p2)
{
	GLOBAL.inInterrupt = 1;

	/* This can be called before the service is ready */
	if (GLOBAL.qdVBLInterrupt && GLOBAL.qdInterruptsEnable)
		VSLDoInterruptService(GLOBAL.qdVBLInterrupt);
	
	/* Reschedule */
	ScheduleVBLTimer();

	GLOBAL.inInterrupt = 0;
}

OSStatus QemuVga_Open(void)
{
	lprintf("QemuVga v1.00\n");

	GLOBAL.isOpen = true;

	/* Schedule the timer now if timers are supported. They aren't on OS X
	 * in which case we must not create the VSL service, otherwise OS X will expect
	 * a VBL and fail to update the cursor when not getting one.
	 */
	GLOBAL.hasTimer = (ScheduleVBLTimer() == noErr);
	GLOBAL.qdInterruptsEnable = GLOBAL.hasTimer;

	/* Create VBL if timer works */
	if (GLOBAL.hasTimer && !GLOBAL.qdVBLInterrupt)
		VSLNewInterruptService(&GLOBAL.deviceEntry, kVBLInterruptServiceType, &GLOBAL.qdVBLInterrupt);
	
	if (GLOBAL.hasTimer)
		lprintf("Using timer to simulate VBL.\n");	
	else
		lprintf("No timer service (OS X ?), VBL not registered.\n");	

	return noErr;
}

OSStatus QemuVga_Close(void)
{
	lprintf("Closing Driver...\n");

	GLOBAL.isOpen = false;
	
	QemuVga_DisableInterrupts();
	if (GLOBAL.qdVBLInterrupt)
		VSLDisposeInterruptService( GLOBAL.qdVBLInterrupt );
	GLOBAL.qdVBLInterrupt = NULL;

	return noErr;
}

OSStatus QemuVga_Exit(void)
{
	QemuVga_Close();

	return noErr;
}

void QemuVga_EnableInterrupts(void)
{
	GLOBAL.qdInterruptsEnable = true;
	if (GLOBAL.hasTimer)
		ScheduleVBLTimer();	
}

void QemuVga_DisableInterrupts(void)
{
	AbsoluteTime remaining;

	GLOBAL.qdInterruptsEnable = false;
	if (GLOBAL.hasTimer)
		CancelTimer(GLOBAL.VBLTimerID, &remaining);
}

OSStatus QemuVga_SetColorEntry(UInt32 index, RGBColor *color)
{
	//lprintf("SetColorEntry %d, %x %x %x\n", index, color->red, color->green, color->blue);
	VgaWriteB(0x3c8, index);
	VgaWriteB(0x3c9, color->red >> 8);
	VgaWriteB(0x3c9, color->green >> 8);
	VgaWriteB(0x3c9, color->blue >> 8);
	return noErr;
}

OSStatus QemuVga_GetColorEntry(UInt32 index, RGBColor *color)
{
	UInt32 r,g,b;
	
	VgaWriteB(0x3c7, index);
	r = VgaReadB(0x3c9);
	g = VgaReadB(0x3c9);
	b = VgaReadB(0x3c9);
	color->red = (r << 8) | r;
	color->green = (g << 8) | g;
	color->blue = (b << 8) | b;

	return noErr;
}

OSStatus QemuVga_GetModeInfo(UInt32 index, UInt32 *width, UInt32 *height)
{
	if (index >= GLOBAL.numModes)
		return paramErr;
	if (width)
		*width = vModes[index].width;
	if (height)
		*height = vModes[index].height;
	return noErr;
}


OSStatus QemuVga_SetMode(UInt32 mode, UInt32 depth)
{
	UInt32 width, height;

	if (mode >= GLOBAL.numModes)
		return paramErr;
	width = vModes[mode].width;
	height = vModes[mode].height;

	lprintf("Set Mode: %dx%dx%d\n", width, height, depth);

	DispiWriteW(VBE_DISPI_INDEX_ENABLE,      0);
	DispiWriteW(VBE_DISPI_INDEX_BPP,         depth);
	DispiWriteW(VBE_DISPI_INDEX_XRES,        width);
	DispiWriteW(VBE_DISPI_INDEX_YRES,        height);
	DispiWriteW(VBE_DISPI_INDEX_BANK,        0);
	DispiWriteW(VBE_DISPI_INDEX_VIRT_WIDTH,  width);
	DispiWriteW(VBE_DISPI_INDEX_VIRT_HEIGHT, height);
	DispiWriteW(VBE_DISPI_INDEX_X_OFFSET,    0);
	DispiWriteW(VBE_DISPI_INDEX_Y_OFFSET,    0);
	DispiWriteW(VBE_DISPI_INDEX_ENABLE,      VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_8BIT_DAC);	
	GLOBAL.curMode = mode;
	GLOBAL.depth = depth;
	
	return noErr;
}
