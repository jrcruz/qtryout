#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "weather_api.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    WeatherApi w;

    QQmlApplicationEngine engine;
    qmlRegisterSingletonInstance("jcruz.eu", 1, 0, "WeatherAPI", &w);
    engine.load(QStringLiteral("qrc:/main.qml"));

    return app.exec();
}
