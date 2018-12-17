/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000,2008 Silicon Motion, Inc.  All Rights Reserved.
Copyright (C) 2001 Corvin Zahn.  All Rights Reserved.
Copyright (C) 2008 Mandriva Linux.  All Rights Reserved.
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
	
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include<sys/ioctl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<unistd.h>
#include<ctype.h>
#include<fcntl.h>
#include<sys/time.h>
#include<sys/select.h>

#ifdef __GLIBC__
#include<asm/types.h>
#endif
#include<sys/time.h>

#include "xf86.h"
#include "xf86_OSproc.h"
//#include "xf86Crtc.h"
#include "os.h"

#include "fbdevhw.h"

#include	"smi_common.h"
#include    "fbdev_priv.h"
//#include    "xf86Crtc.h"
#include	"smi_video.h"
#include 	"smi_dbg.h"

#ifndef xalloc
#define xalloc xnfalloc
#define xcalloc xnfcalloc
#define xfree free
#endif

/*

   new attribute:

   XV_INTERLACED = 0: only one field of an interlaced video signal is displayed:
   -> half vertical resolution, but no comb like artifacts from
   moving vertical edges
   XV_INTERLACED = 1: both fields of an interlaced video signal are displayed:
   -> full vertical resolution, but comb like artifacts from
   moving vertical edges

   The default value can be set with the driver option Interlaced

 */


#define DWORD   unsigned int

#undef MIN
#undef ABS
#undef CLAMP
#undef ENTRIES

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define CLAMP(v, min, max) (((v) < (min)) ? (min) : MIN(v, max))

#define ENTRIES(array) (sizeof(array) / sizeof((array)[0]))
#define nElems(x)		(sizeof(x) / sizeof(x[0]))

#define MAKE_ATOM(a)	MakeAtom(a, sizeof(a) - 1, TRUE)

#define IS_768(pSmi)	(pSmi->pHardware->devId == SMI_SM768)
#define IS_750(pSmi)	(pSmi->pHardware->devId == SMI_SM750)

#include "dixstruct.h"

unsigned int	total_video_memory_k = 0;


/**
 * Atoms
 */

static Atom xvColorKey;
static Atom xvEncoding;
static Atom xvBrightness,xvCapBrightness, xvContrast, xvSaturation, xvHue;
static Atom xvInterlaced;


/******************************************************************************\
 **																			  **
 **                           C A P A B I L I T I E S                          **
 **																			  **
 \******************************************************************************/


static XF86VideoFormatRec SMI_VideoFormats[] =
{
    { 16, TrueColor },					/* depth, class				*/
    { 24, TrueColor },					/* depth, class				*/
};

#define IMAGE_MAX_WIDTH   2048
#define IMAGE_MAX_HEIGHT  2048

static XF86VideoEncodingRec DummyEncoding[1] = {
    {
         0,
         "XV_IMAGE",
         IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
         {1, 1}
      }
};


/**************************************************************************/

/**
 * Attributes
 */

#define XV_ENCODING_NAME        "XV_ENCODING"
#define XV_BRIGHTNESS_NAME      "XV_BRIGHTNESS"
#define XV_CAPTURE_BRIGHTNESS_NAME      "XV_CAPTURE_BRIGHTNESS"
#define XV_CONTRAST_NAME        "XV_CONTRAST"
#define XV_SATURATION_NAME      "XV_SATURATION"
#define XV_HUE_NAME             "XV_HUE"
#define XV_COLORKEY_NAME        "XV_COLORKEY"
#define XV_INTERLACED_NAME      "XV_INTERLACED"



static XF86AttributeRec SMI_VideoAttributes[] = {
    {XvSettable | XvGettable,        0,           255, XV_BRIGHTNESS_NAME},
    {XvSettable | XvGettable, 0x000000,      0xFFFFFF, XV_COLORKEY_NAME},
};

/**************************************************************************/
static XF86ImageRec SMI_VideoImages[] =
{
    XVIMAGE_YUY2,
    XVIMAGE_YV12,
    XVIMAGE_I420,
    {
        FOURCC_RV15,					/* id						*/
        XvRGB,							/* type						*/
        LSBFirst,						/* byte_order				*/
        { 'R', 'V' ,'1', '5',
            0x00, '5',  0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00 },		/* guid						*/
        16,								/* bits_per_pixel			*/
        XvPacked,						/* format					*/
        1,								/* num_planes				*/
        15,								/* depth					*/
        0x001F, 0x03E0, 0x7C00,			/* red_mask, green, blue	*/
        0, 0, 0,						/* y_sample_bits, u, v		*/
        0, 0, 0,						/* horz_y_period, u, v		*/
        0, 0, 0,						/* vert_y_period, u, v		*/
        { 'R', 'V', 'B' },				/* component_order			*/
        XvTopToBottom					/* scaline_order			*/
    },
    {
        FOURCC_RV16,					/* id						*/
        XvRGB,							/* type						*/
        LSBFirst,						/* byte_order				*/
        { 'R', 'V' ,'1', '6',
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00 },		/* guid						*/
        16,								/* bits_per_pixel			*/
        XvPacked,						/* format					*/
        1,								/* num_planes				*/
        16,								/* depth					*/
        0x001F, 0x07E0, 0xF800,			/* red_mask, green, blue	*/
        0, 0, 0,						/* y_sample_bits, u, v		*/
        0, 0, 0,						/* horz_y_period, u, v		*/
        0, 0, 0,						/* vert_y_period, u, v		*/
        { 'R', 'V', 'B' },				/* component_order			*/
        XvTopToBottom					/* scaline_order			*/
    },
    {
        FOURCC_RV24,					/* id						*/
        XvRGB,							/* type						*/
        LSBFirst,						/* byte_order				*/
        { 'R', 'V' ,'2', '4',
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00 },		/* guid						*/
        24,								/* bits_per_pixel			*/
        XvPacked,						/* format					*/
        1,								/* num_planes				*/
        24,								/* depth					*/
        0x0000FF, 0x00FF00, 0xFF0000,	/* red_mask, green, blue	*/
        0, 0, 0,						/* y_sample_bits, u, v		*/
        0, 0, 0,						/* horz_y_period, u, v		*/
        0, 0, 0,						/* vert_y_period, u, v		*/
        { 'R', 'V', 'B' },				/* component_order			*/
        XvTopToBottom					/* scaline_order			*/
    },
    {
        FOURCC_RV32,					/* id						*/
        XvRGB,							/* type						*/
        LSBFirst,						/* byte_order				*/
        { 'R', 'V' ,'3', '2',
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00 },		/* guid						*/
        32,								/* bits_per_pixel			*/
        XvPacked,						/* format					*/
        1,								/* num_planes				*/
        24,								/* depth					*/
        0x0000FF, 0x00FF00, 0xFF0000,	/* red_mask, green, blue	*/
        0, 0, 0,						/* y_sample_bits, u, v		*/
        0, 0, 0,						/* horz_y_period, u, v		*/
        0, 0, 0,						/* vert_y_period, u, v		*/
        { 'R', 'V', 'B' },				/* component_order			*/
        XvTopToBottom					/* scaline_order			*/
    },
};


