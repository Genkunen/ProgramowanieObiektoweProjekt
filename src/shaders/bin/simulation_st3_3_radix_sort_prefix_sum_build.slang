#include "../shared_types.hpp"
#include "../shared_consts.hpp"

import "../radix_sort/histogram";

struct PushConstants {
    RadixHistogram<uint, pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS>* global_histogram;
};

[vk::push_constant] PushConstants push_constants;

uint div_ceil(uint a, uint b) {
    return (a + b - 1) / b;
}

// Hardware has different wave sizes, but it is not possible to reliably dynamically size arrays in LDS to depend on the hardware wave size.
// Therefore we have to make a guess. Typically we see wave8/wave16 on Intel Arc, wave32 on NVIDIA (and in some cases AMD), and wave64 on AMD.
// I read somewhere that wave4 is a thing so I'm going to assume that's the minimum.
static const uint ASSUMED_MIN_WAVE_SIZE = 4;

static const uint MAX_WAVE_TOTAL_SUMS = pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS / ASSUMED_MIN_WAVE_SIZE;

groupshared uint gs_wave_total_sums[MAX_WAVE_TOTAL_SUMS];

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;

    uint hardware_wave_lane_count = WaveGetLaneCount();
    uint wave_lane = WaveGetLaneIndex();

    uint wave_count = div_ceil(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_RADIX_SORT_HISTOGRAM_RADIX_BUCKETS, hardware_wave_lane_count);
    uint wave_index = tid / hardware_wave_lane_count;

    bool is_first_wave = (wave_index == 0);
    bool is_last_wave_lane = (wave_lane == hardware_wave_lane_count - 1);

    uint lane_hist_value = push_constants.global_histogram->buckets[tid];

    // First perform an intra-wave prefix sum.
    // intrawave_prefix_sum becomes the sum of lane_hist_value of lanes [0, wave_lane - 1] in the wave.
    uint intrawave_prefix_sum = WavePrefixSum(lane_hist_value);

    // Write the total sum of lane_hist_value in this wave to the total sums table in LDS.
    if (is_last_wave_lane) {
        // intrawave_prefix_sum in last lane is the sum of all the others except this lane, so add it to get the total sum of lane_hist_value in this wave.
        uint total_sum = intrawave_prefix_sum + lane_hist_value;

        gs_wave_total_sums[wave_index] = total_sum;
    }
    GroupMemoryBarrierWithGroupSync();

    // Have the first wave transform wave total sums into prefix sums.
    if (is_first_wave) {
        // TODO: support GPUs with execution mode narrower than wave16
        bool is_participating_lane = (wave_lane < wave_count);

        uint value = is_participating_lane ? gs_wave_total_sums[wave_lane] : 0;
        uint value_prefix_sum = WavePrefixSum(value);

        if (is_participating_lane) gs_wave_total_sums[wave_lane] = value_prefix_sum;
    }
    GroupMemoryBarrierWithGroupSync();

    // Once gs_wave_total_sums is transformed into prefix sums we can have individual waves add their corresponding prefix sum in the table to their
    // intrawave prefix sums to obtain the interwave prefix sum.
    uint wave_prefix_sum_in_group = gs_wave_total_sums[wave_index];

    uint interwave_prefix_sum = intrawave_prefix_sum + wave_prefix_sum_in_group;

    push_constants.global_histogram->buckets[tid] = interwave_prefix_sum;
}