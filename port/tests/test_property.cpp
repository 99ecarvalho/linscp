// Exercises every __property shape the transform emits, so the shim is verified
// against real usage rather than against the macro definitions alone.
#include "vcl_property.h"
#include <cassert>
#include <string>

namespace {

struct TStub {
  bool FActive = true;
  int FSpeed = 1;
  int FPort = 22;
  int FReadSide = 0, FWriteSide = 0;
  std::string FUser = "a";
  std::string FSlots[4];

  bool GetActive() const { return FActive; }
  int GetPort() const { return FPort; }
  void SetPort(int v) { FPort = v < 0 ? 0 : v; }      // validating setter
  void SetUser(std::string v) { FUser = v + "!"; }
  std::string GetSlot(int i) const { return FSlots[i]; }
  void PutSlot(int i, std::string v) { FSlots[i] = v; }
  int GetIndexed(int which) const { return which * 10; }

  PROP_R_F(bool, ActiveF, FActive);
  PROP_R_G(bool, ActiveG, GetActive);
  PROP_RW_FF(int, Speed, FSpeed);
  PROP_RW_F2(int, Sides, FReadSide, FWriteSide);
  PROP_RW_FS(std::string, User, FUser, SetUser);
  PROP_RW_GF(int, RawPort, GetPort, FPort);
  PROP_RW_GS(int, Port, GetPort, SetPort);
  PROP_W_S(std::string, UserOnly, SetUser);
  PROP_R_GI(int, Scaled, GetIndexed, 7);
  PROP_IDX_RW_GS(std::string, Slots, int, GetSlot, PutSlot);
};

// Properties must reach *private* accessors from outside the class.
//
// Borland's __property grants that; __declspec(property) does not, because it
// expands to an ordinary member call at the use site. Core relies on the
// Borland rule -- TBookmark declares public properties over private
// SetName/SetLocal/SetRemote -- so the shim routes every form through a
// wrapper declared alongside the property. Without that, these reads and
// writes fail to compile.
struct TPrivateAccessors
{
public:
  PROP_R_G(int, ReadOnly, GetValue);
  PROP_RW_GS(int, Both, GetValue, SetValue);
  PROP_RW_FS(int, FieldRead, FValue, SetValue);
  PROP_W_S(int, WriteOnly, SetValue);
  PROP_IDX_RW_GS(int, Items, int, GetItem, SetItem);

  int Raw() const { return FValue; }

private:
  int GetValue() const { return FValue; }
  void SetValue(int V) { FValue = V * 2; }
  int GetItem(int I) const { return FItems[I]; }
  void SetItem(int I, int V) { FItems[I] = V; }

  int FValue = 5;
  int FItems[4] = { 0, 0, 0, 0 };
};

// A const object must still be readable: Borland's properties have no
// const-correctness, and core's getters are frequently non-const because they
// genuinely mutate.
int ReadFromConst(const TPrivateAccessors & P)
{
  return P.ReadOnly + P.Both + P.FieldRead;
}

void TestPrivateAccessorsAreReachable()
{
  TPrivateAccessors P;
  assert(P.ReadOnly == 5);
  assert(P.Both == 5);
  assert(P.FieldRead == 5);

  P.Both = 10;
  assert(P.Raw() == 20);          // the private setter ran, doubling

  P.FieldRead = 3;
  assert(P.Raw() == 6);

  P.WriteOnly = 4;
  assert(P.Raw() == 8);

  P.Items[2] = 7;
  assert(P.Items[2] == 7);

  assert(ReadFromConst(P) == 8 * 3);
}

}  // namespace

int main() {
  TStub s;

  // read-only, field- and method-backed
  assert(s.ActiveF);
  assert(s.ActiveG);

  // field-backed read/write, including read-modify-write
  s.Speed = 7;
  assert(s.Speed == 7);
  s.Speed = s.Speed + 1;
  assert(s.Speed == 8);

  // two different backing fields
  s.Sides = 5;
  assert(s.FWriteSide == 5);
  assert(s.Sides == 0);          // reads FReadSide, which was never written

  // setter method must actually run
  s.User = "bob";
  assert(s.FUser == "bob!");
  s.UserOnly = "x";
  assert(s.FUser == "x!");

  // validating setter clamps
  s.Port = -5;
  assert(s.Port == 0);
  s.RawPort = 99;                // writes the field directly, bypassing clamp
  assert(s.RawPort == 99);

  // index-constant property passes its constant to the getter
  assert(s.Scaled == 70);

  TestPrivateAccessorsAreReachable();

  // indexed property, read and write
  s.Slots[2] = "hi";
  assert(s.Slots[2] == "hi");
  assert(s.FSlots[2] == "hi");

  return 0;
}
