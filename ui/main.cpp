#include <QApplication>
#include <libquwrof/window/entry.hpp>

auto main(int argc, char* argv[]) -> int
{
  QApplication app(argc, argv);
  MainWindow   mainWindow;
  mainWindow.show();
  return app.exec();
}
