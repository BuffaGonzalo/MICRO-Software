#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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
    connect(ui->pushButton_sendSerial,&QPushButton::clicked, this, &MainWindow::sendDataSerial);
    connect(QSerialPort1,&QSerialPort::readyRead,this,&MainWindow::dataReceived);

    //connects de los timers con las funciones
    connect(timer1,&QTimer::timeout,this,&MainWindow::timeOut);
    connect(timer2,&QTimer::timeout,this,&MainWindow::getData);

    //connects de udp
    connect(QUdpSocket1,&QUdpSocket::readyRead,this,&MainWindow::OnUdpRxData);
    connect(ui->pushButton_sendUdp,&QPushButton::clicked,this,&MainWindow::sendDataUDP);

    //connect(ui->actionScanPorts, &QAction::triggered, settingPorts,&SettingsDialog::show);
    connect(ui->actionGRAPHICS, &QAction::triggered, myGraphics, &graphics::show);

    //añadimos los comandos
    ui->comboBox_CMD->addItem("ALIVE", 0xF0);
    ui->comboBox_CMD->addItem("FIRMWARE", 0xF1);
    ui->comboBox_CMD->addItem("GETMPU", 0xF2);
    ui->comboBox_CMD->addItem("GETADC", 0xF3);
    ui->comboBox_CMD->addItem("SETPWM", 0xF4);
    ui->comboBox_CMD->addItem("SETPID", 0xF5);
    ui->comboBox_CMD->addItem("SETPWMLIMIT", 0xF6);
    ui->comboBox_CMD->addItem("SETSPEED", 0xF7);
    ui->comboBox_CMD->addItem("*", 0xA9);

    //inicializamos
    estadoProtocolo=START;
    rxData.timeOut=0;

    //desabilitamos los botones con el fin de que no puedan ser presionados si no esta conectado
    ui->pushButton_sendUdp->setEnabled(false);
    ui->pushButton_sendSerial->setEnabled(false);

    statusMode = new QLabel(this);
    ui->statusBar->addWidget(statusMode);

    runtimeTimer.start();

    timer1->start(100);
    timer2->start(50);
}

