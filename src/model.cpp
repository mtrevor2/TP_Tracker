#include "model.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>
#include <stdexcept>

namespace tracker {
Truth both(Truth a, Truth b) { return std::min(a, b); }
Truth either(Truth a, Truth b) { return std::max(a, b); }
static Truth truth(bool value) { return value ? Truth::yes : Truth::no; }
std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
static std::string words(std::string s) { std::replace(s.begin(), s.end(), '_', ' '); return trim(s); }
std::string escape(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += c;
        }
    }
    return out;
}
static std::string scalar(const Json& value) { return value.is_string() ? value.get<std::string>() : value.dump(); }
std::string Model::checkDetails(const std::string& name) const {
    const Json* selected = nullptr;
    for (const auto& check : catalogue.at("checks")) if (check.at("name") == name) { selected = &check; break; }
    if (!selected) return "<div>Check no longer available.</div>";
    auto label = [](Truth value) { return value == Truth::yes ? "OPEN" : value == Truth::no ? "LOCKED" : "UNKNOWN"; };
    const auto status = accessible.find(name);
    std::string state = obtained.contains(name) ? "DONE" : skipped.contains(name) ? "SKIPPED" : status == accessible.end() ? "UNKNOWN" : label(status->second);
    std::string out = "<div class='tp-title'>" + escape(name) + "</div><div>" + state + " · " + escape(selected->value("group", "Other")) + "</div>";
    out += "<div class='tp-note'>Reach one of the areas below and meet its requirements. 'and' means both; 'or' means either.</div>";
    std::string expressions;
    for (const auto& route : selected->at("access")) {
        const std::string area = route.at("area"), requirement = route.at("requirement");
        auto found = reached.find(area);
        Truth reach = Truth::no, result = Truth::no;
        if (found == reached.end()) reach = result = Truth::unknown;
        else for (int f=0; f<5; ++f) { reach=either(reach,found->second[f]); result=either(result,both(found->second[f],evaluate(requirement,f))); }
        out += "<div class='tp-group'>" + escape(area) + " — " + label(result) + "</div><div>Area access: " + label(reach) + "</div><div>Local requirement: " + (requirement=="Nothing" ? std::string("No additional items required.") : escape(words(requirement))) + "</div>";
        expressions += " " + words(requirement);
        if (reach != Truth::yes) out += "<div class='tp-note'>" + std::string(reach==Truth::no ? "You cannot currently reach this area with the tracked inventory and seed settings." : "The tracker cannot yet confirm access to this area.") + "</div>";
    }
    if (selected->at("access").empty()) out += "<div>Requirements are not available for this check.</div>";
    // Expand named logic helpers without substituting away AND/OR grouping.
    std::set<std::string> shown;
    for (int pass=0; pass<8 && shown.size()<48; ++pass) {
        bool added=false;
        for (const auto& [macro,value] : catalogue.at("macros").items()) {
            if (shown.size()>=48 || shown.contains(macro) || expressions.find(words(macro))==std::string::npos) continue;
            shown.insert(macro); added=true;
            const auto definition=words(scalar(value));
            out += "<div class='tp-group'>" + escape(words(macro)) + "</div><div>" + escape(definition) + "</div>";
            expressions += " " + definition;
        }
        if (!added) break;
    }
    out += "<div class='tp-group'>Relevant seed settings and inventory</div>";
    for (const auto& [key,value] : settings.items()) if (expressions.find(key)!=std::string::npos)
        out += "<div>"+escape(key)+": "+escape(scalar(value))+"</div>";
    for (const auto& [key,value] : inventory) if (expressions.find(key)!=std::string::npos)
        out += "<div>"+escape(key)+": "+(value<0?std::string("unknown"):std::to_string(value))+"</div>";
    if (settings.value("Logic Rules",std::string())=="No Logic") out += "<div>No Logic is enabled: the tracker does not lock checks by these requirements.</div>";
    if (shuffled) out += "<div>Entrance shuffle is enabled. These routes describe vanilla connections; shuffled routes are not evaluated.</div>";
    return out;
}
void Model::load(const Json& data) {
    if (data.at("version") != 1 || !data.at("checks").is_array() || data.at("checks").size() > 10000)
        throw std::runtime_error("Unsupported tracker catalogue");
    std::set<std::string> names;
    std::map<std::string, std::string> newAliases;
    std::map<std::string, std::set<std::string>> newAreaStages;
    for (const auto& check : data.at("checks")) {
        auto name = check.at("name").get<std::string>();
        if (name.empty() || !names.insert(name).second) throw std::runtime_error("Duplicate check name");
        newAliases[name] = name;
        for (const auto& alias : check.at("aliases")) newAliases[alias.get<std::string>()] = name;
        for (const auto& flag : check.at("flags")) {
            auto kind = flag.at("kind").get<std::string>();
            int id = flag.at("flag").get<int>();
            int save = flag.value("save", -1);
            if (id < 0 || id > 65535 || (kind != "event" && (save < 0 || save >= 32 || id > 255)))
                throw std::runtime_error("Invalid save flag");
            if (kind != "event" && kind != "chest" && kind != "switch" && kind != "item")
                throw std::runtime_error("Unknown save flag kind");
        }
        for (const auto& access : check.at("access"))
            for (const auto& stageName : check.at("stages"))
                newAreaStages[access.at("area").get<std::string>()].insert(stageName.get<std::string>());
    }
    for (const auto& item : data.at("items")) {
        int id = item.at("Id").get<int>();
        if (id < 0 || id > 255) throw std::runtime_error("Invalid item ID");
    }
    if (!data.at("macros").is_object() || !data.at("areas").is_array())
        throw std::runtime_error("Missing world logic");
    catalogue = data;
    // YAML's unquoted "None"/empty categories can export null. Presentation
    // fields are optional and must never disable the mod at tab-build time.
    for (auto& check : catalogue["checks"]) {
        if (!check.contains("group") || !check["group"].is_string() || check["group"].get<std::string>().empty()) check["group"] = "Other";
        if (!check.contains("original_item") || !check["original_item"].is_string()) check["original_item"] = "";
    }
    aliases = std::move(newAliases);
    areaStages = std::move(newAreaStages);
    exteriorCheckStages.clear();
}
void Model::loadSeed(const std::string& text) {
    Json nextSettings = Json::object(), nextEntrances = Json::object();
    std::string nextSeed;
    bool complete = false;
    auto content = trim(text);
    if (!content.empty() && content.front() == '{') {
        auto parsed = Json::parse(content);
        if (parsed.value("version", 1) != 1) throw std::runtime_error("Unsupported seed JSON version");
        nextSettings = parsed.at("settings");
        nextEntrances = parsed.value("entrances", Json::object());
        nextSeed = parsed.at("seed").get<std::string>();
        complete = parsed.value("entrances_complete", false);
    } else {
        // Dusklight anti-spoiler logs contain a flat scalar Settings section.
        // Deliberately do not interpret item placements or arbitrary YAML tags.
        bool inSettings = false;
        std::istringstream input(text);
        std::string line;
        while (std::getline(input, line)) {
            if (line.starts_with("Hash: ")) nextSeed = trim(line.substr(6));
            if (trim(line) == "# Settings") { inSettings = true; continue; }
            if (inSettings && line.starts_with("# ")) break;
            if (!inSettings || line.empty() || line[0] == ' ' || line[0] == '#' || line[0] == '-') continue;
            auto colon = line.find(": ");
            if (colon == std::string::npos) continue;
            auto value = trim(line.substr(colon + 2));
            if (value.size() > 1 && value.front() == '"' && value.back() == '"')
                value = Json::parse(value).get<std::string>();
            nextSettings[trim(line.substr(0, colon))] = value;
        }
    }
    if (nextSeed.empty() || !nextSettings.is_object() || !nextSettings.contains("Logic Rules") || !nextEntrances.is_object())
        throw std::runtime_error("Choose a Dusklight seed log or TPTracker seed JSON");
    for (const auto& [key, value] : nextSettings.items())
        if (!value.is_primitive() || value.is_null()) throw std::runtime_error("Settings must be scalars");
    std::set<std::string> areaNames;
    for (const auto& area : catalogue.at("areas")) areaNames.insert(area.at("Name").get<std::string>());
    std::set<std::string> exitNames;
    for (const auto& area : catalogue.at("areas"))
        if (area.contains("Exits") && area.at("Exits").is_object())
            for (const auto& [exit, req] : area.at("Exits").items()) exitNames.insert(area.at("Name").get<std::string>() + " -> " + exit);
    for (const auto& [key, value] : nextEntrances.items())
        if (!exitNames.contains(key) || !value.is_string() || !areaNames.contains(value.get<std::string>()))
            throw std::runtime_error("Entrance destination is not a known logical area");
    seed = nextSeed;
    settings = std::move(nextSettings);
    entrances = std::move(nextEntrances);
    seedLoaded = true;
    entrancesComplete = complete && !entrances.empty();
    shuffled = false;
    for (const auto& [key, value] : settings.items())
        if (key.starts_with("Randomize ") && (key.find("Entrances") != std::string::npos || key == "Randomize Starting Spawn") && scalar(value) != "Off")
            shuffled = true;
}
void Model::resetSave() {
    skipped.clear();
    inventory.clear(); countedItems.clear(); obtained.clear(); accessible.clear(); events.clear(); reached.clear();
    seed.clear(); settings = Json::object(); entrances = Json::object(); seedLoaded = false; shuffled = false; entrancesComplete = false;
    stage.clear(); room = -1; layer = -1;
}
bool Model::give(const std::string& name) {
    auto found = aliases.find(name);
    if (found!=aliases.end()) skipped.erase(found->second);
    return found != aliases.end() && obtained.insert(found->second).second;
}
bool Model::toggleSkipped(const std::string& name) {
    if (!aliases.contains(name) || obtained.contains(name)) return false;
    if (!skipped.erase(name)) skipped.insert(name);
    return true;
}
void Model::loadExteriorChecks(const Json& positions) {
    exteriorCheckStages.clear();
    for (const auto& check : catalogue.at("checks")) {
        if (dungeonCheck(check)) continue;
        const std::string name=check.at("name");
        if (!positions.contains(name)) continue;
        for (const auto& point : positions.at(name)) {
            if (!point.contains("overworld") || !point.at("overworld").is_object()) continue;
            const auto stageName=point.at("overworld").value("stage",std::string{});
            if (stageName.starts_with("F_")) exteriorCheckStages[name].insert(stageName);
        }
    }
}
bool Model::matchesScene(const Json& check) const {
    if (!stage.starts_with("D_SB") || !check.contains("grotto_scenes") || check.at("grotto_scenes").empty()) return true;
    for (const auto& scene : check.at("grotto_scenes"))
        if (scene.at("stage") == stage && scene.at("room") == room && scene.at("layer") == layer) return true;
    return false;
}
bool Model::inArea(const Json& check) const {
    if (!matchesScene(check)) return false;
    const auto exterior=exteriorCheckStages.find(check.at("name").get<std::string>());
    if (exterior!=exteriorCheckStages.end() && exterior->second.contains(stage)) return true;
    for (const auto& s : check.at("stages")) if (s == stage) return true;
    // Only infer a stage for checks without explicit metadata, and only from an unambiguous area.
    if (check.at("stages").empty()) for (const auto& a : check.at("access")) {
        auto it = areaStages.find(a.at("area").get<std::string>());
        if (it != areaStages.end() && it->second.size() == 1 && it->second.contains(stage)) return true;
    }
    return false;
}
bool Model::enabled(const Json& check) const {
    auto has = [&](const char* cat) { const auto& cats = check.at("categories"); return std::find(cats.begin(), cats.end(), cat) != cats.end(); };
    auto off = [&](const char* key) { return settings.contains(key) && scalar(settings.at(key)) == "Off"; };
    if (has("Twilit Insect")) {
        const auto item = check.value("original_item", std::string{});
        for (const char* province : {"Faron", "Eldin", "Lanayru"}) {
            if (item == std::string(province) + " Twilight Tear" &&
                (settings.value(std::string(province) + " Twilight Cleared", Json("Off")) == "On" ||
                 settings.value(std::string(province) + " Twilight Area Cleared", Json("Off")) == "On")) return false;
        }
    }
    // Optional rupee locations exist in the tracker only when explicitly shuffled.
    if (has("Rupee - Hidden") && settings.value("Hidden Rupees", Json("Off")) != "On") return false;
    if (has("Rupee - Freestanding") && settings.value("Freestanding Rupees", Json("Off")) != "On") return false;
    for (const auto& [cat, setting] : std::initializer_list<std::pair<const char*, const char*>>{
             {"Sky Character", "Sky Characters"}, {"Npc", "Gifts From NPCs"},
             {"Shop", "Shop Items"}, {"Golden Wolf", "Hidden Skills"}})
        if (has(cat) && off(setting)) return false;
    // Unshuffled bugs and souls still exist and are useful collectible checks.
    return true;
}

