// linscp, the Qt shell: core started as WinSCP.exe starts it, under a Qt
// event loop. `--smoke` shows the main window, reports what it runs on and
// quits once the event loop has run: ctest gui_smoke.
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "CoreBridge.h"
#include "MainWindow.h"

int main(int argc, char ** argv)
{
  bool Smoke = false;
  for (int Index = 1; Index < argc; Index++)
  {
    Smoke = Smoke || (std::strcmp(argv[Index], "--smoke") == 0);
  }

  QApplication App(argc, argv);
  try
  {
    CoreBridge::Initialize();
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

  int Result;
  {
    const CoreBridge::TInfo Info = CoreBridge::Info();
    MainWindow Window(Info);
    Window.show();
    if (Smoke)
    {
      QTimer::singleShot(0, &Window, [&Window, &Info]()
      {
        std::printf("%s %s, Qt %s, %s: ok\n", Info.AppName.c_str(), Info.Version.c_str(), qVersion(),
          QGuiApplication::platformName().toUtf8().constData());
        std::fflush(stdout);
        Window.close();
      });
    }
    Result = App.exec();
  }
  CoreBridge::Finalize();
  return Result;
}
