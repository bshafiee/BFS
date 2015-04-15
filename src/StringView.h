/*
 * StringView.h
 *
 *  Created on: Apr 1, 2015
 *      Author: behrooz
 */

#ifndef SRC_STRINGVIEW_H_
#define SRC_STRINGVIEW_H_

#include <vector>
#include <string>

namespace BFS {

class StringView {
public:
  const char* ptr;
  const unsigned int size;
  StringView(const char* _ptr,const unsigned int _len);
  virtual ~StringView();
  void split(std::vector<StringView> &_output,const char _delimiter);
  std::string toString();
  bool operator == (const StringView &other) const;
  bool operator == (const std::string &other) const;
  bool operator != (const StringView &other) const;
  bool operator != (const std::string &other) const;
};

} /* namespace BFS */

#endif /* SRC_STRINGVIEW_H_ */
