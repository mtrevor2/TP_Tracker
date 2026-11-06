#include "seed.hpp"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_MemCard.h"
#include <aurora/card.h>
#include <dolphin/dvd.h>
#include <chrono>
namespace tracker {
bool normalPlayActive() {
    const char* name=mDoMemCd_GetFileName();
    return name && std::string_view(name)=="gczelda2";
}
bool backupSelectedCard(const std::filesystem::path& destination, std::string& error) {
    try {
        // Copy only; never open the live card for writing or delete older backups.
        const auto type = aurora_card_get_type(0);
        if (type != AURORA_CARD_GCI_DIRECTORY) return false;
        const auto* disk = DVDGetCurrentDiskID();
        const char* filename = mDoMemCd_GetFileName();
        if (!disk || !filename || !*filename || std::string(filename).find_first_of("/\\:") != std::string::npos) return false;
        const std::string game(disk->gameName,4), maker(disk->company,2);
        size_t size = aurora_card_get_path(game.c_str(),type,0,nullptr,0);
        if (!size || size > 32768) return false;
        std::string path(size,'\0');
        if (aurora_card_get_path(game.c_str(),type,0,path.data(),size) != size) return false;
        path.resize(size-1);
        const auto key = maker + "-" + game + "-" + filename;
        auto card = std::filesystem::u8path(path) / (key + ".gci");
        if (!std::filesystem::exists(card)) return false;
        auto content = readSeedMetadata(card);
        if (content.size() != 0x8040) throw std::runtime_error("Unexpected card size; backup skipped");
        static std::string previous;
        if (previous == content) return true;
        const auto tick = std::chrono::system_clock::now().time_since_epoch().count();
        auto folder = destination / "card-backups" / std::to_string(tick);
        std::filesystem::create_directories(folder);
        // copy_file refuses to overwrite, even if timestamps happen to collide.
        std::filesystem::copy_file(card, folder / card.filename());
        auto sidecar = std::filesystem::u8path(path) / (key + ".mods");
        if (std::filesystem::exists(sidecar))
            std::filesystem::copy(sidecar,folder / sidecar.filename(),std::filesystem::copy_options::recursive);
        previous = std::move(content);
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
std::filesystem::path selectedSeedFolder(const std::filesystem::path& dataRoot, int slot) {
    if (slot < 0 || slot >= 3) return {};
    const auto type = aurora_card_get_type(0);
    const auto* disk = DVDGetCurrentDiskID();
    const char* filename = mDoMemCd_GetFileName();
    if (type == AURORA_CARD_UNAVAILABLE || !disk || !filename || !*filename ||
        std::string_view(filename).find_first_of("/\\:") != std::string_view::npos) return {};
    const std::string game(disk->gameName, 4), maker(disk->company, 2);
    size_t size = aurora_card_get_path(game.c_str(), type, 0, nullptr, 0);
    if (!size || size > 32768) return {};
    std::string path(size, '\0');
    if (aurora_card_get_path(game.c_str(), type, 0, path.data(), size) != size) return {};
    path.resize(size - 1);
    const std::string key = maker + "-" + game + "-" + filename;
    auto sidecar = type == AURORA_CARD_GCI_DIRECTORY
        ? std::filesystem::u8path(path) / (key + ".mods")
        : std::filesystem::u8path(path + ".mods") / key;
    return seedFolderForSlot(dataRoot, sidecar / "dev.twilitrealm.randomizer.json", slot);
}
}
