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

#ifdef _ENABLE_TEST3
/* Texturing using glteximage2d */ 
void test3(struct globalStruct *globals)
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	int i;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(globals->inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(globals->inNumberOfObjectsPerSide, &pTexCoordArray);

	gettimeofday(&startTime, NULL);
	for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++)	
	{
	  SGXPERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		//draw first area with texture
		common_gl_draw(globals, globals->inNumberOfObjectsPerSide);
		common_eglswapbuffers(globals, globals->eglDisplay, globals->eglSurface);
SGXPERF_ENDPROFILEUNIT		
	}
		
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 3, diffTime2);

	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}
#endif


