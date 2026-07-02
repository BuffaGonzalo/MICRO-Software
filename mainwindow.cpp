#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "internal_data.h"
#include <limits>
#include <QSpinBox>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QDateTime>

static bool isWaitingReply = false;
static int timeoutPatience = 0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Limitar los logs a los últimos 50 comandos
    ui->textBrowserProcessed->document()->setMaximumBlockCount(50);
    ui->textBrowserUnProcessed->document()->setMaximumBlockCount(50);

    initPIDChart();

    // Conectar checkboxes para activar/desactivar curvas de la gráfica PID
    connect(ui->checkBox_P, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_I, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_D, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Out, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_P_line, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_D_line, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Out_line, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Pitch, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Roll, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Yaw, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ax, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ay, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Az, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Gx, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Gy, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Gz, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir1, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir2, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir3, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir4, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir5, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir6, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir7, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);
    connect(ui->checkBox_Ir8, &QCheckBox::toggled, this, &MainWindow::updatePIDChartRange);

    timer1 = new QTimer(this);
    timer2 = new QTimer(this);

    ui->AutoWidget->setSource(QUrl(QStringLiteral("qrc:/Scene3D.qml")));
    ui->AutoWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    //comunicacion
    QSerialPort1 = new QSerialPort(this);
    QUdpSocket1 = new QUdpSocket(this);

    //debug de comandos
    myGraphics = new graphics(this);

    ui->comboBox_PORT->installEventFilter(this);

    //connects del puerto serial
    connect(QSerialPort1,&QSerialPort::readyRead,this,&MainWindow::dataReceived);

    //connects de los timers con las funciones
    connect(timer1,&QTimer::timeout,this,&MainWindow::timeOut);
    connect(timer2,&QTimer::timeout,this,&MainWindow::getData);

    //connects de udp
    connect(QUdpSocket1,&QUdpSocket::readyRead,this,&MainWindow::OnUdpRxData);

    //connect(ui->actionScanPorts, &QAction::triggered, settingPorts,&SettingsDialog::show);
    connect(ui->actionGRAPHICS, &QAction::triggered, myGraphics, &graphics::show);



    //inicializamos
    estadoProtocolo=START;
    rxData.timeOut=0;


    statusMode = new QLabel(this);
    ui->statusBar->addWidget(statusMode);

    runtimeTimer.start();

    timer1->start(100);
    timer2->start(35);

    // Permitir valores de hasta 10000 (y -10000) en todos los QSpinBox
    QList<QSpinBox *> spinBoxes = this->findChildren<QSpinBox *>();
    for (QSpinBox *spinBox : spinBoxes) {
        spinBox->setMinimum(-10000);
        spinBox->setMaximum(10000);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addLogEntry(const QString &data, const QString &type) {
    LogEntry entry;
    entry.time = QDateTime::currentDateTime();
    entry.data = data;
    entry.type = type;
    m_logHistory.append(entry);

    if (type == "UNKNOWN") m_countUnknown++;
    else if (type == "CHK_ERROR") m_countChecksumErrors++;

    cleanOldLogs();
}

void MainWindow::cleanOldLogs() {
    QDateTime fiveMinutesAgo = QDateTime::currentDateTime().addSecs(-300);
    while (!m_logHistory.isEmpty() && m_logHistory.first().time < fiveMinutesAgo) {
        m_logHistory.removeFirst();
    }
}

void MainWindow::on_pushButton_exportExcel_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "Exportar Historial (Excel/CSV)", 
                                                    "log_robot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv", 
                                                    "Archivos CSV (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "No se pudo crear el archivo.");
        return;
    }

    QTextStream out(&file);
    int lost = (m_countSent > m_countReceived) ? (m_countSent - m_countReceived) : 0;

    out << "TIEMPO;TIPO;DATO;ESTADISTICAS\n";
    out << ";;;Enviados: " << m_countSent << "\n";
    out << ";;;Recibidos: " << m_countReceived << "\n";
    out << ";;;Perdidos: " << lost << "\n";
    out << ";;;Desconocidos: " << m_countUnknown << "\n";
    out << ";;;Errores Checksum: " << m_countChecksumErrors << "\n\n";
    
    out << "TIEMPO;TIPO;DATO\n";

    for (const auto &entry : m_logHistory) {
        QString cleanData = entry.data;
        cleanData.replace("\n", " ").replace(";", ",");
        out << entry.time.toString("hh:mm:ss.zzz") << ";" << entry.type << ";" << cleanData << "\n";
    }

    file.close();
    QMessageBox::information(this, "Exportación Exitosa", 
                            QString("Se han exportado %1 registros de los últimos 5 minutos.").arg(m_logHistory.size()));
}

void MainWindow::on_pushButton_exportTxt_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "Exportar Historial (Texto Plano)", 
                                                    "log_robot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt", 
                                                    "Archivos de Texto (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "No se pudo crear el archivo.");
        return;
    }

    QTextStream out(&file);
    int lost = (m_countSent > m_countReceived) ? (m_countSent - m_countReceived) : 0;

    out << "====================================================\n";
    out << "        REPORTE DE ACTIVIDAD DEL ROBOT\n";
    out << "        Generado el: " << QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss") << "\n";
    out << "====================================================\n\n";
    
    out << "ESTADISTICAS (Ultimos 5 minutos):\n";
    out << "----------------------------------------------------\n";
    out << " Comandos Enviados:    " << m_countSent << "\n";
    out << " Comandos Recibidos:   " << m_countReceived << "\n";
    out << " Comandos Perdidos:    " << lost << "\n";
    out << " Comandos Desconocidos: " << m_countUnknown << "\n";
    out << " Errores de Checksum:  " << m_countChecksumErrors << "\n";
    out << "----------------------------------------------------\n\n";

    out << "DETALLE DE LOGS:\n";
    out << "TIMESTAMP    | TIPO      | DATO\n";
    out << "-------------|-----------|--------------------------\n";

    for (const auto &entry : m_logHistory) {
        out << entry.time.toString("hh:mm:ss.zzz").leftJustified(12) << " | "
            << entry.type.leftJustified(9) << " | "
            << entry.data.simplified() << "\n";
    }

    out << "\n--- Fin del reporte ---";

    file.close();
    QMessageBox::information(this, "Exportación Exitosa", "Historial exportado correctamente a .txt");
}

void MainWindow::dataReceived(){
    unsigned char *incomingBuffer;
    int count;
    //uint8_t xId;

    count = QSerialPort1->bytesAvailable();

    if(count<=0)
        return;

    incomingBuffer = new unsigned char[count];

    QSerialPort1->read((char *)incomingBuffer,count);

    QString str="";

    for(int i=0; i<count; i++){
        if(isalnum(incomingBuffer[i]))
            str = str + QString("%1").arg((char)incomingBuffer[i]);
        else
            str = str +"{" + QString("%1").arg(incomingBuffer[i],2,16,QChar('0')) + "}";
    }

    addLogEntry("MBED-->SERIAL-->PC (" + str + ")", "RX");
    ui->textBrowserUnProcessed->append("MBED-->SERIAL-->PC (" + str + ")");

    //Cada vez que se recibe un dato reinicio el timeOut
    rxData.timeOut=6;

    for(int i=0;i<count; i++){
        switch (estadoProtocolo) {
        case START:
            if (incomingBuffer[i]=='U'){
                estadoProtocolo=HEADER_1;
            }
            break;
        case HEADER_1:
            if (incomingBuffer[i]=='N')
                estadoProtocolo=HEADER_2;
            else{
                i--;
                estadoProtocolo=START;
            }
            break;
        case HEADER_2:
            if (incomingBuffer[i]=='E')
                estadoProtocolo=HEADER_3;
            else{
                i--;
                estadoProtocolo=START;
            }
            break;
        case HEADER_3:
            if (incomingBuffer[i]=='R')
                estadoProtocolo=NBYTES;
            else{
                i--;
                estadoProtocolo=START;
            }
            break;
        case NBYTES:
            rxData.nBytes=incomingBuffer[i];
            estadoProtocolo=TOKEN;
            break;
        case TOKEN:
            if (incomingBuffer[i]==':'){
                estadoProtocolo=PAYLOAD;
                rxData.cheksum='U'^'N'^'E'^'R'^ rxData.nBytes^':';
                rxData.payLoad[0]=rxData.nBytes;
                rxData.index=1;
            }
            else{
                i--;
                estadoProtocolo=START;
            }
            break;
        case PAYLOAD:
            if (rxData.nBytes>1){
                rxData.payLoad[rxData.index++]=incomingBuffer[i];
                rxData.cheksum^=incomingBuffer[i];
            }
            rxData.nBytes--;
            if(rxData.nBytes==0){
                estadoProtocolo=START;
                if(rxData.cheksum==incomingBuffer[i]){
                    decodeData(&rxData.payLoad[0], SERIE);
                }else{
                    addLogEntry("Chk Calculado ** " +QString().number(rxData.cheksum,16) + " **", "CHK_ERROR");
                    ui->textBrowserUnProcessed->append("Chk Calculado ** " +QString().number(rxData.cheksum,16) + " **" );
                    addLogEntry("Chk recibido ** " +QString().number(incomingBuffer[i],16) + " **", "CHK_ERROR");
                    ui->textBrowserUnProcessed->append("Chk recibido ** " +QString().number(incomingBuffer[i],16) + " **" );

                }
            }
            break;
        default:
            estadoProtocolo=START;
            break;
        }
    }
    delete [] incomingBuffer;

}

