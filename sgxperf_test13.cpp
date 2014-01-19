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

#ifdef _ENABLE_TEST13
#define NUM_LINES_TEST13 10000 //1000 lines test
/* Draw multiple lines */
void test13(struct globalStruct *globals)
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	int i;
	GLfloat vertices[NUM_LINES_TEST13*4];

	//set up vertices for all the lines for 1000 lines
	for (int i = 0; i < NUM_LINES_TEST13; i += 4) 
	{
	    vertices[i] = 0.0;
	    vertices[i+1] = 0.0;
	    vertices[i+2] = cos((float)i * 2*3.141592653589/ (float)NUM_LINES_TEST13 );
	    vertices[i+3] = sin((float)i * 2*3.141592653589/ (float)NUM_LINES_TEST13 );	    
	}
	
	glEnableVertexAttribArray(VERTEX_ARRAY);
	glVertexAttribPointer(VERTEX_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, (const void*)vertices);
	
	glClearColor(0.2f, 0.4f, 0.8f, 1.0f); // clear blue
	gettimeofday(&startTime, NULL);	
	for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++)
	{	
//		SGXPERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);		
		glDrawArrays(GL_LINES, 0, NUM_LINES_TEST13);
		common_eglswapbuffers(globals, globals->eglDisplay, globals->eglSurface);
//		SGXPERF_ENDPROFILEUNIT;		
	}
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 13, diffTime2);
}
#endif
