#include "vcl_object.h"

#include "vcl_format.h"
#include "vcl_resources.h"

namespace {

// Resolve a resource id to its string, as Delphi's CreateRes forms do.
UnicodeString FromIdent(NativeUInt Ident)
{
  const wchar_t * Text = linscp::FindResourceString(static_cast<int>(Ident));
  return Text != nullptr ? UnicodeString(Text) : UnicodeString();
}

UnicodeString FromResStringRec(PResStringRec Rec)
{
  return Rec == nullptr ? UnicodeString() : FromIdent(Rec->Identifier);
}

// Delphi's CreateFmt forms format the message with the variant arguments.
UnicodeString Formatted(const UnicodeString & Fmt, const TVarRec * Args,
                        int ArgsSize)
{
  if (Args == nullptr || ArgsSize < 0)
  {
    return Fmt;
  }
  // C++Builder passes the *index of the last element*, not the count, which is
  // what ARRAYOFCONST computes. Off by one here would silently drop the final
  // argument from every formatted exception message.
  TFormatArgs Collected;
  for (int I = 0; I <= ArgsSize; ++I)
  {
    Collected.push_back(Args[I]);
  }
  return Format(Fmt, Collected);
}

}  // namespace

Exception::Exception(const UnicodeString & Msg)
  : FMessage(Msg) {}

Exception::Exception(const UnicodeString & Msg, int AHelpContext)
  : FMessage(Msg), FHelpContext(AHelpContext) {}

Exception::Exception(const UnicodeString & Msg, const TVarRec * Args,
                     int Args_Size)
  : FMessage(Formatted(Msg, Args, Args_Size)) {}

Exception::Exception(const UnicodeString & Msg, const TVarRec * Args,
                     int Args_Size, int AHelpContext)
  : FMessage(Formatted(Msg, Args, Args_Size)), FHelpContext(AHelpContext) {}

Exception::Exception(NativeUInt Ident)
  : FMessage(FromIdent(Ident)) {}

Exception::Exception(NativeUInt Ident, int AHelpContext)
  : FMessage(FromIdent(Ident)), FHelpContext(AHelpContext) {}

Exception::Exception(NativeUInt Ident, const TVarRec * Args, int Args_Size)
  : FMessage(Formatted(FromIdent(Ident), Args, Args_Size)) {}

Exception::Exception(NativeUInt Ident, const TVarRec * Args, int Args_Size,
                     int AHelpContext)
  : FMessage(Formatted(FromIdent(Ident), Args, Args_Size)),
    FHelpContext(AHelpContext) {}

Exception::Exception(PResStringRec ResStringRec)
  : FMessage(FromResStringRec(ResStringRec)) {}

Exception::Exception(PResStringRec ResStringRec, int AHelpContext)
  : FMessage(FromResStringRec(ResStringRec)), FHelpContext(AHelpContext) {}

Exception::Exception(PResStringRec ResStringRec, const TVarRec * Args,
                     int Args_Size)
  : FMessage(Formatted(FromResStringRec(ResStringRec), Args, Args_Size)) {}

Exception::Exception(PResStringRec ResStringRec, const TVarRec * Args,
                     int Args_Size, int AHelpContext)
  : FMessage(Formatted(FromResStringRec(ResStringRec), Args, Args_Size)),
    FHelpContext(AHelpContext) {}