bool dungeonCheck(const Json& check) {
    const auto& cats=check.at("categories");
    return std::find(cats.begin(),cats.end(),"Dungeon")!=cats.end();
}
int Model::availableDungeonChecks(const std::string& dungeon, const bool* visibleTypes) const {
    int count=0;
    for (const auto& check : catalogue.at("checks")) {
        const auto& categories=check.at("categories");
        auto has=[&](const char* category) { return std::find(categories.begin(),categories.end(),category)!=categories.end(); };
        if (!dungeonCheck(check) || std::find(categories.begin(),categories.end(),dungeon)==categories.end() ||
            has("Non-Item Location") || has("Hint Sign") || !enabled(check) ||
            (visibleTypes && !visibleTypes[checkType(check)])) continue;
        const std::string name=check.at("name");
        const auto access=accessible.find(name);
        if (!obtained.contains(name) && !skipped.contains(name) && access!=accessible.end() && access->second==Truth::yes) ++count;
    }
    return count;
}
int checkType(const Json& check) {
    const auto& cats=check.at("categories");
    auto has=[&](const char* c){return std::find(cats.begin(),cats.end(),c)!=cats.end();};
    const std::string name=check.at("name");
    if (has("Rupee - Freestanding")) return 8;
    if (has("Rupee - Hidden")) return 9;
    if (has("Chest") && name.find("Grotto")!=std::string::npos) return 7;
    if (has("Sky Character")) return 6;
    if (has("Golden Bug")) return 2;
    if (has("Poe")) return 3;
    if (has("Golden Wolf")) return 4;
    if (has("Shop")) return 5;
    if (has("Chest")) return 0;
    return 1;
}
size_t pageCount(size_t rows,size_t size) { return std::max<size_t>(1,(rows+size-1)/size); }
int Model::inventoryMaximum(const std::string& name) const {
    if (name=="Empty Bottle") return 4;
    if (name=="Heart Count") return 20;
    if (name.ends_with("Twilight Tear")) return 16;
    if (name.starts_with("Progressive ") || name.ends_with(" Small Key") || name=="Poe Soul" || name=="Bomb Bag" || name=="Goron Mines Key Shard") {
        int total=0;
        for (const auto& check:catalogue.at("checks")) total+=check.value("original_item",std::string{})==name;
        return std::max(1,total);
    }
    return 1;
}

