#pragma once
#include <string>
namespace aprs_login {
enum class Reply { Other,Verified,Unverified };
inline Reply parse(const std::string& line,const std::string& identity){
  const std::string prefix="# logresp "+identity+" ";
  if(line.size()>512||line.compare(0,prefix.size(),prefix))return Reply::Other;
  const size_t end=line.find_first_of(", \r\n",prefix.size());
  const std::string status=line.substr(prefix.size(),end==std::string::npos?std::string::npos:end-prefix.size());
  return status=="verified"?Reply::Verified:status=="unverified"?Reply::Unverified:Reply::Other;
}
}
