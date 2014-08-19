/*
 * Container.h
 *
 *  Created on: 2014-05-28
 *      Author: Behrooz Shafiee Sarjaz
 */

#ifndef CONTAINER_H_
#define CONTAINER_H_

#include <Account.h>

namespace Swift {

class Object;

class Container {
  Account* account;
  std::string name;
public:
  Container(Account *_account, std::string _name = "");
  virtual ~Container();

  /** API Functions **/
  SwiftResult<std::vector<Object>*>* swiftGetObjects(bool _newest = false);

  SwiftResult<std::istream*>* swiftListObjects(HTTPHeader &_formatHeader = HEADER_FORMAT_APPLICATION_JSON,
      std::vector<HTTPHeader> *_uriParam = nullptr, bool _newest = false);

  SwiftResult<void*>* swiftCreateContainer(std::vector<HTTPHeader> *_reqMap=nullptr);

  SwiftResult<void*>* swiftDeleteContainer();

  SwiftResult<void*>* swiftCreateMetadata(
      std::vector<std::pair<std::string, std::string>> &_metaData,
      std::vector<HTTPHeader> *_reqMap=nullptr);

  SwiftResult<void*>* swiftUpdateMetadata(
      std::vector<std::pair<std::string, std::string>> &_metaData,
      std::vector<HTTPHeader> *_reqMap=nullptr);

  SwiftResult<void*>* swiftDeleteMetadata(
      std::vector<std::string> &_metaDataKeys,
      std::vector<HTTPHeader> *_reqMap=nullptr);

  SwiftResult<void*>* swiftShowMetadata(bool _newest = false);

  Account* getAccount();
  std::string& getName();
  void setName(const std::string& name);
};

} /* namespace Swift */
#endif /* CONTAINER_H_ */
