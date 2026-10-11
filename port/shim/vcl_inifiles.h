//---------------------------------------------------------------------------
// TCustomIniFile and TMemIniFile.
//
// Core derives TCustomIniFileStorage from THierarchicalStorage and holds a
// TCustomIniFile *, so the base must be abstract with Delphi's virtual surface.
// On Linux this is the storage backend that actually matters: core already
// supports an INI path alongside the registry one, and the INI path is the one
// a port should use.
//
// Section and key names are matched case-insensitively, as Delphi's are.
// Settings written by WinSCP on Windows must read back here, and Windows users
// will have files with inconsistent casing.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_INIFILES_H
#define LINSCP_VCL_INIFILES_H

#include <utility>
#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_strings.h"
#include "vcl_unicodestring.h"

class TCustomIniFile : public TObject
{
public:
  explicit TCustomIniFile(const UnicodeString AFileName)
    : FFileName(AFileName) {}

  PROP_R_F(UnicodeString, FileName, FFileName);

  // Parameters by value, as C++Builder declares Delphi's virtuals: core's own
  // descendants (TOptionsIniFile) override with exactly these signatures, and
  // a reference parameter would make theirs a new function rather than an
  // override -- leaving the class abstract. The abstract set is Delphi's.
  virtual UnicodeString ReadString(const UnicodeString Section,
                                   const UnicodeString Ident,
                                   const UnicodeString Default) = 0;
  virtual void WriteString(const UnicodeString Section,
                           const UnicodeString Ident,
                           const UnicodeString Value) = 0;
  virtual void ReadSection(const UnicodeString Section, TStrings * Strings) = 0;
  virtual void ReadSections(TStrings * Strings) = 0;
  virtual void ReadSectionValues(const UnicodeString Section,
                                 TStrings * Strings) = 0;
  virtual void EraseSection(const UnicodeString Section) = 0;
  virtual void DeleteKey(const UnicodeString Section,
                         const UnicodeString Ident) = 0;
  virtual void UpdateFile() = 0;

  // Not abstract in Delphi: both are answered through ReadSection.
  virtual bool SectionExists(const UnicodeString Section);
  virtual bool ValueExists(const UnicodeString Section, const UnicodeString Ident);

  // Typed accessors, written in terms of ReadString/WriteString exactly as
  // Delphi's are, so a descendant only has to provide the string pair.
  virtual int ReadInteger(const UnicodeString Section, const UnicodeString Ident,
                  int Default);
  virtual void WriteInteger(const UnicodeString Section, const UnicodeString Ident,
                    int Value);
  virtual __int64 ReadInt64(const UnicodeString Section, const UnicodeString Ident,
                    __int64 Default);
  virtual void WriteInt64(const UnicodeString Section, const UnicodeString Ident,
                  __int64 Value);
  virtual bool ReadBool(const UnicodeString Section, const UnicodeString Ident,
                bool Default);
  virtual void WriteBool(const UnicodeString Section, const UnicodeString Ident,
                 bool Value);

protected:
  UnicodeString FFileName;
};

//---------------------------------------------------------------------------
// An in-memory INI file, loaded on construction and written by UpdateFile.
//---------------------------------------------------------------------------
class TMemIniFile : public TCustomIniFile
{
public:
  explicit TMemIniFile(const UnicodeString AFileName);

  UnicodeString ReadString(const UnicodeString Section,
                           const UnicodeString Ident,
                           const UnicodeString Default) override;
  void WriteString(const UnicodeString Section, const UnicodeString Ident,
                   const UnicodeString Value) override;
  void ReadSection(const UnicodeString Section, TStrings * Strings) override;
  void ReadSections(TStrings * Strings) override;
  void ReadSectionValues(const UnicodeString Section,
                         TStrings * Strings) override;
  void EraseSection(const UnicodeString Section) override;
  void DeleteKey(const UnicodeString Section,
                 const UnicodeString Ident) override;
  bool SectionExists(const UnicodeString Section) override;
  bool ValueExists(const UnicodeString Section,
                   const UnicodeString Ident) override;
  void UpdateFile() override;
  void Clear();

  void Rename(const UnicodeString AFileName, bool Reload);
  void GetStrings(TStrings * Strings);
  void SetStrings(TStrings * Strings);

private:
  using TPair = std::pair<UnicodeString, UnicodeString>;
  struct TSection
  {
    UnicodeString Name;
    std::vector<TPair> Values;
  };

  // Ordered rather than hashed: writing the file back must preserve the order
  // a human or an earlier WinSCP wrote, or every save would reshuffle it.
  std::vector<TSection> FSections;

  TSection * FindSection(const UnicodeString Name);
  TSection & EnsureSection(const UnicodeString Name);
  static TPair * FindValue(TSection & Section, const UnicodeString Ident);
  void LoadFromFile();
};

#endif // LINSCP_VCL_INIFILES_H