static const uint16_t PC_LUT_Y_SCALE[16] = {0, 67, 133, 200, 267, 333, 400, 467, 533, 600, 667, 733, 800, 867, 933, 1000};

static const uint16_t PC_LUT_IR0[16] = {514, 546, 553, 569, 576, 578, 583, 590, 594, 608, 618, 659, 1589, 2721, 3890, 3968};
static const uint16_t PC_LUT_IR2[16] = {1283, 1648, 1679, 1687, 1692, 1705, 1712, 1718, 1721, 1725, 1732, 1740, 1749, 1754, 1762, 3968};
static const uint16_t PC_LUT_IR4[16] = {2120, 2134, 2139, 2144, 2148, 2152, 2155, 2158, 2162, 2165, 2169, 2182, 2229, 2255, 2266, 3973};
static const uint16_t PC_LUT_IR6[16] = {2166, 2183, 2187, 2193, 2196, 2200, 2203, 2209, 2215, 2237, 2253, 2264, 2276, 2442, 2600, 3984};
static const uint16_t PC_LUT_IR7[16] = {1068, 1093, 1101, 1107, 1111, 1117, 1123, 1133, 1142, 1151, 1159, 1187, 1291, 1433, 1610, 3940};

static uint16_t PC_LUT_Interpolate(const uint16_t *x, const uint16_t *lut_y, uint16_t raw) {
    if(raw <= x[0])
        return lut_y[0];

    if(raw >= x[15])
        return lut_y[15];

    for(int i = 0; i < 15; i++) {
        if(raw >= x[i] && raw <= x[i + 1]) {
            uint32_t diff_x = x[i + 1] - x[i];
            if (diff_x == 0) return lut_y[i];
            uint32_t diff_y = lut_y[i + 1] - lut_y[i];
            uint32_t offset_x = raw - x[i];
            uint32_t y = lut_y[i] + (((offset_x * diff_y) + (diff_x / 2)) / diff_x);
            return (uint16_t)y;
        }
    }
    return lut_y[15];
}

