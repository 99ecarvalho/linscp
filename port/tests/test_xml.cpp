// The XML DOM, parsed by the expat that upstream vendors.
//
// Core uses this to import FileZilla sessions and to read S3 responses, so the
// fixtures below are shaped like those rather than being minimal XML. A stub
// here would silently import no sessions, which is why this is parsed for real
// and tested against text that looks like the real thing.
#include "vcl_xml.h"

#include <cstdio>
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
  CHECK(Doc->LoadFromXml(UnicodeString(FileZillaLike)));

  _di_IXMLNode Root = Doc->DocumentElement;
  CHECK(Root != NULL);
  CHECK(Root->NodeName == U(L"FileZilla3"));
  CHECK(Root->GetAttribute(U(L"version")) == U(L"3.66.4"));
  CHECK(Root->HasAttribute(U(L"version")));
  CHECK(!Root->HasAttribute(U(L"missing")));
  CHECK(Root->GetAttribute(U(L"missing")).IsEmpty());

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
  CHECK(Doc->LoadFromXml(U(L"<Root><Child>a &amp; b &lt;c&gt;</Child>"
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
  CHECK(Doc->LoadFromXml(U(L"<R><T>before&amp;after</T></R>")));
  CHECK(Doc->DocumentElement->ChildNode(U(L"T"))->Text ==
        U(L"before&after"));

  // A long value, to exercise more than one buffer.
  UnicodeString Long;
  for (int I = 0; I < 500; ++I)
  {
    Long += U(L"0123456789");
  }
  _di_IXMLDocument Big = NewXMLDocument();
  CHECK(Big->LoadFromXml(U(L"<R><T>") + Long + U(L"</T></R>")));
  CHECK(Big->DocumentElement->ChildNode(U(L"T"))->Text.Length() ==
        Long.Length());
}

void TestNonAsciiRoundTrip()
{
  // Session names and paths carry non-ASCII routinely.
  _di_IXMLDocument Doc = NewXMLDocument();
  CHECK(Doc->LoadFromXml(U(L"<R><N>café über</N></R>")));
  CHECK(Doc->DocumentElement->ChildNode(U(L"N"))->Text ==
        U(L"café über"));
}

void TestMalformedAndMissing()
{
  // A failed load must report false and leave no document element, rather
  // than raising: core treats an unreadable import as nothing to import.
  _di_IXMLDocument Bad = NewXMLDocument();
  CHECK(!Bad->LoadFromXml(U(L"<Root><Unclosed></Root>")));
  CHECK(Bad->DocumentElement == NULL);

  _di_IXMLDocument Empty = NewXMLDocument();
  CHECK(!Empty->LoadFromXml(U(L"")));
  CHECK(Empty->DocumentElement == NULL);

  _di_IXMLDocument Junk = NewXMLDocument();
  CHECK(!Junk->LoadFromXml(U(L"not xml at all")));

  _di_IXMLDocument Missing = NewXMLDocument();
  CHECK(!Missing->LoadFromFile(U(L"/definitely/not/here.xml")));
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
  CHECK(Doc->LoadFromFile(Wide));
  CHECK(Doc->DocumentElement->NodeName == U(L"FileZilla3"));
  CHECK(Doc->DocumentElement->ChildNode(U(L"Servers"))->ChildNodes->Count == 2);

  std::error_code Ec;
  fs::remove_all(Dir, Ec);
}

}  // namespace

int main()
{
  TestStructure();
  TestCaseSensitivityAndEntities();
  TestTextSpanningChunks();
  TestNonAsciiRoundTrip();
  TestMalformedAndMissing();
  TestLoadFromFile();

  if (Failures == 0)
  {
    std::printf("all XML tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
