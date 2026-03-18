#include "mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setSamples(4); // Opcional: Esto activa el Anti-Aliasing para que los bordes del 3D no se vean aserrados
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    a.setStyle("fusion"); //linea para colocar el color de la ventana segun el tema de windows
    MainWindow w;
    w.show();
    return a.exec();
}
