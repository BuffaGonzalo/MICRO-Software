#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QResizeEvent>

#include <QtSerialPort/QSerialPort>
#include <QtNetwork/QUdpSocket>
#include <QLabel>
#include <QInputDialog>
#include <QTimer>
#include <QTime>
#include <QSerialPortInfo>

#include <QQuickWidget>
#include <QQuickItem>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void dataReceived();

    void decodeData(uint8_t *datosRx, uint8_t source);

    void sendDataSerial();

    void timeOut();

    void sendDataUDP();

    void OnUdpRxData();

    void on_pushButton_connectSerial_clicked();

    void on_pushButton_sendSerial_clicked();

    void on_pushButton_connectUdp_clicked();

    void on_pushButton_sendUdp_clicked();

    void getData();

    /**
     * @brief buildPayload - Construye la carga útil del comando leyendo la UI
     * @param payload - Puntero al buffer donde se guardarán los datos
     * @param length - Referencia a la variable que guardará la longitud final
     * @return true si el usuario completó los datos, false si canceló
     */
    bool buildPayload(uint8_t *payload, uint8_t &length);

    /**
     * @brief sendSerial - Comando utilizado para enviar datos por el puerto serial en segundo plano
     * @param buf - Comando enviado por serial
     * @param length - longitud del comando en bytes
     */

    void sendSerial(uint8_t *buf, uint8_t length);

    /**
     * @brief sendUdp - Comando utilizado para enviar datos por wifi median udp en segundo plano
     * @param buf - Comando enviado por serial
     * @param length - longitud del comando en bytes
     */
    void sendUdp(uint8_t *buf, uint8_t length);

    bool eventFilter(QObject *watched, QEvent *event);


private:
    Ui::MainWindow *ui;
    QSerialPort *QSerialPort1;
    QLabel *statusMode;

    //Debug *myDebug;

    //timers
    QTimer  *timer1;
    QTimer  *timer2;

    //variables comunicacion udp
    QUdpSocket *QUdpSocket1;
    QHostAddress RemoteAddress;
    quint16 RemotePort;
    QHostAddress clientAddress;
    int puertoremoto;

    //otras
    bool firExe; //bool utilizado para dibujar el fondo del radar
    bool servoDir; //bool utilizado para modificar el sentido de giro del servo
    bool firRadarExe;
    int contadorAlive=0;
    int angle;

    uint32_t lftEncData;
    uint32_t rhtEncData;

    typedef enum{
        START,
        HEADER_1,
        HEADER_2,
        HEADER_3,
        NBYTES,
        TOKEN,
        ID,
        PAYLOAD,
    }_eProtocolo;

    _eProtocolo estadoProtocolo,estadoProtocoloUdp;

    typedef enum{
        UDP=0,
        SERIE=1,
        ACK=0x0D,
        GETALIVE=0xF0,
        GETFIRMWARE=0xF1,
        GETMPU=0xF2,
        GETADC=0xF3,
        SETPWM=0xF4,
        SETPID=0XF5,
        SETPWMLIMIT=0xF6,
        SETLINECTRL=0xF7,
        UNKNOWCMD=0xFF,
        GETFLANKS=0xA8,
        POSITION=0xA9,
        OTHERS
    }_eCmd;

    typedef struct{
        uint8_t timeOut;
        uint8_t cheksum;
        uint8_t payLoad[256];
        uint8_t nBytes;
        uint8_t index;
    }_sDatos ;

    _sDatos rxData, rxDataUdp;

    typedef union {
        double  d32;
        float f32;
        int i32;
        unsigned int ui32;
        unsigned short ui16[2];
        short i16[2];
        uint8_t ui8[4];
        char chr[4];
        unsigned char uchr[4];
        int8_t  i8[4];
    }_udat;

    _udat myWord;


    QQuickWidget *view3D;
    float yawAcumulado = 0.0f; // Para acumular la rotación del giroscopio

};
#endif // MAINWINDOW_H
