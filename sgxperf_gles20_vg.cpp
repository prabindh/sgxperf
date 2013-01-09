/******************************************************************************

Test file for validating throughputs for 2D related operations, using 
the openGLES and VG APIs and PVR2D.

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

Latest code and information can be obtained from
https://gitorious.org/tigraphics
Earlier versions and links to XgxPerf (a Qt based tool) can be obtained from
https://gforge.ti.com/gf/project/gleslayer/

Prabindh Sundareson prabu@ti.com
(c) Texas Instruments 2011
******************************************************************************/
#include <stdio.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "PVRTools.h"
#include "OVGTools.h"
#include "VG/openvg.h"
#include <sys/time.h> //for profiling
#include <stdlib.h> //for profiling
#include <string.h>
#include <signal.h>
#include <unistd.h>
#ifdef _ENABLE_CMEM
#include <cmem.h> //for contiguous physical memory allocation. Always used.
#endif //CMEM

#include "sgxperf.h"
extern unsigned int _photoframe_pvr_1024x1024[];
extern unsigned int _photoframe_565_1024x1024[];
extern unsigned char yukio_160x128yuv[];
extern unsigned int _fan256x256_argb[];
extern unsigned int _fan256x256_rgb565[];

#define MAX_TEST_ID 14 //Max number of available tests
#define SGX_PERF_VERSION 1.0

#define _ENABLE_TEST1 /* Fill entire screen with single colour, no objects */
#define _ENABLE_TEST2 /* Draw a coloured triangle filling entire screen */
#define _ENABLE_TEST3 /* glTexImage2D based texturing */
#define _ENABLE_TEST4 /* Alpha blending full surface texture */
#define _ENABLE_TEST5 /* Alpha blending full surface WITHOUT texture */
#ifdef _ENABLE_CMEM
#define _ENABLE_TEST6 /* EGL_NATIVE_PIXMAP_KHR */
#define _ENABLE_TEST7 /* EGL_GL_TEXTURE_2D_KHR */
#else
#define _ENABLE_TEST8_NO_USERPTR
#endif //CMEM tests

#ifdef _ENABLE_BUFFERCLASS
#define _ENABLE_TEST8 /* GL_IMG_texture_stream */
#endif //IMGSTREAM
#define _ENABLE_TEST9 /* Fill entire screen with single colour, read and render SVG */
#define _ENABLE_TEST10 /* PVR2D Blit */
#define _ENABLE_TEST13 /* Filled Lines */
#define _ENABLE_TEST14 /* multi surface context switch */
#define _ENABLE_TEST15 /* YUV-RGB converter with PVR2D */

#define SGX_PERF_printf dummy_printf
#define SGX_PERF_ERR_printf printf

#define SGX_PERF_STARTPROFILEUNIT {gettimeofday(&unitStartTime, NULL);}  
#define SGX_PERF_ENDPROFILEUNIT {gettimeofday(&unitEndTime, NULL);\
	   if(msToSleep > tv_diff(&startTime, &endTime)) \
      usleep((msToSleep - tv_diff(&startTime, &endTime))/1000); }		


// Index to bind the attributes to vertex shaders
#define VERTEX_ARRAY 0
#define TEXCOORD_ARRAY 1

//Bit types
#define SGXPERF_RGB565 0
#define SGXPERF_ARGB8888 2
#define SGXPERF_BYTE8 1 /* Luminance only */
//surfaceType
#define SGXPERF_SURFACE_TYPE_WINDOW 0
#define SGXPERF_SURFACE_TYPE_PIXMAP_16 1
#define SGXPERF_SURFACE_TYPE_PIXMAP_32 2



//Delta by which rotation happens when rotation is enabled
#define ANGLE_DELTA 0.02f

// dynamically allocated data FOR variable size texture
unsigned int *textureData = 0;

//common to all test cases (with or without textures)
typedef struct _NATIVE_PIXMAP_STRUCT
{
    long ePixelFormat;
    long eRotation;
    long lWidth;
    long lHeight;
    long lStride;
    long lSizeInBytes;
    long pvAddress;
    long lAddress;
}NATIVE_PIXMAP_STRUCT;

/** GLOBALS **/
EGLConfig eglConfig	= 0;
EGLDisplay eglDisplay = 0;
EGLSurface eglSurface = 0;
EGLSurface eglSurface2 = 0;
EGLContext eglContext = 0;
GLuint textureId0;
int matrixLocation;
float mat_x[16], mat_y[16], mat_z[16], mat_temp[16], mat_final[16];
int inTextureWidth = 0;
int inTextureHeight = 0;
int inRotationEnabled = 0;
int inPixelFormat = 0;
char *inSvgFileName;
int inNumberOfObjectsPerSide = 1;
int inSurfaceType;
int windowWidth, windowHeight;
int quitSignal = 0;
int numTestIterations = 0, inFPS=1;
unsigned int msToSleep = 0;
char* cookie;

bool TestEGLError(const char* pszLocation)
{
	EGLint iErr = eglGetError();
	if (iErr != EGL_SUCCESS)
	{
		SGX_PERF_ERR_printf("%s failed (%d).\n", pszLocation, iErr);
		return false;
	}

	return true;
}
static int dummy_printf(const char* format, ...)
{
	return 0;
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

	// _photoframe_pvr_1024x1024 is in ARGB format
	if(width == 1024 && height == 1024 && numbytes == 4)
		memcpy(pTex, _photoframe_pvr_1024x1024, width*height*numbytes);
	else if(width == 1024 && height == 1024 && numbytes == 2)
		memcpy(pTex, _photoframe_565_1024x1024, width*height*numbytes);		
	else if(width == 256 && height == 256 && numbytes == 4)
		memcpy(pTex, _fan256x256_argb, width*height*numbytes);	
	else if(width == 256 && height == 256 && numbytes == 2)
		memcpy(pTex, _fan256x256_rgb565, width*height*numbytes);
	else if(width == 256 && height == 256 && numbytes == 1)
		memcpy(pTex, lena_256x256_8bit, width*height*numbytes);
	else
	{
		for(int i = 0;i < width*height*numbytes;i ++)
			pTex[i] = i;
	}
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
	else {SGX_PERF_ERR_printf("%d texture format unsupported\n", pixelFormat); exit(-1);}
	// load the texture up
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	//SGX_PERF_ERR_printf("\n\nTextureType=%d\n",textureType);
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
		SGX_PERF_ERR_printf("Invalid pixmap format type %ld\n", pixmapFormat);
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
		SGX_PERF_ERR_printf("CMEM_alloc returned NULL\n");
		exit(-1);
	}
    //Get the physical page corresponding to the above cmem buffer
    (*pNativePixmapPtr)->pvAddress = CMEM_getPhys((void*)(*pNativePixmapPtr)->lAddress);
	SGX_PERF_printf("Physical address = %x\n", (*pNativePixmapPtr)->pvAddress);
	if((*pNativePixmapPtr)->pvAddress & 0xFFF)
		SGX_PERF_printf("PVR2DMemWrap may have issues with this non-aligned address!\n");
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
int common_eglinit(int testID, int surfaceType, NATIVE_PIXMAP_STRUCT** pNativePixmapPtr)
{
	EGLint iMajorVersion, iMinorVersion;
	EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	eglDisplay = eglGetDisplay((int)0);

	if (!eglInitialize(eglDisplay, &iMajorVersion, &iMinorVersion))
		return 1;

	if(testID == 9)
		eglBindAPI(EGL_OPENVG_API);
	else
		eglBindAPI(EGL_OPENGL_ES_API);
	if (!TestEGLError("eglBindAPI"))
		return 1;

	EGLint pi32ConfigAttribs[5];
	pi32ConfigAttribs[0] = EGL_SURFACE_TYPE;
	pi32ConfigAttribs[1] = EGL_WINDOW_BIT | EGL_PIXMAP_BIT;
	pi32ConfigAttribs[2] = EGL_RENDERABLE_TYPE;
	if(testID == 9)
		pi32ConfigAttribs[3] = EGL_OPENVG_BIT;
	else
		pi32ConfigAttribs[3] = EGL_OPENGL_ES2_BIT;
	pi32ConfigAttribs[4] = EGL_NONE;

	int iConfigs;
	if (!eglChooseConfig(eglDisplay, pi32ConfigAttribs, &eglConfig, 1, &iConfigs) || (iConfigs != 1))
	{
		SGX_PERF_ERR_printf("Error: eglChooseConfig() failed.\n");
		return 1;
	}
	if(surfaceType == SGXPERF_SURFACE_TYPE_WINDOW)
		eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, (void*) NULL, NULL);
	else if(surfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16)
	{
		common_create_native_pixmap(SGXPERF_RGB565, 
						inTextureWidth, inTextureHeight, pNativePixmapPtr);
		eglSurface = eglCreatePixmapSurface(eglDisplay, eglConfig, *pNativePixmapPtr, NULL);
	}
	else if(surfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32)
	{
		common_create_native_pixmap(SGXPERF_ARGB8888, 
						inTextureWidth, inTextureHeight, pNativePixmapPtr);
		eglSurface = eglCreatePixmapSurface(eglDisplay, eglConfig, *pNativePixmapPtr, NULL);
	}
  else  
    return 999;

	if (!TestEGLError("eglCreateSurface"))
		return 1;
		
	if(testID == 14) //Create one pixmap surface for context switch latency check
	{
		common_create_native_pixmap(SGXPERF_RGB565, 
						inTextureWidth, inTextureHeight, pNativePixmapPtr);
		eglSurface2 = eglCreatePixmapSurface(eglDisplay, eglConfig, *pNativePixmapPtr, NULL);
	}  	
	if (!TestEGLError("eglCreateSurface"))
		return 1;		

	if(testID == 9)
		eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, NULL);
	else
		eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, ai32ContextAttribs);
	if (!TestEGLError("eglCreateContext"))
		return 1;

	eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
	if (!TestEGLError("eglMakeCurrent"))
		return 1;

	eglSwapInterval(eglDisplay, 1);
	if (!TestEGLError("eglSwapInterval"))
		return 1;

	eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH, &windowWidth);
	eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &windowHeight);
	
	fprintf(stderr,"Window width=%d, Height=%d\n", windowWidth, windowHeight);

	return 0;
}
void common_egldeinit(int testID, NATIVE_PIXMAP_STRUCT* pNativePixmap)
{
	eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ;
	if(pNativePixmap)
		common_delete_native_pixmap(pNativePixmap);
	eglDestroyContext(eglDisplay, eglContext);
	eglDestroySurface(eglDisplay, eglSurface);
	if(testID == 14)
	  eglDestroySurface(eglDisplay, eglSurface2);	
	eglTerminate(eglDisplay);
}

