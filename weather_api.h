#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QPointF>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariant>
#include <QtQml/qqml.h>


struct WeatherData {
    QString station_name;
    QPointF station_coor;
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
    QHash<QPointF, QString> _coor_to_station_name;

public:
    WeatherApi();

    Q_PROPERTY(QVariantMap goal_list MEMBER _goal_list
                                     WRITE setGoalList
                                     NOTIFY sendGoalList)
    QVariantMap _goal_list;

    Q_PROPERTY(QVariantList full_station_list MEMBER _full_station_list
                                              WRITE setFullStationList
                                              NOTIFY sendFullStationList)
    QVariantList _full_station_list;

    void setFullStationList(QVariantList new_list);
    void setGoalList(QVariantMap new_list);

public slots:
    // Sends a request to Ilmatieteenlaitos's API for 15 minutes worth of
    //  temperature, pressure and windspeed measurements from the 6 closest
    //  stations to Oulu.
    // Activated once in the constructor and then every 30 seconds by _timer.
    void makeRequest();

    // Parses XML from the API response and extracts raw data (station
    //  coordinates, measurement order, and measurements proper) as strings.
    void getData(QNetworkReply* r);

    // Converts the raw data into a usable format, keeping only the most
    //  recent measurements.
    void parseData(QString locations, QString data);

    // Finds goal stations and sends both them and the full station data to QML.
    void processData(QList<WeatherData>);

signals:
    // Emitted by getData(), caught by parseData().
    void readyToParseData(QString locations, QString raw_data);

    // Emitted by parseData(), caught by processData().
    void readyToProcessData(QList<WeatherData>);

    // Emitted by processData(), caught in QML.
    void sendGoalList(QVariantMap goal_list);

    // Emitted by processData(), caught in QML.
    void sendFullStationList(QVariantList full_station_list);
};
