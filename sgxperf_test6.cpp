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

#ifdef _ENABLE_TEST6
#include "pvr2d.h"
typedef void *EGLImageKHR;
typedef void* GLeglImageOES;
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOES) (GLenum target, GLeglImageOES image);
typedef EGLImageKHR (*PEGLCREATEIMAGEKHR)(EGLDisplay eglDpy, 
			EGLContext eglContext, EGLenum target, EGLClientBuffer buffer, EGLint *pAttribList);
typedef EGLBoolean (*PEGLDESTROYIMAGE)(EGLDisplay eglDpy, EGLImageKHR eglImage);
#define EGL_NATIVE_PIXMAP_KHR					0x30B0	/* eglCreateImageKHR target */
PEGLCREATEIMAGEKHR peglCreateImageKHR;
PEGLDESTROYIMAGE pEGLDestroyImage;
GLeglImageOES eglImage;
PFNGLEGLIMAGETARGETTEXTURE2DOES pFnEGLImageTargetTexture2DOES;
int test6_init_eglimage_khr(NATIVE_PIXMAP_STRUCT *pNativePixmap)
{
    int err;
	//if pixmap is passed in, it is to be used as texture input
	//expected to be initialised earlier

	//Generate and Bind texture
	glGenTextures(1, &textureId0);
	glBindTexture(GL_TEXTURE_2D, textureId0);


	//get extension addresses
	peglCreateImageKHR = (PEGLCREATEIMAGEKHR)eglGetProcAddress("eglCreateImageKHR");
	if(peglCreateImageKHR == NULL)
	{
		SGXPERF_printf("eglCreateImageKHR not found!\n");
		return 2;
	}
	//create an egl image

	SGXPERF_printf("textureId0 = %d\n", textureId0); //getting 70001
	eglImage = peglCreateImageKHR(
							eglDisplay,
							EGL_NO_CONTEXT, //eglContext,
							EGL_NATIVE_PIXMAP_KHR, //EGL_GL_TEXTURE_2D_KHR,
							pNativePixmap, //(void*)textureId0,//
							NULL //miplevel 0 is fine, thank you
							);
	SGXPERF_printf("peglCreateImageKHR returned %x\n", eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGXPERF_printf("Error after peglCreateImageKHR!, error = %x\n", err);
		return 3;
	}

	//bind this to a texture
	pFnEGLImageTargetTexture2DOES = 
		(PFNGLEGLIMAGETARGETTEXTURE2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if(pFnEGLImageTargetTexture2DOES == NULL)
		SGXPERF_printf("pFnEGLImageTargetTexture2DOES not found!\n");
	pFnEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
	if((err = glGetError()) != 0)
	{
		SGXPERF_printf("Error after glEGLImageTargetTexture2DOES!, error = %x\n", err);
		return 4;
	}
	SGXPERF_printf("Destroying eglimage!\n");
	pEGLDestroyImage = (PEGLDESTROYIMAGE)eglGetProcAddress("eglDestroyImageKHR");
	//destroy the eglimage
	pEGLDestroyImage(eglDisplay, eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGXPERF_printf("Error after pEGLDestroyImage!, error = %x\n", err);
		return 5; 
	}
	return 0;
	//exit(1); //to debug
}
void test6_deinit_eglimage_khr(NATIVE_PIXMAP_STRUCT *pNativePixmap)
{
}

/* Use EGL_NATIVE_PIXMAP_KHR extension to texture instead of glteximage2d */
void test6(NATIVE_PIXMAP_STRUCT *pNativePixmap)
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i, err;
	float *pVertexArray, *pTexCoordArray;

	//initialise the vertices
	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	//initialise the eglimage based texture
	err = test6_init_eglimage_khr(pNativePixmap);
	if(err)
	{
		SGXPERF_ERR_printf("TEST6: Init failed with err = %d\n", err);
		goto deinit;
	}

	gettimeofday(&startTime, NULL);

	err = glGetError();
	if(err) 
		SGXPERF_ERR_printf("Error before gl draw loop err = %x\n", err);

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
		SGXPERF_ERR_printf("Error in gl draw loop err = %x\n", err);
	SGXPERF_printf("Exiting DrawArrays loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(6, diffTime2);

deinit:
	//deinit the eglimage
	test6_deinit_eglimage_khr(pNativePixmap);
	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}
#endif

