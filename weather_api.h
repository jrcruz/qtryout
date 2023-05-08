#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QPointF>
#include <optional>
#include <QNetworkAccessManager>
#include <QtQml/qqml.h>




struct WeatherData {
    QPointF coor;
    float temperature;
    std::optional<float> windspeed;
    std::optional<float> pressure;
};







class WeatherApi : public QObject
{
    Q_OBJECT
    QML_SINGLETON

    QNetworkAccessManager net;
    QTimer* timer;
    QHash<QPointF, QString> stations;

public:
    WeatherApi();

    Q_PROPERTY(QString qs READ readQs NOTIFY qsChanged)
    QString qs;
    QString readQs() const { return qs; }

public slots:
    void getData(QNetworkReply* r); // emits processData
    void processData(QList<WeatherData>); //emits readyToDraw

    void changeTestString();


signals:
    // sent by getData, caught by processData
    void readyToProcessData(QList<WeatherData>);

    // sent by processData, caught by qml. Send only the final data (i.e., 3 stations); qml just draws.
    void readyToDraw(QHash<QString, QString>); // QHash size 3

    void qsChanged(QString new_qs);
};
