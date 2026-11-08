#include "model.hpp"
#include "vanilla.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tracker;
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    Model m; std::ifstream file(argv[1]); Json catalogue; file>>catalogue; m.load(catalogue); m.loadVanilla();
    require(m.vanilla && m.seedLoaded && !m.shuffled,"normal play works without log");
    for(const auto& item:m.catalogue.at("items")) m.inventory[item.at("Name")]=0;
    for(const auto& [name,flag]:vanillaStoryEvents) m.inventory["Story "+name]=0;
    for(const char* p:{"Faron","Eldin","Lanayru"}) m.inventory[std::string("Story Can Complete ")+p+" Twilight"]=0;
    m.solve();
    require(m.accessible.at("Wooden Sword Chest")==Truth::no,"wooden sword not delivered before slingshot");
    require(m.accessible.at("Ordon Sword")==Truth::no,"ordon sword unavailable before prison return");
    require(m.accessible.at("Faron Woods Golden Wolf")==Truth::no,"first skill waits for real light restoration");
    m.inventory["Slingshot"]=1; m.solve();
    require(m.accessible.at("Wooden Sword Chest")==Truth::yes,"wooden sword delivered after slingshot");
    m.inventory["Progressive Sword"]=1;
    m.inventory["Story Can Complete Prologue"]=m.inventory["Story Returned From Prison"]=1;
    m.inventory["Story Faron Vessel"]=1;
    m.settings["Starting Form"]="Wolf";
    m.solve();
    require(m.accessible.at("Ordon Sword")==Truth::yes,"wolf can dig into sword house after prison return");
    require(m.accessible.at("Ordon Shield")==Truth::yes,"wolf can enter shield house upper ledge");
    require(m.accessible.at("South Faron Woods Coros House Exterior Twilit Insect")==Truth::yes,"wolf can leave Ordon for Faron twilight");
    require(m.accessible.at("Coro Lantern")==Truth::no,"Coro cannot give lantern to wolf");
    m.settings["Starting Form"]="Human";
    // Equipment must not manufacture completed bosses or advance the story.
    for(auto& [name,count]:m.inventory) if(!name.starts_with("Story ")) count=99;
    m.inventory["Story Can Complete Prologue"]=m.inventory["Story Returned From Prison"]=1;
    m.inventory["Story Can Complete Faron Twilight"]=1;
    m.solve();
    require(m.events.at("Can Complete Forest Temple")==Truth::no,"beatable boss is not cleared boss");
    require(m.accessible.at("Kakariko Gorge Spire Heart Piece")==Truth::no,"no Eldin access before forest clear");
    require(m.accessible.at("Sacred Grove Pedestal Master Sword")==Truth::no,"no master sword before MDH");
    m.inventory["Story Can Complete Forest Temple"]=1; m.solve();
    require(m.events.at("Can Complete Forest Temple")==Truth::yes,"saved boss clear is authoritative");
    require(!vanillaAreaAllowed(m,"Lanayru Field"),"Lanayru waits for mines");
    m.inventory["Story Can Complete Goron Mines"]=1;
    require(vanillaAreaAllowed(m,"Lanayru Field"),"mines opens Lanayru");
    require(!vanillaAreaAllowed(m,"Snowpeak Climb Lower"),"Snowpeak waits for mirror quest");
    m.inventory["Story Master Sword Drawn"]=m.inventory["Story Can Complete Arbiters Grounds"]=1;
    require(vanillaAreaAllowed(m,"Snowpeak Climb Lower"),"desert clear opens Snowpeak");
    m.inventory["Story Eldin Bridge Missing"]=1;
    require(!vanillaEdgeAllowed(m,"North Eldin Field","Eldin Field North of Bridge"),"missing bridge blocks return route");
    m.inventory["Story Eldin Bridge Restored"]=1;
    require(vanillaEdgeAllowed(m,"North Eldin Field","Eldin Field North of Bridge"),"restored bridge reconnects");
    require(m.evaluate("'Howl_at_Death_Mountain_Howling_Stone'")==Truth::no,"howl requires saved event");
    m.inventory["Story Howl at Death Mountain Howling Stone"]=1;
    require(m.evaluate("'Howl_at_Death_Mountain_Howling_Stone'")==Truth::yes,"recorded howl unlocks wolf");
    m.inventory["Story Can Warp"]=m.inventory["Story Midna Injured"]=1;
    m.inventory["Shadow Crystal"]=0;
    require(m.evaluate("Can_Use_Warp_Portals",2)==Truth::no,"injured Midna cannot warp");
    require(m.evaluate("Can_Midna_Jump",2)==Truth::no,"injured Midna cannot jump");
    m.inventory["Story Can Complete MDH"]=1;
    require(m.evaluate("Can_Midna_Jump",2)==Truth::yes,"revived Midna can guide wolf to grove");
    m.inventory["Shadow Crystal"]=1;
    require(m.evaluate("Can_Use_Warp_Portals",2)==Truth::yes,"crystal restores warp access");
    std::set<unsigned> flags{0x2120,0x3c10};
    auto event=[&](unsigned f) { return flags.contains(f); };
    auto first=[](int) { return false; }; auto sw=[](int,int) { return false; };
    require(vanillaCompletion("Sacred Grove Pedestal Master Sword",event,first,sw)==false,"Auru location is not sword");
    require(vanillaCompletion("Faron Woods Golden Wolf",event,first,sw)==false,"goron conversation is not skill");
    flags={0x2020,0x0d04,0x2904,0x6004};
    require(vanillaCompletion("Sacred Grove Pedestal Master Sword",event,first,sw)==true,"native master sword");
    require(vanillaCompletion("Sacred Grove Pedestal Shadow Crystal",event,first,sw)==true,"native crystal");
    require(vanillaCompletion("Faron Woods Golden Wolf",event,first,sw)==true,"native first skill");
    require(vanillaCompletion("Hyrule Field Amphitheater Owl Statue Sky Character",event,first,sw)==true,"native sixth character");
    std::map<std::string,int> inv;
    flags={0x2908,0x2904,0x6080,0x6004,0x2680};
    vanillaInventory(inv,event,first,[](int s){return s==22 ? 0xe9 : 0xff;},sw,2);
    require(inv["Progressive Hidden Skill"]==2 && inv["Progressive Wallet"]==2,"native skill and wallet adapters");
    require(inv["Progressive Sky Book"]==3 && inv["Aurus Memo"]==1,"sky letters and delivered memo persist");
    for(const auto& c:m.catalogue.at("checks")) {
        const std::string name=c.at("name");
        if(name=="Castle Town Goron Shop Hylian Shield") require(!m.enabled(c),"nonpersistent shop purchase hidden");
        if(name=="Coro Gate Key" || name=="Shad Dominion Rod") require(m.enabled(c),"native key and rod restoration visible");
        if(name=="Uli Cradle Delivery" || name=="Faron Woods Golden Wolf") require(m.enabled(c),"native NPC rewards visible");
    }
    m.loadSeed(R"({"seed":"randomizer","settings":{"Logic Rules":"All Locations Reachable"}})");
    require(!m.vanilla,"randomizer load removes native rules");
    m.resetSave(); require(!m.vanilla && !m.seedLoaded && m.inventory.empty(),"slot reset removes story state");
    std::cout<<"Normal Play progression, native completions, inventory and mode transitions passed\n";
}
