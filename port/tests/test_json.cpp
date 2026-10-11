// System.JSON, as S3FileSystem uses it on the AWS credentials response.
#include "vcl_json.h"

#include <cstdio>
#include <memory>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

bool Invalid(const wchar_t * Text)
{
  std::unique_ptr<TJSONValue> V(TJSONObject::ParseJSONValue(Text));
  return V == nullptr;
}

void TestAwsCredentialsResponse()
{
  // S3FileSystem.cpp, step by step.
  std::unique_ptr<TJSONValue> ProfileDataValue(TJSONObject::ParseJSONValue(
    L"{\n  \"Code\" : \"Success\",\n  \"LastUpdated\" : \"2024-03-05T12:00:00Z\",\n"
    L"  \"Type\" : \"AWS-HMAC\",\n  \"AccessKeyId\" : \"ASIAEXAMPLE\",\n"
    L"  \"SecretAccessKey\" : \"s3cr\\/t+key\",\n  \"Token\" : \"tok\",\n"
    L"  \"Expiration\" : \"2024-03-05T18:00:00Z\"\n}"));
  TJSONObject * ProfileData = dynamic_cast<TJSONObject *>(ProfileDataValue.get());
  CHECK(ProfileData != NULL);
  TJSONValue * CodeValue = ProfileData->Values[L"Code"];
  CHECK((CodeValue != NULL) && (CodeValue->Value() == UnicodeString(L"Success")));
  CHECK(ProfileData->Values[L"Missing"] == NULL);
  CHECK(ProfileData->Values[L"SecretAccessKey"]->Value() == UnicodeString(L"s3cr/t+key"));

  std::unique_ptr<TJSONObject::TEnumerator> Enumerator(ProfileData->GetEnumerator());
  int N = 0;
  UnicodeString First;
  while (Enumerator->MoveNext())
  {
    TJSONPair * Pair = Enumerator->Current;
    if (N == 0) { First = Pair->JsonString->Value(); }
    ++N;
  }
  CHECK(N == 7);
  CHECK(First == UnicodeString(L"Code"));                    // document order
}

void TestValuesAndEscapes()
{
  std::unique_ptr<TJSONValue> V(TJSONObject::ParseJSONValue(
    L"{\"s\":\"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\",\"n\":-12.5e3,\"t\":true,"
    L"\"f\":false,\"z\":null,\"a\":[1,[2,{}],\"x\"],\"o\":{\"k\":\"v\"}}"));
  TJSONObject * O = dynamic_cast<TJSONObject *>(V.get());
  CHECK(O != NULL);
  const UnicodeString S = O->Values[L"s"]->Value();
  CHECK(S.Length() == 9);
  CHECK(S.SubString(1, 6) == UnicodeString(L"a\"b\\c\n"));
  CHECK(S[7] == static_cast<wchar_t>(0x00E9));
  CHECK((S[8] == static_cast<wchar_t>(0xD83D)) && (S[9] == static_cast<wchar_t>(0xDE00)));
  CHECK(O->Values[L"n"]->Value() == UnicodeString(L"-12.5e3"));   // the number's text
  CHECK(dynamic_cast<TJSONNumber *>(O->Values[L"n"]) != NULL);
  CHECK(O->Values[L"t"]->Value() == UnicodeString(L"true"));
  CHECK(dynamic_cast<TJSONTrue *>(O->Values[L"t"]) != NULL);
  CHECK(O->Values[L"z"]->Value() == UnicodeString(L"null"));
  TJSONArray * A = dynamic_cast<TJSONArray *>(O->Values[L"a"]);
  CHECK((A != NULL) && (A->Count == 3));
  CHECK(dynamic_cast<TJSONArray *>(A->Items[1]) != NULL);
  CHECK(dynamic_cast<TJSONObject *>(O->Values[L"o"])->Values[L"k"]->Value() == UnicodeString(L"v"));
  // Not an object: the dynamic_cast core does is what catches it.
  std::unique_ptr<TJSONValue> Arr(TJSONObject::ParseJSONValue(L"[1,2]"));
  CHECK((Arr != nullptr) && (dynamic_cast<TJSONObject *>(Arr.get()) == NULL));
}

void TestInvalidInputIsNull()
{
  CHECK(Invalid(L""));
  CHECK(Invalid(L"   "));
  CHECK(Invalid(L"{\"a\":1} junk"));        // trailing content
  CHECK(Invalid(L"{\"a\":1,}"));            // trailing comma
  CHECK(Invalid(L"{\"a\" 1}"));
  CHECK(Invalid(L"{'a':1}"));               // single quotes
  CHECK(Invalid(L"\"unterminated"));
  CHECK(Invalid(L"\"bad \\x escape\""));
  CHECK(Invalid(L"\"tab\there\""));         // raw control character
  CHECK(Invalid(L"01"));                    // leading zero
  CHECK(Invalid(L"1."));
  CHECK(Invalid(L"tru"));
  CHECK(Invalid(L"<html>Error</html>"));    // what a proxy might send instead
  // Deep nesting fails cleanly rather than exhausting the stack.
  UnicodeString Deep;
  for (int I = 0; I < 10000; ++I) { Deep += L'['; }
  CHECK(Invalid(Deep.c_str()));
  CHECK(!Invalid(L" {} "));
}

}  // namespace

int main()
{
  TestAwsCredentialsResponse();
  TestValuesAndEscapes();
  TestInvalidInputIsNull();

  if (Failures == 0)
  {
    std::printf("all JSON tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
