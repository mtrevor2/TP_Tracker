#pragma once
#include "model.hpp"
#include <algorithm>
#include <optional>
#include <string_view>

namespace tracker {
// Native save events, from Dusklight's d_save_bit_labels.inc. These are
// observations, never events inferred from owning enough items to do a quest.
inline const std::map<std::string, unsigned> vanillaStoryEvents = {
    {"Can Complete Prologue",0x0580}, {"Returned From Prison",0x0502},
    {"Can Complete MDH",0x1e08}, {"Can Complete Forest Temple",0x0602},
    {"Can Complete Goron Mines",0x0701}, {"Can Complete Lakebed Temple",0x0904},
    {"Can Complete Arbiters Grounds",0x2010}, {"Can Complete Snowpeak Ruins",0x2008},
    {"Can Complete Temple of Time",0x2004}, {"Can Complete City in the Sky",0x2002},
    {"Can Complete Palace of Twilight",0x4680}, {"Can Wrestle Goron Elder",0x0704},
    {"Can Follow Rutella",0x0810}, {"Reekfish Scent",0x6120},
    {"Howl at Death Mountain Howling Stone",0x3a80},
    {"Howl at Upper Zoras River Howling Stone",0x3a40},
    {"Howl at North Faron Woods Howling Stone",0x3a20},
    {"Howl at Lake Hylia Howling Stone",0x3a10},
    {"Howl at Snowpeak Mountain Howling Stone",0x3a08},
    {"Howl at Hidden Village Howling Stone",0x3a04},
    {"Faron Vessel",0x1380}, {"Eldin Vessel",0x1a01}, {"Lanayru Vessel",0x4b04},
    {"Faron Twilight Entered",0x0640}, {"Epona Rescued",0x0601},
    {"Zora Ice Melted",0x0880}, {"Midna Injured",0x0c01},
    {"Master Sword Drawn",0x2020}, {"Can Fund Malo Mart",0x2e20},
    {"Castle Town Shop Open",0x2210}, {"Eldin Bridge Missing",0x0a20},
    {"Eldin Bridge Restored",0x0f08}, {"Can Warp",0x0604}
};
inline int vanillaCount(const Model& model, const std::string& key) {
    const auto i=model.inventory.find(key);
    return i==model.inventory.end() ? 0 : std::max(0,i->second);
}
inline std::optional<Truth> vanillaEvent(const Model& model, std::string name) {
    if (!model.vanilla) return {};
    if(name.size()>1 && name.front()=='\'' && name.back()=='\'') name=name.substr(1,name.size()-2);
    if(name=="Can Warp" && vanillaCount(model,"Story Midna Injured") && !vanillaCount(model,"Shadow Crystal")) return Truth::no;
    if(name=="Can Midna Jump" && vanillaCount(model,"Story Midna Injured") && !vanillaCount(model,"Story Can Complete MDH")) return Truth::no;
    if(vanillaStoryEvents.contains(name) || name=="Can Complete Faron Twilight" ||
       name=="Can Complete Eldin Twilight" || name=="Can Complete Lanayru Twilight")
        return vanillaCount(model,"Story "+name)>0 ? Truth::yes : Truth::no;
    return {};
}
// Boundaries that the randomizer deliberately opens or shortcuts. These
// supplement the catalogue's item/door requirements only during normal Play.
inline bool vanillaAreaAllowed(const Model& model, const std::string& area) {
    auto done=[&](const char* event) { return vanillaCount(model,std::string("Story ")+event)>0; };
    if(area=="Outside Links House" && done("Can Complete Prologue") && !done("Returned From Prison")) return false;
    if(area=="Faron Field" && !done("Can Complete Forest Temple")) return false;
    if((area=="North Eldin Field" || area=="Lanayru Field" || area=="Outside Castle Town East" ||
        area=="Outside Castle Town South" || area=="Lake Hylia Bridge South") && !done("Can Complete Goron Mines")) return false;
    if(area=="North Faron Lost Woods Ledge" && !done("Can Complete MDH")) return false;
    if(area=="Sacred Grove Past" && !done("Can Complete Snowpeak Ruins")) return false;
    if((area=="Gerudo Desert" || area=="Snowpeak Climb Lower") && !done("Master Sword Drawn")) return false;
    if(area=="Snowpeak Climb Lower" && !done("Can Complete Arbiters Grounds")) return false;
    if(area=="Hidden Village" && !done("Can Complete Temple of Time")) return false;
    if(area=="Castle Town Malo Mart" && !done("Castle Town Shop Open")) return false;
    if(area=="Forest Temple Entrance" && !done("Can Complete Faron Twilight")) return false;
    if(area=="City in the Sky Entrance" && !vanillaCount(model,"Sky Cannon Repaired")) return false;
    return true;
}
inline bool vanillaEdgeAllowed(const Model& model,const std::string& from,const std::string& to) {
    if(!vanillaAreaAllowed(model,to)) return false;
    if((from=="Eldin Field North of Bridge" && to=="North Eldin Field") ||
       (to=="Eldin Field North of Bridge" && from=="North Eldin Field"))
        return !vanillaCount(model,"Story Eldin Bridge Missing") || vanillaCount(model,"Story Eldin Bridge Restored");
    return true;
}
inline std::string vanillaExitRequirement(const Model& model,const std::string& from,
                                          const std::string& to,const std::string& original) {
    if(!model.vanilla) return original;
    const bool returned=vanillaCount(model,"Story Returned From Prison")>0;
    // The prologue's human sword/slingshot gate is gone after escaping prison.
    if(returned && ((from=="Ordon Spring" && to=="Ordon Bridge") ||
                    (from=="Ordon Bridge" && to=="Ordon Spring"))) return "Nothing";
    // Native wolf routes into Rusl's and Jaggle's houses are absent from the
    // randomized door graph. They are essential for the first sword/shield.
    if(returned && from=="Ordon Village" && (to=="Ordon Sword House" || to=="Ordon Shield House Upper Ledge"))
        return "("+original+") or Wolf_Link";
    return original;
}
inline std::string vanillaCheckRequirement(const Json& check) {
    const std::string name=check.at("name");
    if(name=="Wooden Sword Chest") return "Slingshot";
    if(name=="Coro Lantern") return "Human_Link";
    if(name=="Coro Gate Key" || name=="Coro Bottle") return "Story_Can_Complete_Faron_Twilight and Human_Link";
    if(name=="Ordon Shield" || name=="Ordon Sword") return "Story_Returned_From_Prison and Wolf_Link";
    if(name=="Wrestling With Bo") return "Story_Epona_Rescued and Story_Can_Complete_Eldin_Twilight";
    if(name=="Herding Goats Reward") return "Story_Epona_Rescued";
    if(name=="Barnes Bomb Bag" || name=="Kakariko Village Malo Mart Hawkeye") return "Story_Can_Complete_Goron_Mines";
    if(name=="Auru Gift To Fyer" || name=="Sacred Grove Pedestal Master Sword" || name=="Sacred Grove Pedestal Shadow Crystal") return "Story_Can_Complete_MDH";
    if(name=="Ashei Sketch" || name=="Gift From Ralis") return "Story_Can_Complete_Arbiters_Grounds";
    if(name=="Renados Letter" || name=="Telma Invoice" || name=="Wooden Statue" || name=="Ilia Charm" || name=="Ilia Memory Reward" || name=="Skybook From Impaz" || name=="Shad Dominion Rod") return "Story_Can_Complete_Temple_of_Time";
    if(name=="Snowboard Racing Prize") return "Story_Can_Complete_Snowpeak_Ruins";
    const auto& cats=check.at("categories");
    if(std::find(cats.begin(),cats.end(),"Twilit Insect")!=cats.end()) {
        const auto item=check.value("original_item",std::string{});
        for(const char* province:{"Faron","Eldin","Lanayru"})
            if(item.starts_with(province)) return std::string("Story_")+province+"_Vessel";
    }
    if(std::find(cats.begin(),cats.end(),"Sky Character")!=cats.end()) return "count(Progressive_Sky_Book, 1)";
    if(std::find(cats.begin(),cats.end(),"Golden Wolf")!=cats.end()) return "Human_Link";
    return "Nothing";
}
inline bool vanillaCheckEnabled(const Json& check) {
    const auto& cats=check.at("categories");
    // Refillable shop stock and loose rupees have no persistent collection
    // state in the base game. Hint signs are randomizer additions.
    for(const char* category:{"Hint Sign","Rupee - Freestanding","Rupee - Hidden"})
        if(std::find(cats.begin(),cats.end(),category)!=cats.end()) return false;
    const std::string name=check.at("name");
    if(name=="Castle Town Goron Shop Hylian Shield") return false;
    if(name.find("Shop Red Potion")!=std::string::npos || name.find("Shop Lantern Oil")!=std::string::npos ||
       name.find("Shop Arrow Refill")!=std::string::npos || name=="Kakariko Village Malo Mart Red Potion" ||
       name=="Kakariko Village Malo Mart Wooden Shield") return false;
    return true;
}
// nullopt means the catalogue's native flag can be used. Overrides prevent
// reused randomizer flags from completing unrelated vanilla checks.
template<class Event, class First, class Switch>
std::optional<bool> vanillaCompletion(const std::string& name, Event event, First first, Switch sw) {
    if(name=="Sacred Grove Pedestal Master Sword") return event(0x2020);
    if(name=="Sacred Grove Pedestal Shadow Crystal") return event(0x0d04);
    // GWolf's first deletion flag is F_0339 (Ending Blow), not F_0338 (Shield Attack).
    if(name=="Faron Woods Golden Wolf") return event(0x2904);
    if(name=="Shad Dominion Rod") return event(0x2580);
    if(name=="Coro Gate Key") return first(0xfe) || sw(2,0x0c);
    if(name=="Hyrule Field Amphitheater Owl Statue Sky Character") return event(0x6004);
    if(name=="Sera Shop Slingshot") return first(0x4b);
    if(name=="Ordon Sword") return first(0x28);
    if(name=="Ordon Shield") return first(0x2a);
    if(name=="Coro Lantern") return first(0x48);
    if(name=="Kakariko Village Malo Mart Hylian Shield") return event(0x6102);
    if(name=="Castle Town Goron Shop Hylian Shield") return {}; // native purchase flag is not persisted per vendor
    if(name=="Kakariko Village Malo Mart Hawkeye") return first(0x3e);
    if(name=="Castle Town Malo Mart Magic Armor") return first(0x30);
    if(name=="Forest Temple Gale Boomerang") return first(0x40);
    if(name=="Snowpeak Ruins Ball and Chain") return first(0x42);
    if(name=="Forest Temple Dungeon Reward") return event(0x0602);
    if(name=="Arbiters Grounds Dungeon Reward") return event(0x2010);
    if(name=="Temple of Time Dungeon Reward") return event(0x2004);
    if(name=="Palace of Twilight Collect Both Sols") return first(0x49);
    return {};
}
template<class Event, class First, class Slot, class Switch>
void vanillaInventory(std::map<std::string,int>& inventory, Event event, First first, Slot slot, Switch sw, int wallet) {
    for(const auto& [name,flag]:vanillaStoryEvents) inventory["Story "+name]=event(flag);
    inventory["Shadow Crystal"]=event(0x0d04);
    inventory["Progressive Wallet"]=wallet;
    int skill=0;
    for(unsigned flag:{0x2908,0x2904,0x2902,0x2901,0x2a80,0x2a40,0x2a20}) skill+=event(flag);
    inventory["Progressive Hidden Skill"]=skill;
    inventory["Progressive Sky Book"]=(slot(22)==0xe9 || slot(22)==0xea || slot(22)==0xeb) ? 1 : 0;
    if(inventory["Progressive Sky Book"])
        for(unsigned flag:{0x6080,0x6040,0x6020,0x6010,0x6008,0x6004}) inventory["Progressive Sky Book"]+=event(flag);
    inventory["Aurus Memo"]=(slot(19)==0x90 || event(0x2680));
    inventory["Faron Woods Coro Key"]=!event(0x0580) || first(0xfe) || sw(2,0x0c);
    // The rod works inside its own dungeon before Shad restores it outside.
    inventory["Progressive Dominion Rod"]=first(0x46) ? (event(0x2580) ? 2 : 1) : 0;
    struct Portal { const char* name; int save,flag; };
    for(const auto& p:std::initializer_list<Portal>{{"Ordon Spring",0,0x34},{"South Faron",2,0x47},
        {"North Faron",2,2},{"Kakariko Gorge",6,0x15},{"Kakariko Village",3,0x1f},
        {"Death Mountain",3,0x15},{"Bridge of Eldin",6,0x63},{"Castle Town",6,3},
        {"Lake Hylia",4,0xa},{"Upper Zoras River",4,0x15},{"Zoras Domain",4,2},
        {"Snowpeak",8,0x15},{"Gerudo Desert",10,0x15},{"Mirror Chamber",10,0x28},{"Sacred Grove",7,0x64}})
        inventory[std::string(p.name)+" Portal"]=sw(p.save,p.flag);
}
}
