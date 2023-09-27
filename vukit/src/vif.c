#include "vif.h"

void vifSendPacket(vifPacket* packet, uint32_t vif_channel) {
    dmaKit_wait(vif_channel, 0);
	FlushCache(0);
	dmaKit_send_chain(vif_channel, (void *)((u32)packet->base_ptr & 0x0FFFFFFF), 0);
}

vifPacket *vifCreatePacket(uint32_t size) {
	vifPacket* packet = memalign(64, sizeof(vifPacket));
	packet->size = size;
	packet->cur_ptr.p = packet->base_ptr = memalign(64, packet->size*16);
    return packet;
}

void vifDestroyPacket(vifPacket* packet) {
	free(packet->base_ptr);
    free(packet);
}

int getbufferDepth(GSGLOBAL* gsGlobal) {
	switch(gsGlobal->PSMZ){
		case GS_PSMZ_32:
			return 32;
		case GS_PSMZ_24:
			return 24;
		case GS_PSMZ_16:
		case GS_PSMZ_16S:
			return 16;
		default:
			return -1;
	}
}

static inline u32 get_packet_size_for_program(u32 *start, u32 *end)
{
    // Count instructions
    u32 count = (end - start) / 2;
    if (count & 1)
        count++;
    return (count >> 8) + 1;
}

void vu1_upload_micro_program(u32* start, u32* end)
{
	u32 packet_size = get_packet_size_for_program(start, end) + 1; // + 1 for end tag
	vifPacket* packet = vifCreatePacket(packet_size);

	// get the size of the code as we can only send 256 instructions in each MPGtag
	u32 dest = 0;
    u32 count = (end - start) / 2;
    if (count & 1)
        count++;

    u32 *l_start = start;

    while (count > 0)
    {
        u16 curr_count = count > 256 ? 256 : count;

		*packet->cur_ptr.qw++ = DMA_TAG(curr_count / 2, 0, DMA_REF, 0, (const u128 *)l_start, 0);
		*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
		*packet->cur_ptr.dw++ = VIF_CODE(dest, curr_count & 0xFF, VIF_MPG, 0);

        l_start += curr_count * 2;
        count -= curr_count;
        dest += curr_count;
    }

	*packet->cur_ptr.qw++ = DMA_TAG(0, 0, DMA_END, 0, 0, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);

    vifSendPacket(packet, 1);
	vifDestroyPacket(packet);
}

void vu1_set_double_buffer_settings()
{
	vifPacket* packet = vifCreatePacket(2);

	*packet->cur_ptr.qw++ = DMA_TAG(0, 0, DMA_CNT, 0, 0 , 0);
	*packet->cur_ptr.dw++ = VIF_CODE(8, 0, VIF_BASE, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(496, 0, VIF_OFFSET, 0);

	*packet->cur_ptr.qw++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);
	*packet->cur_ptr.dw++ = VIF_CODE(0, 0, VIF_NOP, 0);

    vifSendPacket(packet, 1);
	vifDestroyPacket(packet);
}