void MainWindow::decodeData(uint8_t *datosRx, uint8_t source){
    m_countReceived++; // Comando completo y válido recibido

    //int32_t length = sizeof(*datosRx)/sizeof(datosRx[0]);
    int32_t length = datosRx[0];
    uint8_t id = datosRx[1];
    QString str, strOut;
    _udat w;

    // --- SEMÁFORO PING-PONG ---
    // Solo liberamos el semáforo si la respuesta pertenece a un comando solicitado (MPU, ADC, etc).
    // El heartbeat ALIVE es autónomo del STM32, NO debe liberar el semáforo
    // porque no fue una respuesta a algo que nosotros pedimos.
    if (id != GETALIVE) {
        isWaitingReply = false;
    }

    for(int i = 1; i<length; i++){
        if(isalnum(datosRx[i]))
            str = str + QString("%1").arg(char(datosRx[i]));
        else
            str = str +QString("%1").arg(datosRx[i],2,16,QChar('0'));
    }
    addLogEntry("*(MBED-S->PC)->decodeData (" + str + ")", "RX");
    ui->textBrowserUnProcessed->append("*(MBED-S->PC)->decodeData (" + str + ")");

    str=QString().number(datosRx[0]);
    addLogEntry(str, "RX");
    ui->textBrowserProcessed->append(str);
    str=QString().number(datosRx[1]);
    addLogEntry(str, "RX");
    ui->textBrowserProcessed->append(str);

    switch (datosRx[1]) {
    case GETALIVE://     GETALIVE=0xF0,
        if(datosRx[2]==ACK){
            contadorAlive++;
            if(source)
                str="ALIVE BLUEPILL VIA *SERIE* RECIBIDO!!!";
            else{
                str="ALIVE BLUEPILL VIA *UDP* RECIBIDO N°: " + QString().number(contadorAlive,10);
            }
        }else{
            str= "ALIVE BLUEPILL VIA *SERIE*  NO ACK!!!";
        }
        addLogEntry(str, "RX");
        ui->textBrowserProcessed->append(str);
        break;
    case GETFIRMWARE://     GETFIRMWARE=0xF1
        str = "FIRMWARE: ";
        for(uint8_t i=0;i<(datosRx[0]-2);i++){ //datosRx[0] -> tamaño en bytes del mensaje.
            str += (QChar)datosRx[2+i];
        }
        addLogEntry(str, "RX");
        ui->textBrowserProcessed->append(str);
        break;
    case GETMPU:{

        //Datos acelerometro
        w.i8[0] = datosRx[2];
        w.i8[1] = datosRx[3];

        float ax = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Ax: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ax_data->display(str);
          ///  setText(str);

        w.i8[0] = datosRx[4];
        w.i8[1] = datosRx[5];

        float ay = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Ay: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ay_data->display(str);

        w.i8[0] = datosRx[6];
        w.i8[1] = datosRx[7];

        float az = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Az: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->az_data->display(str);

        //Datos giroscopio
        w.i8[0] = datosRx[8];
        w.i8[1] = datosRx[9];

        float gx = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gx: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->gx_data->display(str);

        w.i8[0] = datosRx[10];
        w.i8[1] = datosRx[11];

        float gy = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gy: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->gy_data->display(str);

        w.i8[0] = datosRx[12];
        w.i8[1] = datosRx[13];

        float gz = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gz: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->gz_data->display(str);

        // 2. Calcular ángulos
        float roll = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / M_PI;
        float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / M_PI;

        // (Opcional) Calcular Yaw integrando el giroscopio
        float gz_grados_seg = gz / 131.0f; // Asumiendo escala de +/- 250deg/s
        if (abs(gz_grados_seg) > 1.0f) {
            yawAcumulado += gz_grados_seg * 0.5f; // asumiendo 100ms de muestreo de tu timer1
        }
        // Añade esta línea para imprimir los ángulos finales en la consola de Qt Creator
        qDebug() << "Angulos Calculados -> Pitch:" << pitch << " | Roll:" << roll << " | Yaw:" << yawAcumulado;

        // ---- NUEVO: Enviar a la gráfica ----
        double t = runtimeTimer.elapsed() / 1000.0;
        updateMPUChart(t, ax, ay, az, gx, gy, gz, pitch, roll, yawAcumulado);

        // 3. Enviar los ángulos a Qt Quick 3D
        if (ui->AutoWidget && ui->AutoWidget->rootObject()) {
            ui->AutoWidget->rootObject()->setProperty("carPitch", roll);
            ui->AutoWidget->rootObject()->setProperty("carRoll", pitch);
            ui->AutoWidget->rootObject()->setProperty("carYaw", yawAcumulado);
        }
        break;
    }
    case GETADC: {
        int16_t sumLineSensors = 0;
        //Datos acelerometro
        w.ui8[0] = datosRx[2];
        w.ui8[1] = datosRx[3];
        uint16_t ir1 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR1: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir1_data->display(str);

        w.ui8[0] = datosRx[4];
        w.ui8[1] = datosRx[5];
        uint16_t ir2 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR2: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir2_data->display(str);
        sumLineSensors += qMax(0, 2400 - w.i16[0]);

        w.ui8[0] = datosRx[6];
        w.ui8[1] = datosRx[7];
        uint16_t ir3 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR3: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir3_data->display(str);

        w.ui8[0] = datosRx[8];
        w.ui8[1] = datosRx[9];
        uint16_t ir4 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR4: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir4_data->display(str);
        sumLineSensors += qMax(0, 2400 - w.i16[0]);

        w.ui8[0] = datosRx[10];
        w.ui8[1] = datosRx[11];
        uint16_t ir5 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR5: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir5_data->display(str);

        w.ui8[0] = datosRx[12];
        w.ui8[1] = datosRx[13];
        uint16_t ir6 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR6: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir6_data->display(str);
        sumLineSensors += qMax(0, 2400 - w.i16[0]);
        str = QString("%1").arg(sumLineSensors, 5, 10, QChar('0'));
        ui->sumSensors_data->display(str);

        w.ui8[0] = datosRx[14];
        w.ui8[1] = datosRx[15];
        uint16_t ir7 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR7: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir7_data->display(str);

        w.ui8[0] = datosRx[16];
        w.ui8[1] = datosRx[17];
        uint16_t ir8 = w.ui16[0];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR8: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir8_data->display(str);

        // ---- NUEVO: Enviar a la gráfica ----
        double t = runtimeTimer.elapsed() / 1000.0;
        updateIRChart(t, ir1, ir2, ir3, ir4, ir5, ir6, ir7, ir8);

        // --- Acumular muestra en buffer circular de IR superiores (último minuto) ---
        UpperIrSample upperSample;
        upperSample.timestamp = QDateTime::currentDateTime();
        upperSample.ir1 = ir1;
        upperSample.ir3 = ir3;
        upperSample.ir5 = ir5;
        upperSample.ir7 = ir7;
        upperSample.ir8 = ir8;
        m_upperIrBuffer.append(upperSample);
        if (m_upperIrBuffer.size() > IR_BUFFER_SIZE)
            m_upperIrBuffer.removeFirst();

        // ---- NUEVO: Calcular y mostrar normalización de sensores de distancia superiores ----
        uint16_t norm_ir0 = PC_LUT_Interpolate(PC_LUT_IR0, PC_LUT_Y_SCALE, ir1);
        uint16_t norm_ir2 = PC_LUT_Interpolate(PC_LUT_IR2, PC_LUT_Y_SCALE, ir3);
        uint16_t norm_ir4 = PC_LUT_Interpolate(PC_LUT_IR4, PC_LUT_Y_SCALE, ir5);
        uint16_t norm_ir6 = PC_LUT_Interpolate(PC_LUT_IR6, PC_LUT_Y_SCALE, ir7);
        uint16_t norm_ir7 = PC_LUT_Interpolate(PC_LUT_IR7, PC_LUT_Y_SCALE, ir8);

        ui->raw_ir0_lcd->display(ir1);
        ui->raw_ir2_lcd->display(ir3);
        ui->raw_ir4_lcd->display(ir5);
        ui->raw_ir6_lcd->display(ir7);
        ui->raw_ir7_lcd->display(ir8);

        ui->norm_ir0_lcd->display(norm_ir0);
        ui->norm_ir2_lcd->display(norm_ir2);
        ui->norm_ir4_lcd->display(norm_ir4);
        ui->norm_ir6_lcd->display(norm_ir6);
        ui->norm_ir7_lcd->display(norm_ir7);

        break;
    }
    case GETINTERNALDATA: {
        w.ui8[0] = datosRx[58]; w.ui8[1] = datosRx[59]; w.ui8[2] = datosRx[60]; w.ui8[3] = datosRx[61];
        float hr_angle = w.i32 / 10000.0f;
        ui->angle_hr_data->display(QString::number(hr_angle, 'f', 4));

        // Unpack raw IR values (IR1, IR3, IR5) with correct mapping: index 1 is Left (IR5), index 5 is Right (IR1)
        w.ui8[0] = datosRx[68]; w.ui8[1] = datosRx[69];
        uint16_t rawIr5 = w.ui16[0];
        ui->raw_ir5_lcd->display(rawIr5);
        w.ui8[0] = datosRx[70]; w.ui8[1] = datosRx[71];
        uint16_t rawIr3 = w.ui16[0];
        ui->raw_ir3_lcd->display(rawIr3);
        w.ui8[0] = datosRx[72]; w.ui8[1] = datosRx[73];
        uint16_t rawIr1 = w.ui16[0];
        ui->raw_ir1_lcd->display(rawIr1);

        // Unpack calibrated IR values (IR1, IR3, IR5) with correct mapping
        w.ui8[0] = datosRx[74]; w.ui8[1] = datosRx[75];
        uint16_t calIr5 = w.ui16[0];
        ui->cal_ir5_lcd->display(calIr5);
        m_calIr5 = calIr5;
        w.ui8[0] = datosRx[76]; w.ui8[1] = datosRx[77];
        uint16_t calIr3 = w.ui16[0];
        ui->cal_ir3_lcd->display(calIr3);
        m_calIr3 = calIr3;
        w.ui8[0] = datosRx[78]; w.ui8[1] = datosRx[79];
        uint16_t calIr1 = w.ui16[0];
        ui->cal_ir1_lcd->display(calIr1);
        m_calIr1 = calIr1;

        // --- Acumular muestra en buffer circular IR (último minuto) ---
        IrSample sample;
        sample.timestamp = QDateTime::currentDateTime();
        sample.ir1 = rawIr1; // Guardar el valor sin procesar (raw)
        sample.ir3 = rawIr3; // Guardar el valor sin procesar (raw)
        sample.ir5 = rawIr5; // Guardar el valor sin procesar (raw)
        m_irBuffer.append(sample);
        if (m_irBuffer.size() > IR_BUFFER_SIZE)
            m_irBuffer.removeFirst();

        if (!paramsSynced) {
            // 1. PID Balancín (indices 2 a 11)
            w.ui8[0] = datosRx[2];  w.ui8[1] = datosRx[3];  ui->setBalanceKp->setValue(w.i16[0]);
            w.ui8[0] = datosRx[4];  w.ui8[1] = datosRx[5];  ui->setBalanceKi->setValue(w.i16[0]);
            w.ui8[0] = datosRx[6];  w.ui8[1] = datosRx[7];  ui->setBalanceKd->setValue(w.i16[0]);
            w.ui8[0] = datosRx[8];  w.ui8[1] = datosRx[9];  ui->setPWMMINR->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[10]; w.ui8[1] = datosRx[11]; ui->setPWMMINL->setValue(w.ui16[0]);

            // 2. Setpoint (indices 12 a 15)
            w.ui8[0] = datosRx[12]; w.ui8[1] = datosRx[13];
            w.ui8[2] = datosRx[14]; w.ui8[3] = datosRx[15];
            ui->setSetpoint->setValue(w.i32);

            // 3. Extra (indices 16 a 29)
            w.ui8[0] = datosRx[16]; w.ui8[1] = datosRx[17]; ui->setLineKp->setValue(w.i16[0]);
            w.ui8[0] = datosRx[18]; w.ui8[1] = datosRx[19]; ui->setLineKd->setValue(w.i16[0]);
            w.ui8[0] = datosRx[20]; w.ui8[1] = datosRx[21]; ui->setOFFSETL->setValue(w.i16[0]);
            w.ui8[0] = datosRx[22]; w.ui8[1] = datosRx[23]; ui->setOFFSETR->setValue(w.i16[0]);
            w.ui8[0] = datosRx[24]; w.ui8[1] = datosRx[25]; ui->setCustomTurn->setValue(w.i16[0]);
            w.ui8[0] = datosRx[26]; w.ui8[1] = datosRx[27];
            ui->setAttackSetpoint->blockSignals(true);
            ui->setAttackSetpoint->setValue(w.i16[0]);
            ui->setAttackSetpoint->blockSignals(false);

            // 4. Esquivador (indices 30 a 39)
            w.ui8[0] = datosRx[30]; w.ui8[1] = datosRx[31]; ui->setFrontDistance->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[32]; w.ui8[1] = datosRx[33]; ui->setSideDistance->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[34]; w.ui8[1] = datosRx[35]; ui->setLostDistance->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[36]; w.ui8[1] = datosRx[37]; ui->setStopCycles->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[38]; w.ui8[1] = datosRx[39]; ui->setCornerDist->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[40]; w.ui8[1] = datosRx[41]; ui->setAlignDist->setValue(w.ui16[0]);
            
            // 5. Rotación (indices 42 a 45)
            w.ui8[0] = datosRx[42]; w.ui8[1] = datosRx[43]; ui->setPWMLROT->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[44]; w.ui8[1] = datosRx[45]; ui->setPWMRROT->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[46]; w.ui8[1] = datosRx[47]; ui->setStaticOff->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[48]; w.ui8[1] = datosRx[49]; ui->setMovingOff->setValue(w.ui16[0]);
            w.ui8[0] = datosRx[50]; w.ui8[1] = datosRx[51]; ui->setLimitAngle->setValue(w.i16[0]);
            w.ui8[0] = datosRx[52]; w.ui8[1] = datosRx[53]; ui->setKpCascada->setValue(w.i16[0]);
            w.ui8[0] = datosRx[54]; w.ui8[1] = datosRx[55]; ui->setKiCascada->setValue(w.i16[0]);
            w.ui8[0] = datosRx[56]; w.ui8[1] = datosRx[57]; ui->setFilterLPF->setValue(w.i16[0]);
            w.ui8[0] = datosRx[62]; w.ui8[1] = datosRx[63]; ui->setVelDampDiv->setValue(w.i16[0]);
            w.ui8[0] = datosRx[64]; w.ui8[1] = datosRx[65]; ui->setVelDampLim->setValue(w.i16[0]);
            w.ui8[0] = datosRx[66]; w.ui8[1] = datosRx[67]; ui->setTurnLimit->setValue(w.i16[0]);

            paramsSynced = true;
            addLogEntry("***PARÁMETROS SINCRONIZADOS DESDE STM32***", "RX");
            ui->textBrowserProcessed->append("***PARÁMETROS SINCRONIZADOS DESDE STM32***");
        }
        break;
    }
    case GETPIDBALANCE: {
        // 1. Extraer Error (arranca en datosRx[2])
        w.ui8[0] = datosRx[2]; w.ui8[1] = datosRx[3]; w.ui8[2] = datosRx[4]; w.ui8[3] = datosRx[5];
        int32_t current_error = w.i32;

        // 2. Extraer Integral
        w.ui8[0] = datosRx[6]; w.ui8[1] = datosRx[7]; w.ui8[2] = datosRx[8]; w.ui8[3] = datosRx[9];
        int32_t current_integral = w.i32;

        // 3. Extraer Derivada
        w.ui8[0] = datosRx[10]; w.ui8[1] = datosRx[11]; w.ui8[2] = datosRx[12]; w.ui8[3] = datosRx[13];
        int32_t current_derivative = w.i32;

        // 4. Extraer Output
        w.ui8[0] = datosRx[14]; w.ui8[1] = datosRx[15]; w.ui8[2] = datosRx[16]; w.ui8[3] = datosRx[17];
        int32_t current_output = w.i32;

        // 5. Extraer Angulo Real del STM32 (Nuevo)
        w.ui8[0] = datosRx[18]; w.ui8[1] = datosRx[19]; w.ui8[2] = datosRx[20]; w.ui8[3] = datosRx[21];
        float stm_angle = w.i32 / 100.0f; // Escala x100 -> real
        ui->angle_data->display(QString::number(stm_angle, 'f', 2));

        // 5b. Extraer Turn Offset del STM32 (Nuevo)
        w.ui8[0] = datosRx[22]; w.ui8[1] = datosRx[23]; w.ui8[2] = datosRx[24]; w.ui8[3] = datosRx[25];
        int32_t current_turn_offset = w.i32;
        ui->turn_offset_data->display(current_turn_offset);

        // 5c. Extraer Delta-time Medido en milisegundos (Nuevo)
        w.ui8[0] = datosRx[26]; w.ui8[1] = datosRx[27]; w.ui8[2] = datosRx[28]; w.ui8[3] = datosRx[29];
        int32_t current_dt_ms = w.i32;
        ui->dt_ms_data->display(current_dt_ms);

        // 6. Leer constantes actuales desde la Interfaz de Qt
        // (Como paramsSynced se encarga de poblarlas, siempre tendremos el valor real aquí)
        int16_t kp = ui->setBalanceKp->value();
        int16_t ki = ui->setBalanceKi->value();
        int16_t kd = ui->setBalanceKd->value();

        // 6. Calcular los Términos Individuales (Misma matemática que el micro)
        double term_P = (kp * current_error) / 1000.0;
        double term_I = (ki * current_integral) / 1000.0;
        double term_D = (kd * current_derivative) / 1000.0;
        double term_Out = current_output;

        // Calcular los Términos de Seguimiento de Línea
        int32_t sum = m_calIr1 + m_calIr3 + m_calIr5;
        if (sum == 0) sum = 1;
        int32_t error_linea = ((-(1000 * (int32_t)m_calIr5) + (1000 * (int32_t)m_calIr1)) / sum) / 10;
        int32_t abs_error = (error_linea > 0) ? error_linea : -error_linea;

        int16_t kp_line = ui->setLineKp->value();
        int16_t kq_line = ui->setLineKd->value();

        double term_P_line = kp_line * error_linea;
        double term_D_line = (kq_line * error_linea * abs_error) / 1000.0;
        double term_Out_line = current_turn_offset;

        // 7. Enviar a la gráfica embebida en MainWindow
        double t = runtimeTimer.elapsed() / 1000.0;
        updatePIDChart(t, term_P, term_I, term_D, term_Out, term_P_line, term_D_line, term_Out_line);

        // Opcional: Imprimir en consola para depurar
        // qDebug() << "P:" << term_P << "I:" << term_I << "D:" << term_D << "Out:" << term_Out;
        break;
    }
    case EXPORTIRCSV:
        if (datosRx[2] == ACK) {
            // Leer número de exportación devuelto por el STM32 (big-endian)
            m_irExportCount = ((int)datosRx[3] << 8) | datosRx[4];
            exportIrCsvToFile();
        }
        break;
    case SETPWML:
    case SETPWMR:
    case SETPWMMINR:
    case SETPWMMINL:
    case SET_KI_EXT:
    case SET_ALFA_LPF:
    case SETBALANCEKP:
    case SETBALANCEKD:
    case SETBALANCEKI:
    case SETSETPOINT:
    case SETLINEKP:
    case SETLINEKD:
    case SETOFFSETL:
    case SETOFFSETR:
    case SETCUSTOMTURN:
    case SETSPEED:
    case SETBKANG:
    case SETFRONTDIST:
    case SETSIDEDIST:
    case SETLOSTDIST:
    case SETSTOPCYCLES:
    case SETCORNERDIST:
    case SETALIGNDIST:
    case SETPWMLROT:
    case SETPWMRROT:
    case SETSTATICOFF:
    case SETMOVINGOFF:
    case SET_KP_EXT:
    case SETLIMITANG:
    case SETVELDAMPDIV:
    case SETVELDAMPLIM:
    case SETTURNLIMIT:
        if(datosRx[2]==ACK){
            str="COMANDO ACEPTADO Y GUARDADO (ACK)!!!";
            addLogEntry(str, "RX");
            ui->textBrowserProcessed->append(str);
        }
        break;

    default:
        str = str + "Comando DESCONOCIDO!!!!";
        addLogEntry(str, "UNKNOWN");
        ui->textBrowserProcessed->append(str);
    }
}

