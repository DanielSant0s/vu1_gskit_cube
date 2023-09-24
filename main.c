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


#include <gsKit.h>
#include <dmaKit.h>
#include <math3d.h>
#include <time.h>
#include <stdio.h>
#include "cube.c"
#include "mesh_data.c"
#include "vif.h"

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

	*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
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
		0, gsGlobal->PrimAAEnable, 0, 0, 0),
        0, 3);

	*p_data++ = DRAW_STQ2_REGLIST;

	*p_data++ = (128 | (u64)128 << 32);
	*p_data++ = (128 | (u64)128 << 32);	
}

/** Calculate cube position and add packet with cube data */
void draw_cube()
{
	create_local_world(local_world, object_position, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	curr_vif_packet = vif_packets[context];

	memset(curr_vif_packet, 0, 16*6);
	
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

	vifSendPacket(vif_packets[context], 1);

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}

GSGLOBAL* init_graphics()
{
	GSGLOBAL* gsGlobal = gsKit_init_global();

	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->PrimAAEnable = GS_SETTING_OFF;

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

		gsKit_sync_flip(gsGlobal);
		gsKit_queue_exec(gsGlobal);
	}
}

int main(int argc, char *argv[])
{
	// Initialize vif packets
	cube_packet = vifCreatePacket(6);
	vif_packets[0] = vifCreatePacket(6);
	vif_packets[1] = vifCreatePacket(6);

	vu1_upload_micro_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd);
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
