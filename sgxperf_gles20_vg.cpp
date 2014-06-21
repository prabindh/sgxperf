/******************************************************************************

Test file for validating throughputs for 2D related operations, using 
the openGLES and PVR2D.

View the README and INSTALL files for usage and build instructions

v0.1 - 26 feb 09
v0.2 - 30 Jul 09
v0.21 - 4 Aug 09
v0.22 - 6 Aug 09 [Added SVG support (needs SDK changes pvrt3d]
v0.23 - Minor updates
v0.24 - Added IMGtexturestream
v0.25 - Fixed eglimage, Added multiobject texturing
v0.26 - Added pixmap surface and loopback to window
v0.28 - Moved to support '3_01_00_02' release of TI Graphics SDK, added PVR2D
v0.29 - Separated pixmapsurfacetype and texturetype  
v0.30 - Added Edge detection code
v0.31 - Made it compatible with xgfxperf
v0.32 - Added Line drawing test (test13)
v0.33 - Added context switch (test14)
v0.34 - Added PVR2D YUV-RGB (test15)
v0.35 - Jun 2013 - Added eglImage-YUV tests (test16)
v0.36 - Dec 2013 - Added FBO tests (test17)

Latest code and information can be obtained from
http://github.com/prabindh/sgxperf

XgxPerf (a Qt5 C++ based 2D benchmarking tool) can be obtained from 
https://github.com/prabindh/xgxperf

Prabindh Sundareson prabu@ti.com
(c) Texas Instruments 2013
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

#ifdef _ENABLE_CMEM
#include <cmem.h> //for contiguous physical memory allocation. Always used.
#endif //CMEM

/** GLOBALS **/
struct globalStruct gTest;

bool TestEGLError(const char* pszLocation)
{
	EGLint iErr = eglGetError();
	if (iErr != EGL_SUCCESS)
	{
		SGXPERF_ERR_printf("%s failed (%d).\n", pszLocation, iErr);
		return false;
	}

	return true;
}
int dummy_printf(const char* format, ...)
{
	return 0;
}

static void initialise_globals(struct globalStruct *in)
{
	memset(in, 0, sizeof(globalStruct));
	in->inNumberOfObjectsPerSide = 1;
	in->inFPS=1;
}

/******************************************************
* Function to create an ARGB texture
* pixelformat can be different from the display format
*******************************************************/
void set_texture(int width, int height, unsigned char* pTex,
					int pixelFormat)
{
	int numbytes=0;
	if(pixelFormat == SGXPERF_ARGB8888) numbytes = 4; //ARGB888
	if(pixelFormat == SGXPERF_RGB565) numbytes = 2; //RGB565
	if(pixelFormat == SGXPERF_BYTE8) numbytes = 1; //LUMINANCE

	for(int i = 0;i < width*height*numbytes;i ++)
		pTex[i] = i;
}

/*************************************************
* The traditional way of texturing - glteximage2d
*************************************************/
void add_texture(int width, int height, void* data, int pixelFormat)
{
	int textureType = GL_RGBA;
	if(pixelFormat == SGXPERF_RGB565) textureType = GL_RGB;
	else if(pixelFormat == SGXPERF_ARGB8888) textureType = GL_RGBA;
	else if(pixelFormat == SGXPERF_BYTE8) textureType = GL_LUMINANCE;
	else {SGXPERF_ERR_printf("%d texture format unsupported\n", pixelFormat); exit(-1);}
	// load the texture up
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	//SGXPERF_ERR_printf("\n\nTextureType=%d\n",textureType);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		textureType,//GL_RGBA,//textureType,
		width,
		height,
		0,
		textureType,//GL_RGBA,//textureType,
		GL_UNSIGNED_BYTE,//textureFormat,
		data
		);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


