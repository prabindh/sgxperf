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
int test8_init_texture_streaming_userptr(struct globalStruct *globals)
{
    const GLubyte *pszGLExtensions;
    const GLubyte *pszStreamDeviceName;    
	  int err;  
	
    //setup CMEM pointers  
    virtualAddress = CMEM_alloc(globals->inTextureWidth*globals->inTextureHeight*4, &cmemParams);
    if(!virtualAddress)                                      
    {
		  SGXPERF_printf("Error in CMEM_alloc\n");
      return 1;
    }
    physicalAddress = CMEM_getPhys(virtualAddress);
    if(!physicalAddress)                                      
    {
		  SGXPERF_printf("Error in CMEM_getPhys\n");
      return 2;
    }
	 //setup driver now
    if ((bcfd = open(BC_CAT_DRV, O_RDWR|O_NDELAY)) == -1) {
		  SGXPERF_printf("Error opening bc driver - is bufferclass_ti module inserted ?\n");  
      CMEM_free((void*)virtualAddress, &cmemParams);
        return 3;
    }
    bc_param.count = 1;
    bc_param.width = globals->inTextureWidth;
    bc_param.height = globals->inTextureHeight;
    bc_param.fourcc = BC_PIX_FMT_YUYV;           
    bc_param.type = BC_MEMORY_USERPTR;
    SGXPERF_printf("About to BCIOREQ_BUFFERS \n");        
    err = ioctl(bcfd, BCIOREQ_BUFFERS, &bc_param);
    if (err != 0) {    
      CMEM_free(virtualAddress, &cmemParams);
        SGXPERF_ERR_printf("ERROR: BCIOREQ_BUFFERS failed err = %x\n", err);
        return 4;
    }                                                      
    SGXPERF_printf("About to BCIOGET_BUFFERCOUNT \n");
    if (ioctl(bcfd, BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {    
      CMEM_free(virtualAddress, &cmemParams);
		  SGXPERF_printf("Error ioctl BCIOGET_BUFFERCOUNT");
        return 5;
    }
    if (ioctl_var.output == 0) {    
        CMEM_free(virtualAddress, &cmemParams);
        SGXPERF_printf("Error no buffers available from driver \n");
        return 6;
    }
    //For USERPTR mode, set the buffer pointer first
    SGXPERF_printf("About to BCIOSET_BUFFERADDR \n");            
    buf_pa.index = 0;    
    buf_pa.pa = (int)physicalAddress;    
    buf_pa.size = globals->inTextureWidth * globals->inTextureHeight * 2;
    if (ioctl(bcfd, BCIOSET_BUFFERPHYADDR, &buf_pa) != 0) 
    {       
        CMEM_free(virtualAddress, &cmemParams);
        SGXPERF_printf("Error BCIOSET_BUFFERADDR failed\n");
        return 7;
    }
        
    /* Retrieve GL extension string */
    pszGLExtensions = glGetString(GL_EXTENSIONS);

    if (!pszGLExtensions || !strstr((char *)pszGLExtensions, "GL_IMG_texture_stream2"))
	{
    CMEM_free(virtualAddress, &cmemParams);
		SGXPERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
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
		  SGXPERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
      return 9;
	}

    pszStreamDeviceName = myglGetTexStreamDeviceNameIMG(0);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG, &numBuffers);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG, &bufferWidth);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG, &bufferHeight);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG, &bufferFormat);

    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    SGXPERF_printf("\nStream Device %s: numBuffers = %d, width = %d, height = %d, format = %x\n",
        pszStreamDeviceName, numBuffers, bufferWidth, bufferHeight, bufferFormat);

		{
			for (int iii= 0; iii < globals->inTextureHeight; iii++)
				memset(((char *) virtualAddress + (globals->inTextureWidth*iii*2)) , 0xa8, globals->inTextureWidth*2);
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

int test8_init_texture_streaming(struct globalStruct *globals)
{
    const GLubyte *pszGLExtensions;
    const GLubyte *pszStreamDeviceName;
	int idx;

	//setup driver now
    if ((bcfd = open(BC_CAT_DRV, O_RDWR|O_NDELAY)) == -1) {
		SGXPERF_printf("Error opening bc driver - is bc_cat module inserted ?\n");
        return 3;
    }
    bc_param.count = 1;
    bc_param.width = globals->inTextureWidth;
    bc_param.height = globals->inTextureHeight;
    bc_param.fourcc = BC_PIX_FMT_UYVY;//pixel_fmt = PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY;           
    bc_param.type = BC_MEMORY_MMAP;
    if (ioctl(bcfd, BCIOREQ_BUFFERS, &bc_param) != 0) {
        SGXPERF_printf("ERROR: BCIOREQ_BUFFERS failed\n");
        return 4;
    }
    if (ioctl(bcfd, BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
		SGXPERF_printf("Error ioctl BCIOGET_BUFFERCOUNT");
        return 5;
    }
    if (ioctl_var.output == 0) {
        SGXPERF_printf("no buffers available from driver \n");
        return 6;
    }

    /* Retrieve GL extension string */
    pszGLExtensions = glGetString(GL_EXTENSIONS);

    if (!pszGLExtensions || !strstr((char *)pszGLExtensions, "GL_IMG_texture_stream2"))
	{
		SGXPERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
		return 1;
	}

    myglTexBindStreamIMG = (PFNGLTEXBINDSTREAMIMGPROC)eglGetProcAddress("glTexBindStreamIMG");
    myglGetTexStreamDeviceAttributeivIMG = 
        (PFNGLGETTEXSTREAMDEVICEATTRIBUTEIVIMGPROC)eglGetProcAddress("glGetTexStreamDeviceAttributeivIMG");
    myglGetTexStreamDeviceNameIMG = 
        (PFNGLGETTEXSTREAMDEVICENAMEIMGPROC)eglGetProcAddress("glGetTexStreamDeviceNameIMG");

    if(!myglTexBindStreamIMG || !myglGetTexStreamDeviceAttributeivIMG || !myglGetTexStreamDeviceNameIMG)
	{
		SGXPERF_printf("TEST8: GL_IMG_texture_stream unsupported\n");
        return 2;
	}

    pszStreamDeviceName = myglGetTexStreamDeviceNameIMG(0);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG, &numBuffers);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG, &bufferWidth);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG, &bufferHeight);
    myglGetTexStreamDeviceAttributeivIMG(0, GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG, &bufferFormat);

    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    SGXPERF_printf("\nStream Device %s: numBuffers = %d, width = %d, height = %d, format = %x\n",
        pszStreamDeviceName, numBuffers, bufferWidth, bufferHeight, bufferFormat);

    for (idx = 0; idx < MAX_BUFFERS; idx++) {
        ioctl_var.input = idx;

        if (ioctl(bcfd, BCIOGET_BUFFERPHYADDR, &ioctl_var) != 0) {
            SGXPERF_printf("BCIOGET_BUFFERADDR: failed\n");
            return 7;
        }
        SGXPERF_printf("phyaddr[%d]: 0x%x\n", idx, ioctl_var.output);

        buf_paddr[idx] = (char*)ioctl_var.output;
        buf_vaddr[idx] = (char *)mmap(NULL, globals->inTextureWidth*globals->inTextureHeight*2,
                          PROT_READ | PROT_WRITE, MAP_SHARED,
                          bcfd, (long)buf_paddr[idx]);

        if (buf_vaddr[idx] == MAP_FAILED) {
            SGXPERF_printf("Error: failed mmap\n");
            return 8;
        }
		{
			for (int iii= 0; iii < globals->inTextureHeight; iii++)
				memset(((char *) buf_vaddr[idx] + (globals->inTextureWidth*iii*2)) , 0xa8, globals->inTextureWidth*2);
		}
    }

	return 0;
}
void test8_deinit_texture_streaming(struct globalStruct *globals)
{
	int idx;
    for (idx = 0; idx < MAX_BUFFERS; idx++) {
        if (buf_vaddr[idx] != MAP_FAILED)
            munmap(buf_vaddr[idx], globals->inTextureWidth*globals->inTextureHeight*2);
    }
    if (bcfd > -1)
        close(bcfd);
}

