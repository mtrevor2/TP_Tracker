#include "map.hpp"
#include <mods/svc/hook.hpp>
#include "d/d_com_inf_game.h"
#include "d/d_menu_fmap.h"
#include "d/d_menu_dmap.h"
#include "d/d_menu_dmap_map.h"
#include "d/d_meter_map.h"
#include "d/d_map.h"
#include "d/d_menu_fmap2D.h"
#include "d/d_map_path_fmap.h"
#include "m_Do/m_Do_ext.h"
#include <JSystem/J2DGraph/J2DPicture.h>
#include <JSystem/J2DGraph/J2DPrint.h>
#include <JSystem/J2DGraph/J2DGrafContext.h>
#include <JSystem/J2DGraph/J2DOrthoGraph.h>
#include <cmath>
#include <algorithm>
#include <optional>
#include <mods/svc/resource.h>
extern const ResourceService* svc_resource;
extern "C" ModContext* mod_ctx;

DEFINE_HOOK_SYMBOL("dMenu_Fmap2DBack_c::draw", void(dMenu_Fmap2DBack_c*), TrackerMapBackDraw);

DEFINE_HOOK_SYMBOL("dMenuMapCommon_c::drawIcon", void(dMenuMapCommon_c*,f32,f32,f32,f32), TrackerDungeonIcons);
DEFINE_HOOK_SYMBOL("dMeterMap_c::draw", void(dMeterMap_c*), TrackerMiniDraw);
DEFINE_HOOK(static_cast<void (J2DPicture::*)(f32,f32,f32,f32,bool,bool,bool)>(&J2DPicture::draw), TrackerMiniPicture);
namespace tracker {
bool mapEnabled = true;
bool mapAvailable = false;
bool minimapEnabled = true, minimapAvailable = false;
bool mapTypes[checkTypeCount] = {true,true,true,true,true,true,true,true,true,true};
bool minimapTypes[checkTypeCount] = {true,true,true,true,true,true,true,true,true,true};
namespace {
Model* state = nullptr;
bool trackerMapCheck(const Json& check) {
    const auto& categories=check.at("categories");
    // The native game already draws Tears of Light / Twilit Bugs.
    return std::find(categories.begin(),categories.end(),"Twilit Insect")==categories.end()
        && state->enabled(check) && !state->skipped.contains(check.at("name").get<std::string>());
}
bool isCoroReward(const std::string& name) {
    return name=="Coro Bottle" || name=="Coro Gate Key" || name=="Coro Lantern";
}
struct Position { size_t check; std::string stage; int room; float x,y,z; bool entrance; std::string label; bool grotto=false; };
std::vector<Position> localPositions, worldPositions;
struct Temple { std::string name,stage; int room; float x,z; };
std::vector<Temple> temples;
struct ArenaEntrance { std::string stage; int room; float x,y,z; std::vector<size_t> checks; };
std::vector<ArenaEntrance> arenaEntrances;
std::pair<int,int> arenaCounts(const ArenaEntrance& arena,const bool* types) {
    int remaining=0,available=0;
    for(auto index:arena.checks) {
        const auto& check=state->catalogue.at("checks")[index];
        const std::string name=check.at("name");
        if(!trackerMapCheck(check) || !types[checkType(check)] || state->obtained.contains(name)) continue;
        ++remaining;
        const auto access=state->accessible.find(name);
        if(access!=state->accessible.end() && access->second==Truth::yes) ++available;
    }
    return {remaining,available};
}
void arenaDot(J2DGrafContext* graf,float x,float y,int available,u8 alpha,bool small) {
    auto color=available ? JUtility::TColor(255,235,30,alpha) : JUtility::TColor(165,165,165,alpha);
    graf->setup2D();
    J2DFillBox(x-2,y-3,4,6,color); J2DFillBox(x-3,y-2,6,4,color);
    if(auto* font=mDoExt_getMesgFont(); font && available>0) {
        JUtility::TColor ink(240,240,240,alpha);
        J2DPrint count(font,ink,ink);
        count.setFontSize(small ? 7 : 10,small ? 9 : 12);
        count.print(x-5,y-5,alpha,"%dx",available);
    }
    graf->setup2D();
}
std::map<std::string,std::set<std::string>> exteriorStages;
dMeterMap_c* drawingMeter=nullptr;
struct MiniRect { float x,y,w,h; bool valid=false; } miniRect;
HookAction beforeMini(ModContext*,void* args,void*,void*) { drawingMeter=mods::arg<dMeterMap_c*>(args,0);miniRect.valid=false; return HOOK_CONTINUE; }
void captureMiniRect(ModContext*,void* args,void*,void*) {
    auto* picture=mods::arg<J2DPicture*>(args,0);
    if (!drawingMeter || picture!=drawingMeter->mMapJ2DPicture) return;
    miniRect={mods::arg<float>(args,1),mods::arg<float>(args,2),mods::arg<float>(args,3),mods::arg<float>(args,4),true};
}
std::vector<unsigned char> chestTexture, doneTexture;
std::map<std::string,std::vector<unsigned char>> iconTextures;
std::set<std::string> caveIcons;
std::string iconName(const Json& check, Truth access) {
    if (caveIcons.contains(check.at("name").get<std::string>())) return "Grotto";
    const char* types[] = {"ItemChest","Gift","Bug","Poe","GoldenWolf","Gift","OwlStatue","Grotto","HiddenRupee","HiddenRupee"};
    std::string base = types[checkType(check)];
    const std::string item = check.value("original_item",std::string{});
    if (base=="ItemChest" && check.at("name").get<std::string>().find("Owl Statue")!=std::string::npos)
        return base+(access==Truth::yes ? "Available" : "Locked");
    if (base=="ItemChest") {
        if (item.find("Rupee")!=std::string::npos) base="RupeeChest";
        else if (item.find("Big Key")!=std::string::npos) base="BossKeyChest";
        else if (item=="Piece of Heart") base="HeartPiece";
        else if (item=="Heart Container") base="HeartContainer";
    }
    if (base=="Poe") return access==Truth::yes ? "PoeAvailable" : "PoeCollected";
    return base=="Grotto" ? base : base+(access==Truth::yes ? "Available" : "Locked");
}
std::map<std::pair<int,int>, std::vector<size_t>> flagIndex;
void draw(ModContext*, void* args, void*, void*) {
    auto* back = mods::arg<dMenu_Fmap2DBack_c*>(args, 0);
    // The game sets this in the constructor and clears it in the destructor.
    // _delete() is empty and cannot safely be used as a lifetime hook.
    auto* active = dMenu_Fmap_c::MyClass;
    if (!mapEnabled || !state || !active || active->mpDraw2DBack != back) return;
    const auto process = active->mProcess;
    if (process != dMenu_Fmap_c::PROC_ALL_MAP && process != dMenu_Fmap_c::PROC_REGION_MAP && process != dMenu_Fmap_c::PROC_SPOT_MAP) return;
    if (back->getRegionCursor() >= 8) return;
    auto* region = active->getNowFmapRegionData();
    if (!region || !active->getNowFmapStageData() || !dComIfGp_getStageStagInfo()) return;
    auto* graf = dComIfGp_getCurrentGrafPort();
    if (!graf) return;
    graf->setup2D();
    struct RestoreGraphics { J2DGrafContext* graf; ~RestoreGraphics() { graf->setup2D(); } } restore{graf};
    // J2DPrint leaves TEX0 enabled. Untextured quads MUST reset descriptors,
    // otherwise FIFO vertex bytes are decoded as commands (indexed XF crash).
    auto fill = [&](float x, float y, float w, float h, JUtility::TColor c) {
        graf->setup2D(); J2DFillBox(x,y,w,h,c);
    };
    // Never retain a picture allocated against a map heap beyond this draw.
    std::optional<J2DPicture> chest, completedChest;
    if (!chestTexture.empty()) chest.emplace(reinterpret_cast<ResTIMG*>(chestTexture.data()));
    if (!doneTexture.empty()) completedChest.emplace(reinterpret_cast<ResTIMG*>(doneTexture.data()));
    std::optional<J2DPicture> heart;
    bool triedHeart = false;
    const float left = back->getMapScissorAreaLX(), top = back->getMapScissorAreaLY();
    const float right = left + back->getMapScissorAreaSizeRealX();
    const float bottom = top + back->getMapScissorAreaSizeRealY();
    if (process == dMenu_Fmap_c::PROC_ALL_MAP) {
        std::map<std::string,std::set<std::string>> markerStages;
        for (const auto& point : worldPositions)
            markerStages[state->catalogue.at("checks")[point.check].at("name").get<std::string>()].insert(point.stage);
        for (int r = 0; r < 8; ++r) {
            auto* data = active->mpRegionData[r];
            if (!data) continue;
            std::set<std::string> stages;
            for (auto* stage = data->getMenuFmapStageDataTop(); stage; stage = stage->getNextData())
                stages.insert(stage->getStageName());
            int total = 0, available = 0;
            bool hasChecks=false;
            for (const auto& check : state->catalogue.at("checks")) {
                if (dungeonCheck(check) || !trackerMapCheck(check) || !mapTypes[checkType(check)]) continue;
                bool belongs = false;
                // Count atlas markers (including exterior entrances), or native
                // chest records. Unmapped hint/sign entries have no visible dot.
                const auto marker=markerStages.find(check.at("name").get<std::string>());
                if (marker!=markerStages.end()) for (const auto& stage : marker->second)
                    belongs |= stages.contains(stage);
                if (!belongs) for (const auto& flag : check.at("flags")) {
                    if (flag.value("kind",std::string{})!="chest") continue;
                    for (const auto& stage : check.at("stages")) belongs |= stages.contains(stage.get<std::string>());
                }
                if (!belongs) {
                    const std::string checkName=check.at("name");
                    auto points=exteriorStages.find(checkName);
                    if (points!=exteriorStages.end()) for (const auto& stage : points->second)
                        if (stages.contains(stage)) { belongs=true;break; }
                }
                if (!belongs) continue;
                const std::string name = check.at("name");
                hasChecks=true;
                if (state->obtained.contains(name)) continue;
                ++total;
                auto a = state->accessible.find(name);
                if (!state->obtained.contains(name) && a != state->accessible.end() && a->second == Truth::yes) ++available;
            }
            if (!hasChecks) continue;
            float x = back->mRegionMinMapX[r] + back->field_0xf0c[r] + back->mRegionMapSizeX[r]*back->mZoom*0.5f + back->mTransX;
            float y = back->mRegionMinMapY[r] + back->field_0xf2c[r] + back->mRegionMapSizeY[r]*back->mZoom*0.5f + back->mTransZ;
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            fill(x-27,y-12,66,21,JUtility::TColor(20,18,12,220));
            J2DPrint label(mDoExt_getMesgFont(),JUtility::TColor(110,245,140,255),JUtility::TColor(0,0,0,255));
            label.setFontSize(11,14); label.print(x-24,y+3,255,"%d / %d",available,total);
        }
        return;
    }
    std::string hovered;
    bool hoverDone = false;
    JUtility::TColor hoverColor(255,255,255,255);
    float closest = 24.f * 24.f;
    std::set<std::string> drawn;
    // The native iterator assumes every stage has at least one room.
    for (auto* stage = active->getNowFmapStageData(); stage; stage = stage->getNextData())
        if (!stage->getFmapRoomDataTop()) return;
    // Chest/key records (0,2,9), tears (4), and quest chest markers (5).
    // Never use isDrawDisp(), which deliberately hides DONE.
    for (u8 group : {u8(0),u8(2),u8(4),u8(5),u8(9)}) {
        dMenuFmapIconDisp_c iter;
        iter.init(region, active->getNowFmapStageData(),
                  group, active->mStayStageNo, dComIfGp_roomControl_getStayNo());
        for (int guard = 0; guard < 4096 && !iter.getValidData(); ++guard) {
            int stageNo = 0, roomNo = 0; float wx = 0, wz = 0;
            const dTres_c::data_s* record = nullptr;
            iter.getPosition(&stageNo, &roomNo, &wx, &wz, &record);
            if (record && active->isRoomCheck(stageNo, roomNo) && iter.mpStageData && iter.mpStageData->getStageArc()) {
                const int save = iter.mpStageData->getStageArc()->getSaveTableNo();
                const auto entries = flagIndex.find({save, record->mNo});
                if (entries != flagIndex.end()) for (size_t index : entries->second) {
                    const auto& check = state->catalogue.at("checks")[index];
                    const std::string name = check.at("name");
                    if (dungeonCheck(check) || !trackerMapCheck(check) || !mapTypes[checkType(check)] || drawn.contains(name)) continue;
                    const auto& cats = check.at("categories");
                    bool tear = std::find(cats.begin(), cats.end(), "Twilit Insect") != cats.end();
                    if (tear != (group == 4)) continue;
                    const auto& stages = check.at("stages");
                    if (!stages.empty() && std::find(stages.begin(), stages.end(), iter.mpStageData->getStageName()) == stages.end()) continue;
                    float x = 0, y = 0;
                    back->calcAllMapPos2D(wx-back->mStageTransX, wz-back->mStageTransZ, &x, &y);
                    x += back->mTransX; y += back->mTransZ;
                    if (!std::isfinite(x) || !std::isfinite(y) || x < left+10 || x > right-10 || y < top+10 || y > bottom-10) continue;
                    drawn.insert(name);
                    bool done = state->obtained.contains(name);
                    if (done) continue;
                    auto found = state->accessible.find(name);
                    auto access = found == state->accessible.end() ? Truth::unknown : found->second;
                    JUtility::TColor color = done ? JUtility::TColor(145,145,145,255) : access == Truth::yes ? JUtility::TColor(90,240,115,255) : access == Truth::no ? JUtility::TColor(255,165,155,255) : JUtility::TColor(235,195,85,255);
                    if (process == dMenu_Fmap_c::PROC_REGION_MAP) {
                        if (done) continue;
                        auto dot=access==Truth::yes ? JUtility::TColor(255,235,30,255) : JUtility::TColor(165,165,165,255);
                        fill(x-2,y-3,4,6,dot); fill(x-3,y-2,6,4,dot);
                    } else {
                    auto* picture = back->mPictures[tear ? ICON_LIGHT_DROP_e : ICON_TREASURE_CHEST_e];
                    if (!tear && chest) picture = done && completedChest ? &*completedChest : &*chest;
                    if (check.value("original_item", "") == "Piece of Heart") {
                        if (!triedHeart) {
                            triedHeart = true;
                            if (auto* archive = dComIfGp_getMain2DArchive())
                                if (auto* texture = static_cast<ResTIMG*>(archive->getResource("tt_heart_00.bti"))) heart.emplace(texture);
                        }
                        if (heart) picture = &*heart;
                    }
                    std::optional<J2DPicture> custom;
                    auto art = iconTextures.find(iconName(check,access));
                    if (!done && art != iconTextures.end()) {
                        custom.emplace(reinterpret_cast<ResTIMG*>(art->second.data())); picture = &*custom;
                    }
                    if (picture) {
                        auto black = picture->getBlack(), white = picture->getWhite();
                        picture->setBlackWhite(JUtility::TColor(0,0,0,0), done ? JUtility::TColor(120,120,120,255) : JUtility::TColor(255,255,255,255));
                        picture->draw(x-12, y-12, 24, 24, false, false, false);
                        picture->setBlackWhite(black, white);
                    }
                    }
                    const float dx = x-back->getArrowPos2DX(), dy = y-back->getArrowPos2DY();
                    const float distance = dx*dx+dy*dy;
                    if (distance < closest) {
                        closest = distance; hoverDone = done; hoverColor = color;
                        hovered = std::string(done ? "[DONE] " : access == Truth::yes ? "[OPEN] " : access == Truth::no ? "[LOCKED] " : "[UNKNOWN] ") + name;
                    }
                }
            }
            if (iter.nextData()) break;
        }
    }
    // Static actor positions supplement live TRES (bugs, Poes, wolves, statues,
    // interiors). Exterior anchors are labelled, not presented as indoor coordinates.
    struct InteriorGroup { float x=0,y=0; std::string label; bool grotto=false; int available=0,done=0,unknown=0; std::set<std::string> checks; };
    std::map<std::string,InteriorGroup> interiors;
    for (const auto& p : worldPositions) {
        const auto& check = state->catalogue.at("checks")[p.check];
        const std::string name = check.at("name");
        if (dungeonCheck(check) || drawn.contains(name) || !trackerMapCheck(check) || !mapTypes[checkType(check)]) continue;
        auto* stage = region->getMenuFmapStageDataTop(); int stageNo=0;
        while (stage && p.stage!=stage->getStageName()) { stage=stage->getNextData(); ++stageNo; }
        if (!stage || p.room<0 || p.room>=64 || !active->isRoomCheck(stageNo,p.room)) continue;
        bool done=state->obtained.contains(name);
        if (done && !p.entrance) continue;
        auto found=state->accessible.find(name); Truth access=found==state->accessible.end()?Truth::unknown:found->second;
        JUtility::TColor color=done ? JUtility::TColor(150,150,150,255) : access==Truth::yes ? JUtility::TColor(110,250,135,255) : access==Truth::no ? JUtility::TColor(255,180,170,255) : JUtility::TColor(240,205,100,255);
        float x=0,y=0;
        back->calcAllMapPos2D(p.x+region->getRegionOffsetX()+stage->getOffsetX()-back->mStageTransX,p.z+region->getRegionOffsetZ()+stage->getOffsetZ()-back->mStageTransZ,&x,&y);
        x+=back->mTransX; y+=back->mTransZ;
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+14||y<top+14||x>right-14||y>bottom-14) continue;
        if (p.entrance) {
            auto& group=interiors[p.stage+":"+p.label];
            group.x=x;group.y=y;group.label=p.label;group.grotto=p.grotto;
            if (group.checks.insert(name).second) {
                group.done+=done;
                group.available+=!done && access==Truth::yes;
                group.unknown+=!done && access==Truth::unknown;
            }
            drawn.insert(name);
            continue;
        }
        if (x>right-14||y>bottom-14) continue;
        if (process==dMenu_Fmap_c::PROC_REGION_MAP) {
            auto dot=access==Truth::yes ? JUtility::TColor(255,235,30,255) : JUtility::TColor(165,165,165,255);
            fill(x-2,y-3,4,6,dot);fill(x-3,y-2,6,4,dot);
        }
        else {
            auto art=iconTextures.find(done && checkType(check)==3 ? "PoeCollected" : iconName(check,access));
            if (art!=iconTextures.end()) {
                J2DPicture picture(reinterpret_cast<ResTIMG*>(art->second.data()));
                if (!done && checkType(check)==3 && access!=Truth::yes) picture.setBlackWhite(JUtility::TColor(0,0,0,0),color);
                if (done) picture.setBlackWhite(JUtility::TColor(0,0,0,0),JUtility::TColor(125,125,125,255));
                picture.draw(x-12,y-12,24,24,false,false,false);
            }
        }
        drawn.insert(name);
        float dx=x-back->getArrowPos2DX(),dy=y-back->getArrowPos2DY(),distance=dx*dx+dy*dy;
        if (distance<closest) {
            closest=distance;hoverDone=done;hoverColor=color;
            hovered=std::string(done?"[DONE] ":access==Truth::yes?"[OPEN] ":access==Truth::no?"[LOCKED] ":"[UNKNOWN] ")+name;
        }
    }
    for (const auto& [key,group]:interiors) {
        const float x=group.x,y=group.y;
        const int total=static_cast<int>(group.checks.size());
        const int remaining=total-group.done;
        const bool done=remaining==0;
        if (done) continue;
        auto color=done ? JUtility::TColor(155,155,155,255) : group.available ? JUtility::TColor(110,250,135,255) : group.unknown ? JUtility::TColor(240,205,100,255) : JUtility::TColor(255,180,170,255);
        const bool singleLocked=remaining==1 && group.available==0 && group.unknown==0;
        auto art=iconTextures.find(singleLocked ? "ItemChestLocked" : group.grotto ? "Grotto" : group.available ? "ItemChestAvailable" : "ItemChestLocked");
        if (process==dMenu_Fmap_c::PROC_REGION_MAP || remaining>1 || group.label=="Coro") {
            auto dot=group.available ? JUtility::TColor(255,235,30,255) : JUtility::TColor(165,165,165,255);
            fill(x-2,y-3,4,6,dot);fill(x-3,y-2,6,4,dot);
            if (remaining>1 && (group.label=="Coro" || (process==dMenu_Fmap_c::PROC_REGION_MAP && !group.available))) {
                JUtility::TColor ink(240,240,240,255);
                J2DPrint count(mDoExt_getMesgFont(),ink,ink);
                count.setFontSize(8,10);count.print(x-5,y-5,255,group.label=="Coro" ? "x%d" : "%dx",remaining);
            }
        } else if (art!=iconTextures.end()) {
            J2DPicture picture(reinterpret_cast<ResTIMG*>(art->second.data()));
            if (done) picture.setBlackWhite(JUtility::TColor(0,0,0,0),color);
            picture.draw(x-12,y-16,24,24,false,false,false);
        }
        // Interior counts belong in the selected marker's hover tooltip only.
        float dx=x-back->getArrowPos2DX(),dy=y-back->getArrowPos2DY(),distance=dx*dx+dy*dy;
        if (distance<closest) {
            closest=distance;hoverDone=done;hoverColor=color;
            hovered=group.label+" - "+std::to_string(group.available)+" / "+std::to_string(remaining)+" available ("+std::to_string(group.done)+" completed)";
        }
    }
    // Count real dungeon checks, independently of the number of mapped actors.
    for (const auto& temple : temples) {
        auto* stage=region->getMenuFmapStageDataTop(); int stageNo=0;
        while (stage && temple.stage!=stage->getStageName()) { stage=stage->getNextData(); ++stageNo; }
        if (!stage || !active->isRoomCheck(stageNo,temple.room)) continue;
        float x=0,y=0;
        back->calcAllMapPos2D(temple.x+region->getRegionOffsetX()+stage->getOffsetX()-back->mStageTransX,
                             temple.z+region->getRegionOffsetZ()+stage->getOffsetZ()-back->mStageTransZ,&x,&y);
        x+=back->mTransX; y+=back->mTransZ;
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+16||y<top+28||x>right-16||y>bottom-16) continue;
        const int available=state->availableDungeonChecks(temple.name,mapTypes);
        const auto art=iconTextures.find(available ? "TempleAvailable" : "Temple");
        graf->setup2D();
        if(available==0) arenaDot(graf,x,y,0,255,false);
        else if(art!=iconTextures.end()) {
            J2DPicture picture(reinterpret_cast<ResTIMG*>(art->second.data()));
            picture.draw(x-16,y-16,32,32,false,false,false);
        }
        if (available>0) {
            const std::string label=std::to_string(available)+"x";
            const float width=label.size()*7.f;
            fill(x-width/2-2,y-29,width+4,12,JUtility::TColor(20,18,12,220));
            J2DPrint count(mDoExt_getMesgFont(),JUtility::TColor(255,240,130,255),JUtility::TColor(0,0,0,255));
            count.setFontSize(10,12); count.print(x-width/2,y-19,255,"%s",label.c_str());
        }
        const float dx=x-back->getArrowPos2DX(),dy=y-back->getArrowPos2DY(),distance=dx*dx+dy*dy;
        if (distance<closest) {
            closest=distance; hoverDone=false;
            hoverColor=available ? JUtility::TColor(255,240,130,255) : JUtility::TColor(190,190,190,255);
            hovered=temple.name+" - "+std::to_string(available)+" checks available";
        }
    }
    if (!hovered.empty()) {
        // Wrap instead of letting long location names spill outside the map.
        const auto split = hovered.size() > 48 ? hovered.rfind(' ',48) : std::string::npos;
        std::string first = split == std::string::npos ? hovered : hovered.substr(0,split);
        std::string second = split == std::string::npos ? "" : hovered.substr(split+1);
        fill(left+12,bottom-66,right-left-24,58,JUtility::TColor(20,18,12,235));
        J2DPrint text(mDoExt_getMesgFont(),hoverColor,hoverColor);
        text.setFontSize(13,16);
        text.print(left+20,bottom-43,255,"%s",first.c_str());
        text.print(left+20,bottom-23,255,"%s",second.c_str());
        if (hoverDone) {
            fill(left+20,bottom-49,std::min<float>(first.size()*7,right-left-40),1,hoverColor);
            if (!second.empty()) fill(left+20,bottom-29,std::min<float>(second.size()*7,right-left-40),1,hoverColor);
        }
    }
}
// The temple pause map has its own renderer. Draw during its icon pass so
// native clipping, floor selection, zoom, panning and opening alpha apply.
void drawDungeon(ModContext*,void* args,void*,void*) {
    auto* common=mods::arg<dMenuMapCommon_c*>(args,0);
    auto* active=dMenu_Dmap_c::myclass;
    if(!mapEnabled || !state || !active || !active->mpDrawBg ||
       static_cast<dMenuMapCommon_c*>(active->mpDrawBg)!=common || !active->mMapCtrl) return;
    auto* ctrl=active->mMapCtrl;
    auto* rend=ctrl->getRendPointer(0);
    auto* graf=dComIfGp_getCurrentGrafPort();
    auto* info=dComIfGp_getStageStagInfo();
    if(!graf || !info) return;
    const float originX=mods::arg<float>(args,1),originY=mods::arg<float>(args,2);
    const float opacity=mods::arg<float>(args,3)*mods::arg<float>(args,4);
    const int stay=dComIfGp_roomControl_getStayNo();
    auto floorAlpha=[&](float height,int room) {
        const auto floor=dMapInfo_c::calcFloorNo(height,true,room);
        float blend=0;
        if(floor==ctrl->getDispFloorNo()) blend+=ctrl->getMapBlendPer();
        if(floor==ctrl->getDispFloor2No()) blend+=1.f-ctrl->getMapBlendPer();
        return static_cast<u8>(255*std::clamp(blend*opacity,0.f,1.f));
    };
    auto project=[&](const BE(Vec)& pos,float& x,float& y) {
        ctrl->cnvPosTo2Dpos(pos.x,pos.z,&x,&y);
        x+=originX; y+=originY;
        return std::isfinite(x) && std::isfinite(y);
    };
    std::set<size_t> drawn;
    auto marker=[&](size_t index,const BE(Vec)& pos,int room) {
        if(drawn.contains(index) || !rend->isDrawRoomIcon(room,stay)) return;
        const auto& check=state->catalogue.at("checks")[index];
        const std::string name=check.at("name");
        if(!state->matchesScene(check) || !trackerMapCheck(check) || !mapTypes[checkType(check)] || state->obtained.contains(name)) return;
        const auto alpha=floorAlpha(pos.y,room);
        if(!alpha) return;
        float x,y; if(!project(pos,x,y)) return;
        const auto a=state->accessible.find(name);
        const auto access=a==state->accessible.end() ? Truth::unknown : a->second;
        auto art=iconTextures.find(iconName(check,access));
        graf->setup2D();
        if(art!=iconTextures.end()) {
            J2DPicture picture(reinterpret_cast<ResTIMG*>(art->second.data()));
            picture.setAlpha(alpha); picture.draw(x-8,y-8,16,16,false,false,false);
        } else {
            auto color=access==Truth::yes ? JUtility::TColor(255,235,30,alpha) : JUtility::TColor(165,165,165,alpha);
            J2DFillBox(x-2,y-3,4,6,color); J2DFillBox(x-3,y-2,6,4,color);
        }
        drawn.insert(index);
    };
    const int save=dStage_stagInfo_GetSaveTbl(info);
    for(int group:{0,2,5,9}) {
        int guard=0;
        for(auto* record=dTres_c::getFirstData(group);record && guard++<512;record=dTres_c::getNextData(record)) {
            auto found=flagIndex.find({save,record->mNo});
            if(found==flagIndex.end()) continue;
            for(auto index:found->second) {
                const auto& stages=state->catalogue.at("checks")[index].at("stages");
                if(!stages.empty() && std::find(stages.begin(),stages.end(),state->stage)==stages.end()) continue;
                marker(index,*record->getPos(),record->mRoomNo);
            }
        }
    }
    for(const auto& p:localPositions) {
        if(p.stage!=state->stage) continue;
        BE(Vec) pos=Vec{p.x,p.y,p.z}; dMapInfo_n::correctionOriginPos(static_cast<s8>(p.room),&pos);
        marker(p.check,pos,p.room);
    }
    for(const auto& arena:arenaEntrances) {
        if(arena.stage!=state->stage || !rend->isDrawRoomIcon(arena.room,stay)) continue;
        const auto [remaining,available]=arenaCounts(arena,mapTypes);
        const auto alpha=floorAlpha(arena.y,arena.room);
        if(!remaining || !alpha) continue;
        BE(Vec) pos=Vec{arena.x,arena.y,arena.z}; dMapInfo_n::correctionOriginPos(static_cast<s8>(arena.room),&pos);
        float x,y; if(project(pos,x,y)) arenaDot(graf,x,y,available,alpha,false);
    }
    graf->setup2D();
}
void drawMini(ModContext*,void* args,void*,void*) {
    auto* meter = mods::arg<dMeterMap_c*>(args,0);
    drawingMeter=nullptr;
    if (!miniRect.valid) return;
    if (!minimapEnabled || !state || !meter || !meter->mMap || !meter->mMap->isDraw() || !meter->mMapAlpha) return;
    auto* map = meter->mMap;
    auto* info = dComIfGp_getStageStagInfo();
    auto* graf = dComIfGp_getCurrentGrafPort();
    if (!info || !graf || map->field_0x8 <= 0 || map->field_0xc <= 0) return;
    graf->setup2D();
    const int save = dStage_stagInfo_GetSaveTbl(info);
    const float left=miniRect.x, top=miniRect.y;
    const float mirror=map->previousMirror ? -1.f : 1.f;
    std::set<size_t> drawn;
    for (int group : {0,2,4,5,9}) {
        int guard = 0;
        for (auto* record = dTres_c::getFirstData(group); record && guard++ < 512; record = dTres_c::getNextData(record)) {
            auto found = flagIndex.find({save,record->mNo});
            if (found == flagIndex.end() || !map->isDrawRoomIcon(record->mRoomNo,map->getStayRoomNo())) continue;
            for (auto index : found->second) {
                const auto& check = state->catalogue.at("checks")[index];
                if (!state->matchesScene(check)) continue;
                const std::string name = check.at("name");
                if (!trackerMapCheck(check) || !minimapTypes[checkType(check)] || state->obtained.contains(name) || drawn.contains(index)) continue;
                const auto& stages = check.at("stages");
                if (!stages.empty() && std::find(stages.begin(),stages.end(),state->stage)==stages.end()) continue;
                const auto& cats = check.at("categories");
                bool tear = std::find(cats.begin(),cats.end(),"Twilit Insect")!=cats.end();
                if (tear != (group==4)) continue;
                const auto* pos = map->getIconPosition(record);
                if (!pos || !map->isRenderingFloor(dMapInfo_c::calcFloorNo(pos->y,true,record->mRoomNo))) continue;
                float x = left + (0.5f+(mirror*(pos->x-map->mPosX))/map->field_0x8)*miniRect.w;
                float y = top + (0.5f+(pos->z-map->mPosZ)/map->field_0xc)*miniRect.h;
                if (!std::isfinite(x) || !std::isfinite(y) || x<left+3 || y<top+3 || x>left+miniRect.w-3 || y>top+miniRect.h-3) continue;
                auto a = state->accessible.find(name);
                auto color = a!=state->accessible.end() && a->second==Truth::yes ? JUtility::TColor(255,235,30,meter->mMapAlpha) : JUtility::TColor(165,165,165,meter->mMapAlpha);
                graf->setup2D();
                J2DFillBox(x-1,y-2,2,4,color); J2DFillBox(x-2,y-1,4,2,color);
                drawn.insert(index);
            }
        }
    }
    for (const auto& p : localPositions) {
        if (p.stage!=state->stage || drawn.contains(p.check) || !map->isDrawRoomIcon(p.room,map->getStayRoomNo())) continue;
        const auto& check=state->catalogue.at("checks")[p.check];const std::string name=check.at("name");
        if (!state->matchesScene(check)) continue;
        if (!trackerMapCheck(check)||!minimapTypes[checkType(check)]||state->obtained.contains(name)) continue;
        // Outdoor NPC spawn heights can differ from their grounded runtime height.
        // Their reward belongs to the visible outdoor room, not a spawn-height floor.
        const auto& categories=check.at("categories");
        const bool outdoorReward=p.stage.starts_with("F_") && std::find(categories.begin(),categories.end(),"Npc")!=categories.end();
        if (!outdoorReward && !map->isRenderingFloor(dMapInfo_c::calcFloorNo(p.y,true,static_cast<s8>(p.room)))) continue;
        BE(Vec) pos=Vec{p.x,p.y,p.z};dMapInfo_n::correctionOriginPos(static_cast<s8>(p.room),&pos);
        float x=left+(0.5f+(mirror*(pos.x-map->mPosX))/map->field_0x8)*miniRect.w;
        float y=top+(0.5f+(pos.z-map->mPosZ)/map->field_0xc)*miniRect.h;
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+3||y<top+3||x>left+miniRect.w-3||y>top+miniRect.h-3) continue;
        auto a=state->accessible.find(name);Truth access=a==state->accessible.end()?Truth::unknown:a->second;
        JUtility::TColor color=access==Truth::yes?JUtility::TColor(255,235,30,meter->mMapAlpha):JUtility::TColor(165,165,165,meter->mMapAlpha);
        graf->setup2D();J2DFillBox(x-1,y-2,2,4,color);J2DFillBox(x-2,y-1,4,2,color);drawn.insert(p.check);
    }
    // Exterior anchors are already room-corrected by the atlas exporter. Do not
    // apply correctionOriginPos again or use an interior's unrelated floor.
    struct EntranceDot { float x=0,y=0; bool available=false; };
    std::map<std::string,EntranceDot> entranceDots;
    for (const auto& p : worldPositions) {
        if (!p.entrance || p.stage!=state->stage || drawn.contains(p.check) ||
            !map->isDrawRoomIcon(p.room,map->getStayRoomNo())) continue;
        const auto& check=state->catalogue.at("checks")[p.check];
        const std::string name=check.at("name");
        if (dungeonCheck(check) || !trackerMapCheck(check) || !minimapTypes[checkType(check)] || state->obtained.contains(name)) continue;
        float x=left+(0.5f+mirror*(p.x-map->mPosX)/map->field_0x8)*miniRect.w;
        float y=top+(0.5f+(p.z-map->mPosZ)/map->field_0xc)*miniRect.h;
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+3||y<top+3||x>left+miniRect.w-3||y>top+miniRect.h-3) continue;
        auto& dot=entranceDots[p.stage+":"+p.label];
        dot.x=x;dot.y=y;
        const auto access=state->accessible.find(name);
        dot.available|=access!=state->accessible.end() && access->second==Truth::yes;
        drawn.insert(p.check);
    }
    for (const auto& [key,dot] : entranceDots) {
        auto color=dot.available ? JUtility::TColor(255,235,30,meter->mMapAlpha) : JUtility::TColor(165,165,165,meter->mMapAlpha);
        graf->setup2D();
        J2DFillBox(dot.x-1,dot.y-2,2,4,color);J2DFillBox(dot.x-2,dot.y-1,4,2,color);
    }
    for(const auto& arena:arenaEntrances) {
        if(arena.stage!=state->stage || !map->isDrawRoomIcon(arena.room,map->getStayRoomNo()) ||
           !map->isRenderingFloor(dMapInfo_c::calcFloorNo(arena.y,true,arena.room))) continue;
        const auto [remaining,available]=arenaCounts(arena,minimapTypes);
        if(!remaining) continue;
        BE(Vec) pos=Vec{arena.x,arena.y,arena.z}; dMapInfo_n::correctionOriginPos(static_cast<s8>(arena.room),&pos);
        float x=left+(0.5f+mirror*(pos.x-map->mPosX)/map->field_0x8)*miniRect.w;
        float y=top+(0.5f+(pos.z-map->mPosZ)/map->field_0xc)*miniRect.h;
        if(!std::isfinite(x)||!std::isfinite(y)||x<left+6||y<top+14||x>left+miniRect.w-6||y>top+miniRect.h-3) continue;
        arenaDot(graf,x,y,available,meter->mMapAlpha,true);
    }
    graf->setup2D();
}

}
ModResult initializeMap(Model* model) {
    state = model;
    auto loadTexture = [](const char* name, std::vector<unsigned char>& bytes) {
        ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
        if (svc_resource->load(mod_ctx,name,&buffer) == MOD_OK) {
            const auto* data = static_cast<const unsigned char*>(buffer.data);
            if (buffer.size > sizeof(ResTIMG)) bytes.assign(data,data+buffer.size);
            svc_resource->free(mod_ctx,&buffer);
        }
    };
    for (const char* name : {"BossKeyChestAvailable","BossKeyChestLocked","BugAvailable","BugLocked","GiftAvailable","GiftLocked","HiddenRupeeAvailable","HiddenRupeeLocked","GoldenWolfAvailable","GoldenWolfLocked","Grotto","HeartContainerAvailable","HeartContainerLocked","HeartPieceAvailable","HeartPieceLocked","ItemChestAvailable","ItemChestLocked","OwlStatueAvailable","OwlStatueLocked","PoeAvailable","PoeCollected","RupeeChestAvailable","RupeeChestLocked"}) {
        std::string path = "icons/" + std::string(name) + ".bti";
        auto& bytes = iconTextures[name]; loadTexture(path.c_str(),bytes);
        if (bytes.empty()) iconTextures.erase(name);
    }
    for (const char* name : {"Temple","TempleAvailable"}) {
        auto& bytes=iconTextures[name];
        loadTexture((std::string("icons/")+name+".bti").c_str(),bytes);
        if (bytes.empty()) iconTextures.erase(name);
    }
    arenaEntrances.clear();
    ResourceBuffer arenaData=RESOURCE_BUFFER_INIT;
    if(svc_resource->load(mod_ctx,"arena_entrances.json",&arenaData)==MOD_OK) {
        try {
            auto data=Json::parse(static_cast<const char*>(arenaData.data),static_cast<const char*>(arenaData.data)+arenaData.size);
            for(const auto& entry:data) {
                ArenaEntrance arena{entry.at("stage").get<std::string>(),entry.at("room").get<int>(),entry.at("pos")[0],entry.at("pos")[1],entry.at("pos")[2],{}};
                for(size_t i=0;i<model->catalogue.at("checks").size();++i) {
                    const auto& name=model->catalogue.at("checks")[i].at("name");
                    if(std::find(entry.at("checks").begin(),entry.at("checks").end(),name)!=entry.at("checks").end()) arena.checks.push_back(i);
                }
                arenaEntrances.push_back(std::move(arena));
            }
        } catch(...) { arenaEntrances.clear(); }
        svc_resource->free(mod_ctx,&arenaData);
    }
    temples.clear();
    ResourceBuffer templeData=RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx,"temples.json",&templeData)==MOD_OK) {
        try {
            auto data=Json::parse(static_cast<const char*>(templeData.data),static_cast<const char*>(templeData.data)+templeData.size);
            for (const auto& [name,p] : data.items())
                temples.push_back({name,p.at("stage").get<std::string>(),p.at("room").get<int>(),p.at("pos")[0].get<float>(),p.at("pos")[2].get<float>()});
        } catch (...) { temples.clear(); }
        svc_resource->free(mod_ctx,&templeData);
    }
    loadTexture("chest.bti",chestTexture); loadTexture("chest_done.bti",doneTexture);
    flagIndex.clear();
    for (size_t i = 0; i < model->catalogue.at("checks").size(); ++i)
        for (const auto& flag : model->catalogue.at("checks")[i].at("flags"))
            if (flag.value("kind", "") == "chest") flagIndex[{flag.at("save").get<int>(),flag.at("flag").get<int>()}].push_back(i);
    localPositions.clear(); worldPositions.clear();
    ResourceBuffer positions=RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx,"positions.json",&positions)==MOD_OK) {
        try {
            auto data=Json::parse(static_cast<const char*>(positions.data),static_cast<const char*>(positions.data)+positions.size);
            model->loadExteriorChecks(data);
            for (size_t i=0;i<model->catalogue.at("checks").size();++i) {
                const std::string name=model->catalogue.at("checks")[i].at("name");
                if (!data.contains(name)) continue;
                auto parse=[&](const Json& value,bool entrance) {
                    return Position{i,value.at("stage").get<std::string>(),value.at("room").get<int>(),value.at("pos")[0].get<float>(),value.at("pos")[1].get<float>(),value.at("pos")[2].get<float>(),entrance,value.value("label",std::string{}),value.value("grotto",false)};
                };
                for (const auto& point : data.at(name)) {
                    auto p=parse(point,false);if (point.value("local",true) && !isCoroReward(name)) localPositions.push_back(p);
                    if (p.stage.starts_with("F_")) {
                        if (point.contains("world_pos")) {p.x=point.at("world_pos")[0];p.y=point.at("world_pos")[1];p.z=point.at("world_pos")[2];}
                        // Coro is outdoors, but his rewards share one aggregate marker.
                        if (isCoroReward(name)) { p.entrance=true; p.label="Coro"; }
                        worldPositions.push_back(p);
                    }
                    else if (point.contains("overworld") && point.at("overworld").is_object()) worldPositions.push_back(parse(point.at("overworld"),true));
                }
            }
        } catch (...) { localPositions.clear();worldPositions.clear(); }
        svc_resource->free(mod_ctx,&positions);
    }
    caveIcons.clear();
    for (const auto& p : worldPositions) {
        const std::string name=model->catalogue.at("checks")[p.check].at("name");
        if (p.grotto || name.find("Cave")!=std::string::npos || name.find("Grotto")!=std::string::npos)
            caveIcons.insert(name);
    }
    exteriorStages.clear();
    for (const auto& p : worldPositions) exteriorStages[model->catalogue.at("checks")[p.check].at("name").get<std::string>()].insert(p.stage);
    const auto miniBefore=mods::hook::add_pre<TrackerMiniDraw>(beforeMini);
    const auto miniPicture=mods::hook::add_post<TrackerMiniPicture>(captureMiniRect);
    const auto miniAfter=mods::hook::add_post<TrackerMiniDraw>(drawMini);
    minimapAvailable=miniBefore==MOD_OK && miniPicture==MOD_OK && miniAfter==MOD_OK;
    if (!minimapAvailable) minimapEnabled=false;
    const auto dungeonResult=mods::hook::add_post<TrackerDungeonIcons>(drawDungeon);
    if(dungeonResult!=MOD_OK) return dungeonResult;
    auto result = mods::hook::add_post<TrackerMapBackDraw>(draw);
    mapAvailable = result == MOD_OK;
    if (!mapAvailable) shutdownMap();
    return result;
}
void shutdownMap() { state = nullptr; mapAvailable = false; flagIndex.clear(); chestTexture.clear(); doneTexture.clear(); iconTextures.clear(); localPositions.clear(); worldPositions.clear(); temples.clear(); arenaEntrances.clear(); exteriorStages.clear(); drawingMeter=nullptr; minimapAvailable=false; }
}

