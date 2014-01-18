#ifndef _ENABLE_SGXPERF_GLES20_VG_H
#define _ENABLE_SGXPERF_GLES20_VG_H

#define GL_CHECK(x) \
		{	\
			x; \
			int err = glGetError(); \
			printf("GL Error = %x for %s\n", err, (char*)(#x)); \
		}



#define MAX_TEST_ID 17 //Max number of available tests
#define SGXPERF_VERSION 1.1

#define _ENABLE_TEST1 /* Fill entire screen with single colour, no objects */
#define _ENABLE_TEST2 /* Draw a coloured rect filling entire screen */
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
#define _ENABLE_TEST10 /* PVR2D Blit */
#define _ENABLE_TEST13 /* Filled Lines */
#define _ENABLE_TEST14 /* multi surface context switch */
#define _ENABLE_TEST15 /* YUV-RGB converter with PVR2D */
#define _ENABLE_TEST16 /* YUV EGLIMAGE with TI extensions */
#define _ENABLE_TEST17 /* FBO */

#define SGXPERF_printf dummy_printf
#define SGXPERF_ERR_printf printf

#define SGXPERF_STARTPROFILEUNIT {gettimeofday(&unitStartTime, NULL);}  
#define SGXPERF_ENDPROFILEUNIT {gettimeofday(&unitEndTime, NULL);\
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

//TODO - Retrieve page size
#define PAGE_SIZE  4096

//Delta by which rotation happens when rotation is enabled
#define ANGLE_DELTA 0.02f


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

typedef struct globalStruct
{
	EGLConfig eglConfig;
	EGLDisplay eglDisplay;
	EGLSurface eglSurface;
	EGLSurface eglSurface2;
	EGLContext eglContext;
	GLuint textureId0;
	int matrixLocation;
	float mat_x[16];
	float mat_y[16];
	float mat_z[16];
	float mat_temp[16];
	float mat_final[16];
	int inTextureWidth;
	int inTextureHeight;
	int inPixelFormat;
	char *inSvgFileName;
	int inNumberOfObjectsPerSide;
	int inSurfaceType;
	int inRotationEnabled;
	int windowWidth;
	int windowHeight;
	int quitSignal;
	int numTestIterations;
	int inFPS;
	unsigned int msToSleep;
	char* cookie;
	unsigned int *_textureData;
	unsigned int *textureData;
};


/* Functions */
void test1(struct globalStruct *globals);
void test2();
void test3();
void test4();
void test5();
void test6();
void test7();
void test8();
void test9();
void test10();
void test11();
void test12();
void test13();
void test14();
void test15();
void test16();
void test17();

void common_eglswapbuffers(
						   EGLDisplay eglDisplay, 
						   EGLSurface eglSurface
						   );
int common_init_gl_vertices(int numObjectsPerSide, GLfloat **vertexArray);
void common_deinit_gl_vertices(GLfloat *vertexArray);
int common_init_gl_texcoords(int numObjectsPerSide, GLfloat **textureCoordArray);
void common_gl_draw(int numObjects);
void eglimage_gl_draw(int numObjects);
void img_stream_gl_draw(int numObjects);
void common_log(int testid, unsigned long time);
unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2);

extern const char* pszFragNoTextureShader;
extern const char* pszVertNoTextureShader;
extern const char* pszFragTextureShader;
extern const char* pszFragIMGTextureStreamShader;
extern const char* pszFragEGLImageShader;
extern const char* pszFragEdgeYUVDetectShader;
extern const char* pszFragEdgeRGBDetectShader;
extern const char* pszVertTextureShader;
extern const char* helpString;


#endif //_ENABLE_SGXPERF_GLES20_VG_H

