#pragma once
namespace setup_credentials {
template<class Text,class Generator>
bool ensure(Text& wifi,Text& admin,bool& custom,Generator generate){
  bool changed=false;
  if(!admin.length()){
    if(!wifi.length())wifi=generate();
    admin=wifi;custom=false;changed=true;
  }else if(!wifi.length()){
    // Legacy shared keys have unknown origin: retain authentication privately.
    custom=true;changed=true;
  }
  if(!wifi.length() || (custom && wifi==admin)){
    do{wifi=generate();}while(wifi==admin);
    changed=true;
  }
  return changed;
}
template<class Text,class Generator>
void chooseAdmin(Text& wifi,Text& admin,bool& custom,const Text& password,Generator generate){
  if(!password.length())return;
  admin=password;custom=true;ensure(wifi,admin,custom,generate);
}
}
