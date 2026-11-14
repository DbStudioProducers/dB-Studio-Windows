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

enum class Decision { allow, unauthenticated, invalidSession, invalidRequest, forbidden, wrongAux };

constexpr bool isValidAux(int aux) noexcept {
    return aux >= 0 && aux < 16;
}

inline Decision authorize(const Session& session, const Request& request) noexcept {
    if (!session.authenticated) return Decision::unauthenticated;
    if (session.stableUserId.empty()) return Decision::invalidSession;

    switch (session.role) {
        case Role::Master:
            switch (request.action) {
                case Action::ReadOwnMix:
                case Action::WriteOwnMix:
                case Action::AssignAux:
                case Action::SelectTalkbackDestinations:
                case Action::TalkbackPtt:
                    return isValidAux(request.targetAux) ? Decision::allow : Decision::invalidRequest;
                case Action::RenameUser:
                case Action::ViewDevices:
                    return Decision::allow;
            }
            return Decision::invalidRequest;

        case Role::Musician:
            switch (request.action) {
                case Action::ReadOwnMix:
                case Action::WriteOwnMix:
                    if (!isValidAux(session.assignedAux)) return Decision::invalidSession;
                    if (!isValidAux(request.targetAux)) return Decision::invalidRequest;
                    return request.targetAux == session.assignedAux
                        ? Decision::allow : Decision::wrongAux;
                case Action::RenameUser:
                case Action::AssignAux:
                case Action::ViewDevices:
                case Action::SelectTalkbackDestinations:
                case Action::TalkbackPtt:
                    return Decision::forbidden;
            }
            return Decision::invalidRequest;
    }
    return Decision::forbidden;
}

} // namespace DbStudioSecurity
