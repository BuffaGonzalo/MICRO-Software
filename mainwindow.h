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

#include <QElapsedTimer>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

#include "graphics.h"

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

    void timeOut();

    void OnUdpRxData();

    void on_pushButton_connectSerial_clicked();

    void on_pushButton_connectUdp_clicked();

    void getData();



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

    /**
     * @brief sendCommand - Evalúa qué conexión está abierta (Serial o UDP) y envía la trama.
     * @param buf - Puntero al buffer de datos
     * @param length - longitud del comando en bytes
     */
    void sendCommand(uint8_t *buf, uint8_t length);

    bool eventFilter(QObject *watched, QEvent *event);


    void on_sendBalanceKp_clicked();
    void on_sendBalanceKi_clicked();
    void on_sendBalanceKd_clicked();
    void on_sendLineKp_clicked();
    void on_sendLineKd_clicked();

    void on_sendPWMMINL_clicked();
    void on_sendPWMMINR_clicked();
    void on_sendSetpoint_clicked();
    void on_sendAttackSetpoint_clicked();
    void on_setAttackSetpoint_valueChanged(int arg1);

    void on_sendPWML_clicked();
    void on_sendPWMR_clicked();
    void on_sendOFFSETL_clicked();
    void on_sendOFFSETR_clicked();
    void on_sendCustomTurn_clicked();
    void on_sendKpCascada_clicked();

    void on_sendFrontDistance_clicked();
    void on_sendSideDistance_clicked();
    void on_sendLostDistance_clicked();
    void on_sendStopCycles_clicked();
    void on_sendCornerDist_clicked();
    void on_sendAlignDist_clicked();
    void on_sendPWMLROT_clicked();
    void on_sendPWMRROT_clicked();
    void on_sendStaticOff_clicked();
    void on_sendMovingOff_clicked();
    void on_sendLimitAngle_clicked();
    void on_sendKiCascada_clicked();
    void on_sendFilterLPF_clicked();
    void on_sendVelDampDiv_clicked();
    void on_sendVelDampLim_clicked();
    void on_sendTurnLimit_clicked();

    void on_P1toP3_clicked();

    void on_P1toP2_clicked();

    void on_P2toP1_clicked();

    void on_P3toP1_clicked();

    void on_P3toP2_clicked();

    void on_P2toP3_clicked();

    void on_P1toP4_clicked();
    void on_P4toP1_clicked();

    void on_pushButton_exportExcel_clicked();
    void on_pushButton_exportIrCsv_clicked();
    void on_pushButton_exportTxt_clicked();

