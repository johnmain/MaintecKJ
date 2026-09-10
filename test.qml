import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 400
    height: 300
    title: "Test Window"
    
    Label {
        text: "Hello World!"
        anchors.centerIn: parent
    }
}