void InitpSmi(FBDevPtr pSmi, ScrnInfoPtr pScrn)
{
	pSmi->Bpp =  pScrn->bitsPerPixel / 8;
	pSmi->Stride = ( pScrn->displayWidth * pSmi->Bpp + 15)&~15;
	
	switch (pScrn->bitsPerPixel)
	{
		case 16:
			pSmi->Stride >>= 1;
			break;
	
		case 24:
		case 32:
			pSmi->Stride >>= 2;
			break;
	}
	xf86Msg(X_INFO, "init smi, stride = [%d]\n", pSmi->Stride);
	pSmi->screen_info = pScrn;

	pSmi->videoKey = 0x003;
//		(1 << pScrn->offset.red)|(1 << pScrn->offset.green)|(((pScrn->mask.blue >> pScrn->offset.blue) - 1)<< pScrn->offset.blue);
	pSmi->interlaced = FALSE;
}

void InitpHw(ScrnInfoPtr pScrn, FBDevPtr pSmi, int entityIndex)
{

#ifdef XSERVER_LIBPCIACCESS
	struct pci_device * pPci;
#else
	pciVideoPtr pPci;
#endif

	ENTER();
#if 0
		/*what's the difference between 'pScrn->entityList[0]' and 'pEntInfo->index'??*/
	pPci = xf86GetPciInfoForEntity(entityIndex);

	if(!pPci)
		pPci = xf86GetPciInfoForEntity(pSmi->pEnt->index);
#else
	pPci = pScrn->vgaDev;
#endif

	if(!pSmi->pHardware)
	{
		/*
		 * Allocate an 'Chip'Rec, and hook it into pScrn->driverPrivate.
		 * pScrn->driverPrivate is initialised to NULL, so we can check if
		 * the allocation has already been done.
		 */

		SMIHWPtr pHw;
		pHw = pSmi->pHardware = (SMIHWPtr)xnfcalloc(1, sizeof(SMIHWRec));
			
		pSmi->pHardware->pPci = pPci;
		pHw->fonts = xalloc(KB(64));
		if(pHw->fonts == NULL)
			LEAVE();

		pSmi->pHardware->dual = 1;
		pSmi->pHardware->primary_screen_rec = pSmi;
		pSmi->screen = 0;

		/* Put what you already known into structure */
#ifdef XSERVER_LIBPCIACCESS
		pSmi->pHardware->phyaddr_reg = pPci->regions[1].base_addr;
		pSmi->pHardware->physize_reg = 0x200000;
		pSmi->pHardware->phyaddr_mem = pPci->regions[0].base_addr;
		pSmi->pHardware->devId = pPci->device_id;
#else
		pSmi->pHardware->phyaddr_reg = pPci->memBase[1];
		pSmi->pHardware->physize_reg = 0x200000;
		pSmi->pHardware->phyaddr_mem = pPci->memBase[0];
		pSmi->pHardware->devId = pPci->device;
#endif

#ifdef XSERVER_LIBPCIACCESS
		pSmi->pHardware->revId = pPci->revision;
		pSmi->pHardware->pPci = pPci;
#endif
		pSmi->videoRAMBytes = pScrn->videoRam;
		pSmi->pHardware->physize_mem = pScrn->videoRam;
		xf86Msg(X_INFO, "SMI devId: %04x\n", pSmi->pHardware->devId);
	}	else
	{
		/*
		 * pSmi->pHardware is not NULL, which means current entity already
		 * mallocated a SMIHWPtr structure,so we are in dualview 
		 * mode!
		 * */
		pSmi->pHardware->dual += 1;/*The total number of screen*/
		pSmi->screen = (pSmi->pHardware->dual)-1;/*The index of screen*/
		pSmi->videoRAMBytes >>= 1;
	}


}

/******************************************************************************\
 **                                                                            **
 **                  X V E X T E N S I O N   I N T E R F A C E                 **
 **                                                                            **
 \******************************************************************************/


void sm768_pcDeepmap(SMIHWPtr pHw)
{
	ENTER();	

    pHw->DPRBase = pHw->pReg + 0x100000;
    pHw->VPRBase = pHw->pReg + 0x000000;
    pHw->DCRBase = pHw->pReg + 0x080000; //Video overlay
    pHw->SCRBase = pHw->pReg + 0x000000; //System
	LEAVE();
}
 
void sm750_pcDeepmap(SMIHWPtr pHw)
{
	ENTER();	

    pHw->DPRBase = pHw->pReg + 0x100000;
    pHw->VPRBase = pHw->pReg + 0x000000;
    pHw->DCRBase = pHw->pReg + 0x080000; //Video overlay
    pHw->SCRBase = pHw->pReg + 0x000000; //System
	LEAVE();
}
 
static void SMI_CSC_Start(FBDevPtr pSmi, CARD32 CSC_control)
{
    CARD32 CSC_old;
    SMIHWPtr pHw = pSmi->pHardware;
    ENTER();
    while (1)
    {
	CSC_old = READ_DPR(pHw, 0xFC);
	if (!(CSC_old & (1 << 31)))
	{
	    CSC_control |= 1 << 31;
	    WRITE_DPR(pHw, 0xFC, CSC_control);
	    break;
	}
    }
    LEAVE();
}

 /*
  * This function maps a physical address into logical address.
  * Return: NULL address pointer if fail
  * 		A Logical address pointer if success.
  */
 void *mapPhysicalAddress(
	 void *phyAddr, 		   /* 32 bit physical address */
	 unsigned long size 	   /* Memory size to be mapped */
 )
 {
	 unsigned long address;
	 int fileDescriptor;
		 
	 fileDescriptor = open("/dev/mem", O_RDWR);
	 if (fileDescriptor == -1)
		 return ((void *)0);
		 
	 address = (unsigned long) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, (unsigned long)phyAddr);
 
	 if ((void *) address == MAP_FAILED)
		 return ((void *)0);
	 
	 return ((void *) address);
 }

int memoryMapping(FBDevPtr pSmi,int entIndex)
{
	int error;
	ENTER();
	SMIHWPtr pHw = pSmi->pHardware;

	/* map regular register */
#ifdef XSERVER_LIBPCIACCESS
	xf86Msg(X_INFO,"phyaddr_reg = 0x%llx, physize_reg = 0x%llx\n", pHw->phyaddr_reg, pHw->physize_reg);
	/*Map the specified memory range so that it can be accessed by the CPU.*/
	void **result = (void**)&pHw->pReg;
	error = pci_device_map_range(
			pHw->pPci,
			pHw->phyaddr_reg,
			pHw->physize_reg,
			PCI_DEV_MAP_FLAG_WRITABLE,
			result);
	if(error){
		ERROR("Map mmio failed\n");
		LEAVE(FALSE);
	}
	else
	{
		xf86Msg(X_NOTICE,"Map mmio address= %p\n",pHw->pReg);
	}
#else
	xf86Msg(X_INFO,"phyaddr_reg = 0x%llx, physize_reg = 0x%llx\n", pHw->phyaddr_reg, pHw->physize_reg);
	pHw->pReg = mapPhysicalAddress((void*)pHw->phyaddr_reg, pHw->physize_reg);
	if(!pHw->pReg){
		ERROR("Map mmio failed\n");
		LEAVE(FALSE);
	}
	else
	{
		xf86Msg(X_NOTICE,"Map mmio address = %p\n",pHw->pReg);
	}
#endif
	/* map vgahw stuffs or chip-private stuffs */
	if (IS_768(pSmi))
	    sm768_pcDeepmap(pHw);
	else
	    sm750_pcDeepmap(pHw);

	LEAVE(TRUE);
}

void sm768_closeAllScreen(SMIHWPtr pHw)
{
	ENTER();

	if(pHw->fonts)
	{
		xf86Msg(X_INFO,"Close Screen Free saved fonts\n");
		xfree(pHw->fonts);
		pHw->fonts = NULL;
	}
	xfree(pHw);
	LEAVE();
}

