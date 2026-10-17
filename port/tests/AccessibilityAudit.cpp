#include "AccessibilityAudit.h"
#include <QRegularExpression>
#include <QDir>
#include <QTreeWidget>
#include <QListWidget>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAccessible>
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSlider>
#include <QTextEdit>
#include <QWidget>

#include <cstdio>
#include <cstdlib>

namespace
{

// The character after a lone '&' ("&&" is a literal one), lower-cased; none: 0.
QChar Mnemonic(const QString & Text)
{
  for (int Index = 0; Index + 1 < Text.size(); Index++)
  {
    if (Text[Index] == QLatin1Char('&'))
    {
      if (Text[Index + 1] == QLatin1Char('&'))
      {
        Index++;
      }
      else
      {
        return Text[Index + 1].toLower();
      }
    }
  }
  return QChar();
}

QString Describe(const QWidget * Widget)
{
  QString Result = QString::fromLatin1(Widget->metaObject()->className());
  if (!Widget->objectName().isEmpty())
  {
    Result += QLatin1Char('#') + Widget->objectName();
  }
  return Result;
}

// A control a user reaches with Tab, and one a reader must name. The parts
// of compound ones (a combo box's or spin box's line edit) are the whole's.
bool NeedsName(const QWidget * Widget)
{
  if (!(Widget->focusPolicy() & Qt::TabFocus))
  {
    return false;
  }
  const QWidget * Parent = Widget->parentWidget();
  if ((qobject_cast<const QLineEdit *>(Widget) != nullptr) &&
      ((qobject_cast<const QComboBox *>(Parent) != nullptr) || (qobject_cast<const QAbstractSpinBox *>(Parent) != nullptr)))
  {
    return false;
  }
  return (qobject_cast<const QLineEdit *>(Widget) != nullptr) || (qobject_cast<const QComboBox *>(Widget) != nullptr) ||
    (qobject_cast<const QAbstractSpinBox *>(Widget) != nullptr) || (qobject_cast<const QAbstractItemView *>(Widget) != nullptr) ||
    (qobject_cast<const QPlainTextEdit *>(Widget) != nullptr) || (qobject_cast<const QTextEdit *>(Widget) != nullptr) ||
    (qobject_cast<const QSlider *>(Widget) != nullptr) || (qobject_cast<const QAbstractButton *>(Widget) != nullptr);
}

// Mnemonics WinSCP itself gives two items of one window or menu (its
// forms' captions), kept as they are: Qt cycles between such items, as
// Windows does.
bool WinScpClash(const QString & One, const QString & Other)
{
  static const QList<QPair<QString, QString>> Clashes = {
    { QStringLiteral("&Existing files only"), QStringLiteral("Continue on &error") },  // Synchronize.dfm
    { QStringLiteral("B&rowse..."), QStringLiteral("&Remote directory:") },  // LocationProfiles.dfm
    { QStringLiteral("Re&do"), QStringLiteral("&Delete") },  // Editor.dfm
    { QStringLiteral("Du&plicate..."), QStringLiteral("&Properties") },  // NonVisual.dfm, Files
    // Preferences.dfm: Endurance, Integration > Applications, Internal editor.
    { QStringLiteral("Automatically reconnect session, if it breaks &while idle"),
      QStringLiteral("Automatically reconnect session, if it breaks &while inactive") },
    { QStringLiteral("Di&sable"), QStringLiteral("Automatically reconnect session, if it &stalls") },
    { QStringLiteral("B&rowse..."), QStringLiteral("&Remember session password and pass it to PuTTY (SSH)") },
    { QStringLiteral("&Tabulator size:"), QStringLiteral("&Text color") },
    { QStringLiteral("Add upload shortcut to Explorer's '&Send to' context menu"), QStringLiteral("Add linscp to &search path") },
    // NonVisual.dfm, Tabs.
    { QStringLiteral("&Rename Tab"), QStringLiteral("&Reconnect Session") },
    { QStringLiteral("&Close Tab"), QStringLiteral("&Change Password...") },
    { QStringLiteral("Server/Protocol &Information"), QStringLiteral("&Install Public Key into Server...") },
    // NonVisual.dfm, Files and Commands.
    { QStringLiteral("File &Custom Commands"), QStringLiteral("&Calculate Directory Sizes") },
    { QStringLiteral("Du&plicate..."), QStringLiteral("&Paste from Clipboard") },
    { QStringLiteral("&Copy to Clipboard"), QStringLiteral("File &Custom Commands") },
    { QStringLiteral("&Copy to Clipboard"), QStringLiteral("&Calculate Directory Sizes") },
    { QStringLiteral("&Compare Directories"), QStringLiteral("Static &Custom Commands") },
    // ScpExplorer.dfm's menus: Files, Commands, View, View > Toolbars.
    { QStringLiteral("&Properties"), QStringLiteral("&Paste from Clipboard") },
    { QStringLiteral("&Calculate Directory Sizes"), QStringLiteral("&Close Tab") },
    { QStringLiteral("Copy &Path to Clipboard"), QStringLiteral("Open in &PuTTY") },
    { QStringLiteral("Se&lection Buttons"), QStringLiteral("&Lock Toolbars") },
    { QStringLiteral("&Standard Buttons"), QStringLiteral("&Show Selective Text Labels") },
    { QStringLiteral("&Toolbars"), QStringLiteral("&Tree") },
    { QStringLiteral("&Toolbars"), QStringLiteral("Lis&t") },
    { QStringLiteral("&Toolbars"), QStringLiteral("&Thumbnails") },
    { QStringLiteral("&Small Icons"), QStringLiteral("&Sort") },
    // SiteAdvanced.dfm: the Color button and its pages; Connection.
    { QStringLiteral("&Color"), QStringLiteral("Post login &commands:") },
    { QStringLiteral("&Color"), QStringLiteral("Listing &command:") },
    { QStringLiteral("&Color"), QStringLiteral("&Client certificate file:") },
    { QStringLiteral("Optimize connection &buffer size"), QStringLiteral("Seconds &between keepalives:") },
    // A default custom command's name and the Enter... item.
    { QStringLiteral("&Execute"), QStringLiteral("&Enter...") },
  };
  for (const auto & Clash : Clashes)
  {
    if (((Clash.first == One) && (Clash.second == Other)) || ((Clash.first == Other) && (Clash.second == One)))
    {
      return true;
    }
  }
  return false;
}

}  // namespace

