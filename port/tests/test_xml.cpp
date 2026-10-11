// The XML DOM, parsed by the expat that upstream vendors.
//
// Core uses this to import FileZilla sessions and to read S3 responses, so the
// fixtures below are shaped like those rather than being minimal XML. A stub
// here would silently import no sessions, which is why this is parsed for real
// and tested against text that looks like the real thing.
#if defined(_WIN32)
  // The COM IXMLDocument at global scope, which ours must coexist with.
  #include <windows.h>
  #include <objbase.h>
  #include <msxml.h>
#endif
#include "vcl_xml.h"

#include <cstdio>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

// Delphi's LoadFromXML raises on malformed input; the tests want a verdict.
bool Load(const _di_IXMLDocument & Doc, const UnicodeString & Xml)
{
  try
  {
    Doc->LoadFromXML(Xml);
    return true;
  }
  catch (const EDOMParseError &)
  {
    return false;
  }
}

UnicodeString U(const wchar_t * S) { return UnicodeString(S); }

const char * const FileZillaLike =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<FileZilla3 version=\"3.66.4\">\n"
  "  <Servers>\n"
  "    <Server>\n"
  "      <Host>example.com</Host>\n"
  "      <Port>22</Port>\n"
  "      <Protocol>1</Protocol>\n"
  "      <User>alice</User>\n"
  "      <Name>My Site</Name>\n"
  "    </Server>\n"
  "    <Server>\n"
  "      <Host>other.example</Host>\n"
  "      <Port>2222</Port>\n"
  "      <Name>Second</Name>\n"
  "    </Server>\n"
  "  </Servers>\n"
  "</FileZilla3>\n";

void TestStructure()
{
  _di_IXMLDocument Doc = NewXMLDocument();
  CHECK(Doc != NULL);
  CHECK(Load(Doc, UnicodeString(FileZillaLike)));

  _di_IXMLNode Root = Doc->DocumentElement;
  CHECK(Root != NULL);
  CHECK(Root->NodeName == U(L"FileZilla3"));
  CHECK(Root->GetAttribute(U(L"version")) == U(L"3.66.4"));
  CHECK(Root->HasAttribute(U(L"version")));
  CHECK(!Root->HasAttribute(U(L"missing")));
  CHECK(Root->GetAttribute(U(L"missing")).IsNull());   // Delphi: Null, not empty

  _di_IXMLNode Servers = Root->ChildNode(U(L"Servers"));
  CHECK(Servers != NULL);

  _di_IXMLNodeList List = Servers->ChildNodes;
  CHECK(List != NULL);
  CHECK(List->Count == 2);

  _di_IXMLNode First = List->Nodes[0];
  CHECK(First != NULL);
  CHECK(First->NodeName == U(L"Server"));
  CHECK(First->ChildNode(U(L"Host"))->Text == U(L"example.com"));
  CHECK(First->ChildNode(U(L"Port"))->Text == U(L"22"));
  CHECK(First->ChildNode(U(L"Name"))->Text == U(L"My Site"));

  _di_IXMLNode Second = List->Nodes[1];
  CHECK(Second->ChildNode(U(L"Host"))->Text == U(L"other.example"));

  // An absent child yields a null interface, which is how core tests for it.
  CHECK(First->ChildNode(U(L"Nonexistent")) == NULL);

  // Out-of-range indices must not crash.
  CHECK(List->Nodes[2] == NULL);
  CHECK(List->Nodes[-1] == NULL);

  CHECK(List->FindNode(U(L"Server")) != NULL);
  CHECK(List->FindNode(U(L"Nope")) == NULL);
  CHECK(List->IndexOf(U(L"Server")) == 0);
  CHECK(List->IndexOf(U(L"Nope")) == -1);
}

void TestCaseSensitivityAndEntities()
{
  _di_IXMLDocument Doc = NewXMLDocument();
  CHECK(Load(Doc, U(L"<Root><Child>a &amp; b &lt;c&gt;</Child>"
                           L"<child>lower</child></Root>")));
  _di_IXMLNode Root = Doc->DocumentElement;

  // Entities must be decoded, or a password containing '&' would import wrong.
  CHECK(Root->ChildNode(U(L"Child"))->Text == U(L"a & b <c>"));

  // Element names are case-sensitive, unlike most of the Delphi RTL.
  CHECK(Root->ChildNode(U(L"child"))->Text == U(L"lower"));
  CHECK(Root->ChildNode(U(L"CHILD")) == NULL);
}

