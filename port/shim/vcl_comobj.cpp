#include "vcl_rtl.h"

EOleSysError::EOleSysError(const UnicodeString & Message, HRESULT AErrorCode,
  int /*HelpContext*/) :
  EOleError(Message.IsEmpty() ? SysErrorMessage(static_cast<int>(AErrorCode)) : Message),
  ErrorCode(AErrorCode)
{
}

namespace System { namespace Win { namespace Comobj {

void __fastcall OleCheck(HRESULT Result)
{
  // Delphi's Succeeded: the severity bit clear.
  if (Result < 0)
  {
    throw EOleSysError(UnicodeString(), Result, 0);
  }
}

}}}
