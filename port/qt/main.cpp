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
#include <memory>
#include <stdexcept>

#include "CoreBridge.h"
#include "Language.h"
#include "MainWindow.h"
#include "QtFrontend.h"
#include "SingleInstance.h"

int main(int argc, char ** argv)
{
  bool Smoke = false;
  for (int Index = 1; Index < argc; Index++)
  {
    Smoke = Smoke || (std::strcmp(argv[Index], "--smoke") == 0);
  }

  QApplication App(argc, argv);
  // Core sees what Qt leaves of the command line: its own switches and the
  // session to open, as WinSCP.exe's.
  {
    std::vector<std::string> Arguments;
    for (const QString & Argument : App.arguments().mid(1))
    {
      Arguments.push_back(Argument.toStdString());
    }
    CoreBridge::SetArguments(Arguments);
  }
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
#if defined(_WIN32)
  // The installer's maintenance tasks: done, nothing else is.
  try
  {
    if (CoreBridge::CommandLineSetupTask())
    {
      CoreBridge::Finalize();
      return 0;
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Frontend.ShowError(E.Error);
    CoreBridge::Finalize();
    return 1;
  }
#endif
  // As WinMain: a session named goes to the window already running, if it
  // takes it (Shift held, as OpenInNewWindow, the other way round).
  if (!Smoke &&
      CoreBridge::CommandLineForAnotherInstance(QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) &&
      SingleInstance::Send(App.arguments().mid(1)))
  {
    CoreBridge::Finalize();
    return 0;
  }

  int Result;
  {
    const CoreBridge::TInfo Info = CoreBridge::Info();
    MainWindow Window(Info);
    Frontend.SetWindow(&Window);
    Frontend.SetStatus([&Window](const QString & Text) { Window.statusBar()->showMessage(Text); });
    Window.show();
    std::unique_ptr<InstanceServer> Server;
    if (!Smoke)
    {
      Server.reset(new InstanceServer([&Window](const QStringList & Arguments)
      {
        return Window.CommandLineFromAnotherInstance(Arguments);
      }));
      // As WinSCP starts: the session the command line names, else its
      // Login dialog, the site manager.
      QTimer::singleShot(0, &Window, [&Window]() { Window.StartUp(); });
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
