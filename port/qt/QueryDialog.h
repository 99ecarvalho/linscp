// Core's question with its answers, as upstream's MoreMessageDialog asks it
// (forms/MessageDlg.cpp): a message, details apart, one button per answer in
// upstream's order and captions, an optional "Never ask me again", and an
// optional timeout that answers by itself.
#ifndef LINSCP_QT_QUERY_DIALOG_H
#define LINSCP_QT_QUERY_DIALOG_H

#include <QDialog>
#include <QList>

#include "CoreBridge.h"

class QCheckBox;
class QPushButton;
class QTimer;

class QueryDialog : public QDialog
{
  Q_OBJECT

public:
  QueryDialog(const CoreBridge::TQuery & Query, QWidget * Parent = nullptr);

  // After exec(): the answer (CancelAnswer when closed), and the box.
  CoreBridge::TQueryResult Result() const;
  // The answers' buttons, in order: for tests.
  QList<QPushButton *> Buttons() const { return FButtons; }
  QCheckBox * NeverAskAgain() const { return FNeverAskAgain; }

  void reject() override;

private:
  void Answer(int Index);
  void Tick();

  CoreBridge::TQuery FQuery;
  QList<QPushButton *> FButtons;
  QCheckBox * FNeverAskAgain = nullptr;
  QTimer * FTimer = nullptr;
  QPushButton * FTimeoutButton = nullptr;
  QString FTimeoutCaption;
  unsigned int FRemaining = 0;
  unsigned int FAnswer = 0;
};

#endif
