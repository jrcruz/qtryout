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

    QNetworkAccessManager _net;
    QTimer _timer;
    QHash<QPointF, QString> _stations;

public:
    WeatherApi();

    Q_PROPERTY(QVariantMap goal_list MEMBER _goal_list WRITE setGoalList NOTIFY sendGoalList)
    QVariantMap _goal_list;
    void setGoalList(QVariantMap new_list) { _goal_list = new_list; emit sendGoalList(_goal_list);}

    Q_PROPERTY(QVariantList full_station_list MEMBER _full_station_list WRITE setFullStationList NOTIFY sendFullStationList)
    QVariantList _full_station_list;
    void setFullStationList(QVariantList new_list) { _full_station_list = new_list; emit sendFullStationList(_full_station_list);}

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
    // Signals that
    // Emitted by getData(), caught by parseData().
    void readyToParseData(QString locations, QString raw_data);

    // Emitted by parseData(), caught by processData().
    void readyToProcessData(QList<WeatherData>);

    // Emitted by processData()
    void sendGoalList(QVariantMap new_l);
    // sent by processData, caught by qml. Send only the final data (i.e., 3 stations); qml just draws.
    void sendFullStationList(QVariantList bro);
};