void MainWindow::sendSerial(uint8_t *buf, uint8_t length){
    uint8_t tx[24];
    uint8_t cks, i;
    QString strHex;
    _udat w;

    if(!QSerialPort1->isOpen())
        return;

    w.i32 = -1000;

    tx[7] = w.ui8[0];
    tx[8] = w.ui8[1];
    tx[9] = w.ui8[2];
    tx[10] = w.ui8[3];


    tx[0] = 'U';
    tx[1] = 'N';
    tx[2] = 'E';
    tx[3] = 'R';
    tx[4] = length + 1;
    tx[5] = ':';

    memcpy(&tx[6], buf, length);

    cks = 0;
    for (i=0; i<(length+6); i++) {
        cks ^= tx[i];
    }

    tx[i] = cks;

    strHex = "--> 0x";
    for (int i=0; i<length+7; i++) {
        strHex = strHex + QString("%1").arg(tx[i], 2, 16, QChar('0')).toUpper();
    }

    addLogEntry(strHex, "TX");
    ui->textBrowserUnProcessed->append(strHex);

    QSerialPort1->write((char *)tx, length+7);
}

void MainWindow::sendUdp(uint8_t *buf, uint8_t length){
    uint8_t tx[256];
    _udat w;
    unsigned char indice=0, cks=0;


    QString str;
    int puerto=0;
    // bool ok;

    if(!QUdpSocket1->isOpen())
        return;


    w.i32 = -1000;

    tx[7] = w.ui8[0];
    tx[8] = w.ui8[1];
    tx[9] = w.ui8[2];
    tx[10] = w.ui8[3];


    tx[0] = 'U';
    tx[1] = 'N';
    tx[2] = 'E';
    tx[3] = 'R';
    tx[4] = length + 1;
    tx[5] = ':';

    memcpy(&tx[6], buf, length);

    cks = 0;
    for (indice=0; indice<(length+6); indice++) {
        cks ^= tx[indice];
    }

    tx[indice] = cks;

    str = "--> 0x";
    for (int i=0; i<length+7; i++) {
        str = str + QString("%1").arg(tx[i], 2, 16, QChar('0')).toUpper();
    }

    puerto=ui->lineEdit_device_port->text().toInt();
    puertoremoto=puerto;

    if(clientAddress.isNull())
        clientAddress.setAddress(ui->lineEdit_device_ip->text());
    if(puertoremoto==0)
        puertoremoto=puerto;
    if(QUdpSocket1->isOpen()){
        //QUdpSocket1->writeDatagram(reinterpret_cast<const char *>(tx), (tx[4]+7), clientAddress, puertoremoto);
        QUdpSocket1->writeDatagram(reinterpret_cast<const char *>(tx), (tx[4]+6), clientAddress, puertoremoto);
    }

    for(int i=0; i<=indice; i++){
        if(isalnum(tx[i]))
            str = str + QString("%1").arg(char(tx[i]));
        else
            str = str +"{" + QString("%1").arg(tx[i],2,16,QChar('0')) + "}";
    }
    str=str + clientAddress.toString() + "  " +  QString().number(puertoremoto,10);

    addLogEntry("PC--UDP-->MBED ( " + str + " )", "TX");
    ui->textBrowserUnProcessed->append("PC--UDP-->MBED ( " + str + " )");
}

void MainWindow::sendCommand(uint8_t *buf, uint8_t length) {
    bool sent = false;

    if (QSerialPort1->isOpen()) {
        sendSerial(buf, length);
        sent = true;
        m_countSent++;
    } else if (QUdpSocket1->isOpen()) {
        sendUdp(buf, length);
        sent = true;
        m_countSent++;
    }

    if (!sent) {
        addLogEntry("***ERROR: NINGUNA CONEXIÓN ABIERTA***", "CHK_ERROR");
        ui->textBrowserProcessed->append("***ERROR: NINGUNA CONEXIÓN ABIERTA***");
    }
}



