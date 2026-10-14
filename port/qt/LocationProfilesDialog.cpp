#include "LocationProfilesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "OpenDirectoryDialog.h"
#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

const int IdRole = Qt::UserRole;
const int FolderRole = Qt::UserRole + 1;

// BookmarkNameValidateName and BookmarkFolderValidateName.
bool ValidName(const QString & Name)
{
  bool Number = false;
  Name.toLongLong(&Number);
  return !Name.isEmpty() && !Number;
}

bool ValidFolder(const QString & Name, bool AllowEmpty)
{
  return (AllowEmpty || !Name.isEmpty()) && !Name.contains(QLatin1Char('\\'));
}

}  // namespace

LocationProfilesDialog::LocationProfilesDialog(const QString & LocalDirectory, const QString & RemoteDirectory,
    const QStringList & LocalHistory, const QStringList & RemoteHistory, bool Remote, const std::string & SessionKey,
    bool AddBookmark, QWidget * Parent) :
  QDialog(Parent),
  FRemoteSide(Remote),
  FSessionKey(SessionKey)
{
  setWindowTitle(tr("Location profiles"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Fields = new QFormLayout();
  FLocal = new QComboBox(this);
  FLocal->setEditable(true);
  FLocal->addItems(LocalHistory);
  FLocal->setEditText(LocalDirectory);
  FLocal->setMinimumWidth(FLocal->fontMetrics().horizontalAdvance(QLatin1Char('x')) * 50);
  QHBoxLayout * LocalRow = new QHBoxLayout();
  LocalRow->addWidget(FLocal, 1);
  QPushButton * Browse = new QPushButton(tr("B&rowse..."), this);
  connect(Browse, &QPushButton::clicked, this, [this]()
  {
    const QString Chosen = QFileDialog::getExistingDirectory(this, QString(), FLocal->currentText());
    if (!Chosen.isEmpty())
    {
      FLocal->setEditText(QDir::toNativeSeparators(Chosen));
    }
  });
  LocalRow->addWidget(Browse);
  Fields->addRow(tr("&Local directory:"), LocalRow);
  FRemote = new QComboBox(this);
  FRemote->setEditable(true);
  FRemote->addItems(RemoteHistory);
  FRemote->setEditText(RemoteDirectory);
  Fields->addRow(tr("&Remote directory:"), FRemote);
  Layout->addLayout(Fields);
  FPages = new QTabWidget(this);
  FPageWidgets[0] = MakePage(0);
  FPageWidgets[1] = MakePage(1);
  FPages->addTab(FPageWidgets[0], tr("Site Location profiles"));
  FPages->addTab(FPageWidgets[1], tr("Shared Location profiles"));
  Layout->addWidget(FPages, 1);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  QPushButton * Switch = Buttons->addButton(tr("&Bookmarks..."), QDialogButtonBox::ResetRole);
  connect(Switch, &QPushButton::clicked, this, [this]()
  {
    FSwitched = true;
    reject();
  });
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);

  for (int Index = 0; Index < 2; Index++)
  {
    for (const CoreBridge::TBookmarkEntry & Entry : CoreBridge::Bookmarks((Index == 0) ? SessionKey : std::string()))
    {
      FProfiles[Index].push_back({ Entry, FNextId++ });
    }
    LoadTree(Index, 0);
  }
  FPages->setCurrentWidget(FPageWidgets[CoreBridge::UseSharedBookmarks() ? 1 : 0]);
  for (QComboBox * Combo : { FLocal, FRemote })
  {
    connect(Combo, &QComboBox::editTextChanged, this, [this]()
    {
      if (!FChanging)
      {
        FindProfile();
        UpdateControls();
      }
    });
  }
  if (AddBookmark)
  {
    QString Name = (FRemoteSide && !RemoteDirectory.isEmpty()) || LocalDirectory.isEmpty() ? RemoteDirectory : LocalDirectory;
    bool Shared = CoreBridge::UseSharedBookmarks();
    if (AskName(Page(), Name, true, Shared))
    {
      AddProfile(Page(), Name, true, Shared);
    }
  }
  FindProfile();
  (FRemoteSide ? FRemote : FLocal)->setFocus();
  UpdateControls();
}

QWidget * LocationProfilesDialog::MakePage(int Index)
{
  QWidget * Widget = new QWidget(this);
  QHBoxLayout * Layout = new QHBoxLayout(Widget);
  FTrees[Index] = new QTreeWidget(Widget);
  FTrees[Index]->setHeaderHidden(true);
  FTrees[Index]->installEventFilter(this);
  Layout->addWidget(FTrees[Index], 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  FAdd[Index] = new QPushButton(tr("&Add..."), Widget);
  FRemove[Index] = new QPushButton(tr("Remo&ve"), Widget);
  FRename[Index] = new QPushButton(tr("Re&name"), Widget);
  FMoveTo[Index] = new QPushButton(tr("&Move To..."), Widget);
  FUp[Index] = new QPushButton(tr("&Up"), Widget);
  FDown[Index] = new QPushButton(tr("&Down"), Widget);
  for (QPushButton * Button : { FAdd[Index], FRemove[Index], FRename[Index], FMoveTo[Index], FUp[Index], FDown[Index] })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  Layout->addLayout(Buttons);
  connect(FAdd[Index], &QPushButton::clicked, this, [this, Index]()
  {
    QString Name = (FRemoteSide && !FRemote->currentText().isEmpty()) || FLocal->currentText().isEmpty() ?
      FRemote->currentText() : FLocal->currentText();
    bool Shared = false;
    if (AskName(Index, Name, false, Shared))
    {
      AddProfile(Index, Name);
    }
  });
  connect(FRemove[Index], &QPushButton::clicked, this, [this, Index]() { RemoveSelected(Index); });
  connect(FRename[Index], &QPushButton::clicked, this, [this, Index]()
  {
    const int Selected = SelectedIndex(Index);
    const QString Old = (Selected >= 0) ? QString::fromStdString(FProfiles[Index][Selected].Entry.Name) : SelectedFolder(Index);
    bool OK = false;
    const QString Name = QInputDialog::getText(this, tr("Re&name"), CoreText(CoreBridge::TText::AddBookmarkPrompt),
      QLineEdit::Normal, Old, &OK);
    if (OK)
    {
      RenameSelected(Index, Name);
    }
  });
  connect(FMoveTo[Index], &QPushButton::clicked, this, [this, Index]()
  {
    const int Selected = SelectedIndex(Index);
    if (Selected < 0)
    {
      return;
    }
    QStringList Folders;
    for (const TProfile & Profile : FProfiles[Index])
    {
      if (!Profile.Entry.Node.empty())
      {
        Folders.append(QString::fromStdString(Profile.Entry.Node));
      }
    }
    Folders.removeDuplicates();
    Folders.sort(Qt::CaseInsensitive);
    bool OK = false;
    const QString Folder = QInputDialog::getItem(this, CoreText(CoreBridge::TText::MoveBookmarkCaption),
      CoreText(CoreBridge::TText::MoveBookmarkPrompt), Folders,
      Folders.indexOf(QString::fromStdString(FProfiles[Index][Selected].Entry.Node)), true, &OK);
    if (OK)
    {
      MoveSelectedTo(Index, Folder);
    }
  });
  connect(FUp[Index], &QPushButton::clicked, this, [this, Index]() { MoveSelected(Index, -1); });
  connect(FDown[Index], &QPushButton::clicked, this, [this, Index]() { MoveSelected(Index, 1); });
  connect(FTrees[Index], &QTreeWidget::currentItemChanged, this, [this, Index]()
  {
    if (!FChanging)
    {
      Selected(Index);
    }
  });
  connect(FTrees[Index], &QTreeWidget::itemDoubleClicked, this, [this, Index](QTreeWidgetItem * Item)
  {
    if (FOk->isEnabled() && (Item != nullptr) && (Item->data(0, IdRole).toInt() != 0))
    {
      accept();
    }
  });
  return Widget;
}

int LocationProfilesDialog::Page() const
{
  return (FPages->currentWidget() == FPageWidgets[1]) ? 1 : 0;
}

QString LocationProfilesDialog::ProfileText(const CoreBridge::TBookmarkEntry & Entry) const
{
  const QString Name = QString::fromStdString(Entry.Name);
  const QString ShortCut = ShortCutText(Entry.ShortCut);
  return ShortCut.isEmpty() ? Name : QStringLiteral("%1 (%2)").arg(Name, ShortCut);
}

// As TLocationProfilesDialog::LoadBookmarks: the folders first, sorted,
// then the profiles in their order.
void LocationProfilesDialog::LoadTree(int Index, int SelectId, const QString & SelectFolder)
{
  FChanging = true;
  QTreeWidget * Tree = FTrees[Index];
  Tree->clear();
  QStringList Folders;
  for (const TProfile & Profile : FProfiles[Index])
  {
    if (!Profile.Entry.Node.empty())
    {
      Folders.append(QString::fromStdString(Profile.Entry.Node));
    }
  }
  Folders.removeDuplicates();
  Folders.sort(Qt::CaseInsensitive);
  QList<QTreeWidgetItem *> FolderItems;
  for (const QString & Folder : Folders)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(Tree, { Folder });
    Item->setData(0, IdRole, 0);
    Item->setData(0, FolderRole, Folder);
    FolderItems.append(Item);
  }
  QTreeWidgetItem * Select = nullptr;
  for (const TProfile & Profile : FProfiles[Index])
  {
    const QString Folder = QString::fromStdString(Profile.Entry.Node);
    QTreeWidgetItem * Parent = Folder.isEmpty() ? nullptr : FolderItems[Folders.indexOf(Folder)];
    QTreeWidgetItem * Item = (Parent != nullptr) ? new QTreeWidgetItem(Parent) : new QTreeWidgetItem(Tree);
    Item->setText(0, ProfileText(Profile.Entry));
    Item->setData(0, IdRole, Profile.Id);
    Item->setData(0, FolderRole, Folder);
    Item->setToolTip(0, CoreText(CoreBridge::TText::BookmarkInfoTip)
      .replace(QStringLiteral("\\n"), QStringLiteral("\n"))
      .replace(QStringLiteral("%s"), QString::fromStdString(Profile.Entry.Local))
      .replace(QStringLiteral("%s"), QString::fromStdString(Profile.Entry.Remote)));
    if (Profile.Id == SelectId)
    {
      Select = Item;
    }
  }
  for (QTreeWidgetItem * Item : FolderItems)
  {
    Item->setExpanded(true);
    if ((SelectId == 0) && !SelectFolder.isEmpty() && (Item->text(0) == SelectFolder))
    {
      Select = Item;
    }
  }
  Tree->setCurrentItem(Select);
  FChanging = false;
}

int LocationProfilesDialog::SelectedIndex(int Index) const
{
  const QTreeWidgetItem * Item = FTrees[Index]->currentItem();
  const int Id = (Item != nullptr) ? Item->data(0, IdRole).toInt() : 0;
  for (int Position = 0; (Id != 0) && (Position < static_cast<int>(FProfiles[Index].size())); Position++)
  {
    if (FProfiles[Index][Position].Id == Id)
    {
      return Position;
    }
  }
  return -1;
}

QString LocationProfilesDialog::SelectedFolder(int Index) const
{
  const QTreeWidgetItem * Item = FTrees[Index]->currentItem();
  return (Item != nullptr) ? Item->data(0, FolderRole).toString() : QString();
}

const std::vector<CoreBridge::TBookmarkEntry> LocationProfilesDialog::Profiles(int Index) const
{
  std::vector<CoreBridge::TBookmarkEntry> Result;
  for (const TProfile & Profile : FProfiles[Index])
  {
    Result.push_back(Profile.Entry);
  }
  return Result;
}

void LocationProfilesDialog::Error(const QString & Message)
{
  CoreBridge::TError Error;
  Error.Message = Message.toStdString();
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  Frontend.ShowError(Error);
}

// TBookmarkNameDialog: the name, the folder's other profiles offered, and
// at first whether shared.
bool LocationProfilesDialog::AskName(int Index, QString & Name, bool Initial, bool & Shared)
{
  QDialog Dialog(this);
  Dialog.setWindowTitle(CoreText(CoreBridge::TText::AddBookmarkCaption));
  QVBoxLayout * Layout = new QVBoxLayout(&Dialog);
  QLabel * Label = new QLabel(CoreText(CoreBridge::TText::AddBookmarkPrompt), &Dialog);
  QComboBox * Combo = new QComboBox(&Dialog);
  Combo->setEditable(true);
  const int Selected = Initial ? -1 : SelectedIndex(Index);
  const std::string Folder = (Selected >= 0) ? FProfiles[Index][Selected].Entry.Node : SelectedFolder(Index).toStdString();
  for (const TProfile & Profile : FProfiles[Index])
  {
    if (Profile.Entry.Node == Folder)
    {
      Combo->addItem(QString::fromStdString(Profile.Entry.Name));
    }
  }
  Combo->setEditText(Name);
  Label->setBuddy(Combo);
  Layout->addWidget(Label);
  Layout->addWidget(Combo);
  QCheckBox * SharedCheck = nullptr;
  if (Initial)
  {
    SharedCheck = new QCheckBox(CoreText(CoreBridge::TText::AddBookmarkShared), &Dialog);
    SharedCheck->setChecked(Shared);
    Layout->addWidget(SharedCheck);
  }
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  connect(Buttons, &QDialogButtonBox::accepted, &Dialog, [this, &Dialog, Combo]()
  {
    if (!ValidName(Combo->currentText()))
    {
      Error(CoreText(CoreBridge::TText::BookmarkInvalidName).replace(QStringLiteral("%s"), Combo->currentText()));
      return;
    }
    Dialog.accept();
  });
  connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Layout->addWidget(Buttons);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  Name = Combo->currentText();
  if (SharedCheck != nullptr)
  {
    Shared = SharedCheck->isChecked();
  }
  return true;
}

// As TLocationProfilesDialog::AddAsBookmark.
bool LocationProfilesDialog::AddProfile(int Index, const QString & Name, bool Initial, bool Shared)
{
  if (!ValidName(Name) || (FLocal->currentText().isEmpty() && FRemote->currentText().isEmpty()))
  {
    return false;
  }
  if (Initial)
  {
    Index = Shared ? 1 : 0;
    try
    {
      CoreBridge::SetUseSharedBookmarks(Shared);
    }
    catch (const CoreBridge::TCoreError &)
    {
      // Kept for this dialog only.
    }
    FPages->setCurrentWidget(FPageWidgets[Index]);
  }
  const int Selected = Initial ? -1 : SelectedIndex(Index);
  const std::string Folder = (Selected >= 0) ? FProfiles[Index][Selected].Entry.Node : SelectedFolder(Index).toStdString();
  int Id = 0;
  for (TProfile & Profile : FProfiles[Index])
  {
    if ((Profile.Entry.Node == Folder) && (Profile.Entry.Name == Name.toStdString()))
    {
      Profile.Entry.Local = FLocal->currentText().toStdString();
      Profile.Entry.Remote = FRemote->currentText().toStdString();
      Id = Profile.Id;
    }
  }
  if (Id == 0)
  {
    TProfile Profile;
    Profile.Entry.Name = Name.toStdString();
    Profile.Entry.Local = FLocal->currentText().toStdString();
    Profile.Entry.Remote = FRemote->currentText().toStdString();
    Profile.Entry.Node = Folder;
    Profile.Id = FNextId++;
    Id = Profile.Id;
    // Before the profile selected, else into the folder selected, or last.
    if (Selected >= 0)
    {
      FProfiles[Index].insert(FProfiles[Index].begin() + Selected, Profile);
    }
    else
    {
      FProfiles[Index].push_back(Profile);
    }
  }
  LoadTree(Index, Id);
  UpdateControls();
  return true;
}

void LocationProfilesDialog::RemoveSelected(int Index, bool Confirm)
{
  const int Selected = SelectedIndex(Index);
  if (Selected >= 0)
  {
    FProfiles[Index].erase(FProfiles[Index].begin() + Selected);
  }
  else
  {
    const QString Folder = SelectedFolder(Index);
    if (Folder.isEmpty())
    {
      return;
    }
    if (Confirm)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Confirmation;
      Query.Message = CoreBridge::Text(CoreBridge::TText::DeleteBookmarkFolder);
      CoreBridge::TQueryButton Yes;
      Yes.Answer = CoreBridge::AnswerYes;
      Yes.Caption = "&Yes";
      CoreBridge::TQueryButton No;
      No.Answer = CoreBridge::AnswerNo;
      No.Caption = "&No";
      Query.Buttons = { Yes, No };
      Query.CancelAnswer = CoreBridge::AnswerNo;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).Answer != CoreBridge::AnswerYes)
      {
        return;
      }
    }
    FProfiles[Index].erase(std::remove_if(FProfiles[Index].begin(), FProfiles[Index].end(),
      [&Folder](const TProfile & Profile) { return Profile.Entry.Node == Folder.toStdString(); }), FProfiles[Index].end());
  }
  LoadTree(Index, 0);
  UpdateControls();
}