/* Texture streaming extension - nonEGLImage */
void test8(struct globalStruct *globals)
{
	timeval startTime, endTime, unitStartTime, unitEndTime;
	unsigned long diffTime2;
	int i;
	int bufferIndex = 0;
	int err;
	static GLfloat texcoord_img[] = 
        {0,0, 1,0, 0,1, 1,1};

#ifdef _ENABLE_TEST8_NO_USERPTR
	//initialise texture streaming
	err = test8_init_texture_streaming(globals);
#else //USERPTR
	//initialise texture streaming - USERPTR
	err = test8_init_texture_streaming_userptr(globals);
#endif
	if(err)
	{
		SGXPERF_ERR_printf("TEST8: Init failed with err = %d\n", err);
		goto deinit;
	}
	//initialise gl vertices
	float *pVertexArray, *pTexCoordArray;

	common_init_gl_vertices(globals->inNumberOfObjectsPerSide, &pVertexArray);
	common_init_gl_texcoords(globals->inNumberOfObjectsPerSide, &pTexCoordArray);

	//override the texturecoords for this extension only
	glDisableVertexAttribArray(TEXCOORD_ARRAY);
	glEnableVertexAttribArray(TEXCOORD_ARRAY);
	glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, (const void*)texcoord_img);
	//override the texture binding
	//glEnable(GL_TEXTURE_STREAM_IMG);

	gettimeofday(&startTime, NULL);
	SGXPERF_printf("TEST8: Entering DrawArrays loop\n");
	for(i = 0;(i < globals->numTestIterations)&&(!globals->quitSignal);i ++)
	{
	  SGXPERF_STARTPROFILEUNIT;	
		glClear(GL_COLOR_BUFFER_BIT);

		myglTexBindStreamIMG(0, bufferIndex);
		img_stream_gl_draw(globals->inNumberOfObjectsPerSide);
		common_eglswapbuffers (globals->eglDisplay, globals->eglSurface);
SGXPERF_ENDPROFILEUNIT		
	}
	err = glGetError();
	if(err) 
		SGXPERF_ERR_printf("Error in glTexBindStream loop err = %d", err);
	SGXPERF_printf("Exiting DrawArrays loop\n");
	gettimeofday(&endTime, NULL);
	diffTime2 = (tv_diff(&startTime, &endTime))/globals->numTestIterations;
	common_log(8, diffTime2);
deinit:
#ifdef _ENABLE_TEST8_NO_USERPTR
	test8_deinit_texture_streaming(globals);
#else
	test8_deinit_texture_streaming_userptr();
#endif
	common_deinit_gl_vertices(pVertexArray);
}
#endif


