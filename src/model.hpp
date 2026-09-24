#pragma once
#include "json.hpp"
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace tracker {
using Json = nlohmann::json;
enum class Truth { no, unknown, yes };
Truth both(Truth a, Truth b);
Truth either(Truth a, Truth b);
std::string escape(std::string_view text);
std::string trim(std::string text);
bool dungeonCheck(const Json& check);
int checkType(const Json& check);
size_t pageCount(size_t rows, size_t size);

// No game pointers or UI handles in the model. Unknown requirements fail closed.
struct Model {
    Json catalogue;
    Json settings = Json::object();
    Json entrances = Json::object();
    std::string seed;
    std::string stage;
    int room = -1;
    int layer = -1;
    std::map<std::string, int> inventory;
    std::set<std::string> countedItems;
    std::set<std::string> obtained;
    std::set<std::string> skipped;
    std::map<std::string, Truth> accessible;
    std::map<std::string, Truth> events;
    std::map<std::string, std::array<Truth, 5>> reached;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::set<std::string>> areaStages;
    std::map<std::string, std::set<std::string>> exteriorCheckStages;
    bool seedLoaded = false;
    bool shuffled = false;
    bool entrancesComplete = false;

    void load(const Json& data);
    void loadExteriorChecks(const Json& positions);
    void loadSeed(const std::string& text);
    void resetSave();
    bool give(const std::string& name);
    bool toggleSkipped(const std::string& name);
    bool inArea(const Json& check) const;
    bool matchesScene(const Json& check) const;
    bool enabled(const Json& check) const;
    int inventoryMaximum(const std::string& name) const;
    int availableDungeonChecks(const std::string& dungeon) const;
    void solve();
    std::string checkDetails(const std::string& name) const;
    Truth evaluate(std::string expression, int form = 0, std::set<std::string> stack = {}) const;
};
}
