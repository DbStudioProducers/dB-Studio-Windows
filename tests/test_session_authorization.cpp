#include "../Source/SessionAuthorization.h"
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL: " << #x << " line " << __LINE__ << std::endl; return 1; } } while (false)

int main() {
    using namespace DbStudioSecurity;
    const Session musician{true, "musician-01", Role::Musician, 3};
    const Session master{true, "master-01", Role::Master, -1};
    const Session anonymous{false, "musician-01", Role::Musician, 3};
    const Session noIdentity{true, "", Role::Master, -1};
    const Session noAux{true, "musician-02", Role::Musician, -1};
    const Session invalidRole{true, "invalid-role", static_cast<Role>(99), -1};

    CHECK(authorize(anonymous, {Action::ReadOwnMix, 3}) == Decision::unauthenticated);
    CHECK(authorize(noIdentity, {Action::ViewDevices, -1}) == Decision::invalidSession);
    CHECK(authorize(invalidRole, {Action::ViewDevices, -1}) == Decision::forbidden);
    CHECK(authorize(musician, {Action::ReadOwnMix, 3}) == Decision::allow);
    CHECK(authorize(musician, {Action::WriteOwnMix, 3}) == Decision::allow);
    CHECK(authorize(musician, {Action::ReadOwnMix, 2}) == Decision::wrongAux);
    CHECK(authorize(musician, {Action::WriteOwnMix, 4}) == Decision::wrongAux);
    CHECK(authorize(musician, {Action::ReadOwnMix, 16}) == Decision::invalidRequest);
    CHECK(authorize(noAux, {Action::ReadOwnMix, 0}) == Decision::invalidSession);

    CHECK(authorize(musician, {Action::RenameUser, -1}) == Decision::forbidden);
    CHECK(authorize(musician, {Action::AssignAux, 5}) == Decision::forbidden);
    CHECK(authorize(musician, {Action::ViewDevices, -1}) == Decision::forbidden);
    CHECK(authorize(musician, {Action::SelectTalkbackDestinations, 3}) == Decision::forbidden);
    CHECK(authorize(musician, {Action::TalkbackPtt, 3}) == Decision::forbidden);

    CHECK(authorize(master, {Action::ReadOwnMix, 15}) == Decision::allow);
    CHECK(authorize(master, {Action::WriteOwnMix, 0}) == Decision::allow);
    CHECK(authorize(master, {Action::RenameUser, -1}) == Decision::allow);
    CHECK(authorize(master, {Action::AssignAux, 15}) == Decision::allow);
    CHECK(authorize(master, {Action::AssignAux, -1}) == Decision::invalidRequest);
    CHECK(authorize(master, {Action::ViewDevices, -1}) == Decision::allow);
    CHECK(authorize(master, {Action::SelectTalkbackDestinations, 7}) == Decision::allow);
    CHECK(authorize(master, {Action::TalkbackPtt, 7}) == Decision::allow);
    CHECK(authorize(master, {Action::TalkbackPtt, 16}) == Decision::invalidRequest);
    CHECK(authorize(master, {static_cast<Action>(99), -1}) == Decision::invalidRequest);
    return 0;
}
