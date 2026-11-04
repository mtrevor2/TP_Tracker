#pragma once
#include "map_projection.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace tracker {
struct MapHitRect {
    float left, top, right, bottom;
    bool contains(MapScreenPoint p) const {
        return std::isfinite(p.x) && std::isfinite(p.y) && p.x>=left && p.x<=right && p.y>=top && p.y<=bottom;
    }
};
// Hit regions come from the actual rendered icons, after all visibility filters
// and mirroring. A marker can represent an interior, dungeon or coincident checks.
struct MapHitTargets {
    struct Target { MapHitRect bounds; std::vector<std::string> names; };
    MapHitRect clip{};
    std::vector<Target> targets;
    // The optional hint can occlude clicks, but never owns the check targets.
    std::optional<MapHitRect> hint, panel;
    void clear(MapHitRect bounds={}) { clip=bounds; targets.clear(); hint.reset(); panel.reset(); }
    void setHint(std::optional<MapHitRect> bounds) { hint=bounds; }
    void add(float x,float y,float halfWidth,float halfHeight,const std::string& name) {
        addGroup(x,y,halfWidth,halfHeight,name.empty() ? std::vector<std::string>{} : std::vector<std::string>{name});
    }
    void addGroup(float x,float y,float halfWidth,float halfHeight,std::vector<std::string> names) {
        targets.push_back({{x-halfWidth,y-halfHeight,x+halfWidth,y+halfHeight},std::move(names)});
    }
    std::vector<std::string> hit(MapScreenPoint p) const {
        if(!clip.contains(p) || (hint && hint->contains(p)) || (panel && panel->contains(p))) return {};
        float closest=1e30f;
        const Target* best=nullptr;
        for(const auto& target:targets) {
            if(!target.bounds.contains(p)) continue;
            if(target.names.empty()) return {};
            float dx=p.x-(target.bounds.left+target.bounds.right)*0.5f;
            float dy=p.y-(target.bounds.top+target.bounds.bottom)*0.5f;
            const float distance=dx*dx+dy*dy;
            if(distance<=closest) { closest=distance; best=&target; }
        }
        if(!best) return {};
        auto names=best->names;
        for(const auto& target:targets) {
            if(std::abs(target.bounds.left+target.bounds.right-best->bounds.left-best->bounds.right)>1 ||
               std::abs(target.bounds.top+target.bounds.bottom-best->bounds.top-best->bounds.bottom)>1) continue;
            names.insert(names.end(),target.names.begin(),target.names.end());
        }
        std::sort(names.begin(),names.end());
        names.erase(std::unique(names.begin(),names.end()),names.end());
        return names;
    }
};
// Selection belongs to the active map, not the optional hint or a short draw
// timeout. Ignore other renderers; zooming out or changing scenes invalidates it.
struct MapInspectionFrame {
    const void* owner=nullptr;
    bool dungeon=false;
    std::string stage;
    MapScreenPoint cursor{};
    MapHitTargets targets;
    void clear() { owner=nullptr; stage.clear(); targets.clear(); }
    bool begin(const void* drawOwner,const void* activeOwner,bool isDungeon,bool inspectable,
               const std::string& scene,MapHitRect clip,MapScreenPoint reticle) {
        if(!drawOwner || drawOwner!=activeOwner) return false;
        clear(); dungeon=isDungeon; cursor=reticle;
        if(inspectable) { owner=activeOwner; stage=scene; targets.clear(clip); }
        return true;
    }
    bool active(const void* activeOwner,bool isDungeon,bool inspectable,const std::string& scene) const {
        return owner && owner==activeOwner && dungeon==isDungeon && inspectable && stage==scene;
    }
};

struct DungeonSummaryLayout {
    MapHitRect bounds;
    bool compact;
};
// The narrow layout uses the native space below the title and above the item
// column. Never move the panel into the map when the right gutter disappears.
inline std::optional<DungeonSummaryLayout> dungeonSummaryLayout(MapHitRect map,MapHitRect screen,MapHitRect compactSlot) {
    constexpr float width=112,height=80,margin=8,frame=24;
    auto valid=[](MapHitRect r) {
        return std::isfinite(r.left) && std::isfinite(r.top) && std::isfinite(r.right) && std::isfinite(r.bottom)
            && r.right>r.left && r.bottom>r.top;
    };
    if(!valid(map) || !valid(screen)) return {};
    const float right=screen.right-margin;
    const float minTop=std::max(map.top+44,screen.top+104);
    const float maxTop=screen.bottom-height-margin;
    if(right-width>=map.right+frame && minTop<=maxTop)
        return DungeonSummaryLayout{{right-width,minTop,right,minTop+height},false};
    compactSlot.left=std::max(compactSlot.left,screen.left+margin);
    compactSlot.right=std::min({compactSlot.right,screen.right-margin,map.left-frame});
    compactSlot.top=std::max(compactSlot.top,screen.top+margin);
    compactSlot.bottom=std::min(compactSlot.bottom,screen.bottom-margin);
    constexpr float compactHeight=24;
    if(!valid(compactSlot) || compactSlot.right-compactSlot.left<width || compactSlot.bottom-compactSlot.top<compactHeight) return {};
    const float left=(compactSlot.left+compactSlot.right-width)/2;
    return DungeonSummaryLayout{{left,compactSlot.top,left+width,compactSlot.top+compactHeight},true};
}

// SDL mouse positions use logical window units. Match Aurora's letterboxed
// presentation viewport before converting to the game's widescreen J2D space.
inline std::optional<MapScreenPoint> mapMousePoint(float x,float y,float windowW,float windowH,
    unsigned nativeW,unsigned nativeH,unsigned contentW,unsigned contentH,MapHitRect game) {
    if(windowW<=0 || windowH<=0 || !nativeW || !nativeH || !contentW || !contentH) return {};
    unsigned h=std::min(nativeH,std::max(1u,static_cast<unsigned>(std::lround(double(nativeW)*contentH/contentW))));
    unsigned w=h==nativeH ? std::min(nativeW,std::max(1u,static_cast<unsigned>(std::lround(double(h)*contentW/contentH)))) : nativeW;
    const float left=(nativeW-w)/2,top=(nativeH-h)/2;
    const float px=x*nativeW/windowW,py=y*nativeH/windowH;
    if(!std::isfinite(px) || !std::isfinite(py) || px<left || py<top || px>=left+w || py>=top+h) return {};
    return MapScreenPoint{game.left+(px-left)/w*(game.right-game.left),game.top+(py-top)/h*(game.bottom-game.top)};
}
}
