//---------------------------------------------------------------------------
// TStream and its concrete descendants.
//
// Core derives TSafeHandleStream from THandleStream (28 uses), so the base
// classes must be inheritable with Delphi's virtual surface intact.
//
// Two conventions matter and are easy to get subtly wrong:
//
//   Read and Write return how many bytes actually moved, which may be fewer
//   than asked. ReadBuffer and WriteBuffer are the all-or-nothing versions and
//   raise EReadError / EWriteError. Core uses both, and swapping them would turn
//   a short read into silent truncation rather than an error.
//
//   Seek exists twice. Delphi kept a legacy 32-bit overload taking a Word
//   origin (soFromBeginning = 0) alongside the modern 64-bit one taking a
//   TSeekOrigin (soBeginning = 0). Core uses both spellings, so both are here.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_STREAMS_H
#define LINSCP_VCL_STREAMS_H

#include <cstddef>
#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

// Delphi's handle type.
using THandle = std::uintptr_t;
constexpr THandle InvalidHandleValue = static_cast<THandle>(-1);

enum TSeekOrigin { soBeginning, soCurrent, soEnd };

// The legacy Word-based origins. Same values, different type, both in use.
enum : Word { soFromBeginning = 0, soFromCurrent = 1, soFromEnd = 2 };

// File open and share modes, at their Delphi values.
enum : Word
{
  fmCreate = 0xFF00,
  fmOpenRead = 0x0000,
  fmOpenWrite = 0x0001,
  fmOpenReadWrite = 0x0002,
  fmShareCompat = 0x0000,
  fmShareExclusive = 0x0010,
  fmShareDenyWrite = 0x0020,
  fmShareDenyRead = 0x0030,
  fmShareDenyNone = 0x0040,
};

class TStream : public TObject
{
public:
  // --- implemented by descendants -----------------------------------------
  virtual int Read(void * Buffer, int Count) = 0;
  virtual int Write(const void * Buffer, int Count) = 0;
  virtual __int64 Seek(const __int64 Offset, TSeekOrigin Origin) = 0;

  // Legacy overload. Delphi routes it to the modern one, and so do we.
  virtual int Seek(int Offset, Word Origin);

  // --- all-or-nothing transfers -------------------------------------------
  void ReadBuffer(void * Buffer, int Count);
  void WriteBuffer(const void * Buffer, int Count);

  // --- position and size ---------------------------------------------------
  __int64 GetPosition();
  void SetPosition(__int64 Pos);
  PROP_RW_GS(__int64, Position, GetPosition, SetPosition);

  virtual __int64 GetSize();
  virtual void SetSize(const __int64 NewSize);
  PROP_RW_GS(__int64, Size, GetSize, SetSize);

  __int64 CopyFrom(TStream * Source, __int64 Count);
};

//---------------------------------------------------------------------------
// A stream over an OS handle. On POSIX the handle carries a file descriptor.
//---------------------------------------------------------------------------
class THandleStream : public TStream
{
public:
  explicit THandleStream(THandle AHandle) : FHandle(AHandle) {}

  int Read(void * Buffer, int Count) override;
  int Write(const void * Buffer, int Count) override;
  __int64 Seek(const __int64 Offset, TSeekOrigin Origin) override;
  void SetSize(const __int64 NewSize) override;

  // Overriding one Seek hides the base's other overload. Core calls both on
  // derived streams, so the legacy one is re-exposed here rather than silently
  // disappearing.
  using TStream::Seek;

  PROP_R_F(THandle, Handle, FHandle);

  // Give up the handle without closing it. Used by the handle-based file API,
  // which wraps a borrowed handle in a temporary stream; without this, the
  // temporary's destructor would close a handle the caller still owns.
  void DetachHandle() { FHandle = InvalidHandleValue; }

protected:
  THandle FHandle = InvalidHandleValue;
};

class TFileStream : public THandleStream
{
public:
  TFileStream(const UnicodeString & FileName, Word Mode);
  ~TFileStream() override;

  PROP_R_F(UnicodeString, FileName, FFileName);

private:
  UnicodeString FFileName;
};

//---------------------------------------------------------------------------
// An in-memory stream. Size and Position are independent: writing past the end
// grows the buffer, and SetSize can truncate below Position, which Delphi
// then clamps.
//---------------------------------------------------------------------------
class TMemoryStream : public TStream
{
public:
  int Read(void * Buffer, int Count) override;
  int Write(const void * Buffer, int Count) override;
  __int64 Seek(const __int64 Offset, TSeekOrigin Origin) override;
  __int64 GetSize() override;
  void SetSize(const __int64 NewSize) override;

  // Overriding one Seek hides the base's other overload. Core calls both on
  // derived streams, so the legacy one is re-exposed here rather than silently
  // disappearing.
  using TStream::Seek;

  void Clear();
  void SaveToStream(TStream * Stream);
  void LoadFromStream(TStream * Stream);

  // Delphi exposes this as a property named Memory, and as a writable Pointer:
  // core does `(char *)FMemory->Memory` and then writes through it, so a
  // const pointer or a method named Memory would both break at the call site.
  void * GetMemory() const
    { return const_cast<void *>(static_cast<const void *>(FData.data())); }
  PROP_R_G(void *, Memory, GetMemory);

private:
  std::vector<unsigned char> FData;
  __int64 FPosition = 0;
};

#endif // LINSCP_VCL_STREAMS_H
