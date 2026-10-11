// TStrings / TStringList tests.
//
// The first test exists because this shim contains two different index bases:
// TStrings is 0-based, UnicodeString is 1-based. Confusing them corrupts data
// silently, so the boundary is pinned explicitly.
#include "vcl_strings.h"

#include <cstdio>

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

void TestIndexingIsZeroBased()
{
  TStringList L;
  L.Add(U(L"first"));
  L.Add(U(L"second"));
  CHECK(L.Count == 2);
  CHECK(L.Strings[0] == U(L"first"));      // TStrings: 0 is the first item
  CHECK(L.Strings[1] == U(L"second"));

  // ... while the strings it holds are themselves 1-based. Taken into a local
  // because clang reads Strings[0][1] as a two-argument getter call (see the
  // limitation note in vcl_property.h).
  const UnicodeString First = L.Strings[0];
  CHECK(First[1] == L'f');
  CHECK(First[5] == L't');

  bool Raised = false;
  try { (void)L.Strings[2]; } catch (const EStringListError &) { Raised = true; }
  CHECK(Raised);

  Raised = false;
  try { (void)L.Strings[-1]; } catch (const EStringListError &) { Raised = true; }
  CHECK(Raised);
}

void TestAddInsertDelete()
{
  TStringList L;
  CHECK(L.Add(U(L"b")) == 0);              // Add returns the new index
  CHECK(L.Add(U(L"c")) == 1);
  L.Insert(0, U(L"a"));
  CHECK(L.Count == 3);
  CHECK(L.Strings[0] == U(L"a") && L.Strings[2] == U(L"c"));

  L.Insert(3, U(L"d"));                    // inserting at Count appends
  CHECK(L.Strings[3] == U(L"d"));

  L.Delete(0);
  CHECK(L.Count == 3 && L.Strings[0] == U(L"b"));
  L.Clear();
  CHECK(L.Count == 0);

  bool Raised = false;
  try { L.Insert(1, U(L"x")); } catch (const EStringListError &) { Raised = true; }
  CHECK(Raised);                           // index past Count is invalid
}

void TestIndexOfIsCaseInsensitiveByDefault()
{
  // Delphi's CaseSensitive defaults to False. Core depends on this; flipping
  // the default would silently change lookup behaviour everywhere.
  TStringList L;
  L.Add(U(L"Alpha"));
  CHECK(!L.CaseSensitive);
  CHECK(L.IndexOf(U(L"alpha")) == 0);
  CHECK(L.IndexOf(U(L"ALPHA")) == 0);
  CHECK(L.IndexOf(U(L"beta")) == -1);      // absent yields -1

  L.CaseSensitive = true;
  CHECK(L.IndexOf(U(L"alpha")) == -1);
  CHECK(L.IndexOf(U(L"Alpha")) == 0);
}

void TestNamesAndValues()
{
  TStringList L;
  L.Add(U(L"Host=example.com"));
  L.Add(U(L"Port=22"));
  L.Add(U(L"Bare"));
  L.Add(U(L"Empty="));

  CHECK(L.Names[0] == U(L"Host"));
  CHECK(L.ValueFromIndex[0] == U(L"example.com"));
  CHECK(L.Values[U(L"Host")] == U(L"example.com"));
  CHECK(L.Values[U(L"Port")] == U(L"22"));
  CHECK(L.Values[U(L"host")] == U(L"example.com"));   // case-insensitive

  // No separator at all: no name, and no value.
  CHECK(L.Names[2].IsEmpty());
  CHECK(L.ValueFromIndex[2].IsEmpty());

  // Separator present but nothing after it.
  CHECK(L.Names[3] == U(L"Empty"));
  CHECK(L.ValueFromIndex[3].IsEmpty());

  CHECK(L.Values[U(L"Missing")].IsEmpty());
  CHECK(L.IndexOfName(U(L"Port")) == 1);
  CHECK(L.IndexOfName(U(L"Nope")) == -1);

  L.Values[U(L"Port")] = U(L"2222");                  // updates in place
  CHECK(L.Values[U(L"Port")] == U(L"2222"));
  CHECK(L.Count == 4);

  L.Values[U(L"New")] = U(L"v");                      // appends when absent
  CHECK(L.Count == 5);

  L.Values[U(L"Port")] = U(L"");                      // clearing removes it
  CHECK(L.Count == 4);
  CHECK(L.IndexOfName(U(L"Port")) == -1);

  TStringList P;
  P.AddPair(U(L"k"), U(L"v"));
  CHECK(P.Strings[0] == U(L"k=v"));

  L.NameValueSeparator = L':';
  TStringList C;
  C.NameValueSeparator = L':';
  C.Add(U(L"a:b"));
  CHECK(C.Names[0] == U(L"a") && C.ValueFromIndex[0] == U(L"b"));
}

void TestText()
{
  TStringList L;
  L.Add(U(L"one"));
  L.Add(U(L"two"));
  // Delphi appends a line break after every line, including the last.
  CHECK(L.Text == U(L"one\r\ntwo\r\n"));

  TStringList R;
  R.Text = U(L"a\r\nb\r\n");
  CHECK(R.Count == 2 && R.Strings[0] == U(L"a") && R.Strings[1] == U(L"b"));

  R.Text = U(L"a\nb");                 // bare LF, no trailing break
  CHECK(R.Count == 2 && R.Strings[1] == U(L"b"));

  R.Text = U(L"a\r\n\r\nb\r\n");       // a blank line is preserved
  CHECK(R.Count == 3 && R.Strings[1].IsEmpty());

  R.Text = U(L"");
  CHECK(R.Count == 0);

  // Round trip
  TStringList A;
  A.Add(U(L"x"));
  A.Add(U(L""));
  A.Add(U(L"z"));
  TStringList B;
  B.Text = A.Text;
  CHECK(B.Equals(&A));
}

