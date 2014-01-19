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
	const char* pszFragEGLImageShader = "\
			#extension GL_OES_EGL_image_external : enable \n \
			uniform samplerExternalOES yuvTexSampler;\
			varying mediump vec2 TexCoord; \
			void main(void) \
			{	\
				gl_FragColor = texture2D(yuvTexSampler, TexCoord); \
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

	const char* helpString = \
"TI SGX OpenGLES2.0 Benchmarking Program For Linux. \n\
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
		9 - OpenVG SVG/PVG performance test (==deprecated==) \n\
		10 - PVR2D copyblit benchmark test \n\
		11 - Lenna Edge Detection benchmark test \n\
		12 - Edge detection (test8) \n\
		13 - Line drawing \n\
		14 - Context switch \n\
		15 - YUV-RGB converter - PVR2D \n\
		16 - YUV-streaming EGLImage \n\
		17 - FBO (ARGB format only) \n\
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
Ex. to test TEST3 with 256x256 32bit texture on LCD with 1 object at 30 fps 100 times, enter \n\
'./sgxperf2 3 256 256 0 2 0 1 0 100 30 1234'\n";
