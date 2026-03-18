import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import "resources"

View3D {
    id: root

    // ── Propiedades que C++ escribe con setProperty() ──────────────
    // Los valores que llegan desde el MPU ya son grados (ej: -2.14, 1.85)
    property real carPitch: 0   // cabeceo  → eje X del mundo
    property real carRoll:  0   // balanceo → eje Z del mundo
    property real carYaw:   0   // giro     → eje Y del mundo

    // ── Función que construye el quaternion final ──────────────────
    // Un quaternion representa una rotación como:
    //   q = (cos(ángulo/2), sin(ángulo/2)*eje_x, sin(ángulo/2)*eje_y, sin(ángulo/2)*eje_z)
    // Qt.quaternion(w, x, y, z)
    //
    // IMPORTANTE: los ángulos vienen en GRADOS desde el MPU,
    // hay que convertirlos a RADIANES para Math.sin/cos
    // Dividimos el ángulo por 2 porque el quaternion trabaja con la mitad del ángulo

    function buildRotation(pitch, roll, yaw) {

        // Convertimos cada ángulo a radianes y lo dividimos por 2
        var halfP = pitch * Math.PI / 360.0   // pitch/2 en radianes
        var halfR = -roll  * Math.PI / 360.0   // roll/2  en radianes
        var halfY = yaw   * Math.PI / 360.0   // yaw/2   en radianes

        // Quaternion para Pitch (rotación alrededor del eje X)
        // eje X = (1, 0, 0)  →  Qt.quaternion(w, x*sin, y*sin, z*sin)
        var qPitch = Qt.quaternion(
            Math.cos(halfP),   // w
            Math.sin(halfP),   // x ← eje X activo
            0,                 // y
            0                  // z
        )

        // Quaternion para Yaw (rotación alrededor del eje Y)
        // eje Y = (0, 1, 0)
        var qYaw = Qt.quaternion(
            Math.cos(halfY),   // w
            0,                 // x
            Math.sin(halfY),   // y ← eje Y activo
            0                  // z
        )

        // Quaternion para Roll (rotación alrededor del eje Z)
        // eje Z = (0, 0, 1)
        var qRoll = Qt.quaternion(
            Math.cos(halfR),   // w
            0,                 // x
            0,                 // y
            Math.sin(halfR)    // z ← eje Z activo
        )

        // Multiplicamos en orden aeronáutico: Yaw → Pitch → Roll
        // La multiplicación de quaternions combina las rotaciones
        // SIN que una afecte los ejes de la otra (no hay Gimbal Lock)
        return qYaw.times(qPitch).times(qRoll)
    }

    // ── Entorno ────────────────────────────────────────────────────
    environment: SceneEnvironment {
        clearColor: "#b0c4de"
        backgroundMode: SceneEnvironment.Color
    }

    // ── Cámara ─────────────────────────────────────────────────────
    PerspectiveCamera {
        id: camera
        z: 300
        y: 100
        eulerRotation.x: -15
    }

    // ── Control de órbita con el mouse ─────────────────────────────
    OrbitCameraController {
        anchors.fill: parent
        origin: carModel
        camera: camera
    }

    // ── Iluminación ────────────────────────────────────────────────
    DirectionalLight {
        eulerRotation.x: -45
        eulerRotation.y: 45
        ambientColor: Qt.rgba(0.5, 0.5, 0.5, 1.0)
    }

    // ── Modelo 3D del robot ────────────────────────────────────────
    AutoMicroSimplify {
        id: carModel
        position: Qt.vector3d(0, 0, 0)
        scale: Qt.vector3d(1500, 1500, 1500)

        // ✅ Aquí es donde se aplica el quaternion al modelo
        // Cada vez que carPitch, carRoll o carYaw cambian (porque C++
        // llama a setProperty), Qt recalcula esta expresión automáticamente
        rotation: root.buildRotation(root.carPitch, root.carRoll, root.carYaw)
    }
}