void MainWindow::timeOut(){
    if(rxData.timeOut){
        rxData.timeOut--;
        if(!rxData.timeOut){
            estadoProtocolo=START;
        }
    }
    //getData();
}

void MainWindow::OnUdpRxData(){
    qint64          count = 0;
    unsigned char   *incomingBuffer = NULL;

    // EL BUCLE PROCESA CADA DATAGRAMA PENDIENTE EN LA COLA
    while(QUdpSocket1->hasPendingDatagrams()){
        count = QUdpSocket1->pendingDatagramSize();
        if (count <= 0) continue;

        incomingBuffer = new unsigned char[count];
        QUdpSocket1->readDatagram(reinterpret_cast<char *>(incomingBuffer), count, &RemoteAddress, &RemotePort);

        // Debug visual de los datos crudos en la interfaz
        QString str = "";
        for(int i = 0; i < count; i++){
            if(isalnum(incomingBuffer[i]))
                str = str + QString("%1").arg(char(incomingBuffer[i]));
            else
                str = str + "{" + QString("%1").arg(incomingBuffer[i], 2, 16, QChar('0')) + "}";
        }
        addLogEntry("MBED-->UDP-->PC (" + str + ")", "RX");
        ui->textBrowserUnProcessed->append("MBED-->UDP-->PC (" + str + ")");
        addLogEntry(" adr " + RemoteAddress.toString(), "RX");
        ui->textBrowserUnProcessed->append(" adr " + RemoteAddress.toString());

        // Actualizar la IP y el puerto detectado del robot
        ui->lineEdit_device_ip->setText(RemoteAddress.toString().right((RemoteAddress.toString().length())-7));
        ui->lineEdit_device_port->setText(QString().number(RemotePort, 10));

        // --- RESET de la MeF al inicio de cada datagrama ---
        // Cada AT+CIPSEND del ESP01 genera exactamente un datagrama UDP completo.
        // Empezar en START por cada datagrama evita que un paquete corrupto
        // arrastre estado residual al siguiente datagrama válido.
        estadoProtocoloUdp = START;

        // MÁQUINA DE ESTADOS DEL PROTOCOLO UNER
        for(int i = 0; i < count; i++){
            switch (estadoProtocoloUdp) {
            case START:
                if (incomingBuffer[i] == 'U'){
                    estadoProtocoloUdp = HEADER_1;
                    rxDataUdp.cheksum = 0;
                }
                break;
            case HEADER_1:
                if (incomingBuffer[i] == 'N')
                    estadoProtocoloUdp = HEADER_2;
                else {
                    i--;
                    estadoProtocoloUdp = START;
                }
                break;
            case HEADER_2:
                if (incomingBuffer[i] == 'E')
                    estadoProtocoloUdp = HEADER_3;
                else {
                    i--;
                    estadoProtocoloUdp = START;
                }
                break;
            case HEADER_3:
                if (incomingBuffer[i] == 'R')
                    estadoProtocoloUdp = NBYTES;
                else {
                    i--;
                    estadoProtocoloUdp = START;
                }
                break;
            case NBYTES:
                rxDataUdp.nBytes = incomingBuffer[i];
                estadoProtocoloUdp = TOKEN;
                break;
            case TOKEN:
                if (incomingBuffer[i] == ':'){
                    estadoProtocoloUdp = PAYLOAD;
                    rxDataUdp.cheksum = 'U' ^ 'N' ^ 'E' ^ 'R' ^ rxDataUdp.nBytes ^ ':';
                    rxDataUdp.payLoad[0] = rxDataUdp.nBytes;
                    rxDataUdp.index = 1;
                }
                else {
                    i--;
                    estadoProtocoloUdp = START;
                }
                break;
            case PAYLOAD:
                if (rxDataUdp.nBytes > 1){
                    rxDataUdp.payLoad[rxDataUdp.index++] = incomingBuffer[i];
                    rxDataUdp.cheksum ^= incomingBuffer[i];
                }
                rxDataUdp.nBytes--;
                if(rxDataUdp.nBytes == 0){
                    estadoProtocoloUdp = START;

                    // Verificación de Checksum
                    if(rxDataUdp.cheksum == incomingBuffer[i]){
                        decodeData(&rxDataUdp.payLoad[0], UDP);
                    } else {
                        addLogEntry(" CHK DISTINTO!!!!! ", "CHK_ERROR");
                        ui->textBrowserProcessed->append(" CHK DISTINTO!!!!! ");
                        // --- LIBERAR SEMÁFORO en error de CHK ---
                        // Sin esto, getData() queda bloqueado ~600ms esperando
                        // el timeoutPatience, paralizando toda la telemetría.
                        isWaitingReply = false;
                    }
                }
                break;

            default:
                estadoProtocoloUdp = START;
                break;
            }
        }

        // IMPORTANTE: Liberar la memoria del buffer actual antes de leer el siguiente paquete
        delete [] incomingBuffer;
    }
}

void MainWindow::getData(){
    // --- 1. GUARDIA DE SEGURIDAD (Frena el spam) ---
    if(!QSerialPort1->isOpen() && !QUdpSocket1->isOpen()) {
        statusMode->setText("CURRENT STATE --> DESCONECTADO");
        return;
    }

    // --- SISTEMA DE ADAPTACIÓN A REDES LENTAS (PING-PONG) ---
    if (isWaitingReply) {
        timeoutPatience++;
        // Si pasaron ~240ms (8 ciclos de 30ms) sin respuesta,
        // asumimos que el paquete se perdió y forzamos un reintento.
        if (timeoutPatience >= 8) {
            isWaitingReply = false;
        } else {
            return; // Seguimos esperando. ¡NO enviamos nada a la ESP-01!
        }
    }

    // Levantamos el semáforo: acabamos de preguntar, ahora esperamos.
    isWaitingReply = true;
    timeoutPatience = 0;

    // --- 2. ACTUALIZAR ESTADO VISUAL ---
    if(QSerialPort1->isOpen())
        statusMode->setText("CURRENT STATE --> CONECTADO SERIE");
    else if (QUdpSocket1->isOpen())
        statusMode->setText("CURRENT STATE --> CONECTADO UDP");

    // --- 3. MÁQUINA DE ESTADOS ---
    static uint8_t commMef = 1;
    uint8_t buf[1];

    switch (commMef){
    case 1: buf[0] = GETMPU; break;
    case 2: buf[0] = GETADC; break;
    case 3: buf[0] = GETMPU; break;
    case 4: buf[0] = GETADC; break;
    case 5: buf[0] = GETPIDBALANCE; break;
    case 6: buf[0] = GETINTERNALDATA; break;
    }

    commMef++;
    if (commMef > 6) {
        commMef = 1;
    }

    sendCommand(buf, 1);
}
bool MainWindow::eventFilter(QObject *watched, QEvent *event){ //utilizado para mostrar los puestos disponibles
    if(watched == ui->comboBox_PORT) {
        if (event->type() == QEvent::MouseButtonPress) {
            ui->comboBox_PORT->clear();
            QSerialPortInfo SerialPortInfo1;

            for(int i=0;i<SerialPortInfo1.availablePorts().count();i++)
                ui->comboBox_PORT->addItem(SerialPortInfo1.availablePorts().at(i).portName());

            return QMainWindow::eventFilter(watched, event);
        }
        else {
            return false;
        }
    }
    else{
        return QMainWindow::eventFilter(watched, event);
    }
}

void MainWindow::on_pushButton_connectSerial_clicked()
{
    if(QSerialPort1->isOpen()){
        QSerialPort1->close();
        ui->pushButton_connectSerial->setText("CONNECT");
        resetInterface();
    }
    else{

        if(ui->comboBox_PORT->currentText() == "")
            return;

        QSerialPort1->setPortName(ui->comboBox_PORT->currentText());
        QSerialPort1->setBaudRate(115200);
        QSerialPort1->setParity(QSerialPort::NoParity);
        QSerialPort1->setDataBits(QSerialPort::Data8);
        QSerialPort1->setStopBits(QSerialPort::OneStop);
        QSerialPort1->setFlowControl(QSerialPort::NoFlowControl);

        if(QSerialPort1->open(QSerialPort::ReadWrite)){
            ui->pushButton_connectSerial->setText("DISCONNECT");
            paramsSynced = false;
            uint8_t b = GETINTERNALDATA;
            sendSerial(&b, 1);
        }
        else
            QMessageBox::information(this, "Serial PORT", "ERROR. Opening PORT");
    }
}

