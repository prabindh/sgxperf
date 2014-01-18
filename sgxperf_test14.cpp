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
void test14(struct globalStruct *globals)
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	int i, err;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(globals->inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(globals->inNumberOfObjectsPerSide, &pTexCoordArray);

	//with switching contexts, still drawing to surface1
	gettimeofday(&startTime, NULL);
	for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++)	
	{	
	  SGXPERF_STARTPROFILEUNIT;	
	  //Switch to surface 2
	  eglMakeCurrent(globals->eglDisplay, globals->eglSurface2, globals->eglSurface2, globals->eglContext);		
	  //Switch back to surface 1 and draw to surface 1
	  eglMakeCurrent(globals->eglDisplay, globals->eglSurface, globals->eglSurface, globals->eglContext);		
		glClear(GL_COLOR_BUFFER_BIT);		
		common_gl_draw(globals, globals->inNumberOfObjectsPerSide);
		common_eglswapbuffers(globals, globals->eglDisplay, globals->eglSurface);
	  SGXPERF_ENDPROFILEUNIT;    		
	}
	err = glGetError();
	if(err)
		SGXPERF_ERR_printf("Error in gldraw err = %x", err);		
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 14, diffTime2);

	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}
#endif //test14
