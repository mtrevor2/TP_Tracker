#include "model.hpp"
#include "notebook.hpp"
#include "seed.hpp"
#include "map.hpp"
#include "mods/svc/host.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"
#include "mods/svc/ui.h"
#include "mods/svc/item.h"
#include "mods/svc/save.h"
#include "mods/svc/resource.h"
#include "mods/svc/file.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_msg_object.h"
#include "d/d_msg_class.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <future>
#include "stick_navigation.hpp"
#include "controller.hpp"
#include "mods/svc/config.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(UiService, svc_ui);
IMPORT_SERVICE(ItemService, svc_item);
IMPORT_SERVICE(SaveService, svc_save);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(FileService, svc_file);
IMPORT_SERVICE(HostService, svc_host);
IMPORT_SERVICE(ConfigService, svc_config);

namespace {
using tracker::Json;
using tracker::Truth;
tracker::Model model;
UiWindowHandle window = 0;
UiWindowHandle detailWindow = 0;
UiElementHandle detailText = 0, detailButton = 0;
std::string selectedCheck;
std::string detailMarkup;
uint64_t detailRevision=0;
std::vector<UiElementHandle> detailSections, detailAnchors;
size_t detailSectionCount=0;
int detailScrollRow=0;
tracker::StickNavigation detailNavigation;
bool skipSelectedDisabled(ModContext*,void*);
void navigateDetail() {
    int x=0,y=0;
    if (!tracker::readRightStick(x,y)) { detailNavigation={}; return; }
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const int direction=detailNavigation.sample(x,y,now);
    if (std::abs(detailNavigation.held)!=2 || detailSectionCount==0) return;
    const int last=static_cast<int>(detailSectionCount)-1+(skipSelectedDisabled(nullptr,nullptr) ? 0 : 1);
    if(direction) detailScrollRow=std::clamp(detailScrollRow+(direction==2 ? 1 : -1),0,last);
    detailScrollRow=std::clamp(detailScrollRow,0,last);
    svc_ui->elem_focus(mod_ctx,detailScrollRow==static_cast<int>(detailSectionCount) ? detailButton : detailAnchors[detailScrollRow]);
}
UiMenuTabHandle menuTab = 0;
ConfigVarHandle keyboardBinding = 0, controllerBinding = 0;
int areaSort = 1;
UiDialogHandle bindingDialog=0;
int capturingDevice=-1;
bool captureReady=false;
UiElementHandle bindingButtons[2]{};
bool shortcutHeld = true;
int64_t previousKeyboard = -1, previousController = -1;
UiElementHandle inventoryElement = 0, checksElement = 0;
UiElementHandle settingsStatus = 0, countElement = 0;
constexpr size_t pageSize = 10;
size_t checkPage = 0, totalPages = 1, visibleRows = 0;
bool focusPageStart = false;
UiElementHandle pageControl = 0;
std::vector<UiElementHandle> checkRows, headingRows;
std::vector<std::string> visibleNames;
Json hintSigns=Json::array(), hintNotes=Json::object();
std::string manualNotes;
std::vector<std::string> personalNotes;
std::vector<UiElementHandle> personalCards, personalDeletes;
Json inventoryArt=Json::object();
UiElementHandle notesElement=0, newNoteControl=0;
std::vector<UiElementHandle> hintSections, hintAnchors;
size_t hintSectionCount=0;
tracker::StickNavigation notesNavigation;
int notesScrollRow=0;
bool notesTab=false, notebookDirty=false;


int stickRow = -1;
tracker::StickNavigation stickNavigation;
void inspectCheck(ModContext*, void* data);
ItemGiveHandle itemObserver = 0;
SaveObserverHandle saveObserver = 0;
bool dirty = true, allChecks = false, hideCompleted = false;
bool engineDirty = false;
bool seedRefreshPending = false;
int statusFilter = 0;
uint64_t revision = 1, renderedRevision = 0;
void previousPage(ModContext*,void*) { if (checkPage) { --checkPage; ++revision; focusPageStart=true; } }
void nextPage(ModContext*,void*) { if (checkPage+1<totalPages) { ++checkPage; ++revision; focusPageStart=true; } }
bool previousDisabled(ModContext*,void*) { return checkPage==0; }
bool nextDisabled(ModContext*,void*) { return checkPage+1>=totalPages; }
bool rowDisabled(ModContext*,void* data) { return reinterpret_cast<uintptr_t>(data)>=visibleRows; }
void pageGet(ModContext*,void*,UiControlValue* v) { v->int_value=checkPage+1; }
void pageSet(ModContext*,void*,const UiControlValue* v) {
    size_t next=static_cast<size_t>(std::clamp<int64_t>(v->int_value,1,static_cast<int64_t>(totalPages)))-1;
    if (next!=checkPage) { checkPage=next; ++revision; focusPageStart=true; }
}
uint64_t logicGeneration = 0, runningGeneration = 0;
bool logicRequested = false;
std::future<std::map<std::string, Truth>> logicJob;
std::string search;
std::string status = "Select this save's anti-spoiler log in F1 > Mods > TPTracker.";
std::string lastInventory, lastChecks;
std::set<std::string> journal;
std::chrono::steady_clock::time_point nextScan{};
constexpr const char* blobName = "tracker-v1";

const char* styles = R"(
window { max-width: 1120dp; max-height: 800dp; background-color: #211e15; border-color: #9b8955; }
window content pane { font-size: 17dp; }
window content { flex-direction: row-reverse; }
window content pane:first-child { flex: 1; background-color: #302c20; }
window content pane:last-child { flex: 0 0 32%; background-color: #191a15; }
.tp-title { display: block; color: #e6d39e; font-size: 24dp; font-weight: bold; margin-bottom: 12dp; }
.tp-note { display: block; color: #c5baa0; font-size: 14dp; margin-top: 4dp; margin-bottom: 12dp; }
.tp-group { display: block; color: #e6d39e; font-size: 20dp; margin-top: 18dp; margin-bottom: 8dp; border-bottom: 2dp #9b8955; padding-bottom: 6dp; }
.tp-grid { display: flex; flex-wrap: wrap; }
.tp-item { width: 44%; min-height: 48dp; padding: 6dp; margin: 2dp; border: 1dp #5b553e; border-radius: 5dp; font-size: 14dp; color: #898577; }
.tp-item.owned { color: #ede0b3; background-color: #45452a; border-color: #ad965a; }
.tp-check { text-align: left; display: block; width: auto; padding: 8dp; margin-bottom: 4dp; border-bottom: 1dp #4e4734; }
.tp-check.open { color: #70ed86; }
.tp-check.locked { color: #ff7c76; }
.tp-check.unknown { color: #e5c977; }
.tp-check.done { color: #999b95; text-decoration: line-through; }
.tp-check.skipped { color: #cd91f4; text-decoration: line-through; }
.tp-detail-sections { display: block; }
.tp-scroll-anchor { display: block; height: 1dp; min-height: 1dp; padding: 0dp; margin: 0dp; border-width: 0dp; opacity: 0; }
.tp-postit { display: block; background-color: #514629; color: #fff0b7; border-left: 4dp #d2b04f; padding: 12dp; margin-top: 10dp; font-size: 17dp; }
.tp-item img { display: block; width: 48dp; height: 48dp; margin: 0dp auto 5dp auto; }
.tp-item { text-align: center; font-size: 13dp; min-height: 92dp; }
.tp-item.missing img { opacity: 0.35; }
.tp-item-count { display: block; color: #d2b866; margin-top: 4dp; }
)";

std::string journalText() {
    Json value = {{"version", 1}, {"seed", model.seed}, {"settings", model.settings},
                  {"entrances", model.entrances}, {"entrances_complete", model.entrancesComplete}, {"checks", journal}};
    value["skipped"]=model.skipped;
    value["hint_notes"]=hintNotes;
    value["manual_notes"]=manualNotes;
    value["personal_notes"]=personalNotes;
    return value.dump();
}
bool persist() {
    auto text = journalText();
    if (text.size() > SAVE_BLOB_BUDGET_BYTES) {
        status = "Tracker journal exceeds save storage budget.";
        ++revision;
        return false;
    }
    auto result = svc_save->set_blob(mod_ctx, blobName, text.data(), text.size());
    if (result != MOD_OK && result != MOD_UNAVAILABLE) mods::log::warn("TPTracker: cannot update save journal");
    return result==MOD_OK;
}

void skipSelectedCheck(ModContext*, void*) {
    const auto before=model.skipped;
    if (model.toggleSkipped(selectedCheck)) {
        if (!persist()) model.skipped=before;
        ++revision;
    }
}
bool skipSelectedDisabled(ModContext*,void*) { return model.obtained.contains(selectedCheck) || !model.aliases.contains(selectedCheck); }
ModResult updateDetail(ModContext*,void*,ModError*) {
    if (!detailText) return MOD_OK;
    navigateDetail();
    if(detailRevision==revision) return MOD_OK;
    detailRevision=revision;
    const auto markup=model.checkDetails(selectedCheck);
    if(markup==detailMarkup) return MOD_OK;
    detailMarkup=markup;
    // Each generated div gets a small focus anchor. The SDK scrolls focused
    // controls into view; a single large RML element cannot scroll in sections.
    detailSectionCount=0;
    size_t start=0;
    while(start<markup.size()) {
        auto end=markup.find("</div>",start);
        end=end==std::string::npos ? markup.size() : end+6;
        const auto part=markup.substr(start,end-start);
        if(detailSectionCount==detailSections.size()) {
            UiElementHandle section=0,anchor=0;
            UiControlDesc control=UI_CONTROL_DESC_INIT;
            control.kind=UI_CONTROL_BUTTON; control.label="";
            control.on_pressed=[](ModContext*,void*) {};
            auto result=svc_ui->pane_add_control(mod_ctx,detailText,&control,&anchor);
            if(result!=MOD_OK) return result;
            svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
            result=svc_ui->pane_add_rml(mod_ctx,detailText,"",&section);
            if(result!=MOD_OK) return result;
            detailAnchors.push_back(anchor); detailSections.push_back(section);
        }
        svc_ui->elem_set_rml(mod_ctx,detailSections[detailSectionCount],part.c_str());
        ++detailSectionCount; start=end;
    }
    for(size_t i=0;i<detailSections.size();++i) {
        svc_ui->elem_set_visible(mod_ctx,detailSections[i],i<detailSectionCount);
        svc_ui->elem_set_visible(mod_ctx,detailAnchors[i],i<detailSectionCount);
    }
    svc_ui->elem_set_text(mod_ctx,detailButton,model.skipped.contains(selectedCheck) ? "Undo skipping this check" : "Skip this check");
    return MOD_OK;
}
ModResult buildDetail(ModContext*,UiWindowHandle,UiElementHandle left,UiElementHandle right,void*,ModError*) {
    UiRowDesc row=UI_ROW_DESC_INIT;
    auto result=svc_ui->pane_add_row(mod_ctx,left,&row,&detailText);
    svc_ui->elem_set_class(mod_ctx,detailText,"tp-detail-sections",true);
    if(result!=MOD_OK) return result;
    UiControlDesc button=UI_CONTROL_DESC_INIT;
    button.kind=UI_CONTROL_BUTTON; button.label="Skip this check";
    button.on_pressed=skipSelectedCheck; button.is_disabled=skipSelectedDisabled;
    result=svc_ui->pane_add_control(mod_ctx,left,&button,&detailButton);
    if(result!=MOD_OK) return result;
    svc_ui->pane_add_text(mod_ctx,right,"Right stick: up/down scrolls requirements. Requirements use this seed's settings and your current inventory. Area access and local requirements must both be met. Completed checks cannot be skipped.",nullptr);
    return updateDetail(nullptr,nullptr,nullptr);
}
void detailClosed(ModContext*,UiWindowHandle,void*) { detailWindow=0; detailText=detailButton=0; selectedCheck.clear(); detailMarkup.clear(); detailRevision=0; detailSections.clear(); detailAnchors.clear(); detailSectionCount=0; detailScrollRow=0; detailNavigation={}; stickNavigation={}; }
void inspectCheck(ModContext*, void* data) {
    stickRow=static_cast<int>(reinterpret_cast<uintptr_t>(data));
    if (stickRow<0 || static_cast<size_t>(stickRow)>=visibleNames.size() || detailWindow) return;
    selectedCheck=visibleNames[stickRow];
    UiTabDesc tab=UI_TAB_DESC_INIT; tab.title="Check details"; tab.build=buildDetail; tab.update=updateDetail;
    UiWindowDesc desc=UI_WINDOW_DESC_INIT; desc.tabs=&tab; desc.tab_count=1; desc.on_closed=detailClosed;
    desc.rcss=R"(
window { max-width: 1000dp; max-height: 800dp; }
window content pane:first-child { flex: 1; }
window content pane:last-child { flex: 0 0 24%; }
window content pane { font-size: 17dp; }
.tp-detail-sections { display: block; }
.tp-scroll-anchor { display: block; height: 1dp; min-height: 1dp; padding: 0dp; margin: 0dp; border-width: 0dp; opacity: 0; }
window content pane div { display: block; margin-bottom: 8dp; }
.tp-title { display: block; font-size: 24dp; font-weight: bold; color: #e4d196; margin-bottom: 14dp; }
.tp-group { display: block; font-size: 20dp; color: #e4d196; margin-top: 18dp; margin-bottom: 10dp; padding-bottom: 6dp; border-bottom: 1dp #9b8955; }
.tp-note { display: block; font-size: 15dp; color: #c5baa0; margin-top: 8dp; margin-bottom: 14dp; }
)";
    if(svc_ui->window_push(mod_ctx,&desc,&detailWindow)!=MOD_OK) detailClosed(nullptr,0,nullptr);
}

void backupCard() {
    const char* path = nullptr;
    std::string error;
    if (svc_host->data_dir(mod_ctx,&path) == MOD_OK && path &&
        !tracker::backupSelectedCard(std::filesystem::u8path(path),error) && !error.empty())
        mods::log::warn("TPTracker: card backup failed: {}",error);
}
void restore(bool autoSeed = true) {
    if(detailWindow) svc_ui->window_close(mod_ctx,detailWindow);
    backupCard();
    model.resetSave();
    ++logicGeneration;
    ++revision;
    journal.clear();
    hintNotes=Json::object(); manualNotes.clear(); personalNotes.clear(); notebookDirty=false;
    size_t size = 0;
    if (svc_save->get_blob(mod_ctx, blobName, nullptr, &size) == MOD_OK && size <= SAVE_BLOB_BUDGET_BYTES) {
        std::string text(size, '\0');
        if (svc_save->get_blob(mod_ctx, blobName, text.data(), &size) == MOD_OK) {
            try {
                auto value = Json::parse(text);
                if (value.at("version") != 1) throw std::runtime_error("Unsupported journal version");
                if (!value.value("seed", std::string{}).empty()) model.loadSeed(text);
                for (const auto& name : value.at("checks"))
                    if (model.aliases.contains(name.get<std::string>())) journal.insert(name.get<std::string>());
                model.obtained = journal;
                for (const auto& name : value.value("skipped",Json::array())) {
                    auto key=name.get<std::string>();
                    if (model.aliases.contains(key) && !model.obtained.contains(key)) model.skipped.insert(key);
                }
                manualNotes=value.value("manual_notes",std::string{});
                personalNotes=value.value("personal_notes",std::vector<std::string>{});
                if (!tracker::trim(manualNotes).empty()) personalNotes.push_back(manualNotes);
                manualNotes.clear();
                hintNotes=value.value("hint_notes",Json::object());
                if (!hintNotes.is_object()) throw std::runtime_error("Invalid hint notebook");
            } catch (const std::exception& e) {
                model.resetSave(); journal.clear(); hintNotes=Json::object(); manualNotes.clear(); personalNotes.clear();
                mods::log::warn("TPTracker: could not restore journal: {}", e.what());
            }
        }
    }
    const char* dataPath = nullptr;
    std::string seedError;
    if (autoSeed && svc_host->data_dir(mod_ctx, &dataPath) == MOD_OK && dataPath &&
        tracker::loadSelectedSeed(model, std::filesystem::u8path(dataPath).parent_path(), seedError)) {
        status = "Seed: " + model.seed + " (selected save)";
    } else {
        status = model.seedLoaded ? "Seed: " + model.seed : "Load this save's anti-spoiler log in F1. Automatic seed lookup needs a saved randomizer slot.";
    }
    dirty = true;
    nextScan = {};
    lastInventory.clear(); lastChecks.clear();
}
void onSaveLoaded(ModContext*, uint32_t, void*) { restore(); }
void onNewSave(ModContext*, uint32_t, void*) { restore(false); }
void onSaveWritten(ModContext*, uint32_t, void*) { seedRefreshPending = true; }

bool flagObtained(const Json& flag) {
    const int id = flag.at("flag").get<int>();
    const auto kind = flag.at("kind").get<std::string>();
    if (kind == "event") return dComIfGs_isEventBit(static_cast<u16>(id));
    const int save = flag.at("save").get<int>();
    if (kind == "chest") return id < 64 && dComIfGs_isStageTbox(save, id);
    if (kind == "switch") return id < 128 && dComIfGs_isStageSwitch(save, id);
    if (kind == "item" && id >= 128 && id < 160) {
        auto* stageInfo = dComIfGp_getStageStagInfo();
        if (stageInfo && save == dStage_stagInfo_GetSaveTbl(stageInfo)) return dComIfGs_isItem(id, -1);
        return g_dComIfG_gameInfo.info.getSavedata().getSave(save).getBit().isItem(id - 128);
    }
    return false;
}

void scan() {
    auto* stageInfo = dComIfGp_getStageStagInfo();
    if (!stageInfo) return;
    const char* name = dComIfGp_getStartStageName();
    std::string stage = name ? name : "";
    int room = dComIfGp_roomControl_getStayNo();
    int layer = dComIfG_play_c::getLayerNo(room);
    bool changed = dirty || model.stage != stage || model.room != room || model.layer != layer;
    model.stage = stage;
    model.room = room;
    model.layer = layer;
    auto inventory = model.inventory;
    inventory.clear();
    for (const auto& item : model.catalogue.at("items")) {
        auto name = item.at("Name").get<std::string>();
        // Progressive/count-based custom items need explicit inventory adapters below.
        if (name.starts_with("Progressive ")) continue;
        const int value = checkItemGet(static_cast<u8>(item.at("Id").get<int>()), -1);
        inventory[name] = value < 0 ? -1 : value > 0 ? 1 : 0;
    }
    auto tiers = [&](const char* name, std::initializer_list<int> ids) {
        int highest = 0, tier = 0;
        bool known = true;
        for (int id : ids) {
            ++tier;
            int value = checkItemGet(static_cast<u8>(id), -1);
            if (value < 0) known = false;
            else if (value > 0) highest = tier;
        }
        inventory[name] = known ? highest : -1;
    };
    // IDs from randomizer/src/item_ids.h and verify_item_functions.cpp.
    tiers("Progressive Sword", {0x3f, 0x28, 0x29, 0x49});
    tiers("Progressive Bow", {0x43, 0x55, 0x56});
    tiers("Progressive Clawshot", {0x44, 0x47});
    tiers("Progressive Fishing Rod", {0x4a, 0x5c});
    // The engine's COPY_ROD_2 query returns -1. The randomizer restores it by
    // setting this event, rather than installing another inventory-slot item.
    inventory["Progressive Dominion Rod"] = checkItemGet(0x46, -1) > 0 ? (dComIfGs_isEventBit(0x2580) ? 2 : 1) : 0;
    inventory["Bomb Bag"] = 0;
    // Custom randomizer keys unlock persistent switches, not the current room's key counter.
    inventory["Faron Woods Coro Key"] = dComIfGs_isStageSwitch(0x2,0x0c) != 0;
    inventory["North Faron Woods Gate Key"] = dComIfGs_isStageSwitch(0x2,0x14) != 0;
    inventory["Gate Keys"] = dComIfGs_isEventBit(0x810) != 0;
    for (int i=0;i<3;++i) inventory["Bomb Bag"] += dComIfGs_getItem(SLOT_15+i,false)!=0xff;
    inventory["Goron Mines Key Shard"] = 0;
    for (int id : {0xf9,0xfa,0xfb}) inventory["Goron Mines Key Shard"] += checkItemGet(id,-1)>0;
    tiers("Progressive Wallet", {0x35, 0x36});
    tiers("Progressive Hidden Skill", {0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7});
    inventory["Poe Soul"] = dComIfGs_getPohSpiritNum();
    inventory["Faron Twilight Tear"] = dComIfGs_getLightDropNum(0);
    inventory["Eldin Twilight Tear"] = dComIfGs_getLightDropNum(1);
    inventory["Lanayru Twilight Tear"] = dComIfGs_getLightDropNum(2);
    model.countedItems = {"Poe Soul", "Faron Twilight Tear", "Eldin Twilight Tear", "Lanayru Twilight Tear"};
    inventory["Heart Count"] = dComIfGs_getMaxLife() / 5;
    inventory["Empty Bottle"] = 0;
    for (int i = 0; i < 4; ++i) inventory["Empty Bottle"] += dComIfGs_getItem(SLOT_11 + i, true) != 0xff;
    model.countedItems.insert("Empty Bottle");
    inventory["Progressive Fused Shadow"] = 0;
    inventory["Progressive Mirror Shard"] = 0;
    for (int i = 0; i < 3; ++i) inventory["Progressive Fused Shadow"] += dComIfGs_isCollectCrystal(i) != 0;
    for (int i = 0; i < 4; ++i) inventory["Progressive Mirror Shard"] += dComIfGs_isCollectMirror(i) != 0;
    struct KeyInfo { int save; const char* name; std::initializer_list<int> doors; };
    const KeyInfo keys[] = {
        {0x10,"Forest Temple",{7,11,43,62}}, {0x11,"Goron Mines",{51,61,63}},
        {0x12,"Lakebed Temple",{35,36,52}}, {0x13,"Arbiters Grounds",{39,70,77,90,91}},
        {0x14,"Snowpeak Ruins",{43,44,47,48}}, {0x15,"Temple of Time",{27,28,29}},
        {0x16,"City in the Sky",{6}}, {0x17,"Palace of Twilight",{6,7,8,35,36,37,51}},
        {0x18,"Hyrule Castle",{76,111,124}}
    };
    for (const auto& key : keys) {
        std::string small = std::string(key.name) + " Small Key";
        int count = key.save == dStage_stagInfo_GetSaveTbl(stageInfo) ? dComIfGs_getKeyNum()
            : dComIfGs_getSaveData()->getSave(key.save).getBit().getKeyNum();
        for (int door : key.doors) count += dComIfGs_isStageSwitch(key.save, door) != 0;
        inventory[small] = count;
        model.countedItems.insert(small);
        inventory[std::string(key.name) + (key.save == 0x14 ? " Bedroom Key" : " Big Key")] = dComIfGs_isDungeonItemBossKey(key.save) != 0;
    }
    auto completed = journal;
    for (const auto& check : model.catalogue.at("checks")) {
        for (const auto& flag : check.at("flags"))
            if (flagObtained(flag)) { completed.insert(check.at("name").get<std::string>()); break; }
        // Only vanilla bugs can be correlated with inventory; a shuffled bug item
        // proves nothing about the original bug's location.
        if (model.settings.value("Golden Bugs", Json("")) == "Off") {
            const auto& categories = check.at("categories");
            if (std::find(categories.begin(), categories.end(), "Golden Bug") != categories.end()) {
                auto item = inventory.find(check.value("original_item", std::string{}));
                if (item != inventory.end() && item->second > 0) completed.insert(check.at("name").get<std::string>());
            }
        }
    }
    changed = changed || inventory != model.inventory || completed != model.obtained;
    model.inventory = std::move(inventory);
    model.obtained = std::move(completed);
    bool unskipped=false;
    for (const auto& name : model.obtained) unskipped=model.skipped.erase(name)>0 || unskipped;
    if (unskipped) persist();
    if (changed) {
        ++revision;
        ++logicGeneration;
        model.accessible.clear();
        logicRequested = true;
    }
    dirty = false;
}

void updateLogic() {
    if (logicJob.valid() && logicJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = logicJob.get();
            if (runningGeneration == logicGeneration) { model.accessible = std::move(result); ++revision; }
        } catch (const std::exception& e) {
            status = std::string("Logic unavailable: ") + e.what(); ++revision;
        }
    }
    if (logicRequested && !logicJob.valid()) {
        logicRequested = false;
        runningGeneration = logicGeneration;
        // The worker owns its copy. No game pointers, SDK calls, or UI calls cross threads.
        logicJob = std::async(std::launch::async, [snapshot = model]() mutable {
            snapshot.solve();
            return std::move(snapshot.accessible);
        });
    }
}

void onGive(ModContext*, const ItemGiveInfo* info, void*) {
    // Grants are the commit point. resolve_check is a preview and must never mark completion.
    if (info && info->check_name && model.give(info->check_name)) {
        journal.insert(model.aliases.at(info->check_name));
        ++revision;
        persist();
    }
    engineDirty = true;
}
// Hints never force a logic solve: only a changed snapshot does that. A bool coalesces
// any number of repeated switch assertions between the bounded snapshot passes.
void onEngineChanged(ModContext*, void*, void*, void*) { engineDirty = true; }

void onDialogueDraw(ModContext*, void* args, void*, void*) {
    auto* message=mods::arg<dMsgObject_c*>(args,0);
    if (!message || !model.seedLoaded || !message->mpRenProc || !message->isDraw()) return;
    const auto status=message->getStatusLocal();
    if (status==0 || status==1 || status==0xb) return;
    auto* actor=message->mpTalkPartner;
    if (!actor || fopAcM_GetName(actor)!=fpcNm_OBJ_KANBAN2_e) return;
    const char* stage=dComIfGp_getStartStageName();
    const auto name=tracker::hintSignAt(hintSigns,stage ? stage : "",fopAcM_GetRoomNo(actor),
        actor->home.pos.x,actor->home.pos.y,actor->home.pos.z);
    if (name.empty() || !model.aliases.contains(name)) return;
    auto* reference=static_cast<const jmessage_tReference*>(message->mpRenProc->getReference());
    if (!reference) return;
    const int page=reference->mPageNum;
    if (page<0 || page>=16) return;
    const auto text=tracker::hintPlainText(reference->mText);
    if (text.empty() || text.size()>2048) return;
    auto& pages=hintNotes[name];
    if (!pages.is_object()) pages=Json::object();
    const auto key=std::to_string(page);
    // Rendered text grows while letters appear. Keep the longest observed page;
    // reopening a sign must not replace a complete note with its first letter.
    if (text.size()<=pages.value(key,std::string{}).size()) return;
    const auto previous=pages;
    pages[key]=text;
    if (journalText().size()>SAVE_BLOB_BUDGET_BYTES-1024) {
        pages=previous;
        return;
    }
    journal.insert(name); model.obtained.insert(name); model.skipped.erase(name);
    notebookDirty=true; ++revision;
}

std::string inventoryRml() {
    std::string rml = "<div class='tp-title'>TPTracker</div><div class='tp-note'>" + tracker::escape(status) + "</div>";
    rml += "<div class='tp-note'>" + tracker::escape(model.stage.empty() ? "Waiting for gameplay" : model.stage) +
           " / room " + std::to_string(model.room) + "</div>";
    for (const char* section : {"Progression","Collectibles","Keys","Quest items"}) {
      rml+="<div class='tp-group'>"+std::string(section)+"</div><div class='tp-grid'>";
      for (const auto& item : model.catalogue.at("items")) {
        auto name = item.at("Name").get<std::string>();
        if (name.ends_with("Portal") || (name.find("Bottle") != std::string::npos && name != "Empty Bottle")) continue;
        if (!inventoryArt.contains(name) || inventoryArt.at(name).at("section")!=section) continue;
        const auto& art=inventoryArt.at(name);
        auto found = model.inventory.find(name);
        int count = found == model.inventory.end() ? -1 : found->second;
        rml += "<div class='tp-item" + std::string(count > 0 ? " owned" : " missing") + "'>";
        rml += "<img src='mod://com.mikey022.tp_randomizer_tracker/res/"+tracker::escape(art.at("icon").get<std::string>())+"?rev=0.4.24'/>";
        rml += tracker::escape(name);
        rml += "<span class='tp-item-count'>" + (count<0 ? std::string("?") : std::to_string(count)) + " / " + std::to_string(model.inventoryMaximum(name)) + "</span>";
        rml += "</div>";
      }
      rml+="</div>";
    }
    return rml + "<div class='tp-note'>Current / maximum. Progressive items show upgrade tiers; small keys include keys already used. ? = count unavailable. Collectibles, quest items and keys can also be required for progression. Artwork: Henriko Magnifico, game and randomizer item icons.</div>";
}
std::string checksRml() {
    std::string rml = "<div class='tp-title'>" + std::string(allChecks ? "All Checks" : "Current Area") + "</div>";
    if (!allChecks && model.catalogue.contains("stage_labels"))
        rml += "<div class='tp-note'>" + tracker::escape(model.catalogue.at("stage_labels").value(model.stage, model.stage)) + "</div>";
    rml += "<div class='tp-note'>OPEN / green · LOCKED / red · UNKNOWN / amber · DONE / gray · SKIPPED / purple</div>";
    rml += "<div class='tp-note'>Select a check to view its requirements. Use Skip or Undo in its details window. Skipped checks are hidden on both maps.</div>";
    if (!model.seedLoaded) rml += "<div class='tp-note'>Load a seed log to evaluate logic.</div>";
    else if (logicRequested || logicJob.valid()) rml += "<div class='tp-note'>Updating reachability...</div>";
    rml += "<div class='tp-note'>Vanilla entrance logic: shuffled connections are intentionally ignored.</div>";
    rml += "<div class='tp-note'>Controller right stick: up/down scrolls checks; left/right changes page. Page buttons also work with mouse or controller.</div>";
    size_t shown = 0, done = 0, enabled = 0;
    struct Row { std::string name, group, css, label; };
    std::vector<Row> rows;
    std::string needle = search;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& check : model.catalogue.at("checks")) {
        if (!model.enabled(check)) continue;
        ++enabled;
        auto name = check.at("name").get<std::string>();
        bool complete = model.obtained.contains(name);
        if (complete) ++done;
        if ((!allChecks && !model.inArea(check)) || (hideCompleted && complete)) continue;
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!needle.empty() && lower.find(needle) == std::string::npos) continue;
        auto state = model.accessible.contains(name) ? model.accessible.at(name) : Truth::unknown;
        bool skipped=!complete && model.skipped.contains(name);
        int filterState = complete ? 3 : skipped ? 5 : state == Truth::yes ? 1 : state == Truth::no ? 2 : 4;
        if (statusFilter != 0 && statusFilter != filterState) continue;
        ++shown;

        const char* css = complete ? "done" : skipped ? "skipped" : state == Truth::yes ? "open" : state == Truth::no ? "locked" : "unknown";
        const char* label = complete ? "DONE" : skipped ? "SKIPPED" : state == Truth::yes ? "OPEN" : state == Truth::no ? "LOCKED" : "UNKNOWN";
        rows.push_back({name,allChecks ? check.at("group").get<std::string>() : "",css,label});
    }
    std::stable_sort(rows.begin(),rows.end(),[](const Row& a,const Row& b) {
        auto rank=[](const std::string& label) { return label=="OPEN" ? 0 : label=="LOCKED" ? 1 : label=="UNKNOWN" ? 2 : label=="DONE" ? 3 : 4; };
        if (areaSort && rank(a.label)!=rank(b.label)) return rank(a.label)<rank(b.label);
        return a.name<b.name;
    });
    const size_t pages = tracker::pageCount(rows.size(),pageSize);
    totalPages=pages;
    checkPage = std::min(checkPage,pages-1);
    visibleRows=std::min(pageSize,rows.size()-checkPage*pageSize);
    std::string previousGroup;
    visibleNames.clear();
    for (size_t i=0;i<checkRows.size();++i) {
        const size_t index = checkPage*pageSize+i;
        const bool visible = index < rows.size();
        svc_ui->elem_set_visible(mod_ctx,checkRows[i],visible);
        bool heading = visible && allChecks && (i==0 || rows[index].group != previousGroup);
        svc_ui->elem_set_visible(mod_ctx,headingRows[i],heading);
        if (!visible) continue;
        const auto& row = rows[index];
        visibleNames.push_back(row.name);
        if (heading) svc_ui->elem_set_text(mod_ctx,headingRows[i],row.group.c_str());
        previousGroup = row.group;
        std::string label = "[" + row.label + "] " + row.name;
        svc_ui->control_set_label(mod_ctx,checkRows[i],label.c_str());
        for (const char* cls : {"done","open","locked","unknown","skipped"})
            svc_ui->elem_set_class(mod_ctx,checkRows[i],cls,row.css==cls);
    }
    const auto count = "Page " + std::to_string(checkPage+1) + " of " + std::to_string(pages) + " / " + std::to_string(shown) + " matching checks / " + std::to_string(done) + " of " + std::to_string(enabled) + " completed across all areas";
    if (countElement) svc_ui->elem_set_text(mod_ctx,countElement,count.c_str());
    if (pageControl) svc_ui->control_set_label(mod_ctx,pageControl,("Page (left / right) / " + std::to_string(pages) + " total").c_str());
    if (focusPageStart) {
        focusPageStart=false;
        svc_ui->elem_focus(mod_ctx,visibleRows ? checkRows.front() : pageControl);
    }
    if (shown == 0) rml += "<div class='tp-note'>No matching checks for " + tracker::escape(allChecks ? "All Checks" : model.stage) + ". Check the status filter, Filter checks text and Hide completed checks setting.</div>";
    return rml;
}
void navigateStick() {
    if (!window || !pageControl) return;
    int x=0,y=0;
    if (!tracker::readRightStick(x,y)) { stickNavigation={}; return; }
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const int direction=stickNavigation.sample(x,y,now);
    if (!stickNavigation.held) return;
    int target=stickRow;
    if (direction==2) target=std::min(target+1,static_cast<int>(visibleRows)-1);
    if (direction==-2) target=target<0 ? static_cast<int>(visibleRows)-1 : std::max(0,target-1);
    if (direction==1 || direction==-1) target=0;
    target=std::clamp(target,0,std::max(0,static_cast<int>(visibleRows)-1));
    // Focus is checked by the host against the top document. Reassert while held
    // so its generic arrow-key pane selection cannot steal right-stick navigation.
    if (svc_ui->elem_focus(mod_ctx,visibleRows ? checkRows[target] : pageControl)!=MOD_OK) {
        stickNavigation={}; return;
    }
    stickRow=target;
    if (direction==1) nextPage(mod_ctx,nullptr);
    if (direction==-1) previousPage(mod_ctx,nullptr);
}
ModResult updateTab(ModContext*, void*, ModError*) {
    if (!inventoryElement || !checksElement) return MOD_OK;
    if(!detailWindow) navigateStick();
    if (renderedRevision == revision) return MOD_OK;
    auto inv = inventoryRml(), checks = checksRml();
    if (inv != lastInventory) {
        auto result = svc_ui->elem_set_rml(mod_ctx, inventoryElement, inv.c_str());
        if (result != MOD_OK) return result;
        lastInventory = std::move(inv);
    }
    if (checks != lastChecks) {
        auto result = svc_ui->elem_set_rml(mod_ctx, checksElement, checks.c_str());
        if (result != MOD_OK) return result;
        lastChecks = std::move(checks);
    }
    renderedRevision = revision;
    return MOD_OK;
}
void statusGet(ModContext*, void*, UiControlValue* v) { v->int_value = statusFilter; }
void statusSet(ModContext*, void*, const UiControlValue* v) { checkPage = 0; statusFilter = std::clamp<int>(static_cast<int>(v->int_value), 0, 5); ++revision; }
void sortGet(ModContext*, void*, UiControlValue* v) { v->int_value=areaSort; }
void sortSet(ModContext*, void*, const UiControlValue* v) {
    areaSort=std::clamp<int>(static_cast<int>(v->int_value),0,1);
    checkPage=0; stickRow=-1; ++revision;
}
ModResult buildTab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle right, void* data, ModError*) {
    std::swap(left,right); // First SDK pane must contain the controls for tab -> Down navigation.
    allChecks = data != nullptr;
    notesTab=false; notesElement=0;
    statusFilter = 0; // Start with all statuses rather than inheriting the other tab's filter.
    checkPage = 0; totalPages=1; visibleRows=0; focusPageStart=false; pageControl=0;
    stickRow=-1; stickNavigation={};
    inventoryElement = checksElement = countElement = 0;
    checkRows.clear(); headingRows.clear();
    lastInventory.clear(); lastChecks.clear();
    renderedRevision = 0;
    auto result = svc_ui->pane_add_rml(mod_ctx, left, "", &inventoryElement);
    if (result != MOD_OK) return result;
    static const char* options[] = {"All statuses", "OPEN", "LOCKED", "DONE", "UNKNOWN", "SKIPPED"};
    UiControlDesc filter = UI_CONTROL_DESC_INIT;
    filter.kind = UI_CONTROL_DROPDOWN; filter.label = "Check status";
    filter.options = options; filter.option_count = 6; filter.get = statusGet; filter.set = statusSet;
    result = svc_ui->pane_add_control(mod_ctx, right, &filter, nullptr);
    if (result != MOD_OK) return result;
    {
        static const char* sorts[]={"A-Z", "Accessibility"};
        UiControlDesc sort=UI_CONTROL_DESC_INIT;
        sort.kind=UI_CONTROL_DROPDOWN; sort.label="Sort By";
        sort.options=sorts; sort.option_count=2; sort.get=sortGet; sort.set=sortSet;
        result=svc_ui->pane_add_control(mod_ctx,right,&sort,nullptr);
        if (result!=MOD_OK) return result;
    }
    result = svc_ui->pane_add_rml(mod_ctx, right, "", &checksElement);
    if (result != MOD_OK) return result;
    result=svc_ui->pane_add_text(mod_ctx,right,"",&countElement);
    if (result!=MOD_OK) return result;
    UiControlDesc pager=UI_CONTROL_DESC_INIT;
    pager.kind=UI_CONTROL_NUMBER; pager.label="Page (left / right)"; pager.min=1; pager.max=1000; pager.step=1; pager.get=pageGet; pager.set=pageSet;
    result=svc_ui->pane_add_control(mod_ctx,right,&pager,&pageControl);
    if (result!=MOD_OK) return result;
    for (const auto& option : {std::pair{"Previous page",&previousPage},std::pair{"Next page",&nextPage}}) {
        UiControlDesc page = UI_CONTROL_DESC_INIT;
        page.kind = UI_CONTROL_BUTTON; page.label = option.first; page.on_pressed = option.second;
        page.is_disabled=option.second==previousPage ? previousDisabled : nextDisabled;
        result = svc_ui->pane_add_control(mod_ctx,right,&page,nullptr);
        if (result != MOD_OK) return result;
    }
    for (size_t i=0;i<pageSize;++i) {
        UiElementHandle heading=0,rowHandle=0;
        result = svc_ui->pane_add_text(mod_ctx,right,"",&heading);
        if (result != MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,heading,"tp-group",true);
        headingRows.push_back(heading);
        UiControlDesc row = UI_CONTROL_DESC_INIT;
        // A check is informational, not an editable page-number field.
        row.kind = UI_CONTROL_BUTTON; row.label = "Check"; row.on_pressed=inspectCheck;
        row.is_disabled=rowDisabled; row.user_data=reinterpret_cast<void*>(i);
        result = svc_ui->pane_add_control(mod_ctx,right,&row,&rowHandle);
        if (result != MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,rowHandle,"tp-check",true);
        checkRows.push_back(rowHandle);
    }
    return updateTab(mod_ctx,nullptr,nullptr);
}
std::string notesRml() {
    std::string result="<div class='tp-title'>Read hints</div><div class='tp-note'>Hints appear here as you read each sign. Notes and skipped checks are stored with this save; save your game to keep them.</div>";
    if (hintNotes.empty()) result+="<div class='tp-note'>No hint signs read yet.</div>";
    for (const auto& [name,pages] : hintNotes.items()) {
        result+="<div class='tp-group'>"+tracker::escape(name)+"</div>";
        for (int page=0;page<16;++page) {
            auto key=std::to_string(page);
            if (pages.contains(key) && pages[key].is_string()) {
                auto text=tracker::escape(pages[key].get<std::string>());
                size_t p=0; while ((p=text.find('\n',p))!=std::string::npos) { text.replace(p,1,"<br/>"); p+=5; }
                result+="<div class='tp-note'>"+text+"</div>";
            }
        }
    }
    return result;
}
void manualGet(ModContext*,void*,UiControlValue* value) { value->string_value=manualNotes.c_str(); }
void manualSet(ModContext*,void*,const UiControlValue* value) {
    const auto previous=personalNotes;
    const std::string text=value->string_value ? value->string_value : "";
    if (!tracker::addPersonalNote(personalNotes,text)) return;
    manualNotes.clear();
    if (!persist()) { personalNotes=previous; manualNotes=text; }
    ++revision;
}
void deleteNote(ModContext*,void* data) {
    const auto previous=personalNotes;
    if (!tracker::deletePersonalNote(personalNotes,reinterpret_cast<uintptr_t>(data))) return;
    if (!persist()) personalNotes=previous;
    ++revision;
}
ModResult updateNotes(ModContext*,void*,ModError*) {
    if (!notesElement) return MOD_OK;
    int x=0,y=0;
    if (tracker::readRightStick(x,y)) {
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const int direction=notesNavigation.sample(x,y,now);
        if(std::abs(notesNavigation.held)==2) {
            std::vector<UiElementHandle> targets{newNoteControl};
            for(size_t i=0;i<personalNotes.size() && i<personalDeletes.size();++i) targets.push_back(personalDeletes[i]);
            targets.insert(targets.end(),hintAnchors.begin(),hintAnchors.begin()+hintSectionCount);
            const int previousRow=notesScrollRow;
            if(direction) notesScrollRow+=direction==2 ? 1 : -1;
            notesScrollRow=std::clamp(notesScrollRow,0,static_cast<int>(targets.size())-1);
            // The host refuses focus while a text-entry dialog is on top.
            if(svc_ui->elem_focus(mod_ctx,targets[notesScrollRow])!=MOD_OK) { notesScrollRow=previousRow; notesNavigation={}; }
        }
    } else notesNavigation={};
    if (renderedRevision==revision) return MOD_OK;
    const auto content=notesRml();
    ModResult result=MOD_OK;
    hintSectionCount=0;
    for(size_t start=0;start<content.size();) {
        auto end=content.find("</div>",start);
        end=end==std::string::npos ? content.size() : end+6;
        if(hintSectionCount==hintSections.size()) {
            UiElementHandle anchor=0,section=0;
            UiControlDesc control=UI_CONTROL_DESC_INIT;
            control.kind=UI_CONTROL_BUTTON; control.label="";
            control.on_pressed=[](ModContext*,void*) {};
            result=svc_ui->pane_add_control(mod_ctx,notesElement,&control,&anchor);
            if(result!=MOD_OK) return result;
            svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
            result=svc_ui->pane_add_rml(mod_ctx,notesElement,"",&section);
            if(result!=MOD_OK) return result;
            hintAnchors.push_back(anchor); hintSections.push_back(section);
        }
        svc_ui->elem_set_rml(mod_ctx,hintSections[hintSectionCount],content.substr(start,end-start).c_str());
        ++hintSectionCount; start=end;
    }
    for(size_t i=0;i<hintSections.size();++i) {
        svc_ui->elem_set_visible(mod_ctx,hintSections[i],i<hintSectionCount);
        svc_ui->elem_set_visible(mod_ctx,hintAnchors[i],i<hintSectionCount);
    }
    for (size_t i=0;i<personalCards.size();++i) {
        const bool visible=i<personalNotes.size();
        svc_ui->elem_set_visible(mod_ctx,personalCards[i],visible);
        svc_ui->elem_set_visible(mod_ctx,personalDeletes[i],visible);
        if (visible) svc_ui->elem_set_text(mod_ctx,personalCards[i],personalNotes[i].c_str());
    }
    if (inventoryElement) svc_ui->elem_set_rml(mod_ctx,inventoryElement,inventoryRml().c_str());
    renderedRevision=revision;
    return result;
}
ModResult buildNotes(ModContext*,UiWindowHandle,UiElementHandle left,UiElementHandle right,void*,ModError*) {
    std::swap(left,right);
    notesTab=true; pageControl=checksElement=countElement=0;
    personalCards.clear(); personalDeletes.clear(); hintSections.clear(); hintAnchors.clear(); hintSectionCount=0; notesScrollRow=0; notesNavigation={};
    checkRows.clear(); headingRows.clear(); visibleNames.clear(); visibleRows=0;
    stickRow=-1; stickNavigation={}; renderedRevision=0;
    auto result=svc_ui->pane_add_rml(mod_ctx,left,"",&inventoryElement);
    if (result!=MOD_OK) return result;
    result=svc_ui->pane_add_rml(mod_ctx,right,"<div class='tp-title'>My notes</div><div class='tp-note'>Right stick: up/down scrolls notes and read hints. Type a reminder and press Enter to post it below. Select New note to write another. Each card has its own Delete button. Up to 64 notes, 1,000 characters each (16,000 total).</div>",nullptr);
    if (result!=MOD_OK) return result;
    UiControlDesc control=UI_CONTROL_DESC_INIT;
    control.kind=UI_CONTROL_STRING; control.label="New note - Enter to add";
    control.max_length=1000; control.get=manualGet; control.set=manualSet;
    control.string_set_mode=UI_STRING_SET_ON_COMMIT;
    result=svc_ui->pane_add_control(mod_ctx,right,&control,&newNoteControl);
    if (result!=MOD_OK) return result;
    for (size_t i=0;i<64;++i) {
        UiElementHandle card=0,button=0;
        result=svc_ui->pane_add_text(mod_ctx,right,"",&card);
        if (result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,card,"tp-postit",true);
        personalCards.push_back(card);
        control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_BUTTON;
        control.label="Delete note"; control.on_pressed=deleteNote; control.user_data=reinterpret_cast<void*>(i);
        result=svc_ui->pane_add_control(mod_ctx,right,&control,&button);
        if (result!=MOD_OK) return result;
        personalDeletes.push_back(button);
    }
    UiRowDesc row=UI_ROW_DESC_INIT;
    result=svc_ui->pane_add_row(mod_ctx,right,&row,&notesElement);
    svc_ui->elem_set_class(mod_ctx,notesElement,"tp-detail-sections",true);
    return result==MOD_OK ? updateNotes(mod_ctx,nullptr,nullptr) : result;
}
void onClosed(ModContext*, UiWindowHandle, void*) { window = 0; inventoryElement = checksElement = notesElement = 0; notesTab=false; }
void toggleGet(ModContext*, void*, UiControlValue* value) { value->bool_value = window != 0; }
void toggleSet(ModContext*, void*, const UiControlValue* value) {
    if (!value->bool_value && window) { if(detailWindow) svc_ui->window_close(mod_ctx,detailWindow); svc_ui->window_close(mod_ctx, window); return; }
    if (!value->bool_value || window) return;
    UiTabDesc tabs[3] = {UI_TAB_DESC_INIT, UI_TAB_DESC_INIT, UI_TAB_DESC_INIT};
    tabs[0].title = "Current Area";
    tabs[1].title = "All Checks";
    static int allTab;
    for (auto& tab : tabs) { tab.build = buildTab; tab.update = updateTab; }
    tabs[1].user_data = &allTab;
    tabs[2].title="Notes"; tabs[2].build=buildNotes; tabs[2].update=updateNotes;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = tabs; desc.tab_count = 3; desc.rcss = styles; desc.on_closed = onClosed;
    auto result = svc_ui->window_push(mod_ctx, &desc, &window);
    if (result != MOD_OK) { window = 0; status = "Could not open tracker window."; mods::log::error("TPTracker: window_push failed"); }
}
void openTracker(ModContext*, void*) {
    UiControlValue value{}; value.bool_value=true; toggleSet(mod_ctx,nullptr,&value);
}
void cancelBinding(ModContext*, UiDialogHandle, void*) {
    capturingDevice=-1; bindingDialog=0; shortcutHeld=true;
}
void dismissBinding(ModContext*, UiDialogHandle, void*) {
    // The host treats B as Back. Capture it here before the prompt is dismissed.
    if (capturingDevice==1 && captureReady) {
        const int pressed=tracker::pressedBinding(true);
        if (pressed) svc_config->set_int(mod_ctx,controllerBinding,pressed);
    }
    cancelBinding(mod_ctx,0,nullptr);
}
void assignBinding(ModContext*,void* data) {
    capturingDevice=static_cast<int>(reinterpret_cast<uintptr_t>(data)); captureReady=false;
    UiDialogAction action=UI_DIALOG_ACTION_INIT;
    action.label="Cancel"; action.on_pressed=cancelBinding;
    UiDialogDesc dialog=UI_DIALOG_DESC_INIT;
    dialog.title="Assign TPTracker shortcut";
    dialog.body_rml=capturingDevice ? "Release the current button, then press any controller button or trigger." : "Release the current key, then press any keyboard key. Escape cancels.";
    dialog.actions=&action; dialog.action_count=1; dialog.on_dismiss=dismissBinding;
    if (svc_ui->dialog_push(mod_ctx,&dialog,&bindingDialog)!=MOD_OK) cancelBinding(mod_ctx,0,nullptr);
}
void clearBinding(ModContext*,void* data) {
    svc_config->set_int(mod_ctx,reinterpret_cast<uintptr_t>(data) ? controllerBinding : keyboardBinding,0);
    shortcutHeld=true;
}
void pollShortcut() {
    if (capturingDevice>=0) {
        const int pressed=tracker::pressedBinding(capturingDevice!=0);
        if (!pressed) captureReady=true;
        if (pressed && captureReady) {
            if (capturingDevice!=0 || !tracker::escapeBinding(pressed))
                svc_config->set_int(mod_ctx,capturingDevice ? controllerBinding : keyboardBinding,pressed);
            const auto dialog=bindingDialog;
            cancelBinding(mod_ctx,0,nullptr);
            if (dialog) svc_ui->dialog_close(mod_ctx,dialog);
        }
        return;
    }
    int64_t key=0,pad=0;
    svc_config->get_int(mod_ctx,keyboardBinding,&key);
    svc_config->get_int(mod_ctx,controllerBinding,&pad);
    const bool down=tracker::shortcutDown(notesTab && window ? 0 : static_cast<int>(key),static_cast<int>(pad));
    // A newly selected binding must be released before it can toggle the window.
    if (key!=previousKeyboard || pad!=previousController) {
        previousKeyboard=key; previousController=pad; shortcutHeld=true;
    }
    if (down && !shortcutHeld) {
        UiControlValue value{}; value.bool_value=!window; toggleSet(mod_ctx,nullptr,&value);
    }
    shortcutHeld=down;
}
void filePicked(ModContext*, ModResult result, const char* const* locations, uint32_t count, const char*, void*) {
    if (result != MOD_OK || count != 1) return;
    ++revision;
    FileStreamHandle stream = 0;
    uint64_t size = 0, read = 0;
    if (svc_file->open(mod_ctx, locations[0], FILE_OPEN_READ, &stream) != MOD_OK) { status = "Cannot open seed file."; return; }
    result = svc_file->size(mod_ctx, stream, &size);
    if (result != MOD_OK || size > 2 * 1024 * 1024) { svc_file->close(mod_ctx, stream); status = "Seed file is unreadable or exceeds 2 MiB."; return; }
    std::string content(static_cast<size_t>(size), '\0');
    result = svc_file->read(mod_ctx, stream, content.data(), size, &read);
    auto closed = svc_file->close(mod_ctx, stream);
    try {
        if (result != MOD_OK || closed != MOD_OK || read != size) throw std::runtime_error("Incomplete seed read");
        model.loadSeed(content);
        status = "Seed: " + model.seed;
        persist(); dirty = true;
    } catch (const std::exception& e) { status = e.what(); mods::log::warn("TPTracker: {}", e.what()); }
}
void pickSeed(ModContext*, void*) {
    ++revision;
    static const FileFilter filters[] = {{"Dusklight anti-spoiler log / TPTracker JSON", "txt;json"}};
    FilePickOptions options = FILE_PICK_OPTIONS_INIT;
    options.filters = filters; options.filter_count = 1;
    if (svc_file->pick_file(mod_ctx, &options, filePicked, nullptr) != MOD_OK) status = "Could not open the file picker.";
}
void searchGet(ModContext*, void*, UiControlValue* value) { value->string_value = search.c_str(); }
void searchSet(ModContext*, void*, const UiControlValue* value) { search = value->string_value ? value->string_value : ""; ++revision; }
void hideGet(ModContext*, void*, UiControlValue* value) { value->bool_value = hideCompleted; }
void hideSet(ModContext*, void*, const UiControlValue* value) { hideCompleted = value->bool_value; ++revision; }
void mapGet(ModContext*, void*, UiControlValue* value) { value->bool_value = tracker::mapAvailable && tracker::mapEnabled; }
void mapSet(ModContext*, void*, const UiControlValue* value) { tracker::mapEnabled = tracker::mapAvailable && value->bool_value; }
void miniGet(ModContext*,void*,UiControlValue* v) { v->bool_value=tracker::minimapEnabled; }
void miniSet(ModContext*,void*,const UiControlValue* v) { tracker::minimapEnabled=tracker::minimapAvailable && v->bool_value; }
void typeGet(ModContext*,void* ptr,UiControlValue* v) { v->bool_value=*static_cast<bool*>(ptr); }
void typeSet(ModContext*,void* ptr,const UiControlValue* v) { *static_cast<bool*>(ptr)=v->bool_value; }
ModResult buildSettings(ModContext*, UiElementHandle pane, void*, ModError*) {
    settingsStatus = 0;
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE; control.label = "TPTracker"; control.get = toggleGet; control.set = toggleSet;
    auto result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    for (int device=0;device<2;++device) {
        control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_BUTTON;
        control.label=device ? "Click any button on controller to assign" : "Click any key on keyboard to assign";
        control.on_pressed=assignBinding; control.user_data=reinterpret_cast<void*>(static_cast<uintptr_t>(device));
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,&bindingButtons[device]);
        if (result!=MOD_OK) return result;
        control.label=device ? "Clear controller binding" : "Clear keyboard binding";
        control.on_pressed=clearBinding;
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
        if (result!=MOD_OK) return result;
    }
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_BUTTON; control.label = "Load this save's seed log / JSON"; control.on_pressed = pickSeed;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_STRING; control.label = "Filter checks"; control.get = searchGet; control.set = searchSet; control.max_length = 128;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE; control.label = "Hide completed checks"; control.get = hideGet; control.set = hideSet;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE; control.label = tracker::mapAvailable ? "Native map check markers" : "Map markers unavailable on this build"; control.get = mapGet; control.set = mapSet;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind=UI_CONTROL_TOGGLE; control.label=tracker::minimapAvailable ? "Minimap check dots" : "Minimap hook unavailable on this build"; control.get=miniGet; control.set=miniSet;
    result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
    if (result!=MOD_OK) return result;
    static const char* types[]={"Treasure chests","NPC / event rewards","Golden bugs","Poe souls","Golden wolves","Shop items","Owl statues","Grotto checks"};
    for (int surface=0;surface<2;++surface) for (int i=0;i<tracker::checkTypeCount;++i) {
        std::string label=std::string(surface ? "Minimap: " : "Map: ")+types[i];
        control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_TOGGLE; control.label=label.c_str();
        control.get=typeGet; control.set=typeSet; control.user_data=surface ? &tracker::minimapTypes[i] : &tracker::mapTypes[i];
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
        if (result!=MOD_OK) return result;
    }
    result = svc_ui->pane_add_text(mod_ctx, pane, status.c_str(), &settingsStatus);
    if (result != MOD_OK) return result;
    return svc_ui->pane_add_text(mod_ctx, pane,
        "Native RmlUi tracker. The current SDK window captures game input and cannot detach onto another monitor. Seed settings are stored per save slot.", nullptr);
}
ModResult updateSettings(ModContext*, void*, ModError*) {
    for (int device=0;device<2;++device) {
        int64_t value=0; svc_config->get_int(mod_ctx,device ? controllerBinding : keyboardBinding,&value);
        const auto label=std::string(device ? "Click any button on controller to assign: " : "Click any key on keyboard to assign: ")+tracker::bindingName(static_cast<int>(value),device!=0);
        if (bindingButtons[device]) svc_ui->control_set_label(mod_ctx,bindingButtons[device],label.c_str());
    }
    return settingsStatus ? svc_ui->elem_set_text(mod_ctx, settingsStatus, status.c_str()) : MOD_OK;
}
}

DEFINE_HOOK(&dSv_info_c::onSwitch, SwitchHook);
DEFINE_HOOK(&dComIfGs_onTbox, ChestHook);
DEFINE_HOOK(&dMsgObject_c::_draw, HintDialogueDraw);

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx, "catalogue.json", &buffer) != MOD_OK)
        return mods::set_error(error, MOD_ERROR, "TPTracker catalogue is missing");
    try { model.load(Json::parse(static_cast<const char*>(buffer.data), static_cast<const char*>(buffer.data) + buffer.size)); }
    catch (const std::exception& e) { svc_resource->free(mod_ctx, &buffer); return mods::set_error(error, MOD_ERROR, e.what()); }
    svc_resource->free(mod_ctx, &buffer);
    buffer=RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx,"hint_signs.json",&buffer)!=MOD_OK)
        return mods::set_error(error,MOD_ERROR,"TPTracker hint sign catalogue is missing");
    try { hintSigns=Json::parse(static_cast<const char*>(buffer.data),static_cast<const char*>(buffer.data)+buffer.size); }
    catch (const std::exception& e) { svc_resource->free(mod_ctx,&buffer); return mods::set_error(error,MOD_ERROR,e.what()); }
    svc_resource->free(mod_ctx,&buffer);
    buffer=RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx,"inventory.json",&buffer)!=MOD_OK)
        return mods::set_error(error,MOD_ERROR,"TPTracker inventory artwork catalogue is missing");
    try { inventoryArt=Json::parse(static_cast<const char*>(buffer.data),static_cast<const char*>(buffer.data)+buffer.size); }
    catch (const std::exception& e) { svc_resource->free(mod_ctx,&buffer); return mods::set_error(error,MOD_ERROR,e.what()); }
    svc_resource->free(mod_ctx,&buffer);
    auto hintResult=mods::hook::add_post<HintDialogueDraw>(onDialogueDraw);
    if (hintResult!=MOD_OK) return mods::set_error(error,hintResult,"TPTracker hint dialogue hook failed");
    auto result = svc_item->observe_gives(mod_ctx, onGive, nullptr, &itemObserver);
    if (result != MOD_OK) return mods::set_error(error, result, "TPTracker item observer failed");
    result = svc_save->observe_saves(mod_ctx, onNewSave, onSaveLoaded, onSaveWritten, nullptr, &saveObserver);
    if (result != MOD_OK) return mods::set_error(error, result, "TPTracker save observer failed");
    result = mods::hook::add_post<SwitchHook>(onEngineChanged);
    if (result != MOD_OK) return mods::set_error(error, result, "TPTracker switch hook failed");
    result = mods::hook::add_post<ChestHook>(onEngineChanged);
    if (result != MOD_OK) return mods::set_error(error, result, "TPTracker chest hook failed");
    result = tracker::initializeMap(&model);
    if (result != MOD_OK) mods::log::warn("TPTracker: dMenu_Fmap2DBack_c::draw hook failed ({}); checklist remains available", static_cast<int>(result));
    else mods::log::info("TPTracker: native map draw hook installed");
    mods::log::info("TPTracker: minimap hooks {}", tracker::minimapAvailable ? "installed" : "unavailable");
    result = tracker::initializeInput();
    if (result != MOD_OK) return mods::set_error(error, result, "TPTracker input event hook failed");
    restore();
    for (auto entry : {std::pair{"tracker-keyboard", &keyboardBinding}, std::pair{"tracker-controller", &controllerBinding}}) {
        ConfigVarDesc config=CONFIG_VAR_DESC_INIT;
        config.name=entry.first; config.type=CONFIG_VAR_INT; config.default_int=0;
        result=svc_config->register_var(mod_ctx,&config,entry.second);
        if (result!=MOD_OK) return result;
    }
    UiMenuTabDesc tab=UI_MENU_TAB_DESC_INIT;
    tab.label="TPTracker"; tab.on_selected=openTracker;
    result=svc_ui->register_menu_tab(mod_ctx,&tab,&menuTab);
    if (result!=MOD_OK) return result;
    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = buildSettings; panel.update = updateSettings;
    return svc_ui->register_mods_panel(mod_ctx, &panel);
}
MOD_EXPORT ModResult mod_update(ModError*) {
    pollShortcut();
    if (seedRefreshPending) {
        // The sidecar is flushed after save observers return. Read it here, and
        // retain the live completion journal rather than restoring an older one.
        seedRefreshPending = false;
        backupCard();
        const char* dataPath = nullptr;
        std::string error;
        if (svc_host->data_dir(mod_ctx, &dataPath) == MOD_OK && dataPath &&
            tracker::loadSelectedSeed(model, std::filesystem::u8path(dataPath).parent_path(), error)) {
            status = "Seed: " + model.seed + " (selected save)";
            dirty = true; ++revision;
        }
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= nextScan) {
        if (notebookDirty) { persist(); notebookDirty=false; }
        nextScan = now + std::chrono::milliseconds(engineDirty ? 250 : 500);
        engineDirty = false;
        scan();
    }
    updateLogic();
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    if(detailWindow) svc_ui->window_close(mod_ctx,detailWindow);
    if (menuTab) svc_ui->unregister_menu_tab(mod_ctx,menuTab);
    menuTab=0;
    tracker::shutdownMap();
    tracker::resetInput();
    if (logicJob.valid()) logicJob.wait();
    if (window) svc_ui->window_close(mod_ctx, window);
    if (itemObserver) svc_item->unobserve_gives(mod_ctx, itemObserver);
    if (saveObserver) svc_save->unobserve_saves(mod_ctx, saveObserver);
    window = 0; inventoryElement = checksElement = 0; itemObserver = 0; saveObserver = 0;
    // The host removes this mod's HookService registrations during detach.
    return MOD_OK;
}
}