AccessibilityAudit::AccessibilityAudit(int & Failures) :
  FFailures(Failures),
  FReportOnly(std::getenv("LINSCP_A11Y_REPORT") != nullptr)
{
}

void AccessibilityAudit::Issue(const QString & Where, const QString & What)
{
  FIssues++;
  if (FReportOnly)
  {
    std::printf("A11Y %s: %s\n", qPrintable(Where), qPrintable(What));
  }
  else
  {
    std::printf("FAIL accessibility: %s: %s\n", qPrintable(Where), qPrintable(What));
    FFailures++;
  }
}

bool AccessibilityAudit::eventFilter(QObject * Object, QEvent * Event)
{
  if (Event->type() == QEvent::Show)
  {
    QWidget * Widget = qobject_cast<QWidget *>(Object);
    if ((Widget != nullptr) && Widget->isWindow() && (qobject_cast<QMenu *>(Widget) == nullptr))
    {
      const QString Key = QString::fromLatin1(Widget->metaObject()->className()) + QLatin1Char('|') + Widget->windowTitle();
      if (!FSeen.contains(Key))
      {
        FSeen.insert(Key);
        // $LINSCP_SHOTS: each window as first shown, for a person to look at.
        const QString Shots = qEnvironmentVariable("LINSCP_SHOTS");
        if (!Shots.isEmpty())
        {
          QDir().mkpath(Shots);
          QString Name = Key;
          Name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
          Widget->grab().save(QDir(Shots).filePath(Name + QStringLiteral(".png")));
        }
        if (FReportOnly)
        {
          std::printf("A11Y-SEEN %s\n", qPrintable(QString::fromLatin1(Widget->metaObject()->className())));
        }
        Audit(Widget);
      }
    }
  }
  return false;
}

void AccessibilityAudit::AuditMenu(QMenu * Menu, const QString & Where)
{
  // What is filled as it opens.
  emit Menu->aboutToShow();
  QMap<QChar, QString> Used;
  for (QAction * Action : Menu->actions())
  {
    if (!Action->isVisible() || Action->isSeparator())
    {
      continue;
    }
    const QChar Key = Mnemonic(Action->text());
    if (!Key.isNull())
    {
      if (Used.contains(Key))
      {
        if (!WinScpClash(Used[Key], Action->text()))
        {
          Issue(Where, QStringLiteral("'%1' and '%2' share the mnemonic '%3'").arg(Used[Key], Action->text(), Key));
        }
      }
      else
      {
        Used[Key] = Action->text();
      }
    }
    if (Action->menu() != nullptr)
    {
      AuditMenu(Action->menu(), Where + QStringLiteral(" > ") + QString(Action->text()).remove(QLatin1Char('&')));
    }
  }
}

