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
