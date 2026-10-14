// The RTF core's code generators write (Common.cpp's Rtf* helpers: colour
// table entries \cfN, italics, \par, escapes, \uN, HYPERLINK fields), as
// HTML to show and as plain text to copy, as upstream's rich edit and
// RtfRemoveHyperlinks have them.
#ifndef LINSCP_QT_RTF_TEXT_H
#define LINSCP_QT_RTF_TEXT_H

#include <QString>

// Colours: TGenerateUrlDialog's table. Links that are not http(s) are
// WinSCP's documentation's pages.
QString RtfToHtml(const QString & Rtf);
QString RtfToText(const QString & Rtf);

#endif
