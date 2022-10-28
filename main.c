/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2022 Daniel Santos <danielsantos346@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
# VU1 + gsKit showcase.
*/

#include <kernel.h>
#include <malloc.h>
#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <math3d.h>
#include <time.h>
#include <stdio.h>
#include "cube.c"
#include "mesh_data.c"

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

#define DRAW_STQ2_REGLIST \
	((u64)GS_ST)     << 0 | \
	((u64)GS_RGBAQ)  << 4 | \
	((u64)GS_XYZ2)   << 8

/** Texture Alpha Expansion */
#define ALPHA_EXPAND_NORMAL			0
#define ALPHA_EXPAND_TRANSPARENT	1

#define VIF_CODE(_immediate, _num, _cmd, _irq) ((u32)(_immediate) | ((u32)(_num) << 16) | ((u32)(_cmd) << 24) | ((u32)(_irq) << 31))

#define VU_GS_PRIM(PRIM, IIP, TME, FGE, ABE, AA1, FST, CTXT, FIX) (u128)(((FIX << 10) | (CTXT << 9) | (FST << 8) | (AA1 << 7) | (ABE << 6) | (FGE << 5) | (TME << 4) | (IIP << 3) | (PRIM)))
#define VU_GS_GIFTAG(NLOOP, EOP, PRE, PRIM, FLG, NREG) (((u64)(NREG) << 60) | ((u64)(FLG) << 58) | ((u64)(PRIM) << 47) | ((u64)(PRE) << 46) | (EOP << 15) | (NLOOP << 0))

#define GIF_SET_TAG(NLOOP, EOP, PRE, PRIM, FLG, NREG)                    \
    (u64)((NLOOP)&0x00007FFF) << 0 | (u64)((EOP)&0x00000001) << 15 |     \
        (u64)((PRE)&0x00000001) << 46 | (u64)((PRIM)&0x000007FF) << 47 | \
        (u64)((FLG)&0x00000003) << 58 | (u64)((NREG)&0x0000000F) << 60

extern unsigned char cube[];

extern u32 VU1Draw3D_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3D_CodeEnd __attribute__((section(".vudata")));

VECTOR object_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR object_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };

MATRIX local_world, world_view, view_screen, local_screen;

u64* vif_packets[2] __attribute__((aligned(64)));
u64* curr_vif_packet;

/** Cube data */
u64 *cube_packet;

u8 context = 0;

VECTOR *c_verts __attribute__((aligned(128))), *c_sts __attribute__((aligned(128)));

static inline u32 lzw(u32 val)
{
	u32 res;
	__asm__ __volatile__ ("   plzcw   %0, %1    " : "=r" (res) : "r" (val));
	return(res);
}

static inline void gsKit_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th)
{
	*tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<*tw))
		(*tw)++;

	*th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<*th))
		(*th)++;
}

/** Calculate packet for cube data */
void calculate_cube(GSGLOBAL* gsGlobal, GSTEXTURE* Texture)
{
	float fX = 2048.0f+gsGlobal->Width/2;
	float fY = 2048.0f+gsGlobal->Height/2;
	float fZ = ((float)0xFFFFFF) / 32.0F;

	u64* p_data = cube_packet;

	*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
	*p_data++ = (*(u32*)(&fZ) | (u64)faces_count << 32);

	*p_data++ = GIF_SET_TAG(1, 0, 0, 0, 0, 1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0);
	*p_data++ = GS_TEX1_1;

	int tw, th;
	gsKit_set_tw_th(Texture, &tw, &th);

	*p_data++ = GS_SETREG_TEX0(
            Texture->Vram/256, Texture->TBW, Texture->PSM,
            tw, th, gsGlobal->PrimAlphaEnable, 0,
    		0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
	*p_data++ = GS_TEX0_1;

	*p_data++ = VU_GS_GIFTAG(faces_count, 1, 1,
    	VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
		gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
        0, 3);

	*p_data++ = DRAW_STQ2_REGLIST;

	*p_data++ = (128 | (u64)128 << 32);
	*p_data++ = (128 | (u64)128 << 32);	
}


static inline u64* vu_add_unpack_data(u64 *p_data, u32 t_dest_address, void *t_data, u32 t_size, u8 t_use_top)
{
    *p_data++ = DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0);
	*p_data++ = (VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0) | (u64)
	VIF_CODE(t_dest_address | ((u32)1 << 14) | ((u32)t_use_top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((u32)0 << 4) | 0x60, 0) << 32 );

	return p_data;
}

/** Calculate cube position and add packet with cube data */
void draw_cube()
{
	create_local_world(local_world, object_position, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	curr_vif_packet = vif_packets[context];

	memset(curr_vif_packet, 16*6, 0);
	
	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 8, 0);

	u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
							 // we don't wan't to unpack at 8 + beggining of buffer, but at
							 // the beggining of the buffer

	// Merge packets
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 6, 1);
	vif_added_bytes += 6;

	// Add vertices
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, c_verts, faces_count, 1);
	vif_added_bytes += faces_count; // one VECTOR is size of qword

	// Add sts
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, c_sts, faces_count, 1);
	vif_added_bytes += faces_count;

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_MSCAL, 0) << 32));

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	FlushCache(0);
	dmaKit_send_chain(DMA_CHANNEL_VIF1, (void *)((u32)vif_packets[context] & 0x0FFFFFFF), 0);

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}

