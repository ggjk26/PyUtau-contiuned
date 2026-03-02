#include "gui/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    pyutau::gui::MainWindow window;
    window.show();

    return app.exec();
}