void memoryUnmap(int entIndex,SMIHWPtr pHw)
{

	ENTER();

	if(pHw->pReg)
	{
#ifndef XSERVER_LIBPCIACCESS
		xf86UnMapVidMem (entIndex,(pointer) pHw->phyaddr_reg, pHw->physize_reg);
#else
		pci_device_unmap_range(pHw->pPci, pHw->pReg, pHw->physize_reg);
#endif
		pHw->pReg = NULL;
	}

	sm768_closeAllScreen(pHw);
	
	LEAVE();
}

void
SMI_Videoclose(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    int entIndex = pSmi->pEnt->index;

    memoryUnmap(entIndex, pSmi->pHardware);
    pSmi->pHardware = NULL;
}

Bool
SMI_Videoinit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    XF86VideoAdaptorPtr *ptrAdaptors = NULL, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int numAdaptors; 
	
    ENTER();

	int entIndex = pSmi->pEnt->index;

	InitpSmi(pSmi,pScrn);
	SMIHWPtr pHw;
	pSmi->pHardware = pHw = NULL;

	InitpHw(pScrn, pSmi, entIndex);
	if (pScrn->vgaDev->vendor_id != PCI_SMI_VENDOR_ID)
	{
		xf86Msg(X_WARNING, "Video not supported on this hardware\n");
		LEAVE(FALSE);
	}

	memoryMapping(pSmi, entIndex);
	//pScrn->videoRam = ((pSmi->pHardware->physize_mem) / (pSmi->pHardware->dual))>>10;

//    int refByes = total_video_memory_k >> 1;	
//    if(pSmi->pHardware->dual > 1)
// 		refByes >>= 1;
	
	pSmi->pFB = pSmi->fbstart;
	xf86Msg(X_INFO,"pSmi->pFB = %p\n", pSmi->pFB);

	numAdaptors = xf86XVListGenericAdaptors(pScrn, &ptrAdaptors);
	newAdaptor = SMI_SetupVideo(pScreen);
	DEBUG("newAdaptor=%p, numAdaptors[%d]\n", newAdaptor, numAdaptors);
	SMI_InitOffscreenImages(pScreen);

    if(newAdaptor != NULL)
    {
        if(numAdaptors == 0)
        {
            numAdaptors = 1;
            ptrAdaptors = &newAdaptor;
        }
        else
        {
            newAdaptors = xalloc((numAdaptors + 1) *
                    sizeof(XF86VideoAdaptorPtr*));
            memcpy(newAdaptors, ptrAdaptors,
                        numAdaptors * sizeof(XF86VideoAdaptorPtr));
	        newAdaptors[numAdaptors++] = newAdaptor;
	        ptrAdaptors = newAdaptors;
        }
    }

    if (numAdaptors != 0)
    {
        xf86XVScreenInit(pScreen, ptrAdaptors, numAdaptors);
    }

    if (newAdaptors != NULL)
    {
        xfree(newAdaptors);
    }
    LEAVE(TRUE);
}


/*************************************************************************/

/**
 * sets video decoder attributes channel, encoding, brightness, contrast, saturation, hue
 */
    static int
SetAttr(ScrnInfoPtr pScrn, int i, int value)
{
    FBDevPtr pSmi = FBDEVPTR(pScrn);

	ENTER();
    if (i < XV_ENCODING || i > XV_HUE)
        LEAVE (BadMatch);

    /* clamps value to attribute range */
    value = CLAMP(value, SMI_VideoAttributes[i].min_value,
            SMI_VideoAttributes[i].max_value);

    if (i == XV_BRIGHTNESS) {
        int my_value = (value <= 128? value + 128 : value - 128);
        SetKeyReg(pSmi, 0x5C, 0xEDEDED | (my_value << 24));
    }

    LEAVE (Success);
}




/******************************************************************************\
 **																			  **
 **						 V I D E O   M A N A G E M E N T					  **
 **																			  **
 \******************************************************************************/

static XF86VideoAdaptorPtr
SMI_SetupVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    SMI_PortPtr smiPortPtr;
    XF86VideoAdaptorPtr ptrAdaptor;

	ENTER();
    ptrAdaptor = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
            sizeof(DevUnion) + sizeof(SMI_PortRec));
    if (ptrAdaptor == NULL)
    {
        LEAVE(NULL);
    }

    ptrAdaptor->type = XvInputMask
        | XvImageMask
        | XvWindowMask
        ;

	// In randr 1.2 mode VIDEO_CLIP_TO_VIEWPORT is broken.
    ptrAdaptor->flags = VIDEO_OVERLAID_IMAGES;

    ptrAdaptor->name = "Silicon Motion XV Engine";
    ptrAdaptor->nPorts = 1;
    ptrAdaptor->pPortPrivates = (DevUnion*) &ptrAdaptor[1];
    ptrAdaptor->pPortPrivates[0].ptr = (pointer) &ptrAdaptor->pPortPrivates[1];
	/* lame trick,i know */
    smiPortPtr = (SMI_PortPtr) ptrAdaptor->pPortPrivates[0].ptr;
    smiPortPtr->enc = DummyEncoding;
    smiPortPtr->nenc = 1;
    ptrAdaptor->nEncodings = smiPortPtr->nenc;
    ptrAdaptor->pEncodings = smiPortPtr->enc;

    ptrAdaptor->nFormats = nElems(SMI_VideoFormats);
    ptrAdaptor->pFormats = SMI_VideoFormats;

    ptrAdaptor->nAttributes = nElems(SMI_VideoAttributes);
	ptrAdaptor->pAttributes = SMI_VideoAttributes;

    ptrAdaptor->nImages = nElems(SMI_VideoImages);
    ptrAdaptor->pImages = SMI_VideoImages;

    ptrAdaptor->PutVideo = NULL;
    ptrAdaptor->PutStill = NULL;
    ptrAdaptor->GetVideo = NULL;
    ptrAdaptor->GetStill = NULL;

    ptrAdaptor->StopVideo = SMI_StopVideo;
    ptrAdaptor->SetPortAttribute = SMI_SetPortAttribute;
    ptrAdaptor->GetPortAttribute = SMI_GetPortAttribute;
    ptrAdaptor->QueryBestSize = SMI_QueryBestSize;
    ptrAdaptor->PutImage = SMI_PutImage;
    ptrAdaptor->QueryImageAttributes = SMI_QueryImageAttributes;

    smiPortPtr->Attribute[XV_COLORKEY] = pSmi->videoKey;
	DEBUG("monk:in %s:smiPortPtr->Attribute[XV_COLORKEY] = 0x%08x\n",__func__,
			smiPortPtr->Attribute[XV_COLORKEY]);
    smiPortPtr->Attribute[XV_INTERLACED] = pSmi->interlaced;
    smiPortPtr->videoStatus = 0;

#if defined(REGION_NULL)
    REGION_NULL(pScreen, &smiPortPtr->clip);
#else
    REGION_INIT(pScreen, &smiPortPtr->clip, NullBox, 0);
#endif
#if (XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,16,0,0,0))
    ptrAdaptor->ClipNotify = SMI_ClipNotify;
#endif
    pSmi->ptrAdaptor      = ptrAdaptor;


    xvColorKey   = MAKE_ATOM(XV_COLORKEY_NAME);
    xvBrightness = MAKE_ATOM(XV_BRIGHTNESS_NAME);
    xvCapBrightness = MAKE_ATOM(XV_CAPTURE_BRIGHTNESS_NAME);


    SMI_ResetVideo(pScrn);
	
    LEAVE(ptrAdaptor);
}

