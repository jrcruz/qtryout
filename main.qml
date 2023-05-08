import QtQuick 2.15
import QtQuick.Window 2.15
import jcruz.eu 1.0

Window {
    width: 640
    height: 480
    visible: true
    title: "Hello World"

    Rectangle {
        width : 200
        height : 200

        Text {
            anchors.centerIn: parent
            text: WeatherAPI.qs
        }
        //onQsChanged: console.log("hi")
        // WeatherAPI.onQsChanged: { console.log("done") }
    }
}