void TestTextSpanningChunks()
{
  // expat delivers character data in arbitrary chunks, and splits at an entity
  // for certain. Text must accumulate rather than be overwritten.
  _di_IXMLDocument Doc = NewXMLDocument();
  CHECK(Load(Doc, U(L"<R><T>before&amp;after</T></R>")));
  CHECK(Doc->DocumentElement->ChildNode(U(L"T"))->Text ==
        U(L"before&after"));

  // A long value, to exercise more than one buffer.
  UnicodeString Long;
  for (int I = 0; I < 500; ++I)
  {
    Long += U(L"0123456789");
  }
  _di_IXMLDocument Big = NewXMLDocument();
  CHECK(Load(Big, U(L"<R><T>") + Long + U(L"</T></R>")));
  CHECK(Big->DocumentElement->ChildNode(U(L"T"))->Text.Length() ==
        Long.Length());
}

void TestNonAsciiRoundTrip()
{
  // Session names and paths carry non-ASCII routinely.
  _di_IXMLDocument Doc = NewXMLDocument();
  CHECK(Load(Doc, U(L"<R><N>café über</N></R>")));
  CHECK(Doc->DocumentElement->ChildNode(U(L"N"))->Text ==
        U(L"café über"));
}

void TestMalformedAndMissing()
{
  // A failed load must report false and leave no document element, rather
  // than raising: core treats an unreadable import as nothing to import.
  _di_IXMLDocument Bad = NewXMLDocument();
  CHECK(!Load(Bad, U(L"<Root><Unclosed></Root>")));
  CHECK(Bad->DocumentElement == NULL);

  _di_IXMLDocument Empty = NewXMLDocument();
  CHECK(!Load(Empty, U(L"")));
  CHECK(Empty->DocumentElement == NULL);

  _di_IXMLDocument Junk = NewXMLDocument();
  CHECK(!Load(Junk, U(L"not xml at all")));

  _di_IXMLDocument Missing = NewXMLDocument();
  // A missing file raises, as Delphi's does; core's FileZilla import relies
  // on the error reaching the user.
  bool Raised = false;
  try { Missing->LoadFromFile(U(L"/definitely/not/here.xml")); }
  catch (const EDOMParseError &) { Raised = true; }
  CHECK(Raised);
  CHECK(Missing->DocumentElement == NULL);
}

void TestLoadFromFile()
{
  const fs::path Dir = fs::temp_directory_path() / "linscp_xml_test";
  fs::remove_all(Dir);
  fs::create_directories(Dir);
  const fs::path Path = Dir / "sites.xml";
  {
    std::ofstream F(Path, std::ios::binary);
    F << FileZillaLike;
  }

  UnicodeString Wide;
  for (char C : Path.string())
  {
    Wide += static_cast<wchar_t>(static_cast<unsigned char>(C));
  }

  _di_IXMLDocument Doc = NewXMLDocument();
  Doc->LoadFromFile(Wide);
  CHECK(Doc->DocumentElement->NodeName == U(L"FileZilla3"));
  CHECK(Doc->DocumentElement->ChildNode(U(L"Servers"))->ChildNodes->Count == 2);

  std::error_code Ec;
  fs::remove_all(Dir, Ec);
}

