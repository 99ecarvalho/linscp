#include "OpenDirectoryDialog.h"
#include "ShortCutDialog.h"

#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "QtFrontend.h"

QString ShortCutText(unsigned ShortCut)
{
  const unsigned Key = ShortCut & 0xFF;
  int QtKey = 0;
  if (((Key >= '0') && (Key <= '9')) || ((Key >= 'A') && (Key <= 'Z')))
  {
    QtKey = static_cast<int>(Key);
  }
  else if ((Key >= 0x70) && (Key <= 0x87))
  {
    QtKey = Qt::Key_F1 + static_cast<int>(Key - 0x70);
  }
  if ((ShortCut == 0) || (QtKey == 0))
  {
    return QString();
  }
  int Modifiers = 0;
  Modifiers |= (ShortCut & 0x2000) ? int(Qt::SHIFT) : 0;
  Modifiers |= (ShortCut & 0x4000) ? int(Qt::CTRL) : 0;
  Modifiers |= (ShortCut & 0x8000) ? int(Qt::ALT) : 0;
  return QKeySequence(QtKey | Modifiers).toString(QKeySequence::NativeText);
}

OpenDirectoryDialog::OpenDirectoryDialog(bool Remote, const QString & Directory, const QStringList & History,
    const std::string & SessionKey, bool AddBookmark, bool Shared, QWidget * Parent, bool AllowSwitch) :
  QDialog(Parent),
  FRemote(Remote),
  FSessionKey(SessionKey)
{
  setWindowTitle(QString::fromStdString(CoreBridge::Text(
    AddBookmark ? CoreBridge::TText::ManageBookmarksCaption : CoreBridge::TText::OpenDirectoryCaption)));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QLabel * Label = new QLabel(tr("&Open directory:"), this);
  QHBoxLayout * Row = new QHBoxLayout();
  FDirectory = new QComboBox(this);
  FDirectory->setEditable(true);
  FDirectory->addItems(History);
  FDirectory->setEditText(Directory);
  FDirectory->setMinimumWidth(FDirectory->fontMetrics().horizontalAdvance(QLatin1Char('x')) * 50);
  Label->setBuddy(FDirectory);
  Row->addWidget(FDirectory, 1);
  if (!Remote)
  {
    QPushButton * Browse = new QPushButton(tr("B&rowse..."), this);
    connect(Browse, &QPushButton::clicked, this, [this]()
    {
      const QString Chosen = QFileDialog::getExistingDirectory(this, QString(), FDirectory->currentText());
      if (!Chosen.isEmpty())
      {
        FDirectory->setEditText(QDir::toNativeSeparators(Chosen));
      }
    });
    Row->addWidget(Browse);
  }
  Layout->addWidget(Label);
  Layout->addLayout(Row);
  FPages = new QTabWidget(this);
  FPageWidgets[0] = MakePage(0);
  FPageWidgets[1] = MakePage(1);
  if (!SessionKey.empty())
  {
    FPages->addTab(FPageWidgets[0], tr("Site bookmarks"));
  }
  else
  {
    FPageWidgets[0]->hide();
  }
  FPages->addTab(FPageWidgets[1], tr("Shared bookmarks"));
  Layout->addWidget(FPages, 1);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  if (AllowSwitch)
  {
    connect(Buttons->addButton(tr("&Location Profiles..."), QDialogButtonBox::ResetRole), &QPushButton::clicked, this, [this]()
    {
      FSwitched = true;
      reject();
    });
  }
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);

  // As TOpenDirectoryDialog::Execute.
  if (!SessionKey.empty())
  {
    FBookmarks[0] = CoreBridge::Bookmarks(SessionKey);
  }
  FBookmarks[1] = CoreBridge::Bookmarks(std::string());
  LoadList(0);
  LoadList(1);
  const int Page = (SessionKey.empty() || Shared) ? 1 : 0;
  FPages->setCurrentWidget(FPageWidgets[Page]);
  connect(FDirectory, &QComboBox::editTextChanged, this, [this]()
  {
    // DirectoryEditChange: the bookmark of the directory typed focused.
    if (FUpdating)
    {
      return;
    }
    FUpdating = true;
    for (int Index = 0; Index < 2; Index++)
    {
      const int Row = FindBookmark(Index, FDirectory->currentText());
      if (Row >= 0)
      {
        FLists[Index]->setCurrentRow(Row);
      }
    }
    FUpdating = false;
    UpdateControls();
  });
  connect(FPages, &QTabWidget::currentChanged, this, [this]()
  {
    Selected(FPages->currentWidget() == FPageWidgets[0] ? 0 : 1);
  });
  if (AddBookmark)
  {
    AddAsBookmark(Page);
    FLists[Page]->setFocus();
  }
  else
  {
    FDirectory->setFocus();
    FDirectory->lineEdit()->selectAll();
  }
  UpdateControls();
}

