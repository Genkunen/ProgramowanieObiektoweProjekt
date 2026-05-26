#include "../shared_consts.hpp"

struct SimulationState {
    uint* object_flags;

    /// Attempts to make an object dead.
    ///
    /// Returns true if the object was not dead before and the thread calling this function won the race with other threads.
    /// Returns false if the object was already dead, or if it was not dead but another thread won the race.
    bool try_set_object_dead(uint object_id) {
        uint old_flags;

        InterlockedOr(object_flags[object_id], pop::shader_consts::SIM_OBJECT_FLAG_DEAD, old_flags);

        return (old_flags & pop::shader_consts::SIM_OBJECT_FLAG_DEAD) == 0;
    }
};
