#include "ExtensionOptionsDialog.h"

#include "ShortCutDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

using TOption = CoreBridge::TExtensionOption;

namespace
{

bool IsHttpUrl(const QString & Text)
{
  return Text.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
    Text.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

// The VCL's filter ("Text files (*.txt)|*.txt|All files|*.*") as Qt's.
QString QtFilter(const QString & Filter)
{
  const QStringList Parts = Filter.split(QLatin1Char('|'));
  QStringList Result;
  for (int Index = 0; Index + 1 < Parts.size(); Index += 2)
  {
    const QString Name = Parts[Index];
    const QString Masks = QString(Parts[Index + 1]).replace(QLatin1Char(';'), QLatin1Char(' '));
    Result.append(Name.contains(QLatin1Char('(')) ? Name : QStringLiteral("%1 (%2)").arg(Name, Masks));
  }
  return Result.join(QStringLiteral(";;"));
}

// Environment variables in a path, %NAME% as Windows writes them.
QString ExpandEnvironment(QString Value)
{
  int From = 0;
  while ((From = Value.indexOf(QLatin1Char('%'), From)) >= 0)
  {
    const int To = Value.indexOf(QLatin1Char('%'), From + 1);
    if (To < 0)
    {
      break;
    }
    const QString Name = Value.mid(From + 1, To - From - 1);
    if (!Name.isEmpty() && qEnvironmentVariableIsSet(Name.toLocal8Bit().constData()))
    {
      const QString Replacement = qEnvironmentVariable(Name.toLocal8Bit().constData());
      Value.replace(From, To - From + 1, Replacement);
      From += Replacement.size();
    }
    else
    {
      From = To;
    }
  }
  return Value;
}

}  // namespace

ExtensionOptionsDialog::ExtensionOptionsDialog(const QString & Title, const std::vector<TOption> & Options, QWidget * Parent,
  const unsigned * ShortCut, const std::vector<unsigned> & Used, const QString & HelpPage) :
  QDialog(Parent),
  FOptions(Options)
{
  // StripEllipsis(StripHotkey(Name)).
  QString Caption = Title;
  Caption.remove(QRegularExpression(QStringLiteral("&(?!&)"))).replace(QStringLiteral("&&"), QStringLiteral("&"));
  if (Caption.endsWith(QStringLiteral("...")))
  {
    Caption.chop(3);
  }
  setWindowTitle(Caption);
  setMinimumWidth(444);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  // The current group's form, or the dialog's own.
  QFormLayout * Form = new QFormLayout();
  Layout->addLayout(Form);
  bool HasGroups = false;
  for (const TOption & Option : FOptions)
  {
    const QString OptionCaption = QString::fromStdString(Option.Caption);
    const QString Value = QString::fromStdString(Option.Value);
    QWidget * Control = nullptr;
    switch (Option.Kind)
    {
      case TOption::TKind::Label:
      {
        QLabel * Label = new QLabel(OptionCaption, this);
        Label->setWordWrap(true);
        Label->setProperty("literalText", true);
        Form->addRow(Label);
        Control = Label;
        break;
      }
      case TOption::TKind::Link:
      {
        // Its caption the URL, or its default; else plain text.
        const QString Url = IsHttpUrl(OptionCaption) ? OptionCaption :
          (IsHttpUrl(QString::fromStdString(Option.Default)) ? QString::fromStdString(Option.Default) : QString());
        QLabel * Label = new QLabel(this);
        if (Url.isEmpty())
        {
          Label->setText(OptionCaption);
        }
        else
        {
          Label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(Url.toHtmlEscaped(), OptionCaption.toHtmlEscaped()));
          Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
          connect(Label, &QLabel::linkActivated, this, [](const QString & Link) { QDesktopServices::openUrl(QUrl(Link)); });
        }
        Label->setWordWrap(true);
        Form->addRow(Label);
        Control = Label;
        break;
      }
      case TOption::TKind::Group:
      {
        QGroupBox * Group = new QGroupBox(OptionCaption, this);
        Form = new QFormLayout(Group);
        Layout->addWidget(Group);
        HasGroups = true;
        break;
      }
      case TOption::TKind::Separator:
      {
        QFrame * Line = new QFrame(this);
        Line->setFrameShape(QFrame::HLine);
        Line->setFrameShadow(QFrame::Sunken);
        Form->addRow(Line);
        break;
      }
      case TOption::TKind::TextBox:
      case TOption::TKind::File:
      {
        QComboBox * Combo = new QComboBox(this);
        Combo->setEditable(true);
        Combo->setInsertPolicy(QComboBox::NoInsert);
        for (const std::string & Item : CoreBridge::History(Option.HistoryKey))
        {
          Combo->addItem(QString::fromStdString(Item));
        }
        Combo->setEditText(Value);
        Combo->setMinimumWidth(250);
        if (Option.Kind == TOption::TKind::TextBox)
        {
          Form->addRow(OptionCaption, Combo);
        }
        else
        {
          // As BrowseButtonClick: its title, filter, default extension;
          // the initial file when the box is empty.
          QPushButton * Browse = new QPushButton(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionOptionsBrowse)), this);
          QWidget * Row = new QWidget(this);
          QHBoxLayout * RowLayout = new QHBoxLayout(Row);
          RowLayout->setContentsMargins(0, 0, 0, 0);
          RowLayout->addWidget(Combo, 1);
          RowLayout->addWidget(Browse);
          QLabel * Label = new QLabel(OptionCaption, this);
          Label->setBuddy(Combo);
          Form->addRow(Label, Row);
          const TOption Copy = Option;
          connect(Browse, &QPushButton::clicked, this, [this, Combo, Copy]()
          {
            QString BrowseTitle = QString::fromStdString(Copy.FileCaption);
            if (BrowseTitle.isEmpty())
            {
              QString Name = QString::fromStdString(Copy.Caption).remove(QLatin1Char('&'));
              if (Name.endsWith(QLatin1Char(':')))
              {
                Name.chop(1);
              }
              BrowseTitle = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionOptionsBrowseTitle))
                .replace(QStringLiteral("%s"), Name);
            }
            const QString Initial = Combo->currentText().isEmpty() ? QString::fromStdString(Copy.FileInitial) : Combo->currentText();
            const QString Expanded = ExpandEnvironment(Initial);
            QFileDialog Dialog(this, BrowseTitle, QFileInfo(Expanded).path(), QtFilter(QString::fromStdString(Copy.FileFilter)));
            Dialog.selectFile(Expanded);
            Dialog.setDefaultSuffix(QString::fromStdString(Copy.FileExt));
            if ((Dialog.exec() == QDialog::Accepted) && !Dialog.selectedFiles().isEmpty())
            {
              const QString Chosen = QDir::toNativeSeparators(Dialog.selectedFiles().first());
              if (Chosen != QDir::toNativeSeparators(Expanded))
              {
                Combo->setEditText(Chosen);
              }
              else if (Combo->currentText().isEmpty())
              {
                // Its initial file confirmed: kept as it was written.
                Combo->setEditText(Initial);
              }
            }
          });
        }
        Control = Combo;
        break;
      }
      case TOption::TKind::DropDownList:
      case TOption::TKind::ComboBox:
      {
        QComboBox * Combo = new QComboBox(this);
        Combo->setEditable(Option.Kind == TOption::TKind::ComboBox);
        int Selected = -1;
        for (const auto & Item : Option.Items)
        {
          Combo->addItem(QString::fromStdString(Item.second), QString::fromStdString(Item.first));
          if (Item.first == Option.Value)
          {
            Selected = Combo->count() - 1;
          }
        }
        Combo->setCurrentIndex(Selected);
        if ((Selected < 0) && Combo->isEditable())
        {
          Combo->setEditText(Value);
        }
        Form->addRow(OptionCaption, Combo);
        Control = Combo;
        break;
      }
      case TOption::TKind::CheckBox:
      {
        QCheckBox * Check = new QCheckBox(OptionCaption, this);
        Check->setChecked(!Option.Items.empty() && (Option.Value == Option.Items[0].first));
        Form->addRow(Check);
        Control = Check;
        break;
      }
      case TOption::TKind::Unknown:
        break;
    }
    FControls.push_back(Control);
  }
  if (ShortCut != nullptr)
  {
    if (HasGroups)
    {
      QGroupBox * Group = new QGroupBox(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionGeneralGroup)), this);
      Form = new QFormLayout(Group);
      Layout->addWidget(Group);
    }
    else if (!FOptions.empty())
    {
      QFrame * Line = new QFrame(this);
      Line->setFrameShape(QFrame::HLine);
      Line->setFrameShadow(QFrame::Sunken);
      Form->addRow(Line);
    }
    FShortCut = new QComboBox(this);
    std::vector<unsigned> Others;
    for (unsigned Each : Used)
    {
      if (Each != *ShortCut)
      {
        Others.push_back(Each);
      }
    }
    FillShortCutCombo(FShortCut, Others, *ShortCut);
    Form->addRow(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionShortCut)), FShortCut);
  }
  Layout->addStretch(1);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  if (!HelpPage.isEmpty())
  {
    Box->addButton(QDialogButtonBox::Help);
    connect(Box, &QDialogButtonBox::helpRequested, this, [HelpPage]() { QDesktopServices::openUrl(QUrl(HelpPage)); });
  }
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
}

