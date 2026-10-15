// Remote files' properties, as WinSCP's Properties dialog
// (forms/Properties.cpp): what is selected, where, its size (calculated on
// request for directories), where a link points, the group, owner and
// permissions to change, recursively for directories, and the server's
// checksums, and S3's tags.
#ifndef LINSCP_QT_PROPERTIES_DIALOG_H
#define LINSCP_QT_PROPERTIES_DIALOG_H

#include <QDialog>

#include <functional>

#include <string>
#include <vector>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class RightsFrame;

class PropertiesDialog : public QDialog
{
  Q_OBJECT

public:
  // Names: entries of Directory, the session's current one.
  PropertiesDialog(CoreBridge::TSession * Session, const std::vector<std::string> & Names, const QString & Directory,
    const CoreBridge::TPropertiesInfo & Info, QWidget * Parent = nullptr);

  // What the dialog says now.
  CoreBridge::TFileProperties Properties() const;
  // Its buttons' actions, for tests and them alike. False when core failed
  // (its error shown).
  bool CalculateSize();
  bool CalculateChecksum();

  // Its controls, for tests.
  QLineEdit * FileEdit() const { return FFile; }
  QLineEdit * SizeEdit() const { return FSize; }
  QLineEdit * LinkEdit() const { return FLink; }
  QComboBox * OwnerCombo() const { return FOwner; }
  QComboBox * GroupCombo() const { return FGroup; }
  RightsFrame * Rights() const { return FRights; }
  QCheckBox * RecursiveCheck() const { return FRecursive; }
  QPushButton * OkButton() const { return FOk; }
  QPushButton * CalculateButton() const { return FCalculate; }
  QTabWidget * Tabs() const { return FTabs; }
  QComboBox * ChecksumAlgorithmCombo() const { return FChecksumAlgorithm; }
  QLineEdit * ChecksumEdit() const { return FChecksum; }
  QTreeWidget * ChecksumList() const { return FChecksums; }
  // The Tags page (S3): null without one.
  QTreeWidget * TagList() const { return FTags; }
  QPushButton * AddTagButton() const { return FAddTag; }
  QPushButton * EditTagButton() const { return FEditTag; }
  QPushButton * RemoveTagButton() const { return FRemoveTag; }
  // The Tags page's buttons' actions. False when cancelled.
  bool AddEditTag(bool Add);
  void RemoveTag();

  // What opens the secondary shell for checksums first, when needed
  // (the window's); false to give up.
  void SetChecksumSession(std::function<bool()> Ensure) { FChecksumSession = std::move(Ensure); }

private:
  std::function<bool()> FChecksumSession;
  void LoadStats(std::int64_t Size, int Files, int Directories, int SymLinks, bool Calculated);
  void UpdateControls();
  void ShowError(const CoreBridge::TCoreError & E);

  CoreBridge::TSession * FSession;
  std::vector<std::string> FNames;
  CoreBridge::TPropertiesInfo FInfo;
  bool FMultiple = false;
  QTabWidget * FTabs = nullptr;
  QLineEdit * FFile = nullptr;
  QLineEdit * FSize = nullptr;
  QLineEdit * FLink = nullptr;
  QPushButton * FCalculate = nullptr;
  QComboBox * FOwner = nullptr;
  QComboBox * FGroup = nullptr;
  RightsFrame * FRights = nullptr;
  QCheckBox * FRecursive = nullptr;
  QPushButton * FOk = nullptr;
  QComboBox * FChecksumAlgorithm = nullptr;
  QLineEdit * FChecksum = nullptr;
  QTreeWidget * FChecksums = nullptr;
  QTreeWidget * FTags = nullptr;
  QPushButton * FAddTag = nullptr;
  QPushButton * FEditTag = nullptr;
  QPushButton * FRemoveTag = nullptr;
};

#endif
