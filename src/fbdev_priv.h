/*
 * Copyright (C) 1994-2003 The XFree86 Project, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is fur-
 * nished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
 * NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
 * NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall not
 * be used in advertising or otherwise to promote the sale, use or other deal-
 * ings in this Software without prior written authorization from the XFree86
 * Project.
 */

/*
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *	     Michel Dänzer, <michel@tungstengraphics.com>
 */

#include "xf86xv.h"
#include "compat-api.h"
#include "smi_common.h"

#ifdef DEBUG
#define DebugMsg(...) ErrorF(__VA_ARGS__)
#else
#define DebugMsg(...)
#endif

typedef struct {
	unsigned char*			fbstart;
	unsigned char*			fbmem;
	int				fboff;
	int				lineLength;
	int				rotate;
	Bool				shadowFB;
	void				*shadow;
	CloseScreenProcPtr		CloseScreen;
	CloseScreenProcPtr		dma_CloseScreen;
	CreateScreenResourcesProcPtr	CreateScreenResources;
	void				(*PointerMoved)(SCRN_ARG_TYPE arg, int x, int y);
	EntityInfoPtr			pEnt;
	/* DGA info */
	DGAModePtr			pDGAMode;
	int				nDGAMode;
	OptionInfoPtr			Options;

	void				*backing_store_tuner_private;

	int 				Bpp;		/* byte per pixel. */
	int				Stride;		/* Stride of frame buffer,in bytes */
	ScrnInfoPtr			screen_info;
	int screen;
	/*frame buffer special*/

	void * 				pFB;		/*start address of frame buffer of current screen*/
	int 				videoRAMBytes;	/* frame buffer size for one screen.full video memory for SIMUL case,and half for dualview*/
	uint32_t 			FBReserved;	/* An offset in framebuffer for reserved memory in frame buffer for XAA to use*/
	SMIHWPtr 			pHardware;	/* two SMIPtr can link to one share*/
	int				videoKey;	/* Video chroma key */
	Bool				interlaced;	/* True: Interlaced Video */
	Bool				videoOn;	/* don't use DMA while video is on */
	XF86VideoAdaptorPtr		ptrAdaptor;	/* Pointer to VideoAdapter structure */
	uint32_t			flags;
} FBDevRec, *FBDevPtr;

#define FL_USE_DMA	0x1
#define FL_USE_SHADOW	0x2
#define FL_USE_DBL_BUF	0x4

#define FBDEVPTR(p) ((FBDevPtr)((p)->driverPrivate))

#define BACKING_STORE_TUNER(p) ((BackingStoreTuner *) \
                       (FBDEVPTR(p)->backing_store_tuner_private))

extern void SMI_DMAInit(ScreenPtr pScreen);
extern Bool SMI_DMAShadowInit(ScreenPtr pScreen);
extern Bool SMI_Videoinit(ScreenPtr pScreen);
extern void SMI_Videoclose(ScreenPtr pScreen);
