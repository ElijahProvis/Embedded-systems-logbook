#include "mbed.h"
#include "arm_book_lib.h"

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);
AnalogIn mq2(A2);
DigitalInOut buzzer(PE_10);

bool tempalarm = false;
bool gasalarm  = false;
bool paused      = false;

void alarmstate();
void printStatus();
void pause();

int main()
{
    buzzer.mode(OpenDrain);
    buzzer.input();

    while (true) {
        pause();
        alarmstate();
        ThisThread:ThisThread::sleep_for(500ms);
    }
}

void alarmstate()
{
    float temp = lm35.read() * 330.0;
    float gas = mq2.read();
    float pot = potentiometer.read();

    float templimit = pot * 100.0;
    float gaslimit  = pot;

    tempalarm = (temp > templimit);
    gasalarm  = (gas > gaslimit);

    if (tempalarm && gasalarm) {
        buzzer.output();
        buzzer = 0;
        uartUsb.write("Alarm on cause: temperature and gas\r\n",
            strlen("Alarm on cause: temperature and gas\r\n"));
    }
    if (tempalarm && !gasalarm) {
        buzzer.output();
        buzzer = 0;
        uartUsb.write("Alarm on cause: temperature\r\n",
            strlen("Alarm on cause: temperature\r\n"));
    }
    if (gasalarm && !tempalarm) {
        buzzer.output();
        buzzer = 0;
        uartUsb.write("Alarm on cause: gas\r\n",
            strlen("Alarm on cause: gas\r\n"));
    }
    if (!tempalarm && !gasalarm) {
        buzzer.input();
        if (!paused) {
            printStatus();
        }
    }
}

void printStatus()
{
    char str[200];
    sprintf(str, "Temp: %.2f C\nGas: %.2f\nPot: %.2f\nSystem Normal\r\n",
        lm35.read() * 330.0,
        mq2.read()*100,
        potentiometer.read()*100);
    uartUsb.write(str, strlen(str));
}

void pause()
{
    char receivedChar = '\0';
    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);
        if (receivedChar == 'q') {
            paused = !paused;
            if (paused) {
                uartUsb.write("PAUSED\r\n", strlen("PAUSED\r\n"));
            }
            if (!paused) {
                uartUsb.write("RESUMED\r\n", strlen("RESUMED\r\n"));
            }
        }
    }
}