MainWindow::~MainWindow()
{
    delete ui;
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
                    ui->textBrowserUnProcessed->append("Chk Calculado ** " +QString().number(rxData.cheksum,16) + " **" );
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
    int32_t length = sizeof(*datosRx)/sizeof(datosRx[0]);
    static int16_t contadorAlive=0;
    QString str, strOut;
    _udat w;
    for(int i = 1; i<length; i++){
        if(isalnum(datosRx[i]))
            str = str + QString("%1").arg(char(datosRx[i]));
        else
            str = str +QString("%1").arg(datosRx[i],2,16,QChar('0'));
    }
    ui->textBrowserUnProcessed->append("*(MBED-S->PC)->decodeData (" + str + ")");

    str=QString().number(datosRx[0]);
    ui->textBrowserProcessed->append(str);
    str=QString().number(datosRx[1]);
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
        ui->textBrowserProcessed->append(str);
        break;
    case GETFIRMWARE://     GETFIRMWARE=0xF1
        str = "FIRMWARE: ";
        for(uint8_t i=0;i<(datosRx[0]-2);i++){ //datosRx[0] -> tamaño en bytes del mensaje.
            str += (QChar)datosRx[2+i];
        }
        ui->textBrowserProcessed->append(str);
        break;
    case GETMPU:{

        //Datos acelerometro
        w.i8[0] = datosRx[2];
        w.i8[1] = datosRx[3];

        float ax = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Ax: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ax_data->setText(str);

        w.i8[0] = datosRx[4];
        w.i8[1] = datosRx[5];

        float ay = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Ay: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ay_data->setText(str);

        w.i8[0] = datosRx[6];
        w.i8[1] = datosRx[7];

        float az = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Az: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->az_data->setText(str);

        //Datos giroscopio
        w.i8[0] = datosRx[8];
        w.i8[1] = datosRx[9];

        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gx: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->gx_data->setText(str);

        w.i8[0] = datosRx[10];
        w.i8[1] = datosRx[11];

        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gy: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->gy_data->setText(str);

        w.i8[0] = datosRx[12];
        w.i8[1] = datosRx[13];

        float gz = w.i16[0];
        str = QString("%1").arg(w.i16[0], 5, 10, QChar('0'));
        strOut = "Gz: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->gz_data->setText(str);

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
    case GETADC:
        //Datos acelerometro
        w.ui8[0] = datosRx[2];
        w.ui8[1] = datosRx[3];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR1: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir1_data->setText(str);

        w.ui8[0] = datosRx[4];
        w.ui8[1] = datosRx[5];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR2: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir2_data->setText(str);

        w.ui8[0] = datosRx[6];
        w.ui8[1] = datosRx[7];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR3: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir3_data->setText(str);

        w.ui8[0] = datosRx[8];
        w.ui8[1] = datosRx[9];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR4: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir4_data->setText(str);

        w.ui8[0] = datosRx[10];
        w.ui8[1] = datosRx[11];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR5: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir5_data->setText(str);

        w.ui8[0] = datosRx[12];
        w.ui8[1] = datosRx[13];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR6: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir6_data->setText(str);

        w.ui8[0] = datosRx[14];
        w.ui8[1] = datosRx[15];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR7: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir7_data->setText(str);

        w.ui8[0] = datosRx[16];
        w.ui8[1] = datosRx[17];
        str = QString("%1").arg(w.ui16[0], 5, 10, QChar('0'));
        strOut = "IR8: " + str;
        ui->textBrowserProcessed->append(strOut);
        ui->ir8_data->setText(str);

        break;
    case SETPWM:
        if(datosRx[2]==ACK){
            str="MODIFICACION PARAMETROS PWM - VELOCIDAD MOTOR!!!";
            ui->textBrowserProcessed->append(str);
        }
        break;
    case SETPID:
        if(datosRx[2]==ACK){
            str="MODIFICACION PARAMETROS PID!!!";
            ui->textBrowserProcessed->append(str);
        }
        break;
    case SETPWMLIMIT:
        if(datosRx[2]==ACK){
            str="MODIFICACION VELOCIDAD MAXIMA PID!!!";
            ui->textBrowserProcessed->append(str);
        }
        break;
    case SETSPEED:
        if(datosRx[2]==ACK){
            str="MODIFICACION PARAMETROS PID!!!";
            ui->textBrowserProcessed->append(str);
        }
        break;
    case GETPIDDATA: {
        // --- 1. Bloque de 32 Bits (Bytes 2 al 37) ---
        w.ui8[0] = datosRx[2]; w.ui8[1] = datosRx[3]; w.ui8[2] = datosRx[4]; w.ui8[3] = datosRx[5];
        ui->acc_angle_hr_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[6]; w.ui8[1] = datosRx[7]; w.ui8[2] = datosRx[8]; w.ui8[3] = datosRx[9];
        ui->gyro_delta_hr_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[10]; w.ui8[1] = datosRx[11]; w.ui8[2] = datosRx[12]; w.ui8[3] = datosRx[13];
        ui->current_angle_hr_data->setText(QString("%1").arg(w.i32));
        ui->current_angle_data->setText(QString("%1").arg(w.i32/100));

        w.ui8[0] = datosRx[14]; w.ui8[1] = datosRx[15]; w.ui8[2] = datosRx[16]; w.ui8[3] = datosRx[17];
        ui->setpoint_dinamico_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[18]; w.ui8[1] = datosRx[19]; w.ui8[2] = datosRx[20]; w.ui8[3] = datosRx[21];
        ui->error_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[22]; w.ui8[1] = datosRx[23]; w.ui8[2] = datosRx[24]; w.ui8[3] = datosRx[25];
        ui->integral_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[26]; w.ui8[1] = datosRx[27]; w.ui8[2] = datosRx[28]; w.ui8[3] = datosRx[29];
        ui->derivative_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[30]; w.ui8[1] = datosRx[31]; w.ui8[2] = datosRx[32]; w.ui8[3] = datosRx[33];
        ui->output_data->setText(QString("%1").arg(w.i32));

        w.ui8[0] = datosRx[34]; w.ui8[1] = datosRx[35]; w.ui8[2] = datosRx[36]; w.ui8[3] = datosRx[37];
        ui->setpointData->setText(QString("%1").arg(w.i32));

        // --- 2. Bloque de 16 Bits (Bytes 38 al 47) ---
        w.ui8[0] = datosRx[38]; w.ui8[1] = datosRx[39];
        ui->kpStableData->setText(QString("%1").arg(w.i16[0]));

        w.ui8[0] = datosRx[40]; w.ui8[1] = datosRx[41];
        ui->kiStableData->setText(QString("%1").arg(w.i16[0]));

        w.ui8[0] = datosRx[42]; w.ui8[1] = datosRx[43];
        ui->kdStableData->setText(QString("%1").arg(w.i16[0]));

        // --- DATOS DE LA RAMPA (Sin rastro de Kick) ---
        w.ui8[0] = datosRx[44]; w.ui8[1] = datosRx[45];
        ui->ramp_step_data->setText(QString("%1").arg(w.i16[0]));

        w.ui8[0] = datosRx[46]; w.ui8[1] = datosRx[47];
        ui->max_offset_data->setText(QString("%1").arg(w.i16[0]));

        // --- 3. Bloque de 8 Bits (Bytes 48 y 49) ---
        ui->maxPwmData->setText(QString().number(datosRx[48]));
        ui->minPwmData->setText(QString().number(datosRx[49]));

        ui->impulseLenght_data->setText(QString().number(datosRx[50]));

        ui->textBrowserProcessed->append("TELEMETRÍA ACTUALIZADA");
        break;
    }
    default:
        str = str + "Comando DESCONOCIDO!!!!";
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
        QUdpSocket1->writeDatagram(reinterpret_cast<const char *>(tx), (tx[4]+7), clientAddress, puertoremoto);

    }

    for(int i=0; i<=indice; i++){
        if(isalnum(tx[i]))
            str = str + QString("%1").arg(char(tx[i]));
        else
            str = str +"{" + QString("%1").arg(tx[i],2,16,QChar('0')) + "}";
    }
    str=str + clientAddress.toString() + "  " +  QString().number(puertoremoto,10);

    ui->textBrowserUnProcessed->append("PC--UDP-->MBED ( " + str + " )");
}

