#pragma once
#include <string>
#include <vector>

namespace vc {

class Session;

// Chat commands ("/gamemode creative", "/tp ~ ~10 ~", "/give ruby 5", ...).
// `cheats`: commands that change the game are allowed (Creative worlds, worlds
// with "Povolit příkazy", the developer). Returns the lines to show in the chat,
// already translated.
std::vector<std::string> runCommand(Session& session, const std::string& line, bool cheats);

// Every command with its usage, for /help and the chat hint.
struct CommandInfo {
    const char* usage;
    const char* description;  // Czech key, see LangTable.inc
    bool cheat;
};
extern const CommandInfo kCommands[];
extern const int kCommandCount;

}  // namespace vc
