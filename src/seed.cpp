#include "seed.hpp"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_MemCard.h"
#include <aurora/card.h>
#include <dolphin/dvd.h>
#include <fstream>
#include <chrono>
namespace tracker {
namespace {
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in || in.tellg() < 0 || in.tellg() > 2 * 1024 * 1024) throw std::runtime_error("Seed metadata unavailable");
    std::string text(static_cast<size_t>(in.tellg()), '\0');
    in.seekg(0); if (!in.read(text.data(), text.size())) throw std::runtime_error("Incomplete seed metadata");
    return text;
}
std::string decode(const std::string& value) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; unsigned bits = 0; int count = 0;
    for (char c : value) {
        if (c == '=') break;
        auto pos = alphabet.find(c);
        if (pos == std::string_view::npos) throw std::runtime_error("Invalid saved seed hash");
        bits = (bits << 6) | static_cast<unsigned>(pos); count += 6;
        if (count >= 8) { count -= 8; out += static_cast<char>((bits >> count) & 255); }
    }
    return out;
}
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
        auto content = read(card);
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
bool loadSelectedSeed(Model& model, const std::filesystem::path& dataRoot, std::string& error) {
    try {
        const int slot = dComIfGs_getDataNum();
        if (slot < 0 || slot >= 3) return false;
        const auto type = aurora_card_get_type(0);
        if (type == AURORA_CARD_UNAVAILABLE) return false;
        const auto* disk = DVDGetCurrentDiskID();
        if (!disk) return false;
        const std::string game(disk->gameName, 4), maker(disk->company, 2);
        const char* filename = mDoMemCd_GetFileName();
        if (!filename || !*filename) return false;
        auto safe = [](const std::string& s) { return !s.empty() && s != "." && s != ".." && s.find_first_of("/\\:") == std::string::npos; };
        if (!safe(filename)) return false;
        size_t size = aurora_card_get_path(game.c_str(), type, 0, nullptr, 0);
        if (!size || size > 32768) return false;
        std::string path(size, '\0');
        if (aurora_card_get_path(game.c_str(), type, 0, path.data(), size) != size) return false;
        path.resize(size - 1);
        const std::string key = maker + "-" + game + "-" + filename;
        auto sidecar = type == AURORA_CARD_GCI_DIRECTORY
            ? std::filesystem::u8path(path) / (key + ".mods")
            : std::filesystem::u8path(path + ".mods") / key;
        auto data = Json::parse(read(sidecar / "dev.twilitrealm.randomizer.json"));
        const auto hash = decode(data.at("slots").at(slot).at("blobs").at("seed_hash").get<std::string>());
        if (!safe(hash)) throw std::runtime_error("Invalid seed folder name");
        auto folder = dataRoot / "dev.twilitrealm.randomizer" / "seeds" / hash;
        auto seedData = read(folder / "seed.dat");
        if (!seedData.starts_with("formatVersion: 3")) throw std::runtime_error("Unsupported seed.dat version");
        Model candidate = model;
        // Both logs contain the same settings. Read only that section; item
        // placements must never substitute for the player's actual inventory.
        auto log = folder / (hash + " Anti-Spoiler Log.txt");
        if (!std::filesystem::exists(log)) log = folder / (hash + " Spoiler Log.txt");
        candidate.loadSeed(read(log));
        if (candidate.seed != hash) throw std::runtime_error("Seed log does not match selected save");
        model = std::move(candidate);
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
}
