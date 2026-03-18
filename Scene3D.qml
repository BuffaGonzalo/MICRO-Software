import QtQuick
import QtQuick3D
import "resources"

View3D {
    id: root

    // Propiedades expuestas a C++ para la telemetría del MPU
    property real carPitch: 0
    property real carRoll: 0
    property real carYaw: 0

    environment: SceneEnvironment {
        // Fondo cambiado a un azul acero claro para máximo contraste
        clearColor: "#b0c4de"
        backgroundMode: SceneEnvironment.Color
    }

    PerspectiveCamera {
        id: camera
        z: 300
        y: 100
        eulerRotation.x: -15
    }

    DirectionalLight {
        eulerRotation.x: -45
        eulerRotation.y: 45
        ambientColor: Qt.rgba(0.5, 0.5, 0.5, 1.0)
    }

    // TU ROBOT BALANCÍN
    AutoMicroSimplify {
        id: carModel

        position: Qt.vector3d(0, 0, 0)
        scale: Qt.vector3d(1500, 1500, 1500)

        // INVERSIÓN DEL PITCH: Agregamos el signo negativo (-)
        eulerRotation.x: root.carPitch

        eulerRotation.z: -root.carRoll
        eulerRotation.y: root.carYaw
    }
}