// As TLocationProfilesDialog::ProfilesViewEdited.
bool LocationProfilesDialog::RenameSelected(int Index, const QString & Name)
{
  const int Selected = SelectedIndex(Index);
  if (Selected >= 0)
  {
    if (!ValidName(Name))
    {
      Error(CoreText(CoreBridge::TText::BookmarkInvalidName).replace(QStringLiteral("%s"), Name));
      return false;
    }
    FProfiles[Index][Selected].Entry.Name = Name.toStdString();
    LoadTree(Index, FProfiles[Index][Selected].Id);
    return true;
  }
  const QString Folder = SelectedFolder(Index);
  if (Folder.isEmpty())
  {
    return false;
  }
  if (!ValidFolder(Name, false))
  {
    Error(CoreText(CoreBridge::TText::BookmarkFolderInvalidName).replace(QStringLiteral("%s"), Name));
    return false;
  }
  for (const TProfile & Profile : FProfiles[Index])
  {
    const QString Other = QString::fromStdString(Profile.Entry.Node);
    if ((Other.compare(Name, Qt::CaseInsensitive) == 0) && (Other.compare(Folder, Qt::CaseInsensitive) != 0))
    {
      Error(CoreText(CoreBridge::TText::DuplicateBookmarkFolder).replace(QStringLiteral("%s"), Name));
      return false;
    }
  }
  for (TProfile & Profile : FProfiles[Index])
  {
    if (Profile.Entry.Node == Folder.toStdString())
    {
      Profile.Entry.Node = Name.toStdString();
    }
  }
  LoadTree(Index, 0, Name);
  return true;
}

