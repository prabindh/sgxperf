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

#ifdef _ENABLE_TEST1 
/* Fill entire screen with single colour, no objects */
void test1()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i;
	gettimeofday(&startTime, NULL);
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	
	  SGXPERF_STARTPROFILEUNIT;	
		glClearColor(0.2f, 0.4f, 0.8f, 1.0f); // clear blue
		glClear(GL_COLOR_BUFFER_BIT);
		common_eglswapbuffers(eglDisplay, eglSurface);
    SGXPERF_ENDPROFILEUNIT;		
	}
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(1, diffTime2);
}
#endif

