#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "internal_data.h"
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

        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gx: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->gx_data->display(str);

        w.i8[0] = datosRx[10];
        w.i8[1] = datosRx[11];

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

        // ---- NUEVO: Enviar a la gráfica ----
        // Obtenemos el tiempo en segundos
        //double t = runtimeTimer.elapsed() / 1000.0;

        // (Por ahora ponemos el setpoint manual, luego puedes leerlo del STM32)
        //double currentSetpoint = 0.5;
        //double currentPwm = 0.0;      // Necesitarás enviarlo desde el STM32

        // (Opcional) Calcular Yaw integrando el giroscopio
        float gz_grados_seg = gz / 131.0f; // Asumiendo escala de +/- 250deg/s
        if (abs(gz_grados_seg) > 1.0f) {
            yawAcumulado += gz_grados_seg * 0.5f; // asumiendo 100ms de muestreo de tu timer1
        }
        // Añade esta línea para imprimir los ángulos finales en la consola de Qt Creator
        qDebug() << "Angulos Calculados -> Pitch:" << pitch << " | Roll:" << roll << " | Yaw:" << yawAcumulado;

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
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR1: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir1_data->display(str);

        w.ui8[0] = datosRx[4];
        w.ui8[1] = datosRx[5];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR2: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir2_data->display(str);
        sumLineSensors += qMax(0, 2400 - w.i16[0]);

        w.ui8[0] = datosRx[6];
        w.ui8[1] = datosRx[7];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR3: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir3_data->display(str);

        w.ui8[0] = datosRx[8];
        w.ui8[1] = datosRx[9];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR4: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir4_data->display(str);
        sumLineSensors += qMax(0, 2400 - w.i16[0]);

        w.ui8[0] = datosRx[10];
        w.ui8[1] = datosRx[11];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR5: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir5_data->display(str);

        w.ui8[0] = datosRx[12];
        w.ui8[1] = datosRx[13];
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
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR7: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir7_data->display(str);

        w.ui8[0] = datosRx[16];
        w.ui8[1] = datosRx[17];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR8: " + str;
        addLogEntry(strOut, "RX");
        ui->textBrowserProcessed->append(strOut);
        ui->ir8_data->display(str);

        break;
    }
    case GETINTERNALDATA: {
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
            w.ui8[0] = datosRx[26]; w.ui8[1] = datosRx[27]; ui->setAttackSetpoint->setValue(w.i16[0]);
            w.ui8[0] = datosRx[28]; w.ui8[1] = datosRx[29]; ui->setCounterAngle->setValue(w.i16[0]);

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

        // 5. Leer constantes actuales desde la Interfaz de Qt
        // (Como paramsSynced se encarga de poblarlas, siempre tendremos el valor real aquí)
        int16_t kp = ui->setBalanceKp->value();
        int16_t ki = ui->setBalanceKi->value();
        int16_t kd = ui->setBalanceKd->value();

        // 6. Calcular los Términos Individuales (Misma matemática que el micro)
        double term_P = (kp * current_error) / 1000.0;
        double term_I = (ki * current_integral) / 1000.0;
        double term_D = (kd * current_derivative) / 1000.0;
        double term_Out = current_output;

        // 7. Enviar a la gráfica embebida en MainWindow
        double t = runtimeTimer.elapsed() / 1000.0;
        updatePIDChart(t, term_P, term_I, term_D, term_Out);

        // Opcional: Imprimir en consola para depurar
        // qDebug() << "P:" << term_P << "I:" << term_I << "D:" << term_D << "Out:" << term_Out;
        break;
    }
    case SETPWML:
    case SETPWMR:
    case SETPWMMINR:
    case SETPWMMINL:
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

void MainWindow::on_sendCounterAngle_clicked() {
    uint8_t payload[10];
    uint8_t index = 0;
    _udat w;
    payload[index++] = SETBKANG;
    w.i32 = ui->setCounterAngle->value();
    payload[index++] = w.ui8[0];
    payload[index++] = w.ui8[1];
    sendCommand(payload, index);
    ui->textBrowserProcessed->append("***COUNTER ANGLE ACTUALIZADO***");
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

// -----------------------------------------------------------------------
// Chart PID embebido en MainWindow (widget PIDchart promovido a QChartView)
// -----------------------------------------------------------------------
void MainWindow::initPIDChart()
{
    pid_pSeries   = new QLineSeries(); pid_pSeries->setName("Proporcional (P)");
    pid_iSeries   = new QLineSeries(); pid_iSeries->setName("Integral (I)");
    pid_dSeries   = new QLineSeries(); pid_dSeries->setName("Derivativo (D)");
    pid_outSeries = new QLineSeries(); pid_outSeries->setName("Output Total");

    chartPID_mw = new QChart();
    chartPID_mw->addSeries(pid_pSeries);
    chartPID_mw->addSeries(pid_iSeries);
    chartPID_mw->addSeries(pid_dSeries);
    chartPID_mw->addSeries(pid_outSeries);
    chartPID_mw->setTitle("Aportes del PID (Balancín)");
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

    ui->PIDchart->setChart(chartPID_mw);
    ui->PIDchart->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::updatePIDChart(double time, double p, double i, double d, double out)
{
    pid_pSeries->append(time, p);
    pid_iSeries->append(time, i);
    pid_dSeries->append(time, d);
    pid_outSeries->append(time, out);

    // Escala adaptativa del eje Y
    double values[] = {p, i, d, out};
    bool changed = false;
    for (double val : values) {
        if (val > pid_yMax) { pid_yMax = val; changed = true; }
        if (val < pid_yMin) { pid_yMin = val; changed = true; }
    }
    if (changed) {
        double margin = (pid_yMax - pid_yMin) * 0.1;
        if (margin < 1.0) margin = 5.0;
        pid_axisY->setRange(pid_yMin - margin, pid_yMax + margin);
    }

    // Scroll del eje X (ventana de 10 segundos)
    if (time > 10.0)
        pid_axisX->setRange(time - 10.0, time);
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


