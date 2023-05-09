import QtQuick 2.15
import QtQuick.Window 2.15
import jcruz.eu 1.0
import QtLocation 5.15
import QtPositioning 5.15

Window {
    width: Qt.platform.os == "android" ? Screen.width : 1080
    height: Qt.platform.os == "android" ? Screen.height : 720
    visible: true
    title: map.center + " zoom " + map.zoomLevel.toFixed(3)
           + " min " + map.minimumZoomLevel + " max " + map.maximumZoomLevel

    function coordinateJitter() {
        return Math.random() / 1000
    }

    Connections {
        target: WeatherAPI
        function onValuesChanged(val) {
            console.log("tsukihi pls")
            //max_t.text = val[0]
            //max_w.text = val[1]
            //min_p.text = val[2]
        }
        function onLChanged(v) {
            max_t.text = "Highest temperature:" + v[0]
            station_for_temp.coordinate = QtPositioning.coordinate(v[1].x + coordinateJitter(), v[1].y + coordinateJitter())
            max_w.text = "Highest wind speed:" + v[2]
            station_for_wind.coordinate = QtPositioning.coordinate(v[3].x + coordinateJitter(), v[3].y + coordinateJitter())
            min_p.text = "Lowest pressure: " + v[4]
            station_for_pres.coordinate = QtPositioning.coordinate(v[5].x + coordinateJitter(), v[5].y + coordinateJitter())
        }
    }

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        //center: QtPositioning.coordinate(59.91, 10.75) // Oslo
        center: QtPositioning.coordinate(65.012755, 25.478793) // Oulu
        zoomLevel: 10
        property var startCentroid

        PinchHandler {
            id: pinch
            target: null
            onActiveChanged: if (active) {
                map.startCentroid = map.toCoordinate(pinch.centroid.position, false)
            }
            onScaleChanged: (delta) => {
                map.zoomLevel += Math.log2(delta)
                map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
            }
            onRotationChanged: (delta) => {
                map.bearing -= delta
                map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
            }
            grabPermissions: PointerHandler.TakeOverForbidden
        }
        WheelHandler {
            id: wheel
            rotationScale: 1/120
            property: "zoomLevel"
        }
        DragHandler {
            id: drag
            target: null
            onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
        }
        Shortcut {
            enabled: map.zoomLevel < map.maximumZoomLevel
            sequence: StandardKey.ZoomIn
            onActivated: map.zoomLevel = Math.round(map.zoomLevel + 1)
        }
        Shortcut {
            enabled: map.zoomLevel > map.minimumZoomLevel
            sequence: StandardKey.ZoomOut
            onActivated: map.zoomLevel = Math.round(map.zoomLevel - 1)
        }

        MapQuickItem {
            id: station_for_temp
            anchorPoint.x: img.width/4
            anchorPoint.y: img.height
            coordinate: QtPositioning.coordinate(59.91, 10.75)
            sourceItem: Image { id: img1; height: 32; width: 32; source: "marker.png" }
        }

        MapQuickItem {
            id: station_for_wind
            anchorPoint.x: img2.width/4
            anchorPoint.y: img2.height
            coordinate: QtPositioning.coordinate(61.91, 10.75)
            sourceItem: Image { id: img2; height: 32; width: 32; source: "marker.png" }
        }

        MapQuickItem {
            id: station_for_pres
            anchorPoint.x: img3.width/4
            anchorPoint.y: img3.height
            coordinate: QtPositioning.coordinate(68.91, 10.75)
            sourceItem: Image { id: img3; height: 32; width: 32; source: "marker.png" }
        }
    }

    Rectangle {
        id: station_list
        color: "red"
        width: 40
        height: 40
    }

    Rectangle {
        id: goal_list
        width: Math.max(max_t.width, max_w.width, min_p.width)
        height: max_t.height + max_w.height + min_p.height + tsu.height
        anchors.top: station_list.bottom

        Text {
            id: max_t
            text: "placeholder max_t"
        }
        Text {
            id: max_w
            text: "placeholder max_w"
            anchors.top: max_t.bottom
        }
        Text {
            id: min_p
            text: "placeholder min_p"
            anchors.top: max_w.bottom
        }
        Text {
            id: tsu
            anchors.top: min_p.bottom
        }
    }
}