void AccessibilityAudit::Audit(QWidget * Window)
{
  const QString Where = QStringLiteral("%1 \"%2\"").arg(QString::fromLatin1(Window->metaObject()->className()), Window->windowTitle());
  QMap<QChar, QString> Used;
  auto UseMnemonic = [this, &Used, &Where](const QString & Text)
  {
    const QChar Key = Mnemonic(Text);
    if (Key.isNull())
    {
      return;
    }
    if (Used.contains(Key))
    {
      if (!WinScpClash(Used[Key], Text))
      {
        Issue(Where, QStringLiteral("'%1' and '%2' share the mnemonic '%3'").arg(Used[Key], Text, Key));
      }
    }
    else
    {
      Used[Key] = Text;
    }
  };
  for (QWidget * Widget : Window->findChildren<QWidget *>())
  {
    if (!Widget->isVisibleTo(Window) || (Widget->window() != Window))
    {
      continue;
    }
    if (QLabel * Label = qobject_cast<QLabel *>(Widget))
    {
      // A message, word-wrapped, shows its '&' as it is: a caption's
      // accelerator left in one is seen.
      if ((Label->textFormat() != Qt::RichText) && Label->wordWrap() && (Label->buddy() == nullptr) &&
          !Label->property("literalText").toBool() && !Mnemonic(Label->text()).isNull())
      {
        Issue(Where, QStringLiteral("label '%1' shows its '&'").arg(Label->text()));
      }
      if ((Label->textFormat() != Qt::RichText) && !Label->wordWrap() && !Mnemonic(Label->text()).isNull())
      {
        if (Label->buddy() == nullptr)
        {
          Issue(Where, QStringLiteral("label '%1' has a mnemonic but no buddy").arg(Label->text()));
        }
        else if (Label->buddy()->isEnabled())
        {
          UseMnemonic(Label->text());
        }
      }
      continue;
    }
    // Lists show their items' text as it is: no accelerators there (but in
    // those whose text is the user's, "literalText").
    QStringList Shown;
    const bool Literal = Widget->property("literalText").toBool();
    if (QComboBox * Combo = qobject_cast<QComboBox *>(Widget))
    {
      for (int Index = 0; Index < Combo->count(); Index++)
      {
        Shown.append(Combo->itemText(Index));
      }
    }
    else if (QListWidget * List = qobject_cast<QListWidget *>(Widget))
    {
      for (int Index = 0; Index < List->count(); Index++)
      {
        Shown.append(List->item(Index)->text());
      }
    }
    else if (QTreeWidget * Tree = qobject_cast<QTreeWidget *>(Widget))
    {
      for (int Column = 0; Column < Tree->columnCount(); Column++)
      {
        Shown.append(Tree->headerItem()->text(Column));
        for (int Index = 0; Index < Tree->topLevelItemCount(); Index++)
        {
          Shown.append(Tree->topLevelItem(Index)->text(Column));
        }
      }
    }
    for (const QString & Text : Shown)
    {
      if (!Literal && !Mnemonic(Text).isNull())
      {
        Issue(Where, QStringLiteral("%1 shows '%2' with its '&'").arg(Describe(Widget), Text));
      }
    }
    if (QAbstractButton * Button = qobject_cast<QAbstractButton *>(Widget); (Button != nullptr) && Button->isEnabled())
    {
      // A button box's standard buttons take the platform's mnemonics (none
      // on Windows), not the program's.
      QDialogButtonBox * Box = qobject_cast<QDialogButtonBox *>(Button->parentWidget());
      if ((Box == nullptr) || (Box->standardButton(Button) == QDialogButtonBox::NoButton))
      {
        UseMnemonic(Button->text());
      }
    }
    if (Widget->isEnabled() && NeedsName(Widget))
    {
      QAccessibleInterface * Interface = QAccessible::queryAccessibleInterface(Widget);
      const QString Name = (Interface != nullptr) ? Interface->text(QAccessible::Name) : QString();
      if (Name.trimmed().isEmpty())
      {
        Issue(Where, QStringLiteral("%1 has no accessible name").arg(Describe(Widget)));
      }
    }
  }
  if (QMainWindow * Main = qobject_cast<QMainWindow *>(Window))
  {
    QMap<QChar, QString> Bar;
    for (QAction * Action : Main->menuBar()->actions())
    {
      const QChar Key = Mnemonic(Action->text());
      if (!Key.isNull() && Bar.contains(Key))
      {
        Issue(Where, QStringLiteral("menus '%1' and '%2' share the mnemonic '%3'").arg(Bar[Key], Action->text(), Key));
      }
      Bar[Key] = Action->text();
      if (Action->menu() != nullptr)
      {
        AuditMenu(Action->menu(), Where + QStringLiteral(" menu ") + QString(Action->text()).remove(QLatin1Char('&')));
      }
    }
  }
}
