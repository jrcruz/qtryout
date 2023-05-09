#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QPointF>
#include <optional>
#include <QNetworkAccessManager>
#include <QVariantList>
#include <QtQml/qqml.h>


constexpr int OULU_MAX_STATIONS = 6;

struct WeatherData {
    QString station_name;
    QPointF station_coor;
    int measurement_time;
    float temperature;
    float windspeed;
    float pressure;
};


class WeatherApi : public QObject
{
    Q_OBJECT
    QML_SINGLETON

    QNetworkAccessManager net;
    QTimer timer;
    QHash<QPointF, QString> stations;

public:
    WeatherApi();

    Q_PROPERTY(QList<QString> values MEMBER values WRITE setValues NOTIFY valuesChanged)
    QList<QString> values;
    void setValues(QList<QString> m) { values = m; emit valuesChanged(values);}


    Q_PROPERTY(QVariantMap l MEMBER l WRITE setL NOTIFY lChanged)
    QVariantMap l;
    void setL(QVariantMap m) { l = m; emit lChanged(l);}

    Q_PROPERTY(QVariantList hog MEMBER hog WRITE setHog NOTIFY sendFullHog)
    QVariantList hog;
    void setHog(QVariantList m) { hog = m; emit sendFullHog(hog);}

public slots:
    // receives. emits
    void makeRequest();
    // receives. emits processData
    void getData(QNetworkReply* r);
    // receives.
    void parseData(QString locations, QString data);
    // receives.
    void processData(QList<WeatherData>); //emits readyToDraw

signals:
    // sent by getData, caught by processData
    void readyToProcessData(QList<WeatherData>);

    // sent by processData, caught by qml. Send only the final data (i.e., 3 stations); qml just draws.
    //void readyToDraw(QHash<QString, QString>); // QHash size 3

    void valuesChanged(QList<QString> new_values);
    void lChanged(QVariantMap new_l);

    void sendFullHog(QVariantList bro);
};
