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

#ifdef _ENABLE_TEST14
void test14()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i, err;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	//with switching contexts, still drawing to surface1
	gettimeofday(&startTime, NULL);
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)	
	{	
	  SGXPERF_STARTPROFILEUNIT;	
	  //Switch to surface 2
	  eglMakeCurrent(eglDisplay, eglSurface2, eglSurface2, eglContext);		
	  //Switch back to surface 1 and draw to surface 1
	  eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);		
		glClear(GL_COLOR_BUFFER_BIT);		
		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
	  SGXPERF_ENDPROFILEUNIT;    		
	}
	err = glGetError();
	if(err)
		SGXPERF_ERR_printf("Error in gldraw err = %x", err);		
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(14, diffTime2);

	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}
#endif //test14