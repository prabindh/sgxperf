/******************************************************************************

Test file for validating throughputs for 2D related operations, using 
the openGLES and PVR2D APIs

View the README and INSTALL files for usage and build instructions

Latest code and information can be obtained from
http://github.com/prabindh/sgxperf

XgxPerf (a Qt5 C++ based 2D benchmarking tool) can be obtained from 
https://github.com/prabindh/xgxperf

Prabindh Sundareson prabu@ti.com
(c) Texas Instruments 2009-2014
******************************************************************************/
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


#ifdef _ENABLE_TEST16
typedef void *EGLImageKHR;
typedef void* GLeglImageOES;
PFNEGLCREATEIMAGEKHRPROC peglCreateImageKHR;
PFNEGLDESTROYIMAGEKHRPROC pEGLDestroyImage;
EGLImageKHR eglImage;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pFnEGLImageTargetTexture2DOES;

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])


#ifndef EGL_TI_raw_video
#  define EGL_TI_raw_video 1
#  define EGL_RAW_VIDEO_TI					0x333A	/* eglCreateImageKHR target */
#  define EGL_GL_VIDEO_FOURCC_TI				0x3331	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_WIDTH_TI					0x3332	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_HEIGHT_TI				0x3333	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_BYTE_STRIDE_TI			0x3334	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_BYTE_SIZE_TI				0x3335	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_YUV_FLAGS_TI				0x3336	/* eglCreateImageKHR attribute */
#endif

#ifndef EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE
#  define EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE (0 << 0)
#  define EGLIMAGE_FLAGS_YUV_FULL_RANGE       (1 << 0)
#  define EGLIMAGE_FLAGS_YUV_BT601            (0 << 1)
#  define EGLIMAGE_FLAGS_YUV_BT709            (1 << 1)
#endif

EGLint eglImageAttributes[] = {
	//NV12 is ridiculously slow
	EGL_GL_VIDEO_FOURCC_TI,      FOURCC_STR("YUYV"),
	EGL_GL_VIDEO_WIDTH_TI,       0,
	EGL_GL_VIDEO_HEIGHT_TI,      0,
	EGL_GL_VIDEO_BYTE_STRIDE_TI, 0,
	EGL_GL_VIDEO_BYTE_SIZE_TI,   0,
	// TODO: pick proper YUV flags..
	EGL_GL_VIDEO_YUV_FLAGS_TI,   EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE |
		EGLIMAGE_FLAGS_YUV_BT601,
	EGL_NONE
    };

void test16(struct globalStruct *globals)
{
	int i, err;
        timeval startTime, endTime;
        unsigned long diffTime2;

	eglImageAttributes[3] = globals->inTextureWidth;
	eglImageAttributes[5] = globals->inTextureHeight;
	eglImageAttributes[7] = globals->inTextureWidth * 2;
	eglImageAttributes[9] = globals->inTextureWidth*globals->inTextureHeight*2;

	peglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
	pFnEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	pEGLDestroyImage = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
	//Create the eglimage
	eglImage = peglCreateImageKHR(globals->eglDisplay, EGL_NO_CONTEXT, EGL_RAW_VIDEO_TI, globals->textureData, eglImageAttributes);
	if(eglImage == EGL_NO_IMAGE_KHR)
	{
		SGXPERF_ERR_printf("EGLImage not created, err = %x\n", eglGetError());
		return;
	}
	
	//Create the texture
	glGenTextures(1, &globals->textureId0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, globals->textureId0);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//Specify the target
	pFnEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);
	//Draw loop
        float *pVertexArray, *pTexCoordArray;
        common_init_gl_vertices(globals->inNumberOfObjectsPerSide, &pVertexArray);
        common_init_gl_texcoords(globals->inNumberOfObjectsPerSide, &pTexCoordArray);


       gettimeofday(&startTime, NULL);
        for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++)
	{
                eglimage_gl_draw(globals->inNumberOfObjectsPerSide);
                common_eglswapbuffers(globals, globals->eglDisplay, globals->eglSurface);
        	err = glGetError();
	        if(err)
		{
	                SGXPERF_ERR_printf("Error in gldraw err = %x\n", err);
		}
	        err = eglGetError();
	        if(err != 0x3000)
		{
	                SGXPERF_ERR_printf("Error in egl err = %x\n", err);
		}
	}
        gettimeofday(&endTime, NULL);
        diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
        common_log(globals, 1, diffTime2);


        common_deinit_gl_vertices(pVertexArray);
        common_deinit_gl_texcoords(pTexCoordArray);
	
	pEGLDestroyImage(globals->eglDisplay, eglImage);
        SGXPERF_ERR_printf("INFO: finished test16 - running at max fps\n");
}
#endif