void TestCommaText()
{
  TStringList L;
  L.Add(U(L"plain"));
  L.Add(U(L"has space"));
  L.Add(U(L"has,comma"));
  L.Add(U(L"has\"quote"));
  L.Add(U(L""));

  const UnicodeString CT = L.CommaText;
  // Items needing it are quoted; embedded quotes are doubled.
  CHECK(CT == U(L"plain,\"has space\",\"has,comma\",\"has\"\"quote\",\"\""));

  TStringList R;
  R.CommaText = CT;
  CHECK(R.Count == 5);
  CHECK(R.Strings[0] == U(L"plain"));
  CHECK(R.Strings[1] == U(L"has space"));
  CHECK(R.Strings[2] == U(L"has,comma"));
  CHECK(R.Strings[3] == U(L"has\"quote"));
  CHECK(R.Strings[4].IsEmpty());
  CHECK(R.Equals(&L));                  // full round trip

  TStringList S;
  S.CommaText = U(L"a,b,c");
  CHECK(S.Count == 3 && S.Strings[2] == U(L"c"));

  // CommaText ignores a custom Delimiter, unlike DelimitedText.
  TStringList D;
  D.Add(U(L"a"));
  D.Add(U(L"b"));
  D.Delimiter = L'|';
  CHECK(D.DelimitedText == U(L"a|b"));
  CHECK(D.CommaText == U(L"a,b"));
}

void TestSortedAndDuplicates()
{
  TStringList L;
  L.Sorted = true;
  L.Add(U(L"c"));
  L.Add(U(L"a"));
  L.Add(U(L"b"));
  CHECK(L.Strings[0] == U(L"a"));
  CHECK(L.Strings[1] == U(L"b"));
  CHECK(L.Strings[2] == U(L"c"));
  CHECK(L.IndexOf(U(L"b")) == 1);       // binary search path
  CHECK(L.IndexOf(U(L"zz")) == -1);

  // dupIgnore is Delphi's default: the duplicate is dropped.
  CHECK(L.Duplicates == dupIgnore);
  L.Add(U(L"b"));
  CHECK(L.Count == 3);

  L.Duplicates = dupAccept;
  L.Add(U(L"b"));
  CHECK(L.Count == 4);

  L.Duplicates = dupError;
  bool Raised = false;
  try { L.Add(U(L"b")); } catch (const EStringListError &) { Raised = true; }
  CHECK(Raised);

  // Setting Sorted on an existing list sorts it.
  TStringList M;
  M.Add(U(L"z"));
  M.Add(U(L"y"));
  CHECK(M.Strings[0] == U(L"z"));
  M.Sorted = true;
  CHECK(M.Strings[0] == U(L"y"));

  // Sort() without Sorted leaves insertion order free to change afterwards.
  TStringList N;
  N.Add(U(L"b"));
  N.Add(U(L"A"));
  N.Sort();
  CHECK(N.Strings[0] == U(L"A"));       // case-insensitive ordering by default
}

void TestObjectsAreReferencedNotOwned()
{
  // Delphi's TStrings does not free the objects it points at, and core relies
  // on owning them elsewhere. A destructor that freed them would be a
  // double-free, so this verifies the object survives the list.
  static int Destroyed = 0;
  struct TTracked : public TObject
  {
    ~TTracked() override { ++Destroyed; }
  };
  TTracked Obj;
  {
    TStringList L;
    L.AddObject(U(L"k"), &Obj);
    CHECK(L.Objects[0] == &Obj);
    CHECK(L.IndexOfObject(&Obj) == 0);
    L.InsertObject(0, U(L"j"), nullptr);
    CHECK(L.Objects[1] == &Obj);
  }
  CHECK(Destroyed == 0);
}

void TestAssignMoveExchange()
{
  TStringList A;
  A.Add(U(L"1"));
  A.Add(U(L"2"));
  A.Delimiter = L';';
  A.CaseSensitive = true;

  TStringList B;
  B.Assign(&A);
  CHECK(B.Equals(&A));
  CHECK(B.Delimiter == L';');        // settings carry over, as in Delphi
  CHECK(B.CaseSensitive);

  TStringList C;
  C.Add(U(L"a"));
  C.Add(U(L"b"));
  C.Add(U(L"c"));
  C.Move(0, 2);
  CHECK(C.Strings[0] == U(L"b") && C.Strings[2] == U(L"a"));
  C.Exchange(0, 2);
  CHECK(C.Strings[0] == U(L"a") && C.Strings[2] == U(L"b"));

  TStringList D;
  D.Add(U(L"x"));
  D.AddStrings(&C);
  CHECK(D.Count == 4 && D.Strings[0] == U(L"x"));

  CHECK(!D.Equals(&C));
  CHECK(!D.Equals(nullptr));
}

void TestUpdateCounterIsBalanced()
{
  TStringList L;
  L.BeginUpdate();
  L.BeginUpdate();
  L.Add(U(L"a"));
  L.EndUpdate();
  L.EndUpdate();
  L.EndUpdate();                     // unbalanced must not underflow
  L.Add(U(L"b"));
  CHECK(L.Count == 2);
}

}  // namespace

int main()
{
  TestIndexingIsZeroBased();
  TestAddInsertDelete();
  TestIndexOfIsCaseInsensitiveByDefault();
  TestNamesAndValues();
  TestText();
  TestCommaText();
  TestSortedAndDuplicates();
  TestObjectsAreReferencedNotOwned();
  TestAssignMoveExchange();
  TestUpdateCounterIsBalanced();

  if (Failures == 0)
  {
    std::printf("all TStrings tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
