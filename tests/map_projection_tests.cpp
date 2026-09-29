#include "map_projection.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace tracker;

void near(float actual,float expected,const char* message) {
    if (!std::isfinite(actual) || std::abs(actual-expected)>0.02f) throw std::runtime_error(message);
}

// The unmirrored calcAllMapPos2D contract, isolated from the native SDK.
// Texture expectations below use the independent rendering-camera equation.
struct FieldMap {
    float mStageTransX=0, mStageTransZ=0;
    float regionMidX=150000, regionMidZ=-90000;
    float regionScreenX, regionScreenY, pixelsPerUnit;
    void calcAllMapPos2D(float x,float z,float* outX,float* outY) const {
        *outX=regionScreenX+(x-regionMidX)*pixelsPerUnit;
        *outY=regionScreenY+(z-regionMidZ)*pixelsPerUnit;
    }
};

int main() {
    try {
        int cases=0;
        for(bool mirrored : {false,true})
        for(float width : {608.f,1067.f,1422.f})
        for(float zoom : {0.25f,1.f,4.f})
        for(float translation : {0.f,37.f,-113.f})
        for(auto pan : std::array<MapScreenPoint,5>{{{0,0},{300,-400},{-1400,250},{900,1300},{-500,-800}}}) {
            const float height=400, left=-75, top=24;
            const float centerX=left+width/2, centerY=top+height/2;
            FieldMap map{pan.x,pan.y,150000,-90000,centerX+51,centerY-27,zoom*0.025f};
            const FieldMapTransform transform{translation,-9,centerX+translation,mirrored};
            // calcRenderingPos includes pan. Texture rendering maps this world
            // camera center to the scissor center and flips only horizontal X.
            const float cameraX=map.regionMidX+pan.x+(centerX-map.regionScreenX)/map.pixelsPerUnit;
            const float cameraZ=map.regionMidZ+pan.y+(centerY-map.regionScreenY)/map.pixelsPerUnit;
            const float viewWidth=width/map.pixelsPerUnit, viewHeight=height/map.pixelsPerUnit;
            for(auto point : std::array<MapScreenPoint,4>{{{150900,-89500},{147700,-91200},{154500,-86000},{149999,-90001}}}) {
                const auto actual=transform.world(map,point.x,point.y);
                const float relativeX=(point.x-cameraX)/viewWidth;
                const float expectedX=left+translation+width*(0.5f+(mirrored?-relativeX:relativeX));
                const float expectedY=top-9+height*(0.5f+(point.y-cameraZ)/viewHeight);
                near(actual.x,expectedX,"marker drifts from map texture while panning/zooming");
                near(actual.y,expectedY,"marker vertical projection differs from texture");

                // Native cursor data is unmirrored; drawing and hit-testing
                // must use the same final screen space as a marker at its site.
                float arrowX,arrowY;
                map.calcAllMapPos2D(point.x-pan.x,point.y-pan.y,&arrowX,&arrowY);
                const auto cursor=transform.screen(arrowX,arrowY);
                near(cursor.x,actual.x,"hover cursor and marker disagree horizontally");
                near(cursor.y,actual.y,"hover cursor and marker disagree vertically");
                // Moving only the cursor cannot move a world-anchored marker.
                const auto movedCursor=transform.screen(arrowX+24,arrowY+12);
                const auto sameMarker=transform.world(map,point.x,point.y);
                near(sameMarker.x,actual.x,"cursor motion changed marker position");
                near(std::abs(movedCursor.x-sameMarker.x),24,"hover radius changed with mirroring");
                ++cases;
            }
            const auto before=transform.world(map,150000,-90000);
            map.mStageTransX+=600;
            map.mStageTransZ+=200;
            const auto after=transform.world(map,150000,-90000);
            near(after.x-before.x,(mirrored?1.f:-1.f)*600*map.pixelsPerUnit,"wrong horizontal pan direction");
            near(after.y-before.y,-200*map.pixelsPerUnit,"wrong vertical pan direction");
            // Region summary centers undergo the same final transform.
            const auto summary=transform.screen(centerX+80,centerY);
            near(summary.x,centerX+translation+(mirrored?-80.f:80.f),"region summary not mirrored around viewport");
        }
        std::cout << "Validated " << cases << " map projections, normal/Mirror Mode panning and hover positions.\n";
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
