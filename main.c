/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2022 Daniel Santos <danielsantos346@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
# VU1 and libpacket2 showcase.
*/

#include <kernel.h>
#include <malloc.h>
#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <packet2.h>
#include <packet2_utils.h>
#include "cube.c"
#include "mesh_data.c"

extern unsigned char cube[];

extern u32 VU1Draw3D_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3D_CodeEnd __attribute__((section(".vudata")));

VECTOR object_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR object_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };

MATRIX local_world, world_view, view_screen, local_screen;

packet2_t *vif_packets[2] __attribute__((aligned(64)));
packet2_t *curr_vif_packet;

/** Cube data */
packet2_t *cube_packet;

u8 context = 0;

prim_t prim;
clutbuffer_t clut;
lod_t lod;

VECTOR *c_verts __attribute__((aligned(128))), *c_sts __attribute__((aligned(128)));

/** Calculate packet for cube data */
void calculate_cube(texbuffer_t *t_texbuff)
{
	packet2_add_float(cube_packet, 2048.0f+320.0f);					  // scale
	packet2_add_float(cube_packet, 2048.0f+224.0f);					  // scale
	packet2_add_float(cube_packet, ((float)0xFFFFFF) / 32.0F); // scale
	packet2_add_s32(cube_packet, faces_count);				  // vertex count
	packet2_utils_gif_add_set(cube_packet, 1);
	packet2_utils_gs_add_lod(cube_packet, &lod);
    packet2_add_2x_s64(
        cube_packet,
        GS_SETREG_TEX0(
            t_texbuff->address/256,
            t_texbuff->width >> 6,
            t_texbuff->psm,
            t_texbuff->info.width,
            t_texbuff->info.height,
            t_texbuff->info.components,
            t_texbuff->info.function,
            clut.address/256,
            clut.psm,
            clut.storage_mode,
            clut.start,
            clut.load_method),
        GS_TEX0_1);
	packet2_utils_gs_add_prim_giftag(cube_packet, &prim, faces_count, DRAW_STQ2_REGLIST, 3, 0);
	u8 j = 0; // RGBA
	for (j = 0; j < 4; j++)
		packet2_add_u32(cube_packet, 128);
}

/** Calculate cube position and add packet with cube data */
void draw_cube()
{
	create_local_world(local_world, object_position, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	curr_vif_packet = vif_packets[context];
	packet2_reset(curr_vif_packet, 0);

	// Add matrix at the beggining of VU mem (skip TOP)
	packet2_utils_vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 8, 0);

	u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
							 // we don't wan't to unpack at 8 + beggining of buffer, but at
							 // the beggining of the buffer

	// Merge packets
	packet2_utils_vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet->base, packet2_get_qw_count(cube_packet), 1);
	vif_added_bytes += packet2_get_qw_count(cube_packet);

	// Add vertices
	packet2_utils_vu_add_unpack_data(curr_vif_packet, vif_added_bytes, c_verts, faces_count, 1);
	vif_added_bytes += faces_count; // one VECTOR is size of qword

	// Add sts
	packet2_utils_vu_add_unpack_data(curr_vif_packet, vif_added_bytes, c_sts, faces_count, 1);
	vif_added_bytes += faces_count;

	packet2_utils_vu_add_start_program(curr_vif_packet, 0);
	packet2_utils_vu_add_end_tag(curr_vif_packet);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	FlushCache(0);
	dmaKit_send_chain(DMA_CHANNEL_VIF1, (void *)((u32)curr_vif_packet->base & 0x0FFFFFFF), 0);

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
void send_texture(GSGLOBAL* gsGlobal, texbuffer_t *texbuf)
{
	texbuf->width = 128;
	texbuf->psm = GS_PSM_CT24;
	texbuf->address = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(128, 128, GS_PSM_CT24), GSKIT_ALLOC_USERBUFFER);

	gsKit_texture_send((u32*)cube, texbuf->width, texbuf->width, texbuf->address, texbuf->psm, texbuf->width>>6, GS_CLUT_NONE);
}

unsigned char gsKit_log2(unsigned int x)
{

	unsigned char res;

	__asm__ __volatile__ ("plzcw %0, %1\n\t" : "=r" (res) : "r" (x));

	res = 31 - (res + 1);
	res += (x > (unsigned int)(1<<res) ? 1 : 0);

	return res;
}

