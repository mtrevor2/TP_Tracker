#include "preferences.hpp"
#include "json.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace tracker;
using Json=nlohmann::json;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
namespace {
Json disk=Json::object(), overrides=Json::object();
std::map<ConfigVarHandle,std::string> live;
ConfigVarHandle nextHandle=1;
int writes=0;
bool failWrite=false;
ModResult add(ModContext*,const ConfigVarDesc* d,ConfigVarHandle* h) {
    for(const auto& [_,name]:live) if(name==d->name) return MOD_CONFLICT;
    if(!disk.contains(d->name)) {
        if(d->type==CONFIG_VAR_BOOL) disk[d->name]=d->default_bool;
        else if(d->type==CONFIG_VAR_INT) disk[d->name]=d->default_int;
        else disk[d->name]=d->default_string;
    }
    *h=nextHandle++; live[*h]=d->name; return MOD_OK;
}
const Json& effective(ConfigVarHandle h) {
    const auto& name=live.at(h);
    return overrides.contains(name) ? overrides.at(name) : disk.at(name);
}
ModResult getBool(ModContext*,ConfigVarHandle h,bool* v) { *v=effective(h).get<bool>(); return MOD_OK; }
ModResult getInt(ModContext*,ConfigVarHandle h,int64_t* v) { *v=effective(h).get<int64_t>(); return MOD_OK; }
ModResult getText(ModContext*,ConfigVarHandle h,char* buffer,size_t capacity,size_t* length) {
    const auto text=effective(h).get<std::string>(); *length=text.size();
    if(buffer) { if(capacity<=text.size()) return MOD_INVALID_ARGUMENT; std::memcpy(buffer,text.c_str(),text.size()+1); }
    return MOD_OK;
}
template<class T> ModResult write(ConfigVarHandle h,const T& v) {
    if(failWrite) return MOD_ERROR;
    disk[live.at(h)]=v; ++writes; return MOD_OK;
}
ModResult setBool(ModContext*,ConfigVarHandle h,bool v) { return write(h,v); }
ModResult setInt(ModContext*,ConfigVarHandle h,int64_t v) { return write(h,v); }
ModResult setText(ModContext*,ConfigVarHandle h,const char* v) { return write(h,v); }
ConfigService config=[] {
    ConfigService s{};
    s.register_var=add; s.get_bool=getBool; s.set_bool=setBool;
    s.get_int=getInt; s.set_int=setInt; s.get_string=getText; s.set_string=setText;
    return s;
}();
struct Settings {
    bool hide=false,map=false,mini=false,mapAccess=false,miniAccess=false,hint=false;
    bool mapTypes[10]{},miniTypes[10]{};
    int statuses[2]{},sort=0;
    std::string search;
    PreferenceTargets targets() { return {hide,map,mini,mapAccess,miniAccess,hint,mapTypes,miniTypes,statuses,sort,search}; }
};
void bind(PreferenceStore& store,Settings& settings) {
    store.initialize(&config,nullptr);
    require(bindTrackerPreferences(store,settings.targets())==MOD_OK,"preference registration/restore failed");
}
}
int main() {
    try {
        Settings first; PreferenceStore store;
        bind(store,first);
        require(disk.size()==30,"some filter preferences are unregistered");
        require(writes==0,"startup overwrote saved preferences with defaults");
        require(first.map && first.mini && !first.hide && !first.mapAccess && !first.miniAccess,"wrong clean-install defaults");
        for(int i=0;i<10;++i) require(first.mapTypes[i] && first.miniTypes[i],"a category is hidden by default");
        require(first.sort==1 && first.statuses[0]==0 && first.statuses[1]==0,"wrong default dropdown values");
        require(store.set(first.hint,false)==MOD_OK,"hidden hint preference failed");
        require(first.map && first.mini && !first.mapAccess && !first.miniAccess,"hiding hint changed map availability");
        require(disk["map-markers"].get<bool>() && disk["minimap-markers"].get<bool>(),"hiding hint disabled saved markers");
        require(store.set(first.hint,true)==MOD_OK && first.map && first.mini,"showing hint changed map availability");
        require(store.set(first.hide,true)==MOD_OK && store.set(first.map,false)==MOD_OK,"toggle write failed");
        require(store.set(first.mapAccess,true)==MOD_OK && store.set(first.miniAccess,false)==MOD_OK,"accessible-only write failed");
        for(int i=0;i<10;++i) {
            require(store.set(first.mapTypes[i],i%2==0)==MOD_OK,"map category write failed");
            require(store.set(first.miniTypes[i],i%2!=0)==MOD_OK,"minimap category write failed");
        }
        require(store.set(first.statuses[0],1)==MOD_OK && store.set(first.statuses[1],5)==MOD_OK,"per-tab status write failed");
        require(store.set(first.sort,0)==MOD_OK,"sort write failed");
        require(store.set(first.search,std::string("Snowpeak Poe"))==MOD_OK,"search write failed");

        // Simulate a host restart: only serialized config survives, runtime
        // targets and all handles are recreated. No game save participates.
        require(first.hint,"map inspection hint should default on");
        require(store.set(first.hint,false)==MOD_OK,"hint toggle write failed");
        const auto saved=disk.dump();
        live.clear(); disk=Json::parse(saved); writes=0;
        Settings second; PreferenceStore restarted;
        bind(restarted,second);
        require(!second.hint,"hidden map hint reset after restart");
        require(writes==0,"restoring preferences rewrote host config");
        require(second.hide && !second.map && second.mini,"marker/hide toggles reset after restart");
        require(second.mapAccess && !second.miniAccess,"map/minimap accessibility filters lost independence");
        for(int i=0;i<10;++i) require(second.mapTypes[i]==(i%2==0) && second.miniTypes[i]==(i%2!=0),"category filters reset after restart");
        require(second.statuses[0]==1 && second.statuses[1]==5,"Current Area and All Checks filters were not restored separately");
        require(second.sort==0 && second.search=="Snowpeak Poe","sort/search reset after restart");
        require(restarted.set(second.mapAccess,false)==MOD_OK && restarted.set(second.miniAccess,true)==MOD_OK,"reverse toggle failed");
        require(!disk["map-accessible-only"].get<bool>() && disk["minimap-accessible-only"].get<bool>(),"turning filters back off/on was not saved");
        failWrite=true;
        require(restarted.set(second.mapAccess,true)==MOD_ERROR && !second.mapAccess,"failed save changed displayed preference");
        failWrite=false;
        bool unbound=false;
        require(restarted.set(unbound,true)==MOD_INVALID_ARGUMENT,"unregistered preference was silently accepted");
        overrides["map-accessible-only"]=true;
        require(restarted.set(second.mapAccess,false)==MOD_OK && second.mapAccess,"host override and runtime preference disagree");
        overrides.clear();
        require(restarted.set(second.statuses[0],INT64_MAX)==MOD_OK && second.statuses[0]==5,"large dropdown input was not bounded before conversion");
        require(restarted.set(second.search,std::string(513,'x'))==MOD_INVALID_ARGUMENT,"oversized search accepted");

        live.clear(); disk["all-checks-status"]=-99; disk["check-sort"]=INT64_MAX; disk["check-search"]=std::string(10000,'x');
        Settings invalid; PreferenceStore again;
        bind(again,invalid);
        require(invalid.statuses[1]==0 && invalid.sort==1 && invalid.search.empty(),"invalid saved selections did not fall back safely");
        std::cout << "Validated 29 persisted preferences, restart restoration, independent filters, defaults and failed writes.\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