void SMI_ResetVideo(ScrnInfoPtr	pScrn )
{
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;
    int r, g, b;
    SMIHWPtr pHw = pSmi->pHardware;

    ENTER();

    if (IS_768(pSmi))
        WRITE_SCR(pHw, 0x60,0x0);
    else
        WRITE_SCR(pHw, 0x6c,0x0);

	
    SetAttr(pScrn, XV_ENCODING, 0);     /* Encoding = pal-composite-0 */
    SetAttr(pScrn, XV_BRIGHTNESS, 128); /* Brightness = 128 (CCIR level) */
    SetAttr(pScrn, XV_CAPTURE_BRIGHTNESS, 128); /* Brightness = 128 (CCIR level) */
    SetAttr(pScrn, XV_CONTRAST, 71);    /* Contrast = 71 (CCIR level) */
    SetAttr(pScrn, XV_SATURATION, 64);  /* Color saturation = 64 (CCIR level) */
    SetAttr(pScrn, XV_HUE, 0);          /* Hue = 0 */

    switch (pScrn->depth)
    {
        case 8:
            SetKeyReg(pSmi, FPR04, pPort->Attribute[XV_COLORKEY] & 0x00FF);
            SetKeyReg(pSmi, FPR08, 0);
            break;

        case 15:
        case 16:
            SetKeyReg(pSmi, FPR04, pPort->Attribute[XV_COLORKEY] & 0xFFFF);
            SetKeyReg(pSmi, FPR08, 0);
            break;

        default:
            r = (pPort->Attribute[XV_COLORKEY] & pScrn->mask.red) >> pScrn->offset.red;
            g = (pPort->Attribute[XV_COLORKEY] & pScrn->mask.green) >> pScrn->offset.green;
            b = (pPort->Attribute[XV_COLORKEY] & pScrn->mask.blue) >> pScrn->offset.blue;
            SetKeyReg(pSmi, FPR04, ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            break;
    }

    SetKeyReg(pSmi, FPR5C, 0xEDEDED | (pPort->Attribute[XV_BRIGHTNESS] << 24));
	LEAVE();
}

static void
SMI_StopVideo(
        ScrnInfoPtr	pScrn,
        pointer		data,
        Bool		shutdown
        )
{
	ENTER();
    SMI_PortPtr pPort = (SMI_PortPtr) data;
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    SMIHWPtr pHw = pSmi->pHardware;

    REGION_EMPTY(pScrn->pScreen, &pPort->clip);

    if (shutdown)
    {
        if (pPort->videoStatus & CLIENT_VIDEO_ON)
        {
      
            WRITE_DCR(pHw, channel0_DCR40, READ_DCR(pHw, channel0_DCR40) & ~0x00000004);
	    if (IS_768(pSmi))
			WRITE_DCR(pHw, channel1_DCR40, READ_DCR(pHw, channel1_DCR40) & ~0x00000004);
        }
        if (pPort->area != NULL)
        {
            xf86FreeOffscreenArea(pPort->area);
            pPort->area = NULL;
        }
        if (pPort->BufferHandle!= NULL)
        {
            xf86FreeOffscreenArea((FBAreaPtr) pPort->BufferHandle);      
            pPort->BufferHandle = NULL;
        }
        pPort->videoStatus = 0;
	pSmi->videoOn = FALSE;
    }
    else
    {
        if (pPort->videoStatus & CLIENT_VIDEO_ON)
        {
            pPort->videoStatus |= OFF_TIMER;
            pPort->offTime = currentTime.milliseconds + OFF_DELAY;
        }
    }
	LEAVE();
}


static int
SMI_SetPortAttribute(
        ScrnInfoPtr	pScrn,
        Atom		attribute,
        INT32		value,
        pointer		data
        )
{
    int res;
    SMI_PortPtr pPort = (SMI_PortPtr) data;
    FBDevPtr pSmi = FBDEVPTR(pScrn);
	ENTER();
    if (attribute == xvColorKey) 
	{
        int r, g, b;
        pPort->Attribute[XV_COLORKEY] = value;

        switch (pScrn->depth)
        {
            case 8:
                SetKeyReg(pSmi, FPR04, value & 0x00FF);
                break;

            case 15:
            case 16:
                SetKeyReg(pSmi, FPR04, value & 0xFFFF);
                break;
			case 24:
                r = (value & pScrn->mask.red) >> pScrn->offset.red;
                g = (value & pScrn->mask.green) >> pScrn->offset.green;
                b = (value & pScrn->mask.blue) >> pScrn->offset.blue;
                SetKeyReg(pSmi, FPR04,((r >> 3) << 11)|((g >> 2) << 5)|(b >> 3));
                break;
        }
        res = Success;
    }
    else if (attribute == xvInterlaced)
	{
        pPort->Attribute[XV_INTERLACED] = (value != 0);
        res = Success;
    }
    else if (attribute == xvEncoding)
	{
        res = SetAttr(pScrn, XV_ENCODING, value);
    }
    else if (attribute == xvBrightness) 
	{
        res = SetAttr(pScrn, XV_BRIGHTNESS, value);
    }
    else if (attribute == xvCapBrightness)
	{
        res = SetAttr(pScrn, XV_CAPTURE_BRIGHTNESS, value);
    }
    else if (attribute == xvContrast)
	{
        res = SetAttr(pScrn, XV_CONTRAST, value);
    }
    else if (attribute == xvSaturation) 
	{
        res = SetAttr(pScrn, XV_SATURATION, value);
    }
    else if (attribute == xvHue)
	{
        res = SetAttr(pScrn, XV_HUE, value);
    }
    else 
	{
        res = BadMatch;
    }

    LEAVE(res);
}


static int
SMI_GetPortAttribute(
        ScrnInfoPtr	pScrn,
        Atom		attribute,
        INT32		*value,
        pointer		data
        )
{
	ENTER();
    SMI_PortPtr pPort = (SMI_PortPtr) data;

    if (attribute == xvEncoding)
        *value = pPort->Attribute[XV_ENCODING];
    else if (attribute == xvBrightness)
        *value = pPort->Attribute[XV_BRIGHTNESS];
    else if (attribute == xvCapBrightness)
        *value = pPort->Attribute[XV_CAPTURE_BRIGHTNESS];
    else if (attribute == xvContrast)
        *value = pPort->Attribute[XV_CONTRAST];
    else if (attribute == xvSaturation)
        *value = pPort->Attribute[XV_SATURATION];
    else if (attribute == xvHue)
        *value = pPort->Attribute[XV_HUE];
    else if (attribute == xvColorKey)
	{
        *value = pPort->Attribute[XV_COLORKEY];
		DEBUG("get color key:0x%08x\n",*value);
	}
    else
    {
        LEAVE(BadMatch);
    }
    LEAVE(Success);
}


static void
SMI_QueryBestSize(
        ScrnInfoPtr		pScrn,
        Bool			motion,
        short			vid_w,
        short			vid_h,
        short			drw_w,
        short			drw_h,
        unsigned int	*p_w,
        unsigned int	*p_h,
        pointer			data
        )
{
	ENTER();
    *p_w = drw_w;
    *p_h = drw_h;
	LEAVE();
}

#if (XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,16,0,0,0))
static void SMI_ClipNotify(ScrnInfoPtr pScrn, pointer data,
                                   WindowPtr window, int dx, int dy)
{
    ENTER();           
    LEAVE();
}
#endif


/* return 0 means no enough video memory for allocation */
FBAreaPtr
SMI_AllocateMemory(ScrnInfoPtr pScrn, FBAreaPtr	area, int numLines)
{
    ScreenPtr	pScreen = screenInfo.screens[pScrn->scrnIndex];

    ENTER();

    if (area != NULL)
    {
        if ((area->box.y2 - area->box.y1) >= numLines)
        {
            LEAVE(area);
        }

        if (xf86ResizeOffscreenArea(area, pScrn->displayWidth, numLines))
        {
            LEAVE(area);
        }

        xf86FreeOffscreenArea(area);
    }

    area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth, numLines, 0,
            NULL, NULL, NULL);

    if (area == NULL)
    {
        int maxW, maxH;

        xf86QueryLargestOffscreenArea(pScreen, &maxW, &maxH, 0,
                FAVOR_WIDTH_THEN_AREA, PRIORITY_EXTREME);

		if ((maxW < pScrn->displayWidth) || (maxH < numLines))
		{
			LEAVE(NULL);
		}
        xf86PurgeUnlockedOffscreenAreas(pScreen);
        area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth, numLines,
                0, NULL, NULL, NULL);
    }

    DEBUG("area = %p\n", area);
    LEAVE(area);

}

