#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace nws {
inline int64_t isoTime(const char* input){
  if(!input)return 0;
  int year,month,day,hour,minute,second,used=0;
  if(std::sscanf(input,"%4d-%2d-%2dT%2d:%2d:%2d%n",&year,&month,&day,&hour,&minute,&second,&used)!=6)return 0;
  if(year<1970||year>2100||month<1||month>12||day<1||hour<0||hour>23||minute<0||minute>59||second<0||second>59)return 0;
  const int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap=(year%4==0&&year%100!=0)||year%400==0;
  if(day>days_in_month[month-1]+(month==2&&leap))return 0;
  const char* zone=input+used;
  if(*zone=='.'){++zone;if(*zone<'0'||*zone>'9')return 0;while(*zone>='0'&&*zone<='9')++zone;}
  int offset=0;
  if(*zone=='Z'){if(zone[1])return 0;}
  else if(*zone=='+'||*zone=='-'){
    int hours=0,minutes=0,end=0;
    if(std::sscanf(zone+1,"%2d:%2d%n",&hours,&minutes,&end)!=2||zone[1+end]||hours<0||hours>23||minutes<0||minutes>59)return 0;
    offset=(hours*60+minutes)*60;if(*zone=='-')offset=-offset;
  }else return 0;
  year-=month<=2;const int era=(year>=0?year:year-399)/400,yoe=year-era*400;
  const int doy=(153*(month+(month>2?-3:9))+2)/5+day-1;
  const int days=era*146097+yoe*365+yoe/4-yoe/100+doy-719468;
  return int64_t(days)*86400+hour*3600+minute*60+second-offset;
}
inline bool ends(const char* text,const char* suffix){size_t n=std::strlen(text),m=std::strlen(suffix);return n>=m&&!std::strcmp(text+n-m,suffix);}
inline int alertRank(const char* event){
  if(!std::strcmp(event,"Tornado Warning"))return 1000;
  if(!std::strcmp(event,"Severe Thunderstorm Warning"))return 950;
  if(!std::strcmp(event,"Flash Flood Warning"))return 900;
  return ends(event,"Warning")?800:ends(event,"Watch")?600:ends(event,"Advisory")?400:200;
}
}
