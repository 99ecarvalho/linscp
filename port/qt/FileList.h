// What both file panels list: a directory's entries -- core's for the remote
// panel, the file system's for the local one -- in WinSCP's columns, with
// ".." first below the root, sorted directories first (DirectoriesFirst) as
// WinSCP's panels sort.
#ifndef LINSCP_QT_FILE_LIST_H
#define LINSCP_QT_FILE_LIST_H

#include <QAbstractTableModel>
#include <QCollator>
#include <QColor>
#include <QMimeData>
class QMenu;
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QHash>
#include <QListView>
#include <QPixmap>
#include <QSet>
#include <QTreeView>

#include <functional>
#include <vector>

#include "CoreBridge.h"

// The parent directory's entry.
extern const char * const ParentEntryName;

class FileListModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  // ExtColumn: WinSCP's extension column (uvExt, dvExt), hidden by default.
  // TypeColumn: its type column (uvType, dvType), the file type's name.
  enum { NameColumn, SizeColumn, ModifiedColumn, RightsColumn, OwnerColumn, GroupColumn, LinkTargetColumn, ExtColumn,
    TypeColumn, ColumnCount };
  // What the sort proxy compares; directories first is the proxy's.
  enum { SortRole = Qt::UserRole, DirectoryRole, ParentRole, HiddenRole, InaccessibleRole };

  using QAbstractTableModel::QAbstractTableModel;
  void SetEntries(std::vector<CoreBridge::TRemoteEntry> Entries, bool WithParent);
  const CoreBridge::TRemoteEntry * Entry(int Row) const;
  // Files > Calculate Directory Sizes: a directory's size shown in its row,
  // until the list is set again. False when it has no such directory.
  bool SetDirectorySize(const QString & Name, std::int64_t Size);
  bool IsParent(int Row) const { return FWithParent && (Row == 0); }
  // The rights column as file attributes (a Windows local panel's).
  void SetAttributes(bool Attributes) { FAttributes = Attributes; }
  // "Show file sizes in" (Preferences, Panels): kilobytes by default.
  void SetSizeFormat(CoreBridge::TPreferences::TSizeFormat Format);
  // File colors (Preferences): the first rule an entry's full path (Path
  // gives it) matches colors it.
  void SetFileColors(const std::vector<CoreBridge::TPreferences::TFileColor> & Colors, bool Local,
    std::function<std::string(const CoreBridge::TRemoteEntry &)> Path);

  int rowCount(const QModelIndex & Parent = QModelIndex()) const override;
  int columnCount(const QModelIndex & Parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & Index, int Role = Qt::DisplayRole) const override;
  QVariant headerData(int Section, Qt::Orientation Orientation, int Role = Qt::DisplayRole) const override;
  // A name edited in place (F2) is a rename asked for, not a change of the
  // list: the panel renames, then lists again.
  Qt::ItemFlags flags(const QModelIndex & Index) const override;
  bool setData(const QModelIndex & Index, const QVariant & Value, int Role = Qt::EditRole) override;

signals:
  void RenameRequested(const QString & Name, const QString & NewName);

private:
  std::vector<CoreBridge::TRemoteEntry> FEntries;
  // The directories whose size has been calculated, by index.
  std::vector<bool> FSized;
  bool FWithParent = false;
  bool FAttributes = false;
  CoreBridge::TPreferences::TSizeFormat FSizeFormat = CoreBridge::TPreferences::TSizeFormat::Kilobytes;
  void UpdateColors();
  std::vector<CoreBridge::TPreferences::TFileColor> FFileColorRules;
  bool FColorsLocal = false;
  std::function<std::string(const CoreBridge::TRemoteEntry &)> FColorPath;
  // Each entry's color; invalid: none.
  std::vector<QColor> FColors;
};

// Directories (and "..") first, as WinSCP's panels sort; then the column.
class DirectoriesFirst : public QSortFilterProxyModel
{
public:
  explicit DirectoriesFirst(QObject * Parent = nullptr);
  // WinSCP's "Show hidden files" (Ctrl+Alt+H); off by default, as WinSCP's.
  void SetShowHidden(bool Show);
  bool ShowHidden() const { return FShowHidden; }
  // Panels > Remote's "Show inaccessible directories" (on by default).
  void SetShowInaccessible(bool Show);
  // Preferences, Panels: "use natural order numerical sorting" (on by
  // default) and "always sort directories by name".
  void SetSorting(bool Natural, bool DirectoriesByName);
  // A panel's filter (Ctrl+Alt+F), as WinSCP's (TCustomDirView's Mask):
  // only what matches is shown; "" shows all.
  void SetMask(const QString & Mask);
  QString Mask() const { return FMask; }

protected:
  bool lessThan(const QModelIndex & Left, const QModelIndex & Right) const override;
  bool filterAcceptsRow(int Row, const QModelIndex & Parent) const override;

private:
  int CompareText(const QString & A, const QString & B) const;