static int
SMI_PutImage(
	ScrnInfoPtr		pScrn,
	short			src_x,
	short			src_y,
	short			drw_x,
	short			drw_y,
	short			src_w,
	short			src_h,
	short			drw_w,
	short			drw_h,
	int			id,
	unsigned char		*buf,
	short			width,
	short			height,
	Bool			sync,
	RegionPtr		clipBoxes,
	pointer			data,
	DrawablePtr		pDraw
)
{
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    SMI_PortPtr pPort = (SMI_PortPtr) data;
    INT32 x1, y1, x2, y2;
    int bpp = 0;
    int fbPitch, srcPitch, srcPitch2 = 0, dstPitch, size, areaHeight;
    BoxRec dstBox;
//    , VPBox;
//    RegionRec VPReg, tempReg;
    CARD32 offset, offset2 = 0, offset3 = 0, tmp;
    int left, top, nPixels, nLines;
    unsigned char *dstStart;

    ENTER();

    //REGION_INIT(pScreen, &tempReg, NULL, 1);

        x1 = src_x;
        y1 = src_y;
        x2 = src_x + src_w;
        y2 = src_y + src_h;

        dstBox.x1 = drw_x;
        dstBox.y1 = drw_y;
        dstBox.x2 = drw_x + drw_w;
        dstBox.y2 = drw_y + drw_h;

		// manual clipping against the CRTC dimensions
        //REGION_COPY(pScreen, &tempReg, clipBoxes);

		if (clipBoxes->extents.x1 < 0)
		{
			clipBoxes->extents.x1 = 0;
		}
		
		if (clipBoxes->extents.x2 > pScrn->virtualX)
		{
			clipBoxes->extents.x2 = pScrn->virtualX;
		}
		
		if (clipBoxes->extents.y1 <0)
		{
			clipBoxes->extents.y1 = 0;
		}
		
		if (clipBoxes->extents.y2 > pScrn->virtualY)
		{
			clipBoxes->extents.y2 = pScrn->virtualY;
		}

        //REGION_INIT(pScreen, &VPReg, &VPBox, 1);
        //REGION_INTERSECT(pScreen, &tempReg, &tempReg, &VPReg);
        //REGION_UNINIT(pScreen, &VPReg);
    		
		
		/* maybe we should use xf86ClipVideoHelper to replace below function */
		if (!SMI_ClipVideo(pScrn, &dstBox, &x1, &y1, &x2, &y2, clipBoxes, width, height))
		{
			LEAVE(Success);
		}
		
		//if (!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2, &tempReg, width, height))
		    //continue;
		
		fbPitch = pSmi->Stride * pSmi->Bpp;


        switch (id) 
		{
            case FOURCC_YV12:
            case FOURCC_I420:
                srcPitch  = (width + 3) & ~3;
                offset2   = srcPitch * height;
                srcPitch2 = ((width >> 1) + 3) & ~3;
                offset3   = offset2 + (srcPitch2 * (height >> 1));

					/* make sure dstPitch is aligned with 16 */
				dstPitch = ((width << 1)+15)&~15;
				size = dstPitch * (height + (height >>1));
				break;
            case FOURCC_RV24:
                bpp = 3;
                srcPitch = width * bpp;
                dstPitch = (srcPitch + 15) & ~15;
                size = dstPitch * (height + (height >> 1));
                break;
            case FOURCC_RV32:
                bpp = 4;
                srcPitch = width * bpp;
                dstPitch = (srcPitch + 15) & ~15;
                size = dstPitch * (height + (height >> 1));
                break;
            case FOURCC_YUY2:
            case FOURCC_RV15:
            case FOURCC_RV16:
            default:
                bpp = 2;
                srcPitch = width * bpp;
                dstPitch = (srcPitch + 15) & ~15;
                size = dstPitch * (height + (height >> 1));
                break;
        }

        //size = dstPitch * height * bpp;

        DEBUG("Size is %d\n",size);        
		
		switch(id)
		{
			case FOURCC_YV12:
			case FOURCC_I420:
				areaHeight = ((dstPitch * height) + fbPitch - 1) / fbPitch;//4	//yuv 4:1:1
				break;
			default:
				areaHeight = ((dstPitch * height) + fbPitch - 1) / fbPitch;
				break;
		}

        pPort->area = SMI_AllocateMemory(pScrn, pPort->area, areaHeight);
        if (pPort->area == NULL)
        {
            LEAVE(BadAlloc);
        }
        //pPort->video_offset = SMI_AllocateMemory(pScrn, &(pPort->BufferHandle), size);//MB(64);//
		//if (pPort->BufferHandle == NULL)
        //{
        //    REGION_UNINIT(pScreen, &tempReg);
        //    LEAVE(BadAlloc);
        //}   
        //left *= bpp;
		
		top = y1 >> 16;
		left = (x1 >> 16) & ~1;
		nPixels = ((((x2 + 0xFFFF) >> 16) + 1) & ~1) - left;

		offset = (pPort->area->box.y1 * fbPitch);
		dstStart = pSmi->pFB + offset;
        //offset = pPort->video_offset + usedVidMem + (top * dstPitch);
        //dstStart = pSmi->pFB + offset + left;
        DEBUG("left = %d offset = %08X\n", left, offset);

        switch(id) 
        {
            case FOURCC_YV12:
            case FOURCC_I420:
                top &= ~1;
				/* calculate the offset in U/V space of video*/
                //tmp = ((top >> 1) * srcPitch2) + (left >> 2);
                //offset2 += tmp;
                //offset3 += tmp;
                if (id == FOURCC_I420) 
				{
                    tmp = offset2;
                    offset2 = offset3;
                    offset3 = tmp;
                }

				//xf86Msg(X_INFO, "offset2=[0x%x], offset3=[0x%x], srcPitch[%d], height[%d]\n", offset2, offset3, srcPitch, height);
				nLines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
				xf86XVCopyYUV12ToPacked(buf,
						   buf + offset2, buf + offset3, dstStart,
						   srcPitch, srcPitch2, dstPitch, height, width);
				id = FOURCC_YUY2; //vvv???
				break;
            case FOURCC_UYVY:
            case FOURCC_YUY2:
            default:
                buf += (top * srcPitch) + left;
                nLines = ((y2 + 0xffff) >> 16) - top;
                xf86XVCopyPacked(buf, dstStart, srcPitch, dstPitch, nLines, nPixels);
                break;
        }


         //REGION_COPY(pScrn->pScreen, &pPort->clip, &tempReg);
	 if (IS_768(pSmi)) {
		 xf86XVFillKeyHelper(pScrn->pScreen, pPort->Attribute[XV_COLORKEY], clipBoxes); 

		 SMI_DisplayVideo0768(pScrn,id, offset, width, height,dstPitch,
			    	&dstBox, src_w, src_h, drw_w, drw_h, /*crtc,*/ 0);
	}
	else
	{
#if 0 // only for overlay
		 xf86XVFillKeyHelper(pScrn->pScreen, pPort->Attribute[XV_COLORKEY], clipBoxes); //vvv: not for CSC
#endif
		 SMI_DisplayVideo0750(pScrn,id, offset, width, height,dstPitch,
			    	&dstBox, src_w, src_h, drw_w, drw_h, /*crtc,*/ 0);
	}
      
        pPort->videoStatus = CLIENT_VIDEO_ON;
	pSmi->videoOn = TRUE;
    //REGION_UNINIT(pScreen, &tempReg);
    LEAVE(Success);
}

