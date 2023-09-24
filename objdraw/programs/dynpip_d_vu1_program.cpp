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
#include "renderer/3d/pipeline/dynamic/core/programs/dynpip_d_vu1_program.hpp"

extern u32 DynPipVU1_D_CodeStart __attribute__((section(".vudata")));
extern u32 DynPipVU1_D_CodeEnd __attribute__((section(".vudata")));

namespace Tyra {

DynPipDVU1Program::DynPipDVU1Program()
    : DynPipVU1Program(
          DynPipDirLights, &DynPipVU1_D_CodeStart, &DynPipVU1_D_CodeEnd,
          ((u64)GIF_REG_RGBAQ) << 0 | ((u64)GIF_REG_XYZ2) << 4, 2, 2) {}

DynPipDVU1Program::~DynPipDVU1Program() {}

std::string DynPipDVU1Program::getStringName() const {
  return std::string("DynPip - D");
}

void DynPipDVU1Program::addProgramQBufferDataToPacket(packet2_t* packet,
                                                      DynPipBag* bag) const {
  u32 addr = VU1_DYNPIP_VERT_DATA_ADDR;

  // Add vertices
  packet2_utils_vu_add_unpack_data(packet, addr, bag->verticesFrom, bag->count,
                                   true);
  addr += bag->count;
  packet2_utils_vu_add_unpack_data(packet, addr, bag->verticesTo, bag->count,
                                   true);
  addr += bag->count;

  if (bag->lighting) {
    // Add normal
    packet2_utils_vu_add_unpack_data(packet, addr, bag->lighting->normalsFrom,
                                     bag->count, true);
    addr += bag->count;
    packet2_utils_vu_add_unpack_data(packet, addr, bag->lighting->normalsTo,
                                     bag->count, true);
    addr += bag->count;
  }
}

}  // namespace Tyra
