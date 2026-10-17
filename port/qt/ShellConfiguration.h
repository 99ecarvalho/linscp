// The shell's configuration: upstream's TGUIConfiguration (compiled as it is,
// port/gui), with what the shell keeps of TWinConfiguration's settings under
// TWinConfiguration's own keys and defaults, so a WinSCP.ini carries over
// both ways (ADR 0014). VCL side.
#ifndef LINSCP_QT_SHELL_CONFIGURATION_H
#define LINSCP_QT_SHELL_CONFIGURATION_H

#include <Common.h>
#include <GUIConfiguration.h>
#include <Bookmarks.h>

#include <functional>
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
  // Panels: incremental search (TIncrementalSearch: isNameStartOnly,
  // isName, isAll), panels' state kept per session, full row, the
  // double-click (TDoubleClickAction: dcaOpen, dcaCopy, dcaEdit; its key
  // CopyOnDoubleClick) and its confirmation, the panels' font (none: the
  // system's).
  int PanelSearch = 0;
  bool PreservePanelState = true;
  bool EnableQueueByDefault = true;
  bool CopyParamAutoSelectNotice = true;
  bool MinimizeToTray = false;
  bool ExternalSessionInExistingInstance = true;
  bool FullRowSelect = false;
  int DoubleClickAction = 2;
  bool CopyOnDoubleClickConfirmation = false;
  UnicodeString PanelFontName;
  int PanelFontSize = 0;
  int PanelFontStyle = 0;
  int PanelFontCharset = 1;
  // ScpCommander.NortonLikeMode (TNortonLikeMode: nlOn, nlOff, nlKeyboard;
  // its key ExplorerStyleSelection).
  int NortonLikeMode = 2;
  // linscp's: the icon set (IconSet.h's ids); empty for the platform's default.
  UnicodeString IconSet;
  bool KeepOpenWhenNoSession = true;
  // Window: the Login dialog at start-up and once the last session closes
  // (ShowLoginWhenNoSession); tab titles truncated to fit
  // (SessionTabCaptionTruncation); a workspace's sessions all connected as
  // it opens, not each as its tab is first selected (WorkspaceConnectAll).
  bool ShowLoginWhenNoSession = true;
  bool SessionTabCaptionTruncation = true;
  bool WorkspaceConnectAll = false;
  // Integration > Applications: each new session opened in PuTTY too.
  bool AutoOpenInPutty = false;
  // Workspaces: saved automatically on closing, with passwords or not, and
  // under which name; the last site or workspace stored on closing.
  bool AutoSaveWorkspace = false;
  bool AutoSaveWorkspacePasswords = false;
  UnicodeString AutoWorkspace;
  UnicodeString LastStoredSession;
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
  UnicodeString EditorWindowParams;
  bool EditorWordWrap = false;
  int EditorTabSize = 8;
  int EditorEncoding = 65001;
  UnicodeString EditorFindText;
  UnicodeString EditorReplaceText;
  bool EditorFindMatchCase = false;
  bool EditorFindWholeWord = false;
  // Its font (TFontConfiguration: an empty name the system's fixed one;
  // the size in points; TFontStyles bits, fsBold 1, fsItalic 2,
  // fsUnderline 4, fsStrikeOut 8), its text and background colours (the
  // VCL's TColor, 0 the defaults), and the large-file warning, in KB.
  UnicodeString EditorFontName;
  int EditorFontSize = 0;
  int EditorFontStyle = 0;
  int EditorFontCharset = 1;
  int EditorFontColor = 0;
  int EditorBackgroundColor = 0;
  bool EditorWarnOrLargeFileSize = true;
  int EditorLargeFileSize = 10 * 1024;
  // EditorCheckNotModified: ask before uploading over a file changed on the
  // server since it was opened.
  bool EditorCheckNotModified = false;
  // Interface\Commander (TScpCommanderConfiguration).
  bool SwappedPanels = false;
  bool ExplorerKeyboardShortcuts = false;
  // ScpCommander.TreeOnLeft: the panels' trees beside their files.
  bool TreeOnLeft = false;
  // Compare Directories' criteria (ScpCommander.CompareByTime, CompareBySize).
  bool CompareByTime = true;
  bool CompareBySize = false;
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
  bool LocalDriveView = false;
  bool RemoteDriveView = false;
  int LocalDriveViewHeight = 100;
  int RemoteDriveViewHeight = 100;
  int LocalDriveViewHeightPixelsPerInch = 96;
  int RemoteDriveViewHeightPixelsPerInch = 96;
  // The trees' widths when beside the files (DriveViewWidth).
  int LocalDriveViewWidth = 100;
  int RemoteDriveViewWidth = 100;
  int LocalDriveViewWidthPixelsPerInch = 96;
  int RemoteDriveViewWidthPixelsPerInch = 96;
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
  // QueueView.Show (qvShow, qvHideWhenEmpty, qvHide) and ScpCommander.StatusBar.
  int QueueViewShow = 1;
  bool CommanderStatusBar = true;
  // ScpCommander.SessionsTabs, its panels' StatusBar, QueueView.ToolBar.
  bool CommanderSessionsTabs = true;
  bool LocalPanelStatusBar = true;
  bool RemotePanelStatusBar = true;
  bool QueueViewToolBar = true;
  bool QueueViewFileList = false;
  // The panels' ViewStyle (TDirViewStyle: 3 dvsReport, 4 dvsThumbnail).
  int LocalPanelViewStyle = 3;
  int RemotePanelViewStyle = 3;
  int OtherLocalPanelViewStyle = 3;
  // Interface's ConfirmExitOnCompletion (TCustomWinConfiguration's).
  bool ConfirmExitOnCompletion = true;
  // Interface's DefaultToNewRemoteTab (TWinConfiguration's).
  bool DefaultToNewRemoteTab = true;
  // The toolbars (Interface: LockToolbars, SelectiveToolbarText,
  // LargerToolbar; ScpCommander.ToolbarsLayout, its key ToolbarsLayout2).
  bool LockToolbars = false;
  bool SelectiveToolbarText = true;
  int LargerToolbar = 1;
  UnicodeString CommanderToolbarsLayout;
  // ScpCommander.ToolbarsButtons: the bands' hidden buttons
  // ("Band=Item:0,Item:0;Band=...", TCustomScpExplorerForm::
  // GetToolbarsButtonsStr); and the Explorer's.
  UnicodeString CommanderToolbarsButtons;
  // ConsoleWin.WindowSize: "width,height,ppi".
  UnicodeString ConsoleWindowSize = L"570,430,96";
  // LoginDialog.SiteSearch (TIncrementalSearch: isNameStartOnly, isName,
  // isAll), the Login dialog's site search.
  int SiteSearch = 1;
  // Interface\Explorer (TScpExplorerConfiguration), the Explorer
  // interface's: its window, its panel's columns, its toolbars (its key
  // ToolbarsLayout2), status bar and tabs, the view style (TDirViewStyle:
  // dvsIcon 0, dvsSmallIcon, dvsList, dvsReport, dvsThumbnail), the full
  // address, the tree and its width at its pixels per inch, where downloads
  // go (empty: the documents folder).
  UnicodeString ExplorerWindowParams;
  UnicodeString ExplorerDirViewParams = L"0;1;0|150,1;70,1;150,1;79,1;62,1;55,0;20,0;150,0;125,0;@96|6;7;8;0;1;2;3;4;5";
  UnicodeString ExplorerToolbarsLayout;
  UnicodeString ExplorerToolbarsButtons;
  bool ExplorerStatusBar = true;
  bool ExplorerSessionsTabs = true;
  int ExplorerViewStyle = 0;
  bool ExplorerShowFullAddress = true;
  bool ExplorerDriveView = true;
  int ExplorerDriveViewWidth = 180;
  int ExplorerDriveViewWidthPixelsPerInch = 96;
  UnicodeString ExplorerLastLocalTargetDirectory;
  // Environment's confirmations and notifications (Interface; TAutoSwitch:
  // asOn, asOff, asAuto, DDTransferConfirmation's key
  // DDTransferConfirmation2; SynchronizeSummary TCustomWinConfiguration's).
  bool ConfirmTransferring = true;
  bool ConfirmDeleting = true;
  bool ConfirmRecycling = true;
  bool ConfirmClosingSession = true;
  int DDTransferConfirmation = 2;
  bool SynchronizeSummary = true;
  bool BalloonNotifications = true;
  bool FlashTaskbar = true;
  // Interface: TInterface (ifCommander, ifExplorer: linscp has only the
  // first, the value kept), DarkTheme (TAutoSwitch).
  int Interface = 0;
  int DarkTheme = 2;
  // Drag & Drop: the temporary folder's free space checked first, against
  // the size times the ratio; DDFakeFile kept as read (the shell
  // extension's).
  bool DDWarnLackOfTempSpace = true;
  double DDWarnLackOfTempSpaceRatio = 1.1;
  bool DDFakeFile = true;
  // Panels' Local and Remote pages: local deletes to the trash, remote
  // directories that cannot be read shown, the remote panel refreshed
  // while idle every so many seconds (stored as a TDateTime);
  // ScpCommander.PreserveLocalDirectory and SystemContextMenu.
  bool DeleteToRecycleBin = true;
  bool ShowInaccesibleDirectories = true;
  bool RefreshRemotePanel = false;
  int RefreshRemotePanelInterval = 60;
  bool PreserveLocalDirectory = false;
  bool SystemContextMenu = false;
  // Interface\Updates (TUpdatesConfiguration): the period in days (0
  // never), the last check (a TDateTime), the result shown on starting,
  // beta versions (TAutoSwitch), the connection (TConnectionType:
  // ctUndefined -1, ctDirect, ctAuto, ctProxy) and its proxy.
  int UpdatesPeriod = 0;
  double UpdatesLastCheck = 0;
  bool UpdatesShowOnStartup = true;
  int UpdatesBetaVersions = 2;
  int UpdatesConnectionType = -1;
  UnicodeString UpdatesProxyHost;
  int UpdatesProxyPort = 8080;
};

