#pragma once
#include <mods/svc/config.h>
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace tracker {
// Runtime values stay in their existing owners; the host owns their persisted
// config keys. Nothing here uses a seed/save blob or writes defaults on startup.
class PreferenceStore {
    struct Binding { ConfigVarHandle handle; int64_t min=0,max=0; size_t textLimit=0; };
    const ConfigService* config=nullptr;
    ModContext* context=nullptr;
    std::map<const void*,Binding> bindings;

    ModResult add(const char* name, ConfigVarDesc& desc, const void* target, Binding& binding) {
        desc.name=name;
        auto result=config->register_var(context,&desc,&binding.handle);
        if(result==MOD_OK) bindings[target]=binding;
        return result;
    }
    ModResult readText(const Binding& binding,std::string& value) const {
        size_t length=0;
        auto result=config->get_string(context,binding.handle,nullptr,0,&length);
        if(result!=MOD_OK) return result;
        // Ignore an oversized saved search rather than allocating arbitrary data.
        if(length>binding.textLimit) { value.clear(); return MOD_OK; }
        std::vector<char> buffer(length+1);
        result=config->get_string(context,binding.handle,buffer.data(),buffer.size(),&length);
        if(result==MOD_OK) value.assign(buffer.data(),length);
        return result;
    }
public:
    void initialize(const ConfigService* service,ModContext* ctx) {
        config=service; context=ctx; bindings.clear();
    }
    ModResult bind(const char* name,bool& value,bool defaultValue) {
        ConfigVarDesc desc=CONFIG_VAR_DESC_INIT; desc.type=CONFIG_VAR_BOOL; desc.default_bool=defaultValue;
        Binding binding{};
        auto result=add(name,desc,&value,binding);
        bool restored=defaultValue;
        if(result==MOD_OK) result=config->get_bool(context,binding.handle,&restored);
        if(result==MOD_OK) value=restored;
        return result;
    }
    ModResult bind(const char* name,int& value,int defaultValue,int minimum,int maximum) {
        ConfigVarDesc desc=CONFIG_VAR_DESC_INIT; desc.type=CONFIG_VAR_INT; desc.default_int=defaultValue;
        Binding binding{0,minimum,maximum};
        auto result=add(name,desc,&value,binding);
        int64_t restored=defaultValue;
        if(result==MOD_OK) result=config->get_int(context,binding.handle,&restored);
        if(result==MOD_OK) value=restored>=minimum && restored<=maximum ? static_cast<int>(restored) : defaultValue;
        return result;
    }
    ModResult bind(const char* name,std::string& value,size_t maximumBytes) {
        ConfigVarDesc desc=CONFIG_VAR_DESC_INIT; desc.type=CONFIG_VAR_STRING; desc.default_string="";
        Binding binding{0,0,0,maximumBytes};
        auto result=add(name,desc,&value,binding);
        if(result==MOD_OK) result=readText(binding,value);
        return result;
    }
    ModResult set(bool& target,bool value) {
        auto found=bindings.find(&target);
        if(found==bindings.end()) return MOD_INVALID_ARGUMENT;
        auto result=config->set_bool(context,found->second.handle,value);
        bool effective=target;
        if(result==MOD_OK) result=config->get_bool(context,found->second.handle,&effective);
        if(result==MOD_OK) target=effective;
        return result;
    }
    ModResult set(int& target,int64_t value) {
        auto found=bindings.find(&target);
        if(found==bindings.end()) return MOD_INVALID_ARGUMENT;
        const auto& binding=found->second;
        auto result=config->set_int(context,binding.handle,std::clamp(value,binding.min,binding.max));
        int64_t effective=target;
        if(result==MOD_OK) result=config->get_int(context,binding.handle,&effective);
        if(result==MOD_OK) target=static_cast<int>(std::clamp(effective,binding.min,binding.max));
        return result;
    }
    ModResult set(std::string& target,const std::string& value) {
        auto found=bindings.find(&target);
        if(found==bindings.end() || value.size()>found->second.textLimit) return MOD_INVALID_ARGUMENT;
        auto result=config->set_string(context,found->second.handle,value.c_str());
        if(result==MOD_OK) result=readText(found->second,target);
        return result;
    }
};

inline constexpr std::array<const char*,10> categoryPreferenceKeys{
    "chests","npc-events","golden-bugs","poes","golden-wolves",
    "shops","owl-statues","grottos","freestanding-rupees","hidden-rupees"};
struct PreferenceTargets {
    bool& hideCompleted;
    bool& mapEnabled;
    bool& minimapEnabled;
    bool& mapAccessibleOnly;
    bool& minimapAccessibleOnly;
    bool (&mapTypes)[10];
    bool (&minimapTypes)[10];
    int (&statusFilters)[2];
    int& sort;
    std::string& search;
};
inline ModResult bindTrackerPreferences(PreferenceStore& store,PreferenceTargets t) {
    for(auto entry : {std::pair{"hide-completed",&t.hideCompleted},
                     std::pair{"map-accessible-only",&t.mapAccessibleOnly},
                     std::pair{"minimap-accessible-only",&t.minimapAccessibleOnly}}) {
        auto result=store.bind(entry.first,*entry.second,false);
        if(result!=MOD_OK) return result;
    }
    for(auto entry : {std::pair{"map-markers",&t.mapEnabled},std::pair{"minimap-markers",&t.minimapEnabled}}) {
        auto result=store.bind(entry.first,*entry.second,true);
        if(result!=MOD_OK) return result;
    }
    for(int surface=0;surface<2;++surface) for(size_t i=0;i<categoryPreferenceKeys.size();++i) {
        const auto key=std::string(surface ? "minimap-" : "map-")+categoryPreferenceKeys[i];
        auto result=store.bind(key.c_str(),surface ? t.minimapTypes[i] : t.mapTypes[i],true);
        if(result!=MOD_OK) return result;
    }
    auto result=store.bind("current-area-status",t.statusFilters[0],0,0,5);
    if(result!=MOD_OK) return result;
    result=store.bind("all-checks-status",t.statusFilters[1],0,0,5);
    if(result!=MOD_OK) return result;
    result=store.bind("check-sort",t.sort,1,0,1);
    if(result!=MOD_OK) return result;
    return store.bind("check-search",t.search,512); // 128 UTF-8 characters at most.
}
}
