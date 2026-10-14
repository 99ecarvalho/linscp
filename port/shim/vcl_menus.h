// Delphi's Vcl.Menus shortcut functions, as upstream's GUI code uses them
// (custom commands and bookmarks keep TShortCut): ShortCut, ShortCutToText
// and TextToShortCut, with Delphi's text ("Shift+Ctrl+Alt+F5", its key
// names), and the virtual keys they name where the platform has none.
#ifndef LINSCP_VCL_MENUS_H
#define LINSCP_VCL_MENUS_H

#include "vcl_set.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

const TShortCut scShift = 0x2000;
const TShortCut scCtrl = 0x4000;
const TShortCut scAlt = 0x8000;
const TShortCut scNone = 0;

#if !defined(_WIN32)
enum : unsigned short
{
  VK_BACK = 0x08, VK_TAB = 0x09, VK_RETURN = 0x0D, VK_ESCAPE = 0x1B, VK_SPACE = 0x20,
  VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24,
  VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28,
  VK_INSERT = 0x2D, VK_DELETE = 0x2E,
  VK_NUMPAD0 = 0x60, VK_NUMPAD9 = 0x69,
  VK_F1 = 0x70, VK_F24 = 0x87,
};
#endif

TShortCut __fastcall ShortCut(Word Key, TShiftState Shift);
UnicodeString __fastcall ShortCutToText(TShortCut ShortCut);
// 0 for what is not one.
TShortCut __fastcall TextToShortCut(UnicodeString Text);

#endif // LINSCP_VCL_MENUS_H