GSGLOBAL* init_graphics()
{
	GSGLOBAL* gsGlobal = gsKit_init_global();

	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	//gsGlobal->PrimAAEnable = GS_SETTING_ON;

	gsGlobal->PSM  = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_32;

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_VIF1);
	dmaKit_wait(DMA_CHANNEL_GIF, 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

	gsKit_vram_clear(gsGlobal);

	gsKit_init_screen(gsGlobal);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	return gsGlobal;

}

/** Send texture data to GS. */
void send_texture(GSGLOBAL* gsGlobal, GSTEXTURE* Texture)
{
	Texture->Width = 128;
	Texture->Height = 128;
	Texture->PSM = GS_PSM_CT24;
	Texture->Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(128, 128, GS_PSM_CT24), GSKIT_ALLOC_USERBUFFER);
	Texture->Mem = (u32*)cube;

	gsKit_texture_upload(gsGlobal, Texture);
}

void render(GSGLOBAL* gsGlobal, GSTEXTURE* Texture)
{
	/** 
	 * Allocate some space for object position calculating. 
	 * c_ prefix = calc_
	 */
	c_verts = (VECTOR *)memalign(128, sizeof(VECTOR) * faces_count);
	c_sts = (VECTOR *)memalign(128, sizeof(VECTOR) * faces_count);

	VECTOR* tmp1 = c_verts;
	VECTOR* tmp2 = c_sts;

	for (int i = 0; i < faces_count; i++, tmp1++, tmp2++)
	{
		memcpy(tmp1, &vertices[faces[i]], sizeof(VECTOR));
		memcpy(tmp2, &sts[faces[i]], sizeof(VECTOR));
	}

	// Create the view_screen matrix.
	create_view_screen(view_screen, 4/3, -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);
	calculate_cube(gsGlobal, Texture);

	if (gsGlobal->ZBuffering == GS_SETTING_ON)
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// The main loop...
	for (;;)
	{
		// Spin the cube a bit.
		object_rotation[0] += 0.008f; //while (object_rotation[0] > 3.14f) { object_rotation[0] -= 6.28f; }
		object_rotation[1] += 0.012f; //while (object_rotation[1] > 3.14f) { object_rotation[1] -= 6.28f; }

		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x40,0x40,0x40,0x80,0x00));

		draw_cube();

		gsKit_queue_exec(gsGlobal);
		gsKit_sync_flip(gsGlobal);
	}
}

void vu1_set_double_buffer_settings()
{
	u64* p_data;
	u64* p_store;
	p_data = p_store = memalign(128, 4*sizeof(u64));

	*p_data++ = DMA_TAG(0, 0, DMA_CNT, 0, 0 , 0);
	*p_data++ = (VIF_CODE(8, 0, VIF_BASE, 0) | (u64)VIF_CODE(496, 0, VIF_OFFSET, 0) << 32);

	*p_data++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*p_data++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	FlushCache(0);
	dmaKit_send_chain(DMA_CHANNEL_VIF1, (void *)((u32)p_store & 0x0FFFFFFF), 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	free(p_store);
}

    static inline u32 get_packet_size_for_program(u32 *start, u32 *end)
    {
        // Count instructions
        u32 count = (end - start) / 2;
        if (count & 1)
            count++;
        return (count >> 8) + 1;
    }

void vu1_upload_micro_program()
{
	u32 packet_size = get_packet_size_for_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd) + 1; // + 1 for end tag
	u64* p_store;
	u64* p_data = p_store = memalign(128, 16*packet_size);

	// get the size of the code as we can only send 256 instructions in each MPGtag
	u32 dest = 0;
    u32 count = (&VU1Draw3D_CodeEnd - &VU1Draw3D_CodeStart) / 2;
    if (count & 1)
        count++;

    u32 *l_start = &VU1Draw3D_CodeStart;

    while (count > 0)
    {
        u16 curr_count = count > 256 ? 256 : count;

		*p_data++ = DMA_TAG(curr_count / 2, 0, DMA_REF, 0, (const u128 *)l_start, 0);

		*p_data++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(dest, curr_count & 0xFF, VIF_MPG, 0) << 32);

        l_start += curr_count * 2;
        count -= curr_count;
        dest += curr_count;
    }

	*p_data++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*p_data++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);


	FlushCache(0);
	dmaKit_send_chain(DMA_CHANNEL_VIF1, (void *)((u32)p_store & 0x0FFFFFFF), 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	free(p_store);
}

int main(int argc, char *argv[])
{
	// Initialize vif packets
	cube_packet = memalign(128, 6*16);
	vif_packets[0] = memalign(128, 6*16);
	vif_packets[1] = memalign(128, 6*16);

	vu1_upload_micro_program();
	vu1_set_double_buffer_settings();

	GSTEXTURE gsTexture;

	// Init the GS, framebuffer, zbuffer, and texture buffer.
	GSGLOBAL* gsGlobal = init_graphics();

	// Load the texture into vram.
	send_texture(gsGlobal, &gsTexture);

	// Render textured cube
	render(gsGlobal, &gsTexture);

	free(vif_packets[0]);
	free(vif_packets[1]);
	free(cube_packet);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}
