// linscp's stand-in for WinSCP's forms/FormsPCH.h, the precompiled header its
// GUI units (windows/, forms/) include through WinPCH.h. Upstream's pulls in
// the whole VCL GUI (WinConfiguration, VCLCommon, GUITools); the units linscp
// compiles from there need only core's and the GUI texts (ADR 0014).
#ifndef LINSCP_GUI_FORMS_PCH_H
#define LINSCP_GUI_FORMS_PCH_H

#include <CorePCH.h>

#include <TextsWin.h>

#include "GuiShim.h"

// The GUI units load nothing but translations as data files, and free
// nothing else (TGUIConfiguration::LoadNewResourceModule,
// FreeResourceModule).
#undef LoadLibraryEx
#define LoadLibraryEx(FileName, File, Flags) (static_cast<HMODULE>(LinscpLoadDataFile(FileName)))
#undef FreeLibrary
#define FreeLibrary(Module) LinscpFreeModule(Module)
#ifdef _WIN32
// The shim's module handle, as the HMODULE the GUI's code assigns it to.
#define HInstance (reinterpret_cast<HMODULE>(::HInstance))
#endif

#endif
