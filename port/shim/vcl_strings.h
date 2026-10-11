//---------------------------------------------------------------------------
// TStrings and TStringList.
//
// WATCH THE INDEX BASE. TStrings is ZERO-based: Strings[0] is the first item.
// UnicodeString, in the same codebase, is ONE-based. Mixing them up produces
// off-by-one corruption rather than a compile error, so both conventions are
// tested at their boundaries.
//
// Two further Delphi defaults that surprise people and that core relies on:
//   CaseSensitive defaults to False, so IndexOf and Values are case-insensitive
//   Duplicates defaults to dupIgnore, and only applies when Sorted is True
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_STRINGS_H
#define LINSCP_VCL_STRINGS_H

#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_unicodestring.h"

enum TDuplicates { dupIgnore, dupAccept, dupError };

class TStrings : public TPersistent
{
public:
  virtual ~TStrings() = default;

  // --- implemented by derived classes --------------------------------------
  virtual int GetCount() const = 0;
  virtual UnicodeString GetString(int Index) const = 0;
  virtual void PutString(int Index, UnicodeString Value) = 0;
  virtual TObject * GetObject(int Index) const = 0;
  virtual void PutObject(int Index, TObject * Value) = 0;
  virtual void Clear() = 0;
  virtual void Delete(int Index) = 0;
  virtual void Insert(int Index, const UnicodeString & S) = 0;

  // --- properties core uses ------------------------------------------------
  PROP_R_G(int, Count, GetCount);
  PROP_IDX_RW_GS(UnicodeString, Strings, int, GetString, PutString);
  PROP_IDX_RW_GS(TObject *, Objects, int, GetObject, PutObject);

  UnicodeString GetName(int Index) const;
  PROP_IDX_R_G(UnicodeString, Names, int, GetName);

  UnicodeString GetValueFromIndex(int Index) const;
  PROP_IDX_R_G(UnicodeString, ValueFromIndex, int, GetValueFromIndex);

  UnicodeString GetValue(const UnicodeString & Name) const;
  void SetValue(const UnicodeString & Name, UnicodeString Value);
  PROP_IDX_RW_GS(UnicodeString, Values, const UnicodeString &, GetValue, SetValue);

  UnicodeString GetText() const;
  void SetText(UnicodeString Value);
  PROP_RW_GS(UnicodeString, Text, GetText, SetText);

  UnicodeString GetCommaText() const;
  void SetCommaText(UnicodeString Value);
  PROP_RW_GS(UnicodeString, CommaText, GetCommaText, SetCommaText);

  UnicodeString GetDelimitedText() const;
  void SetDelimitedText(UnicodeString Value);
  PROP_RW_GS(UnicodeString, DelimitedText, GetDelimitedText, SetDelimitedText);

  PROP_RW_FF(wchar_t, Delimiter, FDelimiter);
  PROP_RW_FF(wchar_t, QuoteChar, FQuoteChar);
  PROP_RW_FF(wchar_t, NameValueSeparator, FNameValueSeparator);
  PROP_RW_FF(bool, CaseSensitive, FCaseSensitive);

  // --- operations ----------------------------------------------------------
  virtual int Add(const UnicodeString & S);
  int AddObject(const UnicodeString & S, TObject * AObject);
  void InsertObject(int Index, const UnicodeString & S, TObject * AObject);
  void AddStrings(const TStrings * Strings);
  void AddPair(const UnicodeString & Name, const UnicodeString & Value);
  virtual int IndexOf(const UnicodeString & S) const;
  int IndexOfName(const UnicodeString & Name) const;
  int IndexOfObject(TObject * AObject) const;
  virtual void Assign(TPersistent * Source) override;
  virtual void Move(int CurIndex, int NewIndex);
  void Exchange(int Index1, int Index2);
  bool Equals(const TStrings * Strings) const;
  void BeginUpdate() { ++FUpdateCount; }
  void EndUpdate() { if (FUpdateCount > 0) { --FUpdateCount; } }

  void SaveToFile(const UnicodeString & FileName) const;
  void LoadFromFile(const UnicodeString & FileName);

protected:
  bool SameString(const UnicodeString & A, const UnicodeString & B) const;
  void CheckIndex(int Index) const;      // raises EStringListError, as Delphi does

  wchar_t FDelimiter = L',';
  wchar_t FQuoteChar = L'"';
  wchar_t FNameValueSeparator = L'=';
  bool FCaseSensitive = false;           // Delphi's default
  int FUpdateCount = 0;
};

class TStringList : public TStrings
{
public:
  TStringList() = default;
  explicit TStringList(bool AOwnsObjects) : FOwnsObjects(AOwnsObjects) {}
  ~TStringList() override;

  int GetCount() const override { return static_cast<int>(FItems.size()); }
  UnicodeString GetString(int Index) const override;
  void PutString(int Index, UnicodeString Value) override;
  TObject * GetObject(int Index) const override;
  void PutObject(int Index, TObject * Value) override;
  void Clear() override;
  void Delete(int Index) override;
  void Insert(int Index, const UnicodeString & S) override;

  int Add(const UnicodeString & S) override;
  int IndexOf(const UnicodeString & S) const override;

  void SetSorted(bool Value);
  bool GetSorted() const { return FSorted; }
  PROP_RW_GS(bool, Sorted, GetSorted, SetSorted);
  PROP_RW_FF(TDuplicates, Duplicates, FDuplicates);

  void Sort();

  // Delphi's binary search: true when S is present. Index is then a match --
  // the first one under dupAccept -- and otherwise the insertion point. Uses
  // the same comparison as sorting, and like Delphi is meaningful only when
  // the list is Sorted.
  bool Find(const UnicodeString & S, int & Index) const;

  // Delphi 2009's TStringList.OwnsObjects. When set, Delete, Clear and the
  // destructor free the objects -- after removing the entries, as Delphi
  // does. Off by default, matching the note on FItems below.
  PROP_RW_FF(bool, OwnsObjects, FOwnsObjects);

private:
  struct TItem
  {
    UnicodeString Text;
    TObject * Object = nullptr;
  };
  // Objects are referenced, never owned: Delphi's TStrings does not free them,
  // and core relies on that for objects it owns elsewhere.
  std::vector<TItem> FItems;
  bool FSorted = false;
  TDuplicates FDuplicates = dupIgnore;   // Delphi's default
  bool FOwnsObjects = false;
};

#endif // LINSCP_VCL_STRINGS_H
