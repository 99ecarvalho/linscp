#include "vcl_xml.h"

#include <expat.h>

#include <map>
#include <string>

#include "vcl_bytestring.h"
#include "vcl_streams.h"
#include "vcl_strutils.h"
#include "vcl_sysutils.h"

using Xml::Xmlintf::IXMLNode;
using Xml::Xmlintf::IXMLNodeList;
using Xml::Xmlintf::TNodeType;

namespace {

// A plain DOM node. Children are owned; the parent is not referenced, because
// core never walks upwards and a back-pointer would make the shared_ptr graph
// cyclic.
struct TNode
{
  TNodeType Type = Xml::Xmlintf::ntElement;
  UnicodeString Name;
  UnicodeString LocalName;
  UnicodeString NamespaceURI;
  UnicodeString Value;          // text, comment and declaration content
  std::map<UnicodeString, UnicodeString> Attributes;
  std::vector<std::shared_ptr<TNode>> Children;
};

UnicodeString FromUtf8(const char * S, std::size_t Len)
{
  return (S == nullptr || Len == 0) ? UnicodeString() : linscp::Utf8ToUtf16(S, Len);
}

UnicodeString FromUtf8(const char * S)
{
  return FromUtf8(S, (S == nullptr) ? 0 : std::char_traits<char>::length(S));
}

_di_IXMLNode MakeNode(const std::shared_ptr<TNode> & Data);

class TXmlNodeList : public IXMLNodeList
{
public:
  explicit TXmlNodeList(std::vector<std::shared_ptr<TNode>> Items)
    : FItems(std::move(Items)) {}

  int GetCount() override { return static_cast<int>(FItems.size()); }

  _di_IXMLNode Get(int Index) override
  {
    if (Index < 0 || Index >= GetCount())
    {
      return _di_IXMLNode();
    }
    return MakeNode(FItems[static_cast<std::size_t>(Index)]);
  }

  _di_IXMLNode FindNode(const UnicodeString & Name) override
  {
    const int I = IndexOf(Name);
    return I < 0 ? _di_IXMLNode() : Get(I);
  }

  _di_IXMLNode FindNode(const UnicodeString & LocalName,
                        const UnicodeString & NamespaceURI) override
  {
    for (std::size_t I = 0; I < FItems.size(); ++I)
    {
      if ((FItems[I]->Type == Xml::Xmlintf::ntElement) &&
          (FItems[I]->LocalName == LocalName) && (FItems[I]->NamespaceURI == NamespaceURI))
      {
        return Get(static_cast<int>(I));
      }
    }
    return _di_IXMLNode();
  }

  int IndexOf(const UnicodeString & Name) override
  {
    for (std::size_t I = 0; I < FItems.size(); ++I)
    {
      // Names are case-sensitive in XML; text nodes are "#text" and so
      // never match an element name.
      if (FItems[I]->Name == Name)
      {
        return static_cast<int>(I);
      }
    }
    return -1;
  }

private:
  std::vector<std::shared_ptr<TNode>> FItems;
};

class TXmlNode : public IXMLNode
{
public:
  explicit TXmlNode(std::shared_ptr<TNode> Data) : FData(std::move(Data)) {}

  UnicodeString GetNodeName() override { return FData->Name; }
  UnicodeString GetLocalName() override
    { return (FData->Type == Xml::Xmlintf::ntElement) ? FData->LocalName : FData->Name; }
  UnicodeString GetNamespaceURI() override { return FData->NamespaceURI; }
  TNodeType GetNodeType() override { return FData->Type; }

  Variant GetNodeValue() override
  {
    if (FData->Type == Xml::Xmlintf::ntElement)
    {
      return Variant(GetText());
    }
    return Variant(FData->Value);
  }

  UnicodeString GetText() override
  {
    if (FData->Type != Xml::Xmlintf::ntElement)
    {
      return FData->Value;
    }
    UnicodeString Result;
    for (const auto & Child : FData->Children)
    {
      if ((Child->Type == Xml::Xmlintf::ntText) || (Child->Type == Xml::Xmlintf::ntCData))
      {
        Result += Child->Value;
      }
    }
    return Result;
  }

  _di_IXMLNodeList GetChildNodes() override
  {
    return _di_IXMLNodeList(std::make_shared<TXmlNodeList>(FData->Children));
  }

