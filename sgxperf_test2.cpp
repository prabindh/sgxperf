
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
#ifdef _ENABLE_TEST2
		/* Draw a coloured rectangle filling entire screen */
void test2()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i;
	float *pVertexArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);

	gettimeofday(&startTime, NULL);

	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGXPERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);

		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
  SGXPERF_ENDPROFILEUNIT		
	}
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	

	common_deinit_gl_vertices(pVertexArray);
	common_log(2, diffTime2);
}
#endif

