// TList, TObjectList and Delphi sets.
//
// The Notify hook gets the most attention: core overrides it, and both when it
// fires and what it reports are load-bearing. TObjectList frees owned objects
// from inside Notify, so a wrong Action means either a leak or a double free --
// neither of which announces itself.
#include "vcl_object.h"
#include "vcl_containers.h"
#include "vcl_set.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

int Ints[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
void * P(int I) { return &Ints[I]; }

void TestListBasics()
{
  TList L;
  CHECK(L.Count == 0);
  CHECK(L.Add(P(0)) == 0);              // Add returns the new index
  CHECK(L.Add(P(1)) == 1);
  CHECK(L.Count == 2);
  CHECK(L.Items[0] == P(0));            // 0-based
  CHECK(L.Items[1] == P(1));

  L.Insert(0, P(2));
  CHECK(L.Items[0] == P(2) && L.Count == 3);
  L.Insert(3, P(3));                    // inserting at Count appends
  CHECK(L.Items[3] == P(3));

  CHECK(L.IndexOf(P(1)) == 2);
  CHECK(L.IndexOf(P(7)) == -1);         // absent yields -1
  CHECK(L.First() == P(2));
  CHECK(L.Last() == P(3));

  CHECK(L.Remove(P(2)) == 0);
  CHECK(L.Count == 3);
  CHECK(L.Remove(P(7)) == -1);          // removing an absent item is a no-op

  L.Items[0] = P(5);
  CHECK(L.Items[0] == P(5));

  bool Raised = false;
  try { (void)L.Items[99]; } catch (const EListError &) { Raised = true; }
  CHECK(Raised);
  Raised = false;
  try { L.Insert(99, P(0)); } catch (const EListError &) { Raised = true; }
  CHECK(Raised);

  L.Clear();
  CHECK(L.Count == 0);
}

void TestMoveAndExchange()
{
  TList L;
  L.Add(P(0));
  L.Add(P(1));
  L.Add(P(2));
  L.Move(0, 2);
  CHECK(L.Items[0] == P(1) && L.Items[2] == P(0));
  L.Exchange(0, 2);
  CHECK(L.Items[0] == P(0) && L.Items[2] == P(1));
  L.Move(1, 1);                         // no-op
  CHECK(L.Items[1] == P(2));
}

// A list that records every Notify call, the way core's subclasses observe it.
class TWatchingList : public TList
{
public:
  std::vector<std::pair<void *, TListNotification>> Seen;

protected:
  void Notify(void * Ptr, TListNotification Action) override
  {
    Seen.emplace_back(Ptr, Action);
    TList::Notify(Ptr, Action);
  }
};

void TestNotifyFiresCorrectly()
{
  TWatchingList L;
  L.Add(P(0));
  CHECK(L.Seen.size() == 1);
  CHECK(L.Seen[0].first == P(0) && L.Seen[0].second == lnAdded);

  L.Insert(0, P(1));
  CHECK(L.Seen.size() == 2 && L.Seen[1].second == lnAdded);

  L.Delete(0);
  CHECK(L.Seen.size() == 3);
  CHECK(L.Seen[2].first == P(1) && L.Seen[2].second == lnDeleted);

  L.Remove(P(0));
  CHECK(L.Seen.size() == 4 && L.Seen[3].second == lnDeleted);

  // Extract reports lnExtracted, not lnDeleted: the caller is taking
  // ownership, so an owning list must not free it.
  L.Add(P(2));
  L.Seen.clear();
  CHECK(L.Extract(P(2)) == P(2));
  CHECK(L.Seen.size() == 1 && L.Seen[0].second == lnExtracted);
  CHECK(L.Count == 0);
  CHECK(L.Extract(P(7)) == nullptr);

  // Clear notifies once per item.
  L.Seen.clear();
  L.Add(P(0));
  L.Add(P(1));
  L.Seen.clear();
  L.Clear();
  CHECK(L.Seen.size() == 2);
  for (const auto & E : L.Seen)
  {
    CHECK(E.second == lnDeleted);
  }

  // Move and Exchange must not notify: nothing entered or left the list.
  L.Add(P(0));
  L.Add(P(1));
  L.Seen.clear();
  L.Move(0, 1);
  L.Exchange(0, 1);
  CHECK(L.Seen.empty());
}

int Destroyed = 0;
struct TTracked : public TObject
{
  ~TTracked() override { ++Destroyed; }
};

void TestObjectListOwnership()
{
  // Owned by default, and freed on removal and on destruction.
  Destroyed = 0;
  {
    TObjectList L;
    CHECK(L.OwnsObjects);
    L.Add(new TTracked());
    L.Add(new TTracked());
    CHECK(L.Count == 2);
    L.Delete(0);
    CHECK(Destroyed == 1);
  }
  CHECK(Destroyed == 2);                // the destructor freed the rest

  // Not owned: the list must leave them alone.
  Destroyed = 0;
  {
    TTracked A;
    TTracked B;
    TObjectList L(false);
    CHECK(!L.OwnsObjects);
    L.Add(&A);
    L.Add(&B);
    L.Delete(0);
    CHECK(Destroyed == 0);
  }
  CHECK(Destroyed == 2);                // freed by going out of scope, not by L

  // Extract hands ownership back even when the list owns its items, so the
  // object must survive and be ours to delete.
  Destroyed = 0;
  {
    TObjectList L;
    auto * T = new TTracked();
    L.Add(T);
    CHECK(L.Extract(T) == T);
    CHECK(Destroyed == 0);
    CHECK(L.Count == 0);
    delete T;
    CHECK(Destroyed == 1);
  }
  CHECK(Destroyed == 1);                // not freed twice

  // Clear frees owned items.
  Destroyed = 0;
  {
    TObjectList L;
    L.Add(new TTracked());
    L.Add(new TTracked());
    L.Clear();
    CHECK(Destroyed == 2);
    CHECK(L.Count == 0);
  }
  CHECK(Destroyed == 2);
}

void TestObjectListTypedAccess()
{
  TObjectList L(false);
  TTracked A;
  L.Add(&A);
  CHECK(L.Objects[0] == &A);
  CHECK(L.IndexOf(&A) == 0);
  CHECK(L.Remove(&A) == 0);
}

int CompareInts(void * A, void * B)
{
  const int X = *static_cast<int *>(A);
  const int Y = *static_cast<int *>(B);
  return X < Y ? -1 : (X > Y ? 1 : 0);
}

int CompareDescending(void * A, void * B)
{
  return -CompareInts(A, B);
}

void TestSort()
{
  // Core sorts through this with its own comparison proc (NamedObjectSortProc).
  TList L;
  L.Add(P(3));
  L.Add(P(1));
  L.Add(P(2));
  L.Sort(CompareInts);
  CHECK(L.Items[0] == P(1));
  CHECK(L.Items[1] == P(2));
  CHECK(L.Items[2] == P(3));

  L.Sort(CompareDescending);
  CHECK(L.Items[0] == P(3));
  CHECK(L.Items[2] == P(1));

  L.Sort(nullptr);                      // must not crash or reorder
  CHECK(L.Items[0] == P(3));

  TList Empty;
  Empty.Sort(CompareInts);
  CHECK(Empty.Count == 0);

  // Sorting must not notify: nothing entered or left the list.
  TWatchingList W;
  W.Add(P(2));
  W.Add(P(1));
  W.Seen.clear();
  W.Sort(CompareInts);
  CHECK(W.Seen.empty());
  CHECK(W.Items[0] == P(1));
}

void TestSets()
{
  TShiftState S;
  CHECK(S.Empty());
  CHECK(!S.Contains(ssShift));

  S << ssShift;
  CHECK(!S.Empty());
  CHECK(S.Contains(ssShift));
  CHECK(!S.Contains(ssAlt));

  S << ssAlt << ssCtrl;                 // chaining, as in Delphi
  CHECK(S.Contains(ssAlt) && S.Contains(ssCtrl));

  S >> ssAlt;
  CHECK(!S.Contains(ssAlt));
  CHECK(S.Contains(ssShift));

  TShiftState A;
  A << ssShift;
  TShiftState B;
  B << ssCtrl;

  const TShiftState Union = A + B;
  CHECK(Union.Contains(ssShift) && Union.Contains(ssCtrl));

  const TShiftState Diff = Union - A;
  CHECK(!Diff.Contains(ssShift) && Diff.Contains(ssCtrl));

  const TShiftState Inter = Union * A;
  CHECK(Inter.Contains(ssShift) && !Inter.Contains(ssCtrl));

  TShiftState C;
  C << ssShift;
  CHECK(C == A);
  CHECK(C != B);

  C.Clear();
  CHECK(C.Empty());

  // The last element of the range must be usable, which is where an
  // off-by-one in the bitset size would show up.
  TShiftState Edge;
  Edge << ssHorizontal;
  CHECK(Edge.Contains(ssHorizontal));
}

struct TTerminalLike : TObject { int Id = 0; };

void TestObjectListItemsAreTObject()
{
  TObjectList L;
  TTerminalLike * T = new TTerminalLike();
  T->Id = 42;
  L.Add(T);
  // TTerminalList::GetTerminal's expression.
  TTerminalLike * Back = dynamic_cast<TTerminalLike *>(L.Items[0]);
  CHECK(Back == T);
  CHECK(Back->Id == 42);
}

}  // namespace

int main()
{
  TestListBasics();
  TestMoveAndExchange();
  TestNotifyFiresCorrectly();
  TestObjectListOwnership();
  TestObjectListTypedAccess();
  TestSort();
  TestSets();

  TestObjectListItemsAreTObject();

  if (Failures == 0)
  {
    std::printf("all container/set tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