void common_create_native_pixmap(
								 unsigned long pixmapFormat,
								 unsigned long pixmapwidth,
								 unsigned long pixmapHeight,
								 NATIVE_PIXMAP_STRUCT **pNativePixmapPtr
								 )
{
#ifdef _ENABLE_CMEM
	//The cmem module should be inserted before running this application
	//Create a contiguous buffer of required size for texture, and get userspace address
	*pNativePixmapPtr = (NATIVE_PIXMAP_STRUCT*)malloc(sizeof(NATIVE_PIXMAP_STRUCT));

  if(pixmapFormat == SGXPERF_RGB565)
    (*pNativePixmapPtr)->ePixelFormat = 0;
  else if(pixmapFormat == SGXPERF_ARGB8888)
    (*pNativePixmapPtr)->ePixelFormat = 2;
  else           
  {  
		SGXPERF_ERR_printf("Invalid pixmap format type %ld\n", pixmapFormat);
		exit(-1);
  }
    (*pNativePixmapPtr)->eRotation = 0;
    (*pNativePixmapPtr)->lWidth = pixmapwidth;//480;
    (*pNativePixmapPtr)->lHeight = pixmapHeight;//640; 
	if(pixmapFormat == SGXPERF_RGB565)    
		(*pNativePixmapPtr)->lStride = (*pNativePixmapPtr)->lWidth* 16/8; //bitwidth/8
	else if(pixmapFormat == SGXPERF_ARGB8888)
		(*pNativePixmapPtr)->lStride = (*pNativePixmapPtr)->lWidth* 32/8; //bitwidth/8
    (*pNativePixmapPtr)->lSizeInBytes = (*pNativePixmapPtr)->lHeight * (*pNativePixmapPtr)->lStride;
    (*pNativePixmapPtr)->lAddress = (long) CMEM_alloc((*pNativePixmapPtr)->lSizeInBytes, NULL);
	if(!(*pNativePixmapPtr)->lAddress)
	{
		SGXPERF_ERR_printf("CMEM_alloc returned NULL\n");
		exit(-1);
	}
    //Get the physical page corresponding to the above cmem buffer
    (*pNativePixmapPtr)->pvAddress = CMEM_getPhys((void*)(*pNativePixmapPtr)->lAddress);
	SGXPERF_printf("Physical address = %x\n", (*pNativePixmapPtr)->pvAddress);
	if((*pNativePixmapPtr)->pvAddress & 0xFFF)
		SGXPERF_printf("PVR2DMemWrap may have issues with this non-aligned address!\n");
#endif
}
void common_delete_native_pixmap(
			NATIVE_PIXMAP_STRUCT *pNativePixmap)
{
#ifdef _ENABLE_CMEM
	if(!pNativePixmap) return;
	if(pNativePixmap->lAddress)
		CMEM_free((void*)pNativePixmap->lAddress, NULL);
	//delete the pixmap itself
	free(pNativePixmap);
#endif //ENABLE_CMEM
}

