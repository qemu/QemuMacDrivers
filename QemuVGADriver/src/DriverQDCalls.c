#include "VideoDriverPrivate.h"
#include "VideoDriverPrototypes.h"
#include "DriverQDCalls.h"
#include "QemuVga.h"

static OSStatus		GraphicsCoreDoSetEntries(VDSetEntryRecord *entryRecord, Boolean directDevice, UInt32 start, UInt32 stop, Boolean useValue);

/************************ Color Table Stuff ****************************/

OSStatus
GraphicsCoreSetEntries(VDSetEntryRecord *entryRecord)
{
	Boolean useValue	= (entryRecord->csStart < 0);
	UInt32	start		= useValue ? 0UL : (UInt32)entryRecord->csStart;
	UInt32	stop		= start + entryRecord->csCount;

	Trace(GraphicsCoreSetEntries);

	return GraphicsCoreDoSetEntries(entryRecord, false, start, stop, useValue);
}
						
OSStatus
GraphicsCoreDirectSetEntries(VDSetEntryRecord *entryRecord)
{
	Boolean useValue	= (entryRecord->csStart < 0);
	UInt32	start		= useValue ? 0 : entryRecord->csStart;
	UInt32	stop		= start + entryRecord->csCount;

	Trace(GraphicsCoreDirectSetEntries);
	
	return GraphicsCoreDoSetEntries(entryRecord, true, start, stop, useValue);
}

OSStatus
GraphicsCoreDoSetEntries(VDSetEntryRecord *entryRecord, Boolean directDevice, UInt32 start, UInt32 stop, Boolean useValue)
{
	UInt32 i;
	
	CHECK_OPEN( controlErr );
	if (GLOBAL.depth != 8)
		return controlErr;
	if (NULL == entryRecord->csTable)
		return controlErr;
//	if (directDevice != (VMODE.depth != 8))
//		return controlErr;
	
	/* Note that stop value is included in the range */
	for(i=start;i<=stop;i++) {
		UInt32	tabIndex = i-start;
		UInt32	colorIndex = useValue ? entryRecord->csTable[tabIndex].value : tabIndex;
		QemuVga_SetColorEntry(colorIndex, &entryRecord->csTable[tabIndex].rgb);
	}
	
	return noErr;
}

OSStatus
GraphicsCoreGetEntries(VDSetEntryRecord *entryRecord)
{
	Boolean useValue	= (entryRecord->csStart < 0);
	UInt32	start		= useValue ? 0UL : (UInt32)entryRecord->csStart;
	UInt32	stop		= start + entryRecord->csCount;
	UInt32	i;
	
	Trace(GraphicsCoreGetEntries);

	for(i=start;i<=stop;i++) {
		UInt32	tabIndex = i-start;
		UInt32	colorIndex = useValue ? entryRecord->csTable[tabIndex].value : tabIndex;
		QemuVga_GetColorEntry(colorIndex, &entryRecord->csTable[tabIndex].rgb);
	}

	return noErr;
}

/************************ Gamma ****************************/

OSStatus
GraphicsCoreSetGamma(VDGammaRecord *gammaRec)
{
	CHECK_OPEN( controlErr );
		
	return noErr;
}

OSStatus
GraphicsCoreGetGammaInfoList(VDGetGammaListRec *gammaList)
{
	Trace(GraphicsCoreGammaInfoList);

	return statusErr;
}

OSStatus
GraphicsCoreRetrieveGammaTable(VDRetrieveGammaRec *gammaRec)
{
	Trace(GraphicsCoreRetrieveGammaTable);

	return statusErr;
}

OSStatus
GraphicsCoreGetGamma(VDGammaRecord *gammaRecord)
{
	CHECK_OPEN( statusErr );
		
	Trace(GraphicsCoreGetGamma);

	gammaRecord->csGTable = NULL;

	return noErr;
}


/************************ Gray pages ****************************/
			
OSStatus
GraphicsCoreGrayPage(VDPageInfo *pageInfo)
{
	CHECK_OPEN( controlErr );
		
	Trace(GraphicsCoreGrayPage);

	if (pageInfo->csPage != 0)
		return paramErr;
		
	return noErr;
}
			
OSStatus
GraphicsCoreSetGray(VDGrayRecord *grayRecord)
{
	CHECK_OPEN( controlErr );
	
	Trace(GraphicsCoreSetGray);

	GLOBAL.qdLuminanceMapping	= grayRecord->csMode;
	return noErr;
}


OSStatus
GraphicsCoreGetPages(VDPageInfo *pageInfo)
{
/*	DepthMode mode; */
	CHECK_OPEN( statusErr );

	Trace(GraphicsCoreGetPages);

	pageInfo->csPage = 1;
	return noErr;
}

			
OSStatus
GraphicsCoreGetGray(VDGrayRecord *grayRecord)
{
	CHECK_OPEN( statusErr );
		
	Trace(GraphicsCoreGetGray);
		
	grayRecord->csMode = (GLOBAL.qdLuminanceMapping);
	
	return noErr;
}

