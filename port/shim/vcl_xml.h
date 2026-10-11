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
#include <string>
#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_set.h"
#include "vcl_strings.h"
#include "vcl_unicodestring.h"
#include "vcl_variant.h"

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

// Embarcadero's layout: the interfaces live in unit Xml.XMLIntf. Declaring them
// at global scope instead collided on Windows with the COM IXMLDocument that
// <msxml.h> declares there, reached through <objbase.h>.
namespace Xml {
namespace Xmlintf {

class IXMLNode;
class IXMLNodeList;
class IXMLDocument;

using _di_IXMLNode = DelphiInterface<IXMLNode>;
using _di_IXMLNodeList = DelphiInterface<IXMLNodeList>;
using _di_IXMLDocument = DelphiInterface<IXMLDocument>;

enum TNodeType
{
  ntReserved, ntElement, ntAttribute, ntText, ntCData, ntEntityRef, ntEntity,
  ntProcessingInstr, ntComment, ntDocument, ntDocType, ntDocFragment, ntNotation,
};

enum TParseOption { poResolveExternals, poValidateOnParse, poPreserveWhiteSpace, poAsyncLoad };
using TParseOptions = Set<TParseOption, poResolveExternals, poAsyncLoad>;

// Delphi's DOM node. Text and comment nodes are children like elements are, as
// MSXML provides them: FileZilla keeps a site's name as a text node mixed in
// with its element children, and core finds it by NodeType.
class IXMLNode
{
public:
  virtual ~IXMLNode() = default;

  virtual UnicodeString GetNodeName() = 0;     // "#text", "#comment" for those
  // The name without its prefix, and the namespace it resolves to through the
  // xmlns declarations in scope ("" for none).
  virtual UnicodeString GetLocalName() = 0;
  virtual UnicodeString GetNamespaceURI() = 0;
  virtual TNodeType GetNodeType() = 0;
  // A text or comment node's content. An element's is its text, as Text below.
  virtual Variant GetNodeValue() = 0;
  // The element's text children, concatenated. Delphi raises when an element
  // holds more than a single text node; this does not -- see the divergence
  // table.
  virtual UnicodeString GetText() = 0;
  virtual _di_IXMLNodeList GetChildNodes() = 0;

  // Returns a null interface when absent, which is how core tests for it.
  virtual _di_IXMLNode ChildNode(const UnicodeString & Name) = 0;
  virtual bool HasAttribute(const UnicodeString & Name) = 0;
  // Delphi's OleVariant: Null when the attribute is absent.
  virtual Variant GetAttribute(const UnicodeString & Name) = 0;

  PROP_R_G(UnicodeString, NodeName, GetNodeName);
  PROP_R_G(UnicodeString, LocalName, GetLocalName);
  PROP_R_G(UnicodeString, NamespaceURI, GetNamespaceURI);
  PROP_R_G(TNodeType, NodeType, GetNodeType);
  PROP_R_G(Variant, NodeValue, GetNodeValue);
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
  // Delphi's overload: by local name and namespace URI, which is how S3
  // finds elements under its xmlns="https://..." default namespace.
  virtual _di_IXMLNode FindNode(const UnicodeString & LocalName,
                                const UnicodeString & NamespaceURI) = 0;
  virtual int IndexOf(const UnicodeString & Name) = 0;

  PROP_R_G(int, Count, GetCount);
  PROP_IDX_R_G(_di_IXMLNode, Nodes, int, Get);
};

class IXMLDocument
{
public:
  virtual ~IXMLDocument() = default;

  // Delphi's: raise EDOMParseError on missing or malformed input. Core relies
  // on that -- ImportFromFilezilla has no handler, so the error reaches the
  // user rather than an import silently finding nothing.
  virtual void LoadFromFile(const UnicodeString & FileName) = 0;
  virtual void LoadFromXML(const UnicodeString & XML) = 0;
  virtual _di_IXMLNode GetDocumentElement() = 0;
  // The XML declaration, if any, then the root element.
  virtual _di_IXMLNodeList GetChildNodes() = 0;
  virtual TParseOptions GetParseOptions() = 0;
  virtual void SetParseOptions(TParseOptions Value) = 0;

  PROP_R_G(_di_IXMLNode, DocumentElement, GetDocumentElement);
  PROP_R_G(_di_IXMLNodeList, ChildNodes, GetChildNodes);
  PROP_RW_GS(TParseOptions, ParseOptions, GetParseOptions, SetParseOptions);
};

// Creates an empty document. Delphi spells this NewXMLDocument.
_di_IXMLDocument NewXMLDocument();

}  // namespace Xmlintf

namespace Xmldoc {

// The concrete document, used as core uses it:
//     interface_cast<Xmlintf::IXMLDocument>(new TXMLDocument(NULL))
// With no owner it is reference-counted, so the interface owns it.
class TXMLDocument : public Xmlintf::IXMLDocument
{
public:
  explicit TXMLDocument(TObject * AOwner = nullptr);
  ~TXMLDocument() override;

  void LoadFromFile(const UnicodeString & FileName) override;
  void LoadFromXML(const UnicodeString & XML) override;
  Xmlintf::_di_IXMLNode GetDocumentElement() override;
  Xmlintf::_di_IXMLNodeList GetChildNodes() override;
  Xmlintf::TParseOptions GetParseOptions() override { return FParseOptions; }
  void SetParseOptions(Xmlintf::TParseOptions Value) override { FParseOptions = Value; }

private:
  void Parse(const std::string & Utf8);

  struct TState;
  std::unique_ptr<TState> FState;
  Xmlintf::TParseOptions FParseOptions;
};

}  // namespace Xmldoc
}  // namespace Xml

LINSCP_DERIVED_EXCEPTION(EXMLDocError, Exception);
LINSCP_DERIVED_EXCEPTION(EDOMParseError, Exception);

// Takes ownership of a fresh implementation, as C++Builder's interface_cast
// does for an unowned object.
template <typename I, typename T>
DelphiInterface<I> interface_cast(T * Object)
{
  return DelphiInterface<I>(std::shared_ptr<I>(Object));
}

// Core spells the unit scope alone (Xmlintf::IXMLDocument), and names the
// interfaces only through their _di_ aliases. So those, and not the bare
// interface names, are made visible globally -- which keeps the global
// IXMLDocument free for the Windows COM one. The enumerators and
// TXMLDocument are used unqualified.
namespace Xmlintf = Xml::Xmlintf;
namespace Xmldoc = Xml::Xmldoc;
using Xml::Xmlintf::_di_IXMLNode;
using Xml::Xmlintf::_di_IXMLNodeList;
using Xml::Xmlintf::_di_IXMLDocument;
using Xml::Xmlintf::NewXMLDocument;
using Xml::Xmlintf::TNodeType;
using Xml::Xmlintf::ntElement;
using Xml::Xmlintf::ntText;
using Xml::Xmlintf::ntCData;
using Xml::Xmlintf::ntComment;
using Xml::Xmlintf::ntProcessingInstr;
using Xml::Xmlintf::TParseOption;
using Xml::Xmlintf::TParseOptions;
using Xml::Xmlintf::poPreserveWhiteSpace;
using Xml::Xmldoc::TXMLDocument;

#endif // LINSCP_VCL_XML_H
