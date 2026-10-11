//---------------------------------------------------------------------------
// TRegistry.
//
// Core holds a TRegistry * in TRegistryStorage, one of two THierarchicalStorage
// backends; the other is INI-backed and already exists upstream.
//
// On Windows this wraps the real registry. On POSIX there is no registry, and
// the honest behaviour is to report that every key is absent: OpenKey fails,
// KeyExists and ValueExists are false. That makes TRegistryStorage visibly
// unusable rather than quietly returning defaults for settings a user has
// actually set -- which would look like their configuration had been lost.
//
// Selecting the INI backend on Linux is a decision for the frontend, tracked
// as its own task. This class exists so core compiles and so the registry path
// fails loudly if anything reaches it.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_REGISTRY_H
#define LINSCP_VCL_REGISTRY_H

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_strings.h"
#include "vcl_unicodestring.h"

#if defined(_WIN32)
  #include <windows.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
#endif

enum TRegDataType { rdUnknown, rdString, rdExpandString, rdInteger, rdBinary };

struct TRegDataInfo
{
  TRegDataType RegData = rdUnknown;
  int DataSize = 0;
};

class TRegistry : public TObject
{
public:
  TRegistry() = default;
  explicit TRegistry(unsigned AAccess) : FAccess(AAccess) {}
  ~TRegistry() override;

  PROP_RW_FF(HKEY, RootKey, FRootKey);
  PROP_RW_FF(unsigned, Access, FAccess);
  PROP_R_F(HKEY, CurrentKey, FCurrentKey);
  PROP_R_F(UnicodeString, CurrentPath, FCurrentPath);

  bool OpenKey(const UnicodeString & Key, bool CanCreate);
  bool OpenKeyReadOnly(const UnicodeString & Key);
  void CloseKey();
  bool CreateKey(const UnicodeString & Key);
  bool DeleteKey(const UnicodeString & Key);
  bool DeleteValue(const UnicodeString & Name);
  bool KeyExists(const UnicodeString & Key);
  bool ValueExists(const UnicodeString & Name);
  void GetKeyNames(TStrings * Strings);
  void GetValueNames(TStrings * Strings);

  int GetDataSize(const UnicodeString & Name);
  TRegDataType GetDataType(const UnicodeString & Name);

  UnicodeString ReadString(const UnicodeString & Name);
  void WriteString(const UnicodeString & Name, const UnicodeString & Value);
  int ReadInteger(const UnicodeString & Name);
  void WriteInteger(const UnicodeString & Name, int Value);
  bool ReadBool(const UnicodeString & Name);
  void WriteBool(const UnicodeString & Name, bool Value);
  int ReadBinaryData(const UnicodeString & Name, void * Buffer, int BufSize);
  void WriteBinaryData(const UnicodeString & Name, const void * Buffer,
                       int BufSize);

private:
  HKEY FRootKey = HKEY_CURRENT_USER;
  HKEY FCurrentKey = nullptr;
  UnicodeString FCurrentPath;
  unsigned FAccess = KEY_ALL_ACCESS;
};

#endif // LINSCP_VCL_REGISTRY_H
