// The accessibility pass (P3-09), as a check on every window the GUI tests
// show: each window audited the first time it appears (by class and title).
//
//  - every control that takes the focus has a name for screen readers
//    (QAccessible's Name: its text, its buddy label's, or an accessible
//    name given);
//  - a label's mnemonic (&) reaches a control: the label has a buddy;
//  - no two of a window's visible controls share a mnemonic, nor two items
//    of one menu (the menu bar's included).
//
// An issue is a test failure (Failures), unless LINSCP_A11Y_REPORT is set:
// then it is only listed ("A11Y ...").
#ifndef LINSCP_TESTS_ACCESSIBILITY_AUDIT_H
#define LINSCP_TESTS_ACCESSIBILITY_AUDIT_H

#include <QObject>
#include <QSet>
#include <QString>

class QMenu;
class QWidget;

class AccessibilityAudit : public QObject
{
public:
  explicit AccessibilityAudit(int & Failures);
  // Audits Window now (its menus too), whether seen before or not.
  void Audit(QWidget * Window);
  void AuditMenu(QMenu * Menu, const QString & Where);
  int Issues() const { return FIssues; }

protected:
  bool eventFilter(QObject * Object, QEvent * Event) override;

private:
  void Issue(const QString & Where, const QString & What);

  int & FFailures;
  int FIssues = 0;
  bool FReportOnly = false;
  QSet<QString> FSeen;
};

#endif