//swapping buffers
void common_eglswapbuffers(
						   EGLDisplay eglDisplay, 
						   EGLSurface eglSurface
						   )
{
	if(inSurfaceType == SGXPERF_SURFACE_TYPE_WINDOW)
		eglSwapBuffers(eglDisplay, eglSurface);
	else if(inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16 || inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32)
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
	glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, (const void*)afVertices);

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
	glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, (const void*)afVertices);

	return 0;
}
//Common function to draw the previously input vertices
//Texture mapping if needed, has to be done before
void common_gl_draw(int numObjects)
{
	int startIndex = 0;
	static int alreadyDone = 0;
	
	for(int vertical = 0;vertical < numObjects;vertical ++)
		for(int horizontal = 0;horizontal < numObjects;horizontal ++)
		{
			{
				// Create and Bind texture
				glGenTextures(1, &textureId0);
				glBindTexture(GL_TEXTURE_2D, textureId0);
				add_texture(inTextureWidth, inTextureHeight, textureData, inPixelFormat);
				alreadyDone = 1;
			}
			glDrawArrays(GL_TRIANGLE_STRIP, startIndex, 4);
			{
				glDeleteTextures(1, &textureId0);
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
//matrix mult
void calculate_rotation_y(
	float	*mOut,
	const float fAngle)
{
	float		fCosine, fSine;
	fCosine =	(float)PVRTFCOS(fAngle);
    fSine =		(float)PVRTFSIN(fAngle);
	mOut[ 0]=fCosine;		mOut[ 4]=0.f;	mOut[ 8]=fSine;	mOut[12]=0.0f;
	mOut[ 1]=0.f;		mOut[ 5]=1.0f;	mOut[ 9]=0.0f;	mOut[13]=0.0f;
	mOut[ 2]=-fSine;		mOut[ 6]=0.0f;	mOut[10]=fCosine;	mOut[14]=0.0f;
	mOut[ 3]=0.0f;		mOut[ 7]=0.0f;	mOut[11]=0.0f;	mOut[15]=1.0f;
}


void calculate_rotation_x(
	float	*mOut,
	const float fAngle)
{
	float		fCosine, fSine;
	fCosine =	(float)PVRTFCOS(fAngle);
    fSine =		(float)PVRTFSIN(fAngle);
	/* Create the trigonometric matrix corresponding to about x */
	mOut[ 0]=1.0f;		mOut[ 4]=0.0f;	mOut[ 8]=0.0f;	mOut[12]=0.0f;
	mOut[ 1]=0.0f;		mOut[ 5]=fCosine;	mOut[ 9]=-fSine;	mOut[13]=0.0f;
	mOut[ 2]=0.0f;		mOut[ 6]=fSine;	mOut[10]=fCosine;	mOut[14]=0.0f;
	mOut[ 3]=0.0f;		mOut[ 7]=0.0f;	mOut[11]=0.0f;	mOut[15]=1.0f;
}



void calculate_rotation_z(
	float	*mOut,
	const float fAngle)
{
	float		fCosine, fSine;
	fCosine =	(float)PVRTFCOS(fAngle);
    fSine =		(float)PVRTFSIN(fAngle);
	/* Create the trigonometric matrix corresponding to Rotation about z */
	mOut[ 0]=fCosine;		mOut[ 4]=-fSine;	mOut[ 8]=0.0f;	mOut[12]=0.0f;
	mOut[ 1]=fSine;		mOut[ 5]=fCosine;	mOut[ 9]=0.0f;	mOut[13]=0.0f;
	mOut[ 2]=0.0f;		mOut[ 6]=0.0f;	mOut[10]=1.0f;	mOut[14]=0.0f;
	mOut[ 3]=0.0f;		mOut[ 7]=0.0f;	mOut[11]=0.0f;	mOut[15]=1.0f;
}

void matrix_mult(
				 float *mA,
				 float *mB,
				 float *mRet
				 )
{
	/* Perform calculation on a dummy matrix (mRet) */
	mRet[ 0] = mA[ 0]*mB[ 0] + mA[ 1]*mB[ 4] + mA[ 2]*mB[ 8] + mA[ 3]*mB[12];
	mRet[ 1] = mA[ 0]*mB[ 1] + mA[ 1]*mB[ 5] + mA[ 2]*mB[ 9] + mA[ 3]*mB[13];
	mRet[ 2] = mA[ 0]*mB[ 2] + mA[ 1]*mB[ 6] + mA[ 2]*mB[10] + mA[ 3]*mB[14];
	mRet[ 3] = mA[ 0]*mB[ 3] + mA[ 1]*mB[ 7] + mA[ 2]*mB[11] + mA[ 3]*mB[15];

	mRet[ 4] = mA[ 4]*mB[ 0] + mA[ 5]*mB[ 4] + mA[ 6]*mB[ 8] + mA[ 7]*mB[12];
	mRet[ 5] = mA[ 4]*mB[ 1] + mA[ 5]*mB[ 5] + mA[ 6]*mB[ 9] + mA[ 7]*mB[13];
	mRet[ 6] = mA[ 4]*mB[ 2] + mA[ 5]*mB[ 6] + mA[ 6]*mB[10] + mA[ 7]*mB[14];
	mRet[ 7] = mA[ 4]*mB[ 3] + mA[ 5]*mB[ 7] + mA[ 6]*mB[11] + mA[ 7]*mB[15];

	mRet[ 8] = mA[ 8]*mB[ 0] + mA[ 9]*mB[ 4] + mA[10]*mB[ 8] + mA[11]*mB[12];
	mRet[ 9] = mA[ 8]*mB[ 1] + mA[ 9]*mB[ 5] + mA[10]*mB[ 9] + mA[11]*mB[13];
	mRet[10] = mA[ 8]*mB[ 2] + mA[ 9]*mB[ 6] + mA[10]*mB[10] + mA[11]*mB[14];
	mRet[11] = mA[ 8]*mB[ 3] + mA[ 9]*mB[ 7] + mA[10]*mB[11] + mA[11]*mB[15];

	mRet[12] = mA[12]*mB[ 0] + mA[13]*mB[ 4] + mA[14]*mB[ 8] + mA[15]*mB[12];
	mRet[13] = mA[12]*mB[ 1] + mA[13]*mB[ 5] + mA[14]*mB[ 9] + mA[15]*mB[13];
	mRet[14] = mA[12]*mB[ 2] + mA[13]*mB[ 6] + mA[14]*mB[10] + mA[15]*mB[14];
	mRet[15] = mA[12]*mB[ 3] + mA[13]*mB[ 7] + mA[14]*mB[11] + mA[15]*mB[15];
}


static unsigned long
tv_diff(struct timeval *tv1, struct timeval *tv2)
{
    return (tv2->tv_sec - tv1->tv_sec) * 1000 +
        (tv2->tv_usec - tv1->tv_usec) / 1000;
}

static void common_log(int testid, unsigned long time)
{
	SGX_PERF_ERR_printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%ld\n", 
							testid, 
							inTextureWidth, 
							inTextureHeight, 
							inRotationEnabled, 
							inPixelFormat,
              inSurfaceType,
							inNumberOfObjectsPerSide,
							time);
  //Also log to files required by xgxperf
  //Get CPU load
  system("sh getload.sh");
  FILE* tempFile = fopen("logload.txt","rt");
  unsigned char loadData[100];
  memset(loadData, 0, sizeof(loadData));
  fread(loadData, 1,50, tempFile);
  fclose(tempFile);
  unsigned long offset=(unsigned long)strchr((const char*)loadData,' ') - (unsigned long)loadData;
  loadData[offset]=loadData[offset+1]='\0';
  float CPULoad = atof((const char*)loadData);
  char xmlfilename[100];
  memset(xmlfilename,0,sizeof(xmlfilename));
  strcat(xmlfilename,"profiledata_");
  strcat(xmlfilename,cookie);
  strcat(xmlfilename,".xml");
  char htmlfilename[100];
  memset(htmlfilename,0,sizeof(htmlfilename));
  strcat(htmlfilename,"profiledata_");
  strcat(htmlfilename,cookie);
  strcat(htmlfilename,".html");
  
  FILE* xmlfile=fopen(xmlfilename,"wt");
  fprintf(xmlfile,"<XgxPerf_Profile_Test_Inputs_Outputs>\
        <testId>%d</testId><textureWidth>%d</textureWidth>\
        <textureHeight>%d</textureHeight>\
        <rotation>%d</rotation>\
        <pixelformat>%d</pixelformat>\
        <surfacetype>%d</surfacetype>\
        <numobjects>%d</numobjects>\
        <time_ms>%ld</time_ms>\
        <cpuload_percentage_0_to_1>%f</cpuload_percentage_0_to_1>\
        <fps>%d</fps>\
        </XgxPerf_Profile_Test_Inputs_Outputs>",
							testid, 
							inTextureWidth, 
							inTextureHeight, 
							inRotationEnabled, 
							inPixelFormat,
              inSurfaceType,
							inNumberOfObjectsPerSide,
							time,
              CPULoad,
              inFPS); 
  fclose(xmlfile);

  FILE* htmlfile=fopen(htmlfilename,"wt");
  fprintf(htmlfile,"<HTML><table border=1>\
              <tr>\
              <th>testid</th>\
							<th>texturewd</th>\
							<th>textureht</th>\
							<th>rot</th>\
							<th>texturetype</th>\
              <th>surface</th>\
							<th>n.obj</th>\
							<th>time</th>\
              <th>CPULoad</th>\
              <th>inFPS</th><tr>");
  fprintf(htmlfile,"<tr><td>%d</td><td>%d<td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%ld</td><td>%f</td><td>%d\
          </tr><br></HTML>", 
							testid, 
							inTextureWidth, 
							inTextureHeight, 
							inRotationEnabled, 
							inPixelFormat,
              inSurfaceType,
							inNumberOfObjectsPerSide,
							time,
              CPULoad,
              inFPS);
  fclose(htmlfile);
  //No screendump as no compressor is available for now							
							
}


/****************************************************
* TESTS START HERE
*****************************************************/

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
	
	  SGX_PERF_STARTPROFILEUNIT;	
		glClearColor(0.2f, 0.4f, 0.8f, 1.0f); // clear blue
		glClear(GL_COLOR_BUFFER_BIT);
		common_eglswapbuffers(eglDisplay, eglSurface);
    SGX_PERF_ENDPROFILEUNIT;		
	}
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(1, diffTime2);
}
#endif
#ifdef _ENABLE_TEST2
		/* Draw a coloured rectangle filling entire screen */
