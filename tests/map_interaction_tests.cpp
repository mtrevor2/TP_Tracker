#include "map_interaction.hpp"
#include <stdexcept>
#include <iostream>
#include <limits>
using namespace tracker;
void require(bool v) { if(!v) throw std::runtime_error("map interaction regression"); }
bool near(float a,float b) { return std::abs(a-b)<0.01f; }
int main() {
 MapHitTargets t; t.clear({0,0,200,150}); t.add(50,50,12,12,"Chest"); t.add(90,50,12,12,"Rupee");
 require(t.hit({50,50})=="Chest" && t.hit({90,50})=="Rupee");
 require(t.hit({65,50}).empty() && t.hit({50,75}).empty()); // not merely nearby
 t.add(50,50,16,16,""); require(t.hit({50,50}).empty()); // aggregate marker blocks underlying check
 t.add(220,50,12,12,"Offscreen"); require(t.hit({220,50}).empty());
 t.clear({0,0,200,150}); require(t.hit({50,50}).empty());
 t.add(10,10,8,8,"Behind hint"); t.add(90,10,90,10,""); require(t.hit({10,10}).empty());
 t.clear({0,0,200,150}); t.add(10,10,8,8,"Behind hint");
 t.setHint(MapHitRect{0,0,180,20}); require(t.hit({10,10}).empty());
 t.setHint(std::nullopt); require(t.hit({10,10})=="Behind hint"); // hiding does not clear checks or retain an invisible blocker
 require(t.targets.size()==1);
 t.clear({0,0,200,150});
 for(bool mirrored:{false,true}) {
  FieldMapTransform map{20,10,100,mirrored};
  auto screen=map.screen(40,30); t.add(screen.x,screen.y,12,12,"Chest");
  require(t.hit(screen)=="Chest"); t.clear({0,0,200,150});
 }
 int mapOwner=0,otherOwner=0;
 MapInspectionFrame frame;
 for(bool dungeon:{false,true}) {
  require(frame.begin(&mapOwner,&mapOwner,dungeon,true,"stage",{0,0,200,150},{50,50}));
  frame.targets.add(50,50,12,12,"Chest");
  for(bool visible:{true,false,true,false}) {
   frame.targets.setHint(visible ? std::optional<MapHitRect>{{0,0,100,20}} : std::nullopt);
   require(frame.active(&mapOwner,dungeon,true,"stage") && frame.targets.hit(frame.cursor)=="Chest");
   require(!frame.begin(&otherOwner,&mapOwner,!dungeon,false,"other",{},{}));
   require(frame.active(&mapOwner,dungeon,true,"stage") && frame.targets.hit(frame.cursor)=="Chest");
  }
  require(!frame.active(nullptr,dungeon,true,"stage"));
  require(!frame.active(&mapOwner,dungeon,false,"stage"));
  require(!frame.active(&mapOwner,dungeon,true,"different scene"));
  frame.targets.panel=MapHitRect{40,40,60,60}; require(frame.targets.hit({50,50}).empty());
  frame.targets.panel.reset(); require(frame.targets.hit({50,50})=="Chest");
  require(frame.begin(&mapOwner,&mapOwner,dungeon,false,"stage",{},{}));
  require(!frame.owner && frame.targets.targets.empty());
 }
 auto overlaps=[](MapHitRect a,MapHitRect b) {
  return a.left<b.right && a.right>b.left && a.top<b.bottom && a.bottom>b.top;
 };
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
 std::cout << "Map icon geometry, mirrored hit tests and DPI/letterbox mapping passed\n";
}