Truth Model::evaluate(std::string expression, int form, std::set<std::string> stack) const {
    auto s = trim(expression);
    if (s.empty() || stack.size() > 64) return Truth::unknown;
    // Split only at the outer nesting level. OR has lower precedence than AND.
    for (const std::string op : {" or ", " and "}) {
        int depth = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '(') ++depth;
            if (s[i] == ')') --depth;
            if (depth < 0) return Truth::unknown;
            if (depth == 0 && s.compare(i, op.size(), op) == 0) {
                auto a = evaluate(s.substr(0, i), form, stack);
                auto b = evaluate(s.substr(i + op.size()), form, stack);
                return op == " or " ? either(a, b) : both(a, b);
            }
        }
        if (depth != 0) return Truth::unknown;
    }
    if (s.front() == '(' && s.back() == ')') return evaluate(s.substr(1, s.size() - 2), form, stack);
    s = words(s);
    if (s == "Nothing" || s == "True") return Truth::yes;
    if (s == "Impossible" || s == "False") return Truth::no;
    if (s == "Human Link") return truth(form < 2);
    if (s == "Wolf Link") return truth(form == 2 || form == 3);
    if (s == "Day") return truth(form == 0 || form == 2);
    if (s == "Night") return truth(form == 1 || form == 3);
    if (s == "Twilight") return truth(form == 4);
    if (s.front() == '\'' && s.back() == '\'') {
        auto it = events.find(s.substr(1, s.size() - 2));
        return it == events.end() ? Truth::unknown : it->second;
    }
    auto countOf = [&](const std::string& name) -> int {
        auto it = inventory.find(name); return it == inventory.end() ? -1 : it->second;
    };
    auto number = [&](std::string value) -> int {
        value = trim(value);
        if (settings.contains(value)) value = scalar(settings.at(value));
        int n = -1;
        auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), n);
        return ec == std::errc{} && end == value.data() + value.size() ? n : -1;
    };
    if (s.starts_with("hearts(") && s.back() == ')') {
        int required = number(s.substr(7,s.size()-8)), count = countOf("Heart Count");
        return required < 0 || count < 0 ? Truth::unknown : truth(count >= required);
    }
    if (s.starts_with("dungeons completed(") && s.back() == ')') {
        int required = number(s.substr(19,s.size()-20)), have = 0, uncertain = 0;
        for (const char* dungeon : {"Forest Temple","Goron Mines","Lakebed Temple","Arbiters Grounds","Snowpeak Ruins","Temple of Time","City in the Sky","Palace of Twilight"}) {
            auto it = events.find(std::string("Can Complete ") + dungeon);
            if (it == events.end() || it->second == Truth::unknown) ++uncertain;
            else if (it->second == Truth::yes) ++have;
        }
        return required < 0 ? Truth::unknown : have >= required ? Truth::yes : have+uncertain < required ? Truth::no : Truth::unknown;
    }
    if (s.starts_with("golden bugs(") && s.back() == ')') {
        int required = number(s.substr(12,s.size()-13)), have = 0, uncertain = 0;
        for (const auto& item : catalogue.at("items")) {
            int id = item.at("Id");
            if (id < 0xc0 || id > 0xd7) continue;
            int count = countOf(item.at("Name"));
            if (count < 0) ++uncertain; else if (count > 0) ++have;
        }
        return required < 0 ? Truth::unknown : have >= required ? Truth::yes : have+uncertain < required ? Truth::no : Truth::unknown;
    }
    if (s.starts_with("count(") && s.back() == ')') {
        auto comma = s.find(',');
        if (comma == std::string::npos) return Truth::unknown;
        auto count = countOf(trim(s.substr(6, comma - 6)));
        auto required = number(s.substr(comma + 1, s.size() - comma - 2));
        auto itemName = trim(s.substr(6, comma - 6));
        // Boolean inventory adapters cannot prove a multi-item count.
        if (required > 1 && !itemName.starts_with("Progressive ") && !countedItems.contains(itemName)) return Truth::unknown;
        return count < 0 || required < 0 ? Truth::unknown : truth(count >= required);
    }
    for (const std::string op : {"==", "!=", ">=", "<="}) {
        auto pos = s.find(op);
        if (pos == std::string::npos) continue;
        auto key = trim(s.substr(0, pos)), expected = trim(s.substr(pos + 2));
        if (!settings.contains(key)) return Truth::unknown;
        auto actual = scalar(settings.at(key));
        if (op == "==") return truth(actual == expected);
        if (op == "!=") return truth(actual != expected);
        if (catalogue.contains("setting_options") && catalogue.at("setting_options").contains(key)) {
            const auto& options=catalogue.at("setting_options").at(key);
            const auto a=std::find(options.begin(),options.end(),actual), b=std::find(options.begin(),options.end(),expected);
            if(a!=options.end() && b!=options.end()) return truth(op==">=" ? a>=b : a<=b);
        }
        int a = number(actual), b = number(expected);
        return a < 0 || b < 0 ? Truth::unknown : truth(op == ">=" ? a >= b : a <= b);
    }
    const auto& macros = catalogue.at("macros");
    if (macros.contains(s) && !stack.contains(s)) {
        stack.insert(s);
        return evaluate(macros.at(s).get<std::string>(), form, std::move(stack));
    }
    int count = countOf(s);
    return count < 0 ? Truth::unknown : truth(count > 0);
}

