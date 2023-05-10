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

    function jitterCoordinate(c) {
        return c + Math.random() / 1000
    }

    Connections {
        target: WeatherAPI

        function onSendGoalList(goal_list) {
            max_t.text = "Highest temperature: " + goal_list["highest_temperature"][0]
            const tc = goal_list["highest_temperature"][1]
            station_for_temp.coordinate = QtPositioning.coordinate(jitterCoordinate(tc.x), jitterCoordinate(tc.y))

            max_w.text = "Highest wind speed: " + goal_list["strongest_wind"][0]
            const wc = goal_list["strongest_wind"][1]
            station_for_wind.coordinate = QtPositioning.coordinate(jitterCoordinate(wc.x), jitterCoordinate(wc.y))

            min_p.text = "Lowest pressure: " + goal_list["lowest_pressure"][0]
            const pc = goal_list["lowest_pressure"][1]
            station_for_pres.coordinate = QtPositioning.coordinate(jitterCoordinate(pc.x), jitterCoordinate(pc.y))
        }

        function onSendFullStationList(full_station_list) {
            for (let j = 0; j < full_station_list.length; ++j) {
                const n  = full_station_list[j]["station_name"]
                const cx = full_station_list[j]["station_coor"].x.toFixed(2)
                const cy = full_station_list[j]["station_coor"].y.toFixed(2)
                const t  = full_station_list[j]["temperature"].toFixed(2)
                let p  = full_station_list[j]["pressure"]
                p = p === undefined ? "N/A" : p.toFixed(2)
                let w  = full_station_list[j]["windspeed"]
                w = w === undefined ? "N/A" : w.toFixed(2)

                station_list.children[j].text = `${n} (${cx}, ${cy}): ${t}ºC, ${p}hPa, ${w}m/s`
            }
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
            anchorPoint.x: img1.width/4
            anchorPoint.y: img1.height
            coordinate: QtPositioning.coordinate(59.91, 10.75)
            sourceItem: Image { id: img1; height: 32; width: 32; source: "marker-red.png" }
        }

        MapQuickItem {
            id: station_for_wind
            anchorPoint.x: img2.width/4
            anchorPoint.y: img2.height
            coordinate: QtPositioning.coordinate(61.91, 10.75)
            sourceItem: Image { id: img2; height: 32; width: 32; source: "marker-blue.png" }
        }

        MapQuickItem {
            id: station_for_pres
            anchorPoint.x: img3.width/4
            anchorPoint.y: img3.height
            coordinate: QtPositioning.coordinate(68.91, 10.75)
            sourceItem: Image { id: img3; height: 32; width: 32; source: "marker-green.png" }
        }
    }

    Grid {
        id: station_list
        width: Math.max(row1.width, row2.width, row3.width, row4.width, row5.width, row6.width)
        height: station_list.children[0].height * station_list.children.size
        columns: 1
        Text { id: row1 }
        Text { id: row2 }
        Text { id: row3 }
        Text { id: row4 }
        Text { id: row5 }
        Text { id: row6 }
    }

    Rectangle {
        id: goal_list
        width: Math.max(max_t.width, max_w.width, min_p.width)
        height: max_t.height + max_w.height + min_p.height + tsu.height
        anchors.top: station_list.bottom

        Text { id: max_t; color: "red" }
        Text { id: max_w; color: "blue"; anchors.top: max_t.bottom }
        Text { id: min_p; color: "green"; anchors.top: max_w.bottom }
        Text { id: tsu; anchors.top: min_p.bottom }
    }
}
