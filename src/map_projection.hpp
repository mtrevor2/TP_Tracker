#pragma once

namespace tracker {
struct MapScreenPoint { float x, y; };

// Match the final transform used by Dusklight's native field-map icons and
// cursor: project after subtracting the map pan, translate, then mirror about
// the scissor center. The cursor's world position must never anchor a marker.
struct FieldMapTransform {
    float translateX, translateY, mirrorCenterX;
    bool mirrored;

    MapScreenPoint screen(float x, float y) const {
        x += translateX;
        y += translateY;
        return {mirrored ? 2.f * mirrorCenterX - x : x, y};
    }

    template<class FieldMap>
    MapScreenPoint world(FieldMap& map, float worldX, float worldZ) const {
        float x=0, y=0;
        map.calcAllMapPos2D(worldX-map.mStageTransX, worldZ-map.mStageTransZ, &x, &y);
        return screen(x,y);
    }
};
}