  _di_IXMLNode ChildNode(const UnicodeString & Name) override
  {
    for (const auto & Child : FData->Children)
    {
      if ((Child->Type == Xml::Xmlintf::ntElement) && (Child->Name == Name))
      {
        return MakeNode(Child);
      }
    }
    return _di_IXMLNode();
  }

  bool HasAttribute(const UnicodeString & Name) override
  {
    return FData->Attributes.find(Name) != FData->Attributes.end();
  }

  Variant GetAttribute(const UnicodeString & Name) override
  {
    const auto It = FData->Attributes.find(Name);
    return (It == FData->Attributes.end()) ? Null() : Variant(It->second);
  }

private:
  std::shared_ptr<TNode> FData;
};

_di_IXMLNode MakeNode(const std::shared_ptr<TNode> & Data)
{
  return _di_IXMLNode(std::make_shared<TXmlNode>(Data));
}

//---------------------------------------------------------------------------
// expat glue
//---------------------------------------------------------------------------
struct TParseState
{
  bool PreserveWhiteSpace = false;
  std::vector<std::shared_ptr<TNode>> Top;     // declaration, root
  std::vector<TNode *> Stack;
  std::string PendingText;                     // UTF-8, across chunks
  // Namespace bindings in scope, one frame per open element; "" is the
  // default namespace.
  std::vector<std::map<UnicodeString, UnicodeString>> Scopes;

  UnicodeString Resolve(const UnicodeString & Prefix) const
  {
    for (auto It = Scopes.rbegin(); It != Scopes.rend(); ++It)
    {
      const auto Found = It->find(Prefix);
      if (Found != It->end())
      {
        return Found->second;
      }
    }
    return UnicodeString();
  }

  void Add(std::shared_ptr<TNode> Node)
  {
    if (Stack.empty())
    {
      Top.push_back(std::move(Node));
    }
    else
    {
      Stack.back()->Children.push_back(std::move(Node));
    }
  }

  // Character data arrives in arbitrary chunks and must accumulate into one
  // node. Whitespace-only text is dropped unless asked to keep it, as
  // Delphi's default ParseOptions do.
  void FlushText()
  {
    if (PendingText.empty() || Stack.empty())
    {
      PendingText.clear();
      return;
    }
    const bool Blank = PendingText.find_first_not_of(" \t\r\n") == std::string::npos;
    if (PreserveWhiteSpace || !Blank)
    {
      auto Node = std::make_shared<TNode>();
      Node->Type = Xml::Xmlintf::ntText;
      Node->Name = L"#text";
      Node->Value = FromUtf8(PendingText.data(), PendingText.size());
      Add(std::move(Node));
    }
    PendingText.clear();
  }
};

void OnStart(void * UserData, const XML_Char * Name, const XML_Char ** Attrs)
{
  auto * State = static_cast<TParseState *>(UserData);
  State->FlushText();
  auto Node = std::make_shared<TNode>();
  Node->Name = FromUtf8(Name);
  std::map<UnicodeString, UnicodeString> Bindings;
  for (int I = 0; (Attrs != nullptr) && (Attrs[I] != nullptr) && (Attrs[I + 1] != nullptr); I += 2)
  {
    const UnicodeString AttrName = FromUtf8(Attrs[I]);
    const UnicodeString AttrValue = FromUtf8(Attrs[I + 1]);
    Node->Attributes[AttrName] = AttrValue;
    if (AttrName == L"xmlns")
    {
      Bindings[UnicodeString()] = AttrValue;
    }
    else if (AttrName.SubString(1, 6) == L"xmlns:")
    {
      Bindings[AttrName.SubString(7)] = AttrValue;
    }
  }
  State->Scopes.push_back(std::move(Bindings));
  const int Colon = Node->Name.Pos(UnicodeString(L":"));
  const UnicodeString Prefix = (Colon > 0) ? Node->Name.SubString(1, Colon - 1) : UnicodeString();
  Node->LocalName = (Colon > 0) ? Node->Name.SubString(Colon + 1) : Node->Name;
  Node->NamespaceURI = State->Resolve(Prefix);
  TNode * Raw = Node.get();
  State->Add(std::move(Node));
  State->Stack.push_back(Raw);
}

void OnEnd(void * UserData, const XML_Char *)
{
  auto * State = static_cast<TParseState *>(UserData);
  State->FlushText();
  if (!State->Stack.empty())
  {
    State->Stack.pop_back();
  }
  if (!State->Scopes.empty())
  {
    State->Scopes.pop_back();
  }
}

void OnText(void * UserData, const XML_Char * S, int Len)
{
  static_cast<TParseState *>(UserData)->PendingText.append(S, static_cast<std::size_t>(Len));
}

void OnComment(void * UserData, const XML_Char * Data)
{
  auto * State = static_cast<TParseState *>(UserData);
  State->FlushText();
  auto Node = std::make_shared<TNode>();
  Node->Type = Xml::Xmlintf::ntComment;
  Node->Name = L"#comment";
  Node->Value = FromUtf8(Data);
  State->Add(std::move(Node));
}

void OnXmlDecl(void * UserData, const XML_Char * Version, const XML_Char * Encoding, int)
{
  auto * State = static_cast<TParseState *>(UserData);
  auto Node = std::make_shared<TNode>();
  Node->Type = Xml::Xmlintf::ntProcessingInstr;
  Node->Name = L"xml";
  Node->Value = UnicodeString(L"version=\"") + FromUtf8(Version) + L"\"";
  if (Encoding != nullptr)
  {
    Node->Value += UnicodeString(L" encoding=\"") + FromUtf8(Encoding) + L"\"";
  }
  State->Add(std::move(Node));
}

}  // namespace

