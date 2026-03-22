#include "graphics.h"
#include "ui_graphics.h"

graphics::graphics(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::graphics)
{
    ui->setupUi(this);

    // --- GRÁFICA 1: EQUILIBRIO ---
    angleSeries = new QLineSeries();
    angleSeries->setName("Ángulo Actual (°)");
    setpointSeries = new QLineSeries();
    setpointSeries->setName("Setpoint (°)");

    chartBalance = new QChart();
    chartBalance->addSeries(angleSeries);
    chartBalance->addSeries(setpointSeries);
    chartBalance->setTitle("Dinámica de Equilibrio (Cabeceo)");
    chartBalance->layout()->setContentsMargins(0,0,0,0);

    axisX_bal = new QValueAxis(); axisX_bal->setRange(0, 10);
    axisY_bal = new QValueAxis(); axisY_bal->setRange(-90, 90); // Ajusta según tu necesidad

    chartBalance->addAxis(axisX_bal, Qt::AlignBottom);
    chartBalance->addAxis(axisY_bal, Qt::AlignLeft);
    angleSeries->attachAxis(axisX_bal); angleSeries->attachAxis(axisY_bal);
    setpointSeries->attachAxis(axisX_bal); setpointSeries->attachAxis(axisY_bal);

    ui->visorBalancin->setChart(chartBalance);
    ui->visorBalancin->setRenderHint(QPainter::Antialiasing);

    // --- GRÁFICA 2: SEGUIDOR DE LÍNEA ---
    errorSeries = new QLineSeries();
    errorSeries->setName("Error de Línea");
    pwmSeries = new QLineSeries();
    pwmSeries->setName("PWM de Giro");

    chartLine = new QChart();
    chartLine->addSeries(errorSeries);
    chartLine->addSeries(pwmSeries);
    chartLine->setTitle("Dinámica de Dirección");
    chartLine->layout()->setContentsMargins(0,0,0,0);

    axisX_line = new QValueAxis(); axisX_line->setRange(0, 10);
    axisY_line = new QValueAxis(); axisY_line->setRange(-100, 100);

    chartLine->addAxis(axisX_line, Qt::AlignBottom);
    chartLine->addAxis(axisY_line, Qt::AlignLeft);
    errorSeries->attachAxis(axisX_line); errorSeries->attachAxis(axisY_line);
    pwmSeries->attachAxis(axisX_line); pwmSeries->attachAxis(axisY_line);

    ui->visorSeguidor->setChart(chartLine);
    ui->visorSeguidor->setRenderHint(QPainter::Antialiasing);
}

graphics::~graphics()
{
    delete ui;
}

void graphics::updateTelemetry(double time, double angle, double setpoint, int lineError, int turnPwm, int sumIR, int state)
{
    // Actualizar Gráficas
    angleSeries->append(time, angle);
    setpointSeries->append(time, setpoint);
    errorSeries->append(time, lineError);
    pwmSeries->append(time, turnPwm);

    // Scroll del eje X (Ventana de 10 segundos)
    if (time > 10.0) {
        axisX_bal->setRange(time - 10.0, time);
        axisX_line->setRange(time - 10.0, time);
    }
}