class TCustomCommandList;
class TEditorList;

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
  // TCustomWinConfiguration's histories (History\<Name>: 0, 1, ...), as
  // SaveToHistory keeps them: newest first, once each, at most 30.
  std::vector<UnicodeString> History(const UnicodeString & Name) const;
  void AddToHistory(const UnicodeString & Name, const UnicodeString & Value);
  // As TWinConfiguration::ExpandedTemporaryDirectory.
  UnicodeString ExpandedTemporaryDirectory() const;
  // As TWinConfiguration's: whether temporary folders of the program's
  // are left (TemporaryDir's mask), and their removal (ExtException with
  // those that could not be removed).
  bool AnyTemporaryFolders();
  void CleanupTemporaryFolders();
  // TWinConfiguration::DoFindTemporaryFolders: NULL when there are none.
  TStrings * FindTemporaryFolders(bool OnlyFirst);
  // The bookmarks, kept as TWinConfiguration keeps them (its Bookmarks
  // key): per site, by session key, and shared.
  TBookmarks * Bookmarks() const { return FBookmarks.get(); }
  // The custom commands, as TWinConfiguration keeps them (CustomCommands;
  // its defaults until changed), and their options' values
  // (CustomCommandOptions).
  const TCustomCommandList * CustomCommandList() const { return FCustomCommandList.get(); }
  // The editors files open in, by mask, as TWinConfiguration keeps them
  // (Interface\Editor's subkeys; WinSCP's defaults until changed).
  const TEditorList * EditorList() const { return FEditorList.get(); }
  void SetEditorList(const TEditorList * List);
  void SetCustomCommandList(const TCustomCommandList * List);
  TStrings * CustomCommandOptions() const { return FCustomCommandOptions.get(); }
  void SetCustomCommandOptions(TStrings * Options);
  // TWinConfiguration's, for the custom commands' slice: an extension's
  // string in the user interface's language, from its translation's
  // EXTENSION_STRINGS.
  UnicodeString __fastcall ExtensionStringTranslation(const UnicodeString & ExtensionId, const UnicodeString & S);
  // The extensions, as TWinConfiguration has them: each *.WinSCPextension.*
  // file of the program's own folder ("common"), its Extensions folder
  // ("commonext"; on POSIX share/linscp/Extensions beside bin) and the
  // user's ("userext", in the configuration's folder), the deleted ones
  // that could not be removed skipped, in the order kept, with their keys
  // (ExtensionsDeleted, ExtensionsOrder, ExtensionsShortCuts).
  const TCustomCommandList * ExtensionList() const { return FExtensionList.get(); }
  void SetExtensionList(const TCustomCommandList * List);
  void LoadExtensionList();
  UnicodeString GetUserExtensionsPath();
  TStrings * GetExtensionsPaths();
  UnicodeString GetExtensionId(const UnicodeString & ExtensionPath);
  UnicodeString GetProvisionaryExtensionId(const UnicodeString & FileName);
  UnicodeString UniqueExtensionName(const UnicodeString & ExtensionName, int Counter);

  // The master password, as TWinConfiguration's: stored passwords
  // encrypted with it (AES, StronglyRecryptPassword), decrypted as read
  // (DecryptPassword), asking for it through OnMasterPasswordPrompt
  // (which sets it, else aborts). Its verifier is kept under Security.
  bool UseMasterPassword() const { return FUseMasterPassword; }
  void SetMasterPassword(const UnicodeString & Value);
  void ChangeMasterPassword(const UnicodeString & Value, TStrings * RecryptPasswordErrors);
  bool ValidateMasterPassword(const UnicodeString & Value);
  void ClearMasterPassword(TStrings * RecryptPasswordErrors);
  void AskForMasterPassword();
  void AskForMasterPasswordIfNotSet();
  // Asked once at most until the session ends (as the Login dialog has it).
  void BeginMasterPasswordSession();
  void EndMasterPasswordSession();
  // How often it was not given: core reads a password it could not
  // decrypt as an empty one (TSessionData::DecryptPassword).
  int MasterPasswordRefusals() const { return FMasterPasswordRefusals; }
  std::function<void()> OnMasterPasswordPrompt;
  // The passwords of sessions open, re-encrypted too (TTerminalManager's).
  std::function<void()> OnMasterPasswordRecrypt;
  virtual UnicodeString __fastcall DecryptPassword(RawByteString Password, UnicodeString Key);
  virtual RawByteString __fastcall StronglyRecryptPassword(RawByteString Password, UnicodeString Key);