//egl init
int common_eglinit(struct globalStruct* globals, int testID, int surfaceType, NATIVE_PIXMAP_STRUCT** pNativePixmapPtr)
{
	EGLint iMajorVersion, iMinorVersion;
	EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	globals->eglDisplay = eglGetDisplay((int)0);

	if (!eglInitialize(globals->eglDisplay, &iMajorVersion, &iMinorVersion))
		return 1;

	eglBindAPI(EGL_OPENGL_ES_API);
	if (!TestEGLError("eglBindAPI"))
		return 1;

	EGLint pi32ConfigAttribs[5];
	pi32ConfigAttribs[0] = EGL_SURFACE_TYPE;
	pi32ConfigAttribs[1] = EGL_WINDOW_BIT | EGL_PIXMAP_BIT;
	pi32ConfigAttribs[2] = EGL_RENDERABLE_TYPE;
	pi32ConfigAttribs[3] = EGL_OPENGL_ES2_BIT;
	pi32ConfigAttribs[4] = EGL_NONE;

	int iConfigs;
	if (!eglChooseConfig(globals->eglDisplay, pi32ConfigAttribs, &globals->eglConfig, 1, &iConfigs) || (iConfigs != 1))
	{
		SGXPERF_ERR_printf("Error: eglChooseConfig() failed.\n");
		return 1;
	}
	if(surfaceType == SGXPERF_SURFACE_TYPE_WINDOW)
		globals->eglSurface = eglCreateWindowSurface(globals->eglDisplay, globals->eglConfig, (EGLNativeWindowType) NULL, NULL);
	else if(surfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16)
	{
		common_create_native_pixmap(SGXPERF_RGB565, 
						globals->inTextureWidth, globals->inTextureHeight, pNativePixmapPtr);
		globals->eglSurface = eglCreatePixmapSurface(globals->eglDisplay, globals->eglConfig, (EGLNativePixmapType)*pNativePixmapPtr, NULL);
	}
	else if(surfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32)
	{
		common_create_native_pixmap(SGXPERF_ARGB8888, 
						globals->inTextureWidth, globals->inTextureHeight, pNativePixmapPtr);
		globals->eglSurface = eglCreatePixmapSurface(globals->eglDisplay, globals->eglConfig, (EGLNativePixmapType)*pNativePixmapPtr, NULL);
	}
  	else  
	    return 999;

	if (!TestEGLError("eglCreateSurface"))
		return 1;
		
	if(testID == 14) //Create one pixmap surface for context switch latency check
	{
		common_create_native_pixmap(SGXPERF_RGB565, 
						globals->inTextureWidth, globals->inTextureHeight, pNativePixmapPtr);
		globals->eglSurface2 = eglCreatePixmapSurface(globals->eglDisplay, globals->eglConfig, (EGLNativePixmapType)*pNativePixmapPtr, NULL);
	}  	
	if (!TestEGLError("eglCreateSurface"))
		return 1;		

	globals->eglContext = eglCreateContext(globals->eglDisplay, globals->eglConfig, NULL, ai32ContextAttribs);
	if (!TestEGLError("eglCreateContext"))
		return 1;

	eglMakeCurrent(globals->eglDisplay, globals->eglSurface, globals->eglSurface, globals->eglContext);
	if (!TestEGLError("eglMakeCurrent"))
		return 1;

	eglSwapInterval(globals->eglDisplay, 1);
	if (!TestEGLError("eglSwapInterval"))
		return 1;

	eglQuerySurface(globals->eglDisplay, globals->eglSurface, EGL_WIDTH, &globals->windowWidth);
	eglQuerySurface(globals->eglDisplay, globals->eglSurface, EGL_HEIGHT, &globals->windowHeight);
	
	SGXPERF_printf("Window width=%d, Height=%d\n", globals->windowWidth, globals->windowHeight);

	return 0;
}
void common_egldeinit(struct globalStruct *globals, int testID, NATIVE_PIXMAP_STRUCT* pNativePixmap)
{
	eglMakeCurrent(globals->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ;
	if(pNativePixmap)
		common_delete_native_pixmap(pNativePixmap);
	eglDestroyContext(globals->eglDisplay, globals->eglContext);
	eglDestroySurface(globals->eglDisplay, globals->eglSurface);
	if(testID == 14)
	  eglDestroySurface(globals->eglDisplay, globals->eglSurface2);	
	eglTerminate(globals->eglDisplay);
}

//swapping buffers
void common_eglswapbuffers(
			struct globalStruct *globals,
						   EGLDisplay eglDisplay, 
						   EGLSurface eglSurface
						   )
{
	if(globals->inSurfaceType == SGXPERF_SURFACE_TYPE_WINDOW)
		eglSwapBuffers(eglDisplay, eglSurface);
	else if(globals->inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16 || globals->inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32)
		eglWaitGL();
}
//Vertices init
int common_init_gl_vertices(int numObjectsPerSide, GLfloat **vertexArray)
{
	GLfloat currX = -1, currY = 1, currZ = -1; //set to top left
	GLfloat objWidth = 2/(float)numObjectsPerSide, objHeight = 2/(float)numObjectsPerSide;

	GLfloat *afVertices = (GLfloat*)malloc(4*3*sizeof(GLfloat)*numObjectsPerSide*numObjectsPerSide);
	*vertexArray = NULL;
	if(afVertices == NULL)
		return 1;
	*vertexArray = afVertices;	
	for(int vertical = 0;vertical < numObjectsPerSide; vertical ++)
	{
		for(int horizontal = 0;horizontal < numObjectsPerSide; horizontal ++)
		{
			//p0
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3] = currX;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 1] = currY;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 2] = currZ;
			//p1
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 3] = currX;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 4] = currY - objHeight;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 5] = currZ;
			//p2
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 6] = currX + objWidth;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 7] = currY;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 8] = currZ;
			//p3
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 9] = currX + objWidth;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 10] = currY - objHeight;
			afVertices[vertical*4*3*numObjectsPerSide + horizontal*4*3 + 11] = currZ;

			currX += objWidth;
		}
		currX = -1; //reset
		currY -= objHeight;
	}
	//for(int temp = 0;temp < 4*numObjectsPerSide*2;temp ++)
	//	printf("vertex=%f %f %f\n", afVertices[temp], afVertices[temp + 1], afVertices[temp + 2] );

	glEnableVertexAttribArray(VERTEX_ARRAY);
	GL_CHECK(glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, (const void*)afVertices));
	return 0;
}

