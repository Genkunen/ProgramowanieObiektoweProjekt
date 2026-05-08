#include "../shared_consts.hpp"

struct PushConstants {
    uint* sort_keys;
    uint* sort_values;
    uint bitonic_sort_stage;
    uint bitonic_sort_step;
    uint keys_count;
};

[vk::push_constant] PushConstants push_constants;

[shader("compute")]
[numthreads(pop::shader_consts::CS_SIMULATION_ACCELERATION_GRID_BITONIC_SORT_GROUP_SIZE_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint i = dtid.x;
    if (i >= push_constants.keys_count) return;

    uint i_xor_j = i ^ push_constants.bitonic_sort_step;
    if (i_xor_j >= push_constants.keys_count) return;

    if (i_xor_j > i) {
        bool is_ascending = (i & push_constants.bitonic_sort_stage) == 0;

        if ((push_constants.sort_keys[i] > push_constants.sort_keys[i_xor_j]) == is_ascending) {
            uint tmp_key = push_constants.sort_keys[i];
            uint tmp_value = push_constants.sort_values[i];

            push_constants.sort_keys[i] = push_constants.sort_keys[i_xor_j];
            push_constants.sort_values[i] = push_constants.sort_values[i_xor_j];

            push_constants.sort_keys[i_xor_j] = tmp_key;
            push_constants.sort_values[i_xor_j] = tmp_value;
        }
    }
}