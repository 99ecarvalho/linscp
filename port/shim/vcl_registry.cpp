#include "vcl_registry.h"

#include <vector>

#include "vcl_bytestring.h"
#include "vcl_strutils.h"

#if defined(_WIN32)

namespace {

// Delphi strips a leading backslash and opens relative to RootKey.
UnicodeString Relative(const UnicodeString & Key)
{
  if (!Key.IsEmpty() && (Key[1] == L'\\' || Key[1] == L'/'))
  {
    return Key.SubString(2);
  }
  return Key;
}

}  // namespace

TRegistry::~TRegistry()
{
  CloseKey();
}

bool TRegistry::OpenKey(const UnicodeString & Key, bool CanCreate)
{
  const UnicodeString Path = Relative(Key);
  HKEY Opened = nullptr;
  LONG Result;
  if (CanCreate)
  {
    DWORD Disposition = 0;
    Result = RegCreateKeyExW(FRootKey, Path.c_str(), 0, nullptr,
                             REG_OPTION_NON_VOLATILE,
                             static_cast<REGSAM>(FAccess), nullptr, &Opened,
                             &Disposition);
  }
  else
  {
    Result = RegOpenKeyExW(FRootKey, Path.c_str(), 0,
                           static_cast<REGSAM>(FAccess), &Opened);
  }
  if (Result != ERROR_SUCCESS)
  {
    return false;
  }
  CloseKey();
  FCurrentKey = Opened;
  FCurrentPath = Path;
  return true;
}

bool TRegistry::OpenKeyReadOnly(const UnicodeString & Key)
{
  const unsigned Saved = FAccess;
  FAccess = KEY_READ;
  const bool Result = OpenKey(Key, false);
  FAccess = Saved;
  return Result;
}

void TRegistry::CloseKey()
{
  if (FCurrentKey != nullptr)
  {
    RegCloseKey(FCurrentKey);
    FCurrentKey = nullptr;
    FCurrentPath = UnicodeString();
  }
}

bool TRegistry::CreateKey(const UnicodeString & Key)
{
  HKEY Created = nullptr;
  DWORD Disposition = 0;
  const LONG Result = RegCreateKeyExW(
    FRootKey, Relative(Key).c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
    static_cast<REGSAM>(FAccess), nullptr, &Created, &Disposition);
  if (Result != ERROR_SUCCESS)
  {
    return false;
  }
  RegCloseKey(Created);
  return true;
}

bool TRegistry::DeleteKey(const UnicodeString & Key)
{
  return RegDeleteKeyW(FRootKey, Relative(Key).c_str()) == ERROR_SUCCESS;
}

bool TRegistry::DeleteValue(const UnicodeString & Name)
{
  return FCurrentKey != nullptr &&
         RegDeleteValueW(FCurrentKey, Name.c_str()) == ERROR_SUCCESS;
}

bool TRegistry::KeyExists(const UnicodeString & Key)
{
  HKEY Opened = nullptr;
  if (RegOpenKeyExW(FRootKey, Relative(Key).c_str(), 0, KEY_READ,
                    &Opened) != ERROR_SUCCESS)
  {
    return false;
  }
  RegCloseKey(Opened);
  return true;
}

bool TRegistry::ValueExists(const UnicodeString & Name)
{
  return FCurrentKey != nullptr &&
         RegQueryValueExW(FCurrentKey, Name.c_str(), nullptr, nullptr, nullptr,
                          nullptr) == ERROR_SUCCESS;
}

