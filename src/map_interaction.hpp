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
// and mirroring. Empty names occlude individual checks under aggregate markers.
struct MapHitTargets {
    struct Target { MapHitRect bounds; std::string name; };
    MapHitRect clip{};
    std::vector<Target> targets;
    // The optional hint can occlude clicks, but never owns the check targets.
    std::optional<MapHitRect> hint;
    void clear(MapHitRect bounds={}) { clip=bounds; targets.clear(); hint.reset(); }
    void setHint(std::optional<MapHitRect> bounds) { hint=bounds; }
    void add(float x,float y,float halfWidth,float halfHeight,const std::string& name) {
        targets.push_back({{x-halfWidth,y-halfHeight,x+halfWidth,y+halfHeight},name});
    }
    std::string hit(MapScreenPoint p) const {
        if(!clip.contains(p) || (hint && hint->contains(p))) return {};
        float closest=1e30f;
        const Target* best=nullptr;
        for(const auto& target:targets) {
            if(!target.bounds.contains(p)) continue;
            if(target.name.empty()) return {}; // summary drawn over a check
            float dx=p.x-(target.bounds.left+target.bounds.right)*0.5f;
            float dy=p.y-(target.bounds.top+target.bounds.bottom)*0.5f;
            const float distance=dx*dx+dy*dy;
            if(distance<=closest) { closest=distance; best=&target; }
        }
        return best ? best->name : std::string{};
    }
};
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
