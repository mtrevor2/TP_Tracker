#pragma once
#include <initializer_list>
#include <map>
#include <string>

namespace tracker {
// First-item flags are set by execItemGet, including starting items. Equipment
// flags can also be set by a forced equip and do not prove a reward was received.
template<class ReadFirst>
int progressiveSwordTier(ReadFirst first) {
    int result=0, tier=0;
    for(int id : {0x3f,0x28,0x29,0x49}) { ++tier; if(first(id)) result=tier; }
    return result;
}
// A full Goron big key grants only the third shard flag; it still counts as 3.
template<class ReadFirst>
int goronKeyShards(ReadFirst first, bool bigKey) {
    if(bigKey || first(0xfb)) return 3;
    return first(0xfa) ? 2 : first(0xf9) ? 1 : 0;
}
template<class ReadFirst, class ReadSlot>
void stableQuestInventory(std::map<std::string,int>& inventory, ReadFirst first, ReadSlot slot) {
    inventory["Ordon Pumpkin"]=first(0xf4);
    inventory["Ordon Cheese"]=first(0xf5);
    inventory["Ball and Chain"]=slot(6)==0x42;
    inventory["Hylian Shield"]=first(0x2c);
    inventory["Ordon Shield"]=first(0x2a);
    inventory["Hawkeye"]=first(0x3e);
}

// Randomizer item.cpp stores the memo in slot 7 (vanilla queries slot 19).
// tools.cpp also recognizes it after Fyer consumes it via event 0x2680.
template<class ReadSlot, class ReadEvent>
bool aurusMemoOwned(ReadSlot slot, ReadEvent event) {
    return slot(7) == 0x90 || event(0x2680);
}

// D_MN05/STG_00 yodoor between rooms 1/2: angle.z low byte is 0x0B.
// dComIfGs_isStageSwitch reads live flags in Forest Temple and saved flags outside.
template<class ReadStageSwitch>
bool forestSecondMonkeyDoorUnlocked(ReadStageSwitch stageSwitch) {
    return stageSwitch(0x10, 0x0b);
}
}
