#include "map.hpp"
#include "map_positions.hpp"
#include "map_projection.hpp"
#include "map_interaction.hpp"
#include "controller.hpp"
#include <mods/svc/ui.h>
#include <aurora/aurora.h>
#include <chrono>
#include <mods/svc/hook.hpp>
#include "d/d_com_inf_game.h"
#include "d/d_menu_fmap.h"
#include "d/d_menu_dmap.h"
#include "d/d_menu_dmap_map.h"
#include "d/d_meter_map.h"
#include "d/d_meter2_info.h"
#include "d/d_map.h"
#include "d/d_menu_fmap2D.h"
#include "d/d_map_path_fmap.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include <JSystem/J2DGraph/J2DPicture.h>
#include <JSystem/J2DGraph/J2DPrint.h>
#include <JSystem/J2DGraph/J2DGrafContext.h>
#include <JSystem/J2DGraph/J2DOrthoGraph.h>
#include <cmath>
#include <algorithm>
#include <optional>
#include <mods/svc/resource.h>
extern const ResourceService* svc_resource;
extern const UiService* svc_ui;
extern "C" ModContext* mod_ctx;

DEFINE_HOOK_SYMBOL("src/dusk/mouse.cpp#dusk::mouse::should_capture_mouse", bool(SDL_Window*), TrackerMapMouseCapture);
DEFINE_HOOK_SYMBOL("src/dusk/mouse.cpp#dusk::mouse::should_show_cursor", bool(bool), TrackerMapMouseCursor);
DEFINE_HOOK_SYMBOL("dMenu_Fmap2DBack_c::draw", void(dMenu_Fmap2DBack_c*), TrackerMapBackDraw);

