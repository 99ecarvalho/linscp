// The Generate URL dialog's generation (forms/GenerateUrl.cpp's
// TGenerateUrlDialog), VCL side: what it writes for a session, as the
// dialog does it, without the dialog.
#ifndef LINSCP_QT_GENERATE_CODE_H
#define LINSCP_QT_GENERATE_CODE_H

#include <SessionData.h>

#include "CoreBridge.h"

CoreBridge::TGeneratedCode GenerateSessionCode(TSessionData * Data, const CoreBridge::TGenerateCodeOptions & Options,
  const UnicodeString & ExeName);

#endif
