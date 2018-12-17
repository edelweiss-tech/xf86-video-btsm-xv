/*
 * Baikal DMA specific stuff.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* BAIKAL: Headers needed for signal handling */
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "shadow.h"
#include "fb.h"
#include "fbdevhw.h"
#include "fbdev_priv.h"

/* BAIKAL: Headers needed for Baikal PCIe DMA support */
#include "sm750_ioctl.h"
#include "smi_dbg.h"

/* BAIKAL: Set the kernel-reserved contiguous physical memory address
 * for shadow framebuffer */

#define RESERVED_MEM_ADDR_SHORT 0x07000000
#define RESERVED_MEM_ADDR 0x07000000
/* Just for convenience */
#define MB (1024*1024)


/* BAIKAL: Forward declaration of shadow framebuffer update routine */
void
shadowUpdatePackedDMA(ScreenPtr pScreen, shadowBufPtr pBuf);

/* BAIKAL: Some forward declarations */
static void timer_handler (int sig);
int fb_fd;
Bool smooth = FALSE;

/* BAIKAL: Here DMA-specific staff begins */

/* Shadow framebuffer update in progress flag */
static int update_flag = 1;
/* DMA from signal handler needed flag */
static int signal_flag = 0;
/* Start and end of DMA region to be copied to device framebuffer */
static unsigned char *start=(unsigned char*)0xFFFFFFFF, *end=NULL;
/* Base address of shadow framebuffer area */
static FbBits *shaBase;

/* Check the current DMA status */
static int checkDMA ()
{
    int status;
    if (ioctl(fb_fd, FBIO_DW_GET_STAT_DMA_TRANSFER, &status))
	return -1;
    return status;
}

/* Initialize DMA transfer */
static void startDMA ()
{
  fb_dma_req_t req;
  int status;
  /* Should never happen, just a sanity check */
  if (start == (unsigned char*)0xFFFFFFFF || end == NULL) return;
  /* If we want smooth rendering, use a copy of shadow framebuffer area */
  if (smooth) req.from = start + 8 * MB;
  else req.from = start;
  /* Setup offset in a device framebuffer */
  req.fb_off = start - (unsigned char*)shaBase;
  req.size = end - start;
#if 0
  fprintf(stderr, "DMA: from=%p, end=%p, fb_off=%llu, size=%u\n",
	  start, end, req.fb_off, end - start);
#endif
  /* Yet another paranoic sanity check to avoid overwriting low memory */
  if (req.fb_off + req.size > 16 * MB) {
    fprintf(stderr, "DMA out of range\n"); exit(-1);
  }
  status = ioctl(fb_fd, FBIO_DW_DMA_WRITE, &req);
  if (status <= 0)
    xf86Msg(X_ERROR, "startDMA: ioctl error %d\n", status);
  /* Reset margins of DMA region */
  start = (unsigned char*)0xFFFFFFFF; end = NULL; 
}

/* Timer interrupt signal handler */

static void
timer_handler(int sig)
{
  /* Shadow framebuffer is not valid or being updated, don't use DMA
   * from signal handler */
  if (update_flag) return;
  /* If DMA needed */
  if (signal_flag) {
    /* Try to start DMA */
    if (checkDMA() != DW_PCI_DMA_RUNNING) {
      startDMA();
      /* On success reset the flag */
      signal_flag = 0;
    }
  }
}

/* This is never called and may be replaced with a dummy function */
static void *
smi_WindowLinear(ScreenPtr pScreen, CARD32 row, CARD32 offset, int mode,
		CARD32 *size, void *closure)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    FBDevPtr fPtr = FBDEVPTR(pScrn);

    if (fPtr->lineLength)
        *size = fPtr->lineLength;
    else
        *size = fPtr->lineLength = fbdevHWGetLineLength(pScrn);

    return ((CARD8 *)fPtr->fbstart + row * fPtr->lineLength + offset);
}