/************************ Hardware Cursor ****************************/

OSStatus
GraphicsCoreSupportsHardwareCursor(VDSupportsHardwareCursorRec *hwCursRec)
{
	CHECK_OPEN( statusErr );
		
	Trace(GraphicsCoreSupportsHardwareCursor);

	hwCursRec->csReserved1 = 0;
	hwCursRec->csReserved2 = 0;

	hwCursRec->csSupportsHardwareCursor = false;

	return noErr;
}

OSStatus
GraphicsCoreSetHardwareCursor(VDSetHardwareCursorRec *setHwCursRec)
{
	Trace(GraphicsCoreSetHardwareCursor);

	return controlErr;
}

OSStatus
GraphicsCoreDrawHardwareCursor(VDDrawHardwareCursorRec *drawHwCursRec)
{
	Trace(GraphicsCoreDrawHardwareCursor);

	return controlErr;
}

OSStatus
GraphicsCoreGetHardwareCursorDrawState(VDHardwareCursorDrawStateRec *hwCursDStateRec)
{
	Trace(GraphicsCoreGetHardwareCursorDrawState);

	return statusErr;
}

/************************ Misc ****************************/

OSStatus
GraphicsCoreSetInterrupt(VDFlagRecord *flagRecord)
{
	CHECK_OPEN( controlErr );

	Trace(GraphicsCoreSetInterrupt);

	if (!flagRecord->csMode)
	    QemuVga_EnableInterrupts();
	else
	    QemuVga_DisableInterrupts();

	return noErr;
}

OSStatus
GraphicsCoreGetInterrupt(VDFlagRecord *flagRecord)
{
	Trace(GraphicsCoreGetInterrupt);

	CHECK_OPEN( statusErr );
		
	flagRecord->csMode = !GLOBAL.qdInterruptsEnable;
	return noErr;
}

/* assume initial state is always "power-on" */
// XXX FIXME
static unsigned long MOLVideoPowerState = kAVPowerOn;

OSStatus
GraphicsCoreSetSync(VDSyncInfoRec *syncInfo)
{
	unsigned char syncmask;
	unsigned long newpowermode;

	Trace(GraphicsCoreSetSync);

	CHECK_OPEN( controlErr );

	syncmask = (!syncInfo->csFlags)? kDPMSSyncMask: syncInfo->csFlags;
	if (!(syncmask & kDPMSSyncMask)) /* nothing to do */
		return noErr;
	switch (syncInfo->csMode & syncmask) {
	case kDPMSSyncOn:
		newpowermode = kAVPowerOn;
		break;
	case kDPMSSyncStandby:
		newpowermode = kAVPowerStandby;
		break;
	case kDPMSSyncSuspend:
		newpowermode = kAVPowerSuspend;
		break;
	case kDPMSSyncOff:
		newpowermode = kAVPowerOff;
		break;
	default:
		return paramErr;
	}
	if (newpowermode != MOLVideoPowerState) {
		//OSI_SetVPowerState(newpowermode);
		MOLVideoPowerState = newpowermode;
	}

	return noErr;
}

OSStatus
GraphicsCoreGetSync(VDSyncInfoRec *syncInfo)
{
	CHECK_OPEN( statusErr );
		
	Trace(GraphicsCoreGetSync);

	if (syncInfo->csMode == 0xff) {
		/* report back the capability */
		syncInfo->csMode = 0 | ( 1 << kDisableHorizontalSyncBit)
							 | ( 1 << kDisableVerticalSyncBit)
							 | ( 1 << kDisableCompositeSyncBit);
	} else if (syncInfo->csMode == 0) {
		/* current sync mode */
		switch (MOLVideoPowerState) {
		case kAVPowerOn:
			syncInfo->csMode = kDPMSSyncOn;
			break;
		case kAVPowerStandby:
			syncInfo->csMode = kDPMSSyncStandby;
			break;
		case kAVPowerSuspend:
			syncInfo->csMode = kDPMSSyncSuspend;
			break;
		case kAVPowerOff:
			syncInfo->csMode = kDPMSSyncOff;
			break;
		}
	} else /* not defined ? */
		return paramErr;

	return noErr;
}

OSStatus
GraphicsCoreSetPowerState(VDPowerStateRec *powerStateRec)
{
	Trace(GraphicsCoreSetPowerState);

	CHECK_OPEN( controlErr );

	if (powerStateRec->powerState > kAVPowerOn)
		return paramErr;
		
	if (MOLVideoPowerState != powerStateRec->powerState) {
		//OSI_SetVPowerState(powerStateRec->powerState);
		MOLVideoPowerState = powerStateRec->powerState;
	}
	powerStateRec->powerFlags = 0;

	return noErr;
}

