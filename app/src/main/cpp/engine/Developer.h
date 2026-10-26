#pragma once
#include <cstdint>
#include <string>

namespace vc {

// Developer access: a name and password unlock the developer menu (modes and
// experiments that normal players do not get). Only a SHA-256 hash of the
// credentials is in the game, never the password itself.
//
// This keeps the menu away from normal players; it is not protection against
// someone who modifies the APK.
std::string sha256Hex(const std::string& data);
bool checkDeveloperLogin(const std::string& name, const std::string& password);

}  // namespace vc
