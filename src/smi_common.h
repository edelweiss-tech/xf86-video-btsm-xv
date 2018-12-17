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
authorization from the XFree86 Project and Silicon Motion.
*/

#ifndef  SMI_COMMON_INC
#define  SMI_COMMON_INC

#ifdef XSERVER_LIBPCIACCESS
#include <pciaccess.h>
#endif
#include "xf86fbman.h"
#include "compiler.h"
#include "xorgVersion.h"

/* Chip tags */
#define PCI_SMI_VENDOR_ID	0x126f
#define SMI_UNKNOWN			0
#define SMI_SM768           0x0768
#define SMI_SM750           0x0750


#define KB(X) ((X)<<10)


#ifndef GLYPH_HAS_GLYPH_PICTURE_ACCESSOR
#define GetGlyphPicture(g, s) GlyphPicture((g))[(s)->myNum]
#define SetGlyphPicture(g, s, p) GlyphPicture((g))[(s)->myNum] = p
#endif

#ifndef XF86_HAS_SCRN_CONV
#define xf86ScreenToScrn(s) xf86Screens[(s)->myNum]
#define xf86ScrnToScreen(s) screenInfo.screens[(s)->scrnIndex]
#endif

/* 
 * driver defined PROC 
 */
typedef void (*SmiHwProc)(pointer pHw);
typedef void (*SmiProc)(ScrnInfoPtr pScrn);
typedef int (*SmiRetProc)(ScrnInfoPtr pScrn);
typedef ModeStatus (*SmiModeProc)(ScrnInfoPtr pScrn,DisplayModePtr mode);
typedef void (*SmiValueProc)(ScrnInfoPtr pScrn,int value);	
typedef void (*SmiPaletteProc)(ScrnInfoPtr pScrn, int numColors, int *indicies, LOCO * colors, VisualPtr pVisual);
typedef int (*SmiGetProc)(pointer pHw);
typedef void (*SmiVideoProc)(ScrnInfoPtr	pScrn);

typedef struct
{	
	/* hardware attributes */
#ifdef XSERVER_LIBPCIACCESS
	pciaddr_t 	phyaddr_reg;/* don't forget 712 get another mmio(for vgaIO),so we use reg to define 
							the physica register address */
	pciaddr_t 	physize_reg;
	pciaddr_t 	phyaddr_mem;
	pciaddr_t 	physize_mem;
	
	struct pci_device *pPci;
#else
	ADDRESS 	phyaddr_reg;
	uint32_t 	physize_reg;
	ADDRESS 	phyaddr_mem;
	uint32_t 	physize_mem;

	pciVideoPtr pPci;
#endif


	volatile CARD8 *	DPRBase;	/* Base of DPR registers */
	volatile CARD8 *	VPRBase;	/* Base of VPR registers */
	volatile CARD8 *	DCRBase;    /* Base of DCR registers - for 0501 chipset */
	volatile CARD8 *	SCRBase;    /* Base of SCR registers - for 0501 chipset */	

	
	void           *	pReg;/* mapped registers virtual address */
	int 			dual;
	uint16_t 		devId;/*This member save 'pPci->device'*/
	uint8_t 		revId;
	EntityInfoPtr	pEnt_info;

	void *  		primary_screen_rec;

	SmiHwProc 	pcDeepMap;/* can be zero pointer */	

	SmiGetProc 	pcFBSize;
	char *fonts;

}SMIHWRec,*SMIHWPtr;

#define IS_SM750(pHw)	(pHw->devId == SMI_SM750)
#define IS_SM768(pHw)	(pHw->devId == SMI_SM768)

#define WRITE_DPR(pHw, dpr, data)	MMIO_OUT32(pHw->DPRBase, dpr, data) 
#define READ_DPR(pHw, dpr)			MMIO_IN32(pHw->DPRBase, dpr)
#define WRITE_VPR(pHw, vpr, data)	MMIO_OUT32(pHw->VPRBase, vpr, data) 
#define READ_VPR(pHw, vpr)			MMIO_IN32(pHw->VPRBase, vpr)
#define WRITE_CPR(pHw, cpr, data)	MMIO_OUT32(pHw->CPRBase, cpr, data)
#define READ_CPR(pHw, cpr)			MMIO_IN32(pHw->CPRBase, cpr)
//#define WRITE_FPR(pHw, fpr, data)	MMIO_OUT32(pHw->FPRBase, fpr, data) 
//#define READ_FPR(pHw, fpr)			MMIO_IN32(pHw->FPRBase, fpr)
#define WRITE_DCR(pHw, dcr, data)	MMIO_OUT32(pHw->DCRBase, dcr, data)
#define READ_DCR(pHw, dcr)			MMIO_IN32(pHw->DCRBase, dcr)
#define WRITE_SCR(pHw, scr, data)	MMIO_OUT32(pHw->SCRBase, scr, data) 
#define READ_SCR(pHw, scr)			MMIO_IN32(pHw->SCRBase, scr)

