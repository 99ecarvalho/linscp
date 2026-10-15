#include "QueryDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

QStyle::StandardPixmap IconOf(CoreBridge::TQueryType Type)
{
  switch (Type)
  {
    case CoreBridge::TQueryType::Warning: return QStyle::SP_MessageBoxWarning;
    case CoreBridge::TQueryType::Error: return QStyle::SP_MessageBoxCritical;
    case CoreBridge::TQueryType::Information: return QStyle::SP_MessageBoxInformation;
    default: return QStyle::SP_MessageBoxQuestion;
  }
}

QString TitleOf(CoreBridge::TQueryType Type)
{
  switch (Type)
  {
    case CoreBridge::TQueryType::Warning: return QueryDialog::tr("Warning");
    case CoreBridge::TQueryType::Error: return QueryDialog::tr("Error");
    case CoreBridge::TQueryType::Information: return QueryDialog::tr("Information");
    default: return QueryDialog::tr("Confirm");
  }
}

}  // namespace

QueryDialog::QueryDialog(const CoreBridge::TQuery & Query, QWidget * Parent) :
  QDialog(Parent),
  FQuery(Query),
  FAnswer(Query.CancelAnswer)
{
  setWindowTitle(TitleOf(Query.Type));

  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Top = new QHBoxLayout();
  QLabel * Icon = new QLabel(this);
  const int Size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this);
  Icon->setPixmap(style()->standardIcon(IconOf(Query.Type), nullptr, this).pixmap(Size, Size));
  Icon->setAlignment(Qt::AlignTop);
  Top->addWidget(Icon);
  QLabel * Message = new QLabel(QString::fromStdString(Query.Message), this);
  // What a message quotes is the user's, '&' and all (audited as such).
  Message->setProperty("literalText", true);
  Message->setWordWrap(true);
  Message->setTextInteractionFlags(Qt::TextSelectableByMouse);
  Message->setTextFormat(Qt::PlainText);
  Top->addWidget(Message, 1);
  Layout->addLayout(Top);

  if (!Query.MoreMessages.empty())
  {
    QPlainTextEdit * More = new QPlainTextEdit(QString::fromStdString(Query.MoreMessages), this);
    More->setAccessibleName(tr("More messages"));
    More->setReadOnly(true);
    More->setMaximumHeight(fontMetrics().lineSpacing() * 8);
    Layout->addWidget(More);
  }

  if (Query.NeverAskAgainCheck)
  {
    // As MessageDialog: for a message with OK alone, "never show".
    const bool OKOnly = (Query.Buttons.size() == 1) && (Query.Buttons.front().Answer == CoreBridge::AnswerOK);
    FNeverAskAgain = new QCheckBox(
      !Query.NeverAskAgainTitle.empty() ? QString::fromStdString(Query.NeverAskAgainTitle) :
        (OKOnly ? tr("Never show this message a&gain") : tr("&Never ask me again")), this);
    FNeverAskAgain->setChecked(Query.NeverAskAgainCheckedInitially);
    Layout->addWidget(FNeverAskAgain);
  }

  QDialogButtonBox * Box = new QDialogButtonBox(this);
  for (int Index = 0; Index < static_cast<int>(Query.Buttons.size()); Index++)
  {
    const CoreBridge::TQueryButton & B = Query.Buttons[Index];
    QPushButton * Button = Box->addButton(QString::fromStdString(B.Caption), QDialogButtonBox::ActionRole);
    // The first answer is the default, as upstream's.
    Button->setDefault(Index == 0);
    connect(Button, &QPushButton::clicked, this, [this, Index]() { Answer(Index); });
    FButtons.append(Button);
    if ((Query.Timeout > 0) && (B.Answer == Query.TimeoutAnswer))
    {
      FTimeoutButton = Button;
      FTimeoutCaption = Button->text();
    }
  }
  Layout->addWidget(Box);

  if (Query.Timeout > 0)
  {
    // Counted down on the answer it will give, as upstream's.
    FRemaining = (Query.Timeout + 999) / 1000;
    FTimer = new QTimer(this);
    connect(FTimer, &QTimer::timeout, this, &QueryDialog::Tick);
    FTimer->start(1000);
    Tick();
  }
}

void QueryDialog::Answer(int Index)
{
  const CoreBridge::TQueryButton & B = FQuery.Buttons[Index];
  unsigned int Result = B.Answer;
  if (B.OnSubmit)
  {
    // Acts first; closes only if it gives an answer (TMessageForm::ButtonSubmit).
    Result = B.OnSubmit();
    if (Result == 0)
    {
      return;
    }
  }
  FAnswer = Result;
  accept();
}

void QueryDialog::Tick()
{
  if (FRemaining == 0)
  {
    FTimer->stop();
    FAnswer = FQuery.TimeoutAnswer;
    accept();
    return;
  }
  if (FTimeoutButton != nullptr)
  {
    FTimeoutButton->setText(tr("%1 (%2 s)").arg(FTimeoutCaption).arg(FRemaining));
  }
  FRemaining--;
}

void QueryDialog::reject()
{
  FAnswer = FQuery.CancelAnswer;
  QDialog::reject();
}

CoreBridge::TQueryResult QueryDialog::Result() const
{
  CoreBridge::TQueryResult Result;
  Result.Answer = FAnswer;
  Result.NeverAskAgain = (FNeverAskAgain != nullptr) && FNeverAskAgain->isChecked();
  return Result;
}
