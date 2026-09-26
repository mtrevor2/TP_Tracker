#pragma once

namespace tracker {
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
