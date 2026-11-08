#include "inventory_adapters.hpp"
#include "model.hpp"
#include "vanilla.hpp"
#include "entrance_journal.hpp"
#include "notebook.hpp"
#include "check_guides.hpp"
#include "seed.hpp"
#include "map.hpp"
#include "preferences.hpp"
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
tracker::EntranceJourney entranceJourney;
tracker::CheckGuides checkGuides;
UiWindowHandle window = 0;
UiWindowHandle detailWindow = 0, itemWindow=0;
std::string selectedItem;
bool detailAdvanced=false;
UiElementHandle detailToggle=0;
bool mapDetail=false, mapDetailClosing=false;
UiElementHandle detailText = 0, detailButton = 0;
std::string selectedCheck;
std::vector<std::string> detailChecks;
std::string detailMarkup;
uint64_t detailRevision=0;
std::vector<UiElementHandle> detailSections, detailAnchors, detailBlocks;
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
ConfigVarHandle shortcutBindings[4]{};
int areaSort = 1;
UiDialogHandle bindingDialog=0;
int capturingBinding=-1;
bool captureReady=false;
UiElementHandle bindingButtons[4]{};
bool shortcutHeld = true;
int64_t previousKeyboard = -1, previousController = -1;
UiElementHandle inventoryElement = 0, checksElement = 0;
std::vector<UiElementHandle> inventorySections, inventoryBlocks;
std::vector<std::string> inventoryNames;
UiElementHandle inventoryHeader=0, chooseLogButton=0;
UiElementHandle settingsStatus = 0, countElement = 0;
constexpr size_t pageSize = 10;
size_t checkPage = 0, totalPages = 1, visibleRows = 0;
bool restorePageFocus = false;
UiElementHandle pageControl = 0;
std::vector<UiElementHandle> checkRows, headingRows;
std::vector<std::string> visibleNames;
Json hintSigns=Json::array(), hintNotes=Json::object();
std::string manualNotes;
std::vector<std::string> personalNotes;
std::vector<UiElementHandle> personalCards, personalDeletes;
Json inventoryArt=Json::object();
UiElementHandle notesElement=0, newNoteControl=0;
std::vector<UiElementHandle> hintSections, hintAnchors, hintBlocks;
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
int seedSlot = -1;
bool savedSlot = false, needsSeedLog = false;
bool waitingForNativeSeed = false;
std::chrono::steady_clock::time_point nextSeedProbe{};
uintptr_t saveGeneration = 0;
std::filesystem::path activeSeedFolder;
void openLogPicker(bool guestSetup);
int statusFilters[2]{};
bool showHintSigns[2]{true,true};
tracker::PreferenceStore preferences;
uint64_t revision = 1, renderedRevision = 0;
void previousPage(ModContext*,void*) { if (checkPage) { --checkPage; ++revision; } }
void nextPage(ModContext*,void*) { if (checkPage+1<totalPages) { ++checkPage; ++revision; } }
bool previousDisabled(ModContext*,void*) { return checkPage==0; }
bool nextDisabled(ModContext*,void*) { return checkPage+1>=totalPages; }
bool rowDisabled(ModContext*,void* data) { return reinterpret_cast<uintptr_t>(data)>=visibleRows; }
void pageGet(ModContext*,void*,UiControlValue* v) { v->int_value=checkPage+1; }
void pageSet(ModContext*,void*,const UiControlValue* v) {
    size_t next=static_cast<size_t>(std::clamp<int64_t>(v->int_value,1,static_cast<int64_t>(totalPages)))-1;
    if (next!=checkPage) { checkPage=next; ++revision; }
}
uint64_t logicGeneration = 0, runningGeneration = 0;
bool logicRequested = false;
std::future<tracker::LogicResult> logicJob;
std::string search;
std::string status = "Select this save's spoiler log in F1 > Mods > TPTracker.";
std::string lastChecks;
std::set<std::string> journal;
std::chrono::steady_clock::time_point nextScan{};
constexpr const char* blobName = "tracker-v1";