void Model::solve() {
    accessible.clear(); events.clear(); reached.clear();
    for (const auto& check : catalogue.at("checks")) accessible[check.at("name").get<std::string>()] = Truth::unknown;
    if (!seedLoaded) return;
    if (settings.value("Logic Rules", Json("")) == "No Logic") {
        for (auto& [name, value] : accessible) value = Truth::yes;
        return;
    }
    // Use vanilla connections for all seeds, as requested by the tracker user.
    std::map<std::string, Truth> twilightCleared;
    for (const auto& area : catalogue.at("areas")) {
        auto name = area.at("Name").get<std::string>();
        reached[name] = {};
        twilightCleared[name] = area.contains("Twilight") ? evaluate("Can Complete " + area.at("Twilight").get<std::string>() + " Twilight") : Truth::yes;
        events["Can Access " + name] = Truth::no;
        if (area.contains("Events") && area.at("Events").is_object())
            for (const auto& [event, req] : area.at("Events").items()) events[event] = Truth::no;
    }
    reached["Root"].fill(Truth::yes);
    reached["Root"][4] = Truth::no;
    bool changed = true;
    size_t passes = 0;
    while (changed && ++passes <= reached.size() * 8 + 1) {
        changed = false;
        auto promote = [&](Truth& destination, Truth value) {
            auto next = either(destination, value);
            if (next != destination) { destination = next; changed = true; }
        };
        for (const auto& area : catalogue.at("areas")) {
            auto name = area.at("Name").get<std::string>();
            auto& forms = reached.at(name);
            auto clear = twilightCleared.at(name);
            auto before = forms;
            bool changeTime = area.value("Can Change Time", false);
            auto transform = area.value("Can Transform", std::string("Always"));
            bool canTransform = transform == "Always" || (transform == "If Transform Anywhere" && settings.value("Logic Transform Anywhere", Json("Off")) == "On");
            auto crystal = inventory.find("Shadow Crystal");
            bool hasCrystal = crystal != inventory.end() && crystal->second > 0;
            for (int f = 0; f < 4; ++f) {
                if (changeTime) promote(forms[f ^ 1], both(before[f], clear));
                if (canTransform && hasCrystal) promote(forms[f ^ 2], both(before[f], clear));
            }
            auto anyForm = Truth::no;
            for (auto value : forms) anyForm = either(anyForm, value);
            promote(events["Can Access " + name], anyForm);
            for (int f = 0; f < 5; ++f) {
                auto reach = forms[f];
                if (reach == Truth::no) continue;
                if (area.contains("Events") && area.at("Events").is_object())
                    for (const auto& [event, requirement] : area.at("Events").items())
                        promote(events[event], both(reach, evaluate(scalar(requirement), f)));
                if (area.contains("Exits") && area.at("Exits").is_object())
                    for (const auto& [exit, requirement] : area.at("Exits").items()) {
                        if (!reached.contains(exit)) continue;
                        auto cleared = twilightCleared.at(exit);
                        auto access = both(reach, evaluate(scalar(requirement), f));
                        if (cleared != Truth::no && f < 4)
                            promote(reached.at(exit)[f], both(access, cleared));
                        if (cleared != Truth::yes)
                            promote(reached.at(exit)[4], both(access, cleared == Truth::no ? Truth::yes : Truth::unknown));
                    }
            }
            // The upstream search also tests the twilight form when entering an
            // uncleared province, even from a reachable human/wolf parent area.
            if (area.contains("Exits") && area.at("Exits").is_object())
                for (const auto& [exit, requirement] : area.at("Exits").items()) {
                    if (!reached.contains(exit) || twilightCleared.at(exit) == Truth::yes) continue;
                    auto access = both(anyForm, evaluate(scalar(requirement), 4));
                    promote(reached.at(exit)[4], both(access, twilightCleared.at(exit) == Truth::no ? Truth::yes : Truth::unknown));
                }
        }
    }
    for (const auto& check : catalogue.at("checks")) {
        auto result = Truth::no;
        for (const auto& access : check.at("access")) {
            auto area = reached.find(access.at("area").get<std::string>());
            if (area == reached.end()) { result = either(result, Truth::unknown); continue; }
            for (int f = 0; f < 5; ++f)
                result = either(result, both(area->second[f], evaluate(access.at("requirement").get<std::string>(), f)));
        }
        const std::string checkName=check.at("name");
        // Agitha's outstanding deliveries depend on the caught bug, not the
        // solver's assumed route to Castle Town. Delivery flags mark completion.
        if (checkName.starts_with("Agitha ") && checkName.ends_with(" Reward")) {
            const auto bug=checkName.substr(7,checkName.size()-14);
            auto owned=inventory.find(bug);
            result=obtained.contains(checkName) ? Truth::no : owned==inventory.end() || owned->second<0 ? Truth::unknown : truth(owned->second>0);
        }
        accessible[checkName] = check.at("access").empty() ? Truth::unknown : result;
    }
}
}
