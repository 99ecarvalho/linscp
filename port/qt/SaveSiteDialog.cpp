#include "SaveSiteDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

}  // namespace

SaveSiteDialog::SaveSiteDialog(const CoreBridge::TSession::TSaveAsSite & Defaults, QWidget * Parent) :
  QDialog(Parent),
  FRootFolder(CoreText(CoreBridge::TText::SaveSessionRootFolder))
{
  setWindowTitle(CoreText(CoreBridge::TText::SaveSessionCaption));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Fields = new QFormLayout();
  // As Execute: the name's last part, its folder in the combo.
  const QString Full = QString::fromStdString(Defaults.Name);
  const int Slash = Full.lastIndexOf(QLatin1Char('/'));
  FName = new QLineEdit(Full.mid(Slash + 1), this);
  FName->setMinimumWidth(300);
  Fields->addRow(CoreText(CoreBridge::TText::SaveSessionPrompt), FName);
  FFolder = new QComboBox(this);
  FFolder->setEditable(true);
  FFolder->addItem(FRootFolder);
  for (const std::string & Folder : Defaults.Folders)
  {
    FFolder->addItem(QString::fromStdString(Folder));
  }
  FFolder->setEditText((Slash > 0) ? Full.left(Slash) : FRootFolder);
  Fields->addRow(CoreText(CoreBridge::TText::SaveSessionFolder), FFolder);
  Layout->addLayout(Fields);
  FSavePassword = new QCheckBox(CoreText(Defaults.NotRecommended ? CoreBridge::TText::SaveSessionPassword :
    CoreBridge::TText::SaveSessionPasswordRecommended), this);
  FSavePassword->setChecked(Defaults.SavePassword);
  FSavePassword->setEnabled(Defaults.CanSavePassword);
  Layout->addWidget(FSavePassword);
  FCreateShortcut = new QCheckBox(CoreText(CoreBridge::TText::SaveSiteShortcut), this);
  Layout->addWidget(FCreateShortcut);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &SaveSiteDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  // DoChange: a name to submit.
  auto Update = [this]() { FOk->setEnabled(!FName->text().isEmpty()); };
  connect(FName, &QLineEdit::textChanged, this, Update);
  Update();
}

// As GetSessionName: the folder (none for "<none>") and the name.
QString SaveSiteDialog::Name() const
{
  const QString Folder = FFolder->currentText().trimmed();
  return ((Folder.isEmpty() || (Folder == FRootFolder)) ? QString() : (Folder + QLatin1Char('/'))) + FName->text();
}

bool SaveSiteDialog::CreateShortcut() const
{
  return FCreateShortcut->isChecked();
}

bool SaveSiteDialog::SavePassword() const
{
  return FSavePassword->isEnabled() && FSavePassword->isChecked();
}

// SessionNameValidate's overwrite question; the rest of DoValidate is
// core's, when the site is saved.
void SaveSiteDialog::accept()
{
  if (CoreBridge::SiteExists(Name().toStdString()))
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreText(CoreBridge::TText::ConfirmOverwriteSite).replace(QStringLiteral("%s"), Name()).toStdString();
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
  if (SavePassword() && !CoreBridge::EnsureMasterPassword())
  {
    return;
  }
  QDialog::accept();
}
