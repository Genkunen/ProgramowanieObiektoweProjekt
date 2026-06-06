import "../../shared_code_shim";
import "../../math/div_ceil";
import "../../math/interwave_prefix_sum";

[vk::constant_id(0)] const uint SET_WAVE_SIZE = 4; // pessimistic assumption if not set

[vk::push_constant] pop::shaders::SimulationAccelerationGridRadixSortGlobalPrefixSumCSPushConstants push_constants;

// ------------------------------------------------------------------------------------------------------------------------------------------------------------

static const uint ASSUMED_MIN_WAVE_SIZE = 4;
static const uint MAX_WAVE_TOTAL_SUMS = pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS / SET_WAVE_SIZE;

groupshared uint gs_intrawave_prefix_sum_scratch[MAX_WAVE_TOTAL_SUMS];

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint tid = dtid.x;
    uint group_tid = gtid.x;
    uint group_id = gid.x;

    uint hardware_wave_lane_count = WaveGetLaneCount();
    uint wave_lane = WaveGetLaneIndex();

    uint wave_count = div_ceil(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, hardware_wave_lane_count);
    uint wave_index = group_tid / hardware_wave_lane_count;

    uint column_sum = push_constants.global_histogram[tid];
    uint prefix_sum = compute_interwave_prefix_sum(column_sum, hardware_wave_lane_count, wave_count, wave_index, wave_lane, gs_intrawave_prefix_sum_scratch);

    push_constants.global_histogram[tid] = prefix_sum;
}