void MainWindow::on_pushButton_connectUdp_clicked()
{
    int Port;
    bool ok;

    if(QUdpSocket1->isOpen()){
        QUdpSocket1->close();
        ui->pushButton_connectUdp->setText("CONNECT");
        resetInterface();
        return;
    }

    Port=ui->lineEdit_local_port->text().toInt(&ok,10);
    if(!ok || Port<=0 || Port>65535){
        QMessageBox::information(this, tr("SERVER PORT"),tr("ERRRO. Number PORT."));
        return;
    }

    try{
        QUdpSocket1->abort();
        QUdpSocket1->bind(Port);
        QUdpSocket1->open(QUdpSocket::ReadWrite);
    }catch(...){
        QMessageBox::information(this, tr("SERVER PORT"),tr("Can't OPEN Port."));
        return;
    }

    ui->pushButton_connectUdp->setText("DISCONNECT");
    paramsSynced = false;
    uint8_t b = GETINTERNALDATA;
    sendUdp(&b, 1);
    if(QUdpSocket1->isOpen()){
        if(clientAddress.isNull())
            clientAddress.setAddress(ui->lineEdit_device_ip->text());
        if(puertoremoto==0)
            puertoremoto=ui->lineEdit_device_port->text().toInt();
        QUdpSocket1->writeDatagram("r", 1, clientAddress, puertoremoto);
    }
}

void MainWindow::on_sendBalanceKp_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETBALANCEKP;
    w.i32 = ui->setBalanceKp->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***KP BALANCÍN ACTUALIZADO***");
}

void MainWindow::on_sendBalanceKi_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETBALANCEKI;
    w.i32 = ui->setBalanceKi->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***KI BALANCÍN ACTUALIZADO***");
}

void MainWindow::on_sendBalanceKd_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETBALANCEKD;
    w.i32 = ui->setBalanceKd->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***KD BALANCÍN ACTUALIZADO***");
}

void MainWindow::on_sendLineKp_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETLINEKP;
    w.i32 = ui->setLineKp->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***KP LÍNEA ACTUALIZADO***");
}

void MainWindow::on_sendLineKd_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETLINEKD;
    w.i32 = ui->setLineKd->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***KD LÍNEA ACTUALIZADO***");
}

void MainWindow::on_sendPWMMINL_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETPWMMINL; // 0xA7
    w.i32 = ui->setPWMMINL->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***PWM MIN L ACTUALIZADO***");
}

void MainWindow::on_sendPWMMINR_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETPWMMINR; // 0xA6
    w.i32 = ui->setPWMMINR->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***PWM MIN R ACTUALIZADO***");
}

void MainWindow::on_sendSetpoint_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETSETPOINT; // 0xAB
    w.i32 = ui->setSetpoint->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1]; // Enviamos 2 bytes
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***SETPOINT ACTUALIZADO***");
}

void MainWindow::on_sendAttackSetpoint_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETSPEED; // 0xB1
    w.i32 = ui->setAttackSetpoint->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1]; // Enviamos 2 bytes
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***ATTACK SETPOINT ACTUALIZADO***");
}

void MainWindow::on_setAttackSetpoint_valueChanged(int arg1) {
    (void)arg1;
    on_sendAttackSetpoint_clicked();
}

void MainWindow::on_sendPWML_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETPWML;
    w.i32 = ui->setPWML->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***PWM L ACTUALIZADO***");
}

void MainWindow::on_sendPWMR_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETPWMR;
    w.i32 = ui->setPWMR->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***PWM R ACTUALIZADO***");
}

void MainWindow::on_sendOFFSETL_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETOFFSETL;
    w.i32 = ui->setOFFSETL->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***OFFSET L ACTUALIZADO***");
}

void MainWindow::on_sendOFFSETR_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETOFFSETR;
    w.i32 = ui->setOFFSETR->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***OFFSET R ACTUALIZADO***");
}

void MainWindow::on_sendCustomTurn_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETCUSTOMTURN;
    w.i32 = ui->setCustomTurn->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***CUSTOM TURN ACTUALIZADO***");
}

void MainWindow::on_sendKpCascada_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SET_KP_EXT;
    w.i16[0] = ui->setKpCascada->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***KP CASCADA ACTUALIZADO***");
}



void MainWindow::on_P1toP3_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
}


void MainWindow::on_P1toP2_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}



void MainWindow::on_P2toP1_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}


void MainWindow::on_P3toP1_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}


void MainWindow::on_P3toP2_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}


void MainWindow::on_P2toP3_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
}

void MainWindow::on_P1toP4_clicked()
{
    ui->stackedWidget->setCurrentIndex(3);
}

void MainWindow::on_P4toP1_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

