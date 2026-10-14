#include "RtfText.h"

#include <QStringList>

namespace
{

// TGenerateUrlDialog::UpdateControls' colour table, from 1.
const char * const Colors[] = { nullptr, "#010101", "#008000", "#008080", "#800000", "#0000ff", "#993333", "#808080" };

class TRtfReader
{
public:
  TRtfReader(const QString & Rtf, bool Html) : FRtf(Rtf), FHtml(Html) {}

  QString Read()
  {
    Group(false);
    Close();
    return FOut;
  }

private:
  const QString & FRtf;
  bool FHtml;
  int FPos = 0;
  QString FOut;
  int FColor = 0;
  bool FItalic = false;
  bool FSpanOpen = false;

  void Close()
  {
    if (FSpanOpen)
    {
      FOut += QStringLiteral("</span>");
      FSpanOpen = false;
    }
  }

  void Style()
  {
    if (!FHtml)
    {
      return;
    }
    Close();
    QString Css;
    if ((FColor > 0) && (FColor < 8))
    {
      Css += QStringLiteral("color:%1;").arg(QLatin1String(Colors[FColor]));
    }
    if (FItalic)
    {
      Css += QStringLiteral("font-style:italic;");
    }
    if (!Css.isEmpty())
    {
      FOut += QStringLiteral("<span style=\"%1\">").arg(Css);
      FSpanOpen = true;
    }
  }

  void Text(QChar Ch)
  {
    if (!FHtml)
    {
      FOut += Ch;
    }
    else if (Ch == QLatin1Char('<'))
    {
      FOut += QStringLiteral("&lt;");
    }
    else if (Ch == QLatin1Char('>'))
    {
      FOut += QStringLiteral("&gt;");
    }
    else if (Ch == QLatin1Char('&'))
    {
      FOut += QStringLiteral("&amp;");
    }
    else if (Ch == QLatin1Char(' '))
    {
      // Code's indentation is kept.
      FOut += (FOut.endsWith(QLatin1Char(' ')) || FOut.endsWith(QStringLiteral("<br>"))) ? QStringLiteral("&nbsp;") : QStringLiteral(" ");
    }
    else
    {
      FOut += Ch;
    }
  }

  // A control word at FPos (after the backslash): its name and parameter.
  bool Word(QString & Name, int & Parameter, bool & HasParameter)
  {
    Name.clear();
    while ((FPos < FRtf.size()) && FRtf.at(FPos).isLetter())
    {
      Name += FRtf.at(FPos++);
    }
    QString Number;
    if ((FPos < FRtf.size()) && ((FRtf.at(FPos) == QLatin1Char('-')) || FRtf.at(FPos).isDigit()))
    {
      Number += FRtf.at(FPos++);
      while ((FPos < FRtf.size()) && FRtf.at(FPos).isDigit())
      {
        Number += FRtf.at(FPos++);
      }
    }
    HasParameter = !Number.isEmpty();
    Parameter = Number.toInt();
    // A space ends a control word, and is part of it.
    if ((FPos < FRtf.size()) && (FRtf.at(FPos) == QLatin1Char(' ')))
    {
      FPos++;
    }
    return !Name.isEmpty();
  }

