#include "weather_api.h"

#include <QNetworkRequest>
#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QIODevice>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QHash>
#include <QList>
#include <QTimer>
#include <QDateTime>
#include <QTimeZone>

#include <limits>
#include <algorithm>


constexpr int OULU_MAX_STATIONS = 6;


// Needed in order to store QPointF's in QHashes. We do seed + 1 in
//  the second coordinate's hash to handle addition being commutative
//  (so hash Point(1,2) != hash Point(2,1)).
namespace std
{
template <>
struct hash<QPointF>
{
    size_t operator()(const QPointF &p, size_t seed = 0) const
    {
        return qHash(qHash(p.x(), seed) + qHash(p.y(), seed + 1), seed);
    }
};
} // namespace std.



WeatherApi::WeatherApi()
    : _net{}
    , _timer{}
    , _coor_to_station_name{}
    , _goal_list{}
    , _full_station_list{}
{
    // Connect makeRequest -> getData -> parseData -> processData.
    connect(&_net, &QNetworkAccessManager::finished, this, &WeatherApi::getData);
    connect(this, &WeatherApi::readyToParseData, this, &WeatherApi::parseData);
    connect(this, &WeatherApi::readyToProcessData, this, &WeatherApi::processData);

    // Initial request.
    makeRequest();

    connect(&_timer, &QTimer::timeout, this, &WeatherApi::makeRequest);
    // Repeat request every 15 seconds.
    _timer.start(15 * 1000);
}