static int
SMI_QueryImageAttributes(
        ScrnInfoPtr		pScrn,
        int				id,
        unsigned short	*width,
        unsigned short	*height,
        int				*pitches,
        int				*offsets
        )
{
//    FBDevPtr pSmi = FBDEVPTR(pScrn);
    int size, tmp;
	ENTER();


    *width = (*width + 1) & ~1;
    if (offsets != NULL)
    {
        offsets[0] = 0;
    }

    switch (id)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
            *height = (*height + 1) & ~1;
            size = (*width + 3) & ~3;
            if (pitches != NULL)
            {
                pitches[0] = size;
            }
            size *= *height;
            if (offsets != NULL)
            {
                offsets[1] = size;
            }
            tmp = ((*width >> 1) + 3) & ~3;
            if (pitches != NULL)
            {
                pitches[1] = pitches[2] = tmp;
            }
            tmp *= (*height >> 1);
            size += tmp;
            if (offsets != NULL)
            {
                offsets[2] = size;
            }
            size += tmp;
            break;

        case FOURCC_YUY2:
        case FOURCC_RV15:
        case FOURCC_RV16:
        default:
            size = *width * 2;
            if (pitches != NULL)
            {
                pitches[0] = size;
            }
            size *= *height;
            break;

        case FOURCC_RV24:
            size = *width * 3;
            if (pitches != NULL)
            {
                pitches[0] = size;
            }
            size *= *height;
            break;

        case FOURCC_RV32:
            size = *width * 4;
            if (pitches != NULL)
            {
                pitches[0] = size;
            }
            size *= *height;
            break;
    }

    LEAVE(size);
}


/******************************************************************************\
 **																			  **
 **						S U P P O R T   F U N C T I O N S					  **
 **																			  **
 \******************************************************************************/


static Bool
SMI_ClipVideo(
        ScrnInfoPtr	pScrn,
        BoxPtr		dst,
        INT32		*x1,
        INT32		*y1,
        INT32		*x2,
        INT32		*y2,
        RegionPtr	reg,
        INT32		width,
        INT32		height
        )
{
	ENTER();
//    ScreenPtr pScreen = pScrn->pScreen;
    INT32 vscale, hscale;
    BoxPtr extents = REGION_EXTENTS(pScreen, reg);
    int diff;
//    FBDevPtr pSmi = FBDEVPTR(pScrn);



    hscale = ((*x2 - *x1) << 16) / (dst->x2 - dst->x1);
    vscale = ((*y2 - *y1) << 16) / (dst->y2 - dst->y1);
	
    *x1 <<= 16; *y1 <<= 16;
    *x2 <<= 16; *y2 <<= 16;

    diff = extents->x1 - dst->x1;
    if (diff > 0)
    {
        dst->x1 = extents->x1;
        *x1 += diff * hscale;
    }

    diff = extents->y1 - dst->y1;
    if (diff > 0)
    {
        dst->y1 = extents->y1;
        *y1 += diff * vscale;
    }

    diff = dst->x2 - extents->x2;
    if (diff > 0)
    {
        dst->x2 = extents->x2; /* PDR#687 */
        *x2 -= diff * hscale;
    }

    diff = dst->y2 - extents->y2;
    if (diff > 0)
    {
        dst->y2 = extents->y2;
        *y2 -= diff * vscale;
    } 

    if (*x1 < 0)
    {
        diff = (-*x1 + hscale - 1) / hscale;
        dst->x1 += diff;
        *x1 += diff * hscale;
    }

    if (*y1 < 0)
    {
        diff = (-*y1 + vscale - 1) / vscale;
        dst->y1 += diff;
        *y1 += diff * vscale;
    }



    if ((*x1 >= *x2) || (*y1 >= *y2))
    {
        LEAVE(FALSE);
    }

    if (   (dst->x1 != extents->x1) || (dst->y1 != extents->y1)
            || (dst->x2 != extents->x2) || (dst->y2 != extents->y2)
       )
    {
        RegionRec clipReg;
        REGION_INIT(pScreen, &clipReg, dst, 1);
        REGION_INTERSECT(pScreen, reg, reg, &clipReg);
        REGION_UNINIT(pScreen, &clipReg);
    }


    LEAVE(TRUE);
}


static void
SMI_DisplayVideo0750(
        ScrnInfoPtr	pScrn,
        int			id,
        int			offset,
        short		width,
        short		height,
        int			pitch,
        BoxPtr		dstBox,
        short		vid_w,
        short		vid_h,
        short		drw_w,
        short		drw_h,
	int		crtc_index
)
{
	ENTER();
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    SMIHWPtr pHw = pSmi->pHardware;
    DWORD HScale, VScale;
    CARD32 crtc_fb;
    DWORD Ybase, Ubase, Vbase;
    int32_t csc;
    int src_fmt = 0;
    int bpp = pScrn->bitsPerPixel / 8;
    int fbPitch = pSmi->Stride * pSmi->Bpp;

    HScale = ((vid_w - 1) << 13) / (drw_w -1);
    if (HScale & 0xffff0000)
	HScale = 0xffff;
    VScale = ((vid_h - 1) << 13) / (drw_h -1);
    if (VScale & 0xffff0000)
	VScale = 0xffff;

    if (HScale < (1 << 13) || VScale < (1 << 13))
        csc = 3 << 24;
    else
        csc = 0;
    if (bpp > 2)
	csc |= 1 << 26;

    crtc_fb = READ_DCR(pHw, 0xc) & 0x3ffffff;
    Ybase = crtc_fb + offset;
    Ubase = Ybase;
    Vbase = Ybase;

    switch(id) {
    case FOURCC_YV12:
	src_fmt = 2;
	Vbase += pitch * vid_h;
	Ubase += Vbase + pitch * vid_h / 4;
	break;
    case FOURCC_I420:
	src_fmt = 2;
	Ubase += pitch * vid_h;
	Vbase += Ubase + pitch * vid_h / 4;
	break;
    case FOURCC_YUY2:
	src_fmt = 0;
	break;
    case FOURCC_RV16:
	src_fmt = 6;
	break;
    case FOURCC_RV32:
	src_fmt = 7;
    }
    csc |= src_fmt << 28;
    csc |= 1 << 31;

    WRITE_DPR(pHw, 0xcc, 0);
    WRITE_DPR(pHw, 0xd0, 0);
    WRITE_DPR(pHw, 0xd4, 0);
    WRITE_DPR(pHw, 0xe0, (vid_w << 16) | vid_h);
    WRITE_DPR(pHw, 0xe4, ((pitch >> 4) << 16) | (pitch >> 5));
    WRITE_DPR(pHw, 0xe8, (dstBox->x1 << 16) | dstBox->y1);
    WRITE_DPR(pHw, 0xec, (drw_w << 16) | drw_h);
    WRITE_DPR(pHw, 0xf0, ((fbPitch >> 4) << 16) | drw_h);
    WRITE_DPR(pHw, 0xf4, (HScale << 16) | VScale);
    WRITE_DPR(pHw, 0xc8, Ybase);
    WRITE_DPR(pHw, 0xd8, Ubase);
    WRITE_DPR(pHw, 0xdc, Vbase);
    WRITE_DPR(pHw, 0xf8, crtc_fb);

    SMI_CSC_Start(pSmi, csc);
    LEAVE();
}


