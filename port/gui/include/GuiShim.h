// What WinSCP's GUI units expect of the VCL and Windows beyond what core
// does, for the units linscp compiles from there (ADR 0014), and the shell's
// ways into it. The macros that redirect the GUI units' own calls are in
// FormsPCH.h, which only they include.
#ifndef LINSCP_GUI_SHIM_H
#define LINSCP_GUI_SHIM_H

#include <cstdint>

// Delphi's module handle type: an integer (TLibModule's fields are).
typedef std::uintptr_t HINST;

// A translation is loaded as WinSCP loads its DLL, as a data file
// (TGUIConfiguration::LoadNewResourceModule): here, the shim's translation
// module (port/shim/vcl_resources.h, doc/qt/translations.md).
void * LinscpLoadDataFile(const wchar_t * FileName);
void LinscpFreeModule(void * Module);
// The file the last translation that loaded came from.
UnicodeString LinscpLastTranslationFile();
// A locale as POSIX or BCP 47 names it ("pt_BR.UTF-8", "pt-BR", "de"), as
// Windows numbers it (Windows' own list, port/gui/locales.inc); a language
// alone is its SUBLANG_DEFAULT locale. 0 for none Windows knows.
LCID LinscpLocaleOfName(const UnicodeString & Name);
// LOCALE_SNAME ("pt-BR"); empty for a locale Windows does not list.
UnicodeString LinscpLocaleName(LCID Locale);

#ifndef _WIN32
#define LOAD_LIBRARY_AS_DATAFILE 0x00000002
#define PRIMARYLANGID(LangId) (static_cast<WORD>(LangId) & 0x3ff)
#define SUBLANGID(LangId) (static_cast<WORD>(LangId) >> 10)
#define LANG_CHINESE 0x04
#define SUBLANG_CHINESE_TRADITIONAL 0x01
#define LOCALE_SLANGUAGE 0x00000002
#define LOCALE_SABBREVLANGNAME 0x00000003
#define LOCALE_SNATIVELANGNAME 0x00000004
#define LOCALE_SENGLANGUAGE 0x00001001
// Windows' data for the locale (port/gui/locales.inc); 0 when it has none.
int GetLocaleInfo(LCID Locale, DWORD Type, wchar_t * Data, int Size);
// The locale of LC_ALL, LC_MESSAGES or LANG, as Windows numbers it; US
// English when there is none or it is not one Windows knows.
LANGID GetUserDefaultUILanguage();
enum TKnownFolder { FOLDERID_ProgramFiles };
// There is no Program Files: empty.
bool KnownFolderPath(TKnownFolder FolderID, UnicodeString & Path);
#else
bool KnownFolderPath(REFKNOWNFOLDERID FolderID, UnicodeString & Path);
#endif

// VCL's SysUtils.TLanguages: the system's locales, here Windows' own list on
// every platform (port/gui/locales.inc), so the GUI matches translations to
// locales the same everywhere.
class TLanguages
{
public:
  int GetCount() const;
  LCID GetLocaleID(int Index) const;
  // LOCALE_SABBREVLANGNAME: "PTB".
  UnicodeString GetExt(int Index) const;
  // LOCALE_SNAME: "pt-BR".
  UnicodeString GetLocaleName(int Index) const;
  __declspec(property(get = GetCount)) int Count;
  __declspec(property(get = GetLocaleID)) LCID LocaleID[];
  __declspec(property(get = GetExt)) UnicodeString Ext[];
  __declspec(property(get = GetLocaleName)) UnicodeString LocaleName[];
};
TLanguages * Languages();

// VCL's Screen, as far as the GUI's configuration asks it: the shell's forms
// are Qt's, none is the VCL's.
struct TScreen
{
  int FormCount = 0;
  int DataModuleCount = 0;
};
extern TScreen * Screen;

// VCL's cache of resource strings: none here, LoadStr reads the module.
inline void ResStringCleanupCache() {}

#endif