struct Xml::Xmldoc::TXMLDocument::TState
{
  std::vector<std::shared_ptr<TNode>> Top;
};

Xml::Xmldoc::TXMLDocument::TXMLDocument(TObject *) : FState(new TState()) {}
Xml::Xmldoc::TXMLDocument::~TXMLDocument() = default;

void Xml::Xmldoc::TXMLDocument::LoadFromFile(const UnicodeString & FileName)
{
  std::string Bytes;
  try
  {
    TFileStream F(FileName, fmOpenRead);
    const __int64 Length = F.Size;
    if (Length > 0)
    {
      Bytes.resize(static_cast<std::size_t>(Length));
      F.ReadBuffer(&Bytes[0], static_cast<int>(Length));
    }
  }
  catch (const EStreamError & E)
  {
    throw EDOMParseError(E.Message);
  }
  Parse(Bytes);
}

void Xml::Xmldoc::TXMLDocument::LoadFromXML(const UnicodeString & XML)
{
  const UTF8String Utf8(XML);
  Parse(std::string(Utf8.c_str(), static_cast<std::size_t>(Utf8.Length())));
}

void Xml::Xmldoc::TXMLDocument::Parse(const std::string & Utf8)
{
  FState->Top.clear();
  XML_Parser Parser = XML_ParserCreate(nullptr);   // honour the declared encoding
  if (Parser == nullptr)
  {
    throw EDOMParseError(UnicodeString(L"Cannot create XML parser"));
  }
  TParseState State;
  State.PreserveWhiteSpace = FParseOptions.Contains(Xml::Xmlintf::poPreserveWhiteSpace);
  XML_SetUserData(Parser, &State);
  XML_SetElementHandler(Parser, OnStart, OnEnd);
  XML_SetCharacterDataHandler(Parser, OnText);
  XML_SetCommentHandler(Parser, OnComment);
  XML_SetXmlDeclHandler(Parser, OnXmlDecl);
  const XML_Status Status = XML_Parse(
    Parser, Utf8.empty() ? "" : Utf8.data(), static_cast<int>(Utf8.size()), XML_TRUE);
  UnicodeString Error;
  if (Status != XML_STATUS_OK)
  {
    Error = FromUtf8(XML_ErrorString(XML_GetErrorCode(Parser))) + L" at line " +
            IntToStr(static_cast<__int64>(XML_GetCurrentLineNumber(Parser)));
  }
  XML_ParserFree(Parser);
  if (!Error.IsEmpty())
  {
    throw EDOMParseError(Error);
  }
  FState->Top = std::move(State.Top);
}

_di_IXMLNode Xml::Xmldoc::TXMLDocument::GetDocumentElement()
{
  for (const auto & Node : FState->Top)
  {
    if (Node->Type == Xml::Xmlintf::ntElement)
    {
      return MakeNode(Node);
    }
  }
  return _di_IXMLNode();
}

_di_IXMLNodeList Xml::Xmldoc::TXMLDocument::GetChildNodes()
{
  return _di_IXMLNodeList(std::make_shared<TXmlNodeList>(FState->Top));
}

_di_IXMLDocument Xml::Xmlintf::NewXMLDocument()
{
  return interface_cast<IXMLDocument>(new Xml::Xmldoc::TXMLDocument(nullptr));
}
