// The shell's configuration: upstream's TGUIConfiguration (compiled as it is,
// port/gui), with what the shell keeps of TWinConfiguration's settings under
// TWinConfiguration's own keys and defaults, so a WinSCP.ini carries over
// both ways (ADR 0014). VCL side.
#ifndef LINSCP_QT_SHELL_CONFIGURATION_H
#define LINSCP_QT_SHELL_CONFIGURATION_H

#include <Common.h>
#include <GUIConfiguration.h>

// TWinConfiguration's settings the shell has, as TWinConfiguration's
// constructor defaults them (WinConfiguration.cpp, Default()). The sizes'
// format is BaseUtils.pas' TFormatBytesStyle: fbNone, fbKilobytes, fbShort.
struct TShellSettings
{
  // Interface.
  bool ShowHiddenFiles = false;
  int FormatSizeBytes = 1;
  bool NaturalOrderNumericalSorting = true;
  bool AlwaysSortDirectoriesByName = false;
  bool RenameWholeName = false;
  bool DefaultDirIsHome = true;
  // Window: the current directory in the title (TPathInCaption: picShort,
  // picFull, picNone); the window kept when the last session closes.
  int PathInCaption = 0;
  bool KeepOpenWhenNoSession = true;
  // File colors (TFileColorData::SaveList: "BBGGRR:mask" items, comma text).
  UnicodeString FileColors;
  // Interface\Commander (TScpCommanderConfiguration).
  bool SwappedPanels = false;
  bool ExplorerKeyboardShortcuts = false;
  // The layout, kept between runs, in TWinConfiguration's own formats: the
  // window's placement ("left;top;right;bottom;state;ppi", "-1;-1;w;h;..."
  // never placed; empty here for the shell's default), the local panel's
  // share of the width, the panel that was active (osLocal, osRemote),
  // each panel's columns (TIEListViewColProperties' params), and the queue's
  // height at its pixels per inch.
  UnicodeString WindowParams;
  double LocalPanelWidth = 0.5;
  int CurrentPanel = 0;
  UnicodeString LocalDirViewParams = L"0;1;0|150,1;70,1;120,1;150,1;55,0;55,0;@96|5;0;1;2;3;4";
  UnicodeString RemoteDirViewParams = L"0;1;0|150,1;70,1;150,1;79,1;62,1;55,0;20,0;150,0;125,0;@96|6;7;8;0;1;2;3;4;5";
  int QueueViewHeight = 140;
  int QueueViewHeightPixelsPerInch = 96;
};

class TShellConfiguration : public TGUIConfiguration
{
public:
  // As TWinConfiguration's constructor: the defaults, then the translation
  // of the user interface's language, if there is one, which is the
  // default locale.
  TShellConfiguration();

  virtual UnicodeString TemporaryDir(bool Mask);
  // The shell's executable, whose Translations/ the GUI reads; when
  // Translations is given, the shell beside that folder (a test program
  // using the shell's translations).
  virtual UnicodeString __fastcall ModuleFileName();
  void SetTranslationsDirectory(const UnicodeString & Translations);
  // A locale for this run only (linscp's --lang): applied, but what is
  // saved stays the locale chosen before (or after, through
  // SetPreferredLocale).
  void OverrideLocale(LCID Locale);
  // As the Preferences dialog's Languages page sets it: saved, applied now
  // while the shell's window does not exist, else on the next start, as
  // upstream's CanApplyLocaleImmediately has it. 0: the user interface's
  // language.
  void SetPreferredLocale(LCID Locale);
  // The user interface's language's translation, as TWinConfiguration's
  // DefaultLocale.
  LCID DefaultLocale() const { return FDefaultLocale; }

  const TShellSettings & Settings() const { return FSettings; }
  void SetSettings(const TShellSettings & Settings);

protected:
  virtual void __fastcall SaveData(THierarchicalStorage * Storage, bool All);
  virtual void __fastcall LoadData(THierarchicalStorage * Storage);

private:
  bool FOverridden = false;
  LCID FSavedLocale = 0;
  LCID FDefaultLocale = 0;
  UnicodeString FModuleFileName;
  TShellSettings FSettings;
};

#endif