DEFINE_HOOK_SYMBOL("dMenuMapCommon_c::drawIcon", void(dMenuMapCommon_c*,f32,f32,f32,f32), TrackerDungeonIcons);
DEFINE_HOOK_SYMBOL("dMeterMap_c::draw", void(dMeterMap_c*), TrackerMiniDraw);
DEFINE_HOOK(static_cast<void (J2DPicture::*)(f32,f32,f32,f32,bool,bool,bool)>(&J2DPicture::draw), TrackerMapPicture);
namespace tracker {
bool mapEnabled = true;
bool showCheckInfoHint = true;
bool mapAccessibleOnly = false, minimapAccessibleOnly = false;
bool mapAvailable = false;
bool minimapEnabled = true, minimapAvailable = false;
bool mapTypes[checkTypeCount] = {true,true,true,true,true,true,true,true,true,true};
bool minimapTypes[checkTypeCount] = {true,true,true,true,true,true,true,true,true,true};
namespace {
Model* state = nullptr;
MapCheckHandler checkHandler=nullptr;
bool (*closeCheckHandler)()=nullptr;
MapHitTargets mapHits;
const void* hitOwner=nullptr;
bool hitDungeon=false;
MapScreenPoint mapCursor{};
std::chrono::steady_clock::time_point hitTime{};
using WindowSizeFn=AuroraWindowSize (*)();
WindowSizeFn windowSize=nullptr;
bool inspectionContext() {
    if(!mapEnabled || !state || !hitOwner || std::chrono::steady_clock::now()-hitTime>std::chrono::milliseconds(250)) return false;
    if(hitDungeon) {
        auto* map=dMenu_Dmap_c::myclass;
        return map && map==hitOwner && map->m_process==1 && map->mMapCtrl && map->mMapCtrl->isEndZoomIn();
    }
    auto* map=dMenu_Fmap_c::MyClass;
    return map && map==hitOwner && map->mProcess==dMenu_Fmap_c::PROC_SPOT_MAP;
}
HookAction mapMouseCapture(ModContext*,void*,void* result,void*) {
    if(!inspectionContext()) return HOOK_CONTINUE;
    *static_cast<bool*>(result)=false;
    return HOOK_SKIP_ORIGINAL;
}
HookAction mapMouseCursor(ModContext*,void*,void* result,void*) {
    float x=0,y=0;
    if(!inspectionContext() || !readMousePosition(x,y)) return HOOK_CONTINUE;
    *static_cast<bool*>(result)=true;
    return HOOK_SKIP_ORIGINAL;
}
std::optional<MapScreenPoint> mousePoint(float x,float y) {
    if(!windowSize) return {};
    const auto size=windowSize();
    return mapMousePoint(x,y,float(size.width),float(size.height),size.native_fb_width,size.native_fb_height,size.fb_width,size.fb_height,
        {mDoGph_gInf_c::getMinXF(),mDoGph_gInf_c::getMinYF(),mDoGph_gInf_c::getMinXF()+mDoGph_gInf_c::getWidthF(),mDoGph_gInf_c::getMinYF()+mDoGph_gInf_c::getHeightF()});
}
std::optional<MapScreenPoint> hoverPoint() {
    float x=0,y=0;
    if(readMousePosition(x,y)) return mousePoint(x,y);
    return mapCursor;
}
bool inspectFromMap(MapAction action,float x,float y) {
    if(action==MapAction::Dismiss) return closeCheckHandler && closeCheckHandler();
    if(!inspectionContext() || !checkHandler) return false;
    bool visible=true;
    if(svc_ui->is_any_document_visible(mod_ctx,&visible)!=MOD_OK || visible) return false;
    auto point=action==MapAction::MouseClick ? mousePoint(x,y) : std::optional<MapScreenPoint>(mapCursor);
    const auto name=point ? mapHits.hit(*point) : std::string{};
    if(name.empty() || state->obtained.contains(name) || state->skipped.contains(name)) return false;
    return checkHandler(name);
}
void inspectionHint(J2DGrafContext* graf,float left,float top) {
    // Hide only the optional overlay. Keep every rendered check selectable.
    mapHits.setHint(std::nullopt);
    if(!showCheckInfoHint) return;
    bool visible=true;
    if(!inspectionContext() || svc_ui->is_any_document_visible(mod_ctx,&visible)!=MOD_OK || visible) return;
    auto* font=mDoExt_getMesgFont(); if(!font) return;
    const auto point=hoverPoint();
    const bool selected=point && !mapHits.hit(*point).empty();
    auto fill=[&](float x,float y,float w,float h,JUtility::TColor color) { graf->setup2D(); J2DFillBox(x,y,w,h,color); };
    float mouseX=0,mouseY=0;
    const bool mouse=readMousePosition(mouseX,mouseY);
    const float width=mouse ? 138.f : 181.f;
    fill(left,top,width,22,JUtility::TColor(20,18,12,220));
    fill(left+5,top+4,mouse ? 56.f : 32.f,14,JUtility::TColor(75,72,53,240));
    if(!mouse) fill(left+49,top+4,49,14,JUtility::TColor(75,72,53,240));
    const JUtility::TColor ink=selected ? JUtility::TColor(255,234,132,255) : JUtility::TColor(220,215,195,255);
    J2DPrint text(font,ink,ink); text.setFontSize(8,10);
    text.print(left+8,top+14,255,mouse ? "Left click" : "RT/R2");
    if(!mouse) { text.print(left+40,top+14,255,"+"); text.print(left+52,top+14,255,"A/Cross"); }
    text.print(left+(mouse ? 70 : 107),top+14,255,"Check Info");
    // The hint itself is not a check and must not click through to an icon.
    mapHits.setHint(MapHitRect{left,top,left+width,top+22});
    graf->setup2D();
}

bool trackerMapCheck(const Json& check, bool accessibleOnly) {
    const auto& categories=check.at("categories");
    // The native game already draws Tears of Light / Twilit Bugs.
    return std::find(categories.begin(),categories.end(),"Twilit Insect")==categories.end()
        && passesAccessibilityFilter(*state,check.at("name").get<std::string>(),accessibleOnly)
        && state->enabled(check) && !state->skipped.contains(check.at("name").get<std::string>())
        && markerMilestoneVisible(*state,check.at("name").get<std::string>());
}
bool isRupeeCheck(const Json& check) {
    const int type=checkType(check);
    return type==8 || type==9;
}
JUtility::TColor checkDotColor(bool available,bool rupee,u8 alpha) {
    if(!available) return JUtility::TColor(165,165,165,alpha);
    return rupee ? JUtility::TColor(65,150,255,alpha) : JUtility::TColor(255,235,30,alpha);
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
std::pair<int,int> arenaCounts(const ArenaEntrance& arena,const bool* types,bool accessibleOnly) {
    int remaining=0,available=0;
    for(auto index:arena.checks) {
        const auto& check=state->catalogue.at("checks")[index];
        const std::string name=check.at("name");
        if(!trackerMapCheck(check,accessibleOnly) || !types[checkType(check)] || state->obtained.contains(name)) continue;
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
dMenu_Fmap2DBack_c* drawingFieldMap=nullptr;
bool fieldMapMirrored=false;
HookAction beforeFieldMap(ModContext*,void* args,void*,void*) {
    drawingFieldMap=mods::arg<dMenu_Fmap2DBack_c*>(args,0);
    // Native map state is a fallback when debug options hide region artwork.
    auto* meter=dMeter2Info_getMeterMapClass();
    fieldMapMirrored=meter && meter->mMap && meter->mMap->previousMirror;
    return HOOK_CONTINUE;
}
struct MiniRect { float x,y,w,h; bool valid=false; } miniRect;
HookAction beforeMini(ModContext*,void* args,void*,void*) { drawingMeter=mods::arg<dMeterMap_c*>(args,0);miniRect.valid=false; return HOOK_CONTINUE; }
void captureMapPicture(ModContext*,void* args,void*,void*) {
    auto* picture=mods::arg<J2DPicture*>(args,0);
    // regionTextureDraw passes the live Mirror Mode flag to its background
    // pictures. Reuse it each frame instead of reading private host settings
    // or retaining an orientation from an earlier map/session.
    if (drawingFieldMap && picture) {
        for (auto* area : drawingFieldMap->mpAreaTex)
            if (picture==area) { fieldMapMirrored=mods::arg<bool>(args,5); break; }
    }
    if (!drawingMeter || picture!=drawingMeter->mMapJ2DPicture) return;
    miniRect={mods::arg<float>(args,1),mods::arg<float>(args,2),mods::arg<float>(args,3),mods::arg<float>(args,4),true};
}
std::vector<unsigned char> chestTexture, doneTexture;
std::map<std::string,std::vector<unsigned char>> iconTextures;
std::set<std::string> caveIcons;
std::string iconName(const Json& check, Truth access) {
    if (isRupeeCheck(check)) return access==Truth::yes ? "HiddenRupeeAvailable" : "HiddenRupeeLocked";
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
    drawingFieldMap=nullptr;
    hitOwner=nullptr; mapHits.clear();
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
    const FieldMapTransform projection{back->mTransX,back->mTransZ,back->field_0x11dc,fieldMapMirrored};
    mapCursor=projection.screen(back->getArrowPos2DX(),back->getArrowPos2DY());
    const auto hoveredPoint=hoverPoint();
    const auto cursor=hoveredPoint.value_or(MapScreenPoint{-1e6f,-1e6f});
    const float left = back->mTransX+back->getMapScissorAreaLX(), top = back->mTransZ+back->getMapScissorAreaLY();
    const float right = left + back->getMapScissorAreaSizeRealX();
    const float bottom = top + back->getMapScissorAreaSizeRealY();
    if(process==dMenu_Fmap_c::PROC_SPOT_MAP) {
        hitOwner=active; hitDungeon=false; hitTime=std::chrono::steady_clock::now();
        mapHits.clear({left,top,right,bottom});
    }
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
                if (dungeonCheck(check) || !trackerMapCheck(check,mapAccessibleOnly) || !mapTypes[checkType(check)]) continue;
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
            const auto [x,y]=projection.screen(
                back->mRegionMinMapX[r]+back->field_0xf0c[r]+back->mRegionMapSizeX[r]*back->mZoom*0.5f,
                back->mRegionMinMapY[r]+back->field_0xf2c[r]+back->mRegionMapSizeY[r]*back->mZoom*0.5f);
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
                    if (dungeonCheck(check) || !trackerMapCheck(check,mapAccessibleOnly) || !mapTypes[checkType(check)] || drawn.contains(name)) continue;
                    const auto& cats = check.at("categories");
                    bool tear = std::find(cats.begin(), cats.end(), "Twilit Insect") != cats.end();
                    if (tear != (group == 4)) continue;
                    const auto& stages = check.at("stages");
                    if (!stages.empty() && std::find(stages.begin(), stages.end(), iter.mpStageData->getStageName()) == stages.end()) continue;
                    const auto [x,y]=projection.world(*back,wx,wz);
                    if (!std::isfinite(x) || !std::isfinite(y) || x < left+10 || x > right-10 || y < top+10 || y > bottom-10) continue;
                    drawn.insert(name);
                    bool done = state->obtained.contains(name);
                    if (done) continue;
                    auto found = state->accessible.find(name);
                    auto access = found == state->accessible.end() ? Truth::unknown : found->second;
                    JUtility::TColor color = done ? JUtility::TColor(145,145,145,255) : access == Truth::yes ? JUtility::TColor(90,240,115,255) : access == Truth::no ? JUtility::TColor(255,165,155,255) : JUtility::TColor(235,195,85,255);
                    if (process == dMenu_Fmap_c::PROC_REGION_MAP) {
                        if (done) continue;
                        auto dot=checkDotColor(access==Truth::yes,isRupeeCheck(check),255);
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
                    if(process==dMenu_Fmap_c::PROC_SPOT_MAP) mapHits.add(x,y,12,12,name);
                    const float dx = x-cursor.x, dy = y-cursor.y;
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
    struct InteriorGroup { float x=0,y=0; std::string label; bool grotto=false,rupeesOnly=true; int available=0,done=0,unknown=0; std::set<std::string> checks; };
    std::map<std::string,InteriorGroup> interiors;
    for (const auto& p : worldPositions) {
        const auto& check = state->catalogue.at("checks")[p.check];
        const std::string name = check.at("name");
        if (dungeonCheck(check) || drawn.contains(name) || !trackerMapCheck(check,mapAccessibleOnly) || !mapTypes[checkType(check)]) continue;
        auto* stage = region->getMenuFmapStageDataTop(); int stageNo=0;
        while (stage && p.stage!=stage->getStageName()) { stage=stage->getNextData(); ++stageNo; }
        if (!stage || p.room<0 || p.room>=64 || !active->isRoomCheck(stageNo,p.room)) continue;
        bool done=state->obtained.contains(name);
        if (done && !p.entrance) continue;
        auto found=state->accessible.find(name); Truth access=found==state->accessible.end()?Truth::unknown:found->second;
        JUtility::TColor color=done ? JUtility::TColor(150,150,150,255) : access==Truth::yes ? JUtility::TColor(110,250,135,255) : access==Truth::no ? JUtility::TColor(255,180,170,255) : JUtility::TColor(240,205,100,255);
        const auto [x,y]=projection.world(*back,
            p.x+region->getRegionOffsetX()+stage->getOffsetX(),
            p.z+region->getRegionOffsetZ()+stage->getOffsetZ());
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+14||y<top+14||x>right-14||y>bottom-14) continue;
        if (p.entrance) {
            auto& group=interiors[p.stage+":"+p.label];
            group.x=x;group.y=y;group.label=p.label;group.grotto=p.grotto;
            if (group.checks.insert(name).second) {
                group.done+=done;
                if(!done) group.rupeesOnly&=isRupeeCheck(check);
                group.available+=!done && access==Truth::yes;
                group.unknown+=!done && access==Truth::unknown;
            }
            drawn.insert(name);
            continue;
        }
        if (x>right-14||y>bottom-14) continue;
        if (process==dMenu_Fmap_c::PROC_REGION_MAP) {
            auto dot=checkDotColor(access==Truth::yes,isRupeeCheck(check),255);
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
        if(process==dMenu_Fmap_c::PROC_SPOT_MAP) mapHits.add(x,y,12,12,name);
        float dx=x-cursor.x,dy=y-cursor.y,distance=dx*dx+dy*dy;
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
        auto art=iconTextures.find(group.rupeesOnly ? (group.available ? "HiddenRupeeAvailable" : "HiddenRupeeLocked") : singleLocked ? "ItemChestLocked" : group.grotto ? "Grotto" : group.available ? "ItemChestAvailable" : "ItemChestLocked");
        if (process==dMenu_Fmap_c::PROC_REGION_MAP || remaining>1 || group.label=="Coro") {
            auto dot=checkDotColor(group.available>0,group.rupeesOnly,255);
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
        if(process==dMenu_Fmap_c::PROC_SPOT_MAP) {
            std::string name;
            if(remaining==1) for(const auto& candidate:group.checks) if(!state->obtained.contains(candidate)) name=candidate;
            // Counts represent several checks, so never open an arbitrary one.
            if(remaining>1 || group.label=="Coro") mapHits.add(x,y,3,3,name);
            else mapHits.add(x,y-4,12,12,name);
        }
        // Interior counts belong in the selected marker's hover tooltip only.
        float dx=x-cursor.x,dy=y-cursor.y,distance=dx*dx+dy*dy;
        if (distance<closest) {
            closest=distance;hoverDone=done;hoverColor=color;
            if (remaining==1) {
                const auto last=std::find_if(group.checks.begin(),group.checks.end(),[](const auto& name) { return !state->obtained.contains(name); });
                hovered=std::string(group.available ? "[OPEN] " : group.unknown ? "[UNKNOWN] " : "[LOCKED] ")+*last;
            } else {
                hovered=group.label+" - "+std::to_string(group.available)+" / "+std::to_string(remaining)+" available ("+std::to_string(group.done)+" completed)";
            }
        }
    }
    // Count real dungeon checks, independently of the number of mapped actors.
    for (const auto& temple : temples) {
        auto* stage=region->getMenuFmapStageDataTop(); int stageNo=0;
        while (stage && temple.stage!=stage->getStageName()) { stage=stage->getNextData(); ++stageNo; }
        if (!stage || !active->isRoomCheck(stageNo,temple.room)) continue;
        const auto [x,y]=projection.world(*back,
            temple.x+region->getRegionOffsetX()+stage->getOffsetX(),
            temple.z+region->getRegionOffsetZ()+stage->getOffsetZ());
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+16||y<top+28||x>right-16||y>bottom-16) continue;
        const int available=state->availableDungeonChecks(temple.name,mapTypes);
        if(mapAccessibleOnly && !available) continue;
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
        if(process==dMenu_Fmap_c::PROC_SPOT_MAP) mapHits.add(x,y,16,16,"");
        const float dx=x-cursor.x,dy=y-cursor.y,distance=dx*dx+dy*dy;
        if (distance<closest) {
            closest=distance; hoverDone=false;
            hoverColor=available ? JUtility::TColor(255,240,130,255) : JUtility::TColor(190,190,190,255);
            hovered=temple.name+" - "+std::to_string(available)+" checks available";
        }
    }
    // Leave room for the native location-name banner above the map.
    inspectionHint(graf,left+12,top+48);
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
    hitOwner=nullptr; mapHits.clear();
    auto* ctrl=active->mMapCtrl;
    auto* rend=ctrl->getRendPointer(0);
    auto* graf=dComIfGp_getCurrentGrafPort();
    auto* info=dComIfGp_getStageStagInfo();
    if(!graf || !info) return;
    const float originX=mods::arg<float>(args,1),originY=mods::arg<float>(args,2);
    const float opacity=mods::arg<float>(args,3)*mods::arg<float>(args,4);
    const int stay=dComIfGp_roomControl_getStayNo();
    const float width=ctrl->field_0x94,height=ctrl->field_0x98;
    if(ctrl->isEndZoomIn() && active->m_process==1 && opacity>=0.99f) {
        hitOwner=active; hitDungeon=true; hitTime=std::chrono::steady_clock::now();
        mapHits.clear({originX,originY,originX+width,originY+height});
        mapCursor={originX+width/2,originY+height/2};
    }
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
        if(!state->matchesScene(check) || !trackerMapCheck(check,mapAccessibleOnly) || !mapTypes[checkType(check)] || state->obtained.contains(name)) return;
        const auto alpha=floorAlpha(pos.y,room);
        if(!alpha) return;
        float x,y; if(!project(pos,x,y)) return;
        const auto a=state->accessible.find(name);
        const auto access=a==state->accessible.end() ? Truth::unknown : a->second;
        auto art=iconTextures.find(iconName(check,access));
        graf->setup2D();
        if((!isRupeeCheck(check) || ctrl->isEndZoomIn()) && art!=iconTextures.end()) {
            J2DPicture picture(reinterpret_cast<ResTIMG*>(art->second.data()));
            picture.setAlpha(alpha); picture.draw(x-8,y-8,16,16,false,false,false);
        } else {
            auto color=checkDotColor(access==Truth::yes,isRupeeCheck(check),alpha);
            J2DFillBox(x-2,y-3,4,6,color); J2DFillBox(x-3,y-2,6,4,color);
        }
        if(hitOwner && alpha>=250) mapHits.add(x,y,8,8,name);
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
        const auto [remaining,available]=arenaCounts(arena,mapTypes,mapAccessibleOnly);
        const auto alpha=floorAlpha(arena.y,arena.room);
        if(!remaining || !alpha) continue;
        BE(Vec) pos=Vec{arena.x,arena.y,arena.z}; dMapInfo_n::correctionOriginPos(static_cast<s8>(arena.room),&pos);
        float x,y; if(project(pos,x,y)) {
            arenaDot(graf,x,y,available,alpha,false);
            if(hitOwner && alpha>=250) {
                std::string name;
                if(remaining==1) for(auto index:arena.checks) {
                    const auto& check=state->catalogue.at("checks")[index];
                    const std::string candidate=check.at("name");
                    if(trackerMapCheck(check,mapAccessibleOnly) && mapTypes[checkType(check)] && !state->obtained.contains(candidate)) name=candidate;
                }
                mapHits.add(x,y,5,5,name);
            }
        }
    }
    if(hitOwner) {
        float mx=0,my=0;
        bool visible=true;
        svc_ui->is_any_document_visible(mod_ctx,&visible);
        // Temple maps have no free cyan cursor: pan a check under this reticle.
        if(!visible && !readMousePosition(mx,my)) {
            graf->setup2D();
            const JUtility::TColor cyan(70,235,255,255);
            J2DFillBox(mapCursor.x-12,mapCursor.y-1,7,2,cyan); J2DFillBox(mapCursor.x+5,mapCursor.y-1,7,2,cyan);
            J2DFillBox(mapCursor.x-1,mapCursor.y-12,2,7,cyan); J2DFillBox(mapCursor.x-1,mapCursor.y+5,2,7,cyan);
        }
        inspectionHint(graf,originX+8,originY+8);
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
                if (!trackerMapCheck(check,minimapAccessibleOnly) || !minimapTypes[checkType(check)] || state->obtained.contains(name) || drawn.contains(index)) continue;
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
                auto color = checkDotColor(a!=state->accessible.end() && a->second==Truth::yes,isRupeeCheck(check),meter->mMapAlpha);
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
        if (!trackerMapCheck(check,minimapAccessibleOnly)||!minimapTypes[checkType(check)]||state->obtained.contains(name)) continue;
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
        JUtility::TColor color=checkDotColor(access==Truth::yes,isRupeeCheck(check),meter->mMapAlpha);
        graf->setup2D();J2DFillBox(x-1,y-2,2,4,color);J2DFillBox(x-2,y-1,4,2,color);drawn.insert(p.check);
    }
    // Exterior anchors are already room-corrected by the atlas exporter. Do not
    // apply correctionOriginPos again or use an interior's unrelated floor.
    struct EntranceDot { float x=0,y=0; bool available=false,rupeesOnly=true; };
    std::map<std::string,EntranceDot> entranceDots;
    for (const auto& p : worldPositions) {
        if (!p.entrance || p.stage!=state->stage || drawn.contains(p.check) ||
            !map->isDrawRoomIcon(p.room,map->getStayRoomNo())) continue;
        const auto& check=state->catalogue.at("checks")[p.check];
        const std::string name=check.at("name");
        if (dungeonCheck(check) || !trackerMapCheck(check,minimapAccessibleOnly) || !minimapTypes[checkType(check)] || state->obtained.contains(name)) continue;
        float x=left+(0.5f+mirror*(p.x-map->mPosX)/map->field_0x8)*miniRect.w;
        float y=top+(0.5f+(p.z-map->mPosZ)/map->field_0xc)*miniRect.h;
        if (!std::isfinite(x)||!std::isfinite(y)||x<left+3||y<top+3||x>left+miniRect.w-3||y>top+miniRect.h-3) continue;
        auto& dot=entranceDots[p.stage+":"+p.label];
        dot.x=x;dot.y=y;dot.rupeesOnly&=isRupeeCheck(check);
        const auto access=state->accessible.find(name);
        dot.available|=access!=state->accessible.end() && access->second==Truth::yes;
        drawn.insert(p.check);
    }
    for (const auto& [key,dot] : entranceDots) {
        auto color=checkDotColor(dot.available,dot.rupeesOnly,meter->mMapAlpha);
        graf->setup2D();
        J2DFillBox(dot.x-1,dot.y-2,2,4,color);J2DFillBox(dot.x-2,dot.y-1,4,2,color);
    }
    for(const auto& arena:arenaEntrances) {
        if(arena.stage!=state->stage || !map->isDrawRoomIcon(arena.room,map->getStayRoomNo()) ||
           !map->isRenderingFloor(dMapInfo_c::calcFloorNo(arena.y,true,arena.room))) continue;
        const auto [remaining,available]=arenaCounts(arena,minimapTypes,minimapAccessibleOnly);
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
void setMapCheckHandler(MapCheckHandler handler,bool (*closeHandler)()) { checkHandler=handler; closeCheckHandler=closeHandler; }
bool mapInspectionActive() { return inspectionContext(); }
ModResult initializeMap(Model* model) {
    state = model;
    void* address=nullptr;
    if(svc_hook->resolve(mod_ctx,"aurora::window::get_window_size",&address,nullptr)==MOD_OK)
        windowSize=reinterpret_cast<WindowSizeFn>(address);
    setMapActionHandler(inspectFromMap);
    // Keep a usable pointer even when the player's mouse-camera option is on.
    mods::hook::add_pre<TrackerMapMouseCapture>(mapMouseCapture);
    mods::hook::add_pre<TrackerMapMouseCursor>(mapMouseCursor);
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
                    if (const auto* anchor=worldMapAnchor(point)) {
                        if (anchor!=&point) p=parse(*anchor,true);
                        else {
                            if (point.contains("world_pos")) {p.x=point.at("world_pos")[0];p.y=point.at("world_pos")[1];p.z=point.at("world_pos")[2];}
                            // Coro is outdoors, but his rewards share one aggregate marker.
                            if (isCoroReward(name)) { p.entrance=true; p.label="Coro"; }
                        }
                        worldPositions.push_back(p);
                    }
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
    const auto mapPicture=mods::hook::add_post<TrackerMapPicture>(captureMapPicture);
    const auto miniAfter=mods::hook::add_post<TrackerMiniDraw>(drawMini);
    minimapAvailable=miniBefore==MOD_OK && mapPicture==MOD_OK && miniAfter==MOD_OK;
    if (!minimapAvailable) minimapEnabled=false;
    const auto dungeonResult=mods::hook::add_post<TrackerDungeonIcons>(drawDungeon);
    if(dungeonResult!=MOD_OK) return dungeonResult;
    const auto mapBefore=mods::hook::add_pre<TrackerMapBackDraw>(beforeFieldMap);
    const auto mapAfter=mods::hook::add_post<TrackerMapBackDraw>(draw);
    mapAvailable = mapBefore==MOD_OK && mapAfter==MOD_OK && mapPicture==MOD_OK;
    if (!mapAvailable) shutdownMap();
    return mapBefore!=MOD_OK ? mapBefore : mapAfter!=MOD_OK ? mapAfter : mapPicture;
}
void shutdownMap() { hitOwner=nullptr; mapHits.clear(); checkHandler=nullptr; closeCheckHandler=nullptr; windowSize=nullptr; setMapActionHandler(nullptr); state = nullptr; mapAvailable = false; flagIndex.clear(); chestTexture.clear(); doneTexture.clear(); iconTextures.clear(); localPositions.clear(); worldPositions.clear(); temples.clear(); arenaEntrances.clear(); exteriorStages.clear(); drawingMeter=nullptr; drawingFieldMap=nullptr; fieldMapMirrored=false; minimapAvailable=false; }
}

