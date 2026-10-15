// The internal editor, as WinSCP's (forms/Editor.cpp): a text file in a
// window of its own, saved where it was opened from, with WinSCP's find and
// replace, go to line, encodings (UTF-8, else the system's code page) and
// status bar. Remote files are the main window's to download and upload
// back (OnFileChanged, P3-05, ADR 0015).
#ifndef LINSCP_QT_EDITOR_WINDOW_H
#define LINSCP_QT_EDITOR_WINDOW_H

#include <QMainWindow>

#include <functional>

class QAction;
class QLabel;
class QPlainTextEdit;
class FindDialog;

class EditorWindow : public QMainWindow
{
  Q_OBJECT

public:
  // Caption: what the title says the file is (the remote path; the file
  // name when empty).
  EditorWindow(const QString & FileName, const QString & Caption, const QString & AppName, QWidget * Parent = nullptr);
  ~EditorWindow() override;

  // Reads the file, as TEditorForm::LoadFromFile: UTF-8 when it starts with
  // its BOM, else the configured encoding, falling back to the other where
  // the text is not valid in it (with a message). False when it could not
  // be read (the error shown).
  bool Load();
  // The large-file question (TEditorForm::CheckFileSize): false when declined.
  bool CheckFileSize();
  // Writes the text back in its encoding, BOM and line endings, then calls
  // OnFileChanged. False when it was not saved (or OnFileChanged declined:
  // the text stays modified).
  bool Save();
  // Reads the file again, after asking when modified.
  bool Reload();
  // The encoding: UTF-8, else the system's; reloaded, after asking when
  // modified.
  bool SetUtf8(bool Utf8);

  struct TFind
  {
    QString Text;
    QString ReplaceText;
    bool MatchCase = false;
    bool WholeWord = false;
    bool Down = true;
  };
  enum class TFindMode { Find, Replace, ReplaceAll };
  // As TEditorForm::Find: the next occurrence selected (Replace: the
  // selected one replaced first; ReplaceAll: every one from here), WinSCP's
  // messages when there is no more. The options are kept as the next
  // search's. The number found or replaced.
  int Find(TFindMode Mode, const TFind & Options);
  // F3: the last search again.
  int FindNext();
  // As TEditorForm::GoToLine: false (and the message) when there is no such
  // line.
  bool GoToLine(int Line);

  // Called after the file is written: the main window's upload. False
  // keeps the text modified.
  std::function<bool()> OnFileChanged;
  // Reload's, before the file is read again: a remote file downloaded again
  // (false: not reloaded). Save All's: every editor saved.
  std::function<bool()> OnFileReload;
  std::function<void()> OnSaveAll;

  QPlainTextEdit * Editor() const { return FEditor; }
  QString FileName() const { return FFileName; }
  bool Modified() const;
  bool Utf8() const { return FUtf8; }
  bool Bom() const { return FBom; }
  bool CrLf() const { return FCrLf; }
  // The status bar's panels: line, column, character, encoding, state.
  QString StatusText(int Panel) const;

protected:
  // Asks to save a modified file, as TEditorForm::FormCloseQuery.
  void closeEvent(QCloseEvent * Event) override;

private:
  // TEditorForm's FInstances, FInstance, FWindowParams.
  static int GInstances;
  int FInstance = 0;
  QString FWindowParams;
  bool LoadAs(bool Utf8, bool Primary);
  void StartFind(bool Replace);
  void UpdateStatus();
  void UpdateTitle();
  QString EncodingName(bool Utf8) const;

  QString FFileName;
  QString FCaption;
  QString FAppName;
  QPlainTextEdit * FEditor = nullptr;
  QLabel * FStatus[5] = {};
  QAction * FDefaultEncodingAction = nullptr;
  QAction * FUtf8Action = nullptr;
  FindDialog * FFindDialog = nullptr;
  bool FUtf8 = true;
  bool FBom = false;
  bool FCrLf = false;
  bool FSaving = false;
  TFind FLastFind;
};

#endif