QWidget * OpenDirectoryDialog::MakePage(int Page)
{
  QWidget * Widget = new QWidget(FPages);
  QHBoxLayout * Layout = new QHBoxLayout(Widget);
  FLists[Page] = new QListWidget(Widget);
  FLists[Page]->installEventFilter(this);
  // Named as its tab.
  FLists[Page]->setAccessibleName((Page == 0) ? tr("Site bookmarks") : tr("Shared bookmarks"));
  Layout->addWidget(FLists[Page], 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  FAdd[Page] = new QPushButton(tr("&Add"), Widget);
  FRemove[Page] = new QPushButton(tr("Remo&ve"), Widget);
  FUp[Page] = new QPushButton(tr("&Up"), Widget);
  FDown[Page] = new QPushButton(tr("&Down"), Widget);
  Buttons->addWidget(FAdd[Page]);
  Buttons->addWidget(FRemove[Page]);
  // Shortcuts are the shared bookmarks' only, which the main window knows.
  if (Page == 1)
  {
    FShortCut = new QPushButton(tr("&Shortcut..."), Widget);
    Buttons->addWidget(FShortCut);
    connect(FShortCut, &QPushButton::clicked, this, [this]() { ShortCutBookmark(); });
  }
  Buttons->addWidget(FUp[Page]);
  Buttons->addWidget(FDown[Page]);
  Buttons->addStretch(1);
  Layout->addLayout(Buttons);
  connect(FAdd[Page], &QPushButton::clicked, this, [this, Page]() { AddAsBookmark(Page); });
  connect(FRemove[Page], &QPushButton::clicked, this, [this, Page]() { RemoveBookmark(Page); });
  connect(FUp[Page], &QPushButton::clicked, this, [this, Page]() { MoveBookmark(Page, -1); });
  connect(FDown[Page], &QPushButton::clicked, this, [this, Page]() { MoveBookmark(Page, 1); });
  connect(FLists[Page], &QListWidget::currentRowChanged, this, [this, Page]()
  {
    if (!FUpdating)
    {
      Selected(Page);
    }
  });
  connect(FLists[Page], &QListWidget::itemDoubleClicked, this, [this]() { accept(); });
  return Widget;
}

QString OpenDirectoryDialog::BookmarkDirectory(const CoreBridge::TBookmarkEntry & Bookmark) const
{
  return QString::fromStdString(FRemote ? Bookmark.Remote : Bookmark.Local);
}

// As TOpenDirectoryDialog::LoadBookmarks: those with a directory this side,
// each once, with its shortcut.
void OpenDirectoryDialog::LoadList(int Page)
{
  FUpdating = true;
  const int Current = FLists[Page]->currentRow();
  FLists[Page]->clear();
  FRows[Page].clear();
  for (int Index = 0; Index < static_cast<int>(FBookmarks[Page].size()); Index++)
  {
    const QString Directory = BookmarkDirectory(FBookmarks[Page][Index]);
    if (!Directory.isEmpty() && (FindBookmark(Page, Directory) < 0))
    {
      const QString ShortCut = ShortCutText(FBookmarks[Page][Index].ShortCut);
      FLists[Page]->addItem(ShortCut.isEmpty() ? Directory : QStringLiteral("%1 (%2)").arg(Directory, ShortCut));
      FRows[Page].push_back(Index);
    }
  }
  FLists[Page]->setCurrentRow(qMin(Current, FLists[Page]->count() - 1));
  FUpdating = false;
}

int OpenDirectoryDialog::FindBookmark(int Page, const QString & Directory) const
{
  for (int Row = 0; Row < static_cast<int>(FRows[Page].size()); Row++)
  {
    const QString Bookmark = BookmarkDirectory(FBookmarks[Page][FRows[Page][Row]]);
    // Remote paths are case sensitive; local ones, as WinSCP's, not.
    if (Bookmark.compare(Directory, FRemote ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0)
    {
      return Row;
    }
  }
  return -1;
}

void OpenDirectoryDialog::Selected(int Page)
{
  const int Row = FLists[Page]->currentRow();
  if ((Row >= 0) && (Row < static_cast<int>(FRows[Page].size())))
  {
    FUpdating = true;
    FDirectory->setEditText(BookmarkDirectory(FBookmarks[Page][FRows[Page][Row]]));
    FUpdating = false;
  }
  UpdateControls();
}

void OpenDirectoryDialog::AddAsBookmark(int Page)
{
  const QString Directory = FDirectory->currentText();
  if (Directory.isEmpty() || (FindBookmark(Page, Directory) >= 0))
  {
    return;
  }
  CoreBridge::TBookmarkEntry Bookmark;
  (FRemote ? Bookmark.Remote : Bookmark.Local) = Directory.toStdString();
  Bookmark.Name = Directory.toStdString();
  // Before the one focused, else last, as upstream.
  const int Row = FLists[Page]->currentRow();
  int Position = static_cast<int>(FBookmarks[Page].size());
  if ((Row >= 0) && (Row < static_cast<int>(FRows[Page].size())))
  {
    Position = FRows[Page][Row];
  }
  FBookmarks[Page].insert(FBookmarks[Page].begin() + Position, Bookmark);
  LoadList(Page);
  FUpdating = true;
  FLists[Page]->setCurrentRow(FindBookmark(Page, Directory));
  FUpdating = false;
  UpdateControls();
}

void OpenDirectoryDialog::RemoveBookmark(int Page)
{
  const int Row = FLists[Page]->currentRow();
  if ((Row < 0) || (Row >= static_cast<int>(FRows[Page].size())))
  {
    return;
  }
  FBookmarks[Page].erase(FBookmarks[Page].begin() + FRows[Page][Row]);
  LoadList(Page);
  FUpdating = true;
  FLists[Page]->setCurrentRow(qMin(Row, FLists[Page]->count() - 1));
  FUpdating = false;
  Selected(Page);
}

// As TOpenDirectoryDialog::ShortCutBookmarkButtonClick: one the custom
// commands and the other shared bookmarks do not use.
bool OpenDirectoryDialog::ShortCutBookmark()
{
  const int Page = 1;
  const int Row = FLists[Page]->currentRow();
  if ((Row < 0) || (Row >= static_cast<int>(FRows[Page].size())))
  {
    return false;
  }
  CoreBridge::TBookmarkEntry & Bookmark = FBookmarks[Page][FRows[Page][Row]];
  std::vector<unsigned> Used = CustomCommandShortCuts();
  for (const CoreBridge::TBookmarkEntry & Other : FBookmarks[Page])
  {
    if (Other.ShortCut != 0)
    {
      Used.push_back(Other.ShortCut);
    }
  }
  ShortCutDialog Dialog(Used, Bookmark.ShortCut, this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  Bookmark.ShortCut = Dialog.ShortCut();
  LoadList(Page);
  UpdateControls();
  return true;
}

void OpenDirectoryDialog::MoveBookmark(int Page, int By)
{
  const int Row = FLists[Page]->currentRow();
  const int To = Row + By;
  if ((Row < 0) || (To < 0) || (To >= static_cast<int>(FRows[Page].size())))
  {
    return;
  }
  // Past the shown neighbour, as TBookmarkList::MoveTo.
  const int From = FRows[Page][Row];
  const int Neighbour = FRows[Page][To];
  CoreBridge::TBookmarkEntry Bookmark = FBookmarks[Page][From];
  FBookmarks[Page].erase(FBookmarks[Page].begin() + From);
  FBookmarks[Page].insert(FBookmarks[Page].begin() + Neighbour, Bookmark);
  LoadList(Page);
  FUpdating = true;
  FLists[Page]->setCurrentRow(To);
  FUpdating = false;
  UpdateControls();
}

// As TOpenDirectoryDialog::UpdateControls and UpdateBookmarkControls.
void OpenDirectoryDialog::UpdateControls()
{
  const QString Directory = FDirectory->currentText();
  FOk->setEnabled(!Directory.isEmpty());
  for (int Page = 0; Page < 2; Page++)
  {
    const int Row = FLists[Page]->currentRow();
    FAdd[Page]->setEnabled(!Directory.isEmpty() && (FindBookmark(Page, Directory) < 0));
    FRemove[Page]->setEnabled(Row >= 0);
    FUp[Page]->setEnabled(Row > 0);
    FDown[Page]->setEnabled((Row >= 0) && (Row < FLists[Page]->count() - 1));
  }
  FShortCut->setEnabled(FLists[1]->currentRow() >= 0);
}

// Delete removes the focused bookmark, as BookmarksListKeyDown.
bool OpenDirectoryDialog::eventFilter(QObject * Object, QEvent * Event)
{
  if (Event->type() == QEvent::KeyPress)
  {
    for (int Page = 0; Page < 2; Page++)
    {
      if ((Object == FLists[Page]) && (static_cast<QKeyEvent *>(Event)->key() == Qt::Key_Delete) &&
          (FLists[Page]->currentRow() >= 0))
      {
        RemoveBookmark(Page);
        return true;
      }
    }
  }
  return QDialog::eventFilter(Object, Event);
}

QString OpenDirectoryDialog::Directory() const
{
  return FDirectory->currentText();
}

void OpenDirectoryDialog::done(int Result)
{
  try
  {
    CoreBridge::SetBookmarks(std::string(), FBookmarks[1]);
    if (!FSessionKey.empty())
    {
      CoreBridge::SetBookmarks(FSessionKey, FBookmarks[0]);
      CoreBridge::SetUseSharedBookmarks(FPages->currentWidget() == FPageWidgets[1]);
    }
    if (FSwitched)
    {
      CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
      Preferences.UseLocationProfiles = true;
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
