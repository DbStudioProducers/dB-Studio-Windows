#include "../Source/AuxAssignmentRegistry.h"
#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL: " << #condition << " at line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while (false)

int main() {
    AuxAssignmentRegistry registry;
    AuxAssignmentRegistry::Assignment found;

    CHECK(registry.assignedCount() == 0);
    CHECK(registry.assign("", "Sem ID", 0) == AuxAssignmentRegistry::Result::invalidDeviceId);
    CHECK(registry.assign("device-a", "Guitarra", 16) == AuxAssignmentRegistry::Result::invalidMix);
    CHECK(registry.assign("device-a", "Guitarra", 0) == AuxAssignmentRegistry::Result::success);
    CHECK(registry.assign("device-a", "Outro nome", 1) == AuxAssignmentRegistry::Result::deviceAlreadyAssigned);
    CHECK(registry.assign("device-b", "Vocal", 0) == AuxAssignmentRegistry::Result::mixAlreadyAssigned);
    CHECK(registry.assignedCount() == 1);

    CHECK(registry.renameDevice("device-a", "Guitarra Lead") == AuxAssignmentRegistry::Result::success);
    CHECK(registry.findByDevice("device-a", found));
    CHECK(found.displayName == "Guitarra Lead");
    CHECK(found.mixIndex == 0);
    CHECK(registry.findByMix(0, found));
    CHECK(found.deviceId == "device-a");
    CHECK(!registry.findByMix(1, found));
    CHECK(!registry.findByMix(16, found));

    for (std::size_t mix = 1; mix < AuxAssignmentRegistry::kMixCount; ++mix) {
        const auto id = "device-" + std::to_string(mix + 1);
        CHECK(registry.assign(id, id, mix) == AuxAssignmentRegistry::Result::success);
    }
    CHECK(registry.assignedCount() == 16);
    CHECK(registry.assign("device-overflow", "Overflow", 0) == AuxAssignmentRegistry::Result::mixAlreadyAssigned);
    CHECK(registry.unassignDevice("device-a") == AuxAssignmentRegistry::Result::success);
    CHECK(registry.unassignDevice("device-a") == AuxAssignmentRegistry::Result::deviceNotFound);
    CHECK(registry.assign("device-new", "Novo músico", 0) == AuxAssignmentRegistry::Result::success);
    CHECK(registry.assignedCount() == 16);
    CHECK(registry.findByMix(0, found) && found.deviceId == "device-new");

    const std::string path = "dbstudio_aux_registry_teste_á.dbsaux";
    CHECK(registry.saveToFile(path) == AuxAssignmentRegistry::Result::success);
    CHECK(registry.saveToFile("diretorio_inexistente/subpasta/nao_pode_salvar.dbsaux") == AuxAssignmentRegistry::Result::persistenceError);
    AuxAssignmentRegistry restored;
    CHECK(restored.loadFromFile(path) == AuxAssignmentRegistry::Result::success);
    CHECK(restored.assignedCount() == 16);
    CHECK(restored.findByDevice("device-new", found));
    CHECK(found.displayName == "Novo músico");
    CHECK(found.mixIndex == 0);
    { std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
      corrupt << "DBSAUX1\n0\t4141\t4262\n0\t4343\t4464\n"; }
    CHECK(restored.loadFromFile(path) == AuxAssignmentRegistry::Result::invalidData);
    CHECK(restored.assignedCount() == 16);
    CHECK(restored.findByDevice("device-new", found));
    std::remove(path.c_str());
    return 0;
}
