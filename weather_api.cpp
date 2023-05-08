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
#include <iostream>

QList<WeatherData> parseXmlResponse(QNetworkReply* r)
{
    std::cout << "reply error? " << (r->error()) << "\n";

    //QString s(r->read(5));
    //std::cout << "-" << s.toStdString() << "-\n";
    //return {};
    QXmlStreamReader xml(r);

    QHash<QString, QString> ok;
    QString positions;
    QString data;

    std::cout << "before parse\n";
    while (!xml.atEnd()) {
        auto k = xml.readNext();
        std::cout << "readNext() returned " << k << "\n";
        std::cout << "Read " << xml.name().toString().toStdString() << "\n";
        if (xml.name() == "Point") {
            if (!ok.empty()) {
                xml.skipCurrentElement();
            }
            else {
                xml.readNextStartElement();

                QString station_name = xml.readElementText();
                xml.readNextStartElement();
                QString station_location = xml.readElementText();
                ok.insert(station_name, station_location);
                xml.skipCurrentElement();
                // point, recurse down
                std::cout << "saw station: " << station_name.toStdString() << " at " << station_location.toStdString() << "\n";
            }


        }

        if (xml.name() == "SimpleMultiPoint") {
            // get coords in order
            xml.readNextStartElement();
            positions = xml.readElementText();
            xml.skipCurrentElement();
        }

        else if (xml.name() == "doubleOrNilReasonTupleList") {
            // get values in order
            xml.readNextStartElement();
            data = xml.readElementText();
            xml.skipCurrentElement();
        }
        //xml.skipCurrentElement();
    }

    std::cout << "after parse\n";
    emit
    return {};
}


WeatherApi::WeatherApi()
    : net{}
{
    std::cout << "Entering cons\n";

    QUrl endpoint("http://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&parameters=temperature,ws,pressure&storedquery_id=fmi::observations::weather::multipointcoverage&place=oulu&starttime=2023-05-08T01:58:00Z&endtime=2023-05-08T02:00:00Z&maxlocations=10");
    //QUrl endpoint("http://example.org/");
//    QUrlQuery query;
  //  query.addQueryItem("hi", "bye");
    //endpoint.setQuery(query);

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


void WeatherApi::getData(QNetworkReply* r)
{
    std::cout << "arrived!\n";
    parseXmlResponse(r);
    std::cout << "printed!\n";


    // emit signal when ready to process it


    // in the processor, after doing it, emit a signal for qml

}