void MainWindow::sendDataSerial(){
    uint8_t payload[256];
    uint8_t length = 0;

    // Si el usuario completa los datos correctamente, lo enviamos
    if (buildPayload(payload, length)) {
        sendSerial(payload, length);
        ui->textBrowserProcessed->append("***COMANDO ENVIADO POR SERIAL***");
    }
}

void MainWindow::sendDataUDP(){
    uint8_t payload[256];
    uint8_t length = 0;

    // Si el usuario completa los datos correctamente, lo enviamos
    if (buildPayload(payload, length)) {
        sendUdp(payload, length);
        ui->textBrowserProcessed->append("***COMANDO ENVIADO POR UDP***");
    }
}

bool MainWindow::buildPayload(uint8_t *payload, uint8_t &length) {
    uint8_t cmdId = ui->comboBox_CMD->currentData().toInt();
    _udat w;
    QString str;
    bool ok;
    uint8_t index = 0;

    switch (cmdId) {
    case GETALIVE:
    case GETADC:
    case GETMPU:
    case GETFIRMWARE:
    case GETPIDDATA:
        payload[index++] = cmdId;
        break;
    case SETPWM:
        payload[index++] = SETPWM;
        w.i32 = QInputDialog::getInt(this, "Settear_PWM", "Channel 1:", 0, 0, 100, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];

        w.i32 = QInputDialog::getInt(this, "Settear_PWM", "Channel 2:", 0, 0, 100, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];

        w.i32 = QInputDialog::getInt(this, "Settear_PWM", "Channel 3:", 0, 0, 100, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];

        w.i32 = QInputDialog::getInt(this, "Settear_PWM", "Channel 4:", 0, 0, 100, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];
        break;
    case SETPID:
        payload[index++] = SETPID;
        w.i32 = QInputDialog::getInt(this, "PID_Balancin", "Kp", 0, 0, 5000, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];
        payload[index++] = w.ui8[1];

        w.i32 = QInputDialog::getInt(this, "PID_Balancin", "Kd", 0, 0, 5000, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];
        payload[index++] = w.ui8[1];

        w.i32 = QInputDialog::getInt(this, "PID_Balancin", "Ki", 0, 0, 5000, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];
        payload[index++] = w.ui8[1];
        break;
    case SETPWMLIMIT:
        payload[index++] = SETPWMLIMIT;
        w.i32 = QInputDialog::getInt(this, "Intervalo_PWM", "PWM_MAX: ", 0, 0, 200, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];

        w.i32 = QInputDialog::getInt(this, "Intervalo_PWM", "PWM_MIN", 0, 0, 200, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];

        break;
    case SETSPEED:
        payload[index++] = SETSPEED;

        // 1. Ramp Step
        w.i32 = QInputDialog::getInt(this, "Config Rampa", "Ramp Step (ej: 4):", 10, 1, 500, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];
        payload[index++] = w.ui8[1];

        // 2. Max Offset
        w.i32 = QInputDialog::getInt(this, "Config Rampa", "Max Offset (ej: 250):", 200, 0, 10000, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];
        payload[index++] = w.ui8[1];

        // 3. Setpoint Base
        w.i32 = QInputDialog::getInt(this, "Angulo Equilibrio", "Setpoint base (ej: -150):", -150, -5000, 5000, 10, &ok);
        if(!ok) return false;
        payload[index++] = w.i8[0];
        payload[index++] = w.i8[1];

        // 4. Dirección / Custom Speed
        w.i32 = QInputDialog::getInt(this, "Movimiento", "Direccion (-1=Av, 1=Re, 0=Stop):", -1, -10000, 10000, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.i8[0];
        payload[index++] = w.i8[1];

        // 5. Duración del Impulso (NUEVO PARAMETRO)
        w.i32 = QInputDialog::getInt(this, "Dedo Digital", "Ciclos de pulso (ej: 6 = 120ms):", 6, 0, 100, 1, &ok);
        if(!ok) return false;
        payload[index++] = w.ui8[0];

        break;
    default:
        return false; // Comando desconocido
    }

    length = index; // Guardamos la cantidad de bytes que conforman el payload
    return true;
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
    qint64          count=0;
    unsigned char   *incomingBuffer = NULL;

    while(QUdpSocket1->hasPendingDatagrams()){
        count = QUdpSocket1->pendingDatagramSize();
        incomingBuffer = new unsigned char[count];
        QUdpSocket1->readDatagram( reinterpret_cast<char *>(incomingBuffer), count, &RemoteAddress, &RemotePort);
    }
    if (count<=0)
        return;

    QString str="";
    for(int i=0; i<count; i++){
        if(isalnum(incomingBuffer[i]))
            str = str + QString("%1").arg(char(incomingBuffer[i]));
        else
            str = str +"{" + QString("%1").arg(incomingBuffer[i],2,16,QChar('0')) + "}";
    }
    ui->textBrowserUnProcessed->append("MBED-->UDP-->PC (" + str + ")");
    QString adress=RemoteAddress.toString();
    ui->textBrowserUnProcessed->append(" adr " + adress);

    ui->lineEdit_device_ip->setText(RemoteAddress.toString().right((RemoteAddress.toString().length())-7));
    ui->lineEdit_device_port->setText(QString().number(RemotePort,10));

    for(int i=0;i<count; i++){
        switch (estadoProtocoloUdp) {
        case START:
            if (incomingBuffer[i]=='U'){
                estadoProtocoloUdp=HEADER_1;
                rxDataUdp.cheksum=0;
            }
            break;
        case HEADER_1:
            if (incomingBuffer[i]=='N')
                estadoProtocoloUdp=HEADER_2;
            else{
                i--;
                estadoProtocoloUdp=START;
            }
            break;
        case HEADER_2:
            if (incomingBuffer[i]=='E')
                estadoProtocoloUdp=HEADER_3;
            else{
                i--;
                estadoProtocoloUdp=START;
            }
            break;
        case HEADER_3:
            if (incomingBuffer[i]=='R')
                estadoProtocoloUdp=NBYTES;
            else{
                i--;
                estadoProtocoloUdp=START;
            }
            break;
        case NBYTES:
            rxDataUdp.nBytes=incomingBuffer[i];
            estadoProtocoloUdp=TOKEN;
            break;
        case TOKEN:
            if (incomingBuffer[i]==':'){
                estadoProtocoloUdp=PAYLOAD;
                rxDataUdp.cheksum='U'^'N'^'E'^'R'^ rxDataUdp.nBytes^':';
                rxDataUdp.payLoad[0]=rxDataUdp.nBytes;
                rxDataUdp.index=1;
            }
            else{
                i--;
                estadoProtocoloUdp=START;
            }
            break;
        case PAYLOAD:
            if (rxDataUdp.nBytes>1){
                rxDataUdp.payLoad[rxDataUdp.index++]=incomingBuffer[i];
                rxDataUdp.cheksum^=incomingBuffer[i];
            }
            rxDataUdp.nBytes--;
            if(rxDataUdp.nBytes==0){
                estadoProtocoloUdp=START;

                if(rxDataUdp.cheksum==incomingBuffer[i]){
                    decodeData(&rxDataUdp.payLoad[0],UDP);
                }else{
                    ui->textBrowserProcessed->append(" CHK DISTINTO!!!!! ");
                }
            }
            break;

        default:
            estadoProtocoloUdp=START;
            break;
        }
    }
    delete [] incomingBuffer;

}


void MainWindow::getData(){
    static uint8_t wifiMef = 1;
    uint8_t buf[1];

    switch (wifiMef){
    case 1:
        buf[0]=GETMPU;
        break;
    case 2:
        buf[0]=GETADC;
        break;
    case 3:
        buf[0]=GETPIDDATA;
        break;
    }


    wifiMef++;
    if (wifiMef > 3) {
        wifiMef = 1;
    }

    sendSerial(buf, 1);
    sendUdp(buf, 1);

    if(!QSerialPort1->isOpen() && !QUdpSocket1->isOpen()) //colocamos un estadopredeterminado en caso de no estar conectado
        statusMode->setText("CURRENT STATE --> DESCONECTADO");
    else
        if(QSerialPort1->isOpen())
            statusMode->setText("CURRENT STATE --> CONNECTADO SERIE");
        else if (QUdpSocket1->isOpen())
            statusMode->setText("CURRENT STATE --> CONNECTADO UDP");
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
        ui->pushButton_sendSerial->setEnabled(false);
        ui->pushButton_connectSerial->setText("CONNECT");
    }
    else{
        ui->pushButton_sendSerial->setEnabled(true);

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
        }
        else
            QMessageBox::information(this, "Serial PORT", "ERROR. Opening PORT");
    }
}

void MainWindow::on_pushButton_sendSerial_clicked()
{

}

void MainWindow::on_pushButton_connectUdp_clicked()
{
    int Port;
    bool ok;

    if(QUdpSocket1->isOpen()){
        QUdpSocket1->close();
        ui->pushButton_sendUdp->setEnabled(false);
        ui->pushButton_connectUdp->setText("CONNECT");
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
    ui->pushButton_sendUdp->setEnabled(true);
    if(QUdpSocket1->isOpen()){
        if(clientAddress.isNull())
            clientAddress.setAddress(ui->lineEdit_device_ip->text());
        if(puertoremoto==0)
            puertoremoto=ui->lineEdit_device_port->text().toInt();
        QUdpSocket1->writeDatagram("r", 1, clientAddress, puertoremoto);
    }
}
