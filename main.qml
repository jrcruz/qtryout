import QtQuick 2.15
import QtQuick.Window 2.15
import jcruz.eu 1.0
import QtLocation 5.15
import QtPositioning 5.15

Window {
    width: Qt.platform.os == "android" ? Screen.width : 512
    height: Qt.platform.os == "android" ? Screen.height : 512
    visible: true
    title: map.center + " zoom " + map.zoomLevel.toFixed(3)
           + " min " + map.minimumZoomLevel + " max " + map.maximumZoomLevel

    Connections {
        target: WeatherAPI
        function onValuesChanged(val) {
            console.log("tsukihi pls")
            max_t.text = val[0]
            max_w.text = val[1]
            min_p.text = val[2]
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
        center: QtPositioning.coordinate(59.91, 10.75) // Oslo
        zoomLevel: 14
        property var startCentroid

        property MapCircle temp
        property MapCircle pres
        property MapCircle wind

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
            id: marker
            anchorPoint.x: image.width/4
            anchorPoint.y: image.height
            coordinate: QtPositioning.coordinate(59.91, 10.75)
            sourceItem: Image {
                id: image
                height: 32
                width: 32
                source: "marker.png"
            }
        }

    }
    Rectangle {
        id: test
        width: 300
        height: max_t.height + max_w.height + min_p.height + tsu.height
        color: "red"

        Text {
            id: max_t
            text: "1"
        }
        Text {
            id: max_w
            text: "2"
            anchors.top: max_t.bottom
        }
        Text {
            id: min_p
            text: "3"
            anchors.top: max_w.bottom
        }
        Text {
            id: tsu
            anchors.top: min_p.bottom
        }
    }

    /*
    MapQuickItem {
        id: marker2
        anchorPoint.x: image2.width/4
        anchorPoint.y: image2.height

        sourceItem: Image {
            id: image2
        }
    }

    MapQuickItem {
        id: marker3
        anchorPoint.x: image3.width/4
        anchorPoint.y: image3.height

        sourceItem: Image {
            id: image3
        }
    }
    */
}