// -----------------------------------------------------------------------
// Chart PID embebido en MainWindow (widget PIDchart promovido a QChartView)
// -----------------------------------------------------------------------
void MainWindow::initPIDChart()
{
    pid_pSeries   = new QLineSeries(); pid_pSeries->setName("P Balance");
    pid_iSeries   = new QLineSeries(); pid_iSeries->setName("I Balance");
    pid_dSeries   = new QLineSeries(); pid_dSeries->setName("D Balance");
    pid_outSeries = new QLineSeries(); pid_outSeries->setName("Output Balance");

    pid_pLineSeries   = new QLineSeries(); pid_pLineSeries->setName("P Seguimiento");
    pid_dLineSeries   = new QLineSeries(); pid_dLineSeries->setName("D Seguimiento");
    pid_outLineSeries = new QLineSeries(); pid_outLineSeries->setName("Giro Seguimiento");

    // MPU series
    pid_axSeries = new QLineSeries(); pid_axSeries->setName("Ax");
    pid_aySeries = new QLineSeries(); pid_aySeries->setName("Ay");
    pid_azSeries = new QLineSeries(); pid_azSeries->setName("Az");
    pid_gxSeries = new QLineSeries(); pid_gxSeries->setName("Gx");
    pid_gySeries = new QLineSeries(); pid_gySeries->setName("Gy");
    pid_gzSeries = new QLineSeries(); pid_gzSeries->setName("Gz");

    // Angles series
    pid_pitchSeries = new QLineSeries(); pid_pitchSeries->setName("Pitch");
    pid_rollSeries  = new QLineSeries(); pid_rollSeries->setName("Roll");
    pid_yawSeries   = new QLineSeries(); pid_yawSeries->setName("Yaw");

    // IR series
    pid_ir1Series = new QLineSeries(); pid_ir1Series->setName("IR1");
    pid_ir2Series = new QLineSeries(); pid_ir2Series->setName("IR2");
    pid_ir3Series = new QLineSeries(); pid_ir3Series->setName("IR3");
    pid_ir4Series = new QLineSeries(); pid_ir4Series->setName("IR4");
    pid_ir5Series = new QLineSeries(); pid_ir5Series->setName("IR5");
    pid_ir6Series = new QLineSeries(); pid_ir6Series->setName("IR6");
    pid_ir7Series = new QLineSeries(); pid_ir7Series->setName("IR7");
    pid_ir8Series = new QLineSeries(); pid_ir8Series->setName("IR8");

    chartPID_mw = new QChart();
    chartPID_mw->addSeries(pid_pSeries);
    chartPID_mw->addSeries(pid_iSeries);
    chartPID_mw->addSeries(pid_dSeries);
    chartPID_mw->addSeries(pid_outSeries);
    chartPID_mw->addSeries(pid_pLineSeries);
    chartPID_mw->addSeries(pid_dLineSeries);
    chartPID_mw->addSeries(pid_outLineSeries);

    chartPID_mw->addSeries(pid_axSeries);
    chartPID_mw->addSeries(pid_aySeries);
    chartPID_mw->addSeries(pid_azSeries);
    chartPID_mw->addSeries(pid_gxSeries);
    chartPID_mw->addSeries(pid_gySeries);
    chartPID_mw->addSeries(pid_gzSeries);

    chartPID_mw->addSeries(pid_pitchSeries);
    chartPID_mw->addSeries(pid_rollSeries);
    chartPID_mw->addSeries(pid_yawSeries);

    chartPID_mw->addSeries(pid_ir1Series);
    chartPID_mw->addSeries(pid_ir2Series);
    chartPID_mw->addSeries(pid_ir3Series);
    chartPID_mw->addSeries(pid_ir4Series);
    chartPID_mw->addSeries(pid_ir5Series);
    chartPID_mw->addSeries(pid_ir6Series);
    chartPID_mw->addSeries(pid_ir7Series);
    chartPID_mw->addSeries(pid_ir8Series);

    chartPID_mw->setTitle("Curvas de telemetría");
    chartPID_mw->layout()->setContentsMargins(0, 0, 0, 0);
    chartPID_mw->setBackgroundRoundness(0);

    pid_axisX = new QValueAxis();
    pid_axisX->setRange(0, 10);
    pid_axisX->setTitleText("Tiempo (s)");

    pid_axisY = new QValueAxis();
    pid_axisY->setRange(pid_yMin, pid_yMax);
    pid_axisY->setTitleText("Valor");

    chartPID_mw->addAxis(pid_axisX, Qt::AlignBottom);
    chartPID_mw->addAxis(pid_axisY, Qt::AlignLeft);

    pid_pSeries->attachAxis(pid_axisX);   pid_pSeries->attachAxis(pid_axisY);
    pid_iSeries->attachAxis(pid_axisX);   pid_iSeries->attachAxis(pid_axisY);
    pid_dSeries->attachAxis(pid_axisX);   pid_dSeries->attachAxis(pid_axisY);
    pid_outSeries->attachAxis(pid_axisX); pid_outSeries->attachAxis(pid_axisY);

    pid_pLineSeries->attachAxis(pid_axisX);   pid_pLineSeries->attachAxis(pid_axisY);
    pid_dLineSeries->attachAxis(pid_axisX);   pid_dLineSeries->attachAxis(pid_axisY);
    pid_outLineSeries->attachAxis(pid_axisX); pid_outLineSeries->attachAxis(pid_axisY);

    pid_axSeries->attachAxis(pid_axisX);   pid_axSeries->attachAxis(pid_axisY);
    pid_aySeries->attachAxis(pid_axisX);   pid_aySeries->attachAxis(pid_axisY);
    pid_azSeries->attachAxis(pid_axisX);   pid_azSeries->attachAxis(pid_axisY);
    pid_gxSeries->attachAxis(pid_axisX);   pid_gxSeries->attachAxis(pid_axisY);
    pid_gySeries->attachAxis(pid_axisX);   pid_gySeries->attachAxis(pid_axisY);
    pid_gzSeries->attachAxis(pid_axisX);   pid_gzSeries->attachAxis(pid_axisY);

    pid_pitchSeries->attachAxis(pid_axisX);   pid_pitchSeries->attachAxis(pid_axisY);
    pid_rollSeries->attachAxis(pid_axisX);    pid_rollSeries->attachAxis(pid_axisY);
    pid_yawSeries->attachAxis(pid_axisX);     pid_yawSeries->attachAxis(pid_axisY);

    pid_ir1Series->attachAxis(pid_axisX);   pid_ir1Series->attachAxis(pid_axisY);
    pid_ir2Series->attachAxis(pid_axisX);   pid_ir2Series->attachAxis(pid_axisY);
    pid_ir3Series->attachAxis(pid_axisX);   pid_ir3Series->attachAxis(pid_axisY);
    pid_ir4Series->attachAxis(pid_axisX);   pid_ir4Series->attachAxis(pid_axisY);
    pid_ir5Series->attachAxis(pid_axisX);   pid_ir5Series->attachAxis(pid_axisY);
    pid_ir6Series->attachAxis(pid_axisX);   pid_ir6Series->attachAxis(pid_axisY);
    pid_ir7Series->attachAxis(pid_axisX);   pid_ir7Series->attachAxis(pid_axisY);
    pid_ir8Series->attachAxis(pid_axisX);   pid_ir8Series->attachAxis(pid_axisY);

    ui->PIDchart->setChart(chartPID_mw);
    ui->PIDchart->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::updatePIDChart(double time, double p, double i, double d, double out, double pLine, double dLine, double outLine)
{
    pid_pSeries->append(time, p);
    pid_iSeries->append(time, i);
    pid_dSeries->append(time, d);
    pid_outSeries->append(time, out);

    pid_pLineSeries->append(time, pLine);
    pid_dLineSeries->append(time, dLine);
    pid_outLineSeries->append(time, outLine);

    // Limitar la cantidad de puntos para no saturar memoria
    int maxPoints = 1000;
    while (pid_pSeries->count() > maxPoints) pid_pSeries->remove(0);
    while (pid_iSeries->count() > maxPoints) pid_iSeries->remove(0);
    while (pid_dSeries->count() > maxPoints) pid_dSeries->remove(0);
    while (pid_outSeries->count() > maxPoints) pid_outSeries->remove(0);
    while (pid_pLineSeries->count() > maxPoints) pid_pLineSeries->remove(0);
    while (pid_dLineSeries->count() > maxPoints) pid_dLineSeries->remove(0);
    while (pid_outLineSeries->count() > maxPoints) pid_outLineSeries->remove(0);

    // Scroll del eje X (ventana de 10 segundos)
    if (time > 10.0) {
        pid_axisX->setRange(time - 10.0, time);
    } else {
        pid_axisX->setRange(0, 10.0);
    }

    // Actualizar visibilidad y rango dinámico adaptativo
    updatePIDChartRange();
}

void MainWindow::updateMPUChart(double time, double ax, double ay, double az, double gx, double gy, double gz, double pitch, double roll, double yaw)
{
    pid_axSeries->append(time, ax);
    pid_aySeries->append(time, ay);
    pid_azSeries->append(time, az);
    pid_gxSeries->append(time, gx);
    pid_gySeries->append(time, gy);
    pid_gzSeries->append(time, gz);
    pid_pitchSeries->append(time, pitch);
    pid_rollSeries->append(time, roll);
    pid_yawSeries->append(time, yaw);

    // Limitar puntos
    int maxPoints = 1000;
    while (pid_axSeries->count() > maxPoints) pid_axSeries->remove(0);
    while (pid_aySeries->count() > maxPoints) pid_aySeries->remove(0);
    while (pid_azSeries->count() > maxPoints) pid_azSeries->remove(0);
    while (pid_gxSeries->count() > maxPoints) pid_gxSeries->remove(0);
    while (pid_gySeries->count() > maxPoints) pid_gySeries->remove(0);
    while (pid_gzSeries->count() > maxPoints) pid_gzSeries->remove(0);
    while (pid_pitchSeries->count() > maxPoints) pid_pitchSeries->remove(0);
    while (pid_rollSeries->count() > maxPoints) pid_rollSeries->remove(0);
    while (pid_yawSeries->count() > maxPoints) pid_yawSeries->remove(0);

    // Scroll del eje X
    if (time > 10.0) {
        pid_axisX->setRange(time - 10.0, time);
    } else {
        pid_axisX->setRange(0, 10.0);
    }

    updatePIDChartRange();
}

void MainWindow::updateIRChart(double time, double ir1, double ir2, double ir3, double ir4, double ir5, double ir6, double ir7, double ir8)
{
    pid_ir1Series->append(time, ir1);
    pid_ir2Series->append(time, ir2);
    pid_ir3Series->append(time, ir3);
    pid_ir4Series->append(time, ir4);
    pid_ir5Series->append(time, ir5);
    pid_ir6Series->append(time, ir6);
    pid_ir7Series->append(time, ir7);
    pid_ir8Series->append(time, ir8);

    // Limitar puntos
    int maxPoints = 1000;
    while (pid_ir1Series->count() > maxPoints) pid_ir1Series->remove(0);
    while (pid_ir2Series->count() > maxPoints) pid_ir2Series->remove(0);
    while (pid_ir3Series->count() > maxPoints) pid_ir3Series->remove(0);
    while (pid_ir4Series->count() > maxPoints) pid_ir4Series->remove(0);
    while (pid_ir5Series->count() > maxPoints) pid_ir5Series->remove(0);
    while (pid_ir6Series->count() > maxPoints) pid_ir6Series->remove(0);
    while (pid_ir7Series->count() > maxPoints) pid_ir7Series->remove(0);
    while (pid_ir8Series->count() > maxPoints) pid_ir8Series->remove(0);

    // Scroll del eje X
    if (time > 10.0) {
        pid_axisX->setRange(time - 10.0, time);
    } else {
        pid_axisX->setRange(0, 10.0);
    }

    updatePIDChartRange();
}

void MainWindow::updatePIDChartRange()
{
    double time = pid_axisX->max();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();
    bool hasPoints = false;

    struct SeriesInfo {
        QLineSeries* series;
        bool visible;
    };
    QList<SeriesInfo> seriesList = {
        {pid_pSeries, ui->checkBox_P->isChecked()},
        {pid_iSeries, ui->checkBox_I->isChecked()},
        {pid_dSeries, ui->checkBox_D->isChecked()},
        {pid_outSeries, ui->checkBox_Out->isChecked()},
        {pid_pLineSeries, ui->checkBox_P_line->isChecked()},
        {pid_dLineSeries, ui->checkBox_D_line->isChecked()},
        {pid_outLineSeries, ui->checkBox_Out_line->isChecked()},

        {pid_axSeries, ui->checkBox_Ax->isChecked()},
        {pid_aySeries, ui->checkBox_Ay->isChecked()},
        {pid_azSeries, ui->checkBox_Az->isChecked()},
        {pid_gxSeries, ui->checkBox_Gx->isChecked()},
        {pid_gySeries, ui->checkBox_Gy->isChecked()},
        {pid_gzSeries, ui->checkBox_Gz->isChecked()},

        {pid_pitchSeries, ui->checkBox_Pitch->isChecked()},
        {pid_rollSeries, ui->checkBox_Roll->isChecked()},
        {pid_yawSeries, ui->checkBox_Yaw->isChecked()},

        {pid_ir1Series, ui->checkBox_Ir1->isChecked()},
        {pid_ir2Series, ui->checkBox_Ir2->isChecked()},
        {pid_ir3Series, ui->checkBox_Ir3->isChecked()},
        {pid_ir4Series, ui->checkBox_Ir4->isChecked()},
        {pid_ir5Series, ui->checkBox_Ir5->isChecked()},
        {pid_ir6Series, ui->checkBox_Ir6->isChecked()},
        {pid_ir7Series, ui->checkBox_Ir7->isChecked()},
        {pid_ir8Series, ui->checkBox_Ir8->isChecked()}
    };

    double xMin = qMax(0.0, time - 10.0);
    double xMax = time;

    for (const auto& info : seriesList) {
        if (info.series) {
            info.series->setVisible(info.visible);
            if (info.visible) {
                const QList<QPointF> points = info.series->points();
                for (const QPointF& pt : points) {
                    if (pt.x() >= xMin && pt.x() <= xMax) {
                        if (pt.y() < yMin) yMin = pt.y();
                        if (pt.y() > yMax) yMax = pt.y();
                        hasPoints = true;
                    }
                }
            }
        }
    }

    if (hasPoints) {
        double margin = (yMax - yMin) * 0.1;
        if (margin < 1.0) margin = 5.0;
        pid_axisY->setRange(yMin - margin, yMax + margin);
        pid_yMin = yMin;
        pid_yMax = yMax;
    } else {
        pid_axisY->setRange(-10.0, 10.0);
        pid_yMin = -10.0;
        pid_yMax = 10.0;
    }
}

void MainWindow::resetInterface() {
    paramsSynced = false;
    
    // Resetear QSpinBoxes
    QList<QSpinBox *> spinBoxes = this->findChildren<QSpinBox *>();
    for (QSpinBox *spinBox : spinBoxes) {
        bool oldState = spinBox->blockSignals(true);
        spinBox->setValue(0);
        spinBox->blockSignals(oldState);
    }

    // Resetear QLCDNumbers (Sensores IR, Acelerómetro, Giroscopio, etc.)
    QList<QLCDNumber *> lcdNumbers = this->findChildren<QLCDNumber *>();
    for (QLCDNumber *lcd : lcdNumbers) {
        lcd->display(0);
    }
}

void MainWindow::on_sendFrontDistance_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setFrontDistance->value();
    buf[0] = SETFRONTDIST;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendSideDistance_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setSideDistance->value();
    buf[0] = SETSIDEDIST;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendLostDistance_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setLostDistance->value();
    buf[0] = SETLOSTDIST;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendStopCycles_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setStopCycles->value();
    buf[0] = SETSTOPCYCLES;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendCornerDist_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setCornerDist->value();
    buf[0] = SETCORNERDIST;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendAlignDist_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setAlignDist->value();
    buf[0] = SETALIGNDIST;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendPWMLROT_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setPWMLROT->value();
    buf[0] = SETPWMLROT;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendPWMRROT_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setPWMRROT->value();
    buf[0] = SETPWMRROT;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendStaticOff_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setStaticOff->value();
    buf[0] = SETSTATICOFF;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendMovingOff_clicked() {
    uint8_t buf[3];
    _udat w;
    w.i16[0] = ui->setMovingOff->value();
    buf[0] = SETMOVINGOFF;
    buf[1] = w.ui8[0];
    buf[2] = w.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendLimitAngle_clicked() {
    uint8_t buf[3];
    buf[0] = SETLIMITANG;
    myWord.i16[0] = ui->setLimitAngle->value();
    buf[1] = myWord.ui8[0];
    buf[2] = myWord.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendKiCascada_clicked() {
    uint8_t buf[3];
    buf[0] = SET_KI_EXT;
    myWord.i16[0] = ui->setKiCascada->value();
    buf[1] = myWord.ui8[0];
    buf[2] = myWord.ui8[1];
    sendCommand(buf, 3);
    ui->textBrowserProcessed->append("***KI CASCADA ACTUALIZADO***");
}

void MainWindow::on_sendFilterLPF_clicked() {
    uint8_t buf[3];
    buf[0] = SET_ALFA_LPF;
    myWord.i16[0] = ui->setFilterLPF->value();
    buf[1] = myWord.ui8[0];
    buf[2] = myWord.ui8[1];
    sendCommand(buf, 3);
    ui->textBrowserProcessed->append("***FILTRO LPF CASCADA ACTUALIZADO***");
}

void MainWindow::on_sendVelDampDiv_clicked() {
    uint8_t buf[3];
    buf[0] = SETVELDAMPDIV;
    myWord.i16[0] = ui->setVelDampDiv->value();
    buf[1] = myWord.ui8[0];
    buf[2] = myWord.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendVelDampLim_clicked() {
    uint8_t buf[3];
    buf[0] = SETVELDAMPLIM;
    myWord.i16[0] = ui->setVelDampLim->value();
    buf[1] = myWord.ui8[0];
    buf[2] = myWord.ui8[1];
    sendCommand(buf, 3);
}

void MainWindow::on_sendTurnLimit_clicked() {
    uint8_t buf[3];
    buf[0] = SETTURNLIMIT;
    myWord.i16[0] = ui->setTurnLimit->value();
    buf[1] = myWord.ui8[0];
    buf[2] = myWord.ui8[1];
    sendCommand(buf, 3);
    ui->textBrowserProcessed->append("***TURN LIMIT ACTUALIZADO***");
}

// ---------------------------------------------------------------------------
// Exportar CSV de Sensores IR (último minuto)
// ---------------------------------------------------------------------------
void MainWindow::exportIrCsvToFile() {
    bool lowerOk = false;
    QString fileNameLower;
    if (!m_irBuffer.isEmpty()) {
        QString defaultNameLower = "ir_sensors_lower_" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
        fileNameLower = QFileDialog::getSaveFileName(
            this, "Exportar Sensores IR Inferiores - CSV", defaultNameLower,
            "Archivos CSV (*.csv)");

        if (!fileNameLower.isEmpty()) {
            QFile file(fileNameLower);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "ExportNum;" << m_irExportCount << "\n";
                out << "Timestamp;IR1 (Der-Raw);IR3 (Cen-Raw);IR5 (Izq-Raw);Promedio\n";
                for (const IrSample &s : m_irBuffer) {
                    uint32_t avg = ((uint32_t)s.ir1 + s.ir3 + s.ir5) / 3;
                    out << s.timestamp.toString("hh:mm:ss.zzz") << ";"
                        << s.ir1 << ";"
                        << s.ir3 << ";"
                        << s.ir5 << ";"
                        << avg   << "\n";
                }
                file.close();
                lowerOk = true;
            } else {
                QMessageBox::critical(this, "Error", "No se pudo crear el archivo CSV de sensores inferiores.");
            }
        }
    }

    bool upperOk = false;
    QString fileNameUpper;
    if (!m_upperIrBuffer.isEmpty()) {
        QString defaultNameUpper = "ir_sensors_upper_" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
        fileNameUpper = QFileDialog::getSaveFileName(
            this, "Exportar Sensores IR Superiores (Esquivar Objeto) - CSV", defaultNameUpper,
            "Archivos CSV (*.csv)");

        if (!fileNameUpper.isEmpty()) {
            QFile file(fileNameUpper);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "ExportNum;" << m_irExportCount << "\n";
                out << "Timestamp;IR1;IR3;IR5;IR7;IR8;Promedio\n";
                for (const UpperIrSample &s : m_upperIrBuffer) {
                    uint32_t avg = ((uint32_t)s.ir1 + s.ir3 + s.ir5 + s.ir7 + s.ir8) / 5;
                    out << s.timestamp.toString("hh:mm:ss.zzz") << ";"
                        << s.ir1 << ";"
                        << s.ir3 << ";"
                        << s.ir5 << ";"
                        << s.ir7 << ";"
                        << s.ir8 << ";"
                        << avg   << "\n";
                }
                file.close();
                upperOk = true;
            } else {
                QMessageBox::critical(this, "Error", "No se pudo crear el archivo CSV de sensores superiores.");
            }
        }
    }

    if (lowerOk || upperOk) {
        QString msg = QString("Exportación N° %1 exitosa.\n").arg(m_irExportCount);
        if (lowerOk) msg += QString("- %1 muestras inferiores guardadas en:\n  %2\n").arg(m_irBuffer.size()).arg(fileNameLower);
        if (upperOk) msg += QString("- %1 muestras superiores guardadas en:\n  %2\n").arg(m_upperIrBuffer.size()).arg(fileNameUpper);
        QMessageBox::information(this, "Exportación IR exitosa", msg);
    }
}

void MainWindow::on_pushButton_exportIrCsv_clicked() {
    if (m_irBuffer.isEmpty() && m_upperIrBuffer.isEmpty()) {
        QMessageBox::warning(this, "Sin datos",
            "El buffer está vacío. Conectate al robot y esperá que lleguen datos de telemetría.");
        return;
    }
    // Enviar comando EXPORTIRCSV al STM32 para obtener el número de exportación
    uint8_t cmd[2] = { (uint8_t)EXPORTIRCSV, 0x00 };
    sendCommand(cmd, 2);
}