void test2()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i, err;
	float angle = 0;
	float *pVertexArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);

	gettimeofday(&startTime, NULL);

	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, angle);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}

		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
  SGX_PERF_ENDPROFILEUNIT		
	}
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	err = glGetError();
	if(err)
		SGX_PERF_ERR_printf("Error in gldraw err = %x", err);	

	common_deinit_gl_vertices(pVertexArray);
	common_log(2, diffTime2);
}
#endif
#ifdef _ENABLE_TEST3
/* Texturing using glteximage2d */ 
void test3()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i, err;
	float angle = 0;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	gettimeofday(&startTime, NULL);
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)	
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, angle);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}
		//draw first area with texture
		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err)
		SGX_PERF_ERR_printf("Error in gldraw err = %x", err);		
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(3, diffTime2);

	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}
#endif

#ifdef _ENABLE_TEST4
/* Draw frames WITH texturing one above other with blending */
void test4()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i, err;
	float angle = 0;
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClear(GL_COLOR_BUFFER_BIT);
	gettimeofday(&startTime, NULL);
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, 0);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}
		//some nonsense will come on screen as Alpha data is invalid for the sample
		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err)
		SGX_PERF_ERR_printf("Error in gldraw err = %x", err);
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(4, diffTime2);

	glDisableVertexAttribArray(VERTEX_ARRAY);
	glDisableVertexAttribArray(TEXCOORD_ARRAY);
	glDisable(GL_BLEND);
	common_deinit_gl_vertices(pVertexArray);

}
#endif
#ifdef _ENABLE_TEST5
/* Draw frames WITH-OUT texturing one above other with blending */
void test5()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i, err;
	float angle = 0;

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	gettimeofday(&startTime, NULL);
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, 0);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}
		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err)
		SGX_PERF_ERR_printf("Error in gldraw err = %x", err);
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(5, diffTime2);

	glDisableVertexAttribArray(VERTEX_ARRAY);
	glDisable(GL_BLEND);
	common_deinit_gl_vertices(pVertexArray);
}
#endif
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
		SGX_PERF_printf("eglCreateImageKHR not found!\n");
		return 2;
	}
	//create an egl image

	SGX_PERF_printf("textureId0 = %d\n", textureId0); //getting 70001
	eglImage = peglCreateImageKHR(
							eglDisplay,
							EGL_NO_CONTEXT, //eglContext,
							EGL_NATIVE_PIXMAP_KHR, //EGL_GL_TEXTURE_2D_KHR,
							pNativePixmap, //(void*)textureId0,//
							NULL //miplevel 0 is fine, thank you
							);
	SGX_PERF_printf("peglCreateImageKHR returned %x\n", eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGX_PERF_printf("Error after peglCreateImageKHR!, error = %x\n", err);
		return 3;
	}

	//bind this to a texture
	pFnEGLImageTargetTexture2DOES = 
		(PFNGLEGLIMAGETARGETTEXTURE2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if(pFnEGLImageTargetTexture2DOES == NULL)
		SGX_PERF_printf("pFnEGLImageTargetTexture2DOES not found!\n");
	pFnEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
	if((err = glGetError()) != 0)
	{
		SGX_PERF_printf("Error after glEGLImageTargetTexture2DOES!, error = %x\n", err);
		return 4;
	}
	SGX_PERF_printf("Destroying eglimage!\n");
	pEGLDestroyImage = (PEGLDESTROYIMAGE)eglGetProcAddress("eglDestroyImageKHR");
	//destroy the eglimage
	pEGLDestroyImage(eglDisplay, eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGX_PERF_printf("Error after pEGLDestroyImage!, error = %x\n", err);
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
	float angle = 0;
	float *pVertexArray, *pTexCoordArray;

	//initialise the vertices
	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	//initialise the eglimage based texture
	err = test6_init_eglimage_khr(pNativePixmap);
	if(err)
	{
		SGX_PERF_ERR_printf("TEST6: Init failed with err = %d\n", err);
		goto deinit;
	}

	gettimeofday(&startTime, NULL);

	err = glGetError();
	if(err) 
		SGX_PERF_ERR_printf("Error before gl draw loop err = %x\n", err);

	SGX_PERF_printf("Entering DrawArrays loop\n");
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGX_PERF_STARTPROFILEUNIT;	
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
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, 0);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}
		eglimage_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err) 
		SGX_PERF_ERR_printf("Error in gl draw loop err = %x\n", err);
	SGX_PERF_printf("Exiting DrawArrays loop\n");
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

#ifdef _ENABLE_TEST7

#ifndef _ENABLE_TEST6 //test6 already defines these
#include "pvr2d.h"
typedef void *EGLImageKHR;
typedef void* GLeglImageOES;
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOES) (GLenum target, GLeglImageOES image);
typedef EGLImageKHR (*PEGLCREATEIMAGEKHR)(EGLDisplay eglDpy, 
			EGLContext eglContext, EGLenum target, EGLClientBuffer buffer, EGLint *pAttribList);
typedef EGLBoolean (*PEGLDESTROYIMAGE)(EGLDisplay eglDpy, EGLImageKHR eglImage);
PEGLCREATEIMAGEKHR peglCreateImageKHR;
PEGLDESTROYIMAGE pEGLDestroyImage;
GLeglImageOES eglImage;
PFNGLEGLIMAGETARGETTEXTURE2DOES pFnEGLImageTargetTexture2DOES;
#endif //ifndef test6
#define EGL_GL_TEXTURE_2D_KHR					0x30B1	/* eglCreateImageKHR target */
int test7_init_eglimage_khr()
{
    int err;
	//do a teximage2d to ensure things are inited fine - TODO: Check with IMG
	// Create and Bind texture
	glGenTextures(1, &textureId0);
	glBindTexture(GL_TEXTURE_2D, textureId0);
	add_texture(inTextureWidth, inTextureHeight, textureData, inPixelFormat);

	//get extension addresses
	peglCreateImageKHR = (PEGLCREATEIMAGEKHR)eglGetProcAddress("eglCreateImageKHR");
	if(peglCreateImageKHR == NULL)
		SGX_PERF_printf("eglCreateImageKHR not found!\n");
	//create an egl image

	SGX_PERF_printf("textureId0 = %d\n", textureId0); //getting 70001
	eglImage = peglCreateImageKHR(
							eglDisplay,
							eglContext,
							EGL_GL_TEXTURE_2D_KHR,
							(void*)textureId0,//
							NULL //miplevel 0 is fine, thank you
							);
	SGX_PERF_printf("peglCreateImageKHR returned %x\n", eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGX_PERF_printf("Error after peglCreateImageKHR!, error = %x\n", err);
		return 1;
	}

	//bind this to a texture
	pFnEGLImageTargetTexture2DOES = 
		(PFNGLEGLIMAGETARGETTEXTURE2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if(pFnEGLImageTargetTexture2DOES == NULL)
	{
		SGX_PERF_printf("pFnEGLImageTargetTexture2DOES not found!\n");
		return 2;
	}
	pFnEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGX_PERF_printf("Error after glEGLImageTargetTexture2DOES!, error = %x\n", err);
		return 3;
	}
	SGX_PERF_printf("Destroying eglimage!\n");
	pEGLDestroyImage = (PEGLDESTROYIMAGE)eglGetProcAddress("eglDestroyImageKHR");
	//destroy the eglimage
	pEGLDestroyImage(eglDisplay, eglImage);
	if((err = eglGetError()) != EGL_SUCCESS)
	{
		SGX_PERF_printf("Error after pEGLDestroyImage!, error = %x\n", err);
		return 4;
	}

	return 0;
	//exit(1); //to debug
}
void test7_deinit_eglimage_khr()
{
}

