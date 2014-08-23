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

#ifndef OBJECT_H_
#define OBJECT_H_

#include <Container.h>
#include <Poco/HashMap.h>
#include <Poco/Net/HTTPClientSession.h>

namespace Swift {

class Object {
  Container* container;
  std::string name;
  long length;
  std::string content_type;
  std::string hash;
  std::string last_modified;
public:
  Object(Container* _container, std::string _name = "", long _length = -1,
      std::string _content_type = "", std::string _hash = "",
      std::string _last_modified = "");
  virtual ~Object();

  std::vector<std::pair<std::string, std::string> >* getExistingMetaData();
  Container* getContainer();
  void setContainer(Container* container);
  std::string getName();
  void setName(const std::string& name);
  std::string getContentType();
  void setContentType(const std::string& _contentType);
  std::string getHash();
  void setHash(const std::string& _hash);
  std::string getLastModified();
  void setLastModified(const std::string& _lastModified);
  long getLength();
  void setLength(long _length);

  /** API Functions **/

  /**
   * Returns content of this Object
   * @return
   *  An stream containing content of this object.
   */
  SwiftResult<std::istream*>* swiftGetObjectContent(
      std::vector<HTTPHeader> *_uriParams = nullptr,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  /**
   * Creates or replace this object (if already exist)
   * @return
   *  Nothing.
   * _data
   *  A pointer to the data which should be written to this object.
   * _size
   *  size of input data
   * _calculateETag
   *  Whether to calculate ETag for this object or not; it's highly
   *  recommended to do so because it'll check the integrity of object
   *  on the server.
   */
  SwiftResult<int*>* swiftCreateReplaceObject(const char* _data, ulong _size,
      bool _calculateETag = true, std::vector<HTTPHeader> *_uriParams = nullptr,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  /**
   * In order to create a variable length object you need to use this fucntion.
   * You need to pass a pointer to an outputstream then you can use that to write
   * as much as data you want in this object; however, there is a max limit of
   * 5GB for each object.
   * How to use this function:
   * you should pass a outputStream pointer so you will have access to an output stream to write your data
   * this pointer is valid as long as you call httpclientsession->recveiveRespone() to receive actual response
   * the valid http response code is HTTP_CREATED(201).
   * Here is an example to use this function:
   * ostream *outStream = nullptr;
   * SwiftResult<HTTPClientSession*> *chunkedResult = chucnkedObject.swiftCreateReplaceObject(outStream);
   *
   * (*outStream)<<mydata;
   *
   * HTTPResponse response;
   * chunkedResult->getPayload()->receiveResponse(response);
   * if(response.getStatus() == HTTP_CREATED)
   * success.
   *
   * @return
   *   A pointer to the httpsession to the Swift server so you can send your request after you are
   *   done with writing your content to this object.
   */
  SwiftResult<Poco::Net::HTTPClientSession*>* swiftCreateReplaceObject(
      std::ostream* &ouputStream, std::vector<HTTPHeader> *_uriParams = nullptr,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  /**
   * Makes a copy of this object to another object on the server.
   * @return
   *  Nothing.
   * _dstObjectname
   *  Name of destination object.
   * _dstContainer
   *  Name of destination Container.
   */
  SwiftResult<int*>* swiftCopyObject(const std::string &_dstObjectName,
      Container &_dstContainer, std::vector<HTTPHeader> *_reqMap = nullptr);

  /**
   * Deletes this object form server.
   * _multipartManifest
   *  Used for multipart objects(not implemented yet)
   */
  SwiftResult<std::istream*>* swiftDeleteObject(
      bool _multipartManifest = false);
  /**
   * Adds metadata to this Object
   * @return
   *  Nothing
   * _metaData
   *  A vector of string pairs (key,value)
   */
  SwiftResult<std::istream*>* swiftCreateMetadata(
      std::map<std::string, std::string> &_metaData,
      std::vector<HTTPHeader> *_reqMap = nullptr, bool _keepExistingMetadata =
          true);
  /**
   * Updates existing metadata for this Object
   * @return
   *  Nothing
   * _metaData
   *  A vector of string pairs (key,value)
   */
  SwiftResult<std::istream*>* swiftUpdateMetadata(
      std::map<std::string, std::string> &_metaData,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  /**
   * Removes specified metadata (with key) from this object
   * @return
   *  Nothing
   * _metaDataKeys
   *  A vector containing keys of metadata which should be removed.
   */
  SwiftResult<std::istream*>* swiftDeleteMetadata(
      std::vector<std::string> &_metaDataKeys);
  /**
   * Gets the existing metadata for this object
   * @return
   *  Nothing. The payload is nullptr; however, the returned metadata are
   *  part of httpresponse. For example, getResponse()->write(cout);
   */
  SwiftResult<int*>* swiftShowMetadata(std::vector<HTTPHeader>* _uriParams =
      nullptr, bool _newest = false);
};

} /* namespace Swift */
#endif /* OBJECT_H_ */
