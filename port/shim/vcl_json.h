//---------------------------------------------------------------------------
// System.JSON, as far as core uses it.
//
// One use, in S3FileSystem: parsing the AWS instance-metadata credentials
// response. ParseJSONValue returns null for invalid input, Values[Name] null
// for a missing member, Value() a string's unescaped text, and GetEnumerator an
// owned enumerator over the pairs. Objects own their pairs, pairs their name
// and value; deleting the root value frees the tree, as in Delphi.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_JSON_H
#define LINSCP_VCL_JSON_H

#include <memory>
#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"
#include "vcl_unicodestring.h"

namespace System {
namespace Json {

class TJSONValue : public TObject
{
public:
  // Delphi's: a string's text, a number's digits, "true", "false", "null";
  // empty for objects and arrays.
  virtual UnicodeString Value() const { return UnicodeString(); }
};

class TJSONString : public TJSONValue
{
public:
  explicit TJSONString(const UnicodeString & S) : FValue(S) {}
  UnicodeString Value() const override { return FValue; }

private:
  UnicodeString FValue;
};

// Delphi derives TJSONNumber from TJSONString; Value() is the number's text.
class TJSONNumber : public TJSONString
{
public:
  using TJSONString::TJSONString;
};

class TJSONBool : public TJSONValue
{
public:
  explicit TJSONBool(bool B) : FValue(B) {}
  UnicodeString Value() const override { return FValue ? L"true" : L"false"; }
  bool AsBoolean() const { return FValue; }

private:
  bool FValue;
};

class TJSONTrue : public TJSONBool { public: TJSONTrue() : TJSONBool(true) {} };
class TJSONFalse : public TJSONBool { public: TJSONFalse() : TJSONBool(false) {} };

class TJSONNull : public TJSONValue
{
public:
  UnicodeString Value() const override { return L"null"; }
};

class TJSONPair : public TObject
{
public:
  TJSONPair(TJSONString * Name, TJSONValue * Value) : FName(Name), FValue(Value) {}

  TJSONString * GetJsonString() const { return FName.get(); }
  TJSONValue * GetJsonValue() const { return FValue.get(); }
  PROP_R_G(TJSONString *, JsonString, GetJsonString);
  PROP_R_G(TJSONValue *, JsonValue, GetJsonValue);

private:
  std::unique_ptr<TJSONString> FName;
  std::unique_ptr<TJSONValue> FValue;
};

class TJSONArray : public TJSONValue
{
public:
  void Add(TJSONValue * V) { FItems.emplace_back(V); }
  int GetCount() const { return static_cast<int>(FItems.size()); }
  TJSONValue * GetItem(int Index) const
  {
    return ((Index >= 0) && (Index < GetCount())) ? FItems[static_cast<std::size_t>(Index)].get()
                                                   : nullptr;
  }
  PROP_R_G(int, Count, GetCount);
  PROP_IDX_R_G(TJSONValue *, Items, int, GetItem);

private:
  std::vector<std::unique_ptr<TJSONValue>> FItems;
};

class TJSONObject : public TJSONValue
{
public:
  class TEnumerator
  {
  public:
    explicit TEnumerator(const TJSONObject * Object) : FObject(Object) {}
    bool MoveNext() { return ++FIndex < FObject->GetCount(); }
    TJSONPair * GetCurrent() const { return FObject->GetPair(FIndex); }
    PROP_R_G(TJSONPair *, Current, GetCurrent);

  private:
    const TJSONObject * FObject;
    int FIndex = -1;
  };

  // Null for anything that is not a single well-formed JSON value, trailing
  // whitespace aside. The caller owns the result.
  static TJSONValue * ParseJSONValue(const UnicodeString & Data);

  void AddPair(TJSONPair * Pair) { FPairs.emplace_back(Pair); }
  int GetCount() const { return static_cast<int>(FPairs.size()); }
  TJSONPair * GetPair(int Index) const
  {
    return ((Index >= 0) && (Index < GetCount())) ? FPairs[static_cast<std::size_t>(Index)].get()
                                                   : nullptr;
  }
  // The first member with this exact name, or null.
  TJSONValue * GetValue(const UnicodeString & Name) const;
  // Owned by the caller, as core's unique_ptr expects.
  TEnumerator * GetEnumerator() const { return new TEnumerator(this); }

  PROP_R_G(int, Count, GetCount);
  PROP_IDX_R_G(TJSONPair *, Pairs, int, GetPair);
  PROP_IDX_R_G(TJSONValue *, Values, UnicodeString, GetValue);

private:
  std::vector<std::unique_ptr<TJSONPair>> FPairs;
};

}  // namespace Json
}  // namespace System

using System::Json::TJSONValue;
using System::Json::TJSONString;
using System::Json::TJSONNumber;
using System::Json::TJSONBool;
using System::Json::TJSONTrue;
using System::Json::TJSONFalse;
using System::Json::TJSONNull;
using System::Json::TJSONPair;
using System::Json::TJSONArray;
using System::Json::TJSONObject;

#endif // LINSCP_VCL_JSON_H
