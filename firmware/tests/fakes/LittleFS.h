#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdint>
class String {
 public:
  std::string s;
  String()=default;String(const char* t):s(t){} String(std::string t):s(std::move(t)){}
  const char* c_str()const{return s.c_str();} size_t length()const{return s.size();}
  bool endsWith(const char* suffix)const{std::string t(suffix);return s.size()>=t.size()&&s.compare(s.size()-t.size(),t.size(),t)==0;}
  int lastIndexOf(char c)const{auto n=s.rfind(c);return n==s.npos?-1:int(n);}
  String substring(size_t a)const{return s.substr(a);}String substring(size_t a,size_t b)const{return s.substr(a,b-a);}
  friend String operator+(const String&a,const String&b){return a.s+b.s;}
};
namespace fs_test {
inline std::filesystem::path root;
inline bool limit_names=true, reject_writes=false;
inline std::filesystem::path path(const String&s){return root/s.s.substr(1);}
inline bool valid(const String&s){
  if(!limit_names)return true;
  for(const auto& part:std::filesystem::path(s.s))if(part.string().size()>32)return false;
  return true;
}
}
class File {
  struct Data {
    std::filesystem::path path;std::fstream stream;bool directory=false;
    std::vector<std::filesystem::path> entries;size_t next=0;
  };
  std::shared_ptr<Data> d;
 public:
  File()=default;
  explicit File(std::filesystem::path path,const char* mode="r") {
    d=std::make_shared<Data>();d->path=path;
    d->directory=std::filesystem::is_directory(path);
    if(d->directory){for(auto&e:std::filesystem::directory_iterator(path))d->entries.push_back(e.path());}
    else d->stream.open(path,mode[0]=='w'?std::ios::out|std::ios::binary|std::ios::trunc:std::ios::in|std::ios::binary);
  }
  explicit operator bool()const{return d&&(d->directory||d->stream.is_open());}
  bool isDirectory()const{return d&&d->directory;}
  File openNextFile(){return d&&d->next<d->entries.size()?File(d->entries[d->next++]):File();}
  String name()const{return d?String(d->path.filename().string()):String();}
  void close(){d.reset();}
  size_t size(){return d&&std::filesystem::exists(d->path)?std::filesystem::file_size(d->path):0;}
  size_t read(uint8_t*out,size_t n){d->stream.read(reinterpret_cast<char*>(out),n);return d->stream.gcount();}
  size_t write(const uint8_t*bytes,size_t n){if(fs_test::reject_writes)return 0;d->stream.write(reinterpret_cast<const char*>(bytes),n);d->stream.flush();return d->stream?n:0;}
  bool seek(size_t n){d->stream.clear();d->stream.seekg(n);return bool(d->stream);}
  void flush(){d->stream.flush();}
};
struct FakeLittleFS {
  bool begin(bool){return true;}
  bool exists(const String&p){return fs_test::valid(p)&&std::filesystem::exists(fs_test::path(p));}
  bool mkdir(const String&p){return fs_test::valid(p)&&std::filesystem::create_directory(fs_test::path(p));}
  bool rmdir(const String&p){return std::filesystem::remove(fs_test::path(p));}
  bool remove(const String&p){return std::filesystem::remove(fs_test::path(p));}
  File open(const String&p,const char*mode="r"){return fs_test::valid(p)?File(fs_test::path(p),mode):File();}
  uint64_t totalBytes(){return 1572864;}
  uint64_t usedBytes(){uint64_t n=0;for(auto&e:std::filesystem::recursive_directory_iterator(fs_test::root))if(e.is_regular_file())n+=e.file_size();return n;}
};
inline FakeLittleFS LittleFS;