static void
SMI_DisplayVideo0768(
        ScrnInfoPtr	pScrn,
        int			id,
        int			offset,
        short		width,
        short		height,
        int			pitch,
        BoxPtr		dstBox,
        short		vid_w,
        short		vid_h,
        short		drw_w,
        short		drw_h,
		//xf86CrtcPtr crtc,
		int crtcIndex)
{
	ENTER();
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    SMIHWPtr pHw = pSmi->pHardware;
    CARD32 dcr40;
    int hstretch, vstretch;
    int scale_factor = 0;
//    uint32_t tmp1, tmp2, lines;
	int HwOffset = crtcIndex?0x8000:0;
	int SrcYPitch, SrcUVPitch;
	int hwvideo = 0;
	

	if(READ_SCR(pHw, 0x60) == 0xaabb)
		hwvideo = 1;
	else
		hwvideo = 0;
	
   	if(hwvideo == 1){
		SrcUVPitch = (width/2 + 15) & ~15;
    	SrcYPitch = SrcUVPitch * 2;
   	}

    dcr40 = READ_DCR(pHw, channel0_DCR40 + HwOffset) & ~0x00003FFF;//video control

    dcr40 |= (1<<2);
	dcr40 |= (3<<8);
	
    switch (id)
    {
        case FOURCC_YV12:
        case FOURCC_I420:      
			if(hwvideo)
            	dcr40 |= 0x3;
			else
				dcr40 |= 0x2;
            break;
		case FOURCC_UYVY:
			dcr40 |= 0x2;
			dcr40 |= (1<<12);  //byte swap bit
			break;
        case FOURCC_YUY2:
			dcr40 |= 0x2;
			break;
        case FOURCC_RV16:
            dcr40 |= 0x1;
            break;
        case FOURCC_RV32:
            dcr40 |= 0x2;
            break;
    }
   //if (crtc->rotation & (RR_Rotate_0 | RR_Rotate_180)) 
    {
//        lines = height;
        if (drw_h == vid_h)
		{
            scale_factor = 0; 
            vstretch = 1 << 12;
            scale_factor |= (vstretch<<16);
        } 
		else if (drw_h > vid_h) 
		{
            //dcr40 |=(1<<8)|(1<<9); 
            scale_factor = 0; 
            vstretch = vid_h * (1 << 12) / drw_h;
            vstretch -= (drw_h + vid_h - 1) / vid_h;
            scale_factor|= (vstretch<<16);
        } 
		else
		{
            scale_factor |= (1<<31);
            vstretch = drw_h * (1 << 12) / vid_h;
            vstretch = vstretch < (1 << 10) ? (1 << 10) : vstretch;
            scale_factor |= (vstretch<<16);
        }

        if (drw_w == vid_w)
		{
            scale_factor |=0;
            hstretch = 1 << 12;
            scale_factor |=hstretch;
        } 
		else if (drw_w > vid_w) 
		{
            //dcr40 |=(1<<8)|(1<<9); 
            scale_factor |=0; 
            hstretch = vid_w * (1 << 12) / drw_w;
            hstretch -= (drw_w + vid_w - 1) / vid_w;
            scale_factor|=hstretch;
        } 
		else 
		{
            scale_factor |=(1<<15);
            hstretch = drw_w * (1 << 12) / vid_w;
            hstretch = hstretch < (1 << 10) ? (1 << 10) : hstretch;
            scale_factor |=hstretch;
        }
    }
	
    /* Set Color Key Enable bit */
     WRITE_DCR(pHw, channel0_DCR00 + HwOffset, READ_DCR(pHw, channel0_DCR00 + HwOffset) | (1 << 9));
    

    WRITE_DCR(pHw, channel0_DCR60 + HwOffset, (dstBox->x1) | ((dstBox->y1)<< 16));//DCR50
    WRITE_DCR(pHw, channel0_DCR64 + HwOffset, ( (dstBox->y2 - 1)<<16) |(dstBox->x2 - 1)); //DCR54


	if(hwvideo){
	    WRITE_DCR(pHw, channel0_DCR48 + HwOffset, (SrcYPitch<<16) |(SrcYPitch));
		WRITE_DCR(pHw, channel0_DCR50 + HwOffset, (SrcUVPitch<<16) |(SrcUVPitch));
		WRITE_DCR(pHw, channel0_DCR58 + HwOffset, (SrcUVPitch<<16) |(SrcUVPitch));
	}else{
	    WRITE_DCR(pHw, channel0_DCR44 + HwOffset,((1<<31)|offset));//post change
	    WRITE_DCR(pHw, channel0_DCR48 + HwOffset, (pitch<<16) |(pitch));
	
	}
    //WRITE_DCR(pHw, channel0_DCR4C, (offset + pitch*(lines - 1)));
    WRITE_DCR(pHw, channel0_DCR68 + HwOffset, scale_factor); //DCR58    //ilena: care of this . so that the screen can play a movie oversize.

    WRITE_DCR(pHw, channel0_DCR6C + HwOffset, 0x00000000);//DCR5C
    WRITE_DCR(pHw, channel0_DCR5C + HwOffset, 0x00EDEDED);//DCR60

    WRITE_DCR(pHw, channel0_DCR40 + HwOffset, dcr40);//video display control
    LEAVE();

}



/******************************************************************************\
 **																			  **
 **				 O F F S C R E E N   M E M O R Y   M A N A G E R			  **
 **																			  **
 \******************************************************************************/

static void
SMI_InitOffscreenImages(
        ScreenPtr	pScreen
        )
{
	ENTER();
    XF86OffscreenImagePtr offscreenImages;
//    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
//    FBDevPtr pSmi = FBDEVPTR(pScrn);
//    SMI_PortPtr pPort = (SMI_PortPtr) pSmi->ptrAdaptor->pPortPrivates[0].ptr;

    offscreenImages = xalloc(sizeof(XF86OffscreenImageRec));
    if (offscreenImages == NULL)
    {
        LEAVE();
    }

    offscreenImages->image = SMI_VideoImages;
    offscreenImages->flags = VIDEO_OVERLAID_IMAGES
        | VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages->alloc_surface = SMI_AllocSurface;
    offscreenImages->free_surface = SMI_FreeSurface;
    offscreenImages->display = SMI_DisplaySurface;
    offscreenImages->stop = SMI_StopSurface;
    offscreenImages->getAttribute = SMI_GetSurfaceAttribute;
    offscreenImages->setAttribute = SMI_SetSurfaceAttribute;
    offscreenImages->max_width = 3840;
    offscreenImages->max_height = 2160;

    offscreenImages->num_attributes = nElems(SMI_VideoAttributes);
    offscreenImages->attributes = SMI_VideoAttributes; 


    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
	LEAVE();
}



