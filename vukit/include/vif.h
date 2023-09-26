#ifndef VIF_H_
#define VIF_H_

#include <gsKit.h>
#include <dmaKit.h>
#include <stdint.h>
#include <stdio.h>
#include <kernel.h>
#include <malloc.h>
#include <tamtypes.h>

typedef struct {
	size_t size;
	void* base_ptr;
	union {
		void* p;
		float* f;
		uint8_t* b;
		uint16_t* w;
		uint32_t* dw;
		uint64_t* qw;
		__uint128_t* ow;
	} cur_ptr;
} vifPacket;

#define UNPACK_S_32 0x00
#define UNPACK_S_16 0x01
#define UNPACK_S_8 0x02
#define UNPACK_V2_32 0x04
#define UNPACK_V2_16 0x05
#define UNPACK_V2_8 0x06
#define UNPACK_V3_32 0x08
#define UNPACK_V3_16 0x09
#define UNPACK_V3_8 0x0A
#define UNPACK_V4_32 0x0C
#define UNPACK_V4_16 0x0D
#define UNPACK_V4_8 0x0E
#define UNPACK_V4_5 0x0F

#define VIF_NOP 0
#define VIF_STCYCL 1
#define VIF_OFFSET 2
#define VIF_BASE 3
#define VIF_ITOP 4
#define VIF_STMOD 5
#define VIF_MSKPATH3 6
#define VIF_MARK 7
#define VIF_FLUSHE 16
#define VIF_FLUSH 17
#define VIF_FLUSHA 19
#define VIF_MSCAL 20
#define VIF_MSCNT 23
#define VIF_MSCALF 21
#define VIF_STMASK 32
#define VIF_STROW 48
#define VIF_STCOL 49
#define VIF_MPG 74
#define VIF_DIRECT 80
#define VIF_DIRECTHL 81

#define vifRegisterProgram(NAME)  extern u32 NAME ## _CodeStart __attribute__((section(".vudata"))); \
								  extern u32 NAME ## _CodeEnd __attribute__((section(".vudata")));

#define VIF_CODE(_immediate, _num, _cmd, _irq) ((u32)(_immediate) | ((u32)(_num) << 16) | ((u32)(_cmd) << 24) | ((u32)(_irq) << 31))

#define VU1_COL_REGLIST \
	((u64)GS_RGBAQ)  << 0 | \
	((u64)GS_XYZ2)   << 4

#define DRAW_STQ2_REGLIST \
	((u64)GS_ST)     << 0 | \
	((u64)GS_RGBAQ)  << 4 | \
	((u64)GS_XYZ2)   << 8

/** Texture Alpha Expansion */
#define ALPHA_EXPAND_NORMAL			0
#define ALPHA_EXPAND_TRANSPARENT	1

#define VU_GS_PRIM(PRIM, IIP, TME, FGE, ABE, AA1, FST, CTXT, FIX) (u128)(((FIX << 10) | (CTXT << 9) | (FST << 8) | (AA1 << 7) | (ABE << 6) | (FGE << 5) | (TME << 4) | (IIP << 3) | (PRIM)))
#define VU_GS_GIFTAG(NLOOP, EOP, PRE, PRIM, FLG, NREG) (((u64)(NREG) << 60) | ((u64)(FLG) << 58) | ((u64)(PRIM) << 47) | ((u64)(PRE) << 46) | (EOP << 15) | (NLOOP << 0))

static inline void vifAddUnpackData(vifPacket* packet, u32 t_dest_address, void *t_data, u32 t_size, u8 t_use_top)
{
    *packet->cur_ptr.qw++ = DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(t_dest_address | ((u32)1 << 14) | ((u32)t_use_top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((u32)0 << 4) | 0x60, 0);
}

static inline void vifOpenUnpack(vifPacket* packet, u32 t_dest_address, u8 t_use_top)
{
    *packet->cur_ptr.qw++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(t_dest_address | ((u32)1 << 14) | ((u32)t_use_top << 15), 0, UNPACK_V4_32 | ((u32)0 << 4) | 0x60, 0);
}

static inline void vifCloseUnpack(vifPacket* packet)
{
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
}

static inline void vifAddEndTag(vifPacket* packet)
{
	*packet->cur_ptr.qw++ = DMA_TAG(0, 0, DMA_END, 0, 0, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
}

static inline void vifStartProgram(vifPacket *packet) {
	*packet->cur_ptr.qw++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_FLUSH, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_MSCAL, 0);
	
}

static inline void vifResetPacket(vifPacket *packet) {
	packet->cur_ptr.p = packet->base_ptr;
}

static inline void vifClearPacket(vifPacket *packet) {
	packet->cur_ptr.p = packet->base_ptr;
	memset(packet->base_ptr, 0, 16*packet->size);
}

void vu1_upload_micro_program(u32* start, u32* end);

void vu1_set_double_buffer_settings();

void vifSendPacket(vifPacket* packet, uint32_t vif_channel);

vifPacket *vifCreatePacket(uint32_t size);

void vifDestroyPacket(vifPacket* packet);

int getbufferDepth(GSGLOBAL* gsGlobal);

void vifAddScreenSizeData(vifPacket* packet, GSGLOBAL* gsGlobal);

void vifAddFloat(vifPacket* packet, float f);

void vifAddUInt(vifPacket* packet, uint32_t n);

void vifAddGifTag(vifPacket* packet, uint64_t tag, uint64_t data);

void vifAddColorData(vifPacket* packet, uint32_t r, uint32_t g, uint32_t b, uint32_t a);

#endif