/* Needs refinement */

#define SMI_TRANSPARENT_SRC		0x00000100
#define SMI_TRANSPARENT_DEST	0x00000300

#define SMI_OPAQUE_PXL			0x00000000
#define SMI_TRANSPARENT_PXL		0x00000400

#define SMI_MONO_PACK_8			0x00001000
#define SMI_MONO_PACK_16		0x00002000
#define SMI_MONO_PACK_32		0x00003000

#define SMI_ROP2_SRC			0x00008000
#define SMI_ROP2_PAT			0x0000C000
#define SMI_ROP3				0x00000000

#define SMI_BITBLT				0x00000000
#define SMI_RECT_FILL			0x00010000
#define SMI_TRAPEZOID_FILL		0x00030000
#define SMI_SHORT_STROKE    	0x00060000
#define SMI_BRESENHAM_LINE		0x00070000
#define SMI_HOSTBLT_WRITE		0x00080000
#define SMI_HOSTBLT_READ		0x00090000
#define SMI_ROTATE_BLT			0x000B0000

#define SMI_SRC_COLOR			0x00000000
#define SMI_SRC_MONOCHROME		0x00400000

#define SMI_GRAPHICS_STRETCH	0x00800000

#define SMI_ROTATE_ZERO			0x0
#define SMI_ROTATE_CW			0x01000000
#define SMI_ROTATE_CCW			0x02000000
#define SMI_ROTATE_UD			0x03000000

#define SMI_MAJOR_X				0x00000000
#define SMI_MAJOR_Y				0x04000000

#define SMI_LEFT_TO_RIGHT		0x00000000
#define SMI_RIGHT_TO_LEFT		0x08000000

#define SMI_COLOR_PATTERN		0x40000000
#define SMI_MONO_PATTERN		0x00000000

#define SMI_QUICK_START			0x10000000
#define SMI_START_ENGINE		0x80000000


#define RGB8_PSEUDO      (-1)
#define RGB16_565         0
#define RGB16_555         1
#define RGB32_888         2



/*SM768 channel0 Video Control*/
#define channel0_DCR00						0x0000
#define channel0_DCR28						0x0028
#define channel0_DCR40						0x0040
#define channel0_DCR44						0x0044
#define channel0_DCR48						0x0048
#define channel0_DCR4C						0x004C
#define channel0_DCR50						0x0050
#define channel0_DCR54						0x0054
#define channel0_DCR58						0x0058
#define channel0_DCR5C						0x005C
#define channel0_DCR60						0x0060
#define channel0_DCR64						0x0064
#define channel0_DCR68						0x0068
#define channel0_DCR6C						0x006C

/*SM768 channel1 Video Control*/
#define channel1_DCR00						0x8000
#define channel1_DCR28						0x8028
#define channel1_DCR40						0x8040
#define channel1_DCR44						0x8044
#define channel1_DCR48						0x8048
#define channel1_DCR4C						0x804C
#define channel1_DCR50						0x8050
#define channel1_DCR54						0x8054
#define channel1_DCR58						0x8058
#define channel1_DCR5C						0x805C
#define channel1_DCR60						0x8060
#define channel1_DCR64						0x8064
#define channel1_DCR68						0x8068
#define channel1_DCR6C						0x806C

/* SM750 */
#define channel0_DCR08						0x0008
#define DCR00						0x0000
#define DCR40						0x0040
#define DCR44						0x0044
#define DCR48						0x0048
#define DCR4C						0x004C
#define DCR50						0x0050
#define DCR54						0x0054
#define DCR58						0x0058
#define DCR5C						0x005C
#define DCR60						0x0060

#define SCR00						0x0000
#define SCR04						0x0004
#define SCR08						0x0008
#define SCR0C						0x000C
#define SCR10						0x0010
#define SCR10_LOCAL_MEM_SIZE        0x0000E000
#define SCR10_LOCAL_MEM_SIZE_SHIFT  13
#define SCR14						0x0014
#define SCR18						0x0018
#define SCR1C						0x001C
#define SCR20						0x0020
#define SCR24						0x0024
#define SCR28						0x0028
#define SCR2C						0x002C
#define SCR30						0x0030
#define SCR34						0x0034
#define SCR38						0x0038
#define SCR3C						0x003C
#define SCR40						0x0040
#define SCR44						0x0044
#define SCR48						0x0048
#define SCR4C						0x004C
#define SCR50						0x0050
#define SCR54						0x0054
#define SCR58						0x0058
#define SCR5C						0x005C
#define SCR60						0x0060
#define SCR64						0x0064
#define SCR68						0x0068
#define SCR6C						0x006C
#endif   /* ----- #ifndef SMI_COMMON_INC  ----- */
