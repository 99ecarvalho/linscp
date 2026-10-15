#include "DirectoryTree.h"

#include <QDir>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>

namespace
{

const int PathRole = Qt::UserRole + 1;
const int LoadedRole = Qt::UserRole + 2;

// Chosen by the user only: not while the panel's directory is being shown.
void ConnectChoice(QTreeView * Tree, const bool & Showing, const std::function<QString()> & Current,
  const std::function<void(const QString &)> & Emit)
{
  QObject::connect(Tree->selectionModel(), &QItemSelectionModel::currentChanged, Tree,
    [Tree, &Showing, Current, Emit](const QModelIndex & Index)
  {
    if (!Showing && Index.isValid())
    {
      // Once the selection settles (a key held down moves through many).
      QTimer::singleShot(0, Tree, [Current, Emit]()
      {
        const QString Path = Current();
        if (!Path.isEmpty())
        {
          Emit(Path);
        }
      });
    }
  });
}

}  // namespace

RemoteDirectoryTree::RemoteDirectoryTree(QWidget * Parent) :
  QTreeView(Parent)
{
  FModel = new QStandardItemModel(this);
  setModel(FModel);
  setHeaderHidden(true);
  setUniformRowHeights(true);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setAccessibleName(tr("Remote directory tree"));
  connect(this, &QTreeView::expanded, this, [this](const QModelIndex & Index)
  {
    Load(FModel->itemFromIndex(Index));
  });
  ConnectChoice(this, FShowing, [this]() { return CurrentPath(); }, [this](const QString & Path) { emit DirectoryChosen(Path); });
}

QStandardItem * RemoteDirectoryTree::MakeItem(const QString & Name, const QString & Path)
{
  QStandardItem * Item = new QStandardItem(style()->standardIcon(QStyle::SP_DirIcon), Name);
  Item->setData(Path, PathRole);
  Item->setData(false, LoadedRole);
  // Expandable until loaded (TUnixDriveView's HasChildren, not known yet).
  Item->appendRow(new QStandardItem());
  return Item;
}

void RemoteDirectoryTree::SetSession(CoreBridge::TSession * Session)
{
  FSession = Session;
  FModel->clear();
  if ((FSession != nullptr) && FSession->Active())
  {
    FModel->appendRow(MakeItem(QStringLiteral("/"), QStringLiteral("/")));
  }
}

void RemoteDirectoryTree::Reload()
{
  const QString Path = CurrentPath();
  SetSession(FSession);
  if (!Path.isEmpty())
  {
    ShowPath(Path);
  }
}

void RemoteDirectoryTree::Load(QStandardItem * Item)
{
  if ((Item == nullptr) || Item->data(LoadedRole).toBool() || (FSession == nullptr) || !FSession->Active())
  {
    return;
  }
  Item->setData(true, LoadedRole);
  Item->removeRows(0, Item->rowCount());
  const QString Path = Item->data(PathRole).toString();
  try
  {
    for (const std::string & Name : FSession->Subdirectories(Path.toStdString()))
    {
      const QString AName = QString::fromStdString(Name);
      Item->appendRow(MakeItem(AName, (Path.endsWith(QLatin1Char('/')) ? Path : (Path + QLatin1Char('/'))) + AName));
    }
  }
  catch (const CoreBridge::TCoreError &)
  {
    // Unreadable: no subfolders shown, as WinSCP's tree.
  }
}

QStandardItem * RemoteDirectoryTree::Child(QStandardItem * Parent, const QString & Name)
{
  Load(Parent);
  for (int Row = 0; Row < Parent->rowCount(); Row++)
  {
    if (Parent->child(Row)->text() == Name)
    {
      return Parent->child(Row);
    }
  }
  // Not listed (hidden, say): there all the same, as the panel is in it.
  const QString Path = Parent->data(PathRole).toString();
  QStandardItem * Item = MakeItem(Name, (Path.endsWith(QLatin1Char('/')) ? Path : (Path + QLatin1Char('/'))) + Name);
  Parent->appendRow(Item);
  return Item;
}

void RemoteDirectoryTree::ShowPath(const QString & Path)
{
  if ((FModel->rowCount() == 0) || !Path.startsWith(QLatin1Char('/')))
  {
    return;
  }
  FShowing = true;
  QStandardItem * Item = FModel->item(0);
  for (const QString & Part : Path.split(QLatin1Char('/'), Qt::SkipEmptyParts))
  {
    expand(Item->index());
    Item = Child(Item, Part);
  }
  setCurrentIndex(Item->index());
  scrollTo(Item->index());
  FShowing = false;
}

QString RemoteDirectoryTree::CurrentPath() const
{
  const QModelIndex Index = currentIndex();
  return Index.isValid() ? Index.data(PathRole).toString() : QString();
}

LocalDirectoryTree::LocalDirectoryTree(QWidget * Parent) :
  QTreeView(Parent)
{
  FModel = new QFileSystemModel(this);
  FModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
  // The whole file system: on Windows its drives at the top, elsewhere "/".
  FModel->setRootPath(QString());
  setModel(FModel);
  for (int Column = 1; Column < FModel->columnCount(); Column++)
  {
    hideColumn(Column);
  }
  setHeaderHidden(true);
  setUniformRowHeights(true);
  setAccessibleName(tr("Local directory tree"));
  ConnectChoice(this, FShowing, [this]() { return CurrentPath(); }, [this](const QString & Path) { emit DirectoryChosen(Path); });
}

void LocalDirectoryTree::SetShowHidden(bool Show)
{
  FModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives | (Show ? QDir::Hidden : QDir::Filters()));
}

void LocalDirectoryTree::ShowPath(const QString & Path)
{
  const QModelIndex Index = FModel->index(QDir::fromNativeSeparators(Path));
  if (!Index.isValid())
  {
    return;
  }
  FShowing = true;
  for (QModelIndex Parent = Index.parent(); Parent.isValid(); Parent = Parent.parent())
  {
    expand(Parent);
  }
  setCurrentIndex(Index);
  scrollTo(Index);
  FShowing = false;
}

QString LocalDirectoryTree::CurrentPath() const
{
  const QModelIndex Index = currentIndex();
  return Index.isValid() ? FModel->filePath(Index) : QString();
}