void WeatherApi::makeRequest()
{
    // All available features:
    //  https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=describeStoredQueries
    // "Documentation" for chosen paramenters:
    //  https://opendata.fmi.fi/meta?observableProperty=observation&param=temperature,ws,pressure&language=eng
    // All available parameters:
    //  https://opendata.fmi.fi/meta?observableProperty=observation&language=eng
    // Complete URL:
    //  https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&parameters=temperature,ws,pressure&storedquery_id=fmi::observations::weather::multipointcoverage&place=oulu&starttime=2023-05-08T02:15:00Z&endtime=2023-05-08T02:30:00Z&maxlocations=6

    QUrl endpoint(QStringLiteral("https://opendata.fmi.fi/wfs"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("service"), QStringLiteral("WFS"));
    query.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0.0"));
    query.addQueryItem(QStringLiteral("request"), QStringLiteral("getFeature"));
    query.addQueryItem(QStringLiteral("parameters"), QStringLiteral("temperature,ws,pressure"));
    query.addQueryItem(QStringLiteral("storedquery_id"), QStringLiteral("fmi::observations::weather::multipointcoverage"));
    query.addQueryItem(QStringLiteral("place"), QStringLiteral("oulu"));
    // Oulu has 6 weather stations in its vicinity.
    query.addQueryItem(QStringLiteral("maxlocations"), QString{"%1"}.arg(OULU_MAX_STATIONS));

    // Set the start time to 15 minutes ago, in order to get at least one
    //  reading from all stations, since stations do not perform measurements
    //  at the same time.
    // Use UTC+3 (Helsinki time) to be able to compare with real-time data.
    QDateTime start_time{QDateTime::currentDateTimeUtc()
            .addSecs(3 * 60 * 60)
            .addSecs(- 15 * 60)};
    start_time.setOffsetFromUtc(3 * 60 * 60);

    QString start_time_string{start_time.toString(Qt::ISODate)};
    start_time_string.replace('+', QStringLiteral("%2b")); // Need to URL encode '+' ourselves.

    query.addQueryItem(QStringLiteral("starttime"), start_time_string);
    endpoint.setQuery(query);

    _net.get(QNetworkRequest{endpoint});
}


void WeatherApi::getData(QNetworkReply* r)
{
    // Network error? Return here and wait for next timer timeout.
    if (r->error() != 0) {
        qDebug() << "Error while making API request... Retrying in 15 seconds.";
        return;
    }
    QString locations;
    QString raw_data;

    QXmlStreamReader xml{r};
    while (not xml.atEnd()) {
        xml.readNext();

        // Station list incoming.
        if (xml.name() == QStringLiteral("Point")) {

            // Since the station coordinates don't change, only create the
            //  coordinate to name map the first time this function is called
            // (i.e., fills to OULU_MAX_STATIONS the iteration before).
            if (_coor_to_station_name.size() == OULU_MAX_STATIONS) {
                xml.skipCurrentElement();
            }
            else {
                xml.readNextStartElement(); // Go down to gml:name.
                QString station_name = xml.readElementText(); // Read name.
                xml.readNextStartElement(); // Go sideways to gml:pos.
                QString station_location = xml.readElementText(); // Read two floats separated by a space

                auto pos_pair{station_location.split(' ')};
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
        else if (xml.name() == QStringLiteral("positions")) {
            locations = xml.readElementText();
            xml.skipCurrentElement();
        }

        // Single line with (temperature, pressure, wind speed) measurement
        //  triples, for each station in the order as they appear above.
        else if (xml.name() == QStringLiteral("doubleOrNilReasonTupleList")) {
            raw_data = xml.readElementText();
            xml.skipCurrentElement();
        }
    }
    // Got bad data... Upstream problem? In any case, wait for next timeout.
    if (locations.isEmpty() or raw_data.isEmpty()) {
        qDebug() << "Either locations or raw_data were empty, so the parse "
                    "was unsuccessful. Retrying, but this means that either "
                    "you should increase the interval to fetch from (currently "
                    "15 minutes), or upstream's data changed format.";
        return;
    }

    emit readyToParseData(locations, raw_data);
}


void WeatherApi::parseData(QString locations, QString raw_data)
{
    QList<WeatherData> measurements;
    QStringList pos_list{locations.simplified().split(' ')};
    QStringList data_list{raw_data.simplified().split(' ')};

    bool push_new = false;
    QPointF last_coordinate_seen;

    // Position data and measurement data both come in triples.
    for (int j = 0; j < pos_list.size(); j += 3) {
        QPointF station_coor{pos_list[j].toFloat(),
                             pos_list[j + 1].toFloat()};

        // Since it's very likely that we have more than one measurement by
        //  station, we should only keep the most recent one.
        // We can just at the last item because the data comes grouped by
        //  station, and each station comes ordered by (ascending) unix timestamp.
        if (last_coordinate_seen != station_coor) {
            push_new = true;
        }
        last_coordinate_seen = station_coor;

        float temperature = data_list[j].toFloat();

        // Not all stations have windspeed and pressure measurements, in which
        //  case the string is "NAN" and the conversion returns NaN (this is
        //  not a failure). We turn them into float min and max to be the
        //  identity for the max_element and min_element operations
        //  further ahead.
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
            temperature,
            windspeed,
            pressure
        };

        if (push_new) {
            measurements.append(measurement);
        }
        else {
            measurements.last() = measurement;
        }

        push_new = false;
    }

    emit readyToProcessData(measurements);
}


void WeatherApi::processData(QList<WeatherData> measurements)
{
    // Build goal list first. Turning NaNs into float limits lets us use
    //  compare all numbers without any additional work.
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
        {QStringLiteral("highest_temperature"), QVariantList{max_t->station_name, max_t->station_coor}},
        {QStringLiteral("strongest_wind"), QVariantList{max_w->station_name, max_w->station_coor}},
        {QStringLiteral("lowest_pressure"), QVariantList{min_p->station_name, min_p->station_coor}}
    };

    // Build all stations list second. If windspeed and pressure were NaN, then
    //  they were turned into std::numerical_limits<float>::min() and ::max(),
    //  respectively, in order for the comparisons above to work.
    // Since we don't want to display ::min() and ::max() to the user, we'll
    //  build an empty QVariant to signal they were NaNs. QML can then
    //  distinguish these values.
    QVariantList all_stations_data;
    for (const WeatherData& measurement : measurements) {
        QVariant w_maybe_null = qFuzzyCompare(measurement.windspeed,
                                              std::numeric_limits<float>::min() )
                              ? QVariant{} : QVariant{measurement.windspeed};

        QVariant p_maybe_null = qFuzzyCompare(measurement.pressure,
                                              std::numeric_limits<float>::max() )
                              ? QVariant{} : QVariant{measurement.pressure};


        all_stations_data.append(QVariantMap{
            {QStringLiteral("station_name"), measurement.station_name},
            {QStringLiteral("station_coor"), measurement.station_coor},
            {QStringLiteral("temperature"), measurement.temperature},
            {QStringLiteral("windspeed"), w_maybe_null},
            {QStringLiteral("pressure"), p_maybe_null}
        });
    }

    setFullStationList(all_stations_data);
    setGoalList(goal_list);
}


void WeatherApi::setGoalList(QVariantMap new_list)
{
    _goal_list = new_list;
    emit sendGoalList(_goal_list);
}


void WeatherApi::setFullStationList(QVariantList new_list)
{
    _full_station_list = new_list;
    emit sendFullStationList(_full_station_list);
}
