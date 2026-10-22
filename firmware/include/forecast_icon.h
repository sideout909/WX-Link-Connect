#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace forecast_icon {
enum class Kind { Clear,PartlyCloudy,Cloudy,Rain,Thunder,Snow,Fog,Wind,Unknown };
inline Kind classify(std::string text){
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char ch){return char(std::tolower(ch));});
  auto has=[&](const char* token){return text.find(token)!=std::string::npos;};
  if(has("thunder"))return Kind::Thunder;
  if(has("snow")||has("sleet")||has("freezing")||has("wintry")||has("ice"))return Kind::Snow;
  if(has("rain")||has("shower")||has("drizzle"))return Kind::Rain;
  if(has("fog")||has("haze")||has("smoke"))return Kind::Fog;
  if(has("partly")||has("mostly sunny"))return Kind::PartlyCloudy;
  if(has("cloud")||has("overcast"))return Kind::Cloudy;
  if(has("wind")||has("breezy"))return Kind::Wind;
  if(has("sunny")||has("clear"))return Kind::Clear;
  return Kind::Unknown;
}
inline bool illuminated(float illumination,float x,float half_width,bool waning,bool southern){
  if(illumination<0||illumination>100)return false;
  const bool left=waning!=southern;const float terminator=(1-2*illumination/100)*half_width;
  return left?x<=-terminator:x>=terminator;
}
}
