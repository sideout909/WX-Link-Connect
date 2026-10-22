#pragma once
#include <cmath>
#include <cstdint>
#include "forecast_icon.h"

namespace moon_icon {
constexpr uint16_t gray(unsigned v){return ((v&248)<<8)|((v&252)<<3)|(v>>3);}

// Small, deliberately stylized lunar markings, rather than a geographic map.
// Texture is clipped by the same phase mask as the illuminated surface.
inline uint16_t surface(float x,float y){
  struct Crater {float x,y,rx,ry;};
  constexpr Crater craters[]={{-.36f,-.28f,.25f,.20f},{.28f,.10f,.21f,.28f},
    {-.12f,.49f,.17f,.14f},{.43f,-.48f,.13f,.13f},{-.58f,.17f,.12f,.15f}};
  unsigned shade=unsigned(236-13*(x*x+y*y));
  for(const auto& c:craters){
    const float dx=(x-c.x)/c.rx,dy=(y-c.y)/c.ry,d=dx*dx+dy*dy;
    if(d<.65f)shade=181;
    else if(d<1)shade=204;
    else if(d<1.28f&&dy>0)shade=242;
  }
  return gray(shade);
}

inline uint16_t nightTint(uint16_t color){
  const unsigned level=(color>>11)&31;
  const unsigned r=173*level/31,g=135*level/31,b=83*level/31;
  return ((r&248)<<8)|((g&252)<<3)|(b>>3);
}

template <typename Canvas>
void draw(Canvas& canvas,int cx,int cy,int radius,float illumination,bool waning,bool southern,bool low_light=false){
  for(int y=-radius;y<=radius;++y){
    const float half=std::sqrt(float(radius*radius-y*y));
    for(int x=-int(half);x<=int(half);++x){
      const bool lit=forecast_icon::illuminated(illumination,float(x),half,waning,southern);
      const uint16_t color=lit?surface(float(x)/radius,float(y)/radius):gray(47);
      canvas.drawPixel(cx+x,cy+y,low_light?nightTint(color):color);
    }
  }
  canvas.drawCircle(cx,cy,radius,low_light?nightTint(gray(143)):gray(143));
}
}
