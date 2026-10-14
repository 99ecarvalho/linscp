// The shell's configuration: upstream's TGUIConfiguration (compiled as it is,
// port/gui), with what the shell keeps of TWinConfiguration's settings under
// TWinConfiguration's own keys and defaults, so a WinSCP.ini carries over
// both ways (ADR 0014). VCL side.
#ifndef LINSCP_QT_SHELL_CONFIGURATION_H
#define LINSCP_QT_SHELL_CONFIGURATION_H

#include <Common.h>
#include <GUIConfiguration.h>
#include <Bookmarks.h>

#include <memory>

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
  // Integration > Applications: each new session opened in PuTTY too.
  bool AutoOpenInPutty = false;
  // File colors (TFileColorData::SaveList: "BBGGRR:mask" items, comma text).
  UnicodeString FileColors;
  // Storage: where temporary files go (empty: the system's temporary
  // directory), environment variables allowed.
  UnicodeString DDTemporaryDirectory;
  // How edited files' temporary directories are made
  // (TemporaryDirectoryForRemoteFiles): in a unique one under it or in the
  // same place each time; with the session's name and the remote path
  // appended; cleaned up at start (asking first).
  bool TemporaryDirectoryAppendSession = false;
  bool TemporaryDirectoryDeterministic = false;
  bool TemporaryDirectoryAppendPath = true;
  bool TemporaryDirectoryCleanup = true;
  bool ConfirmTemporaryDirectoryCleanup = true;
  // The internal editor (Interface\Editor): word wrap, tab size, the
  // encoding (CP_UTF8, or 0 for the system's code page), and the last find
  // and replace.
  bool EditorWordWrap = false;
  int EditorTabSize = 8;
  int EditorEncoding = 65001;
  UnicodeString EditorFindText;
  UnicodeString EditorReplaceText;
  bool EditorFindMatchCase = false;
  bool EditorFindWholeWord = false;
  // EditorCheckNotModified: ask before uploading over a file changed on the
  // server since it was opened.
  bool EditorCheckNotModified = false;
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
  // Open directory's page: the shared bookmarks rather than the site's.
  bool UseSharedBookmarks = false;
  bool UseLocationProfiles = false;
  // The Generate URL dialog's choices: -1 components (its default).
  int GenerateUrlComponents = -1;
  int GenerateUrlCodeTarget = 0;
  int GenerateUrlScriptFormat = 0;
  int GenerateUrlAssemblyLanguage = 0;
  int QueueViewHeight = 140;
  int QueueViewHeightPixelsPerInch = 96;
};

class TCustomCommandList;

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
  // As TWinConfiguration::ExpandedTemporaryDirectory.
  UnicodeString ExpandedTemporaryDirectory() const;
  // As TWinConfiguration's: whether temporary folders of the program's
  // are left (TemporaryDir's mask), and their removal (ExtException with
  // those that could not be removed).
  bool AnyTemporaryFolders();
  void CleanupTemporaryFolders();
  // The bookmarks, kept as TWinConfiguration keeps them (its Bookmarks
  // key): per site, by session key, and shared.
  TBookmarks * Bookmarks() const { return FBookmarks.get(); }
  // The custom commands, as TWinConfiguration keeps them (CustomCommands;
  // its defaults until changed), and their options' values
  // (CustomCommandOptions).
  const TCustomCommandList * CustomCommandList() const { return FCustomCommandList.get(); }
  void SetCustomCommandList(const TCustomCommandList * List);
  TStrings * CustomCommandOptions() const { return FCustomCommandOptions.get(); }
  void SetCustomCommandOptions(TStrings * Options);
  // TWinConfiguration's, for the custom commands' slice (extensions'
  // strings: not translated yet).
  UnicodeString __fastcall ExtensionStringTranslation(const UnicodeString & ExtensionId, const UnicodeString & S);

protected:
  virtual void __fastcall SaveData(THierarchicalStorage * Storage, bool All);
  virtual void __fastcall LoadData(THierarchicalStorage * Storage);
  virtual void __fastcall Saved();
  virtual void __fastcall DefaultLocalized();
  // TGUIConfiguration's, with PuttyPath's POSIX default (PosixPuttyPath).
  virtual void __fastcall Default();

private:
  // TWinConfiguration::DoFindTemporaryFolders: NULL when there are none.
  TStrings * FindTemporaryFolders(bool OnlyFirst);

  bool FOverridden = false;
  LCID FSavedLocale = 0;
  LCID FDefaultLocale = 0;
  UnicodeString FModuleFileName;
  TShellSettings FSettings;
  std::unique_ptr<TBookmarks> FBookmarks;
  std::unique_ptr<TCustomCommandList> FCustomCommandList;
  bool FCustomCommandsDefaults = true;
  std::unique_ptr<TStringList> FCustomCommandOptions;
  bool FCustomCommandOptionsModified = false;
};

#endif
