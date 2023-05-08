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
#include <iostream>
#include <algorithm>

uint qHash(QPointF p, uint seed)
{
    return qHash(qHash(p.x(), seed) + qHash(p.y(), seed), seed);
}


void WeatherApi::getData(QNetworkReply* r)
{
    std::cout << "reply error? " << (r->error()) << "\n";

    QXmlStreamReader xml(r);
    QString positions;
    QString data;

    while (!xml.atEnd()) {
        auto k = xml.readNext();
        //std::cout << "readNext() returned " << k << "\n";
        //std::cout << "Read " << xml.name().toString().toStdString() << "\n";
        // Station list incoming.
        if (xml.name() == "Point") {
            // Only create station map if it doesn't exist.
            //if (!stations.empty()) {
            //    xml.skipCurrentElement();
            //}
            //else {
                xml.readNextStartElement(); // Go down to X.
                QString station_name = xml.readElementText(); // Name.
                xml.readNextStartElement(); // Go sideways to Y.
                QString station_location = xml.readElementText(); // Two floats separated by space.
                std::cout << "Found station " << station_name.toStdString() << " at " << station_location.toStdString() << "\n";

                auto pos_pair = station_location.split(" ");
                QPointF p(pos_pair[0].toFloat(), pos_pair[1].toFloat());
                std::cout << pos_pair[0].toStdString() << " ; " << pos_pair[1].toStdString() << "\n";
                stations[p] = station_name;
                xml.skipCurrentElement(); // Get out.
            //}
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
    QStringList pos_list = positions.simplified().split(" ");
    QStringList data_list = data.simplified().split(" ");

    std::cout << pos_list.size() << ", " << data_list.size() << "\n";
    //return;
    for (int data_step = 0, position_step = 0;
        data_step < pos_list.size();
        data_step += 3, position_step += 3)
    {
        bool conversion_ok;

        QPointF station_coor{pos_list[position_step].toFloat(), pos_list[position_step + 1].toFloat()};

        float temperature = data_list[data_step].toFloat();

        std::optional<float> pressure;
        float pressure_nan = data_list[data_step + 1].toFloat(&conversion_ok);
        if (conversion_ok) {
            pressure = pressure_nan;
        }

        std::optional<float> windspeed;
        float windspeed_nan = data_list[data_step + 2].toFloat(&conversion_ok);
        if (conversion_ok) {
            windspeed = windspeed_nan;
        }

        measurements.append({
            station_coor,
            temperature,
            pressure,
            windspeed
        });
    }
    std::cout << "endend\n";
    emit readyToProcessData(measurements);
}

void WeatherApi::processData(QList<WeatherData> f)
{
    std::cout << "in process data. printing full weather data\n";
    for (auto& x : f) {
        std::cout << x.coor.x() << ", " << x.coor.y() << ":\n";
        std::cout << "t = " << x.temperature << ", p = " << x.pressure.value_or(-1) << ", w = " << x.windspeed.value_or(-1) << "\n";
    }

    QPointF max_temperature_coor = std::max_element(f.begin(), f.end(), [](const WeatherData& w1, const WeatherData& w2) {
       return w1.temperature < w2.temperature;
    })->coor;

    QPointF max_wind_coor = std::max_element(f.begin(), f.end(), [](WeatherData& w1, WeatherData& w2) {
       return w1.windspeed < w2.windspeed;
    })->coor;

    QPointF min_pressure_coor = std::min_element(f.begin(), f.end(), [](WeatherData& w1, WeatherData& w2) {
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

    emit readyToDraw(info_to_draw);
}

WeatherApi::WeatherApi()
    : net{}
    , timer{new QTimer(this)}
    , stations{}
{
    std::cout << "Entering cons\n";

    QUrl endpoint("http://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&parameters=temperature,ws,pressure&storedquery_id=fmi::observations::weather::multipointcoverage&place=oulu&starttime=2023-05-08T01:58:00Z&endtime=2023-05-08T02:00:00Z&maxlocations=10");
    //QUrl endpoint("http://example.org/");
//    QUrlQuery query;
  //  query.addQueryItem("hi", "bye");
    //endpoint.setQuery(query);

    connect(this, &WeatherApi::readyToProcessData, this, &WeatherApi::processData);


    connect(&net, &QNetworkAccessManager::finished, this, &WeatherApi::getData);

    net.get(QNetworkRequest(endpoint));
    //std::cout << endpoint.toString().toStdString() << "\n";
    //std::cout << k->isFinished() << "\n";

    //QObject::thread()->sleep(5);
    //parseXmlResponse(k);
    std::cout << "left\n";


    // get data
    // register timer
}






