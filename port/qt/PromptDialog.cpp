#include "PromptDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QMenu>
#include <QMouseEvent>
#include <QUrl>
#include <QVBoxLayout>

PromptDialog::PromptDialog(const CoreBridge::TPrompt & Prompt, QWidget * Parent) :
  QDialog(Parent)
{
  OpenUrl = [](const QString & Url) { QDesktopServices::openUrl(QUrl(Url)); };
  setWindowTitle(Prompt.Name.empty() ? tr("Authentication") : QString::fromStdString(Prompt.Name));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  if (!Prompt.Instructions.empty())
  {
    QLabel * Instructions = new QLabel(QString::fromStdString(Prompt.Instructions), this);
    Instructions->setWordWrap(true);
    Instructions->setTextFormat(Qt::PlainText);
    External(Instructions);
    Layout->addWidget(Instructions);
  }
  QFormLayout * Form = new QFormLayout();
  for (const CoreBridge::TPromptField & Field : Prompt.Fields)
  {
    QLineEdit * Edit = new QLineEdit(QString::fromStdString(Field.Value), this);
    Edit->setEchoMode(Field.Echo ? QLineEdit::Normal : QLineEdit::Password);
    // Core's prompts end in ": " already.
    QString Label = QString::fromStdString(Field.Label).trimmed();
    QLabel * Caption = new QLabel(Label, this);
    Caption->setBuddy(Edit);
    External(Caption);
    // A prompt with no label: named by the prompt, for screen readers.
    if (Label.isEmpty())
    {
      Edit->setAccessibleName(!Prompt.Name.empty() ? QString::fromStdString(Prompt.Name) : windowTitle());
    }
    Form->addRow(Caption, Edit);
    FFields.append(Edit);
  }
  Layout->addLayout(Form);
  // Authenticate.dfm's SavePasswordCheck and SessionRememberPasswordCheck,
  // unchecked each time.
  if (Prompt.OfferSave)
  {
    FSavePassword = new QCheckBox(tr("&Change stored password to this one"), this);
    Layout->addWidget(FSavePassword);
  }
  if (Prompt.OfferRemember)
  {
    FRememberPassword = new QCheckBox(tr("&Remember password for this session"), this);
    Layout->addWidget(FRememberPassword);
  }
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
  if (!FFields.isEmpty())
  {
    FFields.first()->setFocus();
  }
  resize(qMax(sizeHint().width(), 400), sizeHint().height());
}

void PromptDialog::Fill(CoreBridge::TPrompt & Prompt) const
{
  for (int Index = 0; (Index < FFields.size()) && (Index < static_cast<int>(Prompt.Fields.size())); Index++)
  {
    Prompt.Fields[Index].Value = FFields[Index]->text().toStdString();
  }
  Prompt.Save = (FSavePassword != nullptr) && FSavePassword->isChecked();
  Prompt.Remember = (FRememberPassword != nullptr) && FRememberPassword->isChecked();
}

QString PromptDialog::UrlIn(const QString & Text)
{
  int At = Text.indexOf(QStringLiteral("https://"));
  QString Prefix = QStringLiteral("https://");
  if (At < 0)
  {
    At = Text.indexOf(QStringLiteral("http://"));
    Prefix = QStringLiteral("http://");
  }
  if (At < 0)
  {
    return QString();
  }
  QString Url = Text.mid(At);
  for (int Index = 0; Index < Url.size(); Index++)
  {
    if (Url.at(Index).isSpace())
    {
      Url.truncate(Index);
      break;
    }
  }
  // At least a letter or digit after the prefix.
  return ((Url.size() > Prefix.size()) && Url.at(Prefix.size()).isLetterOrNumber()) ? Url : QString();
}

void PromptDialog::External(QLabel * Label)
{
  FLabels.append(Label);
  // Server-supplied prompt text is shown literally, never as rich text, so it
  // cannot inject markup to spoof the dialog. Links are still offered through
  // the context menu (UrlIn/LabelMenu), which reads the plain text.
  Label->setTextFormat(Qt::PlainText);
  Label->setContextMenuPolicy(Qt::DefaultContextMenu);
  Label->installEventFilter(this);
  if (!UrlIn(Label->text()).isEmpty())
  {
    Label->setCursor(Qt::PointingHandCursor);
  }
}

QMenu * PromptDialog::LabelMenu(QLabel * Label)
{
  QMenu * Menu = new QMenu(this);
  Menu->addAction(tr("&Copy"), this, [Label]() { QApplication::clipboard()->setText(Label->text()); });
  const QString Url = UrlIn(Label->text());
  if (!Url.isEmpty())
  {
    Menu->addSeparator();
    Menu->addAction(tr("&Open Link"), this, [this, Url]() { OpenUrl(Url); });
  }
  return Menu;
}

bool PromptDialog::eventFilter(QObject * Watched, QEvent * Event)
{
  QLabel * Label = qobject_cast<QLabel *>(Watched);
  if ((Label != nullptr) && FLabels.contains(Label))
  {
    if (Event->type() == QEvent::ContextMenu)
    {
      QMenu * Menu = LabelMenu(Label);
      Menu->setAttribute(Qt::WA_DeleteOnClose);
      Menu->popup(static_cast<QContextMenuEvent *>(Event)->globalPos());
      return true;
    }
    if ((Event->type() == QEvent::MouseButtonRelease) && (static_cast<QMouseEvent *>(Event)->button() == Qt::LeftButton))
    {
      // As LinkClick.
      const QString Url = UrlIn(Label->text());
      if (!Url.isEmpty())
      {
        OpenUrl(Url);
        return true;
      }
    }
  }
  return QDialog::eventFilter(Watched, Event);
}
