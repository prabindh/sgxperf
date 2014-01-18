#include <stdio.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <sys/time.h> //for profiling
#include <stdlib.h> //for profiling
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "gl2ext.h"
#include "eglext.h"

#include "sgxperf_gles20_vg.h"
#include "math.h"

#ifdef _ENABLE_TEST15

#include "pvr2d.h"

static PVR2DERROR ePVR2DStatus1;
static PVR2DDEVICEINFO* pDevInfo1 = 0;
static PVR2DCONTEXTHANDLE hPVR2DContext1 = 0;
static PVR2DMEMINFO *pSrcMemInfo_MemWrap1=0, *pDstMemInfo_MemWrap = 0;
static PPVR2DBLTINFO pBlt1=0;
static PVR2DBLTINFO pvr2dBltInfo;

unsigned int 
test15_process(
void* srcPtr,
void* dstPtr,
unsigned int srcWidthPixels,
unsigned int srcHeightPixels,
unsigned int srcStrideBytes,
unsigned int dstWidthPixels,
unsigned int dstHeightPixels,
unsigned int dstStrideBytes,
unsigned int srcBytesPerPixel,
unsigned int dstBytesPerPixel
)
{
  int nDeviceNum;
  int nDevices;

	nDevices = PVR2DEnumerateDevices(0);
	//printf("nDevices = %d\n", nDevices);
	if(nDevices <= 0) 
	{
		printf("FATAL error - no devices !\n");
		return 1;
	}
	pDevInfo1 = (PVR2DDEVICEINFO *) malloc(nDevices * sizeof(PVR2DDEVICEINFO));
	if(!pDevInfo1)
	{
		printf("FATAL error - could not allocate memory for device info !\n");
		return 2;
	}
	PVR2DEnumerateDevices(pDevInfo1);
	nDeviceNum = pDevInfo1[0].ulDevID;
	//Create the context
	ePVR2DStatus1 = PVR2DCreateDeviceContext (nDeviceNum, &hPVR2DContext1, 0);
	if(!hPVR2DContext1)
	{
		printf("FATAL error - could not create device context !\n");
		return 3;
	}


  //Wrap the source
	ePVR2DStatus1 = PVR2DMemWrap(
                  hPVR2DContext1, srcPtr, 0, 
      						(srcHeightPixels * srcStrideBytes),
      						NULL,
      						&pSrcMemInfo_MemWrap1
      						);

	if(pSrcMemInfo_MemWrap1 == NULL) 
	{
		SGXPERF_printf("Fatal Error in memwrap of source %x, err = %d\n",
        (unsigned int)srcPtr, ePVR2DStatus1);
		return 1;
	}

	pBlt1 = &pvr2dBltInfo;
	memset(pBlt1, 0, sizeof(PVR2DBLTINFO));
	pBlt1->BlitFlags = PVR2D_BLIT_DISABLE_ALL;
	pBlt1->pDstMemInfo = pDstMemInfo_MemWrap;
	pBlt1->DstSurfWidth = dstWidthPixels;
	pBlt1->DstSurfHeight = dstHeightPixels;
	pBlt1->DstStride = dstStrideBytes;
	if(dstBytesPerPixel == 4)
    		pBlt1->DstFormat = PVR2D_ARGB8888;
	else if(dstBytesPerPixel == 2)
		pBlt1->DstFormat = PVR2D_RGB565;
  //Wrap the destination
	ePVR2DStatus1 = PVR2DMemWrap(
                  hPVR2DContext1, dstPtr, 0, 
      						(dstBytesPerPixel * dstHeightPixels * dstWidthPixels),
      						NULL,
      						&pDstMemInfo_MemWrap
      						);

	if(pDstMemInfo_MemWrap == NULL) 
	{
		SGXPERF_printf("Fatal Error in pvr2dmemwrap of %x, exiting\n", dstPtr);
		return 1;
	}

	pBlt1->DSizeX = dstWidthPixels;
	pBlt1->DSizeY = dstHeightPixels;
	pBlt1->DstX = 0;
	pBlt1->DstY = 0;

	pBlt1->pSrcMemInfo = pSrcMemInfo_MemWrap1;
	pBlt1->SrcSurfWidth = srcWidthPixels;
	pBlt1->SrcSurfHeight = srcHeightPixels;
	pBlt1->SrcFormat = PVR2D_YUV422_UYVY; //PVR2D_YUV422_VYUY; //
	pBlt1->SrcStride = srcStrideBytes;
	pBlt1->SizeX = pBlt1->DSizeX;     // what is this to be ?
	pBlt1->SizeY = pBlt1->DSizeY;
  pBlt1->CopyCode = PVR2DROPcopy;
	ePVR2DStatus1 = PVR2DBlt(hPVR2DContext1, pBlt1);		
 	ePVR2DStatus1 = PVR2DQueryBlitsComplete(hPVR2DContext1, pSrcMemInfo_MemWrap1, 1);
	SGXPERF_printf("src Blt Complete Query = %d\n", ePVR2DStatus1);
 	ePVR2DStatus1 = PVR2DQueryBlitsComplete(hPVR2DContext1, pDstMemInfo_MemWrap, 1);
	SGXPERF_printf("dst Blt Complete Query = %d\n", ePVR2DStatus1);
	//unwrap again
	//PVR2DMemFree(hPVR2DContext1, pSrcMemInfo_MemWrap1);
	//PVR2DMemFree(hPVR2DContext1, pDstMemInfo_MemWrap);
	//destroy context
  PVR2DDestroyDeviceContext(hPVR2DContext1);
  if(pDevInfo1)
    free(pDevInfo1);	

  return 0;
}

void test15(struct globalStruct *globals)
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	unsigned int i;
	//with switching contexts, still drawing to surface1
	gettimeofday(&startTime, NULL);

	void* outBuffer = malloc(globals->inTextureWidth * globals->inTextureHeight*2); //always to RGB565 mem buffer only
	if(!outBuffer) 
	{	
		printf("TEST15 FATAL error - could not allocate output buffer!\n");
		return;
	}

	for(i = 0;(i < (unsigned int)globals->numTestIterations)&&(!globals->quitSignal);i ++)	
	{	
	  SGXPERF_STARTPROFILEUNIT;	
		test15_process(
		    globals->textureData,
		    (void*)outBuffer,
		   globals->inTextureWidth,
		    globals->inTextureHeight,
		   globals->inTextureWidth*2,
		    globals->inTextureWidth >> 1, //same parameters for output as well
		    globals->inTextureHeight >> 1,
		    (globals->inTextureWidth >>1)*2,
		    2,
		    2
		    );
	  SGXPERF_ENDPROFILEUNIT;    		
	}

	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 14, diffTime2);

	//Free output buffer
	if(outBuffer) free(outBuffer);	
}
#endif


