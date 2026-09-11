#include "mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>

#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setSamples(4); // Opcional: Esto activa el Anti-Aliasing para que los bordes del 3D no se vean aserrados
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion")); // Estilo Fusion nativo de Qt para funcionamiento correcto de todos los controles y spinboxes
    MainWindow w;
    w.show();
    return a.exec();
}
