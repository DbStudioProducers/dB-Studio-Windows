#pragma once
#include <string>

namespace DbStudioSecurity {

// This is an authorization policy for an already authenticated session.
// It does NOT authenticate users, validate passwords, or provide encryption.
enum class Role { Master, Musician };
enum class Action {
    ReadOwnMix,
    WriteOwnMix,
    RenameUser,
    AssignAux,
    ViewDevices,
    SelectTalkbackDestinations,
    TalkbackPtt
};

struct Session {
    bool authenticated = false;
    std::string stableUserId;
    Role role = Role::Musician;
    int assignedAux = -1; // Required for musicians; ignored for Master.
};

struct Request {
    Action action = Action::ReadOwnMix;
    int targetAux = -1;
};

enum class Decision { allow, unauthenticated, invalidSession, forbidden, wrongAux };

inline Decision authorize(const Session& session, const Request& request) noexcept {
    if (!session.authenticated) return Decision::unauthenticated;
    if (session.stableUserId.empty()) return Decision::invalidSession;

    if (session.role == Role::Master) return Decision::allow;
    if (session.role != Role::Musician) return Decision::forbidden;

    switch (request.action) {
        case Action::ReadOwnMix:
        case Action::WriteOwnMix:
            if (session.assignedAux < 0 || session.assignedAux >= 16)
                return Decision::invalidSession;
            return request.targetAux == session.assignedAux
                ? Decision::allow : Decision::wrongAux;
        case Action::RenameUser:
        case Action::AssignAux:
        case Action::ViewDevices:
        case Action::SelectTalkbackDestinations:
        case Action::TalkbackPtt:
            return Decision::forbidden;
    }
    return Decision::forbidden;
}

} // namespace DbStudioSecurity
