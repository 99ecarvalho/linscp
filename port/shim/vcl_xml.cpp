#include "vcl_xml.h"

#include <expat.h>

#include <map>
#include <string>

#include "vcl_bytestring.h"
#include "vcl_streams.h"
#include "vcl_sysutils.h"

namespace {

// A plain DOM node. Children are owned; the parent is not referenced, because
// core never walks upwards and a back-pointer would make the shared_ptr graph
// cyclic.
struct TNode
{
  UnicodeString Name;
  UnicodeString Text;
  std::map<UnicodeString, UnicodeString> Attributes;
  std::vector<std::shared_ptr<TNode>> Children;
};

UnicodeString FromUtf8(const char * S, int Len)
{
  if (S == nullptr || Len <= 0)
  {
    return UnicodeString();
  }
  return linscp::Utf8ToUtf16(S, static_cast<std::size_t>(Len));
}

// Returns the interface rather than the concrete type: the concrete class is
// defined below, and a conversion to the interface needs the complete type.
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

  int IndexOf(const UnicodeString & Name) override
  {
    for (std::size_t I = 0; I < FItems.size(); ++I)
    {
      // XML element names are case-sensitive, unlike most of the Delphi RTL.
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
  UnicodeString GetText() override { return FData->Text; }

  _di_IXMLNodeList GetChildNodes() override
  {
    return _di_IXMLNodeList(std::make_shared<TXmlNodeList>(FData->Children));
  }

  _di_IXMLNode ChildNode(const UnicodeString & Name) override
  {
    for (const auto & Child : FData->Children)
    {
      if (Child->Name == Name)
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

  UnicodeString GetAttribute(const UnicodeString & Name) override
  {
    const auto It = FData->Attributes.find(Name);
    return It == FData->Attributes.end() ? UnicodeString() : It->second;
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
  std::shared_ptr<TNode> Root;
  std::vector<TNode *> Stack;
};

void OnStart(void * UserData, const XML_Char * Name, const XML_Char ** Attrs)
{
  auto * State = static_cast<TParseState *>(UserData);
  auto Node = std::make_shared<TNode>();
  Node->Name = FromUtf8(Name, static_cast<int>(std::char_traits<char>::length(Name)));
  for (int I = 0; Attrs != nullptr && Attrs[I] != nullptr; I += 2)
  {
    if (Attrs[I + 1] == nullptr)
    {
      break;
    }
    Node->Attributes[FromUtf8(Attrs[I],
        static_cast<int>(std::char_traits<char>::length(Attrs[I])))] =
      FromUtf8(Attrs[I + 1],
        static_cast<int>(std::char_traits<char>::length(Attrs[I + 1])));
  }
  TNode * Raw = Node.get();
  if (State->Stack.empty())
  {
    State->Root = Node;
  }
  else
  {
    State->Stack.back()->Children.push_back(Node);
  }
  State->Stack.push_back(Raw);
}

void OnEnd(void * UserData, const XML_Char *)
{
  auto * State = static_cast<TParseState *>(UserData);
  if (!State->Stack.empty())
  {
    State->Stack.pop_back();
  }
}

void OnText(void * UserData, const XML_Char * S, int Len)
{
  auto * State = static_cast<TParseState *>(UserData);
  if (!State->Stack.empty())
  {
    // Appended rather than assigned: expat delivers character data in
    // arbitrary chunks, so a value split across callbacks must accumulate.
    State->Stack.back()->Text += FromUtf8(S, Len);
  }
}

class TXmlDocument : public IXMLDocument
{
public:
  bool LoadFromFile(const UnicodeString & FileName) override
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
    catch (const EStreamError &)
    {
      return false;
    }
    return ParseUtf8(Bytes);
  }

  bool LoadFromXml(const UnicodeString & Xml) override
  {
    const UTF8String Utf8(Xml);
    return ParseUtf8(std::string(Utf8.c_str(),
                                 static_cast<std::size_t>(Utf8.Length())));
  }

  _di_IXMLNode GetDocumentElement() override
  {
    return FRoot ? MakeNode(FRoot) : _di_IXMLNode();
  }

private:
  bool ParseUtf8(const std::string & Bytes)
  {
    FRoot.reset();
    XML_Parser Parser = XML_ParserCreate("UTF-8");
    if (Parser == nullptr)
    {
      return false;
    }
    TParseState State;
    XML_SetUserData(Parser, &State);
    XML_SetElementHandler(Parser, OnStart, OnEnd);
    XML_SetCharacterDataHandler(Parser, OnText);
    const XML_Status Status = XML_Parse(
      Parser, Bytes.empty() ? "" : Bytes.data(),
      static_cast<int>(Bytes.size()), XML_TRUE);
    XML_ParserFree(Parser);
    if (Status != XML_STATUS_OK)
    {
      return false;      // malformed input is a failed load, not an exception
    }
    FRoot = State.Root;
    return FRoot != nullptr;
  }

  std::shared_ptr<TNode> FRoot;
};

}  // namespace

_di_IXMLDocument NewXMLDocument()
{
  return _di_IXMLDocument(std::make_shared<TXmlDocument>());
}
