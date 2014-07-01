/*
 * Object.h
 *
 *  Created on: 2014-05-28
 *      Author: Behrooz Shafiee Sarjaz
 */

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
  SwiftResult<std::istream*>* swiftGetObjectContent(
      std::vector<HTTPHeader> *_uriParams = nullptr,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  SwiftResult<void*>* swiftCreateReplaceObject(const char* _data, ulong _size,
      bool _calculateETag = true, std::vector<HTTPHeader> *_uriParams = nullptr,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  /**
   * should not be bigger than 5GB
   * This is an especial function:
   * you should pass a outputStream pointer so you will have access to an output stream to write your data
   * this pointer is valid as long as you call httpclientsession->recveiveRespone() to receive actual response
   * the valid http response code is HTTP_CREATED(201).
   */
  SwiftResult<Poco::Net::HTTPClientSession*>* swiftCreateReplaceObject(
      std::ostream* &ouputStream, std::vector<HTTPHeader> *_uriParams = nullptr,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  SwiftResult<void*>* swiftCopyObject(const std::string &_dstObjectName,
      Container &_dstContainer, std::vector<HTTPHeader> *_reqMap = nullptr);

  SwiftResult<std::istream*>* swiftDeleteObject(
      bool _multipartManifest = false);

  SwiftResult<std::istream*>* swiftCreateMetadata(
      std::map<std::string, std::string> &_metaData,
      std::vector<HTTPHeader> *_reqMap = nullptr, bool _keepExistingMetadata =
          true);

  SwiftResult<std::istream*>* swiftUpdateMetadata(
      std::map<std::string, std::string> &_metaData,
      std::vector<HTTPHeader> *_reqMap = nullptr);

  SwiftResult<std::istream*>* swiftDeleteMetadata(
      std::vector<std::string> &_metaDataKeys);

  SwiftResult<void*>* swiftShowMetadata(std::vector<HTTPHeader>* _uriParams =
      nullptr, bool _newest = false);
};

} /* namespace Swift */
#endif /* OBJECT_H_ */