void set_lod_clut_prim_tex_buff(texbuffer_t *t_texbuff)
{
	lod.calculation = LOD_USE_K;
	lod.max_level = 0;
	lod.mag_filter = LOD_MAG_NEAREST;
	lod.min_filter = LOD_MIN_NEAREST;
	lod.l = 0;
	lod.k = 0;

	clut.storage_mode = CLUT_STORAGE_MODE1;
	clut.start = 0;
	clut.psm = 0;
	clut.load_method = CLUT_NO_LOAD;
	clut.address = 0;

	// Define the triangle primitive we want to use.
	prim.type = PRIM_TRIANGLE;
	prim.shading = PRIM_SHADE_GOURAUD;
	prim.mapping = GS_ENABLE;
	prim.fogging = GS_DISABLE;
	prim.blending = GS_ENABLE;
	prim.antialiasing = GS_DISABLE;
	prim.mapping_type = PRIM_MAP_ST;
	prim.colorfix = PRIM_UNFIXED;

	t_texbuff->info.width = gsKit_log2(128);
	t_texbuff->info.height = gsKit_log2(128);
	t_texbuff->info.components = TEXTURE_COMPONENTS_RGB;
	t_texbuff->info.function = TEXTURE_FUNCTION_DECAL;
}

void render(GSGLOBAL* gsGlobal, texbuffer_t *t_texbuff)
{
	int i;

	set_lod_clut_prim_tex_buff(t_texbuff);

	/** 
	 * Allocate some space for object position calculating. 
	 * c_ prefix = calc_
	 */
	c_verts = (VECTOR *)memalign(128, sizeof(VECTOR) * faces_count);
	c_sts = (VECTOR *)memalign(128, sizeof(VECTOR) * faces_count);



	for (i = 0; i < faces_count; i++)
	{
		c_verts[i][0] = vertices[faces[i]][0];
		c_verts[i][1] = vertices[faces[i]][1];
		c_verts[i][2] = vertices[faces[i]][2];
		c_verts[i][3] = vertices[faces[i]][3];

		c_sts[i][0] = sts[faces[i]][0];
		c_sts[i][1] = sts[faces[i]][1];
		c_sts[i][2] = sts[faces[i]][2];
		c_sts[i][3] = sts[faces[i]][3];
	}

	// Create the view_screen matrix.
	create_view_screen(view_screen, 4/3, -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);
	calculate_cube(t_texbuff);

	if (gsGlobal->ZBuffering == GS_SETTING_ON)
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// The main loop...
	for (;;)
	{
		// Spin the cube a bit.
		object_rotation[0] += 0.008f; //while (object_rotation[0] > 3.14f) { object_rotation[0] -= 6.28f; }
		object_rotation[1] += 0.012f; //while (object_rotation[1] > 3.14f) { object_rotation[1] -= 6.28f; }

		gsKit_clear(gsGlobal,  GS_SETREG_RGBAQ(0x40,0x40,0x40,0x80,0x00));

		draw_cube();

		gsKit_queue_exec(gsGlobal);
		gsKit_sync_flip(gsGlobal);
	}
}

void vu1_set_double_buffer_settings()
{
	packet2_t *packet2 = packet2_create(1, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
	packet2_utils_vu_add_double_buffer(packet2, 8, 496);
	packet2_utils_vu_add_end_tag(packet2);
	FlushCache(0);
	dmaKit_send_chain(DMA_CHANNEL_VIF1, (void *)((u32)packet2->base & 0x0FFFFFFF), 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	packet2_free(packet2);
}

void vu1_upload_micro_program()
{
	u32 packet_size = packet2_utils_get_packet_size_for_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd) + 1; // + 1 for end tag
	packet2_t *packet2 = packet2_create(packet_size, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
	packet2_vif_add_micro_program(packet2, 0, &VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd);
	packet2_utils_vu_add_end_tag(packet2);
	FlushCache(0);
	dmaKit_send_chain(DMA_CHANNEL_VIF1, (void *)((u32)packet2->base & 0x0FFFFFFF), 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	packet2_free(packet2);
}

int main(int argc, char *argv[])
{
	// Initialize vif packets
	cube_packet = packet2_create(10, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
	vif_packets[0] = packet2_create(11, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
	vif_packets[1] = packet2_create(11, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);

	vu1_upload_micro_program();
	vu1_set_double_buffer_settings();

	texbuffer_t texbuff;

	// Init the GS, framebuffer, zbuffer, and texture buffer.
	GSGLOBAL* gsGlobal = init_graphics();

	// Load the texture into vram.
	send_texture(gsGlobal, &texbuff);

	// Render textured cube
	render(gsGlobal, &texbuff);

	packet2_free(vif_packets[0]);
	packet2_free(vif_packets[1]);
	packet2_free(cube_packet);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}
