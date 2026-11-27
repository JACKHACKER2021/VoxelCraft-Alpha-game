#pragma once
#include <cstdint>
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

// A .vcworld file is a zip of the world folder, readable by any zip tool. 2.6.0: the regions (.vcr,
// binary) go in as readable text (.vct, see regionToText) and come back as .vcr on import; older
// files (plain .vcr inside) and the sealed files of 2.6.0 preview build 6 are still read.
bool exportWorld(const std::string& worldFolder, const std::string& file, std::string& error);
bool exportFolderZip(const std::string& folder, const std::string& file, std::string& error);  // 2.8.0: a .vcaddon
// Unpacks a .vcworld made by exportWorld into a new folder under `worldsRoot`.
bool importWorld(const std::string& file, const std::string& worldsRoot, std::string& folder, std::string& error);
// 2.6.0: a region file (.vcr) as readable text and back.
std::string regionToText(const std::vector<uint8_t>& vcr, const std::string& name);
bool textToRegion(const std::string& text, std::vector<uint8_t>& vcr);

}  // namespace vc