// FileZilla 3 keeps a site's display name as a text node mixed in with the
// site's element children; SessionData.cpp finds it by NodeType.
void TestMixedContentTextNodes()
{
  const _di_IXMLDocument Doc = interface_cast<Xmlintf::IXMLDocument>(new TXMLDocument(NULL));
  Doc->LoadFromXML(U(L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     L"<FileZilla3><Servers>\n"
                     L"  <Server>\n"
                     L"    <Host>example.com</Host>\n"
                     L"    <!-- a comment -->\n"
                     L"    My Site\n"
                     L"  </Server>\n"
                     L"</Servers></FileZilla3>"));
  // Document-level children: the declaration, then the root.
  CHECK(Doc->ChildNodes->FindNode(L"FileZilla3") != NULL);
  CHECK(Doc->ChildNodes->Get(0)->NodeType == ntProcessingInstr);
  const _di_IXMLNode Server =
    Doc->ChildNodes->FindNode(L"FileZilla3")->ChildNodes->FindNode(L"Servers")
       ->ChildNodes->FindNode(L"Server");
  // Whitespace-only text is dropped by default, so: Host, comment, text.
  CHECK(Server->ChildNodes->Count == 3);
  CHECK(Server->ChildNodes->Get(0)->NodeType == ntElement);
  CHECK(Server->ChildNodes->Get(1)->NodeType == ntComment);
  const _di_IXMLNode Text = Server->ChildNodes->Get(2);
  CHECK(Text->NodeType == ntText);
  const UnicodeString Name = Text->NodeValue;      // as SessionData.cpp reads it
  CHECK(Name.Trim() == U(L"My Site"));
  // FindNode by name ignores text and comment nodes.
  CHECK(Server->ChildNodes->FindNode(L"Host")->Text == U(L"example.com"));
}

void TestPreserveWhiteSpace()
{
  const _di_IXMLDocument Doc = interface_cast<Xmlintf::IXMLDocument>(new TXMLDocument(NULL));
  CHECK(Doc->ParseOptions == TParseOptions());
  Doc->ParseOptions = TParseOptions() << poPreserveWhiteSpace;   // S3's tag parsing
  Doc->LoadFromXML(U(L"<R> <A>x</A> </R>"));
  CHECK(Doc->DocumentElement->ChildNodes->Count == 3);         // kept
  CHECK(Doc->DocumentElement->ChildNodes->Get(0)->NodeType == ntText);

  const _di_IXMLDocument Plain = NewXMLDocument();
  Plain->LoadFromXML(U(L"<R> <A>x</A> </R>"));
  CHECK(Plain->DocumentElement->ChildNodes->Count == 1);       // dropped
}

void TestAttributeVariant()
{
  // SessionData.cpp: OleVariant V = Node->GetAttribute(...); if (!V.IsNull())
  const _di_IXMLDocument Doc = NewXMLDocument();
  Doc->LoadFromXML(U(L"<Pass encoding=\"base64\">c2VjcmV0</Pass>"));
  const OleVariant Encoding = Doc->DocumentElement->GetAttribute(L"encoding");
  CHECK(!Encoding.IsNull());
  CHECK(Encoding == U(L"base64"));
  const OleVariant Absent = Doc->DocumentElement->GetAttribute(L"other");
  CHECK(Absent.IsNull());
  CHECK(!(Absent == U(L"")));                     // Null equals nothing
  bool Raised = false;
  try { const UnicodeString S = Absent; (void)S; }
  catch (const EVariantTypeCastError &) { Raised = true; }
  CHECK(Raised);                                  // Delphi's NullStrictConvert
}

}  // namespace

// Core names the unit scope alone, as SessionData.cpp does with
// interface_cast<Xmlintf::IXMLDocument>; and the _di_ aliases stay global.
static_assert(std::is_same<_di_IXMLDocument,
                           DelphiInterface<Xmlintf::IXMLDocument>>::value, "");
static_assert(std::is_same<_di_IXMLNode,
                           DelphiInterface<Xml::Xmlintf::IXMLNode>>::value, "");
#if defined(_WIN32)
static_assert(!std::is_same<::IXMLDocument, Xmlintf::IXMLDocument>::value,
              "both exist, distinct, in one translation unit");
#endif

int main()
{
  TestStructure();
  TestCaseSensitivityAndEntities();
  TestTextSpanningChunks();
  TestNonAsciiRoundTrip();
  TestMalformedAndMissing();
  TestLoadFromFile();

  TestMixedContentTextNodes();
  TestPreserveWhiteSpace();
  TestAttributeVariant();

  if (Failures == 0)
  {
    std::printf("all XML tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