private:
    struct LogEntry {
        QDateTime time;
        QString data;
        QString type; // "RX", "TX", "UNKNOWN", "CHK_ERROR"
    };
    QList<LogEntry> m_logHistory;

    // --- Buffer circular de IR para exportación CSV ---
    static const int IR_BUFFER_SIZE = 3000; // ~1 minuto a ~50Hz
    struct IrSample {
        QDateTime timestamp;
        uint16_t ir1; // Sensor derecho (calibrado)
        uint16_t ir3; // Sensor centro (calibrado)
        uint16_t ir5; // Sensor izquierdo (calibrado)
    };
    QVector<IrSample> m_irBuffer;

    struct UpperIrSample {
        QDateTime timestamp;
        uint16_t ir1; // Raw Superior 1 (adcDataTx[0])
        uint16_t ir3; // Raw Superior 2 (adcDataTx[2])
        uint16_t ir5; // Raw Superior 3 (adcDataTx[4])
        uint16_t ir7; // Raw Superior 4 (adcDataTx[6])
        uint16_t ir8; // Raw Superior 5 (adcDataTx[7])
    };
    QVector<UpperIrSample> m_upperIrBuffer;

    int m_irExportCount = 0;  // Número de exportación devuelto por el STM32

    void exportIrCsvToFile();

    // Estadísticas
    int m_countSent = 0;
    int m_countReceived = 0;
    int m_countUnknown = 0;
    int m_countChecksumErrors = 0;

    void addLogEntry(const QString &data, const QString &type);
    void cleanOldLogs();
    Ui::MainWindow *ui;
    QSerialPort *QSerialPort1;
    QLabel *statusMode;

    //Debug *myDebug;

    //timers
    QTimer  *timer1;
    QTimer  *timer2;


    graphics *myGraphics;

    // --- Gráfica PID embebida en MainWindow ---
    QChart       *chartPID_mw;
    QLineSeries  *pid_pSeries;
    QLineSeries  *pid_iSeries;
    QLineSeries  *pid_dSeries;
    QLineSeries  *pid_outSeries;
    QLineSeries  *pid_pLineSeries;
    QLineSeries  *pid_dLineSeries;
    QLineSeries  *pid_outLineSeries;

    // Series de MPU
    QLineSeries  *pid_axSeries;
    QLineSeries  *pid_aySeries;
    QLineSeries  *pid_azSeries;
    QLineSeries  *pid_gxSeries;
    QLineSeries  *pid_gySeries;
    QLineSeries  *pid_gzSeries;

    // Series de Ángulos
    QLineSeries  *pid_pitchSeries;
    QLineSeries  *pid_rollSeries;
    QLineSeries  *pid_yawSeries;

    // Series de 8 Sensores IR
    QLineSeries  *pid_ir1Series;
    QLineSeries  *pid_ir2Series;
    QLineSeries  *pid_ir3Series;
    QLineSeries  *pid_ir4Series;
    QLineSeries  *pid_ir5Series;
    QLineSeries  *pid_ir6Series;
    QLineSeries  *pid_ir7Series;
    QLineSeries  *pid_ir8Series;
    QValueAxis   *pid_axisX;
    QValueAxis   *pid_axisY;
    double        pid_yMin = -10.0;
    double        pid_yMax =  10.0;

    // Valores calibrados guardados para cálculo del PID del seguidor
    uint16_t      m_calIr1 = 0;
    uint16_t      m_calIr3 = 0;
    uint16_t      m_calIr5 = 0;

    void initPIDChart();
    void updatePIDChart(double time, double p, double i, double d, double out, double pLine, double dLine, double outLine);
    void updateMPUChart(double time, double ax, double ay, double az, double gx, double gy, double gz, double pitch, double roll, double yaw);
    void updateIRChart(double time, double ir1, double ir2, double ir3, double ir4, double ir5, double ir6, double ir7, double ir8);
    void updatePIDChartRange();
    void resetInterface();

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
    bool paramsSynced = false;

    uint32_t lftEncData;
    uint32_t rhtEncData;

    QElapsedTimer runtimeTimer;

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

        GETALIVE=0xA0,
        GETFIRMWARE=0xA1,
        GETMPU=0xA2,
        GETADC=0xA3,

        SETPWML=0xA4, // -> divisible en 2
        SETPWMR=0xA5,

        SETPWMMINR=0xA6, //-> divisible en max y min
        SETPWMMINL=0xA7,

        SETBALANCEKP=0XA8, //->kp,kd,ki
        SETBALANCEKD=0xA9,
        SETBALANCEKI=0xAA,

        SETSETPOINT=0xAB,

        SETLINEKP=0xAC,
        SETLINEKD=0xAD,

        GETINTERNALDATA = 0xF0, // ->revision exaustiva
        GETPIDBALANCE = 0xF1,

        SETOFFSETL=0xAE,
        SETOFFSETR=0xAF,

        SETCUSTOMTURN = 0xB0, //Valor de rotacion al seguir linea
        SET_KP_EXT = 0xC3, // Ganancia proporcional de lazo externo (x1000)
        SETSPEED = 0xB1, //angulo de ataque movimiento
        SETBKANG = 0xB2, //angulo contra para evitar el aumento de velocidad

        SETFRONTDIST = 0xB3,
        SETSIDEDIST = 0xB4,
        SETLOSTDIST = 0xB5,
        SETSTOPCYCLES = 0xB6,
        SETCORNERDIST = 0xB7,
        SETALIGNDIST = 0xB8,

        SETPWMLROT = 0xB9,
        SETPWMRROT = 0xC0,

        SETSTATICOFF = 0xC1,
        SETMOVINGOFF = 0xC2,
        SETLIMITANG = 0xC4,
        SET_KI_EXT = 0xC5, // Ganancia integral de lazo externo (x10000)
        SET_ALFA_LPF = 0xC6, // Coeficiente alfa del filtro LPF (0-100)
        SETVELDAMPDIV = 0xC7,
        SETVELDAMPLIM = 0xC8,
        SETTURNLIMIT = 0xC9,
        EXPORTIRCSV  = 0xCA,

        UNKNOWCMD=0xFF,
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
