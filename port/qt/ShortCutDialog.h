// Select keyboard shortcut, as WinSCP's (forms/Custom.cpp,
// TShortCutDialog): a shared bookmark's shortcut, from the Open directory
// and Location Profiles dialogs. The choices are WinSCP's for its custom
// shortcuts (InitializeShortCutCombo), which the Custom command dialog's
// shortcut box uses too.
#ifndef LINSCP_QT_SHORTCUT_DIALOG_H
#define LINSCP_QT_SHORTCUT_DIALOG_H

#include <QDialog>

#include <vector>

class QComboBox;

// None, Ctrl+0..9 and Shift+Ctrl+Alt+A..Z, the VCL's TShortCuts as item
// data, but those Used has; Current selected, added where it belongs if it
// is not offered (SetShortCutCombo).
void FillShortCutCombo(QComboBox * Combo, const std::vector<unsigned> & Used, unsigned Current);

// The shortcuts in use: the custom commands' and the extensions' (WinConfiguration's
// CustomCommandShortCuts).
std::vector<unsigned> CustomCommandShortCuts();

class ShortCutDialog : public QDialog
{
  Q_OBJECT

public:
  ShortCutDialog(const std::vector<unsigned> & Used, unsigned Current, QWidget * Parent = nullptr);

  unsigned ShortCut() const;
  QComboBox * Combo() const { return FCombo; }

private:
  QComboBox * FCombo = nullptr;
};

#endif