/* Use EGL_GL_TEXTURE_2D_KHR extension to texture instead of glteximage2d */
void test7()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int err;
	int i;
	float angle = 0;

	//initialise gl draw states
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	//initialise the eglimage based texture
	err = test7_init_eglimage_khr();
	if(err)
	{
		SGX_PERF_ERR_printf("TEST7: Init failed with err = %d", err);
		goto deinit;
	}

	gettimeofday(&startTime, NULL);
	SGX_PERF_printf("Entering DrawArrays loop\n");
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		//texture will be already bound using the eglimage extension, no calling glteximage2d. How nice !!
		if(i & 1)
		{
			//ensure that texture is setup - Is there any other way of doing this ?
			//memset((void*)nativePixmap.lAddress, 0, nativePixmap.lSizeInBytes);
		}
		else
			//memset((void*)nativePixmap.lAddress, 5, nativePixmap.lSizeInBytes);
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, 0);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}
		eglimage_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err) 
		SGX_PERF_printf("Error in gl draw loop err = %d\n", err);
	SGX_PERF_printf("Exiting DrawArrays loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(7, diffTime2);

deinit:
	glDisable(GL_TEXTURE_2D);
	//deinit the eglimage
	test7_deinit_eglimage_khr();
	common_deinit_gl_vertices(pVertexArray);
}
#endif //test7

#ifdef _ENABLE_TEST8 //GL_IMG_texture_stream


#include <sys/mman.h>       // mmap()
#if defined __linux__
#define LINUX //needed for imgdefs
#endif
#include <img_types.h>
#include <servicesext.h>
#include "bc_cat.h"
//#include <glext.h>
#include <fcntl.h>
#include "sys/ioctl.h"
#include <unistd.h>

#define BC_CAT_DRV "/dev/bccat0" //bc_cat"
#define MAX_BUFFERS 1

char *buf_paddr[MAX_BUFFERS];
char *buf_vaddr[MAX_BUFFERS] = { (char *) MAP_FAILED };

#define GL_TEXTURE_STREAM_IMG                                   0x8C0D     
#define GL_TEXTURE_NUM_STREAM_DEVICES_IMG                       0x8C0E     
#define GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG                      0x8C0F
#define GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG                     0x8EA0     
#define GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG                     0x8EA1      
#define GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG                0x8EA2     

typedef void (GL_APIENTRYP PFNGLTEXBINDSTREAMIMGPROC) (GLint device, GLint deviceoffset);
typedef const GLubyte *(GL_APIENTRYP PFNGLGETTEXSTREAMDEVICENAMEIMGPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLGETTEXSTREAMDEVICEATTRIBUTEIVIMGPROC) (GLenum target, GLenum pname, GLint *params);
PFNGLTEXBINDSTREAMIMGPROC myglTexBindStreamIMG = NULL;
PFNGLGETTEXSTREAMDEVICEATTRIBUTEIVIMGPROC myglGetTexStreamDeviceAttributeivIMG = NULL;
PFNGLGETTEXSTREAMDEVICENAMEIMGPROC myglGetTexStreamDeviceNameIMG = NULL;
int numBuffers, bufferWidth, bufferHeight, bufferFormat;
int bcfd = -1; //file descriptor
bc_buf_params_t bc_param;
BCIO_package ioctl_var;

bc_buf_ptr_t buf_pa; //USERPTR buffer
#ifdef _ENABLE_CMEM
CMEM_AllocParams cmemParams = { CMEM_POOL, CMEM_NONCACHED, 4096 };
void* virtualAddress; 
int physicalAddress;
int test8_init_texture_streaming_userptr()
{
    const GLubyte *pszGLExtensions;
    const GLubyte *pszStreamDeviceName;    
	  int err;  
	
    //setup CMEM pointers  
    virtualAddress = CMEM_alloc(inTextureWidth*inTextureHeight*4, &cmemParams);
    if(!virtualAddress)                                      
    {
		  SGX_PERF_printf("Error in CMEM_alloc\n");
      return 1;
    }
    physicalAddress = CMEM_getPhys(virtualAddress);
    if(!physicalAddress)                                      
    {
		  SGX_PERF_printf("Error in CMEM_getPhys\n");
      return 2;
    }
	 //setup driver now
    if ((bcfd = open(BC_CAT_DRV, O_RDWR|O_NDELAY)) == -1) {
		  SGX_PERF_printf("Error opening bc driver - is bufferclass_ti module inserted ?\n");  
      CMEM_free((void*)virtualAddress, &cmemParams);
        return 3;
    }
    bc_param.count = 1;
    bc_param.width = inTextureWidth;
    bc_param.height = inTextureHeight;
    bc_param.fourcc = BC_PIX_FMT_YUYV;           
    bc_param.type = BC_MEMORY_USERPTR;
    SGX_PERF_printf("About to BCIOREQ_BUFFERS \n");        
    err = ioctl(bcfd, BCIOREQ_BUFFERS, &bc_param);
    if (err != 0) {    
      CMEM_free(virtualAddress, &cmemParams);
        SGX_PERF_ERR_printf("ERROR: BCIOREQ_BUFFERS failed err = %x\n", err);
        return 4;
    }                                                      
    SGX_PERF_printf("About to BCIOGET_BUFFERCOUNT \n");
    if (ioctl(bcfd, BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {    
      CMEM_free(virtualAddress, &cmemParams);
		  SGX_PERF_printf("Error ioctl BCIOGET_BUFFERCOUNT");
        return 5;
    }
    if (ioctl_var.output == 0) {    
        CMEM_free(virtualAddress, &cmemParams);
        SGX_PERF_printf("Error no buffers available from driver \n");
        return 6;
    }
    //For USERPTR mode, set the buffer pointer first
    SGX_PERF_printf("About to BCIOSET_BUFFERADDR \n");            
    buf_pa.index = 0;    
    buf_pa.pa = (int)physicalAddress;    
    buf_pa.size = inTextureWidth * inTextureHeight * 2;
    if (ioctl(bcfd, BCIOSET_BUFFERPHYADDR, &buf_pa) != 0) 
    {       
        CMEM_free(virtualAddress, &cmemParams);
        SGX_PERF_printf("Error BCIOSET_BUFFERADDR failed\n");
        return 7;
    }
        
    /* Retrieve GL extension string */
    pszGLExtensions = glGetString(GL_EXTENSIONS);

    if (!pszGLExtensions || !strstr((char *)pszGLExtensions, "GL_IMG_texture_stream2"))
	{
    CMEM_free(virtualAddress, &cmemParams);
		SGX_PERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
		return 8;
	}

    myglTexBindStreamIMG = (PFNGLTEXBINDSTREAMIMGPROC)eglGetProcAddress("glTexBindStreamIMG");
    myglGetTexStreamDeviceAttributeivIMG = 
        (PFNGLGETTEXSTREAMDEVICEATTRIBUTEIVIMGPROC)eglGetProcAddress("glGetTexStreamDeviceAttributeivIMG");
    myglGetTexStreamDeviceNameIMG = 
        (PFNGLGETTEXSTREAMDEVICENAMEIMGPROC)eglGetProcAddress("glGetTexStreamDeviceNameIMG");

    if(!myglTexBindStreamIMG || !myglGetTexStreamDeviceAttributeivIMG || !myglGetTexStreamDeviceNameIMG)
	{
      CMEM_free(virtualAddress, &cmemParams);
		  SGX_PERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
      return 9;
	}

    pszStreamDeviceName = myglGetTexStreamDeviceNameIMG(0);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG, &numBuffers);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG, &bufferWidth);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG, &bufferHeight);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG, &bufferFormat);

    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    SGX_PERF_printf("\nStream Device %s: numBuffers = %d, width = %d, height = %d, format = %x\n",
        pszStreamDeviceName, numBuffers, bufferWidth, bufferHeight, bufferFormat);

		if((inTextureWidth == 160) && (inTextureHeight == 128)) //only for 160x128
			memcpy(virtualAddress, yukio_160x128yuv, 160*128*2);
		else if((inTextureWidth == 720) && (inTextureHeight == 480)) //for VGA
		{
			FILE*fp = fopen("coastguard_d1_422.yuv","rb");
			if(!fp) {SGX_PERF_printf("Error opening YUV file\n"); return 9;}
			fread(virtualAddress,720*480*2,1,fp);
			fclose(fp);
		}
		else
		{
			for (int iii= 0; iii < inTextureHeight; iii++)
				memset(((char *) virtualAddress + (inTextureWidth*iii*2)) , 0xa8, inTextureWidth*2);
		}

	return 0;
}
void test8_deinit_texture_streaming_userptr()
{
    if(virtualAddress)
      CMEM_free(virtualAddress, &cmemParams);
    if (bcfd > -1)
        close(bcfd);
}
#endif //CMEM