  bool FShowHidden = false;
  bool FShowInaccessible = true;
  bool FNatural = true;
  bool FDirectoriesByName = false;
  QString FMask;
  QCollator FCollator;
};

// What a panel's drag carries (application/x-linscp-files): which side the
// entries are on, their directory and names. A local drag carries their file
// URLs too, for other programs.
struct TFileDrag
{
  bool Local = false;
  QString Directory;
  QStringList Names;
};
extern const char * const FileDragMimeType;
QMimeData * MakeFileDrag(const TFileDrag & Drag);
// false when Data is not a panel's drag.
bool ReadFileDrag(const QMimeData * Data, TFileDrag & Drag);

// A remote panel's drag, for other programs too, as WinSCP's drag to a
// temporary directory: the files' URLs in Directory, downloaded there by
// Download when a program asks for them, at its drop. (On Windows, Explorer
// asks while the drag hovers too; with a mouse button still down the URLs
// are given without downloading, as WinSCP gives the names up front.)
class RemoteFileDrag : public QMimeData
{
public:
  RemoteFileDrag(const TFileDrag & Drag, const QString & Directory, std::function<bool()> Download);
  QStringList formats() const override;
  bool hasFormat(const QString & MimeType) const override;

protected:
  QVariant retrieveData(const QString & MimeType, QMetaType Type) const override;

private:
  QStringList FNames;
  QString FDirectory;
  std::function<bool()> FDownload;
  mutable bool FDownloading = false;
  mutable bool FDownloaded = false;
};

// As WinSCP's panels show sizes (BaseUtils.pas' FormatBytes, its default
// short style): bytes below 1 KB, then KB, MB or GB to three digits.
QString FormatBytes(std::int64_t Bytes);
// As FormatPanelBytes: what a panel's size column shows.
QString FormatPanelBytes(std::int64_t Bytes, CoreBridge::TPreferences::TSizeFormat Format);

// A panel's view: its selection dragged as the panel says, and drops onto it
// reported with the directory row they fell on ("" for the panel's own
// directory; ".." for its parent).
// The mask dialog, as WinSCP's TSelectMaskDialog in its modes: Select and
// Deselect (with "apply to directories"), Filter (with Clear); each with
// Edit... (the Edit file mask dialog). False when cancelled.
enum class TMaskDialogMode { Select, Deselect, Filter };
bool MaskDialog(QWidget * Parent, TMaskDialogMode Mode, QString & Mask, bool * Directories = nullptr);

class FileView : public QTreeView
{
  Q_OBJECT

public:
  explicit FileView(QWidget * Parent = nullptr);
  // The drag of the selection; null for none.
  std::function<QMimeData *()> MakeDrag;
  // A drag started is given here instead of to the system (tests), which
  // owns the data.
  std::function<void(QMimeData *)> OnDragStarted;