void common_deinit_gl_vertices(GLfloat *vertexArray)
{
	glDisableVertexAttribArray(VERTEX_ARRAY);
	if(vertexArray)
		free(vertexArray);
}

void common_deinit_gl_texcoords(GLfloat *pTexCoordArray)
{
	glDisableVertexAttribArray(TEXCOORD_ARRAY);
	if(pTexCoordArray)
		free(pTexCoordArray);
}

//Texture Coordinates
int common_init_gl_texcoords(int numObjectsPerSide, GLfloat **textureCoordArray)
{//full span
	GLfloat currX = -1, currY = 1; //set to top left
	GLfloat objWidth = 2/(float)numObjectsPerSide, objHeight = 2/(float)numObjectsPerSide;

	GLfloat *afVertices = (GLfloat*)malloc(4*2*sizeof(GLfloat)*numObjectsPerSide*numObjectsPerSide);
	*textureCoordArray = NULL;
	if(afVertices == NULL)
		return 1;
	*textureCoordArray = afVertices;	
	for(int vertical = 0;vertical < numObjectsPerSide; vertical ++)
	{
		for(int horizontal = 0;horizontal < numObjectsPerSide; horizontal ++)
		{
			//p0
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2] = 0;
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 1] = 1;
			//p1
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 2] = 0;
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 3] = 0;
			//p2
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 4] = 1;
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 5] = 1;
			//p3
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 6] = 1;
			afVertices[vertical*4*2*numObjectsPerSide + horizontal*4*2 + 7] = 0;

			currX += objWidth;
		}
		currX = -1; //reset
		currY -= objHeight;
	}
	//for(int temp = 0;temp < 4*numObjectsPerSide*2;temp ++)
	//	printf("vertex=%f %f\n", afVertices[temp], afVertices[temp + 1]);

	glEnableVertexAttribArray(TEXCOORD_ARRAY);
	GL_CHECK(glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, (const void*)afVertices));


	return 0;
}
//Common function to draw the previously input vertices
//Texture mapping if needed, has to be done before
void common_gl_draw(struct globalStruct *globals, int numObjects)
{
	int startIndex = 0;
	
	for(int vertical = 0;vertical < numObjects;vertical ++)
		for(int horizontal = 0;horizontal < numObjects;horizontal ++)
		{
			{
				// Create and Bind texture
				glGenTextures(1, &globals->textureId0);
				glBindTexture(GL_TEXTURE_2D, globals->textureId0);
				add_texture(globals->inTextureWidth, globals->inTextureHeight, globals->textureData, globals->inPixelFormat);
			}
			glDrawArrays(GL_TRIANGLE_STRIP, startIndex, 4);
			{
				glDeleteTextures(1, &globals->textureId0);
			}
			
			startIndex += 4;
		}
}
void eglimage_gl_draw(int numObjects)
{
	int startIndex = 0;

	for(int vertical = 0;vertical < numObjects;vertical ++)
		for(int horizontal = 0;horizontal < numObjects;horizontal ++)
		{
			// Create and Bind texture
			//glGenTextures(1, &textureId0);
			//glBindTexture(GL_TEXTURE_2D, textureId0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			//update texture here if needed
			//memset(textureData);
			glDrawArrays(GL_TRIANGLE_STRIP, startIndex, 4);
			//glDeleteTextures(1, &textureId0);
			startIndex += 4;
		}
}

