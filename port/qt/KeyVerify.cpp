#include "KeyVerify.h"

#include <QCoreApplication>
#include <QFileDialog>

#include "CoreBridge.h"
#include "QtFrontend.h"

namespace
{

unsigned Ask(QWidget * Window, CoreBridge::TQueryType Type, const std::string & Message,
  const std::vector<std::pair<unsigned, const char *>> & Buttons, unsigned CancelAnswer)
{
  CoreBridge::TQuery Query;
  Query.Type = Type;
  Query.Message = Message;
  for (const auto & Button : Buttons)
  {
    CoreBridge::TQueryButton QueryButton;
    QueryButton.Answer = Button.first;
    QueryButton.Caption = Button.second;
    Query.Buttons.push_back(QueryButton);
  }
  Query.CancelAnswer = CancelAnswer;
  QtFrontend Frontend;
  Frontend.SetWindow(Window);
  return Frontend.Query(Query).Answer;
}

}  // namespace

QString ChooseConvertedKeyFile(QWidget * Window, const QString & Suggested)
{
  return QFileDialog::getSaveFileName(Window, QCoreApplication::translate("SiteManager", "Save converted private key"),
    Suggested, QCoreApplication::translate("SiteManager", "PuTTY Private Key Files (*.ppk)"));
}

bool VerifyAndConvertKey(QWidget * Window, QString & FileName, bool CanIgnore,
  const std::function<QString(const QString & Suggested)> & ChooseTarget)
{
  const CoreBridge::TKeyCheck Check = CoreBridge::CheckKey(FileName.toStdString());
  if (Check.Status == CoreBridge::TKeyCheck::TStatus::Convertible)
  {
    if (Ask(Window, CoreBridge::TQueryType::Confirmation, Check.Message,
          { { CoreBridge::AnswerOK, "OK" }, { CoreBridge::AnswerCancel, "Cancel" } }, CoreBridge::AnswerCancel) !=
        CoreBridge::AnswerOK)
    {
      return false;
    }
    const QString Target = ChooseTarget(QString::fromStdString(Check.ConvertedFileName));
    if (Target.isEmpty())
    {
      return false;
    }
    try
    {
      Ask(Window, CoreBridge::TQueryType::Information, CoreBridge::ConvertKey(FileName.toStdString(), Target.toStdString()),
        { { CoreBridge::AnswerOK, "OK" } }, CoreBridge::AnswerOK);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      if (!E.Aborted)
      {
        QtFrontend Frontend;
        Frontend.SetWindow(Window);
        Frontend.ShowError(E.Error);
      }
      return false;
    }
    FileName = Target;
    return true;
  }
  if (Check.Status == CoreBridge::TKeyCheck::TStatus::Unusable)
  {
    // Upstream's: Ignore to go on anyway (where it may), Abort to stop.
    if (!CanIgnore)
    {
      Ask(Window, CoreBridge::TQueryType::Warning, Check.Message, { { CoreBridge::AnswerOK, "OK" } }, CoreBridge::AnswerOK);
      return false;
    }
    return Ask(Window, CoreBridge::TQueryType::Warning, Check.Message,
      { { CoreBridge::AnswerAbort, "&Abort" }, { CoreBridge::AnswerIgnore, "&Ignore" } }, CoreBridge::AnswerAbort) ==
      CoreBridge::AnswerIgnore;
  }
  return true;
}
