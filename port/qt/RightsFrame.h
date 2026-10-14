// Permissions, as WinSCP's rights frame (forms/Rights.cpp): read, write and
// execute for the owner, the group and others, the special bits, the octal
// form, "add X to directories", and its menu. With several files a bit can
// be mixed (a partially checked box), left as each file has it.
#ifndef LINSCP_QT_RIGHTS_FRAME_H
#define LINSCP_QT_RIGHTS_FRAME_H

#include <QWidget>

#include "CoreBridge.h"

class QCheckBox;
class QLineEdit;

class RightsFrame : public QWidget
{
  Q_OBJECT

public:
  explicit RightsFrame(QWidget * Parent = nullptr);

  void SetRights(const CoreBridge::TRightsState & Rights);
  CoreBridge::TRightsState Rights() const;
  // Bits may be left mixed: several files.
  void SetAllowUndef(bool Allow);
  bool AllowUndef() const { return FAllowUndef; }
  // "Add X to directories", shown when there are directories.
  void SetAllowAddXToDirectories(bool Allow);
  void SetAddXToDirectories(bool Add);
  bool AddXToDirectories() const;
  // As TRightsFrame::Text: core's "rwxr-xr-x", and "add X" with it.
  QString Text() const;

  // Its controls and menu's actions, for tests: the boxes in core's
  // TRight order (set UID, set GID, sticky, then owner, group, others'
  // read, write, execute).
  QCheckBox * Check(int Right) const { return FChecks[Right]; }
  QLineEdit * OctalEdit() const { return FOctal; }
  void NoRights();
  void DefaultRights();
  void AllRights();
  void LeaveAsIs();
  void CopyAsText();
  void CopyAsOctal();
  void Paste();

signals:
  void Changed();

protected:
  void contextMenuEvent(QContextMenuEvent * Event) override;

private:
  void UpdateControls();
  void OctalEdited();

  QCheckBox * FChecks[12] = {};
  QLineEdit * FOctal = nullptr;
  QCheckBox * FDirectoriesX = nullptr;
  bool FAllowUndef = false;
  bool FUpdating = false;
};

#endif
