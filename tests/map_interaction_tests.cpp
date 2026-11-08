#include "map_interaction.hpp"
#include <stdexcept>
#include <iostream>
#include <limits>
using namespace tracker;
void require(bool v) { if(!v) throw std::runtime_error("map interaction regression"); }
bool near(float a,float b) { return std::abs(a-b)<0.01f; }
int main() {
 MapHitTargets t; t.clear({0,0,200,150}); t.add(50,50,12,12,"Chest"); t.add(90,50,12,12,"Rupee");
 require(t.hit({50,50})==std::vector<std::string>{"Chest"} && t.hit({90,50})==std::vector<std::string>{"Rupee"});
 require(t.hit({65,50}).empty() && t.hit({50,75}).empty()); // not merely nearby
 t.add(50,50,16,16,""); require(t.hit({50,50}).empty()); // aggregate marker blocks underlying check
 t.add(220,50,12,12,"Offscreen"); require(t.hit({220,50}).empty());
 t.clear({0,0,200,150}); require(t.hit({50,50}).empty());
 t.addGroup(50,50,12,12,{"Iza Helping Hand","Iza Raging Rapids Minigame"});
 require(t.hit({50,50})==std::vector<std::string>({"Iza Helping Hand","Iza Raging Rapids Minigame"}));
 t.add(50,50,12,12,"Iza Helping Hand");
 require(t.hit({50,50}).size()==2); // duplicate anchors cannot create duplicate tabs
 t.add(50,50,12,12,"Other overlapping check");
 require(t.hit({50,50}).size()==3);
 t.add(90,50,12,12,"Separate check");
 require(t.hit({90,50})==std::vector<std::string>{"Separate check"});
 t.clear({0,0,200,150});
 t.add(10,10,8,8,"Behind hint"); t.add(90,10,90,10,""); require(t.hit({10,10}).empty());
 t.clear({0,0,200,150}); t.add(10,10,8,8,"Behind hint");
 t.setHint(MapHitRect{0,0,180,20}); require(t.hit({10,10}).empty());
 t.setHint(std::nullopt); require(t.hit({10,10})==std::vector<std::string>{"Behind hint"}); // hiding does not clear checks or retain an invisible blocker
 require(t.targets.size()==1);
 t.clear({0,0,200,150});
 for(bool mirrored:{false,true}) {
  FieldMapTransform map{20,10,100,mirrored};
  auto screen=map.screen(40,30); t.add(screen.x,screen.y,12,12,"Chest");
  require(t.hit(screen)==std::vector<std::string>{"Chest"}); t.clear({0,0,200,150});
 }
 int mapOwner=0,otherOwner=0;
 MapInspectionFrame frame;
 for(bool dungeon:{false,true}) {
  require(frame.begin(&mapOwner,&mapOwner,dungeon,true,"stage",{0,0,200,150},{50,50}));
  frame.targets.add(50,50,12,12,"Chest");
  for(bool visible:{true,false,true,false}) {
   frame.targets.setHint(visible ? std::optional<MapHitRect>{{0,0,100,20}} : std::nullopt);
   require(frame.active(&mapOwner,dungeon,true,"stage") && frame.targets.hit(frame.cursor)==std::vector<std::string>{"Chest"});
   require(!frame.begin(&otherOwner,&mapOwner,!dungeon,false,"other",{},{}));
   require(frame.active(&mapOwner,dungeon,true,"stage") && frame.targets.hit(frame.cursor)==std::vector<std::string>{"Chest"});
  }
  require(!frame.active(nullptr,dungeon,true,"stage"));
  require(!frame.active(&mapOwner,dungeon,false,"stage"));
  require(!frame.active(&mapOwner,dungeon,true,"different scene"));
  frame.targets.panel=MapHitRect{40,40,60,60}; require(frame.targets.hit({50,50}).empty());
  frame.targets.panel.reset(); require(frame.targets.hit({50,50})==std::vector<std::string>{"Chest"});
  require(frame.begin(&mapOwner,&mapOwner,dungeon,false,"stage",{},{}));
  require(!frame.owner && frame.targets.targets.empty());
 }
 auto overlaps=[](MapHitRect a,MapHitRect b) {
  return a.left<b.right && a.right>b.left && a.top<b.bottom && a.bottom>b.top;
 };
 // The description stays in the bottom row between Move/Back and the full
 // collectible column, including counters above the description's own height.
 const MapHitRect field{0,40,608,420};
 const std::vector<MapHitRect> controls{{20,350,100,400}},counters{{450,240,580,400}};
 auto description=fieldMapDescriptionLayout(field,controls,counters);
 require(description && near(description->bottom,412) && near(description->left,108) && near(description->right,442));
 for(const auto& r:controls) require(!overlaps(*description,r));
 for(const auto& r:counters) require(!overlaps(*description,r));
 const auto wide=fieldMapDescriptionLayout({-120,40,728,420},{{-100,350,30,400}},{{570,240,700,400}});
 require(wide && near(wide->bottom,412) && near(wide->left,38) && near(wide->right,562));
 // Wider labels narrow the box; they never cause it to jump above the controls.
 const auto wideLabels=fieldMapDescriptionLayout({-120,40,728,420},{{-100,350,240,400}},{{570,240,700,400}});
 require(wideLabels && near(wideLabels->left,248) && near(wideLabels->bottom,412));
 const auto highCounter=fieldMapDescriptionLayout(field,controls,{{450,240,580,300}});
 require(highCounter && near(highCounter->right,442) && near(highCounter->bottom,412));
 const auto clearBottom=fieldMapDescriptionLayout(field,{},counters);
 require(clearBottom && near(clearBottom->left,12) && near(clearBottom->bottom,412));
 for(float ratio:{9.f/16,1.f,4.f/3,16.f/10,16.f/9,21.f/9,32.f/9}) {
  const float w=std::max(608.f,448*ratio),h=std::max(448.f,608/ratio);
  const MapHitRect map{(608-w)/2+16,(448-h)/2+40,(608+w)/2-16,(448+h)/2-28};
  for(float scale:{0.75f,1.f,1.5f,2.f}) for(float labelWidth:{100.f,180.f}) {
   const std::vector<MapHitRect> nativeControls{
    {map.left-5,map.bottom-55*scale,map.left+labelWidth*scale,map.bottom-25*scale},
    {map.left-5,map.bottom-27*scale,map.left+labelWidth*scale,map.bottom}
   };
   const std::vector<MapHitRect> nativeCounters{
    {map.right-80*scale,map.bottom-185*scale,map.right+8,map.bottom-105*scale},
    {map.right-80*scale,map.bottom-102*scale,map.right+8,map.bottom-55*scale},
    {map.right-80*scale,map.bottom-50*scale,map.right+8,map.bottom-8}
   };
   const auto panel=fieldMapDescriptionLayout(map,nativeControls,nativeCounters);
   // If giant HUD elements leave no reading room, omit rather than overlap
   // or move the description upward into the map.
   const float gapWidth=map.right-map.left-(labelWidth+80)*scale-16;
   require(panel.has_value()==(gapWidth>=180));
   if(!panel) continue;
   require(panel->left>=map.left+12 && panel->right<=map.right-12 && panel->top>=map.top+8 && near(panel->bottom,map.bottom-8));
   require(panel->right-panel->left>=180 && near(panel->bottom-panel->top,58));
   for(const auto& r:nativeControls) require(!overlaps(*panel,{r.left-7,r.top-7,r.right+7,r.bottom+7}));
   for(const auto& r:nativeCounters) require(!overlaps(*panel,{r.left-7,r.top-7,r.right+7,r.bottom+7}));
  }
 }
 // Resizing across the old width threshold keeps the bottom row and blocker.
 for(float right:{679.f,680.f,681.f,679.f}) {
  const auto box=fieldMapDescriptionLayout({0,40,right,420},{{20,350,150,400}},{{right-154,240,right-28,400}});
  require(box && near(box->left,158) && near(box->bottom,412));
  t.clear({0,40,right,420}); t.add(300,380,8,8,"Behind description");
  t.setHint(*box); require(t.hit({300,380}).empty());
  t.add(300,320,8,8,"Above description"); require(!t.hit({300,320}).empty());
 }
 require(!fieldMapDescriptionLayout({0,0,100,100},{},{}));
 require(!fieldMapDescriptionLayout(field,{{0,0,608,420}},{}));
 require(!fieldMapDescriptionLayout({0,0,std::numeric_limits<float>::quiet_NaN(),420},{},{}));
 // Native 608x448 layout: the title ends at 67, the item starts at 99,
 // and the highest floor row starts at 65. The compact panel must fit the
 // gap over the item column, not cover either the floor rows or the map.
 for(float ratio:{9.f/16,1.f,4.f/3,16.f/10,16.f/9,21.f/9,32.f/9}) {
  const float w=std::max(608.f,448*ratio),h=std::max(448.f,608/ratio);
  for(float offset:{-12.f,0.f,12.f}) {
   const MapHitRect screen{(608-w)/2,(448-h)/2,(608+w)/2,(448+h)/2};
   const float itemCenter=screen.left+75*w/608;
   const float mapLeft=screen.left+274*w/608+offset;
   const MapHitRect map{mapLeft,74,mapLeft+288,371};
   const MapHitRect title{screen.left,29,screen.right,67};
   const MapHitRect item{itemCenter-36,99,itemCenter+36,171};
   const MapHitRect floors{screen.left+137*w/608,65,map.left-12,380};
   const MapHitRect slot{itemCenter-56,title.bottom+4,itemCenter+56,item.top-4};
   const auto layout=dungeonSummaryLayout(map,screen,slot);
   require(layout.has_value());
   const auto panel=layout->bounds;
   require(panel.left>=screen.left+8 && panel.right<=screen.right-8 && panel.top>=screen.top+8 && panel.bottom<=screen.bottom-8);
   require(near(panel.right-panel.left,112) && near(panel.bottom-panel.top,layout->compact ? 24 : 80));
   require(!overlaps(panel,{map.left-24,map.top-8,map.right+24,map.bottom+8}));
   require(!overlaps(panel,title) && !overlaps(panel,item) && !overlaps(panel,floors));
   if(w==608) require(layout->compact);
  }
 }
 // Resizing across the exact breakpoint must never put the summary inside
 // the map or retain the previous frame's panel/hit blocker.
 for(float right:{719.f,720.f,721.f,719.f}) {
  const auto layout=dungeonSummaryLayout({274,74,576,371},{0,0,right,448},{19,71,131,95});
  require(layout && layout->compact==(right<720));
  require(!overlaps(layout->bounds,{250,66,600,379}));
 }
 require(!dungeonSummaryLayout({0,0,100,100},{0,0,100,100},{})); // no safe space: never overlay the map
 require(!dungeonSummaryLayout({0,0,100,100},{0,0,std::numeric_limits<float>::quiet_NaN(),100},{}));
 MapHitRect game{-123,0,731,480};
 auto center=mapMousePoint(960,540,1920,1080,1920,1080,1920,1080,game);
 require(center && near(center->x,304) && near(center->y,240));
 auto dpi=mapMousePoint(400,300,800,600,1600,1200,1920,1080,game);
 require(dpi && near(dpi->x,304) && near(dpi->y,240));
 auto quarter=mapMousePoint(200,300,800,600,1600,1200,1920,1080,game);
 require(quarter && near(quarter->x,90.5f));
 require(!mapMousePoint(20,20,800,600,1600,1200,1920,1080,game)); // letterbox
 require(!mapMousePoint(0,0,0,600,1600,1200,1920,1080,game));
 require(!mapMousePoint(std::numeric_limits<float>::quiet_NaN(),0,800,600,1600,1200,1920,1080,game));
 std::cout << "Map layouts, icon geometry, mirrored hit tests and DPI/letterbox mapping passed\n";
}