int test8_init_texture_streaming()
{
    const GLubyte *pszGLExtensions;
    const GLubyte *pszStreamDeviceName;
	int idx;

	//setup driver now
    if ((bcfd = open(BC_CAT_DRV, O_RDWR|O_NDELAY)) == -1) {
		SGX_PERF_printf("Error opening bc driver - is bc_cat module inserted ?\n");
        return 3;
    }
    bc_param.count = 1;
    bc_param.width = inTextureWidth;
    bc_param.height = inTextureHeight;
    bc_param.fourcc = BC_PIX_FMT_UYVY;//pixel_fmt = PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY;           
    bc_param.type = BC_MEMORY_MMAP;
    if (ioctl(bcfd, BCIOREQ_BUFFERS, &bc_param) != 0) {
        SGX_PERF_printf("ERROR: BCIOREQ_BUFFERS failed\n");
        return 4;
    }
    if (ioctl(bcfd, BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
		SGX_PERF_printf("Error ioctl BCIOGET_BUFFERCOUNT");
        return 5;
    }
    if (ioctl_var.output == 0) {
        SGX_PERF_printf("no buffers available from driver \n");
        return 6;
    }

    /* Retrieve GL extension string */
    pszGLExtensions = glGetString(GL_EXTENSIONS);

    if (!pszGLExtensions || !strstr((char *)pszGLExtensions, "GL_IMG_texture_stream2"))
	{
		SGX_PERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
		return 1;
	}

    myglTexBindStreamIMG = (PFNGLTEXBINDSTREAMIMGPROC)eglGetProcAddress("glTexBindStreamIMG");
    myglGetTexStreamDeviceAttributeivIMG = 
        (PFNGLGETTEXSTREAMDEVICEATTRIBUTEIVIMGPROC)eglGetProcAddress("glGetTexStreamDeviceAttributeivIMG");
    myglGetTexStreamDeviceNameIMG = 
        (PFNGLGETTEXSTREAMDEVICENAMEIMGPROC)eglGetProcAddress("glGetTexStreamDeviceNameIMG");

    if(!myglTexBindStreamIMG || !myglGetTexStreamDeviceAttributeivIMG || !myglGetTexStreamDeviceNameIMG)
	{
		SGX_PERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
        return 2;
	}

    pszStreamDeviceName = myglGetTexStreamDeviceNameIMG(0);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG, &numBuffers);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG, &bufferWidth);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG, &bufferHeight);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG, &bufferFormat);

    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    SGX_PERF_printf("\nStream Device %s: numBuffers = %d, width = %d, height = %d, format = %x\n",
        pszStreamDeviceName, numBuffers, bufferWidth, bufferHeight, bufferFormat);

    for (idx = 0; idx < MAX_BUFFERS; idx++) {
        ioctl_var.input = idx;

        if (ioctl(bcfd, BCIOGET_BUFFERPHYADDR, &ioctl_var) != 0) {
            SGX_PERF_printf("BCIOGET_BUFFERADDR: failed\n");
            return 7;
        }
        SGX_PERF_printf("phyaddr[%d]: 0x%x\n", idx, ioctl_var.output);

        buf_paddr[idx] = (char*)ioctl_var.output;
        buf_vaddr[idx] = (char *)mmap(NULL, inTextureWidth*inTextureHeight*2,
                          PROT_READ | PROT_WRITE, MAP_SHARED,
                          bcfd, (long)buf_paddr[idx]);

        if (buf_vaddr[idx] == MAP_FAILED) {
            SGX_PERF_printf("Error: failed mmap\n");
            return 8;
        }

		if((inTextureWidth == 160) && (inTextureHeight == 128)) //only for 256x256
			memcpy(buf_vaddr[idx], yukio_160x128yuv, 160*128*2);
		else if((inTextureWidth == 720) && (inTextureHeight == 480)) //for VGA
		{
			FILE*fp = fopen("coastguard_d1_422.yuv","rb");
			if(!fp) {SGX_PERF_printf("Error opening YUV file\n"); return 9;}
			fread(buf_vaddr[idx],720*480*2,1,fp);
			fclose(fp);
		}
		else
		{
			for (int iii= 0; iii < inTextureHeight; iii++)
				memset(((char *) buf_vaddr[idx] + (inTextureWidth*iii*2)) , 0xa8, inTextureWidth*2);
		}
    }

	return 0;
}
void test8_deinit_texture_streaming()
{
	int idx;
    for (idx = 0; idx < MAX_BUFFERS; idx++) {
        if (buf_vaddr[idx] != MAP_FAILED)
            munmap(buf_vaddr[idx], inTextureWidth*inTextureHeight*2);
    }
    if (bcfd > -1)
        close(bcfd);
}

/* Texture streaming extension - nonEGLImage */
void test8()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i;
	int bufferIndex = 0;
	int err;
	static GLfloat texcoord_img[] = 
        {0,0, 1,0, 0,1, 1,1};
	float angle = 0;

#ifdef _ENABLE_TEST8_NO_USERPTR
	//initialise texture streaming
	err = test8_init_texture_streaming();
#else //USERPTR
	//initialise texture streaming - USERPTR
	err = test8_init_texture_streaming_userptr();
#endif
	if(err)
	{
		SGX_PERF_ERR_printf("TEST8: Init failed with err = %d\n", err);
		goto deinit;
	}
	//initialise gl vertices
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(inNumberOfObjectsPerSide, &pTexCoordArray);

	//override the texturecoords for this extension only
	glDisableVertexAttribArray(TEXCOORD_ARRAY);
	glEnableVertexAttribArray(TEXCOORD_ARRAY);
	glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, (const void*)texcoord_img);
	//override the texture binding
	//glEnable(GL_TEXTURE_STREAM_IMG);

	gettimeofday(&startTime, NULL);
	SGX_PERF_printf("TEST8: Entering DrawArrays loop\n");
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);
		if(inRotationEnabled)
		{
			calculate_rotation_z(mat_z, 0);
			calculate_rotation_y(mat_y, angle);
			calculate_rotation_x(mat_x, 0);
			matrix_mult(mat_z, mat_y, mat_temp);
			matrix_mult(mat_temp, mat_x, mat_final);
			glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
			angle += ANGLE_DELTA;
		}

		myglTexBindStreamIMG(0, bufferIndex);
		img_stream_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers (eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err) 
		SGX_PERF_ERR_printf("Error in glTexBindStream loop err = %d", err);
	SGX_PERF_printf("Exiting DrawArrays loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(8, diffTime2);
deinit:
#ifdef _ENABLE_TEST8_NO_USERPTR
	test8_deinit_texture_streaming();
#else
	test8_deinit_texture_streaming_userptr();
#endif
	common_deinit_gl_vertices(pVertexArray);
}
#endif



#ifdef _ENABLE_TEST9
/* Fill entire screen with single colour, read and render SVG */
void test9()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i;
	CPVRTSVGParser SVGParser;
	CPVRTSVGObject m_SVGObject;
	float angle = 0;
	VGfloat color[4] = { 1.0f, 1.0f, 0.0f, 1.0f }; /* Opaque yellow */
	PVRTMat3	m_Transform = PVRTMat3::Identity();
	CPVRTPVGObject* m_pOvgObj;
	char *tempSvgFilename = (char*)"tiger.svg";

	vgSeti(VG_SCISSORING, VG_FALSE);
	vgSetfv(VG_CLEAR_COLOR, 4, color);

	vgClear(0, 0, windowWidth, windowHeight);


	//Check if default filename needs to be used
	if((strstr(inSvgFileName,".svg") == 0) && (strstr(inSvgFileName,".pvg") == 0))
		inSvgFileName=tempSvgFilename;

	//Check if file is SVG or PVG file
	if(strstr(inSvgFileName, ".svg"))
	{
		if(!SVGParser.Load(inSvgFileName, &m_SVGObject, windowWidth, windowHeight))
		{
			SGX_PERF_ERR_printf("\nFailed to load svg file.\n"); 
			exit(-1);
		}

		gettimeofday(&startTime, NULL);
		for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
		{	
	  SGX_PERF_STARTPROFILEUNIT;		
			angle += 0.05f;
			m_Transform.RotationX(angle);
			m_SVGObject.SetTransformation(&m_Transform);
			m_SVGObject.Draw();
			common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT			
		}
		gettimeofday(&endTime, NULL);
	}
	else
	{
		m_pOvgObj = CPVRTPVGObject::FromFile(inSvgFileName);
		if(!m_pOvgObj)
		{
			SGX_PERF_ERR_printf("\nFailed to load pvg file.\n");
			exit(-1);
		}
		gettimeofday(&startTime, NULL);
		for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
		{
	  SGX_PERF_STARTPROFILEUNIT;		
			vgClear(0, 0, windowWidth, windowHeight);
			// Draw object
			m_pOvgObj->Draw();
			common_eglswapbuffers(eglDisplay, eglSurface);
SGX_PERF_ENDPROFILEUNIT			
		}
		gettimeofday(&endTime, NULL);
	}
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(9, diffTime2);
}
#endif