protected:
  virtual void __fastcall SaveData(THierarchicalStorage * Storage, bool All);
  virtual void __fastcall LoadData(THierarchicalStorage * Storage);
  virtual void __fastcall Saved();
  virtual void __fastcall DefaultLocalized();
  // TGUIConfiguration's, with PuttyPath's POSIX default (PosixPuttyPath).
  virtual void __fastcall Default();

private:

  bool FOverridden = false;
  LCID FSavedLocale = 0;
  LCID FDefaultLocale = 0;
  UnicodeString FModuleFileName;
  std::map<UnicodeString, std::vector<UnicodeString>> FHistory;
  TShellSettings FSettings;
  std::unique_ptr<TBookmarks> FBookmarks;
  std::unique_ptr<TCustomCommandList> FCustomCommandList;
  std::unique_ptr<TCustomCommandList> FExtensionList;
  UnicodeString FExtensionsDeleted;
  UnicodeString FExtensionsOrder;
  UnicodeString FExtensionsShortCuts;
  std::unique_ptr<TStringList> FExtensionTranslations;
  void LoadExtensionTranslations();
  void DoLoadExtensionList(const UnicodeString & Path, const UnicodeString & PathId, TStringList * DeletedExtensions);
  std::unique_ptr<TEditorList> FEditorList;
  void DefaultEditorList();
  bool FCustomCommandsDefaults = true;
  std::unique_ptr<TStringList> FCustomCommandOptions;
  bool FCustomCommandOptionsModified = false;
  void RecryptPasswords(TStrings * RecryptPasswordErrors);
  bool FUseMasterPassword = false;
  UnicodeString FMasterPasswordVerifier;
  UnicodeString FPlainMasterPasswordEncrypt;
  UnicodeString FPlainMasterPasswordDecrypt;
  int FMasterPasswordSession = 0;
  bool FMasterPasswordSessionAsked = false;
  int FMasterPasswordRefusals = 0;
};

#endif
