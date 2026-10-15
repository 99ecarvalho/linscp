// The Generate URL dialog's generation (forms/GenerateUrl.cpp's
// TGenerateUrlDialog), VCL side: what it writes for a session, as the
// dialog does it, without the dialog.
#ifndef LINSCP_QT_GENERATE_CODE_H
#define LINSCP_QT_GENERATE_CODE_H

#include <Common.h>
#include <CopyParam.h>
#include <SessionData.h>

#include <vector>

#include "CoreBridge.h"

// The Copy dialog's transfer (TGenerateUrlDialog's FTransfer and the rest):
// its direction, files (names, from SourcePath; only a sample of them
// when PathsSample), all files instead (fsAll), target Path, settings.
struct TGenerateTransfer
{
  bool ToRemote = false;
  bool Move = false;
  int CopyParamAttrs = 0;
  TCopyParamType CopyParam;
  bool AllFiles = false;
  std::vector<UnicodeString> Paths;
  bool PathsSample = false;
  UnicodeString SourcePath;
  UnicodeString Path;
};

CoreBridge::TGeneratedCode GenerateSessionCode(TSessionData * Data, const CoreBridge::TGenerateCodeOptions & Options,
  const UnicodeString & ExeName, const TGenerateTransfer * Transfer = nullptr);

#endif
