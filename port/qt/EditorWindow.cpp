#include "EditorWindow.h"
#include "WindowParams.h"

#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStatusBar>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTextBlock>
#include <QVBoxLayout>

#include "CoreBridge.h"
#include "QtFrontend.h"
#include "FileList.h"

// Find and Replace, as the system's dialogs WinSCP's editor uses: modeless,
// beside the editor.
class FindDialog : public QDialog
{
public:
  FindDialog(EditorWindow * Editor, bool Replace, const EditorWindow::TFind & Initial) :
    QDialog(Editor),
    FEditor(Editor)
  {
    setWindowTitle(Replace ? EditorWindow::tr("Replace") : EditorWindow::tr("Find"));
    QVBoxLayout * Layout = new QVBoxLayout(this);
    QFormLayout * Fields = new QFormLayout();
    FText = new QLineEdit(Initial.Text, this);
    Fields->addRow(EditorWindow::tr("Fi&nd what:"), FText);
    if (Replace)
    {
      FReplaceText = new QLineEdit(Initial.ReplaceText, this);
      Fields->addRow(EditorWindow::tr("Re&place with:"), FReplaceText);
    }
    Layout->addLayout(Fields);
    QHBoxLayout * Row = new QHBoxLayout();
    QVBoxLayout * Checks = new QVBoxLayout();
    FWholeWord = new QCheckBox(EditorWindow::tr("Match &whole word only"), this);
    FWholeWord->setChecked(Initial.WholeWord);
    FMatchCase = new QCheckBox(EditorWindow::tr("Match &case"), this);
    FMatchCase->setChecked(Initial.MatchCase);
    Checks->addWidget(FWholeWord);
    Checks->addWidget(FMatchCase);
    Row->addLayout(Checks);
    QGroupBox * Direction = new QGroupBox(EditorWindow::tr("Direction"), this);
    QHBoxLayout * Directions = new QHBoxLayout(Direction);
    FUp = new QRadioButton(EditorWindow::tr("&Up"), Direction);
    QRadioButton * Down = new QRadioButton(EditorWindow::tr("&Down"), Direction);
    (Initial.Down ? Down : FUp)->setChecked(true);
    Directions->addWidget(FUp);
    Directions->addWidget(Down);
    Row->addWidget(Direction);
    Layout->addLayout(Row);
    QDialogButtonBox * Buttons = new QDialogButtonBox(this);
    QPushButton * Next = Buttons->addButton(EditorWindow::tr("&Find Next"), QDialogButtonBox::ActionRole);
    Next->setDefault(true);
    connect(Next, &QPushButton::clicked, this, [this]() { FEditor->Find(EditorWindow::TFindMode::Find, Options()); });
    if (Replace)
    {
      connect(Buttons->addButton(EditorWindow::tr("&Replace"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this,
        [this]() { FEditor->Find(EditorWindow::TFindMode::Replace, Options()); });
      connect(Buttons->addButton(EditorWindow::tr("Replace &All"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this,
        [this]() { FEditor->Find(EditorWindow::TFindMode::ReplaceAll, Options()); });
    }
    connect(Buttons->addButton(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &QDialog::reject);
    Layout->addWidget(Buttons);
    FText->selectAll();
  }

  bool Replace() const { return (FReplaceText != nullptr); }

  EditorWindow::TFind Options() const
  {
    EditorWindow::TFind Result;
    Result.Text = FText->text();
    Result.ReplaceText = (FReplaceText != nullptr) ? FReplaceText->text() : QString();
    Result.MatchCase = FMatchCase->isChecked();
    Result.WholeWord = FWholeWord->isChecked();
    Result.Down = !FUp->isChecked();
    return Result;
  }

private:
  EditorWindow * FEditor;
  QLineEdit * FText = nullptr;
  QLineEdit * FReplaceText = nullptr;
  QCheckBox * FWholeWord = nullptr;
  QCheckBox * FMatchCase = nullptr;
  QRadioButton * FUp = nullptr;
};

namespace
{

const char Utf8Bom[] = "\xEF\xBB\xBF";

// The system's code page as core takes it: Windows' ANSI one, Latin-1
// elsewhere (core's GetACP).
QStringConverter::Encoding DefaultEncoding()
{
#if defined(_WIN32)
  return QStringConverter::System;
#else
  return QStringConverter::Latin1;
#endif
}

// False when Data is not valid in the encoding.
bool Decode(const QByteArray & Data, bool Utf8, QString & Text)
{
  QStringDecoder Decoder(Utf8 ? QStringConverter::Utf8 : DefaultEncoding());
  Text = Decoder.decode(Data);
  return !Decoder.hasError();
}

}  // namespace

int EditorWindow::GInstances = 0;

EditorWindow::EditorWindow(const QString & FileName, const QString & Caption, const QString & AppName, QWidget * Parent) :
  QMainWindow(Parent),
  FFileName(FileName),
  FCaption(Caption.isEmpty() ? QDir::toNativeSeparators(FileName) : Caption),
  FAppName(AppName)
{
  // Its own window, as WinSCP's (parented to the desktop).
  setWindowFlag(Qt::Window);
  const CoreBridge::TEditorSettings Settings = CoreBridge::EditorSettings();
  FLastFind.Text = QString::fromStdString(Settings.FindText);
  FLastFind.ReplaceText = QString::fromStdString(Settings.ReplaceText);
  FLastFind.MatchCase = Settings.FindMatchCase;
  FLastFind.WholeWord = Settings.FindWholeWord;
  FUtf8 = Settings.Utf8;

  FEditor = new QPlainTextEdit(this);
  FEditor->setAccessibleName(tr("Text"));
  // As TEditorForm::ApplyConfiguration: the configured font, else the
  // system's fixed one; the colours, else the window's.
  QFont Font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  if (!Settings.FontName.empty())
  {
    Font.setFamily(QString::fromStdString(Settings.FontName));
  }
  if (Settings.FontSize > 0)
  {
    Font.setPointSize(Settings.FontSize);
  }
  Font.setBold(Settings.FontBold);
  Font.setItalic(Settings.FontItalic);
  Font.setUnderline(Settings.FontUnderline);
  Font.setStrikeOut(Settings.FontStrikeOut);
  FEditor->setFont(Font);
  QPalette Palette = FEditor->palette();
  if (Settings.BackgroundColor >= 0)
  {
    Palette.setColor(QPalette::Base, QColor(QRgb(Settings.BackgroundColor)));
  }
  if (Settings.FontColor >= 0)
  {
    Palette.setColor(QPalette::Text, QColor(QRgb(Settings.FontColor)));
  }
  FEditor->setPalette(Palette);
  FEditor->setLineWrapMode(Settings.WordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
  FEditor->setTabStopDistance(FEditor->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * qMax(1, Settings.TabSize));
  setCentralWidget(FEditor);

  // WinSCP's actions (Editor.dfm), its keys included.
  QMenu * FileMenu = menuBar()->addMenu(tr("&File"));
  QAction * SaveAction = FileMenu->addAction(tr("&Save"), this, [this]() { Save(); });
  SaveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
  // As SaveAllAction2 (Shift+Ctrl+S): every editor's.
  QAction * SaveAllAction = FileMenu->addAction(tr("Save &All"), this, [this]()
  {
    if (OnSaveAll)
    {
      OnSaveAll();
    }
    else
    {
      Save();
    }
  });
  SaveAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
  QAction * ReloadAction = FileMenu->addAction(tr("&Reload"), this, [this]() { Reload(); });
  ReloadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
  FileMenu->addSeparator();
  // Esc and F10 close it, as TEditorForm::FormKeyDown.
  QAction * CloseAction = FileMenu->addAction(tr("&Close"), this, &QWidget::close);
  CloseAction->setShortcuts({ QKeySequence(Qt::Key_Escape), QKeySequence(Qt::Key_F10) });

  QMenu * EditMenu = menuBar()->addMenu(tr("&Edit"));
  QAction * Undo = EditMenu->addAction(tr("&Undo"), FEditor, &QPlainTextEdit::undo);
  Undo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
  QAction * Redo = EditMenu->addAction(tr("Re&do"), FEditor, &QPlainTextEdit::redo);
  Redo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
  EditMenu->addSeparator();
  QAction * Cut = EditMenu->addAction(tr("Cu&t"), FEditor, &QPlainTextEdit::cut);
  Cut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
  QAction * Copy = EditMenu->addAction(tr("&Copy"), FEditor, &QPlainTextEdit::copy);
  Copy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
  QAction * Paste = EditMenu->addAction(tr("&Paste"), FEditor, &QPlainTextEdit::paste);
  Paste->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
  EditMenu->addAction(tr("&Delete"), this, [this]() { FEditor->textCursor().removeSelectedText(); });
  EditMenu->addSeparator();
  QAction * SelectAll = EditMenu->addAction(tr("Select &All"), FEditor, &QPlainTextEdit::selectAll);
  SelectAll->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
  connect(FEditor, &QPlainTextEdit::undoAvailable, Undo, &QAction::setEnabled);
  connect(FEditor, &QPlainTextEdit::redoAvailable, Redo, &QAction::setEnabled);
  connect(FEditor, &QPlainTextEdit::copyAvailable, Cut, &QAction::setEnabled);
  connect(FEditor, &QPlainTextEdit::copyAvailable, Copy, &QAction::setEnabled);
  Undo->setEnabled(false);
  Redo->setEnabled(false);
  Cut->setEnabled(false);
  Copy->setEnabled(false);

  QMenu * SearchMenu = menuBar()->addMenu(tr("&Search"));
  QAction * FindAction = SearchMenu->addAction(tr("&Find..."), this, [this]() { StartFind(false); });
  FindAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
  QAction * ReplaceAction = SearchMenu->addAction(tr("&Replace..."), this, [this]() { StartFind(true); });
  ReplaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
  QAction * FindNextAction = SearchMenu->addAction(tr("Find &Next"), this, [this]() { FindNext(); });
  FindNextAction->setShortcut(QKeySequence(Qt::Key_F3));
  QAction * GoToLineAction = SearchMenu->addAction(tr("&Go to Line Number..."), this, [this]()
  {
    bool OK = false;
    const QString Line = QInputDialog::getText(this, tr("Go to line"), tr("Line number:"), QLineEdit::Normal, QString(), &OK);
    if (OK)
    {
      bool Number = false;
      const int Value = Line.trimmed().toInt(&Number);
      GoToLine(Number ? Value : -1);
    }
  });
  GoToLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));

  QMenu * EncodingMenu = menuBar()->addMenu(tr("E&ncoding"));
  QActionGroup * Encodings = new QActionGroup(this);
  FDefaultEncodingAction = EncodingMenu->addAction(EncodingName(false), this, [this]() { SetUtf8(false); });
  FUtf8Action = EncodingMenu->addAction(EncodingName(true), this, [this]() { SetUtf8(true); });
  for (QAction * Action : { FDefaultEncodingAction, FUtf8Action })
  {
    Action->setCheckable(true);
    Encodings->addAction(Action);
  }

  for (int Index = 0; Index < 5; Index++)
  {
    FStatus[Index] = new QLabel(this);
    statusBar()->addWidget(FStatus[Index], (Index == 4) ? 1 : 0);
  }
  connect(FEditor, &QPlainTextEdit::cursorPositionChanged, this, &EditorWindow::UpdateStatus);
  connect(FEditor, &QPlainTextEdit::modificationChanged, this, &EditorWindow::UpdateStatus);
  UpdateTitle();
  resize(800, 600);
  // As TEditorForm::CreateParams: the first editor where the last first one
  // was left; the others where the system puts them.
  FInstance = GInstances++;
  if (FInstance == 0)
  {
    RestoreWindowParams(this, QString::fromStdString(CoreBridge::EditorSettings().WindowParams));
  }
  FWindowParams = StoreWindowParams(this);
}

// As ~TEditorForm: the first one's place kept, unless never moved.
EditorWindow::~EditorWindow()
{
  GInstances--;
  if (FInstance == 0)
  {
    const QString Params = StoreWindowParams(this);
    if (Params != FWindowParams)
    {
      CoreBridge::TEditorSettings Settings = CoreBridge::EditorSettings();
      Settings.WindowParams = Params.toStdString();
      try
      {
        CoreBridge::SetEditorSettings(Settings);
      }
      catch (const CoreBridge::TCoreError &)
      {
      }
    }
  }
}

QString EditorWindow::EncodingName(bool Utf8) const
{
  return Utf8 ? tr("UTF-8") : QString::fromStdString(CoreBridge::DefaultEncodingName());
}

void EditorWindow::UpdateTitle()
{
  // As WinSCP's: the file, the editor, the program.
  setWindowTitle(FCaption + QStringLiteral(" - ") + tr("Editor") + QStringLiteral(" - ") + FAppName);
}

bool EditorWindow::Modified() const
{
  return FEditor->document()->isModified();
}

bool EditorWindow::Load()
{
  return CheckFileSize() && LoadAs(FUtf8, true);
}

// As TEditorForm::CheckFileSize: a file larger than the configured size is
// asked about first, "never ask again" turning the warning off.
bool EditorWindow::CheckFileSize()
{
  CoreBridge::TEditorSettings Settings = CoreBridge::EditorSettings();
  const qint64 Size = QFileInfo(FFileName).size();
  if (!Settings.WarnOnLargeFile || (Size <= static_cast<qint64>(Settings.LargeFileSize) * 1024))
  {
    return true;
  }
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Confirmation;
  Query.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::EditorLargeFile))
    .replace(QStringLiteral("%s"), FormatBytes(Size)).remove(QStringLiteral("**")).toStdString();
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  CoreBridge::TQueryButton Cancel;
  Cancel.Answer = CoreBridge::AnswerCancel;
  Cancel.Caption = "Cancel";
  Query.Buttons = { OK, Cancel };
  Query.CancelAnswer = CoreBridge::AnswerCancel;
  Query.NeverAskAgainCheck = true;
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
  if (Answer.Answer != CoreBridge::AnswerOK)
  {
    return false;
  }
  if (Answer.NeverAskAgain)
  {
    Settings.WarnOnLargeFile = false;
    try
    {
      CoreBridge::SetEditorSettings(Settings);
    }
    catch (const CoreBridge::TCoreError &)
    {
      // Asked again next time.
    }
  }
  return true;
}

// Primary: the encoding is the configured one, so a file in the other is
// read in that one (and a BOM says UTF-8); otherwise it is the one asked
// for.
bool EditorWindow::LoadAs(bool Utf8, bool Primary)
{
  QFile File(FFileName);
  if (!File.open(QIODevice::ReadOnly))
  {
    QMessageBox::critical(this, windowTitle(), tr("Error loading file '%1'.").arg(QDir::toNativeSeparators(FFileName)));
    return false;
  }
  const QByteArray Data = File.readAll();
  bool CanTrySecondary = Primary;
  if (Primary && Data.startsWith(Utf8Bom))
  {
    Utf8 = true;
    CanTrySecondary = false;
  }
  QString Text;
  if (!Decode(Data, Utf8, Text))
  {
    QString Message = tr("Error loading file '%1' using '%2' encoding.").arg(QDir::toNativeSeparators(FFileName), EncodingName(Utf8));
    if (!CanTrySecondary || !Decode(Data, !Utf8, Text))
    {
      QMessageBox::critical(this, windowTitle(), Message);
      return false;
    }
    Utf8 = !Utf8;
    QMessageBox::information(this, windowTitle(), Message + QLatin1Char(' ') + tr("Falling back to '%1' encoding.").arg(EncodingName(Utf8)));
  }
  FUtf8 = Utf8;
  // The BOM, kept when saved (TPreambleFilteringFileStream).
  FBom = Utf8 && Data.startsWith(Utf8Bom);
  // The line endings, kept too: the first line's say.
  const qsizetype LineFeed = Text.indexOf(QLatin1Char('\n'));
  FCrLf = (LineFeed > 0) && (Text.at(LineFeed - 1) == QLatin1Char('\r'));
  if (FCrLf)
  {
    Text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  }
  FEditor->setPlainText(Text);
  FEditor->document()->setModified(false);
  UpdateStatus();
  return true;
}

bool EditorWindow::Save()
{
  // As TEditorForm::SaveFile: only what changed.
  if (!Modified())
  {
    return true;
  }
  FSaving = true;
  UpdateStatus();
  // The blocks' own text: QPlainTextEdit::toPlainText changes no-break
  // spaces to spaces.
  QStringList Lines;
  for (QTextBlock Block = FEditor->document()->begin(); Block.isValid(); Block = Block.next())
  {
    Lines.append(Block.text());
  }
  const QString Text = Lines.join(FCrLf ? QStringLiteral("\r\n") : QStringLiteral("\n"));
  QStringEncoder Encoder(FUtf8 ? QStringConverter::Utf8 : DefaultEncoding());
  QByteArray Data = Encoder.encode(Text);
  if (FBom)
  {
    Data.prepend(Utf8Bom);
  }
  QFile File(FFileName);
  bool Result = File.open(QIODevice::WriteOnly | QIODevice::Truncate) && (File.write(Data) == Data.size());
  File.close();
  if (!Result)
  {
    QMessageBox::critical(this, windowTitle(),
      tr("Cannot create file \"%1\". %2").arg(QDir::toNativeSeparators(FFileName), File.errorString()));
  }
  else if (OnFileChanged)
  {
    Result = OnFileChanged();
  }
  if (Result)
  {
    FEditor->document()->setModified(false);
  }
  FSaving = false;
  UpdateStatus();
  return Result;
}

bool EditorWindow::Reload()
{
  if (Modified() &&
      (QMessageBox::question(this, windowTitle(), tr("File was modified. Changes will be lost, if file is reloaded. Continue?"),
         QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok))
  {
    return false;
  }
  if (OnFileReload && !OnFileReload())
  {
    return false;
  }
  return LoadAs(FUtf8, false);
}

bool EditorWindow::SetUtf8(bool Utf8)
{
  if (Utf8 != FUtf8)
  {
    if (Modified() &&
        (QMessageBox::question(this, windowTitle(),
           tr("File was modified. Changes will be lost, if file is reloaded using different encoding. Continue?"),
           QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok))
    {
      UpdateStatus();
      return false;
    }
    if (!LoadAs(Utf8, false))
    {
      UpdateStatus();
      return false;
    }
  }
  return true;
}

void EditorWindow::StartFind(bool Replace)
{
  TFind Options = FLastFind;
  // As WinSCP's: a one-line selection is what to find.
  const QString Selected = FEditor->textCursor().selectedText();
  if (!Selected.isEmpty() && !Selected.contains(QChar::ParagraphSeparator))
  {
    Options.Text = Selected;
  }
  if ((FFindDialog != nullptr) && (FFindDialog->Replace() != Replace))
  {
    delete FFindDialog;
    FFindDialog = nullptr;
  }
  if (FFindDialog == nullptr)
  {
    FFindDialog = new FindDialog(this, Replace, Options);
  }
  FFindDialog->show();
  FFindDialog->raise();
  FFindDialog->activateWindow();
}

int EditorWindow::Find(TFindMode Mode, const TFind & Options)
{
  FLastFind = Options;
  // Kept for next time, as WinSCP's editor's configuration.
  CoreBridge::TEditorSettings Settings = CoreBridge::EditorSettings();
  Settings.FindText = Options.Text.toStdString();
  Settings.ReplaceText = Options.ReplaceText.toStdString();
  Settings.FindMatchCase = Options.MatchCase;
  Settings.FindWholeWord = Options.WholeWord;
  try
  {
    CoreBridge::SetEditorSettings(Settings);
  }
  catch (const CoreBridge::TCoreError &)
  {
    // Not kept: the search goes on.
  }
  if (Options.Text.isEmpty())
  {
    return 0;
  }
  QTextDocument::FindFlags Flags;
  if (Options.MatchCase)
  {
    Flags |= QTextDocument::FindCaseSensitively;
  }
  if (Options.WholeWord)
  {
    Flags |= QTextDocument::FindWholeWords;
  }
  if (!Options.Down)
  {
    Flags |= QTextDocument::FindBackward;
  }
  // Replace All: one step to undo.
  QTextCursor Group(FEditor->document());
  if (Mode == TFindMode::ReplaceAll)
  {
    Group.beginEditBlock();
  }
  int Count = 0;
  bool Found;
  do
  {
    // As TEditorForm::Find: the selection replaced when it is what is
    // looked for (in any case).
    QTextCursor Cursor = FEditor->textCursor();
    if ((Mode != TFindMode::Find) && Cursor.hasSelection() &&
        (Cursor.selectedText().compare(Options.Text, Qt::CaseInsensitive) == 0))
    {
      Cursor.insertText(Options.ReplaceText);
      FEditor->setTextCursor(Cursor);
      Count++;
    }
    Found = FEditor->find(Options.Text, Flags);
    if (Found && (Mode == TFindMode::Find))
    {
      Count++;
    }
  }
  while (Found && (Mode == TFindMode::ReplaceAll));
  if (Mode == TFindMode::ReplaceAll)
  {
    Group.endEditBlock();
  }
  if (!Found)
  {
    if (Count == 0)
    {
      QMessageBox::information(this, windowTitle(), tr("Could not find the string '%1'.").arg(Options.Text));
    }
    else if (Mode == TFindMode::ReplaceAll)
    {
      QMessageBox::information(this, windowTitle(), tr("Total replacements performed: %1").arg(Count));
    }
  }
  return Count;
}

int EditorWindow::FindNext()
{
  if (FLastFind.Text.isEmpty())
  {
    StartFind(false);
    return 0;
  }
  return Find(TFindMode::Find, FLastFind);
}

bool EditorWindow::GoToLine(int Line)
{
  const QTextBlock Block = FEditor->document()->findBlockByNumber(Line - 1);
  if ((Line <= 0) || !Block.isValid())
  {
    QMessageBox::warning(this, windowTitle(), tr("Invalid line number."));
    return false;
  }
  FEditor->setTextCursor(QTextCursor(Block));
  return true;
}

QString EditorWindow::StatusText(int Panel) const
{
  return ((Panel >= 0) && (Panel < 5)) ? FStatus[Panel]->text() : QString();
}

// As TEditorForm::UpdateControls.
void EditorWindow::UpdateStatus()
{
  const QTextCursor Cursor = FEditor->textCursor();
  const QString Line = Cursor.block().text();
  const int Position = Cursor.positionInBlock();
  FStatus[0]->setText(tr("Line: %1/%2").arg(Cursor.blockNumber() + 1).arg(FEditor->document()->blockCount()));
  // The column with tabs expanded.
  const int TabSize = qMax(1, CoreBridge::EditorSettings().TabSize);
  int Column = 0;
  for (int Index = 0; Index <= Position; Index++)
  {
    if ((Index >= 1) && (Line.at(Index - 1) == QLatin1Char('\t')))
    {
      Column = (((Column / TabSize) + 1) * TabSize) + 1;
    }
    else
    {
      Column++;
    }
  }
  FStatus[1]->setText(tr("Column: %1").arg(Column));
  QString Character;
  if (Position < Line.size())
  {
    int Code = Line.at(Position).unicode();
    if (!FUtf8)
    {
      // Its code in the code page, when it has one there.
      QStringEncoder Encoder(DefaultEncoding());
      const QByteArray Bytes = Encoder.encode(QString(Line.at(Position)));
      Code = (!Encoder.hasError() && (Bytes.size() == 1)) ? static_cast<unsigned char>(Bytes[0]) : -1;
    }
    if (Code >= 0)
    {
      Character = tr("Character: %1 (0x%2)").arg(Code).arg(Code, 2, 16, QLatin1Char('0'));
    }
  }
  FStatus[2]->setText(Character);
  FStatus[3]->setText(tr("Encoding: %1").arg(EncodingName(FUtf8)));
  FStatus[4]->setText(FSaving ? tr("Saving...") : (Modified() ? tr("Modified") : QString()));
  FDefaultEncodingAction->setChecked(!FUtf8);
  FUtf8Action->setChecked(FUtf8);
}

void EditorWindow::closeEvent(QCloseEvent * Event)
{
  if (Modified())
  {
    const QMessageBox::StandardButton Answer = QMessageBox::question(this, windowTitle(), tr("File was modified. Save changes?"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if ((Answer == QMessageBox::Cancel) || ((Answer == QMessageBox::Yes) && !Save()))
    {
      Event->ignore();
      return;
    }
  }
  if (FFindDialog != nullptr)
  {
    FFindDialog->close();
  }
  QMainWindow::closeEvent(Event);
}