const char* styles = R"(
window { max-width: 1120dp; max-height: 800dp; background-color: #211e15; border-color: #9b8955; }
window content pane { font-size: 17dp; }
window content pane:first-child { flex: 0 0 32%; background-color: #191a15; }
window content pane:last-child { flex: 1; background-color: #302c20; }
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
/* Keep scroll targets focusable without drawing the host button highlight. */
window content pane .tp-scroll-anchor,
window content pane .tp-scroll-anchor:hover,
window content pane .tp-scroll-anchor:focus,
window content pane .tp-scroll-anchor:focus-visible,
window content pane .tp-scroll-anchor:selected,
window content pane .tp-scroll-anchor:active {
    opacity: 0; background-color: transparent; box-shadow: none; transition: none;
}
.tp-postit { display: block; background-color: #514629; color: #fff0b7; border-left: 4dp #d2b04f; padding: 12dp; margin-top: 10dp; font-size: 17dp; }
.tp-item img { display: block; width: 48dp; height: 48dp; margin: 0dp auto 5dp auto; }
.tp-item { text-align: center; font-size: 13dp; min-height: 92dp; }
.tp-item.missing img { opacity: 0.35; }
.tp-item-count { display: block; color: #d2b866; margin-top: 4dp; }
.tp-inventory-row { align-items: stretch; gap: 5dp; }
.tp-inventory-row .tp-item { position: relative; display: block; flex: 1 1 0dp; width: 0dp; margin: 2dp 0dp; }
.tp-item-art { pointer-events: none; display: block; width: 100%; }
window content pane .tp-item-action { box-sizing: border-box; position: absolute; left: 0dp; top: 0dp; width: 100%; height: 100%; min-height: 0dp; margin: 0dp; padding: 0dp; color: transparent; background-color: transparent; border: 2dp transparent; box-shadow: none; }
window content pane .tp-item-action:hover, window content pane .tp-item-action:focus, window content pane .tp-item-action:focus-visible, window content pane .tp-item-action:selected, window content pane .tp-item-action:active { color: transparent; background-color: transparent; border: 2dp #dcc44b; }
)";

std::string journalText() {
    Json value = {{"version", 1}, {"seed", model.seed}, {"settings", model.settings},
                  {"checks", journal}};
    value["mode"]=model.vanilla ? "vanilla" : "randomizer";
    value["entrance_visits"]=Json::array();
    for(const auto& id:model.visitedEntrances) value["entrance_visits"].push_back(tracker::entranceKey(id));
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
    if(detailButton) {
        const bool skipped=model.skipped.contains(selectedCheck);
        svc_ui->control_set_label(mod_ctx,detailButton,skipped ? "Skipped" : "Skip this check");
        svc_ui->control_set_tooltip(mod_ctx,detailButton,skipped ? "Undo skipped check?" : "Hide this check from the maps until restored.");
    }
    if(detailToggle) svc_ui->control_set_label(mod_ctx,detailToggle,detailAdvanced ? "Hide full logic" : "Show full logic");
    const auto markup=model.checkDetails(selectedCheck,detailAdvanced);
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
            UiElementHandle section=0,anchor=0,block=0;
            UiRowDesc row=UI_ROW_DESC_INIT;
            auto result=svc_ui->pane_add_row(mod_ctx,detailText,&row,&block);
            if(result!=MOD_OK) return result;
            svc_ui->elem_set_class(mod_ctx,block,"tp-detail-sections",true);
            detailBlocks.push_back(block);
            UiControlDesc control=UI_CONTROL_DESC_INIT;
            control.kind=UI_CONTROL_BUTTON; control.label="";
            control.on_pressed=[](ModContext*,void*) {};
            result=svc_ui->pane_add_control(mod_ctx,block,&control,&anchor);
            if(result!=MOD_OK) return result;
            svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
            result=svc_ui->pane_add_rml(mod_ctx,block,"",&section);
            if(result!=MOD_OK) return result;
            detailAnchors.push_back(anchor); detailSections.push_back(section);
        }
        svc_ui->elem_set_rml(mod_ctx,detailSections[detailSectionCount],part.c_str());
        ++detailSectionCount; start=end;
    }
    for(size_t i=0;i<detailSections.size();++i) {
        svc_ui->elem_set_visible(mod_ctx,detailBlocks[i],i<detailSectionCount);
    }
    return MOD_OK;
}
ModResult buildDetail(ModContext*,UiWindowHandle,UiElementHandle left,UiElementHandle right,void* data,ModError*) {
    const auto index=reinterpret_cast<uintptr_t>(data);
    if(index>=detailChecks.size()) return MOD_INVALID_ARGUMENT;
    selectedCheck=detailChecks[index];
    detailText=detailButton=detailToggle=0; detailAdvanced=false; detailMarkup.clear(); detailRevision=0;
    detailSections.clear(); detailAnchors.clear(); detailBlocks.clear(); detailSectionCount=0;
    detailScrollRow=0; detailNavigation={};
    UiRowDesc sections=UI_ROW_DESC_INIT;
    auto result=svc_ui->pane_add_row(mod_ctx,left,&sections,&detailText);
    if(result!=MOD_OK) return result;
    svc_ui->elem_set_class(mod_ctx,detailText,"tp-detail-sections",true);
    result=updateDetail(nullptr,nullptr,nullptr);
    if(result!=MOD_OK) return result;
    UiControlDesc advanced=UI_CONTROL_DESC_INIT;
    advanced.kind=UI_CONTROL_BUTTON; advanced.label="Show full logic";
    advanced.on_pressed=[](ModContext*,void*) { detailAdvanced=!detailAdvanced; ++revision; };
    result=svc_ui->pane_add_control(mod_ctx,left,&advanced,&detailToggle);
    if(result!=MOD_OK) return result;
    UiControlDesc button=UI_CONTROL_DESC_INIT;
    button.kind=UI_CONTROL_BUTTON; button.label="Skip this check";
    button.on_pressed=skipSelectedCheck; button.is_disabled=skipSelectedDisabled;
    result=svc_ui->pane_add_control(mod_ctx,left,&button,&detailButton);
    if(result!=MOD_OK) return result;
    // Direct-pane rows let native controller focus scroll each guide paragraph
    // into view, just like the requirements on the left. Text is escaped RML.
    for(const auto& markup : checkGuides.blocks(selectedCheck)) {
        UiElementHandle block=0,anchor=0;
        UiRowDesc row=UI_ROW_DESC_INIT;
        result=svc_ui->pane_add_row(mod_ctx,right,&row,&block);
        if(result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,block,"tp-detail-sections",true);
        UiControlDesc control=UI_CONTROL_DESC_INIT;
        control.kind=UI_CONTROL_BUTTON; control.label="";
        control.on_pressed=[](ModContext*,void*) {};
        result=svc_ui->pane_add_control(mod_ctx,block,&control,&anchor);
        if(result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
        result=svc_ui->pane_add_rml(mod_ctx,block,markup.c_str(),nullptr);
        if(result!=MOD_OK) return result;
    }
    detailRevision=0;
    return updateDetail(nullptr,nullptr,nullptr);
}
void detailClosed(ModContext*,UiWindowHandle,void*) { mapDetail=mapDetailClosing=false; detailWindow=0; detailText=detailButton=detailToggle=0; detailAdvanced=false; selectedCheck.clear(); detailChecks.clear(); detailMarkup.clear(); detailRevision=0; detailSections.clear(); detailAnchors.clear(); detailBlocks.clear(); detailSectionCount=0; detailScrollRow=0; detailNavigation={}; stickNavigation={}; }
bool openCheckDetails(const std::vector<std::string>& names) {
    if(detailWindow || names.empty()) return false;
    detailChecks=names;
    std::erase_if(detailChecks,[](const auto& name) { return !model.aliases.contains(name); });
    if(detailChecks.empty()) return false;
    std::vector<UiTabDesc> tabs(detailChecks.size(),UI_TAB_DESC_INIT);
    for(size_t i=0;i<tabs.size();++i) {
        tabs[i].title=tabs.size()==1 ? "Check details" : detailChecks[i].c_str();
        tabs[i].build=buildDetail; tabs[i].update=updateDetail; tabs[i].user_data=reinterpret_cast<void*>(i);
    }
    UiWindowDesc desc=UI_WINDOW_DESC_INIT; desc.tabs=tabs.data(); desc.tab_count=tabs.size(); desc.on_closed=detailClosed;
    desc.rcss=R"(
window { max-width: 1060dp; max-height: 800dp; }
.tp-state { padding: 8dp; background-color: #302d20; border-radius: 5dp; }
.OPEN { color: #70ed86; } .LOCKED { color: #ff7c76; } .UNKNOWN { color: #e5c977; }
.DONE { color: #999b95; } .SKIPPED { color: #cd91f4; }
window content pane:first-child { flex: 1; }
window content pane:last-child { flex: 0 0 38%; background-color: #211f17; }
window content pane { font-size: 17dp; }
.tp-detail-sections { display: block; }
.tp-scroll-anchor { display: block; height: 1dp; min-height: 1dp; padding: 0dp; margin: 0dp; border-width: 0dp; opacity: 0; }
/* Keep scroll targets focusable without drawing the host button highlight. */
window content pane .tp-scroll-anchor,
window content pane .tp-scroll-anchor:hover,
window content pane .tp-scroll-anchor:focus,
window content pane .tp-scroll-anchor:focus-visible,
window content pane .tp-scroll-anchor:selected,
window content pane .tp-scroll-anchor:active {
    opacity: 0; background-color: transparent; box-shadow: none; transition: none;
}
window content pane div { display: block; margin-bottom: 4dp; }
.tp-title { display: block; font-size: 24dp; font-weight: bold; color: #e4d196; margin-bottom: 14dp; }
.tp-group { display: block; font-size: 17dp; color: #e4d196; margin-top: 12dp; margin-bottom: 6dp; padding-bottom: 4dp; border-bottom: 1dp #9b8955; }
.tp-note { display: block; font-size: 14dp; color: #c5baa0; margin-top: 6dp; margin-bottom: 8dp; }
)";
    if(svc_ui->window_push(mod_ctx,&desc,&detailWindow)!=MOD_OK) detailClosed(nullptr,0,nullptr);
    return detailWindow!=0;
}
void inspectCheck(ModContext*, void* data) {
    stickRow=static_cast<int>(reinterpret_cast<uintptr_t>(data));
    if(stickRow>=0 && static_cast<size_t>(stickRow)<visibleNames.size()) openCheckDetails({visibleNames[stickRow]});
}
bool inspectMapCheck(const std::vector<std::string>& names) {
    // Push only Check Details. With no tracker window underneath, the host's
    // normal B/Esc close returns directly to the in-game map in one press.
    bool visible=true;
    if(window || detailWindow || capturingBinding>=0 ||
       svc_ui->is_any_document_visible(mod_ctx,&visible)!=MOD_OK || visible) return false;
    shortcutHeld=true;
    mapDetail=openCheckDetails(names);
    return mapDetail;
}
bool closeMapCheck() {
    if(!mapDetail || !detailWindow) return false;
    if(!mapDetailClosing) {
        mapDetailClosing=true;
        svc_ui->window_close(mod_ctx,detailWindow);
    }
    return true;
}

void backupCard() {
    const char* path = nullptr;
    std::string error;
    if (svc_host->data_dir(mod_ctx,&path) == MOD_OK && path &&
        !tracker::backupSelectedCard(std::filesystem::u8path(path),error) && !error.empty())
        mods::log::warn("TPTracker: card backup failed: {}",error);
}
void refreshSelectedSeed() {
    const char* dataPath = nullptr;
    ++revision;
    if(tracker::normalPlayActive()) {
        if(!model.vanilla) { model.loadVanilla(); ++logicGeneration; dirty=true; }
        needsSeedLog=waitingForNativeSeed=false; activeSeedFolder.clear();
        status="Normal Play: tracking inventory, collected checks and story progress.";
        return;
    }
    status = model.seedLoaded ? "Seed: " + model.seed :
        "Save the randomizer game once to identify its seed, or load a seed log in Mods > TPTracker.";
    if (!savedSlot || svc_host->data_dir(mod_ctx, &dataPath) != MOD_OK || !dataPath) return;
    try {
        if (seedSlot < 0 && dComIfGp_getPlayer(0)) seedSlot = dComIfGs_getDataNum();
        auto folder = tracker::selectedSeedFolder(std::filesystem::u8path(dataPath).parent_path(), seedSlot);
        if (folder.empty()) return;
        if (folder != activeSeedFolder) {
            activeSeedFolder = folder;
            needsSeedLog = false;
            ++saveGeneration;
        }
        const auto hash = folder.filename().string();
        if (model.seedLoaded && model.seed != hash) {
            // The journal belongs to another seed. Let the next scan rebuild progress
            // from this save instead of solving the host's world with old settings.
            model.resetSave(); journal.clear(); entranceJourney.reset();
            hintNotes = Json::object(); manualNotes.clear(); personalNotes.clear(); notebookDirty = false;
            ++logicGeneration; dirty = true;
        }
        waitingForNativeSeed = !std::filesystem::exists(folder / "seed.dat");
        if (waitingForNativeSeed) {
            needsSeedLog = false;
            status = "Waiting for seed.dat for the active seed, " + hash + ".";
            nextSeedProbe = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            return;
        }
        needsSeedLog = true;
        needsSeedLog = !tracker::loadSeedFolder(model, folder);
        status = needsSeedLog ? "Seed data found for " + hash + ". Choose the host's matching Anti-Spoiler Log to show reachable checks."
                              : "Seed: " + hash + " (selected save)";
        dirty = true;
    } catch (const std::exception& e) {
        if (!activeSeedFolder.empty()) status = e.what();
        mods::log::warn("TPTracker: selected save seed metadata unavailable: {}", e.what());
    }
}

void restore(int slot, bool autoSeed = true) {
    if(itemWindow) svc_ui->window_close(mod_ctx,itemWindow);
    if(detailWindow) svc_ui->window_close(mod_ctx,detailWindow);
    backupCard();
    model.resetSave(); entranceJourney.reset();
    seedSlot = slot; savedSlot = autoSeed; seedRefreshPending = false;
    activeSeedFolder.clear(); needsSeedLog = waitingForNativeSeed = false; ++saveGeneration;
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
                if(value.contains("mode") && value.at("mode")!=(tracker::normalPlayActive() ? "vanilla" : "randomizer"))
                    throw std::runtime_error("Tracker journal belongs to another play mode");
                if (!tracker::normalPlayActive() && !value.value("seed", std::string{}).empty()) model.loadSeed(text);
                for (const auto& name : value.at("checks"))
                    if (model.aliases.contains(name.get<std::string>())) journal.insert(name.get<std::string>());
                model.obtained = journal;
                for(const auto& key:value.value("entrance_visits",Json::array()))
                    for(const auto& edge:model.catalogue.at("entrance_edges")) {
                        const auto id=edge.at("id").get<std::string>();
                        if(key==tracker::entranceKey(id)) model.visitedEntrances.insert(id);
                    }
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
    refreshSelectedSeed();
    dirty = true;
    nextScan = {};
    lastChecks.clear();
}
void onSaveLoaded(ModContext*, uint32_t slot, void*) { restore(static_cast<int>(slot)); }
void onNewSave(ModContext*, uint32_t slot, void*) { restore(static_cast<int>(slot), false); }
void onSaveWritten(ModContext*, uint32_t slot, void*) {
    if (!savedSlot || seedSlot != static_cast<int>(slot)) {
        ++saveGeneration; activeSeedFolder.clear(); needsSeedLog = waitingForNativeSeed = false;
    }
    seedSlot = static_cast<int>(slot); savedSlot = true; seedRefreshPending = true;
}

bool savedEvent(unsigned flag) {
    const auto* bytes=static_cast<const u8*>(dComIfGs_getPEventBit());
    return tracker::persistentEventBit([&](unsigned index) { return bytes[index]; },flag);
}
bool flagObtained(const Json& flag) {
    const int id = flag.at("flag").get<int>();
    const auto kind = flag.at("kind").get<std::string>();
    if (kind == "event") return savedEvent(static_cast<unsigned>(id));
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
    inventory["Progressive Sword"] = tracker::progressiveSwordTier(
        [](int id) { return dComIfGs_isItemFirstBit(static_cast<u8>(id)) != 0; });
    tracker::stableQuestInventory(inventory,
        [](int id) { return dComIfGs_isItemFirstBit(static_cast<u8>(id)) != 0; },
        [](int slot) { return dComIfGs_getItem(slot, true); });
    tiers("Progressive Bow", {0x43, 0x55, 0x56});
    tiers("Progressive Clawshot", {0x44, 0x47});
    tiers("Progressive Fishing Rod", {0x4a, 0x5c});
    // The engine's COPY_ROD_2 query returns -1. The randomizer restores it by
    // setting this event, rather than installing another inventory-slot item.
    inventory["Progressive Dominion Rod"] = checkItemGet(0x46, -1) > 0 ? (savedEvent(0x2580) ? 2 : 1) : 0;
    inventory["Aurus Memo"] = tracker::aurusMemoOwned(
        [](int slot) { return dComIfGs_getItem(slot, false); },
        [](int event) { return savedEvent(static_cast<unsigned>(event)); });
    inventory["Forest Temple Second Monkey Door Unlocked"] = tracker::forestSecondMonkeyDoorUnlocked(
        [](int save, int flag) { return dComIfGs_isStageSwitch(save, flag) != 0; });
    inventory["Bomb Bag"] = 0;
    // Custom randomizer keys unlock persistent switches, not the current room's key counter.
    inventory["Faron Woods Coro Key"] = dComIfGs_isStageSwitch(0x2,0x0c) != 0;
    inventory["North Faron Woods Gate Key"] = dComIfGs_isStageSwitch(0x2,0x14) != 0;
    inventory["Gate Keys"] = savedEvent(0x810);
    // Repaired cannon is durable proof of City access, including a completed
    // randomized Sky Book (whose final item does not replace the slot item).
    inventory["Sky Cannon Repaired"] = savedEvent(0x3b08);
    // Desert is native field-map region 5 (bit 0x20). A previous visit can
    // keep its warps usable even if the original route is no longer available.
    inventory["Desert Map Unlocked"] = dComIfGs_isRegionBit(5) != 0;
    for (int i=0;i<3;++i) inventory["Bomb Bag"] += dComIfGs_getItem(SLOT_15+i,false)!=0xff;
    inventory["Goron Mines Key Shard"] = tracker::goronKeyShards(
        [](int id) { return dComIfGs_isItemFirstBit(static_cast<u8>(id)) != 0; },
        dComIfGs_isDungeonItemBossKey(0x11) != 0);
    tiers("Progressive Wallet", {0x35, 0x36});
    tiers("Progressive Hidden Skill", {0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7});
    if(model.vanilla) {
        tracker::vanillaInventory(inventory, savedEvent,
            [](int id) { return dComIfGs_isItemFirstBit(static_cast<u8>(id))!=0; },
            [](int slot) { return dComIfGs_getItem(slot,true); },
            [](int save,int flag) { return dComIfGs_isStageSwitch(save,flag)!=0; },dComIfGs_getWalletSize());
        inventory["North Faron Woods Gate Key"]=dComIfGs_isStageTbox(2,24) || dComIfGs_isStageSwitch(2,0x14);
        if(!dComIfGs_isDungeonItemBossKey(0x11))
            inventory["Goron Mines Key Shard"]=int(savedEvent(0x3008))+int(savedEvent(0x3702))+int(savedEvent(0x3701));
        for(int i=0;i<3;++i) {
            const char* provinces[]={"Faron","Eldin","Lanayru"};
            inventory[std::string("Story Can Complete ")+provinces[i]+" Twilight"]=dComIfGs_isDarkClearLV(i)!=0;
        }
        const bool forcedWolf=(inventory["Story Can Complete Prologue"] && !dComIfGs_isDarkClearLV(0)) ||
                              (inventory["Story Midna Injured"] && !inventory["Shadow Crystal"]);
        model.settings["Starting Form"]=forcedWolf ? "Wolf" : "Human";
    }
    inventory["Poe Soul"] = dComIfGs_getPohSpiritNum();
    inventory["Faron Twilight Tear"] = dComIfGs_getLightDropNum(0);
    inventory["Eldin Twilight Tear"] = dComIfGs_getLightDropNum(1);
    inventory["Lanayru Twilight Tear"] = dComIfGs_getLightDropNum(2);
    model.countedItems = {"Goron Mines Key Shard", "Poe Soul", "Faron Twilight Tear", "Eldin Twilight Tear", "Lanayru Twilight Tear"};
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
        if (key.save == 0x10) inventory["Forest Temple Small Keys Available"] = count;
        if (key.save == 0x12) tracker::lakebedEastWaterState(inventory, count,
            [](int save, int flag) { return dComIfGs_isStageSwitch(save, flag) != 0; });
        for (int door : key.doors) count += dComIfGs_isStageSwitch(key.save, door) != 0;
        inventory[small] = count;
        model.countedItems.insert(small);
        inventory[std::string(key.name) + (key.save == 0x14 ? " Bedroom Key" : " Big Key")] = dComIfGs_isDungeonItemBossKey(key.save) != 0;
    }
    std::set<std::string> hiddenMarkers;
    auto completed = journal;
    for (const auto& check : model.catalogue.at("checks")) {
        if(model.vanilla && !model.enabled(check)) continue;
        if (check.contains("map_hide_event") && savedEvent(check.at("map_hide_event").get<unsigned>()))
            hiddenMarkers.insert(check.at("name").get<std::string>());
        const auto native=model.vanilla ? tracker::vanillaCompletion(check.at("name").get<std::string>(),savedEvent,
            [](int id) { return dComIfGs_isItemFirstBit(static_cast<u8>(id))!=0; },
            [](int save,int flag) { return dComIfGs_isStageSwitch(save,flag)!=0; }) : std::optional<bool>{};
        if(native) { if(*native) completed.insert(check.at("name").get<std::string>()); }
        else for (const auto& flag : check.at("flags"))
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
    changed = changed || inventory != model.inventory || completed != model.obtained || hiddenMarkers != model.mapHidden;
    model.mapHidden=std::move(hiddenMarkers);
    model.inventory = std::move(inventory);
    model.obtained = std::move(completed);
    bool unskipped=false;
    for (const auto& name : model.obtained) unskipped=model.skipped.erase(name)>0 || unskipped;
    if (unskipped) persist();
    if (changed) {
        ++revision;
        ++logicGeneration;
        model.accessible.clear(); model.reached.clear(); model.events.clear();
        logicRequested = true;
    }
    dirty = false;
}

void updateLogic() {
    if (logicJob.valid() && logicJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = logicJob.get();
            if (runningGeneration == logicGeneration) { model.applyLogicResult(std::move(result)); ++revision; }
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
            return snapshot.takeLogicResult();
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
tracker::EntranceSpawn entranceArgs(void* args) {
    const char* stage=mods::arg<const char*>(args,0);
    return {stage ? stage : "",mods::arg<s8>(args,2),mods::arg<s16>(args,1),mods::arg<s8>(args,3)};
}
HookAction beforeEntrance(ModContext*,void* args,void*,void*) {
    entranceJourney.reset();
    const char* current=dComIfGp_getStartStageName();
    if(current && dComIfGp_getPlayer(0))
        entranceJourney.begin(model,entranceArgs(args),current,dComIfGp_roomControl_getStayNo());
    return HOOK_CONTINUE;
}
void afterEntrance(ModContext*,void* args,void*,void*) {
    entranceJourney.validateRedirect(model,entranceArgs(args));
}
void arrivedEntrance(ModContext*,void*,void*,void*) {
    const char* stage=dComIfGp_getStartStageName();
    if(entranceJourney.arrive(model,{stage?stage:"",dComIfGp_getStartStageRoomNo(),dComIfGp_getStartStagePoint(),dComIfGp_getStartStageLayer()})) {
        notebookDirty=true; ++revision;
    }
}
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
    const auto text=tracker::hintPlainText(std::string_view(reference->mText, sizeof(reference->mText)));
    if (text.empty() || text.size()>2048) return;
    auto& pages=hintNotes[name];
    if (!pages.is_object()) pages=Json::object();
    const auto key=std::to_string(page);
    // Rendered text grows while letters appear. Keep the longest observed page;
    // reopening a sign must not replace a complete note with its first letter.
    if (text.size()<=pages.value(key,std::string{}).size()) return;
    const auto previous=pages;
    pages[key]=text;
    try {
        if (journalText().size()>SAVE_BLOB_BUDGET_BYTES-1024) { pages=previous; return; }
    } catch (const Json::exception&) {
        pages=previous;
        return; // Never let malformed dialogue disable the mod or replace saved notes.
    }
    journal.insert(name); model.obtained.insert(name); model.skipped.erase(name);
    notebookDirty=true; ++revision;
}

ModResult addReadableBlocks(UiElementHandle pane,const std::string& markup) {
    for(size_t start=0;start<markup.size();) {
        auto end=markup.find("</div>",start); end=end==std::string::npos ? markup.size() : end+6;
        UiElementHandle row=0,anchor=0; UiRowDesc desc=UI_ROW_DESC_INIT;
        auto result=svc_ui->pane_add_row(mod_ctx,pane,&desc,&row); if(result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,row,"tp-detail-sections",true);
        UiControlDesc control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_BUTTON; control.label="";
        control.on_pressed=[](ModContext*,void*) {};
        result=svc_ui->pane_add_control(mod_ctx,row,&control,&anchor); if(result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
        result=svc_ui->pane_add_rml(mod_ctx,row,markup.substr(start,end-start).c_str(),nullptr); if(result!=MOD_OK) return result;
        start=end;
    }
    return MOD_OK;
}
ModResult buildItemHistory(ModContext*,UiWindowHandle,UiElementHandle left,UiElementHandle right,void*,ModError*) {
    auto result=addReadableBlocks(right,model.itemHistory(selectedItem));
    if(result!=MOD_OK) return result;
    return svc_ui->pane_add_rml(mod_ctx,left,"<div class='tp-title'>Item history</div><div class='tp-note'>Locations are revealed only for rewards you have collected. Future upgrades stay hidden. B or Escape returns to the tracker.</div>",nullptr);
}
void inspectItem(ModContext*,void* data) {
    const auto index=reinterpret_cast<uintptr_t>(data);
    if(itemWindow || index>=inventoryNames.size()) return;
    selectedItem=inventoryNames[index];
    UiTabDesc tab=UI_TAB_DESC_INIT; tab.title="Item history"; tab.build=buildItemHistory;
    UiWindowDesc desc=UI_WINDOW_DESC_INIT; desc.tabs=&tab; desc.tab_count=1; desc.rcss=styles;
    desc.on_closed=[](ModContext*,UiWindowHandle,void*) { itemWindow=0; selectedItem.clear(); };
    if(svc_ui->window_push(mod_ctx,&desc,&itemWindow)!=MOD_OK) itemWindow=0;
}
ModResult renderInventory() {
    std::string header="<div class='tp-title'>TPTracker</div><div class='tp-note'>"+tracker::escape(status)+"</div>";
    header+="<div class='tp-note'>Select an item to see its collected sources.</div>";
    if(inventorySections.empty()) {
        inventoryNames.clear();
        auto result=svc_ui->pane_add_rml(mod_ctx,inventoryElement,header.c_str(),&inventoryHeader);
        if(result!=MOD_OK) return result;
        UiControlDesc choose = UI_CONTROL_DESC_INIT;
        choose.kind = UI_CONTROL_BUTTON; choose.label = "Choose log";
        choose.on_pressed = [](ModContext*, void*) { openLogPicker(true); };
        result = svc_ui->pane_add_control(mod_ctx, inventoryElement, &choose, &chooseLogButton);
        if (result != MOD_OK) return result;
        for(const char* section:{"Progression","Collectibles","Keys","Quest items"}) {
            auto title="<div class='tp-group'>"+std::string(section)+"</div>";
            result=svc_ui->pane_add_rml(mod_ctx,inventoryElement,title.c_str(),nullptr); if(result!=MOD_OK) return result;
            int column=0; UiElementHandle row=0;
            for(const auto& item:model.catalogue.at("items")) {
                const auto name=item.at("Name").get<std::string>();
                if(name.ends_with("Portal") || (name.find("Bottle")!=std::string::npos && name!="Empty Bottle")) continue;
                if(!inventoryArt.contains(name) || inventoryArt.at(name).at("section")!=section) continue;
                UiRowDesc desc=UI_ROW_DESC_INIT;
                if(column==0) {
                    result=svc_ui->pane_add_row(mod_ctx,inventoryElement,&desc,&row); if(result!=MOD_OK) return result;
                    svc_ui->elem_set_class(mod_ctx,row,"tp-inventory-row",true);
                }
                UiElementHandle tile=0,text=0,button=0;
                result=svc_ui->pane_add_row(mod_ctx,row,&desc,&tile); if(result!=MOD_OK) return result;
                svc_ui->elem_set_class(mod_ctx,tile,"tp-item",true);
                result=svc_ui->pane_add_rml(mod_ctx,tile,"",&text); if(result!=MOD_OK) return result;
                svc_ui->elem_set_class(mod_ctx,text,"tp-item-art",true);
                UiControlDesc action=UI_CONTROL_DESC_INIT; action.kind=UI_CONTROL_BUTTON; action.label=name.c_str();
                action.on_pressed=inspectItem; action.user_data=reinterpret_cast<void*>(inventoryNames.size());
                result=svc_ui->pane_add_control(mod_ctx,tile,&action,&button); if(result!=MOD_OK) return result;
                svc_ui->elem_set_class(mod_ctx,button,"tp-item-action",true);
                svc_ui->control_set_tooltip(mod_ctx,button,"View collected sources");
                inventoryNames.push_back(name); inventorySections.push_back(text); inventoryBlocks.push_back(tile);
                column=(column+1)%2;
            }
        }
        result=svc_ui->pane_add_rml(mod_ctx,inventoryElement,"<div class='tp-note'>Current / maximum. Progressive items show upgrade tiers; small keys include keys already used. ? = count unavailable. Artwork: Henriko Magnifico, game and randomizer item icons.</div>",nullptr);
        if(result!=MOD_OK) return result;
    }
    svc_ui->elem_set_rml(mod_ctx,inventoryHeader,header.c_str());
    svc_ui->elem_set_visible(mod_ctx, chooseLogButton, needsSeedLog);
    for(size_t i=0;i<inventoryNames.size();++i) {
        const auto& name=inventoryNames[i]; auto found=model.inventory.find(name);
        const int count=found==model.inventory.end() ? -1 : found->second;
        std::string rml="<img src='mod://com.mikey022.tp_randomizer_tracker/res/"+tracker::escape(inventoryArt.at(name).at("icon").get<std::string>())+"'/>";
        rml+=tracker::escape(name)+"<span class='tp-item-count'>"+(count<0?std::string("?"):std::to_string(count))+" / "+std::to_string(model.inventoryMaximum(name))+"</span>";
        svc_ui->elem_set_class(mod_ctx,inventoryBlocks[i],"owned",count>0);
        svc_ui->elem_set_class(mod_ctx,inventoryBlocks[i],"missing",count<=0);
        auto result=svc_ui->elem_set_rml(mod_ctx,inventorySections[i],rml.c_str()); if(result!=MOD_OK) return result;
    }
    return MOD_OK;
}
std::string checksRml() {
    std::string rml = "<div class='tp-title'>" + std::string(allChecks ? "All Checks" : "Current Area") + "</div>";
    if (!allChecks && model.catalogue.contains("stage_labels"))
        rml += "<div class='tp-note'>" + tracker::escape(model.catalogue.at("stage_labels").value(model.stage, model.stage)) + "</div>";
    rml += "<div class='tp-note'>OPEN / green | LOCKED / red | UNKNOWN / amber | DONE / gray | SKIPPED / purple</div>";
    rml += "<div class='tp-note'>Select a check to view its requirements. Use Skip or Undo in its details window. Skipped checks are hidden on both maps.</div>";
    if (!model.seedLoaded) rml += "<div class='tp-note'>Load a seed log to evaluate logic.</div>";
    else if (logicRequested || logicJob.valid()) rml += "<div class='tp-note'>Updating reachability...</div>";
    if(model.shuffled) rml += model.entrancesComplete ? "<div class='tp-note'>Using this seed's shuffled entrances. Visited connections appear in Entrances.</div>" : "<div class='tp-note'>Entrance data is incomplete. Keep this save's spoiler log and seed.dat together; uncertain routes show UNKNOWN.</div>";
    rml += "<div class='tp-note'>Left stick or D-pad: up/down selects rows; left/right moves between the inventory and checks. Right stick: up/down selects checks; left/right changes page without resetting the row. Page buttons also work with mouse or controller.</div>";
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
        if (!tracker::passesHintSignFilter(check,showHintSigns[allChecks])) continue;
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!needle.empty() && lower.find(needle) == std::string::npos) continue;
        auto state = model.accessible.contains(name) ? model.accessible.at(name) : Truth::unknown;
        bool skipped=!complete && model.skipped.contains(name);
        int filterState = complete ? 3 : skipped ? 5 : state == Truth::yes ? 1 : state == Truth::no ? 2 : 4;
        if (statusFilters[allChecks] != 0 && statusFilters[allChecks] != filterState) continue;
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
    if (restorePageFocus) {
        restorePageFocus=false;
        stickRow=std::clamp(stickRow,0,std::max(0,static_cast<int>(visibleRows)-1));
        svc_ui->elem_focus(mod_ctx,visibleRows ? checkRows[stickRow] : pageControl);
    }
    if (shown == 0) rml += "<div class='tp-note'>No matching checks for " + tracker::escape(allChecks ? "All Checks" : model.stage) + ". Check the status filter, Show hint signs toggle, Filter checks text and Hide completed checks setting.</div>";
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
    target=std::clamp(target,0,std::max(0,static_cast<int>(visibleRows)-1));
    // Focus is checked by the host against the top document. Reassert while held
    // so its generic arrow-key pane selection cannot steal right-stick navigation.
    if (svc_ui->elem_focus(mod_ctx,visibleRows ? checkRows[target] : pageControl)!=MOD_OK) {
        stickNavigation={}; return;
    }
    stickRow=target;
    if (direction==1 || direction==-1) {
        const auto before=checkPage;
        if(direction==1) nextPage(mod_ctx,nullptr); else previousPage(mod_ctx,nullptr);
        restorePageFocus=checkPage!=before;
    }
}
ModResult updateTab(ModContext*, void*, ModError*) {
    if (!inventoryElement || !checksElement) return MOD_OK;
    if(!detailWindow && !itemWindow) navigateStick();
    if (renderedRevision == revision) return MOD_OK;
    const auto checks = checksRml();
    auto inventoryResult=renderInventory();
    if(inventoryResult!=MOD_OK) return inventoryResult;
    if (checks != lastChecks) {
        auto result = svc_ui->elem_set_rml(mod_ctx, checksElement, checks.c_str());
        if (result != MOD_OK) return result;
        lastChecks = std::move(checks);
    }
    renderedRevision = revision;
    return MOD_OK;
}
template<class T,class V>
void savePreference(T& target,const V& value) {
    const auto result=preferences.set(target,value);
    if(result!=MOD_OK) mods::log::warn("TPTracker: could not save filter preference ({})",static_cast<int>(result));
}
void statusGet(ModContext*, void*, UiControlValue* v) { v->int_value = statusFilters[allChecks]; }
void statusSet(ModContext*, void*, const UiControlValue* v) { checkPage = 0; savePreference(statusFilters[allChecks],v->int_value); ++revision; }
void hintSignsGet(ModContext*, void* data, UiControlValue* v) { v->bool_value=*static_cast<bool*>(data); }
void hintSignsSet(ModContext*, void* data, const UiControlValue* v) {
    savePreference(*static_cast<bool*>(data),v->bool_value);
    checkPage=0; stickRow=-1; restorePageFocus=false; ++revision;
}
void sortGet(ModContext*, void*, UiControlValue* v) { v->int_value=areaSort; }
void sortSet(ModContext*, void*, const UiControlValue* v) {
    savePreference(areaSort,v->int_value);
    checkPage=0; stickRow=-1; ++revision;
}
ModResult buildTab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle right, void* data, ModError*) {
    allChecks = data != nullptr;
    notesTab=false; notesElement=0;
    checkPage = 0; totalPages=1; visibleRows=0; restorePageFocus=false; pageControl=0;
    stickRow=-1; stickNavigation={};
    inventoryElement = checksElement = countElement = 0;
    checkRows.clear(); headingRows.clear();
    lastChecks.clear();
    renderedRevision = 0;
    inventoryElement=left; inventorySections.clear(); inventoryBlocks.clear();
    ModResult result=MOD_OK;
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
    UiControlDesc hints=UI_CONTROL_DESC_INIT;
    hints.kind=UI_CONTROL_TOGGLE; hints.label="Show hint signs";
    hints.help_rml="Include hint signs in this tab's check list. Read hints remain in Notes.";
    hints.get=hintSignsGet; hints.set=hintSignsSet; hints.user_data=&showHintSigns[allChecks];
    result=svc_ui->pane_add_control(mod_ctx,right,&hints,nullptr);
    if(result!=MOD_OK) return result;
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
bool deleteNoteDisabled(ModContext*,void* data) { return reinterpret_cast<uintptr_t>(data)>=personalNotes.size(); }
void deleteNote(ModContext*,void* data) {
    const auto previous=personalNotes;
    if (!tracker::deletePersonalNote(personalNotes,reinterpret_cast<uintptr_t>(data))) return;
    if (!persist()) personalNotes=previous;
    ++revision;
}
ModResult updateNotes(ModContext*,void*,ModError*) {
    if (!notesElement) return MOD_OK;
    int x=0,y=0;
    if (!itemWindow && tracker::readRightStick(x,y)) {
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
            UiElementHandle anchor=0,section=0,block=0;
            UiRowDesc row=UI_ROW_DESC_INIT;
            result=svc_ui->pane_add_row(mod_ctx,notesElement,&row,&block);
            if(result!=MOD_OK) return result;
            svc_ui->elem_set_class(mod_ctx,block,"tp-detail-sections",true);
            hintBlocks.push_back(block);
            UiControlDesc control=UI_CONTROL_DESC_INIT;
            control.kind=UI_CONTROL_BUTTON; control.label="";
            control.on_pressed=[](ModContext*,void*) {};
            result=svc_ui->pane_add_control(mod_ctx,block,&control,&anchor);
            if(result!=MOD_OK) return result;
            svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
            result=svc_ui->pane_add_rml(mod_ctx,block,"",&section);
            if(result!=MOD_OK) return result;
            hintAnchors.push_back(anchor); hintSections.push_back(section);
        }
        svc_ui->elem_set_rml(mod_ctx,hintSections[hintSectionCount],content.substr(start,end-start).c_str());
        ++hintSectionCount; start=end;
    }
    for(size_t i=0;i<hintSections.size();++i) {
        svc_ui->elem_set_visible(mod_ctx,hintBlocks[i],i<hintSectionCount);
    }
    for (size_t i=0;i<personalCards.size();++i) {
        const bool visible=i<personalNotes.size();
        svc_ui->elem_set_visible(mod_ctx,personalCards[i],visible);
        svc_ui->elem_set_visible(mod_ctx,personalDeletes[i],visible);
        if (visible) svc_ui->elem_set_text(mod_ctx,personalCards[i],personalNotes[i].c_str());
    }
    if (inventoryElement) { result=renderInventory(); if(result!=MOD_OK) return result; }
    renderedRevision=revision;
    return result;
}
ModResult buildNotes(ModContext*,UiWindowHandle,UiElementHandle left,UiElementHandle right,void*,ModError*) {
    notesTab=true; pageControl=checksElement=countElement=0;
    personalCards.clear(); personalDeletes.clear(); hintSections.clear(); hintAnchors.clear(); hintBlocks.clear(); hintSectionCount=0; notesScrollRow=0; notesNavigation={};
    checkRows.clear(); headingRows.clear(); visibleNames.clear(); visibleRows=0;
    stickRow=-1; stickNavigation={}; renderedRevision=0;
    inventoryElement=left; inventorySections.clear(); inventoryBlocks.clear();
    ModResult result=MOD_OK;
    result=svc_ui->pane_add_rml(mod_ctx,right,"<div class='tp-title'>My notes</div><div class='tp-note'>Left stick or D-pad: up/down scrolls notes and hints; left/right moves between panels. Right stick also scrolls notes. Type a reminder and press Enter to post it below. Select New note to write another. Each card has its own Delete button. Up to 64 notes, 1,000 characters each (16,000 total).</div>",nullptr);
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
        control.label="Delete note"; control.on_pressed=deleteNote; control.is_disabled=deleteNoteDisabled; control.user_data=reinterpret_cast<void*>(i);
        result=svc_ui->pane_add_control(mod_ctx,right,&control,&button);
        if (result!=MOD_OK) return result;
        personalDeletes.push_back(button);
    }
    notesElement=right;
    return result==MOD_OK ? updateNotes(mod_ctx,nullptr,nullptr) : result;
}
UiElementHandle entranceHeading=0,entranceSearchControl=0,entrancePrevious=0,entranceNext=0;
std::vector<UiElementHandle> entranceRows,entranceRowText,entranceAnchors;
std::string entranceSearch;
size_t entrancePage=0,entrancePageCount=1,entranceVisibleRows=0;
tracker::StickNavigation entranceNavigation;
int entranceScrollRow=0;
bool entrancePreviousDisabled(ModContext*,void*) { return entrancePage==0; }
bool entranceNextDisabled(ModContext*,void*) { return entrancePage+1>=entrancePageCount; }
void entranceSearchGet(ModContext*,void*,UiControlValue* value) { value->string_value=entranceSearch.c_str(); }
void entranceSearchSet(ModContext*,void*,const UiControlValue* value) {
    const std::string query=value->string_value ? value->string_value : "";
    if(query==entranceSearch) return;
    entranceSearch=query; entrancePage=0; entranceScrollRow=0; entranceNavigation={}; ++revision;
}
void navigateEntrances() {
    int x=0,y=0;
    if(itemWindow || detailWindow || !tracker::readRightStick(x,y)) { entranceNavigation={}; return; }
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const int direction=entranceNavigation.sample(x,y,now);
    if(std::abs(entranceNavigation.held)!=2) return;
    std::vector<UiElementHandle> targets{entranceSearchControl};
    if(!entrancePreviousDisabled(nullptr,nullptr)) targets.push_back(entrancePrevious);
    if(!entranceNextDisabled(nullptr,nullptr)) targets.push_back(entranceNext);
    targets.insert(targets.end(),entranceAnchors.begin(),entranceAnchors.begin()+entranceVisibleRows);
    const int previous=entranceScrollRow;
    if(direction) entranceScrollRow+=direction==2 ? 1 : -1;
    entranceScrollRow=std::clamp(entranceScrollRow,0,static_cast<int>(targets.size())-1);
    // Native focus scrolls each readable block into view. Hidden rows and disabled
    // page buttons are excluded; the host rejects focus while text entry is on top.
    if(svc_ui->elem_focus(mod_ctx,targets[entranceScrollRow])!=MOD_OK) { entranceScrollRow=previous; entranceNavigation={}; }
}
ModResult updateEntrances(ModContext*,void*,ModError*) {
    if(renderedRevision!=revision) {
        auto result=renderInventory(); if(result!=MOD_OK) return result;
        const auto view=tracker::entranceJournalView(model,entranceSearch);
        entrancePageCount=tracker::pageCount(view.entries.size(),pageSize);
        entrancePage=std::min(entrancePage,entrancePageCount-1);
        entranceVisibleRows=std::min(pageSize,view.entries.size()-entrancePage*pageSize);
        std::string heading="<div class='tp-title'>Discovered entrances</div><div class='tp-note'>Connections appear after you use them. Save your game to keep this journal. Each direction is recorded separately. Search entrance names or destinations. Right stick: up/down scrolls this page.</div>";
        heading+="<div>"+std::to_string(view.entries.size())+" matching / "+std::to_string(view.discovered)+" discovered | Page "+std::to_string(entrancePage+1)+" / "+std::to_string(entrancePageCount)+"</div>";
        if(model.settings.value("Decouple Entrances",Json("Off"))=="On") heading+="<div class='tp-note'>Decoupled entrances: the return trip can lead somewhere else.</div>";
        if(!model.entrancesComplete) heading+="<div class='tp-note'>Keep this save's spoiler log and seed.dat together to resolve all entrance connections.</div>";
        if(view.discovered==0) heading+="<div class='tp-note'>No entrances recorded yet. Previously visited routes cannot be reconstructed from the spoiler log; use an entrance to record it.</div>";
        else if(view.entries.empty()) heading+="<div class='tp-note'>No discovered entrances match this search. Clear the search to show all discoveries.</div>";
        svc_ui->elem_set_rml(mod_ctx,entranceHeading,heading.c_str());
        for(size_t i=0;i<entranceRows.size();++i) {
            const bool visible=i<entranceVisibleRows;
            svc_ui->elem_set_visible(mod_ctx,entranceRows[i],visible);
            if(visible) {
                const auto& entry=view.entries[entrancePage*pageSize+i];
                const auto text="<div class='tp-group'>"+tracker::escape(entry.source)+"</div><div>Leads to: "+tracker::escape(entry.destination)+"</div>";
                svc_ui->elem_set_rml(mod_ctx,entranceRowText[i],text.c_str());
            }
        }
        renderedRevision=revision;
    }
    // Run every frame, including while the journal data is unchanged.
    navigateEntrances();
    return MOD_OK;
}
ModResult buildEntrances(ModContext*,UiWindowHandle,UiElementHandle left,UiElementHandle right,void*,ModError*) {
    notesTab=false; pageControl=checksElement=countElement=notesElement=0;
    checkRows.clear(); headingRows.clear(); visibleNames.clear(); visibleRows=0;
    inventoryElement=left; inventorySections.clear(); inventoryBlocks.clear();
    entranceRows.clear(); entranceRowText.clear(); entranceAnchors.clear(); entrancePage=0; renderedRevision=0;
    entranceVisibleRows=0; entranceScrollRow=0; entranceNavigation={};
    auto result=svc_ui->pane_add_rml(mod_ctx,right,"",&entranceHeading); if(result!=MOD_OK) return result;
    UiControlDesc searchControl=UI_CONTROL_DESC_INIT;
    searchControl.kind=UI_CONTROL_STRING; searchControl.label="Search entrances"; searchControl.max_length=128;
    searchControl.get=entranceSearchGet; searchControl.set=entranceSearchSet;
    searchControl.string_set_mode=UI_STRING_SET_ON_COMMIT;
    result=svc_ui->pane_add_control(mod_ctx,right,&searchControl,&entranceSearchControl); if(result!=MOD_OK) return result;
    for(int i=0;i<2;++i) {
        UiControlDesc page=UI_CONTROL_DESC_INIT; page.kind=UI_CONTROL_BUTTON; page.label=i?"Next page":"Previous page";
        page.user_data=reinterpret_cast<void*>(static_cast<uintptr_t>(i));
        page.is_disabled=i?entranceNextDisabled:entrancePreviousDisabled;
        page.on_pressed=[](ModContext*,void* data) { if(data) { if(entrancePage+1<entrancePageCount) ++entrancePage; } else if(entrancePage) --entrancePage; entranceScrollRow=0; ++revision; };
        result=svc_ui->pane_add_control(mod_ctx,right,&page,i?&entranceNext:&entrancePrevious); if(result!=MOD_OK) return result;
    }
    for(size_t i=0;i<pageSize;++i) {
        UiElementHandle row=0,anchor=0,text=0; UiRowDesc desc=UI_ROW_DESC_INIT;
        result=svc_ui->pane_add_row(mod_ctx,right,&desc,&row); if(result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,row,"tp-detail-sections",true);
        UiControlDesc control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_BUTTON; control.label=""; control.on_pressed=[](ModContext*,void*) {};
        result=svc_ui->pane_add_control(mod_ctx,row,&control,&anchor); if(result!=MOD_OK) return result;
        svc_ui->elem_set_class(mod_ctx,anchor,"tp-scroll-anchor",true);
        result=svc_ui->pane_add_rml(mod_ctx,row,"",&text); if(result!=MOD_OK) return result;
        entranceRows.push_back(row); entranceRowText.push_back(text); entranceAnchors.push_back(anchor);
    }
    return updateEntrances(nullptr,nullptr,nullptr);
}
void onClosed(ModContext*, UiWindowHandle, void*) { window = 0; inventoryElement = checksElement = notesElement = 0; notesTab=false; }
void toggleGet(ModContext*, void*, UiControlValue* value) { value->bool_value = window != 0; }
void toggleSet(ModContext*, void*, const UiControlValue* value) {
    if (!value->bool_value && window) { if(itemWindow) svc_ui->window_close(mod_ctx,itemWindow); if(detailWindow) svc_ui->window_close(mod_ctx,detailWindow); svc_ui->window_close(mod_ctx, window); return; }
    if (!value->bool_value || window) return;
    if (!model.seedLoaded || waitingForNativeSeed) refreshSelectedSeed();
    UiTabDesc tabs[4] = {UI_TAB_DESC_INIT, UI_TAB_DESC_INIT, UI_TAB_DESC_INIT, UI_TAB_DESC_INIT};
    tabs[0].title = "Current Area";
    tabs[1].title = "All Checks";
    static int allTab;
    for (auto& tab : tabs) { tab.build = buildTab; tab.update = updateTab; }
    tabs[1].user_data = &allTab;
    tabs[2].title="Notes"; tabs[2].build=buildNotes; tabs[2].update=updateNotes;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    tabs[3].title="Entrances"; tabs[3].build=buildEntrances; tabs[3].update=updateEntrances;
    desc.tabs = tabs; desc.tab_count = model.shuffled ? 4 : 3; desc.rcss = styles; desc.on_closed = onClosed;
    auto result = svc_ui->window_push(mod_ctx, &desc, &window);
    if (result != MOD_OK) { window = 0; status = "Could not open tracker window."; mods::log::error("TPTracker: window_push failed"); }
}
void openTracker(ModContext*, void*) {
    UiControlValue value{}; value.bool_value=true; toggleSet(mod_ctx,nullptr,&value);
}
void cancelBinding(ModContext*, UiDialogHandle, void*) {
    tracker::cancelControllerBindingCapture();
    capturingBinding=-1; bindingDialog=0; shortcutHeld=true;
}
void dismissBinding(ModContext*, UiDialogHandle, void*) {
    // The host treats B as Back. Capture it here before the prompt is dismissed.
    if (capturingBinding>=0 && captureReady) {
        const int pressed=tracker::pressedBinding(capturingBinding%2!=0);
        if (pressed && (capturingBinding>=2 || capturingBinding%2 || !tracker::escapeBinding(pressed)))
            svc_config->set_int(mod_ctx,shortcutBindings[capturingBinding],pressed);
    }
    cancelBinding(mod_ctx,0,nullptr);
}
void assignBinding(ModContext*,void* data) {
    capturingBinding=static_cast<int>(reinterpret_cast<uintptr_t>(data)); captureReady=false;
    tracker::cancelControllerBindingCapture();
    if(capturingBinding%2) tracker::beginControllerBindingCapture();
    UiDialogAction action=UI_DIALOG_ACTION_INIT;
    action.label="Cancel"; action.on_pressed=cancelBinding;
    UiDialogDesc dialog=UI_DIALOG_DESC_INIT;
    dialog.title=capturingBinding>=2 ? "Assign Check Info button" : "Assign TPTracker shortcut";
    dialog.body_rml=capturingBinding%2 ? "Release the current button, then press any controller button or trigger." : capturingBinding>=2 ? "Release the current key, then press any keyboard key. Use Cancel to keep your binding." : "Release the current key, then press any keyboard key. Escape cancels.";
    dialog.actions=&action; dialog.action_count=1; dialog.on_dismiss=dismissBinding;
    if (svc_ui->dialog_push(mod_ctx,&dialog,&bindingDialog)!=MOD_OK) cancelBinding(mod_ctx,0,nullptr);
}
void clearBinding(ModContext*,void* data) {
    svc_config->set_int(mod_ctx,shortcutBindings[reinterpret_cast<uintptr_t>(data)],0);
    shortcutHeld=true;
}
void pollShortcut() {
    int64_t checkKey=0,checkPad=0;
    svc_config->get_int(mod_ctx,shortcutBindings[2],&checkKey);
    svc_config->get_int(mod_ctx,shortcutBindings[3],&checkPad);
    tracker::setCheckBindings(static_cast<int>(checkKey),static_cast<int>(checkPad));
    if (capturingBinding>=0) {
        const bool controller=capturingBinding%2!=0;
        const int pressed=controller ? tracker::pollControllerBindingCapture() : tracker::pressedBinding(false);
        if (!controller && !pressed) captureReady=true;
        if (pressed && (controller || captureReady)) {
            if (capturingBinding>=2 || capturingBinding%2 || !tracker::escapeBinding(pressed))
                svc_config->set_int(mod_ctx,shortcutBindings[capturingBinding],pressed);
            const auto dialog=bindingDialog;
            cancelBinding(mod_ctx,0,nullptr);
            if (dialog) svc_ui->dialog_close(mod_ctx,dialog);
        }
        return;
    }
    int64_t key=0,pad=0;
    svc_config->get_int(mod_ctx,shortcutBindings[0],&key);
    svc_config->get_int(mod_ctx,shortcutBindings[1],&pad);
    const bool down=tracker::shortcutDown(notesTab && window ? 0 : static_cast<int>(key),static_cast<int>(pad));
    // A newly selected binding must be released before it can toggle the window.
    if (key!=previousKeyboard || pad!=previousController) {
        previousKeyboard=key; previousController=pad; shortcutHeld=true;
    }
    // Reserve the Check Info binding while inspecting a map; a custom RT or
    // A tracker shortcut must not open a parent window underneath the details.
    const bool reservedMapButton=tracker::checkBindingUses(static_cast<int>(pad),true) && tracker::shortcutDown(0,static_cast<int>(pad));
    const bool reservedMapKey=tracker::checkBindingUses(static_cast<int>(key),false) && tracker::shortcutDown(static_cast<int>(key),0);
    if(mapDetail || (tracker::mapInspectionActive() && (reservedMapButton || reservedMapKey))) { shortcutHeld=down; return; }
    const bool pressed=tracker::shortcutPressed(notesTab && window ? 0 : static_cast<int>(key),static_cast<int>(pad));
    if ((down || pressed) && !shortcutHeld) {
        UiControlValue value{}; value.bool_value=!window; toggleSet(mod_ctx,nullptr,&value);
    }
    shortcutHeld=down;
}
void filePicked(ModContext*, ModResult result, const char* const* locations, uint32_t count, const char*, void* generation) {
    if (result != MOD_OK || count != 1) return;
    if (seedRefreshPending) refreshSelectedSeed();
    if(tracker::normalPlayActive()) return;
    if (reinterpret_cast<uintptr_t>(generation) != saveGeneration) return;
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
        tracker::importSelectedLog(model, content, activeSeedFolder);
        std::string nativeError;
        if((activeSeedFolder.empty() || waitingForNativeSeed) &&
           !tracker::loadSiblingEntrances(model,std::filesystem::u8path(locations[0]),nativeError) && !nativeError.empty())
            mods::log::warn("TPTracker: {}",nativeError);
        needsSeedLog = false; entranceJourney.reset(); ++logicGeneration;
        status = "Seed: " + model.seed;
        persist(); dirty = true;
    } catch (const std::exception& e) { status = e.what(); mods::log::warn("TPTracker: {}", e.what()); }
}
void openLogPicker(bool guestSetup) {
    ++revision;
    if(tracker::normalPlayActive()) { status="Normal Play uses your save's story progress; no seed log is needed."; return; }
    if (savedSlot && activeSeedFolder.empty()) refreshSelectedSeed();
    static const FileFilter logs[] = {{"Dusklight seed log", "txt"}};
    static const FileFilter filters[] = {{"Dusklight seed logs / TPTracker JSON", "txt;json"}};
    FilePickOptions options = FILE_PICK_OPTIONS_INIT;
    options.filters = guestSetup ? logs : filters; options.filter_count = 1;
    const auto folder = (activeSeedFolder / "").u8string();
    if (needsSeedLog) {
        options.default_location = reinterpret_cast<const char*>(folder.c_str());
    }
    if (svc_file->pick_file(mod_ctx, &options, filePicked, reinterpret_cast<void*>(saveGeneration)) != MOD_OK)
        status = "Could not open the file picker.";
}
void searchGet(ModContext*, void*, UiControlValue* value) { value->string_value = search.c_str(); }
void searchSet(ModContext*, void*, const UiControlValue* value) { savePreference(search,std::string(value->string_value ? value->string_value : "")); ++revision; }
void hideGet(ModContext*, void*, UiControlValue* value) { value->bool_value = hideCompleted; }
void hideSet(ModContext*, void*, const UiControlValue* value) { savePreference(hideCompleted,value->bool_value); ++revision; }
void mapGet(ModContext*, void*, UiControlValue* value) { value->bool_value = tracker::mapAvailable && tracker::mapEnabled; }
void mapSet(ModContext*, void*, const UiControlValue* value) { if(tracker::mapAvailable) savePreference(tracker::mapEnabled,value->bool_value); }
void miniGet(ModContext*,void*,UiControlValue* v) { v->bool_value=tracker::minimapAvailable && tracker::minimapEnabled; }
void miniSet(ModContext*,void*,const UiControlValue* v) { if(tracker::minimapAvailable) savePreference(tracker::minimapEnabled,v->bool_value); }
void typeGet(ModContext*,void* ptr,UiControlValue* v) { v->bool_value=*static_cast<bool*>(ptr); }
void typeSet(ModContext*,void* ptr,const UiControlValue* v) { savePreference(*static_cast<bool*>(ptr),v->bool_value); }
ModResult buildSettings(ModContext*, UiElementHandle pane, void*, ModError*) {
    settingsStatus = 0;
    auto result=svc_ui->pane_add_section(mod_ctx,pane,"Tracker");
    if(result!=MOD_OK) return result;
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE; control.label = "TPTracker"; control.get = toggleGet; control.set = toggleSet;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    for (int device=0;device<4;++device) {
        if(device%2==0) {
            result=svc_ui->pane_add_section(mod_ctx,pane,device>=2 ? "Check Info bindings" : "Open tracker bindings");
            if(result!=MOD_OK) return result;
        }
        control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_BUTTON;
        const bool controller=device%2!=0;
        control.label=device>=2 ? (controller ? "Check Info controller button" : "Check Info keyboard key")
                                : (controller ? "Open tracker controller button" : "Open tracker keyboard key");
        control.help_rml=device>=2 ? "Default: A (Xbox) / Cross (PlayStation) while fully zoomed in over a check. A custom Check Info binding replaces this default. Left click stays available. Clear both Check Info bindings to restore A / Cross." : nullptr;
        control.on_pressed=assignBinding; control.user_data=reinterpret_cast<void*>(static_cast<uintptr_t>(device));
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,&bindingButtons[device]);
        if (result!=MOD_OK) return result;
        control.label=device>=2 ? (controller ? "Clear Check Info controller binding" : "Clear Check Info keyboard binding") : (controller ? "Clear tracker controller binding" : "Clear tracker keyboard binding");
        control.on_pressed=clearBinding;
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
        if (result!=MOD_OK) return result;
    }
    result=svc_ui->pane_add_section(mod_ctx,pane,"Seed setup");
    if(result!=MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_BUTTON; control.label = "Load this save's seed log / JSON";
    control.on_pressed = [](ModContext*, void*) { openLogPicker(false); };
    control.is_disabled=[](ModContext*,void*) { return tracker::normalPlayActive(); };
    control.help_rml="Randomizer saves use a matching seed log. Normal Play tracks your save's story progress automatically.";
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    result=svc_ui->pane_add_section(mod_ctx,pane,"Check filters");
    if(result!=MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_STRING; control.label = "Filter checks"; control.get = searchGet; control.set = searchSet; control.max_length = 128;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE; control.label = "Hide completed checks"; control.get = hideGet; control.set = hideSet;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    result=svc_ui->pane_add_section(mod_ctx,pane,"Map display");
    if(result!=MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE; control.label = tracker::mapAvailable ? "Native map check markers" : "Map markers unavailable on this build"; control.get = mapGet; control.set = mapSet;
    result = svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
    if (result != MOD_OK) return result;
    control = UI_CONTROL_DESC_INIT;
    control.kind=UI_CONTROL_TOGGLE; control.label=tracker::minimapAvailable ? "Minimap check dots" : "Minimap hook unavailable on this build"; control.get=miniGet; control.set=miniSet;
    result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
    if (result!=MOD_OK) return result;
    for (int surface=0;surface<2;++surface) {
        control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_TOGGLE;
        control.label=surface ? "Minimap: Show Accessible Only" : "Map: Show Accessible Only";
        control.get=typeGet; control.set=typeSet;
        control.user_data=surface ? &tracker::minimapAccessibleOnly : &tracker::mapAccessibleOnly;
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
        if(result!=MOD_OK) return result;
    }
    control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_TOGGLE;
    control.label="Map: Show Check Info hint (visual only)";
    control.help_rml="Show the hint beside the check name at the closest map zoom. Mouse click and your Check Info binding still work when this is off.";
    control.get=typeGet; control.set=typeSet; control.user_data=&tracker::showCheckInfoHint;
    result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
    if(result!=MOD_OK) return result;
    control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_TOGGLE;
    control.label="Dungeon map: Show check counts";
    control.help_rml="Show accessible and remaining checks across all floors, using this seed's enabled checks. Completed and skipped checks are excluded. Marker category filters do not change these totals.";
    control.get=typeGet; control.set=typeSet; control.user_data=&tracker::showDungeonSummary;
    result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
    if(result!=MOD_OK) return result;
    static const char* types[]={"Treasure chests","NPC / event rewards","Golden bugs","Poe souls","Golden wolves","Shop items","Owl statues","Grotto checks","Freestanding Rupees","Hidden Rupees"};
    for (int surface=0;surface<2;++surface) for (int i=0;i<tracker::checkTypeCount;++i) {
        if(i==0) {
            result=svc_ui->pane_add_section(mod_ctx,pane,surface ? "Minimap check types" : "Map check types");
            if(result!=MOD_OK) return result;
        }
        std::string label=std::string(surface ? "Minimap: " : "Map: ")+types[i];
        control=UI_CONTROL_DESC_INIT; control.kind=UI_CONTROL_TOGGLE; control.label=label.c_str();
        control.get=typeGet; control.set=typeSet; control.user_data=surface ? &tracker::minimapTypes[i] : &tracker::mapTypes[i];
        result=svc_ui->pane_add_control(mod_ctx,pane,&control,nullptr);
        if (result!=MOD_OK) return result;
    }
    result=svc_ui->pane_add_section(mod_ctx,pane,"Status and information");
    if(result!=MOD_OK) return result;
    result = svc_ui->pane_add_text(mod_ctx, pane, status.c_str(), &settingsStatus);
    if (result != MOD_OK) return result;
    return svc_ui->pane_add_text(mod_ctx, pane,
        "Native RmlUi tracker. The current SDK window captures game input and cannot detach onto another monitor. Filters are remembered across restarts. Seed settings are stored per save slot.", nullptr);
}
ModResult updateSettings(ModContext*, void*, ModError*) {
    int64_t checkKeyboard=0; svc_config->get_int(mod_ctx,shortcutBindings[2],&checkKeyboard);
    for (int device=0;device<4;++device) {
        int64_t value=0; svc_config->get_int(mod_ctx,shortcutBindings[device],&value);
        const auto name=device==3 && !value && !checkKeyboard ? std::string("Default: A / Cross") : tracker::bindingName(static_cast<int>(value),device%2!=0);
        const auto label=std::string(device>=2 ? "Check Info " : "Open tracker ")+(device%2 ? "controller button - " : "keyboard key - ")+name;
        if (bindingButtons[device]) svc_ui->control_set_label(mod_ctx,bindingButtons[device],label.c_str());
    }
    return settingsStatus ? svc_ui->elem_set_text(mod_ctx, settingsStatus, status.c_str()) : MOD_OK;
}
}

#ifdef _MSVC_LANG
#define TRACKER_NEXT_STAGE_SYMBOL "?dComIfGp_setNextStage@@YAXPEBDFCCMIHCFHH@Z"
#else
#define TRACKER_NEXT_STAGE_SYMBOL "_Z21dComIfGp_setNextStagePKcsaafjiasii"
#endif
DEFINE_HOOK_SYMBOL(TRACKER_NEXT_STAGE_SYMBOL, void(const char*,s16,s8,s8,f32,u32,int,s8,s16,int,int), TrackerEntranceRequest);
DEFINE_HOOK(&dComIfGp_setStartStage, TrackerEntranceArrival);
DEFINE_HOOK(&dSv_info_c::onSwitch, SwitchHook);
DEFINE_HOOK(&dComIfGs_onTbox, ChestHook);
DEFINE_HOOK(&dMsgObject_c::_draw, HintDialogueDraw);

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    preferences.initialize(svc_config,mod_ctx);
    const auto preferenceResult=tracker::bindTrackerPreferences(preferences,{
        hideCompleted,tracker::mapEnabled,tracker::minimapEnabled,
        tracker::mapAccessibleOnly,tracker::minimapAccessibleOnly,tracker::showCheckInfoHint,tracker::showDungeonSummary,
        tracker::mapTypes,tracker::minimapTypes,statusFilters,showHintSigns,areaSort,search});
    if(preferenceResult!=MOD_OK) return mods::set_error(error,preferenceResult,"TPTracker filter preferences could not be restored");
    ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx, "catalogue.json", &buffer) != MOD_OK)
        return mods::set_error(error, MOD_ERROR, "TPTracker catalogue is missing");
    try { model.load(Json::parse(static_cast<const char*>(buffer.data), static_cast<const char*>(buffer.data) + buffer.size)); }
    catch (const std::exception& e) { svc_resource->free(mod_ctx, &buffer); return mods::set_error(error, MOD_ERROR, e.what()); }
    svc_resource->free(mod_ctx, &buffer);
    buffer=RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx,"check_guides.json",&buffer)==MOD_OK) {
        try { checkGuides.load(Json::parse(static_cast<const char*>(buffer.data),static_cast<const char*>(buffer.data)+buffer.size)); }
        catch (const std::exception& e) { mods::log::warn("TPTracker: collection guides unavailable: {}",e.what()); }
        svc_resource->free(mod_ctx,&buffer);
    } else mods::log::warn("TPTracker: collection-guide resource is missing");
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
    HookOptions entranceOptions=HOOK_OPTIONS_INIT;
    entranceOptions.priority=1000; // capture original request before the randomizer's priority-0 rewrite
    auto entranceResult=mods::hook::add_pre<TrackerEntranceRequest>(beforeEntrance,&entranceOptions);
    if(entranceResult==MOD_OK) entranceResult=mods::hook::add_post<TrackerEntranceRequest>(afterEntrance);
    if(entranceResult==MOD_OK) entranceResult=mods::hook::add_post<TrackerEntranceArrival>(arrivedEntrance);
    if(entranceResult!=MOD_OK) return mods::set_error(error,entranceResult,"TPTracker entrance observation hook failed");
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
    tracker::setInputFrameHandler(pollShortcut);
    tracker::setMapCheckHandler(inspectMapCheck,closeMapCheck);
    if (result != MOD_OK) return mods::set_error(error, result, "TPTracker input event hook failed");
    // At the title screen the engine's default data number is not an active slot.
    restore(dComIfGp_getPlayer(0) ? dComIfGs_getDataNum() : -1);
    for (auto entry : {std::pair{"tracker-keyboard", &shortcutBindings[0]}, std::pair{"tracker-controller", &shortcutBindings[1]}, std::pair{"check-keyboard", &shortcutBindings[2]}, std::pair{"check-controller", &shortcutBindings[3]}}) {
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
    if (seedRefreshPending) {
        // The sidecar is flushed after save observers return. Read it here, and
        // retain the live completion journal rather than restoring an older one.
        seedRefreshPending = false;
        backupCard();
        refreshSelectedSeed();
    }
    auto now = std::chrono::steady_clock::now();
    if (window && waitingForNativeSeed && now >= nextSeedProbe) {
        nextSeedProbe = now + std::chrono::seconds(2);
        std::error_code error;
        if (std::filesystem::is_regular_file(activeSeedFolder / "seed.dat", error)) refreshSelectedSeed();
    }
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
    tracker::setInputFrameHandler(nullptr);
    tracker::cancelControllerBindingCapture();
    if(itemWindow) svc_ui->window_close(mod_ctx,itemWindow);
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
