#pragma once
#include "model.hpp"
#include <filesystem>
namespace tracker {
bool normalPlayActive();
bool backupSelectedCard(const std::filesystem::path& destination, std::string& error);
// Read-only lookup of the selected CARD file and slot, never a newest-file guess.
std::filesystem::path selectedSeedFolder(const std::filesystem::path& dataRoot, int slot);
std::filesystem::path seedFolderForSlot(const std::filesystem::path& dataRoot, const std::filesystem::path& sidecar, int slot);
std::string readSeedMetadata(const std::filesystem::path& path);
// Returns false only when valid native data exists but the active seed still needs a log.
bool loadSeedFolder(Model& model, const std::filesystem::path& folder);
void importSelectedLog(Model& model, const std::string& content, const std::filesystem::path& folder);
bool loadSiblingEntrances(Model& model, const std::filesystem::path& logPath, std::string& error);
}
