#pragma once

namespace pop::shader_consts {

#ifdef __cplusplus
#include <cstdint>
using uint = std::uint32_t;
#endif

static const uint GPU_WAVE_SIZE = 32;

static const uint CS_UPLOAD_MESHES_GROUP_SIZE_X = 32;
static const uint CS_CLEAR_INSTANCE_COUNT_GROUP_SIZE_X = 32;
static const uint CS_SIMULATION_STEP_GROUP_SIZE_X = 256;
static const uint CS_BUILD_INDIRECT_INSTANCE_COUNT_GROUP_SIZE_X = 256;
static const uint CS_BUILD_INDIRECT_FIRST_INSTANCE_GROUP_SIZE_X = GPU_WAVE_SIZE;
static const uint CS_BUILD_INSTANCE_BUFFER_GROUP_SIZE_X = 256;

static const uint MAX_DRAW_COMMANDS = GPU_WAVE_SIZE;

}