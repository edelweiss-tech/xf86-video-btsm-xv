#ifndef __SM750_IOCTL__
#define __SM750_IOCTL__

#include <linux/fb.h>
#include <sys/ioctl.h>

typedef struct {
        void* from;
        loff_t fb_off;
        __u32 size;
} fb_dma_req_t;

#define FBIO_DW_GET_STAT_DMA_TRANSFER   _IOWR('F', 0xAA, __u32)
#define FBIO_DW_DMA_WRITE               _IOWR('F', 0xAB, fb_dma_req_t)

#define DW_PCI_DMA_RESERVED		0x0
#define DW_PCI_DMA_RUNNING		0x1
#define DW_PCI_DMA_HALTED		0x2
#define DW_PCI_DMA_DONE			0x3
#endif
