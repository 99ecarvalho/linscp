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
  enum { NameColumn, SizeColumn, ModifiedColumn, RightsColumn, OwnerColumn, GroupColumn, LinkTargetColumn, ExtColumn,
    ColumnCount };
  // What the sort proxy compares; directories first is the proxy's.
  enum { SortRole = Qt::UserRole, DirectoryRole, ParentRole, HiddenRole };

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
  // The columns as WinSCP's panels have them: group and link target hidden
  // until shown; the header's context menu shows and hides each but the
  // name. To call once the model is set.
  void SetupColumns();
  // MenuCaptions: the main menu's names for them, else the headers'.
  QMenu * ColumnsMenu(bool MenuCaptions = false);
  // The columns' layout as WinSCP keeps it (TIEListViewColProperties'
  // params: "sort;ascending|width,visible;...;@ppi|order"), over WinSCP's
  // columns: Map gives each of the model's columns its WinSCP index (-1:
  // none), out of Count. What WinSCP has and the view does not (its
  // extension and type columns) is kept as it was given.
  void SetColumnsParams(const QString & Params, const std::vector<int> & Map, int Count);
  QString ColumnsParams(const std::vector<int> & Map, int Count) const;

signals:
  void Dropped(const QMimeData * Data, const QString & OnDirectory, Qt::DropAction Action);
  // Tab: to the other panel, as WinSCP's Commander.
  void SwitchPanel();

protected:
  bool event(QEvent * Event) override;
  void keyPressEvent(QKeyEvent * Event) override;
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
  static bool FLastDirectories;
  // WinSCP's entries as last given, for the columns the view does not have.
  QStringList FColumnEntries;
  QStringList FColumnOrder;
};

#endif