void img_stream_gl_draw(int numObjects)
{
	int startIndex = 0;

	for(int vertical = 0;vertical < numObjects;vertical ++)
		for(int horizontal = 0;horizontal < numObjects;horizontal ++)
		{
			//update texture here if needed
			//memset(textureData);
			glDrawArrays(GL_TRIANGLE_STRIP, startIndex, 4);			
			startIndex += 4;
		}
}
unsigned long
tv_diff(struct timeval *tv1, struct timeval *tv2)
{
    return (tv2->tv_sec - tv1->tv_sec) * 1000 +
        (tv2->tv_usec - tv1->tv_usec) / 1000;
}

void common_log(struct globalStruct *globals, int testid, unsigned long time)
{
	SGXPERF_ERR_printf("id\twidth\theight\trotation\tpixelformat\tsurface\tnumobjects\ttimeper_frame(ms)\n");
	SGXPERF_ERR_printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%ld\n", 
							testid, 
							globals->inTextureWidth, 
							globals->inTextureHeight, 
							globals->inRotationEnabled, 
							globals->inPixelFormat,
					              globals->inSurfaceType,
							globals->inNumberOfObjectsPerSide,
							time);		
}


/****************************************************
* TESTS START HERE
*****************************************************/

/* Signal handler for clean closure of GL */
void sgxperf_signal_handler(int reason) 
{ 
  SGXPERF_ERR_printf("\nGot quit signal - Results will be inaccurate!\n"); 
  gTest.quitSignal = 1; 
}



