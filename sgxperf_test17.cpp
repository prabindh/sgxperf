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

#ifdef _ENABLE_TEST17
void gl_fbo_draw(int numObjects)
{
	int startIndex = 0;
	
	for(int vertical = 0;vertical < numObjects;vertical ++)
		for(int horizontal = 0;horizontal < numObjects;horizontal ++)
		{
			glDrawArrays(GL_TRIANGLE_STRIP, startIndex, 4);
			startIndex += 4;
		}	
}

/* Texturing using glteximage2d */ 
void _test17(struct globalStruct *globals, int objPerSide, int offscreen)
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(objPerSide, &pVertexArray);
	common_init_gl_texcoords(objPerSide, &pTexCoordArray);

	gettimeofday(&startTime, NULL);

	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
	//draw first area with texture
	GL_CHECK(gl_fbo_draw(objPerSide));
	if(offscreen)
		glFlush();
	else
		common_eglswapbuffers(globals, globals->eglDisplay, globals->eglSurface);

	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 17, diffTime2);

	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}

#define NUM_FBO 1

void test17(struct globalStruct *globals)
{
	int i;
	GLuint fboId[NUM_FBO];
	GLuint fboTextureId[NUM_FBO];
	GLuint regularTextureId;
	int numObjectsPerSide = globals->inNumberOfObjectsPerSide;

	//sanity checks
	if(globals->inPixelFormat != SGXPERF_ARGB8888)
	{
		SGXPERF_ERR_printf("TEST17 can currently only be run with ARGB888!");
		exit(-1);
	}

	glGenFramebuffers(NUM_FBO, fboId);
	//fbo texture
	glGenTextures(NUM_FBO, fboTextureId);
	//Regular first texture
	glGenTextures(1, &regularTextureId);

	if(numObjectsPerSide > NUM_FBO) numObjectsPerSide = NUM_FBO;

	for(i = 0;i < numObjectsPerSide;i ++)
	{
		//Bind offscreen texture
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, fboTextureId[i]));
		GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, globals->inTextureWidth, globals->inTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[i]));
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTextureId[i], 0));

		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("FB is not complete for rendering offscreen\n");
			goto err;
		}
		//Bind regular texture
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, regularTextureId));
		add_texture(globals->inTextureWidth, globals->inTextureHeight, globals->textureData, globals->inPixelFormat);
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		 //Draw with regular draw calls to FBO
		GL_CHECK(_test17(globals, numObjectsPerSide, 1));

		//Now get back display framebuffer and unbind the FBO
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("FB is not complete for rendering to display\n");
			goto err;
		}
		//bind to texture
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, fboTextureId[i]));

		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		//draw to display buffer
		_test17(globals, numObjectsPerSide, 0);
	}
err:  
	glDeleteFramebuffers(NUM_FBO, fboId);
	glDeleteTextures(NUM_FBO, fboTextureId);  
	glDeleteTextures(1, &regularTextureId);  
}
#endif
