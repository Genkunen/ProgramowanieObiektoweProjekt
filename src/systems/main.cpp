#include "persistent_settings.hpp"

auto main() -> int {
    using namespace pop::systems;
    SettingNumber nmb{ "aha", 1 };
    SettingNumber nmb2{ "aha" };
    SettingString nmb3{ "aha", "aha" };
    SettingString nmb4{ "aha" };

    PersistentSettings::save(nmb);
    PersistentSettings::save(nmb2);
    PersistentSettings::save(nmb3);
    PersistentSettings::save(nmb4);
}