std::vector<std::string> ExtensionOptionsDialog::Values() const
{
  std::vector<std::string> Result;
  for (std::size_t Index = 0; Index < FOptions.size(); Index++)
  {
    const TOption & Option = FOptions[Index];
    QWidget * Control = FControls[Index];
    std::string Value = Option.Value;
    if (QComboBox * Combo = qobject_cast<QComboBox *>(Control))
    {
      if ((Option.Kind == TOption::TKind::TextBox) || (Option.Kind == TOption::TKind::File))
      {
        Value = Combo->currentText().toStdString();
      }
      else if (Combo->currentIndex() >= 0)
      {
        // As GetComboBoxValue: the item's value; typed in a combo box, the text.
        Value = ((Option.Kind == TOption::TKind::ComboBox) && (Combo->currentText() != Combo->itemText(Combo->currentIndex()))) ?
          Combo->currentText().toStdString() : Combo->currentData().toString().toStdString();
      }
      else
      {
        Value = (Option.Kind == TOption::TKind::ComboBox) ? Combo->currentText().toStdString() : Option.Default;
      }
    }
    else if (QCheckBox * Check = qobject_cast<QCheckBox *>(Control))
    {
      const std::size_t Item = Check->isChecked() ? 0 : 1;
      Value = (Item < Option.Items.size()) ? Option.Items[Item].first : std::string();
    }
    Result.push_back(Value);
  }
  return Result;
}

unsigned ExtensionOptionsDialog::ShortCut() const
{
  return (FShortCut != nullptr) ? FShortCut->currentData().toUInt() : 0;
}

// As SaveHistoryComboBoxValue: the boxes' values into their histories.
void ExtensionOptionsDialog::accept()
{
  for (std::size_t Index = 0; Index < FOptions.size(); Index++)
  {
    const TOption & Option = FOptions[Index];
    QComboBox * Combo = qobject_cast<QComboBox *>(FControls[Index]);
    if ((Combo != nullptr) && ((Option.Kind == TOption::TKind::TextBox) || (Option.Kind == TOption::TKind::File)) &&
        !Combo->currentText().isEmpty())
    {
      CoreBridge::AddToHistory(Option.HistoryKey, Combo->currentText().toStdString());
    }
  }
  QDialog::accept();
}
