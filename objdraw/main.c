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
#include <vif.h>

#include "mesh_data.c"

vifRegisterProgram(DynPipVU1_C);
vifRegisterProgram(DynPipVU1_D);
vifRegisterProgram(DynPipVU1_TC);
vifRegisterProgram(DynPipVU1_TD);

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

VECTOR *c_verts __attribute__((aligned(128))), *c_colours __attribute__((aligned(128)));

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

void calculate_cube(GSGLOBAL* gsGlobal, GSTEXTURE* Texture)
{
	u64 tmp;
	u64* p_data = cube_packet;

	p_data = vifAddScreenSizeData(p_data, gsGlobal);
	p_data = vifAddUInt(p_data, points_count);

	tmp = GIF_TAG(1, 0, 0, 0, 0, 1);
	p_data = vifAddGifTag(p_data, GIF_AD, tmp);
	p_data = vifAddGifTag(p_data, GS_TEX1_1, GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0));

	int tw, th;
	gsKit_set_tw_th(Texture, &tw, &th);

	p_data = vifAddGifTag(p_data, GS_TEX0_1, GS_SETREG_TEX0(
            Texture->Vram/256, Texture->TBW, Texture->PSM,
            tw, th, gsGlobal->PrimAlphaEnable, 0,
    		0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD));

	p_data = vifAddGifTag(p_data, DRAW_STQ2_REGLIST, VU_GS_GIFTAG(
			points_count, 1, 1,
    		VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
			0, gsGlobal->PrimAAEnable, 0, 0, 0),
        	0, 3));

	p_data = vifAddColorData(p_data, 128, 128, 128, 128);
}
/*

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
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, c_colours, faces_count, 1);
	vif_added_bytes += faces_count;

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_MSCAL, 0) << 32));

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	vifSendPacket(vif_packets[context], 1);

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}
*/
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

void render(GSGLOBAL* gsGlobal, GSTEXTURE* Texture)
{

	c_verts = (VECTOR *)memalign(128, sizeof(VECTOR) * points_count);
	c_colours = (VECTOR *)memalign(128, sizeof(VECTOR) * points_count);

	VECTOR* tmp1 = c_verts;
	VECTOR* tmp2 = c_colours;

	for (int i = 0; i < points_count; i++, tmp1++, tmp2++)
	{
		memcpy(tmp1, &vertices[points[i]], sizeof(VECTOR));
		memcpy(tmp2, &colours[points[i]], sizeof(VECTOR));
	}

	// Create the view_screen matrix.
	create_view_screen(view_screen, 4/3, -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);
	//calculate_cube(gsGlobal, Texture);

	if (gsGlobal->ZBuffering == GS_SETTING_ON)
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// The main loop...
	for (;;)
	{
		// Spin the cube a bit.
		object_rotation[0] += 0.008f; //while (object_rotation[0] > 3.14f) { object_rotation[0] -= 6.28f; }
		object_rotation[1] += 0.012f; //while (object_rotation[1] > 3.14f) { object_rotation[1] -= 6.28f; }

		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x40,0x40,0x40,0x80,0x00));

		//draw_cube();

		gsKit_sync_flip(gsGlobal);
		gsKit_queue_exec(gsGlobal);
	}
}

int main(int argc, char *argv[])
{
	// Initialize vif packets
	cube_packet =    vifCreatePacket(6);
	vif_packets[0] = vifCreatePacket(6);
	vif_packets[1] = vifCreatePacket(6);

	vu1_upload_micro_program(&DynPipVU1_C_CodeStart, &DynPipVU1_C_CodeEnd);
	vu1_set_double_buffer_settings();

	GSTEXTURE gsTexture;

	// Init the GS, framebuffer, zbuffer, and texture buffer.
	GSGLOBAL* gsGlobal = init_graphics();

	// Render textured cube
	render(gsGlobal, &gsTexture);

	vifDestroyPacket(vif_packets[0]);
	vifDestroyPacket(vif_packets[1]);
	vifDestroyPacket(cube_packet);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}