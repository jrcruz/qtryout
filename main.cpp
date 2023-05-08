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


    WeatherApi w;


    QQmlApplicationEngine engine;
    qmlRegisterSingletonInstance("jcruz.eu", 1, 0, "WeatherAPI", &w);
    engine.load(QStringLiteral("qrc:/main.qml"));

    std::cout << "bef\n";

    return app.exec();
}