// As BookmarkMoveToButtonClick and BookmarkMove: into the folder, last.
bool LocationProfilesDialog::MoveSelectedTo(int Index, const QString & Folder)
{
  const int Selected = SelectedIndex(Index);
  if (Selected < 0)
  {
    return false;
  }
  if (!ValidFolder(Folder, true))
  {
    Error(CoreText(CoreBridge::TText::BookmarkFolderInvalidName).replace(QStringLiteral("%s"), Folder));
    return false;
  }
  TProfile Profile = FProfiles[Index][Selected];
  if (Profile.Entry.Node == Folder.toStdString())
  {
    return true;
  }
  Profile.Entry.Node = Folder.toStdString();
  FProfiles[Index].erase(FProfiles[Index].begin() + Selected);
  FProfiles[Index].push_back(Profile);
  LoadTree(Index, Profile.Id);
  UpdateControls();
  return true;
}

// Up and Down: past the neighbouring profile of the same folder.
void LocationProfilesDialog::MoveSelected(int Index, int By)
{
  const int Selected = SelectedIndex(Index);
  if (Selected < 0)
  {
    return;
  }
  std::vector<TProfile> & Profiles = FProfiles[Index];
  int Neighbour = -1;
  for (int Position = Selected + By; (Position >= 0) && (Position < static_cast<int>(Profiles.size())); Position += By)
  {
    if (Profiles[Position].Entry.Node == Profiles[Selected].Entry.Node)
    {
      Neighbour = Position;
      break;
    }
  }
  if (Neighbour < 0)
  {
    return;
  }
  const TProfile Profile = Profiles[Selected];
  Profiles.erase(Profiles.begin() + Selected);
  Profiles.insert(Profiles.begin() + Neighbour, Profile);
  LoadTree(Index, Profile.Id);
  UpdateControls();
}

