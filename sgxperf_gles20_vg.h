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

#define SGXPERF_STARTPROFILEUNIT {}
#define SGXPERF_ENDPROFILEUNIT {}

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

struct globalStruct
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
void test2(struct globalStruct *globals);
void test3(struct globalStruct *globals);
void test4(struct globalStruct *globals);
void test5(struct globalStruct *globals);
void test6(struct globalStruct *globals);
void test7(struct globalStruct *globals);
void test8(struct globalStruct *globals);
void test10(struct globalStruct *globals);
void test11(struct globalStruct *globals);
void test12(struct globalStruct *globals);
void test13(struct globalStruct *globals);
void test14(struct globalStruct *globals);
void test15(struct globalStruct *globals);
void test16(struct globalStruct *globals);
void test17(struct globalStruct *globals);

int dummy_printf(const char* format, ...);
void common_eglswapbuffers(
			struct globalStruct *globals,
						   EGLDisplay eglDisplay, 
						   EGLSurface eglSurface
						   );
int common_init_gl_vertices(int numObjectsPerSide, GLfloat **vertexArray);
void common_deinit_gl_vertices(GLfloat *vertexArray);
int common_init_gl_texcoords(int numObjectsPerSide, GLfloat **textureCoordArray);
void common_deinit_gl_texcoords(GLfloat *pTexCoordArray);
void common_gl_draw(struct globalStruct *globals, int numObjects);
void eglimage_gl_draw(int numObjects);
void img_stream_gl_draw(int numObjects);
void common_log(struct globalStruct *globals, int testid, unsigned long time);
unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2);

void add_texture(int width, int height, void* data, int pixelFormat);

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