#ifdef _ENABLE_TEST10
#include "pvr2d.h"
PVR2DERROR ePVR2DStatus;
PVR2DDEVICEINFO* pDevInfo=0;
PVR2DCONTEXTHANDLE hPVR2DContext=0;
PVR2DMEMINFO* pFBMemInfo=0, *pSrcMemInfo_MemWrap = 0;
int nDeviceNum;
int nDevices;
PVR2DFORMAT eDisplayFormat;
long lStride;
int RefreshRate;
long lDisplayWidth;
long lDisplayHeight;
long lDisplayBitsPerPixel;
PPVR2DBLTINFO pBlt=0;
void test10_init()
{
	nDevices = PVR2DEnumerateDevices(0);
	pDevInfo = (PVR2DDEVICEINFO *) malloc(nDevices * sizeof(PVR2DDEVICEINFO));
	PVR2DEnumerateDevices(pDevInfo);
	nDeviceNum = pDevInfo[0].ulDevID;
	ePVR2DStatus = PVR2DCreateDeviceContext (nDeviceNum, &hPVR2DContext, 0);
	ePVR2DStatus = PVR2DGetFrameBuffer ( hPVR2DContext, PVR2D_FB_PRIMARY_SURFACE, &pFBMemInfo);
	ePVR2DStatus = PVR2DGetScreenMode(hPVR2DContext, &eDisplayFormat, &lDisplayWidth, &lDisplayHeight, &lStride, &RefreshRate);
	if(eDisplayFormat == PVR2D_ARGB8888)
		lDisplayBitsPerPixel = 32;
	else
		lDisplayBitsPerPixel = 16; //default
	memset(pFBMemInfo->pBase, 0xEF, pFBMemInfo->ui32MemSize);
	pBlt = (PVR2DBLTINFO *) calloc(1, sizeof(PVR2DBLTINFO));

    pBlt->CopyCode = PVR2DROPcopyInverted;
	pBlt->BlitFlags = PVR2D_BLIT_DISABLE_ALL;
	pBlt->pDstMemInfo = pFBMemInfo;
	pBlt->DstSurfWidth = lDisplayWidth;
	pBlt->DstSurfHeight = lDisplayHeight;
	pBlt->DstStride = lStride;
	pBlt->DstFormat = eDisplayFormat;

}
void test10() //Test PVR2D test cases
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i; 
	void* pLocalTexture = NULL;
	test10_init();

	gettimeofday(&startTime, NULL);
	SGX_PERF_printf("TESTa: Entering DrawArrays loop\n");
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++) //limiting to avoid memwrap issues
	{
	  SGX_PERF_STARTPROFILEUNIT;	
		//Test locality of memwrap
		pLocalTexture = malloc(4 * inTextureWidth * inTextureHeight);
		if(!pLocalTexture) {printf("Error malloc, exit\n"); exit(1);}
		ePVR2DStatus = PVR2DMemWrap
							(hPVR2DContext, pLocalTexture, 0, 
							(4 * inTextureWidth * inTextureHeight),
							NULL,
							&pSrcMemInfo_MemWrap
							);
		if(pSrcMemInfo_MemWrap == NULL) 
		{
			printf("Fatal Error in pvr2dmemwrap, exiting\n");
			exit(1);
		}
		pBlt->DSizeX = inTextureWidth;
		pBlt->DSizeY = inTextureHeight;
		pBlt->DstX = 0;
		pBlt->DstY = 0;

		pBlt->pSrcMemInfo = pSrcMemInfo_MemWrap;
		pBlt->SrcSurfWidth = inTextureWidth;
		pBlt->SrcSurfHeight = inTextureHeight;
		pBlt->SrcFormat = PVR2D_ARGB8888;
		pBlt->SrcStride = ( ( ((inTextureWidth + 31) & ~31) * 32) + 7) >> 3;
		pBlt->SizeX = pBlt->DSizeX;
		pBlt->SizeY = pBlt->DSizeY;
        pBlt->CopyCode = PVR2DROPcopy;
		ePVR2DStatus = PVR2DBlt(hPVR2DContext, pBlt);		
	 	ePVR2DStatus = PVR2DQueryBlitsComplete(hPVR2DContext, pFBMemInfo, 1);
		//unwrap again
		PVR2DMemFree(hPVR2DContext, pSrcMemInfo_MemWrap);
		//free texture
		if(pLocalTexture) free(pLocalTexture);
	  SGX_PERF_ENDPROFILEUNIT;		
	}
	SGX_PERF_printf("Exiting Draw loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log('a', diffTime2);

}
#endif //TEST10

#ifdef _ENABLE_TEST13
#define NUM_LINES_TEST13 10000 //1000 lines test
/* Draw multiple lines */
void test13()
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
	for(i = 0;(i < numTestIterations)&&(!quitSignal);i ++)
	{	
//		SGX_PERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);		
		glDrawArrays(GL_LINES, 0, NUM_LINES_TEST13);
		common_eglswapbuffers(eglDisplay, eglSurface);
//		SGX_PERF_ENDPROFILEUNIT;		
	}
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(13, diffTime2);
}
#endif

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
	  SGX_PERF_STARTPROFILEUNIT;	
	  //Switch to surface 2
	  eglMakeCurrent(eglDisplay, eglSurface2, eglSurface2, eglContext);		
	  //Switch back to surface 1 and draw to surface 1
	  eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);		
		glClear(GL_COLOR_BUFFER_BIT);		
		common_gl_draw(inNumberOfObjectsPerSide);
		common_eglswapbuffers(eglDisplay, eglSurface);
	  SGX_PERF_ENDPROFILEUNIT;    		
	}
	err = glGetError();
	if(err)
		SGX_PERF_ERR_printf("Error in gldraw err = %x", err);		
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(14, diffTime2);

	common_deinit_gl_vertices(pVertexArray);
	common_deinit_gl_texcoords(pTexCoordArray);
}
#endif //test14

#ifdef _ENABLE_TEST15

#include "pvr2d.h"

static PVR2DERROR ePVR2DStatus1;
static PVR2DDEVICEINFO* pDevInfo1 = 0;
static PVR2DCONTEXTHANDLE hPVR2DContext1 = 0;
static PVR2DMEMINFO *pSrcMemInfo_MemWrap1=0, *pDstMemInfo_MemWrap = 0;
static PPVR2DBLTINFO pBlt1=0;
static PVR2DBLTINFO pvr2dBltInfo;

unsigned int 
test15_process(
void* srcPtr,
void* dstPtr,
unsigned int srcWidthPixels,
unsigned int srcHeightPixels,
unsigned int srcStrideBytes,
unsigned int dstWidthPixels,
unsigned int dstHeightPixels,
unsigned int dstStrideBytes,
unsigned int srcBytesPerPixel,
unsigned int dstBytesPerPixel
)
{
  int nDeviceNum;
  int nDevices;

	nDevices = PVR2DEnumerateDevices(0);
	//printf("nDevices = %d\n", nDevices);
	if(nDevices <= 0) 
	{
		printf("FATAL error - no devices !\n");
		return 1;
	}
	pDevInfo1 = (PVR2DDEVICEINFO *) malloc(nDevices * sizeof(PVR2DDEVICEINFO));
	if(!pDevInfo1)
	{
		printf("FATAL error - could not allocate memory for device info !\n");
		return 2;
	}
	PVR2DEnumerateDevices(pDevInfo1);
	nDeviceNum = pDevInfo1[0].ulDevID;
	//Create the context
	ePVR2DStatus1 = PVR2DCreateDeviceContext (nDeviceNum, &hPVR2DContext1, 0);
	if(!hPVR2DContext1)
	{
		printf("FATAL error - could not create device context !\n");
		return 3;
	}


  //Wrap the source
	ePVR2DStatus1 = PVR2DMemWrap(
                  hPVR2DContext1, srcPtr, 0, 
      						(srcHeightPixels * srcStrideBytes),
      						NULL,
      						&pSrcMemInfo_MemWrap1
      						);

	if(pSrcMemInfo_MemWrap1 == NULL) 
	{
		SGX_PERF_printf("Fatal Error in memwrap of source %x, err = %d\n",
        (unsigned int)srcPtr, ePVR2DStatus1);
		return 1;
	}

	pBlt1 = &pvr2dBltInfo;
	memset(pBlt1, 0, sizeof(PVR2DBLTINFO));
	pBlt1->BlitFlags = PVR2D_BLIT_DISABLE_ALL;
	pBlt1->pDstMemInfo = pDstMemInfo_MemWrap;
	pBlt1->DstSurfWidth = dstWidthPixels;
	pBlt1->DstSurfHeight = dstHeightPixels;
	pBlt1->DstStride = dstStrideBytes;
	if(dstBytesPerPixel == 4)
    pBlt1->DstFormat = PVR2D_ARGB8888;
  else if(dstBytesPerPixel == 2)
    pBlt1->DstFormat = PVR2D_RGB565;
  //Wrap the destination
	ePVR2DStatus1 = PVR2DMemWrap(
                  hPVR2DContext, dstPtr, 0, 
      						(dstBytesPerPixel * dstHeightPixels * dstWidthPixels),
      						NULL,
      						&pDstMemInfo_MemWrap
      						);

	if(pDstMemInfo_MemWrap == NULL) 
	{
		SGX_PERF_printf("Fatal Error in pvr2dmemwrap of %x, exiting\n", dstPtr);
		return 1;
	}

	pBlt1->DSizeX = dstWidthPixels;
	pBlt1->DSizeY = dstHeightPixels;
	pBlt1->DstX = 0;
	pBlt1->DstY = 0;

	pBlt1->pSrcMemInfo = pSrcMemInfo_MemWrap1;
	pBlt1->SrcSurfWidth = srcWidthPixels;
	pBlt1->SrcSurfHeight = srcHeightPixels;
	pBlt1->SrcFormat = PVR2D_YUV422_UYVY; //PVR2D_YUV422_VYUY; //
	pBlt1->SrcStride = srcStrideBytes;
	pBlt1->SizeX = pBlt1->DSizeX;     // what is this to be ?
	pBlt1->SizeY = pBlt1->DSizeY;
  pBlt1->CopyCode = PVR2DROPcopy;
	ePVR2DStatus1 = PVR2DBlt(hPVR2DContext1, pBlt1);		
 	ePVR2DStatus1 = PVR2DQueryBlitsComplete(hPVR2DContext1, pSrcMemInfo_MemWrap1, 1);
	SGX_PERF_printf("src Blt Complete Query = %d\n", ePVR2DStatus1);
 	ePVR2DStatus1 = PVR2DQueryBlitsComplete(hPVR2DContext, pDstMemInfo_MemWrap, 1);
	SGX_PERF_printf("dst Blt Complete Query = %d\n", ePVR2DStatus1);
	//unwrap again
	//PVR2DMemFree(hPVR2DContext1, pSrcMemInfo_MemWrap1);
	//PVR2DMemFree(hPVR2DContext1, pDstMemInfo_MemWrap);
	//destroy context
  PVR2DDestroyDeviceContext(hPVR2DContext1);
  if(pDevInfo1)
    free(pDevInfo1);	

  return 0;
}