/* This function was obtained from original shadowUpdatePacked() routine */
void
shadowUpdatePackedDMA(ScreenPtr pScreen, shadowBufPtr pBuf)
{
    RegionPtr damage = shadowDamage(pBuf);
    PixmapPtr pShadow = pBuf->pPixmap;
    int nbox = RegionNumRects(damage);
    BoxPtr pbox = RegionRects(damage);
    FbBits *shaLine;
    FbStride shaStride;
    int byteStride;
    int shaBpp;
    _X_UNUSED int shaXoff, shaYoff;
    int x, y, w, h;

    unsigned char *src, *dst;

    ENTER();

    /* BAIKAL: Shadow framebuffer update in progress, don't use DMA from
     * signal handler */    
    update_flag = 1;
    fbGetDrawable(&pShadow->drawable, shaBase, shaStride, shaBpp, shaXoff,
                  shaYoff);
    byteStride = shaStride / sizeof(FbBits);

    while (nbox--) {
        x = pbox->x1 * shaBpp;
        y = pbox->y1;
        w = (pbox->x2 - pbox->x1) * shaBpp;
        h = pbox->y2 - pbox->y1;

        shaLine = shaBase + y * shaStride + (x >> FB_SHIFT);

        x &= FB_MASK;
        w = (w + x + FB_MASK) >> FB_SHIFT;

	src = (unsigned char *)shaLine;

        if (src < start)
            start = src;
        if ((shaLine + w + h * shaStride) > (FbBits*)end)
            end = (unsigned char*)(shaLine + w + h * shaStride);

        /* If we want smooth rendering, make a copy of data now */
	if (smooth) {
	    dst = src + 8 * MB;
	    while (h--) {
		memcpy(dst, src, w * sizeof(FbBits));
		src += byteStride;
		dst += byteStride;
	    }
	}

        pbox++;
    }

    /* BAIKAL: Try to start DMA */
    if (checkDMA() != DW_PCI_DMA_RUNNING) {
        startDMA();
	/* Reset the flag */
        signal_flag = 0;
    } else {
	/* If we still have previous DMA running, let the signal handler do
	 * DMA in the future */
        signal_flag = 1;
    }
    /* BAIKAL: Shadow framebuffer update completed */
    update_flag = 0;
    LEAVE();
}

static Bool
smi_dma_CloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    unsigned screensize = pScrn->virtualX * pScrn->virtualY *
	                  ((pScrn->bitsPerPixel + 7) / 8);

    if (fPtr->shadow) {
	munmap(fPtr->shadow, screensize);
	fPtr->shadow = NULL;
    }
    return TRUE;
}

static Bool
smi_dma_CreateScreenResources(ScreenPtr pScreen)
{
    PixmapPtr pPixmap;
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    Bool ret;

    ENTER();

    pScreen->CreateScreenResources = fPtr->CreateScreenResources;
    ret = pScreen->CreateScreenResources(pScreen);
    pScreen->CreateScreenResources = smi_dma_CreateScreenResources;

    if (!ret)
	LEAVE(FALSE);

    pPixmap = pScreen->GetScreenPixmap(pScreen);

    /* BAIKAL: Shadow framebuffer update routine changed to DMA-based */
    if (!shadowAdd(pScreen, pPixmap, shadowUpdatePackedDMA,
                   smi_WindowLinear, 0, NULL)) {
        LEAVE(FALSE);
    }

    LEAVE(TRUE);
}

Bool
SMI_DMAShadowInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    FBDevPtr fPtr = FBDEVPTR(pScrn);

    ENTER();

    if (!(fPtr->flags & FL_USE_DMA))
	LEAVE(FALSE);

    if (!shadowSetup(pScreen)) {
	LEAVE(FALSE);
    }

    fPtr->CreateScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = smi_dma_CreateScreenResources;

    LEAVE(TRUE);
}

void SMI_DMAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    struct sigaction sa;
    struct itimerspec tv;
    struct sigevent sev;
    timer_t timerid;


    ENTER();

    if (fPtr->rotate != 0) {
	xf86Msg(X_INFO, "DMA not supported for rotated screen\n");
	fPtr->flags &= ~FL_USE_DMA;
	return;
    }
    fb_fd = fbdevHWGetFD(pScrn);
    if (checkDMA() < 0) {
	xf86Msg(X_WARNING, "DMA not available on fd %d, disabling\n", fb_fd);
	fPtr->flags &= ~FL_USE_DMA;
	return;
    }
    /* BAIKAL: Use /dev/mem to allocate a device framebuffer in order to
     * benefit from 'Uncached Accelerated' memory attribute
     */
    int mem_fd = open("/dev/mem", O_RDWR);
    off_t addr = RESERVED_MEM_ADDR;
    unsigned screensize = pScrn->virtualX * pScrn->virtualY *
	                  ((pScrn->bitsPerPixel + 7) / 8);
    char *shadow_mem = NULL;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "mmaping %i Bytes (w: %i, h: %i, Bpp: %i)\n",
               screensize, pScrn->virtualY, pScrn->virtualX, pScrn->bitsPerPixel);
    shadow_mem = mmap(0, screensize, PROT_READ | PROT_WRITE,
                            MAP_SHARED, mem_fd, addr);
    if (shadow_mem == MAP_FAILED) {
        xf86Msg(X_ERROR, "Unable to map DMA ShadowFB\n");
        return;
    }

    /* BAIKAL: Setup signal handler for timer interrupts */
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGRTMIN, &sa, NULL);
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    timer_create(CLOCK_REALTIME, &sev, &timerid);
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_nsec = 10000*1000;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_nsec = 10000*1000;
    timer_settime(timerid, TIMER_ABSTIME, &tv, NULL);

    /* Hooks */
    fPtr->dma_CloseScreen = smi_dma_CloseScreen;
    fPtr->shadow = shadow_mem;
    fPtr->flags |= FL_USE_DMA | FL_USE_SHADOW;
    if (fPtr->flags & FL_USE_DBL_BUF)
	smooth = TRUE;

    LEAVE();
}

