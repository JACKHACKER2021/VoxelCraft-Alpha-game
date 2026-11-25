#pragma once
#include <string>
#include <vector>

namespace vc {

// 1.4.0: automatic backups of a world, and a world as one file (.vcworld) to share or keep.
//
// Backups live in <backupsRoot>/<world folder name>/<YYYYMMDD-HHMMSS>/, a full copy of the
// world; only the newest `keep` are kept.
bool backupWorld(const std::string& worldFolder, const std::string& backupsRoot, int keep, std::string* made = nullptr);
std::vector<std::string> listBackups(const std::string& worldFolder, const std::string& backupsRoot);  // newest first
// Puts a backup back in place of the world (the current state becomes a backup first).
bool restoreBackup(const std::string& worldFolder, const std::string& backupDir, const std::string& backupsRoot);

// A .vcworld file: 2.6.0 one sealed file ("VCWORLD2", a readable label, the zip of the world folder
// compressed); before 2.6.0 the plain zip, which importWorld still reads.
bool exportWorld(const std::string& worldFolder, const std::string& file, std::string& error);
// Unpacks a .vcworld made by exportWorld into a new folder under `worldsRoot`.
bool importWorld(const std::string& file, const std::string& worldsRoot, std::string& folder, std::string& error);

}  // namespace vc