/*!****************************************************************************
 @Function		main
 @Input			argc		Number of arguments
 @Input			argv		Command line arguments
 @Return		int			result code to OS
 @Description	Main function of the program
******************************************************************************/
int main(int argc, char **argv)
{
	int testID = 0, err; //default
	char *extensions;
	unsigned int delta;
	NATIVE_PIXMAP_STRUCT* pNativePixmap = NULL;


	/* Initialise all globals */
	initialise_globals(&gTest);

	/* Pre init step - check the arguments */
	if(argc < 11)
	{
		SGXPERF_ERR_printf("Error: Invalid number of operands \n\n");
		SGXPERF_ERR_printf("%s",helpString);
		exit(-1);
	}
	testID = atol(argv[1]);
	if(testID > MAX_TEST_ID || testID < 0)
	{
		SGXPERF_ERR_printf("Error: No test available with this ID %d\n\n", testID);
		SGXPERF_ERR_printf("%s",helpString);
		exit(-1);
	}
	gTest.inNumberOfObjectsPerSide = atol(argv[7]);
	gTest.inSurfaceType = atol(argv[8]);

	if(argc < 4)
	{
		if(testID > 2) //1 and 2 do not need textures
		{
			SGXPERF_ERR_printf("Error: Invalid number of operands for selected testID %d\n\n", testID);
			SGXPERF_ERR_printf("%s",helpString);
			exit(-1);
		}
	}
	else
	{
		gTest.inTextureWidth = atol(argv[2]);
		gTest.inTextureHeight = atol(argv[3]);
	}
	gTest.inRotationEnabled = 0;
	//Rotation is unused in latest version
	if(argc >= 5)
		gTest.inRotationEnabled = atol(argv[4]);
	gTest.inPixelFormat = 0;
	if(argc >= 6)
		gTest.inPixelFormat = atol(argv[5]);
	if(gTest.inPixelFormat != SGXPERF_RGB565 && gTest.inPixelFormat != SGXPERF_ARGB8888 && gTest.inPixelFormat != SGXPERF_BYTE8)
	{
		SGXPERF_ERR_printf("Error: Unsupported pixel format for texture %d \n\n", gTest.inPixelFormat);
		SGXPERF_ERR_printf("%s",helpString);
		exit(-1);
	}
  
  //read extra params
  gTest.numTestIterations = atol(argv[9]);
  gTest.inFPS =atol(argv[10]); 
  gTest.msToSleep = 1000/gTest.inFPS;
  gTest.cookie = argv[11];
    
	if((gTest.inTextureWidth > 8000) || (gTest.inTextureHeight > 8000))
	{
		SGXPERF_ERR_printf("Error: Width or Height exceeds 8000 \n\n");
		SGXPERF_ERR_printf("%s",helpString);
		exit(-1);
	}
#ifndef _ENABLE_CMEM
	if(testID == 6 || testID == 7 || gTest.inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16 || 
                                    gTest.inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32)
	{
		SGXPERF_ERR_printf("ERROR: Cannot run native pixmap tests without CMEM\n");
		exit(-1);
	}
#endif
#ifndef _ENABLE_BUFFERCLASS
	if(testID == 8)
	{
		SGXPERF_ERR_printf("ERROR: Cannot run test8 without BUFFERCLASS driver\n");
		exit(-1);
	}
#endif
	//for pixmap related tests, surface cannot be pixmap
	if(testID == 6 && gTest.inSurfaceType != SGXPERF_SURFACE_TYPE_WINDOW)
	{
		SGXPERF_ERR_printf("ERROR: Cannot run native pixmap eglimage test with pixmap surface\n");
		goto cleanup;
	}

  signal(SIGINT, sgxperf_signal_handler);

#ifdef _ENABLE_CMEM
	//Initialise the CMEM module. 
	//CMEM ko should be inserted before this point
	SGXPERF_printf("Configuring CMEM\n");
	CMEM_init();
#endif
	//Allocate texture for use in GL texturing modes(can also be done from CMEM if memory permits
	gTest._textureData = (unsigned int*)malloc(gTest.inTextureWidth*gTest.inTextureHeight*4 + PAGE_SIZE);
	if(!gTest._textureData)
	{
		SGXPERF_ERR_printf("ERROR: No malloc memory for allocating texture!\n");
		goto cleanup;
	}
	delta =(PAGE_SIZE - ((unsigned int)gTest._textureData &
(PAGE_SIZE-1))); 
	gTest.textureData = (unsigned int*)((char*)gTest._textureData + delta);
	memset(gTest.textureData, 0, gTest.inTextureWidth*gTest.inTextureHeight*4);

	set_texture(gTest.inTextureWidth, gTest.inTextureHeight, (unsigned char*)gTest.textureData, gTest.inPixelFormat);

	//initialise egl
	err = common_eglinit(&gTest, testID, gTest.inSurfaceType, &pNativePixmap);
	if(err)
	{
		SGXPERF_ERR_printf("ERROR: eglinit - err = %d\n", err);
		goto cleanup;
	}

	GLuint uiFragShader, uiVertShader;		// Used to hold the fragment and vertex shader handles
	GLuint uiProgramObject;					// Used to hold the program handle (made out of the two previous shaders

	// Create the fragment shader object
	uiFragShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load the source code into it
	if((testID == 3) || (testID == 5) || (testID == 6) || (testID == 7)  || 
        (testID == 14) || (testID == 17))
		glShaderSource(uiFragShader, 1, (const char**)&pszFragTextureShader, NULL);
	else if(testID == 11) //Edge detect with RGB input
		glShaderSource(uiFragShader, 1, (const char**)&pszFragEdgeRGBDetectShader, NULL);
	else if(testID == 12) //Edge detect with YUV input
		glShaderSource(uiFragShader, 1, (const char**)&pszFragEdgeYUVDetectShader, NULL);
	else if(testID == 8) //IMG texture stream2
		glShaderSource(uiFragShader, 1, (const char**)&pszFragIMGTextureStreamShader, NULL);
	else if(testID == 16) //EGLImage streaming
		glShaderSource(uiFragShader, 1, (const char**)&pszFragEGLImageShader, NULL);
	else
		glShaderSource(uiFragShader, 1, (const char**)&pszFragNoTextureShader, NULL);

	// Compile the source code
	glCompileShader(uiFragShader);

	// Check if compilation succeeded
	GLint bShaderCompiled;
    glGetShaderiv(uiFragShader, GL_COMPILE_STATUS, &bShaderCompiled);

	if (!bShaderCompiled)
	{
		// An error happened, first retrieve the length of the log message
		int i32InfoLogLength, i32CharsWritten;
		glGetShaderiv(uiFragShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

		// Allocate enough space for the message and retrieve it
		char* pszInfoLog = new char[i32InfoLogLength];
        glGetShaderInfoLog(uiFragShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog);

		// Displays the error
		SGXPERF_ERR_printf("Failed to compile fragment shader: %s\n", pszInfoLog);
		delete [] pszInfoLog;
		goto cleanup;
	}

	// Loads the vertex shader in the same way
	uiVertShader = glCreateShader(GL_VERTEX_SHADER);
	if((testID == 3) || (testID == 5) || (testID == 6) || (testID == 7) || 
        (testID == 8) || (testID == 11) || (testID == 12)
           || (testID == 14) || (testID == 16) || (testID == 17))
		glShaderSource(uiVertShader, 1, (const char**)&pszVertTextureShader, NULL);
	else
	{
		SGXPERF_ERR_printf("INFO: Using no-texture vertex shader\n");
		glShaderSource(uiVertShader, 1, (const char**)&pszVertNoTextureShader, NULL);
	}

	glCompileShader(uiVertShader);
    glGetShaderiv(uiVertShader, GL_COMPILE_STATUS, &bShaderCompiled);

	if (!bShaderCompiled)
	{
		int i32InfoLogLength, i32CharsWritten;
		glGetShaderiv(uiVertShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		char* pszInfoLog = new char[i32InfoLogLength];
        glGetShaderInfoLog(uiVertShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
		SGXPERF_ERR_printf("Failed to compile vertex shader: %s\n", pszInfoLog);
		delete [] pszInfoLog;
		goto cleanup;
	}

	// Create the shader program
    uiProgramObject = glCreateProgram();

	// Attach the fragment and vertex shaders to it
    glAttachShader(uiProgramObject, uiFragShader);
    glAttachShader(uiProgramObject, uiVertShader);

	// Bind the custom vertex attribute "myVertex" to location VERTEX_ARRAY
	glBindAttribLocation(uiProgramObject, VERTEX_ARRAY, "inVertex");
	glBindAttribLocation(uiProgramObject, TEXCOORD_ARRAY, "inTexCoord");

	// Link the program
    glLinkProgram(uiProgramObject);

	// Check if linking succeeded in the same way we checked for compilation success
    GLint bLinked;
    glGetProgramiv(uiProgramObject, GL_LINK_STATUS, &bLinked);

	if (!bLinked)
	{
		int ui32InfoLogLength, ui32CharsWritten;
		glGetProgramiv(uiProgramObject, GL_INFO_LOG_LENGTH, &ui32InfoLogLength);
		char* pszInfoLog = new char[ui32InfoLogLength];
		glGetProgramInfoLog(uiProgramObject, ui32InfoLogLength, &ui32CharsWritten, pszInfoLog);
		SGXPERF_ERR_printf("Failed to link program: %s\n", pszInfoLog);
		delete [] pszInfoLog;
		goto cleanup;
	}

	// Actually use the created program
	glUseProgram(uiProgramObject);
#ifdef _ENABLE_TEST16
	if(testID == 16)
	{
		int sampler = glGetUniformLocation(uiProgramObject, "yuvTexSampler");
		glUniform1i(sampler, 0);
	}
#endif
	//set rotation variables to init
	gTest.matrixLocation = glGetUniformLocation(uiProgramObject, "MVPMatrix");
	memset(gTest.mat_final, 0, sizeof(gTest.mat_final));
	gTest.mat_final[0] = gTest.mat_final[5] = gTest.mat_final[10] = gTest.mat_final[15] = 1.0;
	glUniformMatrix4fv( gTest.matrixLocation, 1, GL_FALSE, gTest.mat_final);
	
	/* Set rowsize for edge detect */
	if(testID == 11)
	{
		glUniform1f(glGetUniformLocation(uiProgramObject, "rowSize"), 256.0);
		glUniform1f(glGetUniformLocation(uiProgramObject, "columnSize"), 256.0);
	}
	else if(testID == 12)
	{
		glUniform1f(glGetUniformLocation(uiProgramObject, "rowSize"), 128.0);
		glUniform1f(glGetUniformLocation(uiProgramObject, "columnSize"), 160.0);
	}
	/* Set clear */
	glClearColor(0.2f, 0.8f, 1.0f, 1.0f); // blueish greenish red :)
	glClear(GL_COLOR_BUFFER_BIT);

		/*
		Do various tests
		*/
		switch(testID)
		{
			case 0:
				extensions = (char*)glGetString(GL_EXTENSIONS);
				SGXPERF_ERR_printf("\nTESTID = 0: GL SUPPORTED EXTENSIONS = \n%s\n", extensions);
                                extensions = (char*)eglQueryString(gTest.eglDisplay, EGL_EXTENSIONS);
                                SGXPERF_ERR_printf("\nTESTID = 0: EGL SUPPORTED EXTENSIONS = \n%s\n", extensions);
				break;
			case 1:
				/* Fill entire screen with single colour, no objects */
				test1(&gTest);
				break;
			case 2:
				/* Draw a coloured rectangle filling entire screen */
				test2(&gTest);
				break;
			case 3:
				/* Move a coloured rectangle of half screen size to another half with same parameters */
				test3(&gTest);
				break;
			case 4:
				/* Alpha blending full surface texture */
				test4(&gTest);	
				break;
			case 5:
				/* Alpha blending full surface WITHOUT texture */
				test5(&gTest);
				break;
			case 8:
#ifdef _ENABLE_TEST8
				/* GL_IMG_texture_stream */
				test8(&gTest);
#endif
				break;
			case 10:
				/* PVR2D test */
				test10(&gTest);
				break;
			case 11:
				/* Edge detection test - test3 with new shader */
				test3(&gTest);
				break;
#ifdef _ENABLE_TEST8
			case 12:
				/* Edge detection test - test8 with new shader */
				test8(&gTest);
				break;
#endif
			case 13:
				/* Line drawing */
				test13(&gTest);
				break;
#ifdef _ENABLE_TEST14				
			case 14:
				/* Context Switch */
				test14(&gTest);
				break;
#endif				
#ifdef _ENABLE_TEST15
			case 15:
				/* YUV-RGB conversion with PVR2D */
				test15(&gTest);
				break;
#endif
#ifdef _ENABLE_TEST16
			case 16:
				/* YUV-Streaming with EGLImage */
				test16(&gTest);
				break;
#endif
#ifdef _ENABLE_TEST17
			case 17:
				/* FBO */
				test17(&gTest);
				break;
#endif
			default:
				/* Sorry guys, we tried ! */
				SGXPERF_ERR_printf("No matching test code\n");
				break;
		}
#ifdef _ENABLE_CMEM
	/* PIXMAP loopback test  */
	if(gTest.inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16 || gTest.inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32) 
	{
#if 1
		/* Use this for checking out the output in the pixmap */
		/* For DEBUGGING ONLY */
		FILE *fp = fopen("pixmap.raw", "wb");
		int numbytes;
		if(gTest.inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32) numbytes = 4; //ARGB8888
		if(gTest.inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16) numbytes = 2; //RGB565
		SGXPERF_ERR_printf("Writing pixmap data to file pixmap.raw\n");
		fwrite((void*)pNativePixmap->lAddress, 1, gTest.inTextureWidth*gTest.inTextureHeight*numbytes, fp);
		fclose(fp);
#endif
	}
#endif //CMEM needed for pixmap loopback
	// Frees the OpenGL handles for the program and the 2 shaders
	glDeleteProgram(uiProgramObject);
	glDeleteShader(uiFragShader);
	glDeleteShader(uiVertShader);

cleanup:
	if(gTest._textureData) 
		free(gTest._textureData);
	common_egldeinit(&gTest, testID, pNativePixmap);
#ifdef _ENABLE_CMEM
	CMEM_exit();
#endif

	return 0;
}

/******************************************************************************
 End of file 
******************************************************************************/


