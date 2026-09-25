#include "model.hpp"
#include "notebook.hpp"
#include "check_guides.hpp"
#include "stick_navigation.hpp"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace tracker;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        require(argc == 2, "catalogue path missing");
        StickNavigation stick;
        require(stick.sample(1000,500,0)==0,"stick drift triggers navigation");
        require(stick.sample(24000,0,1)==1,"right-stick page direction");
        require(stick.sample(24000,0,350)==0,"stick repeats before delay");
        require(stick.sample(24000,0,351)==1,"stick repeat missing");
        require(stick.sample(-24000,0,352)==-1,"direction reversal delayed");
        require(stick.sample(0,24000,353)==-2,"up should select previous row");
        require(stick.sample(0,-24000,354)==2,"down should select next row");
        require(stick.sample(0,0,355)==0 && stick.held==0,"neutral stick retains focus capture");
        require(stick.sample(20000,-28000,356)==2,"diagonal stick triggers two directions");
        std::ifstream input(argv[1]);
        Json data; input >> data;
        Model model; model.load(data);
        CheckGuides guides;
        require(guides.blocks("unknown")[1].find("unavailable")!=std::string::npos,"missing guides must fail gracefully");
        std::ifstream guideInput(std::filesystem::path(argv[1]).parent_path()/"check_guides.json");
        Json guideData; guideInput >> guideData; guides.load(guideData);
        require(guideData.at("checks").size()==data.at("checks").size(),"guide catalogue coverage differs");
        for(const auto& check : data.at("checks")) {
            const auto blocks=guides.blocks(check.at("name"));
            require(blocks.size()>=4,"check lacks collection directions");
            require(blocks[0].find("How to obtain")!=std::string::npos,"guide heading missing");
            require(blocks.back().find("Sources:")!=std::string::npos,"guide attribution missing");
        }
        const auto rupeeGuide=guides.blocks("Ordon Shield House Ledge Grass Rupee")[1];
        require(rupeeGuide.find("night")!=std::string::npos && rupeeGuide.find("Boomerang")!=std::string::npos && rupeeGuide.find("Clawshot")!=std::string::npos,"ledge rupee collection method incomplete");
        require(guides.blocks("Ordon Hint Sign")[2].find("Notes")!=std::string::npos,"hint sign directions omit recording");
        require(guides.blocks("City in the Sky Big Key Chest")[1].find("unavailable")==std::string::npos,"City in the Sky guide alias missing");
        require(guides.blocks("Defeat Ganondorf")[1].find("Ganondorf")!=std::string::npos,"Ganondorf guide alias missing");
        Json unsafe=guideData;
        unsafe["checks"]["Coro Lantern"]["steps"]=Json::array({"<button onclick='evil'> & collectible"});
        guides.load(unsafe);
        require(guides.blocks("Coro Lantern")[1].find("<button")==std::string::npos && guides.blocks("Coro Lantern")[1].find("&lt;button")!=std::string::npos,"guide text injected markup");
        guides.load(guideData);
        auto malformed=guideData;
        malformed["checks"]["Coro Lantern"]["sources"]=Json::array({"missing source"});
        bool guideRejected=false;
        try { guides.load(malformed); } catch(const std::exception&) { guideRejected=true; }
        require(guideRejected && guides.blocks("Ordon Shield House Ledge Grass Rupee")[1]==rupeeGuide,"invalid guide reload changed valid data");

        for(const auto& [setting,options] : data.at("setting_options").items()) {
            for(size_t i=0;i<options.size();++i) for(size_t j=0;j<options.size();++j) {
                model.settings[setting]=options[i];
                auto option=options[j].get<std::string>(), token=setting;
                std::replace(token.begin(),token.end(),' ','_');
                std::replace(option.begin(),option.end(),' ','_');
                if(model.evaluate(token+" >= "+option)!=(i>=j?Truth::yes:Truth::no)) throw std::runtime_error("ordered setting >= mismatch: "+setting+" "+options[i].get<std::string>()+" / "+option);
                require(model.evaluate(token+" <= "+option)==(i<=j?Truth::yes:Truth::no),"ordered setting <= mismatch");
            }
        }
        model.settings=Json::object();
        std::ifstream positionInput(std::filesystem::path(argv[1]).parent_path()/"positions.json");
        Json positions; positionInput >> positions;
        model.loadExteriorChecks(positions);
        size_t exteriorChecks=0;
        for (const auto& check : model.catalogue.at("checks")) {
            const std::string name=check.at("name");
            if (dungeonCheck(check) || !positions.contains(name)) continue;
            for (const auto& point : positions.at(name)) {
                if (!point.contains("overworld")) continue;
                model.stage=point.at("overworld").at("stage");
                require(model.inArea(check),"interior missing from exterior Current Area");
                ++exteriorChecks;
            }
        }
        require(exteriorChecks>=120,"exterior Current Area coverage unexpectedly reduced");
        model.stage="F_SP00";
        size_t ranchChecks=0;
        for (const auto& check : model.catalogue.at("checks")) ranchChecks+=model.inArea(check);
        require(ranchChecks==2,"Ordon Ranch must include goat reward and grotto check");
        // All four water grottos reuse this stage/room, but not the scene layer.
        model.stage="D_SB09"; model.room=4;
        for (int layer : {0,2,3,4}) {
            model.layer=layer;
            size_t count=0;
            for (const auto& check : model.catalogue.at("checks")) {
                if (!model.inArea(check)) continue;
                ++count;
                require(check.at("grotto_scenes")[0].at("layer")==layer,"wrong grotto shares Current Area");
            }
            require(count==1,"water grotto must show exactly its own check");
        }
        model.layer=-1;
        for (const auto& check : model.catalogue.at("checks"))
            require(!model.inArea(check),"unknown grotto layer exposes unrelated checks");
        model.stage="F_SP00";
        for (const auto& check : model.catalogue.at("checks"))
            if (check.at("name")=="Herding Goats Reward")
                require(model.inArea(check),"Ordon Ranch event reward missing from Current Area");
        model.stage="F_SP115";
        size_t lakeChecks=0;
        for (const auto& check : model.catalogue.at("checks")) lakeChecks+=model.inArea(check);
        require(lakeChecks>10,"Lake Hylia Current Area lost checks or pagination");
        model.stage.clear();
        require(pageCount(0,10)==1 && pageCount(10,10)==1 && pageCount(14,10)==2 && pageCount(40,10)==4,"check pagination boundaries");
        require(model.inventoryMaximum("Progressive Sword")==4 && model.inventoryMaximum("Progressive Clawshot")==2,"upgrade maximums");
        require(model.inventoryMaximum("Empty Bottle")==4 && model.inventoryMaximum("Bomb Bag")==3 && model.inventoryMaximum("Poe Soul")==60,"collectible maximums");
        require(model.inventoryMaximum("Forest Temple Small Key")==4 && model.inventoryMaximum("Male Beetle")==1,"key/unique maximums");
        model.settings["Golden Bugs"]="Off";model.settings["Poe Souls"]="Vanilla";
        for (const auto& c:model.catalogue.at("checks")) {
            const auto type=checkType(c);
            if (type==2 || type==3) require(model.enabled(c),"unshuffled collectible hidden by display filter");
            if (c.at("name")=="Faron Woods Owl Statue Chest") require(type==0,"owl chest uses statue category");
            if (c.at("name")=="Faron Woods Owl Statue Sky Character") require(type==6,"sky character lost statue category");
            if (c.at("name")=="Hyrule Castle Main Hall Southwest Chest") require(dungeonCheck(c),"castle dungeon not excluded from world map");
        }
        model.settings=Json::object();
        require(model.catalogue["checks"].size() == 658, "catalogue count changed; review export");
        require(model.give("Wooden Sword Chest"), "named grant missing");
        require(!model.give("Wooden Sword Chest"), "duplicate grant counted");
        require(!model.give("not a check"), "unknown grant marked complete");
        require(model.give("sky:F_SP108:8"), "sky character generated grant not recognized");
        require(model.obtained.contains("Faron Woods Owl Statue Sky Character"), "sky grant resolved wrong location");
        require(!model.give("sky:F_SP108:8"), "sky grant not deduplicated");
        model.stage = "R_SP01";
        require(model.inArea(data["checks"][0]), "chest stage mismatch");
        model.stage = "D_MN01";
        require(!model.inArea(data["checks"][0]), "cross-stage collision");
        model.inventory["Progressive Clawshot"] = 1;
        require(model.evaluate("count(Progressive_Clawshot, 2)") == Truth::no, "progressive count");
        model.inventory["Progressive Clawshot"] = 2;
        require(model.evaluate("count(Progressive_Clawshot, 2)") == Truth::yes, "progressive upgrade");
        require(model.evaluate("Nothing or Missing_Item and Impossible") == Truth::yes, "precedence");
        require(model.evaluate("Missing_Item and Nothing") == Truth::unknown, "unknown became accessible");
        require(model.evaluate("Human_Link and Wolf_Link", 0) == Truth::no, "form conflation");
        require(model.evaluate("Human_Link", 2) == Truth::no, "wolf counted as human");
        require(model.evaluate("Twilight", 4) == Truth::yes, "twilight form unsupported");
        require(model.evaluate("Human_Link or Wolf_Link", 4) == Truth::no, "twilight conflated with cleared forms");
        model.inventory["Progressive Dominion Rod"] = 1;
        require(model.evaluate("Restored_Dominion_Rod",0) == Truth::no, "unrestored rod opens owl checks");
        model.inventory["Progressive Dominion Rod"] = 2;
        require(model.evaluate("Restored_Dominion_Rod",0) == Truth::yes, "restored rod remains unknown");
        model.inventory["Faron Twilight Tear"] = 16;
        model.countedItems.insert("Faron Twilight Tear");
        require(model.evaluate("count(Faron_Twilight_Tear,16)") == Truth::yes, "tear counts unsupported");
        model.inventory["Heart Count"] = 8;
        require(model.evaluate("hearts(8)") == Truth::yes && model.evaluate("hearts(9)") == Truth::no, "heart requirement bounds");
        model.events["Can Complete Forest Temple"] = Truth::yes;
        require(model.evaluate("dungeons_completed(1)") == Truth::yes, "dungeon predicate unsupported");
        require(escape("<a&\"'>") == "&lt;a&amp;&quot;&#39;&gt;", "RML escaping");
        model.solve();
        require(model.accessible.at("Wooden Sword Chest") == Truth::unknown, "missing seed guessed");
        model.loadSeed("Hash: Test Seed\n# Settings\nLogic Rules: All Locations Reachable\nStarting Form: Human\nStarting Time of Day: Noon\nSkip Prologue: On\nFaron Twilight Cleared: On\nEldin Twilight Cleared: On\nLanayru Twilight Cleared: On\nRandomize Dungeon Entrances: Off\n");
        auto before = model.seed;
        bool rejected = false;
        try { model.loadSeed("{\"seed\":\"bad\",\"settings\":[]}"); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "invalid seed accepted");
        require(model.seed == before, "failed seed load changed state");
        const auto start = std::chrono::steady_clock::now();
        model.solve();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Catalogue solve: " << elapsed << " ms\n";
        require(elapsed < 5000, "logic solve unexpectedly slow");
        model.loadSeed("Hash: Shuffled\n# Settings\nLogic Rules: All Locations Reachable\nRandomize Dungeon Entrances: On\n");
        model.solve();
        require(model.accessible.contains("Wooden Sword Chest"), "vanilla solve missing check");
        model.loadSeed("{\"seed\":\"no logic\",\"settings\":{\"Logic Rules\":\"No Logic\"}}");
        model.solve();
        require(model.accessible.at("Wooden Sword Chest") == Truth::yes, "no logic setting ignored");
        // Tiny graph: vanilla routing cannot be bypassed by a shuffled edge.
        Model graph;
        auto fixture = data;
        fixture["areas"] = Json::parse(R"json([
            {"Name":"Root","Can Transform":"Never","Exits":{"A":"Nothing"}},
            {"Name":"A","Can Transform":"Never","Exits":{"B":"count(Progressive_Clawshot, 2)"}},
            {"Name":"B","Can Transform":"Never"}])json");
        fixture["checks"][0]["access"] = Json::parse(R"([{"area":"B","requirement":"Nothing"}])");
        graph.load(fixture);
        graph.inventory["Progressive Clawshot"] = 1;
        graph.loadSeed(R"({"seed":"test","settings":{"Logic Rules":"All Locations Reachable"}})");
        graph.solve();
        require(graph.accessible.at("Wooden Sword Chest") == Truth::no, "locked path bypassed");
        graph.inventory["Progressive Clawshot"] = 2;
        graph.solve();
        require(graph.accessible.at("Wooden Sword Chest") == Truth::yes, "unlocked path not reached");
        graph.inventory["Progressive Clawshot"] = 0;
        graph.loadSeed(R"({"seed":"test","settings":{"Logic Rules":"All Locations Reachable","Randomize Dungeon Entrances":"On"},"entrances_complete":true,"entrances":{"Root -> A":"B"}})");
        graph.solve();
        require(graph.accessible.at("Wooden Sword Chest") == Truth::no, "shuffled entrance overrode vanilla logic");
        fixture["areas"] = Json::parse(R"([
            {"Name":"Root","Can Transform":"Never","Exits":{"A":"Twilight"}},
            {"Name":"A","Twilight":"Faron","Can Transform":"Never"}])");
        fixture["checks"][0]["access"] = Json::parse(R"([{"area":"A","requirement":"Twilight"}])");
        graph.load(fixture);
        graph.loadSeed(R"({"seed":"twilight","settings":{"Logic Rules":"All Locations Reachable","Faron Twilight Cleared":"Off"}})");
        graph.inventory["Faron Twilight Tear"] = 0;
        graph.countedItems.insert("Faron Twilight Tear");
        graph.solve();
        require(graph.accessible.at("Wooden Sword Chest") == Truth::yes, "uncleared twilight insects unreachable");
        graph.settings["Faron Twilight Cleared"] = "On";
        graph.solve();
        require(graph.accessible.at("Wooden Sword Chest") == Truth::no, "cleared province still exposes twilight-only insects");
        auto nullable = data;
        nullable["checks"][0]["group"] = nullptr;
        nullable["checks"][0]["original_item"] = nullptr;
        Model normalized; normalized.load(nullable);
        require(normalized.catalogue["checks"][0]["group"] == "Other", "null group not normalized");
        require(normalized.catalogue["checks"][0]["original_item"] == "", "null item not normalized");
        for (const auto& check : model.catalogue.at("checks")) {
            if (check.value("original_item",std::string{}) != "Faron Twilight Tear") continue;
            model.settings["Faron Twilight Cleared"] = "On";
            require(!model.enabled(check), "skipped twilight check still visible");
        }
        Model agitha; agitha.load(data); agitha.seedLoaded=true;
        agitha.inventory["Male Ladybug"]=1; agitha.inventory["Female Ladybug"]=1;
        agitha.inventory["Male Beetle"]=0;
        agitha.solve();
        require(agitha.accessible.at("Agitha Male Ladybug Reward")==Truth::yes, "owned male bug reward locked by route logic");
        require(agitha.accessible.at("Agitha Female Ladybug Reward")==Truth::yes, "owned female bug reward locked by route logic");
        require(agitha.accessible.at("Agitha Male Beetle Reward")==Truth::no, "unowned bug reward open");
        agitha.obtained.insert("Agitha Male Ladybug Reward"); agitha.solve();
        require(agitha.accessible.at("Agitha Male Ladybug Reward")==Truth::no, "delivered bug remains available");
        require(agitha.accessible.at("Agitha Female Ladybug Reward")==Truth::yes, "delivery removed other bug reward");
        model.resetSave();
        require(model.obtained.empty() && model.inventory.empty() && !model.seedLoaded, "save slot leaked");
        Model warp; warp.load(data);
        for (const auto& item : data.at("items")) warp.inventory[item.at("Name")]=0;
        warp.loadSeed("Hash: Warp regression\n# Settings\nLogic Rules: All Locations Reachable\nStarting Form: Human\nStarting Time of Day: Noon\nSkip Prologue: On\nFaron Twilight Cleared: On\nEldin Twilight Cleared: On\nLanayru Twilight Cleared: On\nSkip Midna's Desperate Hour: On\nUnlock Map Regions: On\nLogic Transform Anywhere: On\nFaron Woods Logic: Open\n");
        warp.inventory["Lake Hylia Portal"]=1;
        Model coro=warp;
        coro.settings["Small Keys"]="Own Dungeon";
        coro.inventory["Lantern"]=1;
        coro.solve();
        require(coro.accessible.at("Faron Mist Stump Chest")!=Truth::yes,"Coro gate bypassed without key or wolf");
        coro.inventory["Faron Woods Coro Key"]=1;
        coro.solve();
        require(coro.accessible.at("Faron Mist Stump Chest")==Truth::yes,"Coro key and Lantern must open mist chest");
        const auto detail=coro.checkDetails("Faron Mist Stump Chest");
        require(detail.find("Lantern")!=std::string::npos && detail.find("Area access: OPEN")!=std::string::npos,"details omit actual requirements/access");
        require(!coro.skipped.contains("Faron Mist Stump Chest"),"viewing details skips check");
        coro.toggleSkipped("Faron Mist Stump Chest");
        require(coro.checkDetails("Faron Mist Stump Chest").find("SKIPPED")!=std::string::npos,"details missing skipped status");
        coro.inventory["Lantern"]=0; coro.solve();
        require(coro.accessible.at("Faron Mist Stump Chest")!=Truth::yes,"Coro key bypasses Lantern");
        coro.inventory["Lantern"]=1; coro.inventory["Faron Woods Coro Key"]=0;
        coro.settings["Small Keys"]="Keysy"; coro.solve();
        require(coro.accessible.at("Faron Mist Stump Chest")==Truth::yes,"Keysy fails Coro gate");
        coro.settings["Small Keys"]="Own Dungeon"; coro.inventory["Shadow Crystal"]=1; coro.solve();
        require(coro.accessible.at("Faron Mist Stump Chest")==Truth::yes,"Shadow Crystal fails Coro gate");
        require(coro.checkDetails("missing").find("no longer available")!=std::string::npos,"stale details not handled");
        warp.inventory["Castle Town Portal"]=1;
        warp.solve();
        require(warp.reached.at("Warp Portals")[2]!=Truth::yes,"warp opened without Shadow Crystal");
        warp.inventory["Shadow Crystal"]=1;
        warp.solve();
        require(warp.events.at("Can Warp")==Truth::yes,"generated warp event missing");
        require(warp.events.at("Lanayru Province Map Sector")==Truth::yes,"generated map-sector event missing");
        require(warp.reached.at("Warp Portals")[2]==Truth::yes,"Shadow Crystal did not open portals");
        require(warp.accessible.at("Flight By Fowl Second Platform Chest")==Truth::yes,"Lake Hylia warp checks remain inaccessible");
        require(warp.reached.at("Castle Town West")[0]==Truth::yes,"Castle Town warp remains inaccessible");
        require(warp.accessible.at("Lake Hylia Underwater Chest")!=Truth::yes,"warp bypasses local item requirements");
        // Ball and Chain substitutes for the pickup tool, never for area access.
        int ballBugs=0;
        for (const auto& check : data.at("checks")) for (const auto& route : check.at("access")) {
            const auto req=route.at("requirement").get<std::string>();
            if (std::find(check.at("categories").begin(),check.at("categories").end(),"Golden Bug")==check.at("categories").end() || req.find("Ball_and_Chain")==std::string::npos) continue;
            warp.inventory["Ball and Chain"]=0;
            require(warp.evaluate(req,0)==Truth::no,"bug open without pickup tool");
            warp.inventory["Ball and Chain"]=1;
            require(warp.evaluate(req,0)==Truth::yes,"Ball and Chain bug pickup not recognized");
            require(warp.evaluate(req,2)==Truth::no,"wolf can use Ball and Chain");
            ++ballBugs;
        }
        require(ballBugs==9,"expected nine tool-gated bug pickup routes");
        // The async worker must return the same area/event state that produced
        // OPEN/LOCKED, without overwriting notes, skips or the live inventory.
        auto worker=warp; worker.solve();
        const auto expectedAreas=worker.reached;
        const auto expectedEvents=worker.events;
        auto result=worker.takeLogicResult();
        Model live=warp; live.reached.clear(); live.events.clear(); live.accessible.clear();
        live.skipped.insert("Faron Mist Stump Chest");
        live.inventory["Lantern"]=1;
        live.applyLogicResult(std::move(result));
        require(live.reached==expectedAreas && live.events==expectedEvents,"worker discarded area/event results");
        require(live.checkDetails("Flight By Fowl Second Platform Chest").find("Area access: OPEN")!=std::string::npos,"async details report UNKNOWN for a reachable area");
        require(live.skipped.contains("Faron Mist Stump Chest") && live.inventory.at("Lantern")==1,"worker replaced live user state");
        // Each rupee type has its own seed switch and map visibility category.
        int freeRupees=0,hiddenRupees=0;
        for(const auto& check:data.at("checks")) {
            const int type=checkType(check);
            if(type!=8 && type!=9) continue;
            const char* setting=type==8 ? "Freestanding Rupees" : "Hidden Rupees";
            type==8 ? ++freeRupees : ++hiddenRupees;
            live.settings["Freestanding Rupees"]=live.settings["Hidden Rupees"]="On";
            require(live.enabled(check),"enabled rupee excluded from tracker");
            live.settings[setting]="Off";
            require(!live.enabled(check),"disabled seed rupee included in tracker");
            live.settings.erase(setting);
            require(!live.enabled(check),"rupee included without an enabled seed setting");
        }
        require(freeRupees==51 && hiddenRupees==37,"rupee category coverage incomplete");
        Model rupeeCounts; rupeeCounts.load(data);
        rupeeCounts.settings["Freestanding Rupees"]=rupeeCounts.settings["Hidden Rupees"]="On";
        bool visibleTypes[10]={true,true,true,true,true,true,true,true,true,true};
        for(const auto& check:data.at("checks")) {
            const int type=checkType(check);
            if(type<8 || !dungeonCheck(check)) continue;
            const std::string name=check.at("name"), dungeon=check.at("group");
            rupeeCounts.accessible.clear(); rupeeCounts.accessible[name]=Truth::yes;
            require(rupeeCounts.availableDungeonChecks(dungeon,visibleTypes)==1,"enabled dungeon rupee absent from temple count");
            visibleTypes[type]=false;
            require(rupeeCounts.availableDungeonChecks(dungeon,visibleTypes)==0,"hidden rupee category included in temple count");
            require(rupeeCounts.availableDungeonChecks(dungeon)==1,"map filter changed unfiltered availability");
            visibleTypes[type]=true;
        }
        // Isolate the dungeon route from the overworld approach. Local combat
        // equipment alone must not bypass locked doors on the way to Chapel.
        auto snowData=data;
        for(auto& area:snowData["areas"]) if(area["Name"]=="Root")
            {
            area["Exits"]={{"Snowpeak Ruins Entrance","Human_Link"}};
            area["Events"]={{"Can Refill Regular Bombs","Nothing"}};
        }
        Model snow; snow.load(snowData); snow.seedLoaded=true; snow.settings=warp.settings;
        snow.settings["Small Keys"]="Own Dungeon";
        for(const auto& item:data.at("items")) snow.inventory[item.at("Name")]=0;
        snow.inventory["Shadow Crystal"]=1; snow.inventory["Progressive Sword"]=2;
        snow.inventory["Ball and Chain"]=1; snow.inventory["Bomb Bag"]=2;
        snow.countedItems.insert("Snowpeak Ruins Small Key");
        snow.solve();
        require(snow.accessible.at("Snowpeak Ruins Chapel Chest")!=Truth::yes,"Chapel route bypasses small keys");
        snow.inventory["Snowpeak Ruins Small Key"]=4; snow.inventory["Ordon Cheese"]=1; snow.solve();
        require(snow.accessible.at("Snowpeak Ruins Chapel Chest")==Truth::yes,"equipped four-key Snowpeak route remains locked");
        require(snow.accessible.at("Snowpeak Ruins Broken Floor Chest")==Truth::yes,"reachable broken-floor chest remains locked");
        require(snow.checkDetails("Snowpeak Ruins Chapel Chest").find("Area access: OPEN")!=std::string::npos,"Chapel area access differs from check state");
        snow.inventory["Snowpeak Ruins Small Key"]=0; snow.inventory["Ordon Cheese"]=0; snow.settings["Small Keys"]="Keysy"; snow.solve();
        require(snow.accessible.at("Snowpeak Ruins Chapel Chest")==Truth::yes,"Keysy Chapel route incorrectly needs keys");
        Model shops; shops.load(data);
        shops.settings=warp.settings; shops.seedLoaded=true;
        for (const auto& item : data.at("items")) shops.inventory[item.at("Name")]=0;
        shops.inventory["Shadow Crystal"]=1;
        shops.inventory["Kakariko Village Portal"]=1;
        shops.solve();
        require(shops.events.at("Can Farm Lots of Rupees")!=Truth::yes,"shop regression unexpectedly has rupee-farming route");
        for (const auto* name : {"Kakariko Village Malo Mart Hylian Shield", "Kakariko Village Malo Mart Red Potion", "Kakariko Village Malo Mart Wooden Shield", "Barnes Bomb Bag"})
            require(shops.accessible.at(name)==Truth::yes,"Kakariko shop locked by unrelated rupee-farming route");
        require(shops.accessible.at("Kakariko Village Malo Mart Hawkeye")!=Truth::yes,"Hawkeye unlock bypassed");
        require(shops.accessible.at("Renados Letter")!=Truth::yes,"Renado's letter quest requirement bypassed");
        shops.inventory["Shadow Crystal"]=0;
        shops.settings["Skip Prologue"]="Off";
        shops.settings["Faron Woods Logic"]="Closed";
        shops.solve();
        require(shops.accessible.at("Barnes Bomb Bag")!=Truth::yes,"shop access bypasses route to Kakariko");
        Model templeCounts; templeCounts.load(data);
        std::vector<std::string> forest;
        for (const auto& check : data.at("checks")) {
            const auto& cats=check.at("categories");
            if (dungeonCheck(check) && std::find(cats.begin(),cats.end(),"Forest Temple")!=cats.end() && forest.size()<3)
                forest.push_back(check.at("name"));
        }
        require(forest.size()==3,"Forest Temple test data missing");
        for (const auto& name : forest) templeCounts.accessible[name]=Truth::yes;
        templeCounts.accessible["Goron Mines Entrance Chest"]=Truth::yes;
        templeCounts.accessible["Forest Temple Hint Sign"]=Truth::yes;
        require(templeCounts.availableDungeonChecks("Forest Temple")==3,"temple count includes other dungeons or hints");
        templeCounts.obtained.insert(forest[0]);
        require(templeCounts.availableDungeonChecks("Forest Temple")==2,"completed dungeon check still available");
        templeCounts.accessible[forest[1]]=Truth::unknown;
        require(templeCounts.availableDungeonChecks("Forest Temple")==1,"unknown dungeon check counted open");
        templeCounts.accessible[forest[2]]=Truth::no;
        require(templeCounts.availableDungeonChecks("Forest Temple")==0,"locked temple should use inactive icon");
        templeCounts.accessible[forest[2]]=Truth::yes;
        const auto inventoryBeforeSkip=templeCounts.inventory;
        require(templeCounts.toggleSkipped(forest[2]),"unfinished check cannot be skipped");
        require(templeCounts.availableDungeonChecks("Forest Temple")==0,"skipped dungeon check counted available");
        require(!templeCounts.obtained.contains(forest[2]) && templeCounts.inventory==inventoryBeforeSkip,"skip grants an item or marks completion");
        require(templeCounts.toggleSkipped(forest[2]) && templeCounts.availableDungeonChecks("Forest Temple")==1,"restoring skipped check failed");
        require(!templeCounts.toggleSkipped(forest[0]),"completed check can be skipped");
        require(!templeCounts.toggleSkipped("not a check"),"unknown check can be skipped");
        templeCounts.toggleSkipped(forest[2]);
        require(templeCounts.give(forest[2]) && !templeCounts.skipped.contains(forest[2]),"real pickup must supersede skip");
        templeCounts.toggleSkipped(forest[1]);
        templeCounts.resetSave();
        require(templeCounts.skipped.empty(),"skips leak across save slots");
        require(hintPlainText("\x1b" "CC[ff0000ff]A hint\x1b" "GC[ffffffff]\nSecond line")=="A hint\nSecond line","hint formatting leaks into notes");
        require(hintPlainText("safe\x1b" "CC[broken")=="safe","malformed formatting overread");
        std::ifstream signsFile(std::filesystem::path(argv[1]).parent_path()/"hint_signs.json");
        Json signs=Json::parse(signsFile);
        require(signs.size()==34,"hint catalogue incomplete");
        for (const auto& sign : signs) {
            const auto& p=sign.at("pos");
            const std::string name=sign.at("name"),stage=sign.at("stage");
            require(templeCounts.aliases.contains(name),"hint sign does not match tracker check");
            require(hintSignAt(signs,stage,sign.at("room"),p[0],p[1],p[2])==name,"hint coordinates mismatch");
            require(hintSignAt(signs,"wrong_stage",sign.at("room"),p[0],p[1],p[2]).empty(),"hint matched wrong stage");
            require(hintSignAt(signs,stage,999,p[0],p[1],p[2]).empty(),"hint matched wrong room");
        }
        require(escape("<note>&") == "&lt;note&gt;&amp;","notes must escape markup");
        std::vector<std::string> notes;
        require(!addPersonalNote(notes," \n "),"blank note accepted");
        require(addPersonalNote(notes,"  first note  ") && notes[0]=="first note","note submission failed");
        require(addPersonalNote(notes,"<second>& note"),"second note failed");
        require(deletePersonalNote(notes,0) && notes.size()==1 && notes[0]=="<second>& note","deleting note changed wrong entry");
        require(!deletePersonalNote(notes,9),"invalid note deletion accepted");
        require(!addPersonalNote(notes,std::string(1001,'x')),"oversized note accepted");
        auto saved=Json(notes).dump();
        require(Json::parse(saved).get<std::vector<std::string>>()==notes,"note list did not roundtrip");
        notes.assign(64,"note");
        require(!addPersonalNote(notes,"overflow"),"note count limit ignored");
        std::cout << "TPTracker model tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
