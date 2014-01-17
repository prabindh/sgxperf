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

#ifdef _ENABLE_TEST7

#ifndef _ENABLE_TEST6 //test6 already defines these
#include "pvr2d.h"
typedef void *EGLImageKHR;
typedef void* GLeglImageOES;
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOES) (GLenum target, GLeglImageOES image);
typedef EGLImageKHR (*PEGLCREATEIMAGEKHR)(EGLDisplay eglDpy, 
			EGLContext eglContext, EGLenum target, EGLClientBuffer buffer, EGLint *pAttribList);
typedef EGLBoolean (*PEGLDESTROYIMAGE)(EGLDisplay eglDpy, EGLImageKHR eglImage);
PEGLCREATEIMAGEKHR peglCreateImageKHR;
PEGLDESTROYIMAGE pEGLDestroyImage;
GLeglImageOES eglImage;
PFNGLEGLIMAGETARGETTEXTURE2DOES pFnEGLImageTargetTexture2DOES;
#endif //ifndef test6
#define EGL_GL_TEXTURE_2D_KHR					0x30B1	/* eglCreateImageKHR target */
int test7_init_eglimage_khr()
{
    int err;
	//do a teximage2d to ensure things are inited fine - TODO: Check with IMG
	// Create and Bind texture
	glGenTextures(1, &textureId0);
	glBindTexture(GL_TEXTURE_2D, textureId0);
	add_texture(inTextureWidth, inTextureHeight, textureData, inPixelFormat);

	//get extension addresses
	peglCreateImageKHR = (PEGLCREATEIMAGEKHR)eglGetProcAddress("eglCreateImageKHR");
	if(peglCreateImageKHR == NULL)
		SGXPERF_printf("eglCreateImageKHR not found!\n");
	//create an egl image

	SGXPERF_printf("textureId0 = %d\n", textureId0); //getting 70001
	eglImage = peglCreateImageKHR(
							eglDisplay,
							eglContext,
							EGL_GL_TEXTURE_2D_KHR,
							(void*)textureId0,//
							NULL //miplevel 0 is fine, thank you
							);
	SGXPERF_printf("peglCreateImageKHR returned %x\n", eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGXPERF_printf("Error after peglCreateImageKHR!, error = %x\n", err);
		return 1;
	}

	//bind this to a texture
	pFnEGLImageTargetTexture2DOES = 
		(PFNGLEGLIMAGETARGETTEXTURE2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if(pFnEGLImageTargetTexture2DOES == NULL)
	{
		SGXPERF_printf("pFnEGLImageTargetTexture2DOES not found!\n");
		return 2;
	}
	pFnEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGXPERF_printf("Error after glEGLImageTargetTexture2DOES!, error = %x\n", err);
		return 3;
	}
	SGXPERF_printf("Destroying eglimage!\n");
	pEGLDestroyImage = (PEGLDESTROYIMAGE)eglGetProcAddress("eglDestroyImageKHR");
	//destroy the eglimage
	pEGLDestroyImage(eglDisplay, eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGXPERF_printf("Error after pEGLDestroyImage!, error = %x\n", err);
		return 4;
	}

	return 0;
	//exit(1); //to debug
}
void test7_deinit_eglimage_khr()
{
}

/* Use EGL_GL_TEXTURE_2D_KHR extension to texture instead of glteximage2d */
void test7()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int err;
	int i;

	//initialise gl draw states
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	//initialise the eglimage based texture
	err = test7_init_eglimage_khr();
	if(err)
	{
		SGXPERF_ERR_printf("TEST7: Init failed with err = %d", err);
		goto deinit;
	}

	gettimeofday(&startTime, NULL);
	SGXPERF_printf("Entering DrawArrays loop\n");
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGXPERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		//texture will be already bound using the eglimage extension, no calling glteximage2d. How nice !!
		if(i & 1)
		{
			//ensure that texture is setup - Is there any other way of doing this ?
			//memset((void*)nativePixmap.lAddress, 0, nativePixmap.lSizeInBytes);
		}
		else
		{
			//memset((void*)nativePixmap.lAddress, 5, nativePixmap.lSizeInBytes);
		}
		eglimage_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
SGXPERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err) 
		SGXPERF_printf("Error in gl draw loop err = %d\n", err);
	SGXPERF_printf("Exiting DrawArrays loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(7, diffTime2);

deinit:
	glDisable(GL_TEXTURE_2D);
	//deinit the eglimage
	test7_deinit_eglimage_khr();
	common_deinit_gl_vertices(pVertexArray);
}
#endif //test7

