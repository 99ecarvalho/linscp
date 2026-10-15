// WinSCP's hint labels (HintLabel: "mask hints", "patterns"), as buttons
// the keyboard reaches too: the hint shown on hover, as there, and when
// pressed.
#ifndef LINSCP_QT_HINT_BUTTON_H
#define LINSCP_QT_HINT_BUTTON_H

#include <QString>

#include "CoreBridge.h"

#include <initializer_list>

class QPushButton;
class QWidget;

QPushButton * MakeHintButton(const QString & Caption, const QString & Hint, QWidget * Parent);
// A mask field's: core's hint texts, as the dialog's form lists them
// (MASK_HINT2, FILE_MASK_EX_HINT, ...), each a paragraph.
QString MaskHint(std::initializer_list<CoreBridge::TText> Parts);
QPushButton * MakeMaskHintButton(const QString & Caption, std::initializer_list<CoreBridge::TText> Parts, QWidget * Parent);

#endif
