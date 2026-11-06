#include "model.hpp"
#include "seed_data.hpp"
#include "seed.hpp"
#include "entrance_journal.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace tracker;
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::string read(const char* path) { std::ifstream in(path); std::ostringstream text; text<<in.rdbuf(); return text.str(); }
void testGuestSetup(const Json& catalogue, const std::string& native) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("tptracker-guest-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(fs::create_directory(root), "could not create guest fixture");
    struct Cleanup { fs::path root; ~Cleanup() { std::error_code ec; fs::remove_all(root, ec); } } cleanup{root};
    const auto sidecar = root / "randomizer.json";
    std::ofstream(sidecar) << R"({"slots":[{"blobs":{"seed_hash":"U29sbw=="}},{"blobs":{"seed_hash":"SG9zdA=="}}]})";
    const auto folder = seedFolderForSlot(root, sidecar, 1);
    require(folder.filename() == "Host" && seedFolderForSlot(root, sidecar, 0).filename() == "Solo",
            "guest identity did not follow the active slot");
    fs::create_directories(folder);
    std::ofstream(folder / "seed.dat") << native;
    const std::string settings = "\n# Settings\nLogic Rules: All Locations Reachable\nRandomize Overworld Entrances: On\nDecouple Double Door Entrances: On\n";
    const auto external = root / "Host Anti-Spoiler Log.txt";
    std::ofstream(external) << "Hash: Solo" << settings;
    Model guest; guest.load(catalogue);
    guest.obtained.insert("Wooden Sword Chest");
    require(!loadSeedFolder(guest, folder), "native file alone reported ready");
    std::string error;
    try { importSelectedLog(guest, readSeedMetadata(external), folder); }
    catch (const std::exception& e) { error = e.what(); }
    require(error.find("Host") != std::string::npos && error.find("Solo") != std::string::npos &&
            !guest.seedLoaded && guest.settings.empty() && guest.obtained.contains("Wooden Sword Chest"),
            "wrong log with the expected filename changed the guest tracker");
    guest.loadSeed("Hash: Solo" + settings);
    require(!loadSeedFolder(guest, folder), "journal from a different seed satisfied the guest prompt");
    std::ofstream(external) << "Hash: Host" << settings;
    importSelectedLog(guest, readSeedMetadata(external), folder);
    require(guest.seed == "Host" && guest.entrancesComplete && !guest.entranceTargets.empty() &&
            guest.itemPlacements.empty() && guest.visitedEntrances.empty() && guest.obtained.size() == 1,
            "external anti-spoiler log lost native routes, disclosed items or changed progress");
    Model restored; restored.load(catalogue);
    restored.loadSeed(Json{{"seed", guest.seed}, {"settings", guest.settings}}.dump());
    require(loadSeedFolder(restored, folder) && restored.entranceTargets == guest.entranceTargets,
            "saved guest settings did not restore routes without re-selecting the external log");
    std::ofstream(folder / external.filename()) << readSeedMetadata(external);
    restored.resetSave();
    require(loadSeedFolder(restored, folder) && restored.seed == "Host", "automatic anti-spoiler lookup failed");
    std::ofstream(folder / "seed.dat") << "formatVersion: 30\n";
    bool unsupported = false;
    try { loadSeedFolder(restored, folder); } catch (const std::exception&) { unsupported = true; }
    require(unsupported, "unsupported native data triggered guest setup");
}
int main(int argc,char** argv) { try {
    require(argc>=2,"missing catalogue"); auto data=Json::parse(read(argv[1]));
    Model model; model.load(data);
    const std::string log="Hash: Privacy\nAll Starting Items:\n    World 1:\n      - Shadow Crystal\nAll Locations:\n    World 1:\n        Wooden Sword Chest: Progressive Sword\n        Coro Lantern: Progressive Sword\n        Ordon Cat Rescue: Shadow Crystal\nAll Entrances:\n    World 1:\n        Dungeon:\n            Faron Woods -> Forest Temple: Goron Mines from Death Mountain Sumo Hall\n            Death Mountain Sumo Hall -> Goron Mines: Forest Temple from Faron Woods\n# Settings\nLogic Rules: All Locations Reachable\nRandomize Dungeon Entrances: On\nDecouple Entrances: Off\n";
    model.loadSeed(log);
    require(model.inventory.empty() && model.obtained.empty(),"log granted inventory/completion");
    model.inventory["Progressive Sword"]=1;
    require(model.itemHistory("Progressive Sword").find("Wooden Sword Chest")==std::string::npos,"owned item leaked unchecked location");
    model.obtained.insert("Wooden Sword Chest");
    auto history=model.itemHistory("Progressive Sword");
    require(history.find("Wooden Sword Chest")!=std::string::npos,"collected source missing");
    require(history.find("Coro Lantern")==std::string::npos,"uncollected progressive upgrade leaked");
    model.inventory["Progressive Sword"]=0;
    require(model.itemHistory("Progressive Sword").find("Wooden Sword Chest")==std::string::npos,"grey tile leaked location");
    model.inventory["Shadow Crystal"]=1;
    require(model.itemHistory("Shadow Crystal").find("Starting inventory")!=std::string::npos,"starting item source missing");
    require(model.itemHistory("Shadow Crystal").find("Ordon Cat Rescue")==std::string::npos,"starting item leaked another copy");
    const std::string forest="North Faron Woods -> Forest Temple Entrance";
    require(model.entrances.at(forest)=="Goron Mines Entrance","alias destination resolved incorrectly");
    require(model.entrances.at("Goron Mines Entrance -> Death Mountain Sumo Hall Goron Mines Tunnel")=="North Faron Woods","coupled return not inverted");
    require(!model.entrancesComplete,"partial log treated as complete");
    require(model.entranceDestination("Lake Hylia Lakebed Temple Entrance","Lakebed Temple Entrance").empty(),"missing shuffled edge silently fell back to vanilla");
    auto decoupled=log; decoupled.replace(decoupled.find("Decouple Entrances: Off"),std::string("Decouple Entrances: Off").size(),"Decouple Entrances: On");
    model.loadSeed(decoupled);
    require(!model.entrances.contains("Goron Mines Entrance -> Death Mountain Sumo Hall Goron Mines Tunnel"),"decoupled seed inferred return");
    EntranceJourney journey;
    const auto* from=journey.endpoint(model,forest);
    require(from!=nullptr,"native entrance missing");
    auto spawn=[](const Json& e) { return EntranceSpawn{e.at("stage"),e.at("room"),e.at("spawn"),e.at("layer")}; };
    const auto* reverse=journey.endpoint(model,from->at("reverse"));
    const auto* target=journey.endpoint(model,model.entranceTargets.at(forest));
    auto request=spawn(*from), arrival=spawn(*target);
    journey.begin(model,request,reverse->at("stage"),reverse->at("room"));
    require(!journey.source.empty() && model.visitedEntrances.empty(),"request revealed entrance before traversal");
    journey.validateRedirect(model,arrival);
    require(journey.arrive(model,arrival) && model.visitedEntrances.contains(forest),"confirmed traversal not recorded");
    require(model.visitedEntrances.size()==1,"decoupled traversal leaked reverse");
    auto entrances=entranceJournalView(model,"");
    require(entrances.discovered==1 && entrances.entries.size()==1,"journal count includes undiscovered connections");
    require(entranceJournalView(model,"  fArOn  ").entries.size()==1,"source search or whitespace/case handling broken");
    require(entranceJournalView(model,"gOrOn").entries.size()==1,"destination search broken");
    auto hiddenSearch=entranceJournalView(model,"Death Mountain");
    require(hiddenSearch.discovered==1 && hiddenSearch.entries.empty(),"search revealed an unvisited source from the seed");
    require(entranceJournalView(model,"not a real entrance").entries.empty(),"unmatched search did not clear results");
    require(entranceJournalView(model," \t ").entries.size()==1,"clearing search did not restore discoveries");

    model.visitedEntrances.clear();
    require(entranceJournalView(model,"Goron").entries.empty(),"empty journal search revealed seed connections");
    journey.begin(model,request,reverse->at("stage"),reverse->at("room")); journey.validateRedirect(model,request);
    require(!journey.arrive(model,arrival) && model.visitedEntrances.empty(),"cancelled/unmodified request recorded");
    journey.begin(model,request,reverse->at("stage"),reverse->at("room")); model.seed="another save";
    require(!journey.arrive(model,arrival),"pending traversal leaked across seeds");
    // Repeated aliases cannot collapse separate decoupled doors. The native
    // packed table disambiguates destinations which the plain log cannot.
    auto repeated=data;
    Json doors=Json::array();
    for(const auto& e:data.at("entrance_edges")) if(e.at("couple")=="Zoras Domain From Upper Zoras River") doors.push_back(e);
    require(doors.size()==6,"double-door fixture incomplete");
    repeated["entrance_edges"]=doors;
    std::string duplicateLog="Hash: Double doors\nAll Entrances:\n    World 1:\n        Overworld:\n";
    for(const auto& e:doors) {
        auto label=e.at("label").get<std::string>(); auto arrow=label.find(" -> ");
        duplicateLog+="            "+label+": "+label.substr(arrow+4)+" from "+label.substr(0,arrow)+"\n";
    }
    duplicateLog+="# Settings\nLogic Rules: All Locations Reachable\nRandomize Overworld Entrances: On\nDecouple Entrances: On\nDecouple Double Door Entrances: On\n";
    Model doubled; doubled.load(repeated); doubled.loadSeed(duplicateLog);
    require(!doubled.entrancesComplete,"duplicate destination labels guessed without native data");
    std::string native="formatVersion: 3\nmEntranceOverrides:\n";
    for(const auto& e:doors) native+="  "+std::to_string(nativeEntranceKey(e))+": "+std::to_string(nativeEntranceKey(e))+"\n";
    testGuestSetup(repeated, native);
    doubled.loadNativeEntrances(native);
    require(doubled.entrancesComplete && doubled.entranceTargets.size()==6,"native tuples did not resolve every decoupled door");
    for(const auto& e:doors) require(doubled.entranceTargets.at(e.at("id"))==e.at("id").get<std::string>(),"decoupled physical door routed incorrectly");
    auto groupLog=std::string("Hash: Coupled doors\nAll Entrances:\n    World 1:\n        Overworld:\n            Upper Zoras River -> Zoras Domain: Zoras Domain from Upper Zoras River\n            Zoras Domain -> Upper Zoras River: Upper Zoras River from Zoras Domain\n# Settings\nLogic Rules: All Locations Reachable\nRandomize Overworld Entrances: On\nDecouple Entrances: Off\nDecouple Double Door Entrances: Off\n");
    doubled.loadSeed(groupLog);
    require(doubled.entrancesComplete,"coupled double doors left secondary doors unresolved");
    require(doubled.entranceTargets.at(doors[0].at("id"))==doors[4].at("id").get<std::string>(),"coupled door representative differs from generator");
    // Decoupled double-door searches retain physical door names, including underwater routes.
    doubled.loadSeed(duplicateLog); doubled.loadNativeEntrances(native);
    for(const auto& e:doors) doubled.visitedEntrances.insert(e.at("id").get<std::string>());
    const auto underwater=entranceJournalView(doubled,"underwater");
    require(underwater.discovered==6 && !underwater.entries.empty(),"physical double-door labels cannot be searched");
    for(const auto& entry:underwater.entries) require(entry.source.find("Underwater")!=std::string::npos || entry.destination.find("Underwater")!=std::string::npos,"search returned unrelated double door");
    // Broken native imports must preserve a previously valid seed.
    const auto savedTargets=doubled.entranceTargets;
    bool badNative=false; try { doubled.loadNativeEntrances("formatVersion: 3\nmEntranceOverrides:\n  bad: 1\n"); } catch(...) { badNative=true; }
    require(badNative && doubled.entranceTargets==savedTargets,"failed native load modified routes");
    // Every native endpoint gets a stable, collision-free persisted key.
    std::set<uint64_t> keys;
    for(const auto& e:data.at("entrance_edges")) require(keys.insert(entranceKey(e.at("id").get<std::string>())).second,"duplicate entrance journal key");
    require(Json(keys).dump().size()<10000,"entrance journal exceeds compact storage budget");
    // The actual tightrope route requires both actions, in different forms.
    auto tight=data;
    for(auto& a:tight["areas"]) if(a.at("Name")=="Root") a["Exits"]={{"Outside Castle Town South","Human_Link"}};
    model.resetSave(); model.load(tight);
    model.loadSeed("Hash: Tightrope\n# Settings\nLogic Rules: All Locations Reachable\nLanayru Twilight Cleared: On\n");
    model.inventory["Progressive Clawshot"]=0; model.inventory["Shadow Crystal"]=0;
    const auto check="Outside South Castle Town Tightrope Chest";
    model.solve(); require(model.accessible.at(check)==Truth::no,"tightrope open without gear");
    model.inventory["Progressive Clawshot"]=1; model.solve(); require(model.accessible.at(check)==Truth::no,"tightrope open without transformation");
    model.inventory["Progressive Clawshot"]=0; model.inventory["Shadow Crystal"]=1; model.solve(); require(model.accessible.at(check)==Truth::no,"tightrope open without clawshot");
    model.inventory["Progressive Clawshot"]=1; model.solve(); require(model.accessible.at(check)==Truth::yes,"human/wolf sequence locked");
    // Full and compact views must share the same result without expanding every macro by default.
    auto compact=model.checkDetails(check,false), full=model.checkDetails(check,true);
    require(compact.find("OPEN")!=std::string::npos && compact.size()<full.size(),"compact detail status/expansion broken");
    model.resetSave(); require(model.itemPlacements.empty() && model.visitedEntrances.empty(),"seed history leaked across saves");
    if(argc>2) {
        model.load(data); const auto raw=read(argv[2]); model.loadSeed(raw);
        if(argc>3) model.loadNativeEntrances(read(argv[3]));
        auto parsed=readSeedLog(raw);
        for(const auto& id:model.shuffledEdges) if(!model.entrances.contains(id)) std::cerr<<"Unresolved edge: "<<id<<"\n";
        require(model.entrancesComplete,"supplied seed has unresolved connections");
        require(!model.itemPlacements.empty(),"supplied seed placements not parsed");
        require(model.visitedEntrances.empty() && model.obtained.empty(),"loading full seed exposed progress");
        // Max-inventory evaluation catches destination errors without revealing rewards.
        for(const auto& i:data.at("items")) model.inventory[i.at("Name")]=99;
        model.solve();
        std::cout<<"Supplied seed: "<<parsed.connections.size()<<" log connections, "<<model.entrances.size()<<" directed graph mappings; no visits revealed.\n";
    }
    std::cout<<"Seed privacy, routing, traversal and tightrope tests passed.\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; } }