bool LocationProfilesDialog::Select(int Index, const QString & Path)
{
  const int Slash = Path.indexOf(QLatin1Char('/'));
  const std::string Folder = (Slash >= 0) ? Path.left(Slash).toStdString() : std::string();
  const std::string Name = ((Slash >= 0) ? Path.mid(Slash + 1) : Path).toStdString();
  for (const TProfile & Profile : FProfiles[Index])
  {
    if ((Profile.Entry.Node == Folder) && (Profile.Entry.Name == Name))
    {
      FPages->setCurrentWidget(FPageWidgets[Index]);
      LoadTree(Index, Profile.Id);
      Selected(Index);
      return true;
    }
  }
  return false;
}

// As ProfilesViewChange: a profile's directories filled in, and the same
// profile found in the other set.
void LocationProfilesDialog::Selected(int Index)
{
  const int Position = SelectedIndex(Index);
  if (Position >= 0)
  {
    FChanging = true;
    FLocal->setEditText(QString::fromStdString(FProfiles[Index][Position].Entry.Local));
    FRemote->setEditText(QString::fromStdString(FProfiles[Index][Position].Entry.Remote));
    FChanging = false;
    FindProfile();
  }
  UpdateControls();
}

// As TLocationProfilesDialog::FindProfile.
void LocationProfilesDialog::FindProfile()
{
  for (int Index = 0; Index < 2; Index++)
  {
    const auto Matches = [this, Index](int Position)
    {
      return (Position >= 0) && (QString::fromStdString(FProfiles[Index][Position].Entry.Local) == FLocal->currentText()) &&
        (QString::fromStdString(FProfiles[Index][Position].Entry.Remote) == FRemote->currentText());
    };
    if (Matches(SelectedIndex(Index)))
    {
      continue;
    }
    int Id = 0;
    for (int Position = 0; Position < static_cast<int>(FProfiles[Index].size()); Position++)
    {
      if (Matches(Position))
      {
        Id = FProfiles[Index][Position].Id;
        break;
      }
    }
    LoadTree(Index, Id);
  }
}

