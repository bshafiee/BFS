/**************************************************************************
    This is a general SDK for OpenStack Swift API written in C++
    Copyright (C) <2014>  <Behrooz Shafiee Sarjaz>
    This program comes with ABSOLUTELY NO WARRANTY;

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

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
  /**
   * Lists the objects under this container
   * @return
   *  A vector of Objects under this container
   */
  SwiftResult<std::vector<Object>*>* swiftGetObjects(bool _newest = false);

  /**
   * Similar to swiftGetObjects; however, only returns the name
   * of existing objects in this account.
   * @return
   *  A stream containing names of objects under this account
   *  in the specified format(json/xml)
   */
  SwiftResult<std::istream*>* swiftListObjects(HTTPHeader &_formatHeader = HEADER_FORMAT_APPLICATION_JSON,
      std::vector<HTTPHeader> *_uriParam = nullptr, bool _newest = false);

  /**
   * Creates this container on the Swift server
   * @return
   *  Noting.
   */
  SwiftResult<int*>* swiftCreateContainer(std::vector<HTTPHeader> *_reqMap=nullptr);

  /**
   * Deletes this container from Swift server
   * @return
   *  Noting.
   */
  SwiftResult<int*>* swiftDeleteContainer();

  /**
   * Adds metadata to this Container
   * @return
   *  Nothing
   * _metaData
   *  A vector of string pairs (key,value)
   */
  SwiftResult<int*>* swiftCreateMetadata(
      std::vector<std::pair<std::string, std::string>> &_metaData,
      std::vector<HTTPHeader> *_reqMap=nullptr);

  /**
   * Updates existing metadata for this Container
   * @return
   *  Nothing
   * _metaData
   *  A vector of string pairs (key,value)
   */
  SwiftResult<int*>* swiftUpdateMetadata(
      std::vector<std::pair<std::string, std::string>> &_metaData,
      std::vector<HTTPHeader> *_reqMap=nullptr);

  /**
   * Removes specified metadata (with key) from this container
   * @return
   *  Nothing
   * _metaDataKeys
   *  A vector containing keys of metadata which should be removed.
   */
  SwiftResult<int*>* swiftDeleteMetadata(
      std::vector<std::string> &_metaDataKeys,
      std::vector<HTTPHeader> *_reqMap=nullptr);

  /**
   * Gets the existing metadata for this container
   * @return
   *  Nothing. The payload is nullptr; however, the returned metadata are
   *  part of httpresponse. For example, getResponse()->write(cout);
   */
  SwiftResult<int*>* swiftShowMetadata(bool _newest = false);

  Account* getAccount();
  std::string& getName();
  void setName(const std::string& name);
};

} /* namespace Swift */
#endif /* CONTAINER_H_ */
