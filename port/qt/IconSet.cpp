#include "IconSet.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>

namespace
{

const QString SystemSet = QStringLiteral("system");
const QString NoSet = QStringLiteral("none");

QString GCurrent;

// A set's SVG drawn as a mask, filled with the palette's colour for the mode.
class TTintedSvgEngine : public QIconEngine
{
public:
  explicit TTintedSvgEngine(const QString & FileName) :
    FFileName(FileName)
  {
  }

  void paint(QPainter * Painter, const QRect & Rect, QIcon::Mode Mode, QIcon::State State) override
  {
    const qreal Scale = (Painter->device() != nullptr) ? Painter->device()->devicePixelRatioF() : 1.0;
    Painter->drawPixmap(Rect, scaledPixmap(Rect.size(), Mode, State, Scale));
  }

  QPixmap pixmap(const QSize & Size, QIcon::Mode Mode, QIcon::State State) override
  {
    return scaledPixmap(Size, Mode, State, 1.0);
  }

  QPixmap scaledPixmap(const QSize & Size, QIcon::Mode Mode, QIcon::State, qreal Scale) override
  {
    const QSize Pixels = (QSizeF(Size) * Scale).toSize();
    QImage Image(Pixels, QImage::Format_ARGB32_Premultiplied);
    Image.fill(Qt::transparent);
    {
      QSvgRenderer Renderer(FFileName);
      QPainter Painter(&Image);
      Painter.setRenderHint(QPainter::Antialiasing);
      Renderer.render(&Painter, QRectF(QPointF(0, 0), QSizeF(Pixels)));
      // The drawing's shape, in the palette's colour.
      Painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
      const QPalette Palette = QApplication::palette();
      QColor Color = Palette.color(QPalette::Normal, QPalette::WindowText);
      if (Mode == QIcon::Disabled)
      {
        Color = Palette.color(QPalette::Disabled, QPalette::WindowText);
      }
      else if (Mode == QIcon::Selected)
      {
        Color = Palette.color(QPalette::Normal, QPalette::HighlightedText);
      }
      Painter.fillRect(Image.rect(), Color);
    }
    QPixmap Result = QPixmap::fromImage(Image);
    Result.setDevicePixelRatio(Scale);
    return Result;
  }

  QSize actualSize(const QSize & Size, QIcon::Mode, QIcon::State) override
  {
    return Size;
  }

  QIconEngine * clone() const override
  {
    return new TTintedSvgEngine(FFileName);
  }

  QString key() const override
  {
    return QStringLiteral("linscp-tinted-svg");
  }

private:
  QString FFileName;
};

QIcon SetIcon(const QString & Set, const QString & Role)
{
  const QString FileName = QStringLiteral(":/iconsets/%1/%2.svg").arg(Set, Role);
  return QFile::exists(FileName) ? QIcon(new TTintedSvgEngine(FileName)) : QIcon();
}

}  // namespace

