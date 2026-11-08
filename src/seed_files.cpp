#include "seed.hpp"
#include <fstream>

namespace tracker {
namespace fs = std::filesystem;
namespace {
std::string decodeHash(const std::string& value) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    unsigned bits = 0;
    int count = 0;
    for (char c : value) {
        if (c == '=') break;
        auto pos = alphabet.find(c);
        if (pos == std::string_view::npos) throw std::runtime_error("Invalid saved seed hash");
        bits = (bits << 6) | static_cast<unsigned>(pos);
        count += 6;
        if (count >= 8) {
            count -= 8;
            out += static_cast<char>((bits >> count) & 255);
        }
    }
    if (out.empty() || out == "." || out == ".." || out.find_first_of("/\\:") != std::string::npos ||
        out.find('\0') != std::string::npos) throw std::runtime_error("Invalid seed folder name");
    return out;
}

std::string readNativeSeed(const fs::path& folder) {
    auto text = readSeedMetadata(folder / "seed.dat");
    if (trim(text.substr(0, text.find('\n'))) != "formatVersion: 3")
        throw std::runtime_error("Unsupported seed.dat version");
    return text;
}

void applyLog(Model& model, const std::string& text, const std::string& expected, const std::string& native) {
    Model candidate = model;
    candidate.loadSeed(text);
    if (!expected.empty() && candidate.seed != expected)
        throw std::runtime_error("This log is for " + candidate.seed + ". Choose the log for the active seed, " + expected + ".");
    if (candidate.shuffled && !native.empty()) candidate.loadNativeEntrances(native);
    model = std::move(candidate);
}
}

std::string readSeedMetadata(const fs::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in || in.tellg() < 0 || in.tellg() > 2 * 1024 * 1024)
        throw std::runtime_error("Seed metadata unavailable: " + path.generic_string());
    std::string text(static_cast<size_t>(in.tellg()), '\0');
    in.seekg(0);
    if (!in.read(text.data(), text.size())) throw std::runtime_error("Incomplete seed metadata: " + path.generic_string());
    return text;
}

fs::path seedFolderForSlot(const fs::path& dataRoot, const fs::path& sidecar, int slot) {
    const auto data = Json::parse(readSeedMetadata(sidecar));
    const auto hash = decodeHash(data.at("slots").at(slot).at("blobs").at("seed_hash").get<std::string>());
    return dataRoot / "dev.twilitrealm.randomizer" / "seeds" / fs::u8path(hash);
}

bool loadSeedFolder(Model& model, const fs::path& folder) {
    const auto native = readNativeSeed(folder);
    const auto hash = folder.filename().string();
    for (const auto* suffix : {" Spoiler Log.txt", " Anti-Spoiler Log.txt"}) {
        const auto log = folder / (hash + suffix);
        if (fs::exists(log)) {
            applyLog(model, readSeedMetadata(log), hash, native);
            return true;
        }
    }
    // A log chosen outside the seed folder is already saved as per-slot settings.
    // Restore its shuffled routes from the host's native file on subsequent loads.
    if (!model.seedLoaded || model.seed != hash) return false;
    if (model.shuffled) model.loadNativeEntrances(native);
    return true;
}

void importSelectedLog(Model& model, const std::string& content, const fs::path& folder) {
    const auto expected = folder.empty() ? model.seed : folder.filename().string();
    const auto native = !folder.empty() && fs::exists(folder / "seed.dat") ? readNativeSeed(folder) : std::string{};
    applyLog(model, content, expected, native);
}

bool loadSiblingEntrances(Model& model, const fs::path& logPath, std::string& error) {
    error.clear();
    if (!model.shuffled || logPath.parent_path().filename() != fs::u8path(model.seed)) return false;
    try {
        if (!fs::exists(logPath.parent_path() / "seed.dat")) return false;
        model.loadNativeEntrances(readNativeSeed(logPath.parent_path()));
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
}
