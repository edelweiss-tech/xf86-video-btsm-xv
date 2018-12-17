/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.
Copyright (c) 2012 by Silicon Motion, Inc. (SMI)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of the XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from the XFree86 Project and silicon Motion.
*/

#ifndef  SMI_VIDEO_INC
#define  SMI_VIDEO_INC

#include <X11/extensions/Xv.h>
#include <fourcc.h>


static XF86VideoAdaptorPtr SMI_SetupVideo(ScreenPtr pScreen);
void SMI_ResetVideo(ScrnInfoPtr pScrn);

static void SMI_StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown);
static int SMI_SetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
        INT32 value, pointer data);
static int SMI_GetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
        INT32 *value, pointer data);
static void SMI_QueryBestSize(ScrnInfoPtr pScrn, Bool motion,
        short vid_w, short vid_h, short drw_w, short drw_h,
        unsigned int *p_w, unsigned int *p_h, pointer data);
static int SMI_PutImage(ScrnInfoPtr pScrn,
        short src_x, short src_y, short drw_x, short drw_y,
        short src_w, short src_h, short drw_w, short drw_h,
        int id, unsigned char *buf, short width, short height, Bool sync,
        RegionPtr clipBoxes, pointer data, DrawablePtr pDraw);
static int SMI_QueryImageAttributes(ScrnInfoPtr pScrn,
        int id, unsigned short *width, unsigned short *height,
        int *picthes, int *offsets);

#if (XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,16,0,0,0))
static void SMI_ClipNotify(ScrnInfoPtr pScrn, pointer data,
                                   WindowPtr window, int dx, int dy);
#endif

static Bool SMI_ClipVideo(ScrnInfoPtr pScrn, BoxPtr dst,
        INT32 *x1, INT32 *y1, INT32 *x2, INT32 *y2,
        RegionPtr reg, INT32 width, INT32 height);


static void SMI_DisplayVideo0768(ScrnInfoPtr pScrn, int id, int offset,
        short width, short height, int pitch, BoxPtr dstBox, short vid_w, short vid_h, 
        short drw_w, short drw_h, /*xf86CrtcPtr crtc, */int crtcIndex);

static void SMI_DisplayVideo0750(ScrnInfoPtr pScrn, int id, int offset,
        short width, short height, int pitch, BoxPtr dstBox, short vid_w, short vid_h, 
        short drw_w, short drw_h, /*xf86CrtcPtr crtc, */int crtcIndex);

FBAreaPtr
SMI_AllocateMemory(ScrnInfoPtr pScrn, FBAreaPtr area, int numLines);


static void SMI_InitOffscreenImages(ScreenPtr pScreen);

static int SMI_AllocSurface(ScrnInfoPtr pScrn,
        int id, unsigned short width, unsigned short height,
        XF86SurfacePtr surface);
static int SMI_FreeSurface(XF86SurfacePtr surface);
static int SMI_DisplaySurface(XF86SurfacePtr surface,
        short vid_x, short vid_y, short drw_x, short drw_y,
        short vid_w, short vid_h, short drw_w, short drw_h,
        RegionPtr clipBoxes);
static int SMI_StopSurface(XF86SurfacePtr surface);
static int SMI_GetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attr, INT32 *value);
static int SMI_SetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attr, INT32 value);

static int SetAttr(ScrnInfoPtr pScrn, int i, int value);
static void SetKeyReg(FBDevPtr pSmi, int reg, int value);



/* register defines so we're not hardcoding numbers */

#define FPR00					0x0000

/* video window formats - I=indexed, P=packed */
#define FPR00_FMT_8I				0x0
#define FPR00_FMT_15P				0x1
#define FPR00_FMT_16P				0x2
#define FPR00_FMT_32P				0x3



#define FPR00_FMT_24P				0x4
#define FPR00_FMT_8P				0x5
#define FPR00_FMT_YUV422			0x6
#define FPR00_FMT_YUV420			0x7

/* possible bit definitions for FPR00 - VWI = Video Window 1 */
#define FPR00_VWIENABLE				0x00000008
#define FPR00_VWITILE				0x00000010
#define FPR00_VWIFILTER2			0x00000020
#define FPR00_VWIFILTER4			0x00000040
#define FPR00_VWIKEYENABLE			0x00000080
#define FPR00_VWIGDF_SHIFT			16
#define FPR00_VWIGDENABLE			0x00080000
#define FPR00_VWIGDTILE				0x00100000

#define FPR00_MASKBITS				0x0000FFFF

#define FPR04						0x0004
#define FPR08						0x0008
#define FPR0C						0x000C
#define FPR10						0x0010
#define FPR14						0x0014
#define FPR18						0x0018
#define FPR1C						0x001C
#define FPR20						0x0020
#define FPR24						0x0024
#define FPR58						0x0058
#define FPR5C						0x005C
#define FPR68						0x0068
#define FPRB0						0x00B0
#define FPRB4						0x00B4
#define FPRC4						0x00C4
#define FPRCC						0x00CC

#define FPR158                      0x0158
#define FPR158_MASK_MAXBITS         0x07FF
#define FPR158_MASK_BOUNDARY        0x0800
#define FPR15C                      0x015C
#define FPR15C_MASK_HWCCOLORS       0x0000FFFF
#define FPR15C_MASK_HWCADDREN       0xFFFF0000
#define FPR15C_MASK_HWCENABLE       0x80000000


#define SMI_VIDEO_VIDEO		0
#define SMI_VIDEO_IMAGE		1

#define FOURCC_RV15			0x35315652
#define FOURCC_RV16			0x36315652
#define FOURCC_RV24			0x34325652
#define FOURCC_RV32			0x32335652

#define OFF_DELAY			200		/* milliseconds */
#define FREE_DELAY			60000	/* milliseconds */

#define OFF_TIMER			0x01
#define FREE_TIMER			0x02
#define CLIENT_VIDEO_ON		0x04
#define TIMER_MASK			(OFF_TIMER | FREE_TIMER)

#define SAA7110				0x9C
#define SAA7111             0x48

/*
 * Attributes
 */

#define N_ATTRS                 8

#define XV_ENCODING             0
#define XV_BRIGHTNESS           1
#define XV_CAPTURE_BRIGHTNESS	2
#define XV_CONTRAST             3
#define XV_SATURATION           4
#define XV_HUE                  5
#define XV_COLORKEY             6
#define XV_INTERLACED           7

#define USE_DMA 2


typedef struct
{
	FBAreaPtr	area;
	void*       BufferHandle;    // pointer to the buffer used by XV
	RegionRec	clip;
    /* Attributes */
    CARD32      Attribute[N_ATTRS];
	CARD32		videoStatus;
	Time		offTime;
	Time		freeTime;

    /* Memory */
    int		size;
    void	*video_memory;
    int		video_offset;

    /* Encodings */
    XF86VideoEncodingPtr        enc;
    int                         *input;
    int                         *norm;
    int                         *channel;
    int                         nenc,cenc;
} SMI_PortRec, *SMI_PortPtr;

typedef struct
{
	FBAreaPtr	area;
	Bool		isOn;

} SMI_OffscreenRec, *SMI_OffscreenPtr;


Bool SMI_Videoinit(ScreenPtr pScrn);
void memoryUnmap(int entIndex,SMIHWPtr pHw);


#endif   /* ----- #ifndef SMI_VIDEO_INC  ----- */