OSStatus
GraphicsCoreGetPowerState(VDPowerStateRec *powerStateRec)
{
	Trace(GraphicsCoreGetPowerState);

	CHECK_OPEN( statusErr );
		
	powerStateRec->powerState = MOLVideoPowerState;
	powerStateRec->powerFlags = 0;
	return noErr;
}
		
OSStatus
GraphicsCoreSetPreferredConfiguration(VDSwitchInfoRec *switchInfo)
{
	Trace(GraphicsCoreSetPreferredConfiguration);

	CHECK_OPEN( controlErr );
	
	return noErr;
}

static UInt8 DepthToDepthMode(UInt8 depth)
{
	switch (depth) {
	case 8:
		return kDepthMode1;
	case 15:
	case 16:
		return kDepthMode2;
	default:
		return kDepthMode3;
	}
}

static UInt8 DepthModeToDepth(UInt8 mode)
{
	switch (mode) {
	case kDepthMode1:
		return 8;
	case kDepthMode2:
		return 15;
	default:
		return 32;
	}
}

OSStatus
GraphicsCoreGetPreferredConfiguration(VDSwitchInfoRec *switchInfo)
{
	Trace(GraphicsCoreGetPreferredConfiguration);

	CHECK_OPEN( statusErr );
	
	switchInfo->csMode 	 	= DepthToDepthMode(GLOBAL.bootDepth);
	switchInfo->csData		= GLOBAL.bootMode + 1; /* Modes are 1 based */
	switchInfo->csPage		= 0;
	switchInfo->csBaseAddr	= FB_START;

	return noErr;
}

// €***************** Misc status calls *********************/

OSStatus
GraphicsCoreGetBaseAddress(VDPageInfo *pageInfo)
{
	Trace(GraphicsCoreGetBaseAddress);

	CHECK_OPEN( statusErr );

	if (pageInfo->csPage != 0)
		return paramErr;
		
	pageInfo->csBaseAddr = FB_START;
	return noErr;
}
			
OSStatus
GraphicsCoreGetConnection(VDDisplayConnectInfoRec *connectInfo)
{
	Trace(GraphicsCoreGetConnection);

	CHECK_OPEN( statusErr );
		
	connectInfo->csDisplayType			= kVGAConnect;
	connectInfo->csConnectTaggedType	= 0;
	connectInfo->csConnectTaggedData	= 0;

	connectInfo->csConnectFlags		=
		(1 << kTaggingInfoNonStandard) | (1 << kUncertainConnection);
		
	connectInfo->csDisplayComponent		= 0;
	
	return noErr;
}

OSStatus
GraphicsCoreGetMode(VDPageInfo *pageInfo)
{
	Trace(GraphicsCoreGetMode);

	CHECK_OPEN( statusErr );
	
	//lprintf("GetMode\n");
	pageInfo->csMode		= DepthToDepthMode(GLOBAL.depth);
	pageInfo->csPage		= 0;
	pageInfo->csBaseAddr	= FB_START;
	
	return noErr;
}

OSStatus
GraphicsCoreGetCurrentMode(VDSwitchInfoRec *switchInfo)
{
	Trace(GraphicsCoreGetCurrentMode);

	CHECK_OPEN( statusErr );
	
	//lprintf("GetCurrentMode\n");
	switchInfo->csMode		= DepthToDepthMode(GLOBAL.depth);
	switchInfo->csData		= GLOBAL.curMode + 1;
	switchInfo->csPage		= 0;
	switchInfo->csBaseAddr	= FB_START;

	return noErr;
}

/********************** Video mode *****************************/
						
OSStatus
GraphicsCoreGetModeTiming(VDTimingInfoRec *timingInfo)
{
	Trace(GraphicsCoreGetModeTiming);

	CHECK_OPEN( statusErr );

	if (timingInfo->csTimingMode < 1 || timingInfo->csTimingMode > GLOBAL.numModes )
		return paramErr;
	
	timingInfo->csTimingFlags	=
		(1 << kModeValid) | (1 << kModeDefault) | (1 <<kModeSafe);

	timingInfo->csTimingFormat	= kDeclROMtables;
	timingInfo->csTimingData	= timingVESA_640x480_60hz;

	return noErr;
}


OSStatus
GraphicsCoreSetMode(VDPageInfo *pageInfo)
{
	Trace(GraphicsCoreSetMode);

	CHECK_OPEN(controlErr);

	if (pageInfo->csPage != 0)
		return paramErr;
	
	QemuVga_SetMode(GLOBAL.curMode, DepthModeToDepth(pageInfo->csMode));
	pageInfo->csBaseAddr = FB_START;

	return noErr;
}			