namespace IconSet
{

std::vector<TSet> Sets()
{
  std::vector<TSet> Result = {
    { QStringLiteral("lucide"), QStringLiteral("Lucide"), QStringLiteral(":/iconsets/lucide/LICENSE") },
    { QStringLiteral("tabler"), QStringLiteral("Tabler Icons"), QStringLiteral(":/iconsets/tabler/LICENSE") },
    { QStringLiteral("fluent"), QStringLiteral("Fluent UI System Icons"), QStringLiteral(":/iconsets/fluent/LICENSE") },
  };
#if !defined(Q_OS_WIN)
  // Windows has no icon theme.
  Result.push_back({ SystemSet, QApplication::translate("IconSet", "Desktop theme"), QString() });
#endif
  Result.push_back({ NoSet, QApplication::translate("IconSet", "None"), QString() });
  return Result;
}

QString DefaultSet()
{
#if defined(Q_OS_WIN)
  return QStringLiteral("fluent");
#else
  return QStringLiteral("lucide");
#endif
}

QString Current()
{
  for (const TSet & Set : Sets())
  {
    if (Set.Id == GCurrent)
    {
      return GCurrent;
    }
  }
  return DefaultSet();
}

void SetCurrent(const QString & Id)
{
  GCurrent = Id;
}

QString ThemeName(const QString & Role)
{
  static const QHash<QString, QString> Names = {
    { QStringLiteral("back"), QStringLiteral("go-previous") },
    { QStringLiteral("forward"), QStringLiteral("go-next") },
    { QStringLiteral("parent"), QStringLiteral("go-up") },
    { QStringLiteral("root"), QStringLiteral("go-top") },
    { QStringLiteral("home"), QStringLiteral("go-home") },
    { QStringLiteral("refresh"), QStringLiteral("view-refresh") },
    { QStringLiteral("new-folder"), QStringLiteral("folder-new") },
    { QStringLiteral("new-file"), QStringLiteral("document-new") },
    { QStringLiteral("delete"), QStringLiteral("edit-delete") },
    { QStringLiteral("rename"), QStringLiteral("edit-rename") },
    { QStringLiteral("properties"), QStringLiteral("document-properties") },
    { QStringLiteral("edit"), QStringLiteral("document-edit") },
    { QStringLiteral("open"), QStringLiteral("document-open") },
    { QStringLiteral("copy"), QStringLiteral("edit-copy") },
    { QStringLiteral("download"), QStringLiteral("go-down") },
    { QStringLiteral("upload"), QStringLiteral("go-up") },
    { QStringLiteral("find"), QStringLiteral("edit-find") },
    { QStringLiteral("filter"), QStringLiteral("view-filter") },
    { QStringLiteral("synchronize"), QStringLiteral("view-refresh") },
    { QStringLiteral("console"), QStringLiteral("utilities-terminal") },
    { QStringLiteral("putty"), QStringLiteral("utilities-terminal") },
    { QStringLiteral("sites"), QStringLiteral("network-server") },
    { QStringLiteral("new-tab"), QStringLiteral("tab-new") },
    { QStringLiteral("close-tab"), QStringLiteral("window-close") },
    { QStringLiteral("disconnect"), QStringLiteral("network-disconnect") },
    { QStringLiteral("reconnect"), QStringLiteral("network-connect") },
    { QStringLiteral("save-site"), QStringLiteral("document-save") },
    { QStringLiteral("save-workspace"), QStringLiteral("document-save-all") },
    { QStringLiteral("server-info"), QStringLiteral("dialog-information") },
    { QStringLiteral("preferences"), QStringLiteral("preferences-system") },
    { QStringLiteral("about"), QStringLiteral("help-about") },
    { QStringLiteral("quit"), QStringLiteral("application-exit") },
    { QStringLiteral("select-all"), QStringLiteral("edit-select-all") },
    { QStringLiteral("clipboard-copy"), QStringLiteral("edit-copy") },
    { QStringLiteral("paste"), QStringLiteral("edit-paste") },
    { QStringLiteral("bookmark"), QStringLiteral("bookmark-new") },
    { QStringLiteral("open-directory"), QStringLiteral("folder-open") },
    { QStringLiteral("explore"), QStringLiteral("system-file-manager") },
    { QStringLiteral("pause"), QStringLiteral("media-playback-pause") },
    { QStringLiteral("resume"), QStringLiteral("media-playback-start") },
    { QStringLiteral("cancel"), QStringLiteral("process-stop") },
    { QStringLiteral("change-password"), QStringLiteral("dialog-password") },
    { QStringLiteral("clear-caches"), QStringLiteral("edit-clear") },
  };
  return Names.value(Role);
}

QIcon Icon(const QString & Role)
{
  const QString Set = Current();
  if (Set == NoSet)
  {
    return QIcon();
  }
  if (Set == SystemSet)
  {
    // The theme's, else the bundled default's, so that no command lacks one.
    const QString Name = ThemeName(Role);
    const QIcon Fallback = SetIcon(QStringLiteral("lucide"), Role);
    return Name.isEmpty() ? Fallback : QIcon::fromTheme(Name, Fallback);
  }
  return SetIcon(Set, Role);
}

}  // namespace IconSet
