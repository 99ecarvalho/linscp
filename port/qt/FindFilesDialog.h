// Find files (Alt+F7, F3 with Explorer's keys), as WinSCP's
// (forms/FileFind.cpp): the server searched below a directory for files
// matching a mask, each listed as it is found; a file focused in the
// remote panel or edited, files downloaded or deleted, the results copied.
#ifndef LINSCP_QT_FIND_FILES_DIALOG_H
#define LINSCP_QT_FIND_FILES_DIALOG_H

#include <QDialog>

#include <vector>

#include "CoreBridge.h"

class MainWindow;
class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;

class FindFilesDialog : public QDialog
{
  Q_OBJECT

public:
  FindFilesDialog(MainWindow * Window, const QString & Directory);

  // The search, from the dialog's directory and mask; again to stop it.
  void Start();
  void Stop();
  bool Finding() const { return FFinding; }

  // Its buttons' actions on the files selected, for tests and them alike.
  void Focus();
  void Edit();
  void Download();
  void Delete();
  void CopyResults();

  QComboBox * MaskCombo() const { return FMask; }
  QComboBox * DirectoryCombo() const { return FDirectory; }
  QTreeWidget * Results() const { return FResults; }
  QLabel * Status() const { return FStatus; }
  QPushButton * StartButton() const { return FStart; }

protected:
  // A search in progress stops first, as upstream's FormCloseQuery.
  void closeEvent(QCloseEvent * Event) override;

private:
  struct TFound
  {
    QString Directory;
    CoreBridge::TRemoteEntry Entry;
  };
  std::vector<int> SelectedIndexes() const;
  void UpdateControls();

  MainWindow * FWindow;
  QComboBox * FMask = nullptr;
  QComboBox * FDirectory = nullptr;
  QPushButton * FStart = nullptr;
  QTreeWidget * FResults = nullptr;
  QLabel * FStatus = nullptr;
  QPushButton * FFocus = nullptr;
  QPushButton * FEdit = nullptr;
  QPushButton * FDownload = nullptr;
  QPushButton * FDelete = nullptr;
  QPushButton * FCopy = nullptr;
  std::vector<TFound> FFound;
  bool FFinding = false;
  bool FAbort = false;
};

#endif