void test15()
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	unsigned int i;
	//with switching contexts, still drawing to surface1
	gettimeofday(&startTime, NULL);


	unsigned int ret;
	void* outBuffer = malloc(inTextureWidth*inTextureHeight*2); //always to RGB565 mem buffer only
	if(!outBuffer) 
	{	
		printf("TEST15 FATAL error - could not allocate output buffer!\n");
		return;
	}

	for(i = 0;(i < (unsigned int)numTestIterations)&&(!quitSignal);i ++)	
	{	
	  SGX_PERF_STARTPROFILEUNIT;	
		ret = test15_process(
		    textureData,
		    (void*)outBuffer,
		    inTextureWidth,
		    inTextureHeight,
		    inTextureWidth*2,
		    inTextureWidth >> 1, //same parameters for output as well
		    inTextureHeight >> 1,
		    (inTextureWidth >>1)*2,
		    2,
		    2
		    );
	  SGX_PERF_ENDPROFILEUNIT;    		
	}

	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/numTestIterations;
	common_log(14, diffTime2);

	//Free output buffer
	if(outBuffer) free(outBuffer);	
}
#endif

/* Signal handler for clean closure of GL */
void sgxperf_signal_handler(int reason) 
{ 
  SGX_PERF_ERR_printf("\nGot quit signal - Results will be inaccurate!\n"); 
  quitSignal = 1; 
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
	NATIVE_PIXMAP_STRUCT* pNativePixmap = NULL;


	const char* pszFragNoTextureShader = "\
		void main (void)\
		{\
				gl_FragColor = vec4(0.8,0.2,0.1,1.0);\
		}";
	const char* pszVertNoTextureShader = "\
		attribute highp   vec4  inVertex;\
		uniform mediump mat4  MVPMatrix;\
		attribute mediump vec2  inTexCoord;\
		varying mediump vec2  TexCoord;\
		void main()\
		{\
			gl_Position = MVPMatrix * inVertex;\
		}";

	const char* pszFragTextureShader = "\
		uniform sampler2D  sTexture; \
		varying mediump vec2  TexCoord; \
    mediump vec3 tempColor;\
		void main (void)\
		{\
        tempColor = vec3(texture2D(sTexture, TexCoord));\
				gl_FragColor.r = tempColor.r;\
				gl_FragColor.g = tempColor.g;\
				gl_FragColor.b = tempColor.b;\
				gl_FragColor.a = 0.0; \
		}";
	const char* pszFragIMGTextureStreamShader = "\
			 #ifdef GL_IMG_texture_stream2\n \
			 #extension GL_IMG_texture_stream2 : enable \n \
			 #endif \n \
			 varying mediump vec2 TexCoord; \
			 uniform samplerStreamIMG sTexture; \
			 void main(void) \
			 {	\
				gl_FragColor = textureStreamIMG(sTexture, TexCoord); \
			 }";
	const char* pszFragEdgeYUVDetectShader = "\
		 #ifdef GL_IMG_texture_stream2\n \
		 #extension GL_IMG_texture_stream2 : enable \n \
		 #endif \n \
		 varying mediump vec2 TexCoord; \
		 uniform samplerStreamIMG sTexture; \
		mediump float kernel[9];\
		mediump vec2 pixelOffset[9];\
		uniform mediump float rowSize;\
		uniform mediump float columnSize;\
		mediump float rowStepSize = 2.0/rowSize;\
		mediump float columnStepSize=2.0/columnSize;\
		void main(void) \
		{	\
			int i;\
			highp vec4 tempCurrVal = vec4(0.0);\
			highp vec4 tempVal;\
			kernel[0] = 0.0;\
			kernel[1] = 1.0;\
			kernel[2] = 0.0;\
			kernel[3] = 1.0;\
			kernel[4] = -4.0;\
			kernel[5] = 1.0;\
			kernel[6] = 0.0;\
			kernel[7] = 1.0;\
			kernel[8] = 0.0;\
			pixelOffset[0] = vec2(-rowStepSize,-columnStepSize);\
			pixelOffset[1] = vec2(0,-columnStepSize);\
			pixelOffset[2] = vec2(rowStepSize,-columnStepSize);\
			pixelOffset[3] = vec2(-rowStepSize,0);\
			pixelOffset[4] = vec2(0,0);\
			pixelOffset[5] = vec2(rowStepSize,0);\
			pixelOffset[6] = vec2(-rowStepSize,columnStepSize);\
			pixelOffset[7] = vec2(0,columnStepSize);\
			pixelOffset[8] = vec2(rowStepSize,columnStepSize);\
			for(i = 0;i < 9;i ++)\
			{\
				tempVal = textureStreamIMG(sTexture,TexCoord.st + pixelOffset[i]);\
				tempCurrVal += (tempVal * kernel[i]);\
			}\
			gl_FragColor = tempCurrVal;\
		}";
const	char* pszFragEdgeRGBDetectShader = "\
		uniform sampler2D sTexture;\
		varying mediump vec2 TexCoord;\
		mediump float kernel[9];\
		mediump vec2 pixelOffset[9];\
		uniform mediump float rowSize;\
		uniform mediump float columnSize;\
		mediump float rowStepSize = 2.0/rowSize;\
		mediump float columnStepSize=2.0/columnSize;\
		void main(void)\
		{\
			int i;\
			highp vec4 tempCurrVal = vec4(0.0);\
			highp vec4 tempVal;\
			kernel[0] = 0.0;\
			kernel[1] = 1.0;\
			kernel[2] = 0.0;\
			kernel[3] = 1.0;\
			kernel[4] = -4.0;\
			kernel[5] = 1.0;\
			kernel[6] = 0.0;\
			kernel[7] = 1.0;\
			kernel[8] = 0.0;\
			pixelOffset[0] = vec2(-rowStepSize,-columnStepSize);\
			pixelOffset[1] = vec2(0,-columnStepSize);\
			pixelOffset[2] = vec2(rowStepSize,-columnStepSize);\
			pixelOffset[3] = vec2(-rowStepSize,0);\
			pixelOffset[4] = vec2(0,0);\
			pixelOffset[5] = vec2(rowStepSize,0);\
			pixelOffset[6] = vec2(-rowStepSize,columnStepSize);\
			pixelOffset[7] = vec2(0,columnStepSize);\
			pixelOffset[8] = vec2(rowStepSize,columnStepSize);\
			for(i = 0;i < 9;i ++)\
			{\
				tempVal = texture2D(sTexture,TexCoord.st + pixelOffset[i]);\
				tempCurrVal += (tempVal * kernel[i]);\
			}\
			gl_FragColor = tempCurrVal;\
		}";
	const char* pszVertTextureShader = "\
		attribute highp   vec4  inVertex;\
		uniform mediump mat4  MVPMatrix;\
		attribute mediump vec2  inTexCoord;\
		varying mediump vec2  TexCoord;\
		void main()\
		{\
			gl_Position = MVPMatrix * inVertex;\
			TexCoord = inTexCoord;\
		}";

	const char *helpString = \
"TI SGX OpenGLES2.0+VG Benchmarking Program For Linux. \n\
Usage: sgxperf2 testID texwdth texht rot texfmt svgfile numObjects surfaceType numIter fps cookie\n\
testID = ID of test case to run, takes one of the below values: \n\
        0 - Print supported extensions and number \n\
		1 - Fill entire screen with single colour, no objects \n\
		2 - Draw a coloured object filling entire screen without texture\n\
		3 - Draw a textured object filling entire screen \n\
		4 - Alpha blending full surface texture \n\
		5 - Alpha blending full surface WITHOUT texture \n\
		6 - EGL_NATIVE_PIXMAP_KHR (needs CMEM) \n\
		7 - EGL_GL_TEXTURE_2D_KHR (needs CMEM)\n\
		8 - GL_IMG_texture_stream (needs CMEM, BUFFERCLASS_TI)\n\
		9 - OpenVG SVG/PVG performance test (needs svg/pvgfile name) \n\
		10 - PVR2D copyblit benchmark test \n\
		11 - Lenna Edge Detection benchmark test \n\
		12 - Edge detection (test8) \n\
		13 - Line drawing \n\
		14 - Context switch \n\
		15 - YUV-RGB converter - PVR2D \n\
texwdth = width in pixels of input texture \n\
texht = height in pixels of input texture \n\
rot = 1 to enable rotation of objects, 0 to disable (default) \n\
texfmt = 2 for ARGB input texture (default), 0 for RGB565 texture\n\
svgfile = complete path name of the svg/pvg file to be tested (.svg or .pvg)\n\
numObjects = number of on-screen objects that will be drawn vertically and horizontally\n\
surfaceType = type of surface(0 = WindowSurface, 1 = pixmapSurface_16b, 2 = pixmapSurface_32b)\n\
numIterations = number of iterations (> 0)\n\
fps = Target frames per second \n\
cookie = unique number for running this instance of the test (any number > 0) \n\
Ex. to test TEST3 with 256x256 32bit texture on LCD with 1 object at 30 fps 100 times, enter \
'./sgxperf2 3 256 256 0 2 0 1 0 100 30 1234'\n";

	/* Pre init step - check the arguments */
	if(argc < 11)
	{
		SGX_PERF_ERR_printf("Error: Invalid number of operands \n\n");
		SGX_PERF_ERR_printf(helpString);
		exit(-1);
	}
	testID = atol(argv[1]);
	if(testID > MAX_TEST_ID || testID < 0)
	{
		SGX_PERF_ERR_printf("Error: No test available with this ID %d\n\n", testID);
		SGX_PERF_ERR_printf(helpString);
		exit(-1);
	}
	inNumberOfObjectsPerSide = atol(argv[7]);
	inSurfaceType = atol(argv[8]);

	if((testID == 9) && argc < 7)
	{
		SGX_PERF_ERR_printf("Error: SVG needs file path\n\n");
		SGX_PERF_ERR_printf(helpString);
		exit(-1);
	}
	else
		inSvgFileName = argv[6];

	if(argc < 4)
	{
		if(testID > 2) //1 and 2 do not need textures
		{
			SGX_PERF_ERR_printf("Error: Invalid number of operands for selected testID %d\n\n", testID);
			SGX_PERF_ERR_printf(helpString);
			exit(-1);
		}
	}
	else
	{
		inTextureWidth = atol(argv[2]);
		inTextureHeight = atol(argv[3]);
	}
	inRotationEnabled = 0;
	if(argc >= 5)
		inRotationEnabled = atol(argv[4]);
	inPixelFormat = 0;
	if(argc >= 6)
		inPixelFormat = atol(argv[5]);
	if(inPixelFormat != SGXPERF_RGB565 && inPixelFormat != SGXPERF_ARGB8888 && inPixelFormat != SGXPERF_BYTE8)
	{
		SGX_PERF_ERR_printf("Error: Unsupported pixel format for texture %d \n\n", inPixelFormat);
		SGX_PERF_ERR_printf(helpString);
		exit(-1);
	}
  
  //read extra params
  numTestIterations = atol(argv[9]);
  inFPS =atol(argv[10]); 
  msToSleep = 1000/inFPS;
  cookie = argv[11];
    
	if((inTextureWidth > 8000) || (inTextureHeight > 8000))
	{
		SGX_PERF_ERR_printf("Error: Width or Height exceeds 8000 \n\n");
		SGX_PERF_ERR_printf(helpString);
		exit(-1);
	}
#ifndef _ENABLE_CMEM
	if(testID == 6 || testID == 7 || inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16 || 
                                    inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32)
	{
		SGX_PERF_ERR_printf("ERROR: Cannot run native pixmap tests without CMEM\n");
		exit(-1);
	}
#endif
#ifndef _ENABLE_BUFFERCLASS
	if(testID == 8)
	{
		SGX_PERF_ERR_printf("ERROR: Cannot run test8 without BUFFERCLASS driver\n");
		exit(-1);
	}
#endif
	//for pixmap related tests, surface cannot be pixmap
	if(testID == 6 && inSurfaceType != SGXPERF_SURFACE_TYPE_WINDOW)
	{
		SGX_PERF_ERR_printf("ERROR: Cannot run native pixmap eglimage test with pixmap surface\n");
		goto cleanup;
	}

  signal(SIGINT, sgxperf_signal_handler);

#ifdef _ENABLE_CMEM
	//Initialise the CMEM module. 
	//CMEM ko should be inserted before this point
	SGX_PERF_printf("Configuring CMEM\n");
	CMEM_init();
#endif

	//Allocate texture for use in GL texturing modes(can also be done from CMEM if memory permits
	textureData = (unsigned int*)malloc(inTextureWidth*inTextureHeight*4);
	if(!textureData)
		SGX_PERF_ERR_printf("ERROR: No malloc memory for allocating texture!\n");
	set_texture(inTextureWidth, inTextureHeight, (unsigned char*)textureData, inPixelFormat);

	//initialise egl
	err = common_eglinit(testID, inSurfaceType, &pNativePixmap);
	if(err)
	{
		SGX_PERF_ERR_printf("ERROR: eglinit - err = %d\n", err);
		goto cleanup;
	}

	GLuint uiFragShader, uiVertShader;		// Used to hold the fragment and vertex shader handles
	GLuint uiProgramObject;					// Used to hold the program handle (made out of the two previous shaders

	// Create the fragment shader object
	uiFragShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load the source code into it
	if((testID == 3) || (testID == 5) || (testID == 6) || (testID == 7)  || 
        (testID == 14))
		glShaderSource(uiFragShader, 1, (const char**)&pszFragTextureShader, NULL);
	else if(testID == 11) //Edge detect with RGB input
		glShaderSource(uiFragShader, 1, (const char**)&pszFragEdgeRGBDetectShader, NULL);
	else if(testID == 12) //Edge detect with YUV input
		glShaderSource(uiFragShader, 1, (const char**)&pszFragEdgeYUVDetectShader, NULL);
	else if(testID == 8) //IMG texture stream2
		glShaderSource(uiFragShader, 1, (const char**)&pszFragIMGTextureStreamShader, NULL);
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
		SGX_PERF_ERR_printf("Failed to compile fragment shader: %s\n", pszInfoLog);
		delete [] pszInfoLog;
		goto cleanup;
	}

	// Loads the vertex shader in the same way
	uiVertShader = glCreateShader(GL_VERTEX_SHADER);
	if((testID == 3) || (testID == 5) || (testID == 6) || (testID == 7) || 
        (testID == 8) || (testID == 11) || (testID == 12)
           || (testID == 14))
		glShaderSource(uiVertShader, 1, (const char**)&pszVertTextureShader, NULL);
	else
		glShaderSource(uiVertShader, 1, (const char**)&pszVertNoTextureShader, NULL);

	glCompileShader(uiVertShader);
    glGetShaderiv(uiVertShader, GL_COMPILE_STATUS, &bShaderCompiled);

	if (!bShaderCompiled)
	{
		int i32InfoLogLength, i32CharsWritten;
		glGetShaderiv(uiVertShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		char* pszInfoLog = new char[i32InfoLogLength];
        glGetShaderInfoLog(uiVertShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
		SGX_PERF_ERR_printf("Failed to compile vertex shader: %s\n", pszInfoLog);
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
		SGX_PERF_ERR_printf("Failed to link program: %s\n", pszInfoLog);
		delete [] pszInfoLog;
		goto cleanup;
	}

	// Actually use the created program
    glUseProgram(uiProgramObject);
	//set rotation variables to init
	matrixLocation = glGetUniformLocation(uiProgramObject, "MVPMatrix");
	calculate_rotation_z(mat_z, 0);
	calculate_rotation_y(mat_y, 0);
	calculate_rotation_x(mat_x, 0);
	matrix_mult(mat_z, mat_y, mat_temp);
	matrix_mult(mat_temp, mat_x, mat_final);
	glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, mat_final);
	
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
				SGX_PERF_ERR_printf("\nTESTID = 0: SUPPORTED EXTENSIONS = \n%s\n", extensions);
				break;
			case 1:
				/* Fill entire screen with single colour, no objects */
				test1();
				break;
			case 2:
				/* Draw a coloured triangle filling entire screen */
				test2();
				break;
			case 3:
				/* Move a coloured triangle of half screen size to another half with same parameters */
				test3();
				break;
			case 4:
				/* Alpha blending full surface texture */
				test4();	
				break;
			case 5:
				/* Alpha blending full surface WITHOUT texture */
				test5();
				break;
			case 6:
#ifdef _ENABLE_TEST6
				/* EGL_NATIVE_PIXMAP_KHR */
				//Create native pixmap with CMEM
        //Note that this pixmap object is for a texture, not surface
				common_create_native_pixmap(inPixelFormat, 
						inTextureWidth, inTextureHeight, &pNativePixmap);
				//copy to it with native pixel format
				set_texture(pNativePixmap->lWidth, pNativePixmap->lHeight, 
						(unsigned char*)pNativePixmap->lAddress, inPixelFormat);
				test6(pNativePixmap);
				common_delete_native_pixmap(pNativePixmap);
				pNativePixmap = NULL;
#endif
				break;
			case 7:
#ifdef _ENABLE_TEST7
				/* EGL_GL_TEXTURE_2D_KHR */
				test7();
#endif
				break;
			case 8:
#ifdef _ENABLE_TEST8
				/* GL_IMG_texture_stream */
				test8();
#endif
				break;
			case 9:
				/* SVG file test */
				test9();
				break;
			case 10:
				/* PVR2D test */
				test10();
				break;
			case 11:
				/* Edge detection test - test3 with new shader */
				test3();
				break;
#ifdef _ENABLE_TEST8
			case 12:
				/* Edge detection test - test8 with new shader */
				test8();
				break;
#endif
			case 13:
				/* Line drawing */
				test13();
				break;
#ifdef _ENABLE_TEST14				
			case 14:
				/* Context Switch */
				test14();
				break;
#endif				
#ifdef _ENABLE_TEST15
			case 15:
				/* YUV-RGB conversion with PVR2D */
				test15();
				break;
#endif
			default:
				/* Sorry guys, we tried ! */
				break;
		}
#ifdef _ENABLE_CMEM
	/* PIXMAP loopback test  */
	if(inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16 || inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32) 
	{
#if 1
		/* Use this for checking out the output in the pixmap */
		/* For DEBUGGING ONLY */
		FILE *fp = fopen("pixmap.raw", "wb");
		int numbytes;
		if(inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_32) numbytes = 4; //ARGB8888
		if(inSurfaceType == SGXPERF_SURFACE_TYPE_PIXMAP_16) numbytes = 2; //RGB565
		SGX_PERF_ERR_printf("Writing pixmap data to file pixmap.raw\n");
		fwrite((void*)pNativePixmap->lAddress, 1, inTextureWidth*inTextureHeight*numbytes, fp);
		fclose(fp);
#endif
	}
#endif //CMEM needed for pixmap loopback
	// Frees the OpenGL handles for the program and the 2 shaders
	glDeleteProgram(uiProgramObject);
	glDeleteShader(uiFragShader);
	glDeleteShader(uiVertShader);

cleanup:
	if(textureData) 
		free(textureData);
	common_egldeinit(testID, pNativePixmap);
#ifdef _ENABLE_CMEM
	CMEM_exit();
#endif

	return 0;
}

/******************************************************************************
 End of file 
******************************************************************************/