  // WinSCP's selection commands, as its keys do them: Insert, the
  // focused entry's selection toggled and the focus down; the keypad's *,
  // the selection inverted; + and -, entries selected or unselected by a
  // file mask (after asking for it, unless given); Ctrl+Shift+L, none.
  void ToggleCurrent();
  void InvertSelection();
  // False when the mask is not one (the error is shown).
  bool SelectByMask(bool Select, const QString & Masks, bool Directories);
  void SelectByMaskDialog(bool Select);
  // The mask last given (WinConfiguration's SelectMask), which Find files
  // shares.
  static QString LastMask() { return FLastMask; }
  static void SetLastMask(const QString & Mask) { FLastMask = Mask; }
  // F2, as WinSCP's Commander renames: the focused entry's name edited in
  // place. False when there is none ("..").
  bool EditCurrentName();
  // Preferences, Panels: the whole name selected for renaming, rather than
  // the name without its extension (WinSCP's default).
  bool RenameWholeName = false;
  // The focused entry; null for none or "..".
  const CoreBridge::TRemoteEntry * CurrentEntry() const;
  // The panel's status line, as WinSCP's (FileStatusBarText): the selected
  // entries' size of all, and how many of how many; and how many hidden.
  QString StatusText() const;
  // The columns as WinSCP's panels have them: group, link target and
  // extension hidden until shown, and the type unless TypeShown (WinSCP's
  // local panel shows it, its remote one does not); the header's context
  // menu shows and hides each but the name. To call once the model is set:
  // what it sets is the layout Reset Layout goes back to.
  void SetupColumns(bool TypeShown = false);
  // MenuCaptions: the main menu's Columns (NonVisual.dfm's ShowHide...
  // ColumnActions, in WinSCP's order, then Size Automatically and Reset
  // Layout), else the header's. Checked as the columns are when shown.
  QMenu * ColumnsMenu(bool MenuCaptions = false);
  // Columns > Size Automatically: each shown column as wide as its content.
  void SizeColumnsAutomatically();
  // Columns > Reset Layout (ResetLayoutColumns): the columns as SetupColumns
  // left them.
  void ResetLayout();
  // The panel menus' Sort (NonVisual.dfm's ...SortBy...Actions): Ascending,
  // then by each column; Local, the local panel's (attributes, no owner).
  QMenu * SortMenu(bool Local);
  // Mark > Restore Selection, as TCustomDirView's SaveSelectedNames and
  // RestoreSelectedNames: the names selected when an operation started
  // (a transfer, Properties, a custom command), selected again; forgotten
  // when the directory changes.
  void SaveSelectedNames();
  bool SelectedNamesSaved() const { return !FSavedNames.isEmpty(); }
  void RestoreSelectedNames();
  void ForgetSelectedNames() { FSavedNames.clear(); }
  // Preferences > Panels and Commander (P3-16): Explorer-style selection
  // (Never: the focus alone moves with the keys and plain clicks; MouseOnly,
  // WinSCP's default: with the keys only), full row selection,
  // incremental search (by name's start, any part of it, all columns).
  void SetExplorerSelection(CoreBridge::TPreferences::TExplorerSelection Selection);
  void SetFullRowSelect(bool FullRow);
  void SetPanelSearch(CoreBridge::TPreferences::TPanelSearch Search) { FPanelSearch = Search; }
  // What incremental search has typed (empty: none).
  QString SearchText() const { return FSearch; }
  void keyboardSearch(const QString & Search) override;
  // The columns' layout as WinSCP keeps it (TIEListViewColProperties'
  // params: "sort;ascending|width,visible;...;@ppi|order"), over WinSCP's
  // columns: Map gives each of the model's columns its WinSCP index (-1:
  // none), out of Count. What WinSCP has and the view does not (its
  // extension and type columns) is kept as it was given.
  void SetColumnsParams(const QString & Params, const std::vector<int> & Map, int Count);
  QString ColumnsParams(const std::vector<int> & Map, int Count) const;

signals:
  // Incremental search's text changed (empty: it ended).
  void SearchChanged(const QString & Text);
  void Dropped(const QMimeData * Data, const QString & OnDirectory, Qt::DropAction Action);
  // Tab: to the other panel, as WinSCP's Commander.
  void SwitchPanel();

protected:
  bool event(QEvent * Event) override;
  void keyPressEvent(QKeyEvent * Event) override;
  QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex & Index, const QEvent * Event = nullptr) const override;
  void focusOutEvent(QFocusEvent * Event) override;
  void startDrag(Qt::DropActions Actions) override;
  void dragEnterEvent(QDragEnterEvent * Event) override;
  void dragMoveEvent(QDragMoveEvent * Event) override;
  void dropEvent(QDropEvent * Event) override;

private:
  QString DirectoryAt(const QPoint & Pos) const;
  // The listed entry at a row of the view; null for "..".
  const CoreBridge::TRemoteEntry * EntryAt(int Row) const;
  // As WinSCP's Select dialog remembers them (WinConfiguration's
  // SelectMask and SelectDirectories), for the session.
  static QString FLastMask;
  CoreBridge::TPreferences::TExplorerSelection FExplorerSelection = CoreBridge::TPreferences::TExplorerSelection::MouseOnly;
  CoreBridge::TPreferences::TPanelSearch FPanelSearch = CoreBridge::TPreferences::TPanelSearch::NameStart;
  QString FSearch;
  class TPanelDelegate * FDelegate = nullptr;
  QByteArray FDefaultLayout;
  QStringList FSavedNames;
  void EndSearch();
  static bool FLastDirectories;
  // WinSCP's entries as last given, for the columns the view does not have.
  QStringList FColumnEntries;
  QStringList FColumnOrder;
};

// View > Thumbnails (dvsThumbnail): the panel's entries as a grid, an
// image's own preview where ImagePath gives its file (loaded in the
// background, 128 pixels at 96 DPI, as TCustomDirView's ThumbnailSize),
// else its type's icon. The same model and selection as the panel's
// FileView, which keeps doing the panel's commands.
class ThumbnailView : public QListView
{
  Q_OBJECT

public:
  explicit ThumbnailView(FileView * Details, QWidget * Parent = nullptr);
  // An entry's local image to preview; empty: none (yet).
  std::function<QString(const CoreBridge::TRemoteEntry &)> ImagePath;
  // Local: type icons from the files themselves, else by their names.
  bool Local = true;
  // The previews forgotten (the directory changed).
  void Forget();
  // Whether a preview of the file is loaded (tests).
  bool HasPreview(const QString & Path) const { return FPreviews.contains(Path); }
  int ThumbnailSize() const { return FSize; }
  const CoreBridge::TRemoteEntry * EntryAt(const QModelIndex & Index) const;
  // The preview, or null; a missing one is asked for.
  QPixmap Preview(const QString & Path);

private:
  FileView * FDetails;
  int FSize;
  QHash<QString, QPixmap> FPreviews;
  QSet<QString> FPending;
};

#endif
