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

constexpr int OULU_MAX_STATIONS = 6;

uint qHash(QPointF p, uint seed)
{
    return qHash(qHash(p.x(), seed) + qHash(p.y(), seed + 1), seed);
}


WeatherApi::WeatherApi()
    : _net{}
    , _timer{}
    , _stations{}
{
    connect(&_net, &QNetworkAccessManager::finished, this, &WeatherApi::getData);
    connect(this, &WeatherApi::readyToParseData, this, &WeatherApi::parseData);
    connect(this, &WeatherApi::readyToProcessData, this, &WeatherApi::processData);

    // Initial request.
    makeRequest();

    connect(&_timer, &QTimer::timeout, this, &WeatherApi::makeRequest);
    _timer.start(5000);
}


void WeatherApi::makeRequest()
{
    QUrl endpoint("http://opendata.fmi.fi/wfs");
    QUrlQuery query;
    query.addQueryItem("service", "WFS");
    query.addQueryItem("version", "2.0.0");
    query.addQueryItem("request", "getFeature");
    query.addQueryItem("parameters", "temperature,ws,pressure");
    query.addQueryItem("storedquery_id", "fmi::observations::weather::multipointcoverage");
    query.addQueryItem("place", "oulu");
    query.addQueryItem("maxlocations", QString("%1").arg(OULU_MAX_STATIONS));

    //XXX explain time
    QDateTime end_time{QDateTime::currentDateTimeUtc()};
    end_time.setOffsetFromUtc(3 * 60 * 60);
    QDateTime start_time{end_time.addSecs(- 15 * 60)}; // start time is 15 mins before now
    start_time.setOffsetFromUtc(3 * 60 * 60);

    QString end_time_string{end_time.toString(Qt::ISODate)};
    end_time_string.replace('+', "%2b");
    QString start_time_string{start_time.toString(Qt::ISODate)};
    start_time_string.replace('+', "%2b");

    query.addQueryItem("starttime", start_time_string);
    query.addQueryItem("endtime", end_time_string);
    endpoint.setQuery(query);

    _net.get(QNetworkRequest(endpoint));
}


void WeatherApi::getData(QNetworkReply* r)
{
    std::cout << "reply error? " << (r->error()) << "\n";

    QXmlStreamReader xml(r);
    QString locations;
    QString raw_data;

    while (!xml.atEnd()) {
        xml.readNext();
        // Station list incoming.
        if (xml.name() == "Point") {
            // Only create station map if it doesn't exist.
            if (_stations.size() == OULU_MAX_STATIONS) {
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
                _stations[p] = station_name;
                xml.skipCurrentElement(); // Get out.
            }
        }
        // Single line with station x, y and unix time for each measurement. the order is the same as the data
        else if (xml.name() == "SimpleMultiPoint") {
            xml.readNextStartElement();
            locations = xml.readElementText();
            xml.skipCurrentElement();
        }
        // single line with temperature, pressure and wind speed for each station in the order as they appear in SimpleMultiPoint.
        else if (xml.name() == "doubleOrNilReasonTupleList") {
            raw_data = xml.readElementText();
            xml.skipCurrentElement();
        }
    }
    std::cout << "end" << std::endl;
    //std::cout << positions.simplified().size() << "\n" << positions.simplified().toStdString() << "\n";
    //std::cout << data.simplified().size() << "\n" << data.simplified().toStdString() << "\n";

    emit readyToParseData(locations, raw_data);
    return;
/*
    //std::cout << std::endl;
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
        int measured_at = pos_list[position_step].toInt();

        if (station_time.contains(station_coor)) {
            if (station_time[station_coor] > measured_at) {
                replace = true;
            }
            continue;
        }
        station_time[station_coor] = measured_at;

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
            _stations[station_coor],
            station_coor,
            measured_at,
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
            */
}


void WeatherApi::parseData(QString locations, QString raw_data)
{
    QList<WeatherData> measurements;
    QStringList pos_list{locations.simplified().split(" ")};
    QStringList data_list{raw_data.simplified().split(" ")};

    QHash<QPointF, int> station_time;
    bool replace = false;
    std::cout << pos_list.size() << ", " << data_list.size() << "\n";
    //return;
    for (int data_step = 0, position_step = 0;
        data_step < pos_list.size();
        data_step += 3, position_step += 3)
    {
        QPointF station_coor{pos_list[position_step].toFloat(), pos_list[position_step + 1].toFloat()};
        int measured_at = pos_list[position_step].toInt();

        if (station_time.contains(station_coor)) {
            if (station_time[station_coor] > measured_at) {
                replace = true;
            }
            continue;
        }
        station_time[station_coor] = measured_at;

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
            _stations[station_coor],
            station_coor,
            measured_at,
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
    // Send updated list of measurements first.
    QVariantList all_stations_data;
    for (const WeatherData& measurement : measurements) {
        all_stations_data.append(QVariantMap{
             {"station_name", measurement.station_name},
             {"station_coor", measurement.station_coor},
             {"temperature", measurement.temperature},
             {"pressure", measurement.pressure},
             {"windspeed", measurement.windspeed}
        });
    }
    setFullStationList(all_stations_data);

    for (auto& x : measurements) {
        std::cout << "(" << _stations[x.station_coor].toStdString() << ") " << x.station_coor.x() << ", " << x.station_coor.y() << ":\n";
        std::cout << "t = " << x.temperature << ", p = " << x.pressure << ", w = " << x.windspeed << "\n";
    }

    // Then send goal list.
    QPointF max_temperature_coor = std::max_element(measurements.cbegin(),
                                                    measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.temperature < w2.temperature;
    })->station_coor;

    QPointF max_wind_coor = std::max_element(measurements.cbegin(),
                                             measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.windspeed < w2.windspeed;
    })->station_coor;

    QPointF min_pressure_coor = std::min_element(measurements.cbegin(),
                                                 measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.pressure < w2.pressure;
    })->station_coor;

    std::cout << "highest temperature: " << _stations[max_temperature_coor].toStdString() << "\n"
              << "strongest wind: "      << _stations[max_wind_coor].toStdString() << "\n"
              << "lowest pressure: "     << _stations[min_pressure_coor].toStdString() << "\n";

    QVariantMap goal_list{
        {"highest_temperature", QVariantList{_stations[max_temperature_coor], max_temperature_coor}},
        {"strongest_wind", QVariantList{_stations[max_wind_coor], max_wind_coor}},
        {"lowest_pressure", QVariantList{_stations[min_pressure_coor], min_pressure_coor}}
    };
    setGoalList(goal_list);
}