OSStatus
GraphicsCoreSwitchMode(VDSwitchInfoRec *switchInfo)
{
	UInt32 newMode, newDepth;

	Trace(GraphicsCoreSwitchMode);

	CHECK_OPEN(controlErr);

	if (switchInfo->csPage != 0)
		return paramErr;
	
	newMode = switchInfo->csData - 1;
	newDepth = DepthModeToDepth(switchInfo->csMode);
	
	if (newMode != GLOBAL.curMode || newDepth != GLOBAL.depth) {
		if (QemuVga_SetMode(newMode, newDepth))
			return controlErr;
	}
	switchInfo->csBaseAddr = FB_START;

	return noErr;
}

OSStatus
GraphicsCoreGetNextResolution(VDResolutionInfoRec *resInfo)
{
	UInt32 width, height;
	int id = resInfo->csPreviousDisplayModeID;

	Trace(GraphicsCoreGetNextResolution);

	CHECK_OPEN(statusErr);

	if (id == kDisplayModeIDFindFirstResolution)
		id = 0;
	else if (id == kDisplayModeIDCurrent)
		id = GLOBAL.curMode;
	id++;
	
	if (id == GLOBAL.numModes + 1) {
		resInfo->csDisplayModeID = kDisplayModeIDNoMoreResolutions;
		return noErr;
	}
	if (id < 1 || id > GLOBAL.numModes)
		return paramErr;
	
	if (QemuVga_GetModeInfo(id - 1, &width, &height))
		return paramErr;

	resInfo->csDisplayModeID	= id;
	resInfo->csHorizontalPixels	= width;
	resInfo->csVerticalLines	= height;
	resInfo->csRefreshRate		= 60;
	resInfo->csMaxDepthMode		= kDepthMode3; /* XXX Calculate if it fits ! */

	return noErr;
}

// Looks quite a bit hard-coded, isn't it ?
OSStatus
GraphicsCoreGetVideoParams(VDVideoParametersInfoRec *videoParams)
{
	UInt32 width, height, depth;
	OSStatus err = noErr;
	
	Trace(GraphicsCoreGetVideoParams);

	CHECK_OPEN(statusErr);

	//lprintf("GetVideoParams(ID=%d, depthMode=%d)\n",
	//	videoParams->csDisplayModeID,
	//	videoParams->csDepthMode);
 		
	if (videoParams->csDisplayModeID < 1 || videoParams->csDisplayModeID > GLOBAL.numModes)
		return paramErr;

	if (QemuVga_GetModeInfo(videoParams->csDisplayModeID - 1, &width, &height))
		return paramErr;

	videoParams->csPageCount	= 1;
	
	depth = DepthModeToDepth(videoParams->csDepthMode);

	//lprintf(" -> width=%d, height=%d, depth=%d\n", width, height, depth);
	
	(videoParams->csVPBlockPtr)->vpBaseOffset 		= 0;			// For us, it's always 0
	(videoParams->csVPBlockPtr)->vpBounds.top 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpBounds.left 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpVersion 			= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpPackType 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpPackSize 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpHRes 			= 0x00480000;	// Hard coded to 72 dpi
	(videoParams->csVPBlockPtr)->vpVRes 			= 0x00480000;	// Hard coded to 72 dpi
	(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;			// Always 0

	(videoParams->csVPBlockPtr)->vpBounds.bottom	= height;
	(videoParams->csVPBlockPtr)->vpBounds.right		= width;
	(videoParams->csVPBlockPtr)->vpRowBytes			= width * ((depth + 7) / 8);

	switch (depth) {
	case 8:
		videoParams->csDeviceType 						= clutType;
		(videoParams->csVPBlockPtr)->vpPixelType 		= 0;
		(videoParams->csVPBlockPtr)->vpPixelSize 		= 8;
		(videoParams->csVPBlockPtr)->vpCmpCount 		= 1;
		(videoParams->csVPBlockPtr)->vpCmpSize 			= 8;
		(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;
		break;
	case 15:
	case 16:
		videoParams->csDeviceType 						= directType;
		(videoParams->csVPBlockPtr)->vpPixelType 		= 16;
		(videoParams->csVPBlockPtr)->vpPixelSize 		= 16;
		(videoParams->csVPBlockPtr)->vpCmpCount 		= 3;
		(videoParams->csVPBlockPtr)->vpCmpSize 			= 5;
		(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;
		break;
	case 32:
		videoParams->csDeviceType 						= directType;
		(videoParams->csVPBlockPtr)->vpPixelType 		= 16;
		(videoParams->csVPBlockPtr)->vpPixelSize 		= 32;
		(videoParams->csVPBlockPtr)->vpCmpCount 		= 3;
		(videoParams->csVPBlockPtr)->vpCmpSize 			= 8;
		(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;
		break;
	default:
		err = paramErr;
		break;
	}

	return err;
}
