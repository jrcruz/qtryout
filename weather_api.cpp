#include "weather_api.h"
#include <QObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QIODevice>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QDebug>
#include <QHash>
#include <QList>
#include <QTimer>
#include <QDateTime>
#include <QTimeZone>
#include <iostream>
#include <limits>
#include <algorithm>



uint qHash(QPointF p, uint seed)
{
    return qHash(qHash(p.x(), seed) + qHash(p.y(), seed + 1), seed);
}


void WeatherApi::getData(QNetworkReply* r)
{
    std::cout << "reply error? " << (r->error()) << "\n";

    QXmlStreamReader xml(r);
    QString positions;
    QString data;

    while (!xml.atEnd()) {
        xml.readNext();
        // Station list incoming.
        if (xml.name() == "Point") {
            // Only create station map if it doesn't exist.
            if (stations.size() == OULU_MAX_STATIONS) {
                xml.skipCurrentElement();
            }
            else {
                xml.readNextStartElement(); // Go down to X.
                QString station_name = xml.readElementText(); // Name.
                xml.readNextStartElement(); // Go sideways to Y.
                QString station_location = xml.readElementText(); // Two floats separated by space.
                std::cout << "Found station " << station_name.toStdString() << " at " << station_location.toStdString() << "\n";

                auto pos_pair = station_location.split(" ");
                QPointF p(pos_pair[0].toFloat(), pos_pair[1].toFloat());
                stations[p] = station_name;
                xml.skipCurrentElement(); // Get out.
            }
        }
        // Single line with station x, y and unix time for each measurement. the order is the same as the data
        else if (xml.name() == "SimpleMultiPoint") {
            // get coords in order
            xml.readNextStartElement();
            positions = xml.readElementText();
            xml.skipCurrentElement();
        }
        // single line with temperature, pressure and wind speed for each station in the order as they appear in SimpleMultiPoint.
        else if (xml.name() == "doubleOrNilReasonTupleList") {
            data = xml.readElementText();
            xml.skipCurrentElement();
        }
    }
    std::cout << "end" << std::endl;

    std::cout << positions.simplified().size() << "\n"
              << positions.simplified().toStdString() << "\n";
    std::cout << data.simplified().size() << "\n"
              << data.simplified().toStdString() << "\n";

    std::cout << std::endl;
    QList<WeatherData> measurements;
    QStringList pos_list{positions.simplified().split(" ")};
    QStringList data_list{data.simplified().split(" ")};

    QHash<QPointF, int> station_time;
    bool replace = false;
    std::cout << pos_list.size() << ", " << data_list.size() << "\n";
    //return;
    for (int data_step = 0, position_step = 0;
        data_step < pos_list.size();
        data_step += 3, position_step += 3)
    {
        QPointF station_coor{pos_list[position_step].toFloat(), pos_list[position_step + 1].toFloat()};

        if (station_time.contains(station_coor)) {
            if (station_time[station_coor] > pos_list[position_step].toInt()) {
                replace = true;
            }
            continue;
        }
        station_time[station_coor] = pos_list[position_step].toInt();

        float temperature = data_list[data_step].toFloat();
        float pressure = data_list[data_step + 1].toFloat();
        if (qIsNaN(pressure)) {
            pressure = std::numeric_limits<float>::max();
        }
        float windspeed = data_list[data_step + 2].toFloat();
        if (qIsNaN(windspeed)) {
            windspeed = std::numeric_limits<float>::min();
        }
        WeatherData d{
            station_coor,
            temperature,
            pressure,
            windspeed
        };
        if (replace) {
            measurements.last() = d;
        }
        else {
            measurements.append(d);
        }
        replace = false;

    }
    std::cout << "endend\n";

    emit readyToProcessData(measurements);
}

void WeatherApi::processData(QList<WeatherData> measurements)
{
    std::cout << "in process data. printing full weather data\n";
    for (auto& x : measurements) {
        std::cout << "(" << stations[x.coor].toStdString() << ") " << x.coor.x() << ", " << x.coor.y() << ":\n";
        std::cout << "t = " << x.temperature << ", p = " << x.pressure << ", w = " << x.windspeed << "\n";
    }

    QPointF max_temperature_coor = std::max_element(measurements.cbegin(),
                                                    measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.temperature < w2.temperature;
    })->coor;

    QPointF max_wind_coor = std::max_element(measurements.cbegin(),
                                             measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.windspeed < w2.windspeed;
    })->coor;

    QPointF min_pressure_coor = std::min_element(measurements.cbegin(),
                                                 measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.pressure < w2.pressure;
    })->coor;

    QHash<QString, QString> info_to_draw{
        {"highest temperature", stations[max_temperature_coor]},
        {"strongest wind", stations[max_wind_coor]},
        {"lowest pressure", stations[min_pressure_coor]}
    };

    for (auto& x : info_to_draw.keys()) {
        std::cout << x.toStdString() << ": " << info_to_draw[x].toStdString() << "\n";
    }

    setValues({"hi", "bye", "what"});

    QVariantList f;
    f.append(QVariantList{stations[max_temperature_coor], max_temperature_coor});
    f.append(QVariantList{stations[max_wind_coor], max_wind_coor});
    f.append(QVariantList{stations[min_pressure_coor], min_pressure_coor});
    setL(f);
}

void WeatherApi::makeRequest()
{
    //QUrl endpoint("https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&parameters=temperature,ws,pressure&storedquery_id=fmi::observations::weather::multipointcoverage&place=oulu&starttime=2023-05-08T01:58:00Z&endtime=2023-05-08T02:00:00Z&maxlocations=10");
//https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&parameters=temperature,ws,pressure&storedquery_id=fmi::observations::weather::multipointcoverage&place=oulu&starttime=2023-05-08T01:58:00Z&endtime=2023-05-08T02:00:00Z&maxlocations=10

    QUrl endpoint("http://opendata.fmi.fi/wfs");
    QUrlQuery query;
    query.addQueryItem("service", "WFS");
    query.addQueryItem("version", "2.0.0");
    query.addQueryItem("request", "getFeature");
    query.addQueryItem("parameters", "temperature,ws,pressure");
    query.addQueryItem("storedquery_id", "fmi::observations::weather::multipointcoverage");
    query.addQueryItem("place", "oulu");
    query.addQueryItem("maxlocations", "10");
    query.addQueryItem("starttime", "2023-05-08T01:58:00%2b03:00");// must escape + to %2b
    query.addQueryItem("endtime", "2023-05-08T02:00:00%2b03:00");
    endpoint.setQuery(query);

    QDateTime dt{QDateTime::currentDateTimeUtc()};
    dt = dt.addSecs(3 * 60 * 60);
    dt.setOffsetFromUtc(3 * 60 * 60);

    std::cout << "date: " << dt.toString(Qt::ISODate).toStdString() << "\n";

    net.get(QNetworkRequest(endpoint));
    // signal "finished" is processed by processData
}

WeatherApi::WeatherApi()
    : net{}
    , timer{}
    , stations{}
    , values{}
{
    connect(this, &WeatherApi::readyToProcessData, this, &WeatherApi::processData);
    connect(&net, &QNetworkAccessManager::finished, this, &WeatherApi::getData);

    // Initial request.
    makeRequest();

    connect(&timer, &QTimer::timeout, this, &WeatherApi::makeRequest);
    timer.start(3000);
}