// As UpdateControls and UpdateProfilesControls.
void LocationProfilesDialog::UpdateControls()
{
  const bool Any = !FLocal->currentText().isEmpty() || !FRemote->currentText().isEmpty();
  FOk->setEnabled(Any);
  for (int Index = 0; Index < 2; Index++)
  {
    const int Selected = SelectedIndex(Index);
    const bool Folder = (Selected < 0) && !SelectedFolder(Index).isEmpty();
    bool Previous = false;
    bool Next = false;
    for (int Position = 0; (Selected >= 0) && (Position < static_cast<int>(FProfiles[Index].size())); Position++)
    {
      if ((Position != Selected) && (FProfiles[Index][Position].Entry.Node == FProfiles[Index][Selected].Entry.Node))
      {
        Previous = Previous || (Position < Selected);
        Next = Next || (Position > Selected);
      }
    }
    FAdd[Index]->setEnabled(Any);
    FRemove[Index]->setEnabled((Selected >= 0) || Folder);
    FRename[Index]->setEnabled((Selected >= 0) || Folder);
    FMoveTo[Index]->setEnabled(Selected >= 0);
    FUp[Index]->setEnabled(Previous);
    FDown[Index]->setEnabled(Next);
  }
}

bool LocationProfilesDialog::eventFilter(QObject * Object, QEvent * Event)
{
  if (Event->type() == QEvent::KeyPress)
  {
    for (int Index = 0; Index < 2; Index++)
    {
      if ((Object == FTrees[Index]) && (FTrees[Index]->currentItem() != nullptr))
      {
        const int Key = static_cast<QKeyEvent *>(Event)->key();
        if (Key == Qt::Key_Delete)
        {
          RemoveSelected(Index);
          return true;
        }
        if (Key == Qt::Key_F2)
        {
          FRename[Index]->click();
          return true;
        }
      }
    }
  }
  return QDialog::eventFilter(Object, Event);
}

QString LocationProfilesDialog::LocalDirectory() const
{
  return FLocal->currentText();
}

QString LocationProfilesDialog::RemoteDirectory() const
{
  return FRemote->currentText();
}

void LocationProfilesDialog::done(int Result)
{
  try
  {
    if (!FSessionKey.empty())
    {
      CoreBridge::SetBookmarks(FSessionKey, Profiles(0));
    }
    CoreBridge::SetBookmarks(std::string(), Profiles(1));
    CoreBridge::SetUseSharedBookmarks(Page() == 1);
    if (FSwitched)
    {
      CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
      Preferences.UseLocationProfiles = false;
      CoreBridge::SetPreferences(Preferences);
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
  QDialog::done(Result);
}
