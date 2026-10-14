// linscp, the Qt shell: core started as WinSCP.exe starts it, under a Qt
// event loop, in the system's language or --lang=<locale>'s. `--smoke` shows
// the main window, reports what it runs on and quits once the event loop has
// run: ctest gui_smoke.
#include <QApplication>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "CoreBridge.h"
#include "Language.h"
#include "MainWindow.h"
#include "QtFrontend.h"

int main(int argc, char ** argv)
{
  bool Smoke = false;
  for (int Index = 1; Index < argc; Index++)
  {
    Smoke = Smoke || (std::strcmp(argv[Index], "--smoke") == 0);
  }

  QApplication App(argc, argv);
  // Core's questions while it starts go to dialogs of their own; once the
  // window exists, they belong to it.
  QtFrontend Frontend;
  try
  {
    CoreBridge::Initialize(&Frontend);
  }
  catch (const std::runtime_error & E)
  {
    if (Smoke)
    {
      std::fprintf(stderr, "core did not start: %s\n", E.what());
    }
    else
    {
      QMessageBox::critical(nullptr, QStringLiteral("linscp"), QString::fromUtf8(E.what()));
    }
    return 1;
  }
  // As upstream: core's configuration chose the language as it started.
  const TLanguage Language = SetUpLanguage(RequestedLocale(argc, argv));

  int Result;
  {
    const CoreBridge::TInfo Info = CoreBridge::Info();
    MainWindow Window(Info);
    Frontend.SetWindow(&Window);
    Frontend.SetStatus([&Window](const QString & Text) { Window.statusBar()->showMessage(Text); });
    Window.show();
    if (!Smoke)
    {
      // As WinSCP starts: with its Login dialog, the site manager.
      QTimer::singleShot(0, &Window, [&Window]() { Window.SiteManagerDialog(); });
    }
    if (Smoke)
    {
      QTimer::singleShot(0, &Window, [&Window, &Info, &Language]()
      {
        std::printf("%s %s, Qt %s, %s: ok (%s; core %s, WinSCP %s)\n", Info.AppName.c_str(), Info.Version.c_str(), qVersion(),
          QGuiApplication::platformName().toUtf8().constData(), Language.Locale.toUtf8().constData(),
          Language.CoreCode.isEmpty() ? "English" : Language.CoreCode.toUtf8().constData(), Info.WinScpVersion.c_str());
        std::fflush(stdout);
        Window.close();
      });
    }
    Result = App.exec();
    Frontend.SetWindow(nullptr);
    Frontend.SetStatus(nullptr);
  }
  CoreBridge::Finalize();
  return Result;
}
