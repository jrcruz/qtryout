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


// Needed in order to store QPointF's in QHashes. We do seed + 1 in the second
//  coordinate's hash to handle addition being commutative.
uint qHash(QPointF p, uint seed)
{
    return qHash(qHash(p.x(), seed) + qHash(p.y(), seed + 1), seed);
}


WeatherApi::WeatherApi()
    : _net{}
    , _timer{}
    , _coor_to_station_name{}
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
    // All available features:
    // https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=describeStoredQueries
    // "Documentation" for chosen paramenters:
    // https://opendata.fmi.fi/meta?observableProperty=observation&param=temperature,ws,pressure&language=eng
    // All available parameters:
    // https://opendata.fmi.fi/meta?observableProperty=observation&language=eng

    QUrl endpoint("https://opendata.fmi.fi/wfs");
    QUrlQuery query;
    query.addQueryItem("service", "WFS");
    query.addQueryItem("version", "2.0.0");
    query.addQueryItem("request", "getFeature");
    query.addQueryItem("parameters", "temperature,ws,pressure");
    query.addQueryItem("storedquery_id", "fmi::observations::weather::multipointcoverage");
    query.addQueryItem("place", "oulu");
    // Oulu has 6 weather stations in its vicinity.
    query.addQueryItem("maxlocations", QString{"%1"}.arg(OULU_MAX_STATIONS));

    // Set the start time to 15 minutes ago, in order to get at least one
    //  reading from all stations, since stations do not perform measurements
    //  at the same time.
    // Use UTC+3 (Helsinki time) to be able to compare with real-time data.
    QDateTime start_time{QDateTime::currentDateTimeUtc()
            .addSecs(3 * 60 * 60)
            .addSecs(- 15 * 60)};
    start_time.setOffsetFromUtc(3 * 60 * 60);

    QString start_time_string{start_time.toString(Qt::ISODate)};
    start_time_string.replace('+', "%2b"); // Need to URL encode + ourselves.

    query.addQueryItem("starttime", start_time_string);
    endpoint.setQuery(query);

    _net.get(QNetworkRequest{endpoint});
}


void WeatherApi::getData(QNetworkReply* r)
{
    // Network error? Return here and wait for next timer timeout.
    if (r->error() != 0) {
        return;
    }
    QString locations;
    QString raw_data;

    QXmlStreamReader xml{r};
    while (!xml.atEnd()) {
        xml.readNext();

        // Station list incoming.
        if (xml.name() == "Point") {

            // Since the station coordinates don't change, only create station
            //  map the first time this function is called (fills to
            //  OULU_MAX_STATIONS the iteration before).
            if (_coor_to_station_name.size() == OULU_MAX_STATIONS) {
                xml.skipCurrentElement();
            }
            else {
                xml.readNextStartElement(); // Go down to gml:name.
                QString station_name = xml.readElementText(); // Read name.
                xml.readNextStartElement(); // Go sideways to gml:pos.
                QString station_location = xml.readElementText(); // Read two floats separated by a space

                auto pos_pair{station_location.split(" ")};
                _coor_to_station_name.insert({pos_pair[0].toFloat(), pos_pair[1].toFloat()},
                                              station_name);
                xml.skipCurrentElement(); // Get out.
            }
        }

        // Single element with multiple lines, one for each station
        //  measurement. Each line has the station's x coord, y coord, and the
        //  time the station output a measurement, as unix time, all separated
        //  by spaces.
        // The actual measurements appear in the same order as the stations in
        //  this element, i.e., measurement n was done by station n, as ordered
        //  in this element.
        // Stations can appear more than once; this means that they have more
        //  than one measurement for the selected time period.
        else if (xml.name() == "positions") {
            locations = xml.readElementText();
            xml.skipCurrentElement();
        }

        // Single line with (temperature, pressure, wind speed) measurement
        //  triples, for each station in the order as they appear above.
        else if (xml.name() == "doubleOrNilReasonTupleList") {
            raw_data = xml.readElementText();
            xml.skipCurrentElement();
        }
    }

    emit readyToParseData(locations, raw_data);
}


