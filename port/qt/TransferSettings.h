// Between the bridge's transfer settings and core's copy parameters, for
// the VCL side of the bridge (TransferSettings.cpp).
#ifndef LINSCP_QT_TRANSFER_SETTINGS_H
#define LINSCP_QT_TRANSFER_SETTINGS_H

#include <GUIConfiguration.h>

#include "CoreBridge.h"

namespace CoreBridge
{

TTransferSettings SettingsOf(const TGUICopyParamType & CopyParam);
TGUICopyParamType CopyParamOf(const TTransferSettings & Settings, const TGUICopyParamType & Base);
// What a transfer uses: Settings when given (the Copy dialog's own), else
// the GUI's current ones (the preset in use, or the default), as WinSCP's.
TGUICopyParamType TransferCopyParam(const TTransferSettings * Settings);

}  // namespace CoreBridge

#endif
