//---------------------------------------------------------------------------
// The XML DOM interfaces core uses, and C++Builder's DelphiInterface wrapper.
//
// Core reaches XML in two places: importing FileZilla sessions
// (SessionData.cpp) and parsing S3 responses (S3FileSystem.cpp). The surface is
// small -- ChildNodes, NodeName, Text, attributes, FindNode -- but it is parsed
// for real, using the expat that upstream already vendors, rather than stubbed.
// A stub here would silently import no sessions.
//
// _di_IXMLNode and friends are C++Builder's names for a reference-counted
// interface pointer, which is why they are spelled that way.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_XML_H
#define LINSCP_VCL_XML_H

#include <memory>
#include <type_traits>
#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_strings.h"
#include "vcl_unicodestring.h"

// C++Builder's interface pointer. Reference counting comes from shared_ptr
// rather than COM, which is all core needs: it only ever holds and dereferences
// these.
template <typename T>
class DelphiInterface
{
public:
  DelphiInterface() = default;
  DelphiInterface(std::nullptr_t) {}
  explicit DelphiInterface(std::shared_ptr<T> Ptr) : FPtr(std::move(Ptr)) {}

  // Accepts a pointer to a derived implementation, which is how the concrete
  // DOM classes hand themselves back through the interface.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  explicit DelphiInterface(std::shared_ptr<U> Ptr)
    : FPtr(std::static_pointer_cast<T>(std::move(Ptr))) {}

  T * operator->() const { return FPtr.get(); }
  T & operator*() const { return *FPtr; }
  T * get() const { return FPtr.get(); }

  explicit operator bool() const { return FPtr != nullptr; }

  // Core tests these against NULL, not nullptr.
  friend bool operator==(const DelphiInterface & I, std::nullptr_t)
    { return I.FPtr == nullptr; }
  friend bool operator!=(const DelphiInterface & I, std::nullptr_t)
    { return I.FPtr != nullptr; }
  friend bool operator==(std::nullptr_t, const DelphiInterface & I)
    { return I.FPtr == nullptr; }
  friend bool operator!=(std::nullptr_t, const DelphiInterface & I)
    { return I.FPtr != nullptr; }

private:
  std::shared_ptr<T> FPtr;
};

class IXMLNode;
class IXMLNodeList;
class IXMLDocument;

using _di_IXMLNode = DelphiInterface<IXMLNode>;
using _di_IXMLNodeList = DelphiInterface<IXMLNodeList>;
using _di_IXMLDocument = DelphiInterface<IXMLDocument>;

class IXMLNode
{
public:
  virtual ~IXMLNode() = default;

  virtual UnicodeString GetNodeName() = 0;
  virtual UnicodeString GetText() = 0;
  virtual _di_IXMLNodeList GetChildNodes() = 0;

  // Returns a null interface when absent, which is how core tests for it.
  virtual _di_IXMLNode ChildNode(const UnicodeString & Name) = 0;
  virtual bool HasAttribute(const UnicodeString & Name) = 0;
  virtual UnicodeString GetAttribute(const UnicodeString & Name) = 0;

  PROP_R_G(UnicodeString, NodeName, GetNodeName);
  PROP_R_G(UnicodeString, Text, GetText);
  PROP_R_G(_di_IXMLNodeList, ChildNodes, GetChildNodes);
};

class IXMLNodeList
{
public:
  virtual ~IXMLNodeList() = default;

  virtual int GetCount() = 0;
  virtual _di_IXMLNode Get(int Index) = 0;
  virtual _di_IXMLNode FindNode(const UnicodeString & Name) = 0;
  virtual int IndexOf(const UnicodeString & Name) = 0;

  PROP_R_G(int, Count, GetCount);
  PROP_IDX_R_G(_di_IXMLNode, Nodes, int, Get);
};

class IXMLDocument
{
public:
  virtual ~IXMLDocument() = default;

  // False when the file is missing or malformed, rather than raising: core
  // treats an unreadable import as "nothing to import".
  virtual bool LoadFromFile(const UnicodeString & FileName) = 0;
  virtual bool LoadFromXml(const UnicodeString & Xml) = 0;
  virtual _di_IXMLNode GetDocumentElement() = 0;

  PROP_R_G(_di_IXMLNode, DocumentElement, GetDocumentElement);
};

// Creates an empty document. Delphi spells this NewXMLDocument.
_di_IXMLDocument NewXMLDocument();

#endif // LINSCP_VCL_XML_H
