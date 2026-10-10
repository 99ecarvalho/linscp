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

  // indexed property, read and write
  s.Slots[2] = "hi";
  assert(s.Slots[2] == "hi");
  assert(s.FSlots[2] == "hi");

  return 0;
}
