/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "debug/debug.hpp"
#include "renderer/3d/pipeline/dynamic/core/programs/dynpip_tc_vu1_program.hpp"

extern u32 DynPipVU1_TC_CodeStart __attribute__((section(".vudata")));
extern u32 DynPipVU1_TC_CodeEnd __attribute__((section(".vudata")));

namespace Tyra {

DynPipTCVU1Program::DynPipTCVU1Program()
    : DynPipVU1Program(DynPipTextureColor, &DynPipVU1_TC_CodeStart,
                       &DynPipVU1_TC_CodeEnd,
                       ((u64)GIF_REG_ST) << 0 | ((u64)GIF_REG_RGBAQ) << 4 |
                           ((u64)GIF_REG_XYZ2) << 8,
                       3, 2) {}

DynPipTCVU1Program::~DynPipTCVU1Program() {}

std::string DynPipTCVU1Program::getStringName() const {
  return std::string("DynPip - TC");
}

void DynPipTCVU1Program::addProgramQBufferDataToPacket(packet2_t* packet,
                                                       DynPipBag* bag) const {
  u32 addr = VU1_DYNPIP_VERT_DATA_ADDR;

  // Add vertices
  packet2_utils_vu_add_unpack_data(packet, addr, bag->verticesFrom, bag->count,
                                   true);
  addr += bag->count;
  packet2_utils_vu_add_unpack_data(packet, addr, bag->verticesTo, bag->count,
                                   true);
  addr += bag->count;

  if (bag->texture) {
    // Add sts
    packet2_utils_vu_add_unpack_data(
        packet, addr, bag->texture->coordinatesFrom, bag->count, true);
    addr += bag->count;
    packet2_utils_vu_add_unpack_data(packet, addr, bag->texture->coordinatesTo,
                                     bag->count, true);
    addr += bag->count;
  }
}

}  // namespace Tyra
