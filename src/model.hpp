#pragma once
#include "json.hpp"
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

namespace tracker {
using Json = nlohmann::json;
enum class Truth { no, unknown, yes };
Truth both(Truth a, Truth b);
Truth either(Truth a, Truth b);
std::string escape(std::string_view text);
std::string trim(std::string text);
bool dungeonCheck(const Json& check);
bool passesHintSignFilter(const Json& check, bool showHintSigns);
int checkType(const Json& check);
size_t pageCount(size_t rows, size_t size);

struct DungeonCheckCounts { int accessible=0, remaining=0; };

// Transfer the entire solved state together without replacing live save data.
struct LogicResult {
    std::map<std::string, Truth> accessible, events;
    std::map<std::string, std::array<Truth, 5>> reached;
};

// No game pointers or UI handles in the model. Unknown requirements fail closed.
struct Model {
    Json catalogue;
    Json settings = Json::object();
    Json entrances = Json::object();
    std::map<std::string,std::string> itemPlacements, entranceTargets;
    std::map<std::string,int> startingItems;
    std::map<std::string,std::set<std::string>> entranceCandidates;
    std::set<std::string> shuffledEdges, visitedEntrances;
    std::string seed;
    std::string stage;
    int room = -1;
    int layer = -1;
    std::map<std::string, int> inventory;
    std::set<std::string> countedItems;
    std::set<std::string> obtained;
    std::set<std::string> skipped;
    std::set<std::string> mapHidden; // Milestone markers, independent of reward completion.
    std::map<std::string, Truth> accessible;
    std::map<std::string, Truth> events;
    std::map<std::string, std::array<Truth, 5>> reached;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::set<std::string>> areaStages;
    std::map<std::string, std::set<std::string>> exteriorCheckStages;
    bool seedLoaded = false;
    bool vanilla = false;
    bool shuffled = false;
    bool entrancesComplete = false;

    void load(const Json& data);
    void loadExteriorChecks(const Json& positions);
    void loadSeed(const std::string& text);
    void loadVanilla();
    void applySeedData(const std::string& text);
    void loadNativeEntrances(const std::string& text);
    void resetSave();
    bool give(const std::string& name);
    bool toggleSkipped(const std::string& name);
    bool inArea(const Json& check) const;
    bool matchesScene(const Json& check) const;
    bool enabled(const Json& check) const;
    int inventoryMaximum(const std::string& name) const;
    std::string dungeonForStage(const std::string& stageName) const;
    DungeonCheckCounts dungeonCheckCounts(const std::string& dungeon, const bool* visibleTypes = nullptr) const;
    int availableDungeonChecks(const std::string& dungeon, const bool* visibleTypes = nullptr) const;
    void solve();
    LogicResult takeLogicResult() { return {std::move(accessible),std::move(events),std::move(reached)}; }
    void applyLogicResult(LogicResult result) {
        accessible=std::move(result.accessible); events=std::move(result.events); reached=std::move(result.reached);
    }
    std::string checkDetails(const std::string& name, bool advanced = true) const;
    std::string itemHistory(const std::string& name) const;
    std::string entranceDestination(const std::string& parent, const std::string& vanilla) const;
    Truth evaluate(std::string expression, int form = 0, std::set<std::string> stack = {}) const;
};
}
