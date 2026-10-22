#pragma once

#include <cstddef>
#include <cstring>
#include <initializer_list>

namespace wxconnect::web_security {
inline bool validCsrf(const char* expected,const char* supplied){
  if(!expected||!supplied||std::strlen(expected)!=32||std::strlen(supplied)!=32)return false;
  unsigned difference=0;
  for(size_t i=0;i<32;++i)difference|=static_cast<unsigned char>(expected[i])^
      static_cast<unsigned char>(supplied[i]);
  return difference==0;
}
inline bool sameOrigin(const char* origin,const char* host){
  if(!origin||!host||!origin[0]||!host[0])return false;
  const size_t host_size=std::strlen(host);
  for(const char* scheme:{"http://","https://"}){
    const size_t scheme_size=std::strlen(scheme);
    if(std::strncmp(origin,scheme,scheme_size)==0&&
       std::strlen(origin+scheme_size)==host_size&&
       std::strcmp(origin+scheme_size,host)==0)return true;
  }
  return false;
}
inline bool validSessionId(const char* id){
  if(!id||std::strlen(id)!=32)return false;
  for(size_t i=0;i<32;++i)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
  return true;
}
}  // namespace wxconnect::web_security