void WeatherApi::parseData(QString locations, QString raw_data)
{
    QList<WeatherData> measurements;
    QStringList pos_list{locations.simplified().split(" ")};
    QStringList data_list{raw_data.simplified().split(" ")};

    QHash<QPointF, int> station_time;
    bool replace = false;
    QPointF last_coordinate_seen;

    for (int j = 0; j < pos_list.size(); j += 3) {
        QPointF station_coor{pos_list[j].toFloat(),
                             pos_list[j + 1].toFloat()};
        int measured_at = pos_list[j].toInt();

        if (last_coordinate_seen == station_coor) {
            continue;
        }
        last_coordinate_seen = station_coor;

        // Since it's very likely that we have more than one measurement by station, we should only keep the most recent one.
        // we can just at the last item because the stations come grouped
        if (station_time.contains(station_coor)) {
            if (station_time[station_coor] > measured_at) {
                replace = true;
            }
            continue;
        }
        station_time[station_coor] = measured_at;

        // not all stations have windspeed and pressure measurements, so they return nan.
        // the conversion doesn't fail, but we should know that we're dealing with nans, both to find the correct maximum/minimums, and to display the values properly
        float temperature = data_list[j].toFloat();

        float windspeed = data_list[j + 2].toFloat();
        if (qIsNaN(windspeed)) {
            windspeed = std::numeric_limits<float>::min();
        }

        float pressure = data_list[j + 1].toFloat();
        if (qIsNaN(pressure)) {
            pressure = std::numeric_limits<float>::max();
        }

        WeatherData measurement{
            _coor_to_station_name[station_coor],
            station_coor,
            measured_at,
            temperature,
            windspeed,
            pressure
        };

        if (replace) {
            measurements.last() = measurement;
        }
        else {
            measurements.append(measurement);
        }

        replace = false;
    }

    emit readyToProcessData(measurements);
}


void WeatherApi::processData(QList<WeatherData> measurements)
{
    // Calculate goal list first.
    auto max_t = std::max_element(measurements.cbegin(), measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.temperature < w2.temperature;
    });
    auto max_w = std::max_element(measurements.cbegin(), measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.windspeed < w2.windspeed;
    });
    auto min_p = std::min_element(measurements.cbegin(), measurements.cend(),
        [](const WeatherData& w1, const WeatherData& w2) {
            return w1.pressure < w2.pressure;
    });

    QVariantMap goal_list{
        {"highest_temperature", QVariantList{max_t->station_name, max_t->station_coor}},
        {"strongest_wind", QVariantList{max_w->station_name, max_w->station_coor}},
        {"lowest_pressure", QVariantList{min_p->station_name, min_p->station_coor}}
    };


    // then Send updated list of complete measurements.
    QVariantList all_stations_data;
    for (const WeatherData& measurement : measurements) {
        QVariant w_maybe_null = qFuzzyCompare(measurement.windspeed,
                                              std::numeric_limits<float>::min() )
                              ? QVariant{} : QVariant{measurement.windspeed};

        QVariant p_maybe_null = qFuzzyCompare(measurement.pressure,
                                              std::numeric_limits<float>::max() )
                              ? QVariant{} : QVariant{measurement.pressure};

        all_stations_data.append(QVariantMap{
             {"station_name", measurement.station_name},
             {"station_coor", measurement.station_coor},
             {"temperature", measurement.temperature},
             {"windspeed", w_maybe_null},
             {"pressure", p_maybe_null}
        });
    }

    setFullStationList(all_stations_data);
    setGoalList(goal_list);

    /*
    for (auto& x : measurements) {
        std::cout << "(" << _coor_to_station_name[x.station_coor].toStdString() << ") " << x.station_coor.x() << ", " << x.station_coor.y() << ":\n";
        std::cout << "t = " << x.temperature << ", p = " << x.pressure << ", w = " << x.windspeed << "\n";
    }

    std::cout << "highest temperature: " << _coor_to_station_name[max_temperature_coor].toStdString() << "\n"
              << "strongest wind: "      << _coor_to_station_name[max_wind_coor].toStdString() << "\n"
              << "lowest pressure: "     << _coor_to_station_name[min_pressure_coor].toStdString() << "\n";
    */
}