  // The rest of a group, with Skip its text not written.
  void Group(bool Skip)
  {
    const int Color = FColor;
    const bool Italic = FItalic;
    while (FPos < FRtf.size())
    {
      const QChar Ch = FRtf.at(FPos++);
      if (Ch == QLatin1Char('}'))
      {
        break;
      }
      if (Ch == QLatin1Char('{'))
      {
        if (FRtf.mid(FPos).startsWith(QStringLiteral("\\field")))
        {
          Field(Skip);
        }
        else if (FRtf.mid(FPos).startsWith(QStringLiteral("\\colortbl")) || FRtf.mid(FPos).startsWith(QStringLiteral("\\fonttbl")) ||
                 FRtf.mid(FPos).startsWith(QStringLiteral("\\*")))
        {
          Group(true);
        }
        else
        {
          Group(Skip);
        }
        continue;
      }
      if ((Ch == QLatin1Char('\n')) || (Ch == QLatin1Char('\r')))
      {
        continue;
      }
      if (Ch != QLatin1Char('\\'))
      {
        if (!Skip)
        {
          Text(Ch);
        }
        continue;
      }
      if (FPos >= FRtf.size())
      {
        break;
      }
      const QChar Next = FRtf.at(FPos);
      if ((Next == QLatin1Char('\\')) || (Next == QLatin1Char('{')) || (Next == QLatin1Char('}')))
      {
        FPos++;
        if (!Skip)
        {
          Text(Next);
        }
        continue;
      }
      if (Next == QLatin1Char('\''))
      {
        const int Code = FRtf.mid(FPos + 1, 2).toInt(nullptr, 16);
        FPos += 3;
        if (!Skip)
        {
          Text(QChar(Code));
        }
        continue;
      }
      QString Name;
      int Parameter = 0;
      bool HasParameter = false;
      if (!Word(Name, Parameter, HasParameter))
      {
        FPos++;
        continue;
      }
      if (Skip)
      {
        continue;
      }
      if (Name == QStringLiteral("par"))
      {
        Close();
        FOut += FHtml ? QStringLiteral("<br>") : QStringLiteral("\n");
        Style();
      }
      else if (Name == QStringLiteral("cf"))
      {
        FColor = Parameter;
        Style();
      }
      else if (Name == QStringLiteral("i"))
      {
        FItalic = !HasParameter || (Parameter != 0);
        Style();
      }
      else if (Name == QStringLiteral("u"))
      {
        Text(QChar(static_cast<char16_t>(Parameter < 0 ? Parameter + 65536 : Parameter)));
        // Its fallback character.
        if ((FPos < FRtf.size()) && (FRtf.at(FPos) == QLatin1Char('?')))
        {
          FPos++;
        }
      }
    }
    if ((FColor != Color) || (FItalic != Italic))
    {
      FColor = Color;
      FItalic = Italic;
      Style();
    }
  }

  // {\field{\*\fldinst{HYPERLINK "link" }}{\fldrslt{text}}}, the leading
  // brace read.
  void Field(bool Skip)
  {
    const int Instruction = FRtf.indexOf(QStringLiteral("HYPERLINK \""), FPos);
    const int End = (Instruction >= 0) ? FRtf.indexOf(QLatin1Char('"'), Instruction + 11) : -1;
    const QString Link = (End >= 0) ? FRtf.mid(Instruction + 11, End - Instruction - 11) : QString();
    const int Result = FRtf.indexOf(QStringLiteral("{\\fldrslt"), FPos);
    if (Result < 0)
    {
      Group(true);
      return;
    }
    FPos = Result + 9;
    const bool Html = FHtml && !Skip;
    if (Html)
    {
      Close();
      const QString Url = (Link.startsWith(QStringLiteral("http://")) || Link.startsWith(QStringLiteral("https://"))) ? Link :
        (QStringLiteral("https://winscp.net/eng/docs/") + Link);
      FOut += QStringLiteral("<a href=\"%1\" style=\"text-decoration:none\">").arg(Url.toHtmlEscaped());
      Style();
    }
    Group(Skip);
    if (Html)
    {
      Close();
      FOut += QStringLiteral("</a>");
      Style();
    }
    // The field's own closing brace.
    while ((FPos < FRtf.size()) && (FRtf.at(FPos) != QLatin1Char('}')))
    {
      FPos++;
    }
    FPos++;
  }
};

}  // namespace

QString RtfToHtml(const QString & Rtf)
{
  return TRtfReader(Rtf, true).Read();
}

QString RtfToText(const QString & Rtf)
{
  return TRtfReader(Rtf, false).Read();
}