bool TRegistry::HasSubKeys()
{
  wchar_t Name[256];
  DWORD Length = static_cast<DWORD>(sizeof(Name) / sizeof(Name[0]));
  return (FCurrentKey != nullptr) &&
    (RegEnumKeyExW(FCurrentKey, 0, Name, &Length, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
}

void TRegistry::GetKeyNames(TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  Strings->Clear();
  if (FCurrentKey == nullptr)
  {
    return;
  }
  wchar_t Name[256];
  for (DWORD I = 0;; ++I)
  {
    DWORD Length = static_cast<DWORD>(sizeof(Name) / sizeof(Name[0]));
    if (RegEnumKeyExW(FCurrentKey, I, Name, &Length, nullptr, nullptr, nullptr,
                      nullptr) != ERROR_SUCCESS)
    {
      break;
    }
    Strings->Add(UnicodeString(Name, static_cast<int>(Length)));
  }
}

void TRegistry::GetValueNames(TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  Strings->Clear();
  if (FCurrentKey == nullptr)
  {
    return;
  }
  wchar_t Name[256];
  for (DWORD I = 0;; ++I)
  {
    DWORD Length = static_cast<DWORD>(sizeof(Name) / sizeof(Name[0]));
    if (RegEnumValueW(FCurrentKey, I, Name, &Length, nullptr, nullptr, nullptr,
                      nullptr) != ERROR_SUCCESS)
    {
      break;
    }
    Strings->Add(UnicodeString(Name, static_cast<int>(Length)));
  }
}

int TRegistry::GetDataSize(const UnicodeString & Name)
{
  DWORD Size = 0;
  if (FCurrentKey == nullptr ||
      RegQueryValueExW(FCurrentKey, Name.c_str(), nullptr, nullptr, nullptr,
                       &Size) != ERROR_SUCCESS)
  {
    return -1;
  }
  return static_cast<int>(Size);
}

TRegDataType TRegistry::GetDataType(const UnicodeString & Name)
{
  DWORD Type = 0;
  if (FCurrentKey == nullptr ||
      RegQueryValueExW(FCurrentKey, Name.c_str(), nullptr, &Type, nullptr,
                       nullptr) != ERROR_SUCCESS)
  {
    return rdUnknown;
  }
  switch (Type)
  {
    case REG_SZ:        return rdString;
    case REG_EXPAND_SZ: return rdExpandString;
    case REG_DWORD:     return rdInteger;
    case REG_BINARY:    return rdBinary;
    default:            return rdUnknown;
  }
}

UnicodeString TRegistry::ReadString(const UnicodeString & Name)
{
  const int Size = GetDataSize(Name);
  if (Size <= 0)
  {
    return UnicodeString();
  }
  std::vector<wchar_t> Buffer(static_cast<std::size_t>(Size) / sizeof(wchar_t) + 1, L'\0');
  DWORD Length = static_cast<DWORD>(Size);
  if (RegQueryValueExW(FCurrentKey, Name.c_str(), nullptr, nullptr,
                       reinterpret_cast<LPBYTE>(Buffer.data()),
                       &Length) != ERROR_SUCCESS)
  {
    return UnicodeString();
  }
  return UnicodeString(Buffer.data());
}

void TRegistry::WriteString(const UnicodeString & Name,
                            const UnicodeString & Value)
{
  if (FCurrentKey != nullptr)
  {
    RegSetValueExW(FCurrentKey, Name.c_str(), 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(Value.c_str()),
                   static_cast<DWORD>((Value.Length() + 1) * sizeof(wchar_t)));
  }
}

int TRegistry::ReadInteger(const UnicodeString & Name)
{
  DWORD Value = 0;
  DWORD Length = sizeof(Value);
  if (FCurrentKey == nullptr ||
      RegQueryValueExW(FCurrentKey, Name.c_str(), nullptr, nullptr,
                       reinterpret_cast<LPBYTE>(&Value),
                       &Length) != ERROR_SUCCESS)
  {
    return 0;
  }
  return static_cast<int>(Value);
}

void TRegistry::WriteInteger(const UnicodeString & Name, int Value)
{
  if (FCurrentKey != nullptr)
  {
    const DWORD V = static_cast<DWORD>(Value);
    RegSetValueExW(FCurrentKey, Name.c_str(), 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&V), sizeof(V));
  }
}

int TRegistry::ReadBinaryData(const UnicodeString & Name, void * Buffer,
                              int BufSize)
{
  DWORD Length = static_cast<DWORD>(BufSize);
  if (FCurrentKey == nullptr || Buffer == nullptr || BufSize <= 0 ||
      RegQueryValueExW(FCurrentKey, Name.c_str(), nullptr, nullptr,
                       static_cast<LPBYTE>(Buffer), &Length) != ERROR_SUCCESS)
  {
    return 0;
  }
  return static_cast<int>(Length);
}

void TRegistry::WriteBinaryData(const UnicodeString & Name, const void * Buffer,
                                int BufSize)
{
  if (FCurrentKey != nullptr && Buffer != nullptr && BufSize >= 0)
  {
    RegSetValueExW(FCurrentKey, Name.c_str(), 0, REG_BINARY,
                   static_cast<const BYTE *>(Buffer),
                   static_cast<DWORD>(BufSize));
  }
}

#else   // POSIX: there is no registry.

TRegistry::~TRegistry() = default;

// Everything reports "absent" rather than "empty". Returning defaults for
// settings a user has actually set would look to them like their configuration
// had been lost, which is far worse than a backend that plainly does not work.
bool TRegistry::OpenKey(const UnicodeString &, bool) { return false; }
bool TRegistry::OpenKeyReadOnly(const UnicodeString &) { return false; }
void TRegistry::CloseKey() { FCurrentKey = nullptr; FCurrentPath = UnicodeString(); }
bool TRegistry::CreateKey(const UnicodeString &) { return false; }
bool TRegistry::DeleteKey(const UnicodeString &) { return false; }
bool TRegistry::DeleteValue(const UnicodeString &) { return false; }
bool TRegistry::KeyExists(const UnicodeString &) { return false; }
bool TRegistry::HasSubKeys() { return false; }
bool TRegistry::ValueExists(const UnicodeString &) { return false; }

void TRegistry::GetKeyNames(TStrings * Strings)
{
  if (Strings != nullptr) { Strings->Clear(); }
}

void TRegistry::GetValueNames(TStrings * Strings)
{
  if (Strings != nullptr) { Strings->Clear(); }
}

int TRegistry::GetDataSize(const UnicodeString &) { return -1; }
TRegDataType TRegistry::GetDataType(const UnicodeString &) { return rdUnknown; }
UnicodeString TRegistry::ReadString(const UnicodeString &) { return UnicodeString(); }
void TRegistry::WriteString(const UnicodeString &, const UnicodeString &) {}
int TRegistry::ReadInteger(const UnicodeString &) { return 0; }
void TRegistry::WriteInteger(const UnicodeString &, int) {}
int TRegistry::ReadBinaryData(const UnicodeString &, void *, int) { return 0; }
void TRegistry::WriteBinaryData(const UnicodeString &, const void *, int) {}

#endif

// Shared across both platforms: Delphi stores booleans as integers.
bool TRegistry::ReadBool(const UnicodeString & Name)
{
  return ReadInteger(Name) != 0;
}

void TRegistry::WriteBool(const UnicodeString & Name, bool Value)
{
  WriteInteger(Name, Value ? 1 : 0);
}

double TRegistry::ReadFloat(const UnicodeString & Name)
{
  double Value = 0.0;
  return (ReadBinaryData(Name, &Value, sizeof(Value)) == static_cast<int>(sizeof(Value))) ? Value : 0.0;
}

void TRegistry::WriteFloat(const UnicodeString & Name, double Value)
{
  WriteBinaryData(Name, &Value, sizeof(Value));
}

TDateTime TRegistry::ReadDateTime(const UnicodeString & Name)
{
  return TDateTime(ReadFloat(Name));
}

void TRegistry::WriteDateTime(const UnicodeString & Name, TDateTime Value)
{
  WriteFloat(Name, Value.Val());
}
