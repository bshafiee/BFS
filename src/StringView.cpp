/*
 * StringView.cpp
 *
 *  Created on: Apr 1, 2015
 *      Author: behrooz
 */

#include "StringView.h"
#include <cstring>

namespace BFS {
using namespace std;

StringView::StringView(const char* _ptr, const unsigned int _len):
    ptr(_ptr),size(_len) {}

StringView::~StringView() {}

void StringView::split(std::vector<StringView>& _output, const char _delimiter) {
  unsigned int start = 0;
  for(unsigned int i=0;i<size;i++){
    if(ptr[i] == _delimiter){
      if(i>start){ //No empty token
        _output.emplace_back(ptr+start,i-start);
      }
      start = (i+1>=size)?i:i+1;
    }
  }
  if(size>0 && size-1>=start && ptr[start]!=_delimiter)
    _output.emplace_back(ptr+start,size-start);
}

std::string StringView::toString() {
  return string(ptr,size);
}

bool StringView::operator ==(const StringView& other) const {
  if(other.size != this->size)
    return false;
  return (memcmp(ptr,other.ptr,size)==0);
}

bool StringView::operator ==(const std::string& other) const {
  if(other.length() != this->size)
    return false;
  return (memcmp(ptr,other.c_str(),size)==0);
}

bool StringView::operator !=(const StringView& other) const {
  return !(*this == other);
}

bool StringView::operator !=(const std::string& other) const {
  return !(*this == other);
}

} /* namespace BFS */
