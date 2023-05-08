#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <iostream>
#include "weather_api.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);


    QQmlApplicationEngine engine;
    engine.load(QStringLiteral("qrc:/main.qml"));

    std::cout << "bef\n";
    WeatherApi w{};
    (void)w;

    return app.exec();
}
