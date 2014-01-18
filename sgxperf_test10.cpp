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

#ifdef _ENABLE_TEST10
#include "pvr2d.h"
PVR2DERROR ePVR2DStatus;
PVR2DDEVICEINFO* pDevInfo=0;
PVR2DCONTEXTHANDLE hPVR2DContext=0;
PVR2DMEMINFO* pFBMemInfo=0, *pSrcMemInfo_MemWrap = 0;
int nDeviceNum;
int nDevices;
PVR2DFORMAT eDisplayFormat;
long lStride;
int RefreshRate;
long lDisplayWidth;
long lDisplayHeight;
long lDisplayBitsPerPixel;
PPVR2DBLTINFO pBlt=0;
void test10_init()
{
	nDevices = PVR2DEnumerateDevices(0);
	pDevInfo = (PVR2DDEVICEINFO *) malloc(nDevices * sizeof(PVR2DDEVICEINFO));
	PVR2DEnumerateDevices(pDevInfo);
	nDeviceNum = pDevInfo[0].ulDevID;
	ePVR2DStatus = PVR2DCreateDeviceContext (nDeviceNum, &hPVR2DContext, 0);
	ePVR2DStatus = PVR2DGetFrameBuffer ( hPVR2DContext, PVR2D_FB_PRIMARY_SURFACE, &pFBMemInfo);
	ePVR2DStatus = PVR2DGetScreenMode(hPVR2DContext, &eDisplayFormat, &lDisplayWidth, &lDisplayHeight, &lStride, &RefreshRate);
	if(eDisplayFormat == PVR2D_ARGB8888)
		lDisplayBitsPerPixel = 32;
	else
		lDisplayBitsPerPixel = 16; //default
	memset(pFBMemInfo->pBase, 0xEF, pFBMemInfo->ui32MemSize);
	pBlt = (PVR2DBLTINFO *) calloc(1, sizeof(PVR2DBLTINFO));

    pBlt->CopyCode = PVR2DROPcopyInverted;
	pBlt->BlitFlags = PVR2D_BLIT_DISABLE_ALL;
	pBlt->pDstMemInfo = pFBMemInfo;
	pBlt->DstSurfWidth = lDisplayWidth;
	pBlt->DstSurfHeight = lDisplayHeight;
	pBlt->DstStride = lStride;
	pBlt->DstFormat = eDisplayFormat;

}
void test10(struct globalStruct *globals) //Test PVR2D test cases
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	int i; 
	void* pLocalTexture = NULL;
	test10_init();

	gettimeofday(&startTime, NULL);
	SGXPERF_printf("TESTa: Entering DrawArrays loop\n");
	for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++) //limiting to avoid memwrap issues
	{
	  SGXPERF_STARTPROFILEUNIT;	
		//Test locality of memwrap
		pLocalTexture = malloc(4 * globals->inTextureWidth * globals->inTextureHeight);
		if(!pLocalTexture) {printf("Error malloc, exit\n"); exit(1);}
		ePVR2DStatus = PVR2DMemWrap
							(hPVR2DContext, pLocalTexture, 0, 
							(4 * globals->inTextureWidth * globals->inTextureHeight),
							NULL,
							&pSrcMemInfo_MemWrap
							);
		if(pSrcMemInfo_MemWrap == NULL) 
		{
			printf("Fatal Error in pvr2dmemwrap, exiting\n");
			exit(1);
		}
		pBlt->DSizeX = globals->inTextureWidth;
		pBlt->DSizeY = globals->inTextureHeight;
		pBlt->DstX = 0;
		pBlt->DstY = 0;

		pBlt->pSrcMemInfo = pSrcMemInfo_MemWrap;
		pBlt->SrcSurfWidth = globals->inTextureWidth;
		pBlt->SrcSurfHeight = globals->inTextureHeight;
		pBlt->SrcFormat = PVR2D_ARGB8888;
		pBlt->SrcStride = ( ( ((globals->inTextureWidth + 31) & ~31) * 32) + 7) >> 3;
		pBlt->SizeX = pBlt->DSizeX;
		pBlt->SizeY = pBlt->DSizeY;
        pBlt->CopyCode = PVR2DROPcopy;
		ePVR2DStatus = PVR2DBlt(hPVR2DContext, pBlt);		
	 	ePVR2DStatus = PVR2DQueryBlitsComplete(hPVR2DContext, pFBMemInfo, 1);
		//unwrap again
		PVR2DMemFree(hPVR2DContext, pSrcMemInfo_MemWrap);
		//free texture
		if(pLocalTexture) free(pLocalTexture);
	  SGXPERF_ENDPROFILEUNIT;		
	}
	SGXPERF_printf("Exiting Draw loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 'a', diffTime2);

}
#endif //TEST10
