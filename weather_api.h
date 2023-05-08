#pragma once

#include <QObject>
#include <QList>
#include <QNetworkAccessManager>



struct WeatherData {
    QPair<double, double> coor;
    double temp;
    double windspeed;
    double pressure;
};


class WeatherApi : public QObject
{
    Q_OBJECT


public:
    WeatherApi();
    QNetworkAccessManager net;

public slots:
    void getData(QNetworkReply* r);

signals:
    void newDataAvailable(QList<WeatherData>);

};
