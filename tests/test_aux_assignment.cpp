#include "../Source/AuxAssignmentRegistry.h"
#include <cassert>
#include <string>

int main() {
    AuxAssignmentRegistry registry;
    AuxAssignmentRegistry::Assignment found;

    assert(registry.assignedCount() == 0);
    assert(registry.assign("", "Sem ID", 0) == AuxAssignmentRegistry::Result::invalidDeviceId);
    assert(registry.assign("device-a", "Guitarra", 16) == AuxAssignmentRegistry::Result::invalidMix);
    assert(registry.assign("device-a", "Guitarra", 0) == AuxAssignmentRegistry::Result::success);
    assert(registry.assign("device-a", "Outro nome", 1) == AuxAssignmentRegistry::Result::deviceAlreadyAssigned);
    assert(registry.assign("device-b", "Vocal", 0) == AuxAssignmentRegistry::Result::mixAlreadyAssigned);
    assert(registry.assignedCount() == 1);

    assert(registry.renameDevice("device-a", "Guitarra Lead") == AuxAssignmentRegistry::Result::success);
    assert(registry.findByDevice("device-a", found));
    assert(found.displayName == "Guitarra Lead");
    assert(found.mixIndex == 0);
    assert(registry.findByMix(0, found));
    assert(found.deviceId == "device-a");
    assert(!registry.findByMix(1, found));
    assert(!registry.findByMix(16, found));

    for (std::size_t mix = 1; mix < AuxAssignmentRegistry::kMixCount; ++mix) {
        const auto id = "device-" + std::to_string(mix + 1);
        assert(registry.assign(id, id, mix) == AuxAssignmentRegistry::Result::success);
    }
    assert(registry.assignedCount() == 16);
    assert(registry.assign("device-overflow", "Overflow", 0) == AuxAssignmentRegistry::Result::mixAlreadyAssigned);
    assert(registry.unassignDevice("device-a") == AuxAssignmentRegistry::Result::success);
    assert(registry.unassignDevice("device-a") == AuxAssignmentRegistry::Result::deviceNotFound);
    assert(registry.assign("device-new", "Novo músico", 0) == AuxAssignmentRegistry::Result::success);
    assert(registry.assignedCount() == 16);
    assert(registry.findByMix(0, found) && found.deviceId == "device-new");

    return 0;
}
