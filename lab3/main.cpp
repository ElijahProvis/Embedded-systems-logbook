#include "mbed.h"
#include "arm_book_lib.h"
#include <cstring>

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

bool gasalarm = false;
bool overtempalarm = false;
bool monitoringMode = false;

void uartTask();
void sendAlarmStates();

int main()
{
    while (true) {
        uartTask();
    }
}

void sendAlarmStates()
{
    if (gasalarm) {
        uartUsb.write("GAS ALARM ACTIVE\r\n", strlen("GAS ALARM ACTIVE\r\n"));
    } else {
        uartUsb.write("GAS ALARM CLEAR\r\n", strlen("GAS ALARM CLEAR\r\n"));
    }
    if (overtempalarm) {
        uartUsb.write("TEMP ALARM ACTIVE\r\n", strlen("TEMP ALARM ACTIVE\r\n"));
    } else {
        uartUsb.write("TEMP ALARM CLEAR\r\n", strlen("TEMP ALARM CLEAR\r\n"));
    }
}

void uartTask()
{
    char receivedChar = '\0';
    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);
        if (receivedChar == '1') {
            gasalarm = true;
            uartUsb.write("WARNING: Gas triggered\r\n",
                strlen("WARNING: Gas triggered\r\n"));
        }
        if (receivedChar == '2') {
            if (gasalarm) {
                uartUsb.write("GAS ALARM ACTIVE\r\n",
                    strlen("GAS ALARM ACTIVE\r\n"));
            } else {
                uartUsb.write("GAS ALARM CLEAR\r\n",
                    strlen("GAS ALARM CLEAR\r\n"));
            }
        }
        if (receivedChar == '3') {
            if (overtempalarm) {
                uartUsb.write("TEMP ALARM ACTIVE\r\n",
                    strlen("TEMP ALARM ACTIVE\r\n"));
            } else {
                uartUsb.write("TEMP ALARM CLEAR\r\n",
                    strlen("TEMP ALARM CLEAR\r\n"));
            }
        }
        if (receivedChar == '4') {
            overtempalarm= true;
            uartUsb.write("WARNING: Over temp triggered\r\n",
                strlen("WARNING: Over temp triggered\r\n"));
        }
        if (receivedChar == '5') {
            gasalarm = false;
            overtempalarm= false;
            uartUsb.write("ALARMS RESET\r\n",
                strlen("ALARMS RESET\r\n"));
        }
        if (receivedChar == '6') {
            monitoringMode = !monitoringMode;
            if (monitoringMode) {
                uartUsb.write("Monitoring mode ENABLED\r\n",
                    strlen("Monitoring mode ENABLED\r\n"));
                while (monitoringMode) {
                    ThisThread::sleep_for(2000ms);
                    sendAlarmStates();
                    if (uartUsb.readable()) {
                        uartUsb.read(&receivedChar, 1);
                        if (receivedChar == '6') {
                            monitoringMode = false;
                            uartUsb.write("Monitoring mode DISABLED\r\n",
                                strlen("Monitoring mode DISABLED\r\n"));
                        }
                    }
                }
            }
        }
    }
}
