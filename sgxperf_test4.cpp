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
#ifdef _ENABLE_TEST4
/* Draw frames WITH texturing one above other with blending */
void test4(struct globalStruct *globals)
{
	timeval startTime, endTime;
	unsigned long diffTime2;
	int i;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(globals->inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(globals->inNumberOfObjectsPerSide, &pTexCoordArray);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClear(GL_COLOR_BUFFER_BIT);
	gettimeofday(&startTime, NULL);
	for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++)
	{
	  SGXPERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		//some nonsense will come on screen as Alpha data is invalid for the sample
		common_gl_draw(globals, globals->inNumberOfObjectsPerSide);
		common_eglswapbuffers(globals, globals->eglDisplay, globals->eglSurface);
SGXPERF_ENDPROFILEUNIT		
	}

	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(globals, 4, diffTime2);

	glDisableVertexAttribArray(VERTEX_ARRAY);
	glDisableVertexAttribArray(TEXCOORD_ARRAY);
	glDisable(GL_BLEND);
	common_deinit_gl_vertices(pVertexArray);

}
#endif