static int
SMI_AllocSurface(
        ScrnInfoPtr		pScrn,
        int				id,
        unsigned short	width,
        unsigned short	height,
        XF86SurfacePtr	surface
        )
{
    FBDevPtr pSmi = FBDEVPTR(pScrn);
    int numLines, pitch, fbPitch, bpp;
    SMI_OffscreenPtr ptrOffscreen;
    FBAreaPtr area;

	ENTER();

    if (pSmi->Bpp == 3)
    {
        fbPitch = pSmi->Stride;
    }
    else
    {
        fbPitch = pSmi->Stride * pSmi->Bpp;
    }

    width = (width + 1) & ~1;
    switch (id)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
        case FOURCC_YUY2:
        case FOURCC_RV15:
        case FOURCC_RV16:
            bpp = 2;
            break;

        case FOURCC_RV24:
            bpp = 3;
            break;

        case FOURCC_RV32:
            bpp = 4;
            break;

        default:
            return(BadAlloc);
    }
    pitch = (width * bpp + 15) & ~15;

    numLines = ((height * pitch) + fbPitch - 1) / fbPitch;

    area = SMI_AllocateMemory(pScrn, NULL, numLines);
    if (area == NULL)
    {
        return(BadAlloc);
    }

    surface->pitches = xalloc(sizeof(int));
    if (surface->pitches == NULL)
    {
        xf86FreeOffscreenArea(area);
        return(BadAlloc);
    }
    surface->offsets = xalloc(sizeof(int));
    if (surface->offsets == NULL)
    {
        xfree(surface->pitches);
        xf86FreeOffscreenArea(area);
        return(BadAlloc);
    }

    ptrOffscreen = xalloc(sizeof(SMI_OffscreenRec));
    if (ptrOffscreen == NULL)
    {
        xfree(surface->offsets);
        xfree(surface->pitches);
        xf86FreeOffscreenArea(area);
        return(BadAlloc);
    }

    surface->pScrn = pScrn;
    surface->id = id;
    surface->width = width;
    surface->height = height;
    surface->pitches[0] = pitch;
    surface->offsets[0] = area->box.y1 * fbPitch;
    surface->devPrivate.ptr = (pointer) ptrOffscreen;

    ptrOffscreen->area = area;
    ptrOffscreen->isOn = FALSE;

    LEAVE(Success);
}

static int
SMI_FreeSurface(
        XF86SurfacePtr	surface
        )
{
    SMI_OffscreenPtr ptrOffscreen = (SMI_OffscreenPtr) surface->devPrivate.ptr;

	ENTER();

    if (ptrOffscreen->isOn)
    {
        SMI_StopSurface(surface);
    }

    xf86FreeOffscreenArea(ptrOffscreen->area);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    LEAVE(Success);
}

static int
SMI_DisplaySurface(
        XF86SurfacePtr	surface,
        short			vid_x,
        short			vid_y,
        short			drw_x,
        short			drw_y,
        short			vid_w,
        short			vid_h,
        short			drw_w,
        short			drw_h,
        RegionPtr		clipBoxes
        )
{
    SMI_OffscreenPtr ptrOffscreen = (SMI_OffscreenPtr) surface->devPrivate.ptr;
    FBDevPtr pSmi = FBDEVPTR(surface->pScrn);
    SMI_PortPtr pPort = pSmi->ptrAdaptor->pPortPrivates[0].ptr;
    INT32 x1, y1, x2, y2;
    BoxRec dstBox;
    //xf86CrtcPtr crtc;
	ENTER();
    x1 = vid_x;
    x2 = vid_x + vid_w;
    y1 = vid_y;
    y2 = vid_y + vid_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    if (!SMI_ClipVideo(surface->pScrn, &dstBox, &x1, &y1, &x2, &y2, clipBoxes,
                surface->width, surface->height))
    {
        LEAVE(Success);
    }

    dstBox.x1 -= surface->pScrn->frameX0;
    dstBox.y1 -= surface->pScrn->frameY0;
    dstBox.x2 -= surface->pScrn->frameX0;
    dstBox.y2 -= surface->pScrn->frameY0;

    xf86XVFillKeyHelper(surface->pScrn->pScreen,
            pPort->Attribute[XV_COLORKEY], clipBoxes);


    SMI_ResetVideo(surface->pScrn);
    if (IS_768(pSmi))
        SMI_DisplayVideo0768(surface->pScrn, surface->id, surface->offsets[0],
            surface->width, surface->height, surface->pitches[0], 
            &dstBox, vid_w, vid_h, drw_w, drw_h, /*crtc,*/ 0);//tmp index set. by ilena
    else
        SMI_DisplayVideo0750(surface->pScrn, surface->id, surface->offsets[0],
            surface->width, surface->height, surface->pitches[0], 
            &dstBox, vid_w, vid_h, drw_w, drw_h, /*crtc,*/ 0);//tmp index set. by ilena


    ptrOffscreen->isOn = TRUE;
    if (pPort->videoStatus & CLIENT_VIDEO_ON)
    {
        REGION_EMPTY(surface->pScrn->pScreen, &pPort->clip);
        UpdateCurrentTime();
        pPort->videoStatus = FREE_TIMER;
        pPort->freeTime = currentTime.milliseconds + FREE_DELAY;
    }

    LEAVE(Success);
}

static int
SMI_StopSurface(
        XF86SurfacePtr	surface
        )
{
    SMI_OffscreenPtr ptrOffscreen = (SMI_OffscreenPtr) surface->devPrivate.ptr;

	ENTER();

    if (ptrOffscreen->isOn)
    {
        FBDevPtr pSmi = FBDEVPTR(surface->pScrn);
        SMIHWPtr pHw = pSmi->pHardware;
        {
            WRITE_VPR(pHw, 0x00, READ_VPR(pHw, 0x00) & ~0x00000008);
        }

        ptrOffscreen->isOn = FALSE;
    }

    LEAVE(Success);
}

static int
SMI_GetSurfaceAttribute(
        ScrnInfoPtr	pScrn,
        Atom		attr,
        INT32		*value
        )
{
    FBDevPtr pSmi = FBDEVPTR(pScrn);
	ENTER();

    LEAVE(SMI_GetPortAttribute(pScrn, attr, value,
                (pointer) pSmi->ptrAdaptor->pPortPrivates[0].ptr));
}

static int
SMI_SetSurfaceAttribute(
        ScrnInfoPtr	pScrn,
        Atom		attr,
        INT32		value
        )
{
	ENTER();
    FBDevPtr pSmi = FBDEVPTR(pScrn);

    LEAVE(SMI_SetPortAttribute(pScrn, attr, value,(pointer) pSmi->ptrAdaptor->pPortPrivates[0].ptr));
}

    static void
SetKeyReg(FBDevPtr pSmi, int reg, int value)
{
	ENTER();
	SMIHWPtr pHw = pSmi->pHardware;

    /* We don't change the color mask, and we don't do brightness.  IF
     * they write to the colorkey register, we'll write the value to the
     * 501 colorkey register */
    if (FPR04 == reg )	/* Only act on colorkey value writes */
    {
	    if (IS_768(pSmi)) {
                WRITE_DCR(pHw, channel0_DCR28, value);	
		WRITE_DCR(pHw, channel1_DCR28, value);
	    }
            else
                WRITE_DCR(pHw, channel0_DCR08, value);

    }  
	

	LEAVE();
}



