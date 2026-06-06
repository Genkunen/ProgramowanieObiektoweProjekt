import "div_ceil";

uint compute_interwave_prefix_sum(uint val, uint hardware_wave_lane_count, uint group_wave_count, uint wave_index, uint wave_lane, inout uint gs_scratch[]) {
    bool is_first_wave = wave_index == 0;
    bool is_last_wave_lane = wave_lane == hardware_wave_lane_count - 1;

    // First perform an intra-wave prefix sum.
    // intrawave_prefix_sum becomes the sum of lane_hist_value of lanes [0, wave_lane - 1] in the wave.
    uint intrawave_prefix_sum = WavePrefixSum(val);

    // Have the last lane in a wave write the the total sum of val in that wave to the total sums table in LDS.
    if (is_last_wave_lane) {
        uint intrawave_total_sum = intrawave_prefix_sum + val;
        gs_scratch[wave_index] = intrawave_total_sum;
    }
    GroupMemoryBarrierWithGroupSync();

    // Have the first wave in the group transform the intrawave total sums into a prefix sum of it.
    if (is_first_wave) {
        uint iterations = div_ceil(group_wave_count, hardware_wave_lane_count);
        if (iterations == 1) {
            // 1 wave is responsible for prefix sums, if execution mode is too wide then excess lanes must not participate
            bool is_participating_lane = wave_lane < group_wave_count;

            uint value = is_participating_lane ? gs_scratch[wave_lane] : 0;
            uint value_prefix_sum = WavePrefixSum(value);

            if (is_participating_lane) gs_scratch[wave_lane] = value_prefix_sum;
        } else {
            // Given that the group count is a power of 2, and the lane count is another power of 2, we don't have to mask threads here
            // as there's not going to be any fraction
            uint last_highest = 0;
            for (uint i = 0; i < iterations; i++) {
                uint index = wave_lane + i * hardware_wave_lane_count;
                uint value = gs_scratch[index];
                uint value_prefix_sum = last_highest + WavePrefixSum(value);

                gs_scratch[index] = value_prefix_sum;

                last_highest = WaveReadLaneAt(value_prefix_sum + value, hardware_wave_lane_count - 1);
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // All waves read back the prefix sum at their respective wave index and add it to their prefix sums to obtain an interwave prefix sum across all
    // waves in the group.
    return intrawave_prefix_sum + gs_scratch[wave_index];
}