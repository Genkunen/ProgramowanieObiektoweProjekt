#include "persistent_settings.hpp"

auto main() -> int {
    using namespace pop::systems;
    PersistentSettings::load();
    PersistentSettings::save();
}

