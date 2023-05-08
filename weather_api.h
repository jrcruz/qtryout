#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QPointF>
#include <optional>
#include <QNetworkAccessManager>



struct WeatherData {
    QPointF coor;
    float temperature;
    std::optional<float> windspeed;
    std::optional<float> pressure;
};







class WeatherApi : public QObject
{
    Q_OBJECT

    QNetworkAccessManager net;
    QTimer* timer;
    QHash<QPointF, QString> stations;

public:
    WeatherApi();


public slots:
    void getData(QNetworkReply* r); // emits processData
    void processData(QList<WeatherData>); //emits readyToDraw

signals:
    // sent by getData, caught by processData
    void readyToProcessData(QList<WeatherData>);

    // sent by processData, caught by qml. Send only the final data (i.e., 3 stations); qml just draws.
    void readyToDraw(QHash<QString, QString>); // QHash size 3

};
