#include "persistent_settings.hpp"

auto main() -> int {
    using namespace pop::systems;
    SettingNumber nmb{ "aha", 1 };
    SettingNumber nmb2{ "aha2" };
    SettingString nmb3{ "aha3", "aha" };
    SettingString nmb4{ "aha4" };

    PersistentSettings::amend(nmb);
    PersistentSettings::amend(nmb2);
    PersistentSettings::amend(nmb3);
    PersistentSettings::amend(nmb4);
    PersistentSettings::save();
}

