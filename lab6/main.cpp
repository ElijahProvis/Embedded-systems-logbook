#include "mbed.h"
#include "arm_book_lib.h"
#include "display.h"   

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);
 
AnalogIn     potentiometer(A0);
AnalogIn     lm35(A1);
AnalogIn     mq2(A2);
DigitalInOut buzzer(PE_10);
 
DigitalOut keypadRowPins[4] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn  keypadColPins[4] = {PB_12, PB_13, PB_15, PC_6};
 
typedef enum {
    MATRIX_KEYPAD_SCANNING,
    MATRIX_KEYPAD_DEBOUNCE,
    MATRIX_KEYPAD_KEY_HOLD_PRESSED
} matrixKeypadState_t;
 
matrixKeypadState_t matrixKeypadState         = MATRIX_KEYPAD_SCANNING;
char                matrixKeypadLastKeyPressed = '\0';
int                 accumulatedDebounceTime    = 0;
 
char matrixKeypadIndexToCharArray[] = {
    '1','2','3','A',
    '4','5','6','B',
    '7','8','9','C',
    '*','0','#','D'
};
 
bool tempalarm        = false;
bool gasalarm         = false;
bool paused           = false;
bool alarmOn          = false;
bool alarmDeactivated = false;
 
char alarmCause[64]    = "";        
char lcdAlarmCause[32] = "";        

char codeEntry[6] = "00000";        
int  codeIndex    = 0;
 
char evLog0[64] = "";
char evLog1[64] = "";
char evLog2[64] = "";
char evLog3[64] = "";
char evLog4[64] = "";
int  numEvents  = 0;

int tickCount    = 0;   
int lcdTickCount = 0;   
 
float lastTemp = 0.0f;
float lastGas  = 0.0f;
float lastPot  = 0.5f;
 
void alarmstate();
void printStatus(float temp, float gas, float pot);
void pause();
void keypadTask();
void logEvent(const char* msg);
void printEventLog();
void matrixKeypadInit();
char matrixKeypadScan();
char matrixKeypadUpdate();
 
void lcdClear();
void lcdShowStartup();
void lcdShowGasState();
void lcdShowTempState();
void lcdShowAlarmState();
void lcdShowWarning(float temp, float gas, float pot);
 
int main()
{
    buzzer.mode(OpenDrain);
    buzzer.input();
    matrixKeypadInit();

    displayInit(DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER);
 
    lcdShowStartup();
 
    while (true) {
        pause();
        keypadTask();
 
    
        lcdTickCount++;
        if (lcdTickCount >= 6000) {  
            lcdTickCount = 0;
            lcdShowAlarmState();
        }
 
        tickCount++;
        if (tickCount >= 50) {  
            tickCount = 0;
            alarmstate();
        }
 
        delay(10);
    }
}
 

void lcdClear()
{
    const char blank[] = "                    "; 
    displayCharPositionWrite(0, 0); displayStringWrite(blank);
    displayCharPositionWrite(0, 1); displayStringWrite(blank);
    displayCharPositionWrite(0, 2); displayStringWrite(blank);
    displayCharPositionWrite(0, 3); displayStringWrite(blank);
}
 
void lcdShowStartup()
{
    lcdClear();
    displayCharPositionWrite(0, 0); displayStringWrite("  ALARM SYSTEM  ");
    displayCharPositionWrite(0, 1); displayStringWrite("Enter Code to   ");
    displayCharPositionWrite(0, 2); displayStringWrite("Deactivate Alarm");
    displayCharPositionWrite(0, 3); displayStringWrite("Code: 5 digits  ");
}
 

void lcdShowGasState()
{
    float gasPpm   = (lastGas  / 1.1f) * 800.0f;
    float limitPpm = (lastPot * 0.55f / 0.55f) * 800.0f;
 
    char line0[21], line1[21], line2[21];
    sprintf(line0, "Gas:  %5.0f ppm     ", gasPpm);
    sprintf(line1, "Limit:%5.0f ppm     ", limitPpm);
    sprintf(line2, gasalarm ? "Status: ALARM   " : "Status: Normal  ");
 
    lcdClear();
    displayCharPositionWrite(0, 0); displayStringWrite(line0);
    displayCharPositionWrite(0, 1); displayStringWrite(line1);
    displayCharPositionWrite(0, 2); displayStringWrite(line2);
    // line 3 left blank
}
 

void lcdShowTempState()
{
    float templimit = 25.0f + lastPot * 12.0f;
 
    char line0[21], line1[21], line2[21];
    sprintf(line0, "Temp:  %5.1f C      ", lastTemp);
    sprintf(line1, "Limit: %5.1f C      ", templimit);
    sprintf(line2, tempalarm ? "Status: ALARM   " : "Status: Normal  ");
 
    lcdClear();
    displayCharPositionWrite(0, 0); displayStringWrite(line0);
    displayCharPositionWrite(0, 1); displayStringWrite(line1);
    displayCharPositionWrite(0, 2); displayStringWrite(line2);
}
 

void lcdShowAlarmState()
{
    char line2[21], line3[21];
    float gasPpm = (lastGas / 1.1f) * 800.0f;
 
    lcdClear();
 
    if (alarmOn) {
        displayCharPositionWrite(0, 0); displayStringWrite("ALARM ACTIVE");
        displayCharPositionWrite(0, 1); displayStringWrite(lcdAlarmCause);
        displayCharPositionWrite(0, 2); displayStringWrite("Enter 5-digit   ");
        displayCharPositionWrite(0, 3); displayStringWrite("deactivation code");
    } else {
        displayCharPositionWrite(0, 0); displayStringWrite("System: no alarms");
        sprintf(line2, "Temp: %.1fC         ", lastTemp);
        sprintf(line3, "Gas:  %.0f ppm      ", gasPpm);
        displayCharPositionWrite(0, 2); displayStringWrite(line2);
        displayCharPositionWrite(0, 3); displayStringWrite(line3);
    }
}
 

void lcdShowWarning(float temp, float gas, float pot)
{
    float templimit = 25.0f + pot * 12.0f;
    float gaslimit  = pot * 0.55f;
    float gasPpm    = (gas  / 1.1f) * 800.0f;
    float limitPpm  = (gaslimit / 0.55f) * 800.0f;
 
    char line1[21], line2[21];
 
    lcdClear();
    displayCharPositionWrite(0, 0); displayStringWrite("ALARM ");
 
    if (tempalarm && gasalarm) {
        sprintf(line1, "T:%.1fC > %.1fC    ", temp, templimit);
        sprintf(line2, "G:%.0f>%.0fppm  ", gasPpm, limitPpm);
        displayCharPositionWrite(0, 1); displayStringWrite(line1);
        displayCharPositionWrite(0, 2); displayStringWrite(line2);
    }
 
    if (tempalarm && !gasalarm) {
        sprintf(line1, "Temp %.1fC>%.1fC   ", temp, templimit);
        displayCharPositionWrite(0, 1); displayStringWrite(line1);
        displayCharPositionWrite(0, 2); displayStringWrite("Over-Temp Alarm ");
    }
 
    if (gasalarm && !tempalarm) {
        sprintf(line1, "Gas %.0f>%.0fppm   ", gasPpm, limitPpm);
        displayCharPositionWrite(0, 1); displayStringWrite(line1);
        displayCharPositionWrite(0, 2); displayStringWrite("Gas Level Alarm ");
    }
 
    displayCharPositionWrite(0, 3); displayStringWrite("Enter 5-digit code");
}
 
void alarmstate()
{
    float temp = lm35.read() * 330.0f;
    float gas  = mq2.read() * 2.0f;
    float pot  = potentiometer.read();
 
    lastTemp = temp;
    lastGas  = gas;
    lastPot  = pot;
 
    float templimit = 25.0f + pot * 12.0f;   
    float gaslimit  = pot * 0.55f;            
 
    tempalarm = (temp > templimit);
    gasalarm  = (gas  > gaslimit);
 
    if (!tempalarm && !gasalarm) {
        alarmDeactivated = false;
    }
 
    if ((tempalarm || gasalarm) && !alarmOn && !alarmDeactivated) {
        alarmOn = true;
 
        char msg[64] = "";
 
        if (tempalarm && gasalarm) {
            sprintf(msg,           "BOTH: T=%.1fC G=%.0fppm", temp, (gas / 1.1f) * 800.0f);
            sprintf(alarmCause,    "Alarm cause: temp and gas\r\n");
            sprintf(lcdAlarmCause, "Cause:Temp & Gas");
        }
        if (tempalarm && !gasalarm) {
            sprintf(msg,           "TEMP: %.1fC > %.1fC", temp, templimit);
            sprintf(alarmCause,    "Alarm cause: temperature\r\n");
            sprintf(lcdAlarmCause, "Cause:Temp High ");
        }
        if (gasalarm && !tempalarm) {
            sprintf(msg,           "GAS: %.0fppm > %.0fppm", (gas / 1.1f) * 800.0f, (gaslimit / 0.55f) * 800.0f);
            sprintf(alarmCause,    "Alarm cause: gas\r\n");
            sprintf(lcdAlarmCause, "Cause:Gas High  ");
        }
 
        logEvent(msg);

        lcdShowWarning(temp, gas, pot);
    }
 
    if (alarmOn) {
        buzzer.output();
        buzzer = 0;
        uartUsb.write(alarmCause, strlen(alarmCause));
        uartUsb.write("Enter 5-Digit Code to Deactivate\r\n",
                      strlen("Enter 5-Digit Code to Deactivate\r\n"));
    }

    if (!alarmOn) {
        buzzer.input();
        if (!paused) {
            printStatus(temp, gas, pot);
        }
    }
}
 
void printStatus(float temp, float gas, float pot)
{
    char str[200];
    sprintf(str,
        "Temp: %.2f C\nGas: %.0f ppm\nTemp Limit: %.2f C\nGas Limit: %.0f ppm\nSystem Normal\r\n",
        temp,
        (gas / 1.1f) * 800.0f,
        25.0f + pot * 12.0f,
        (pot * 0.55f / 0.55f) * 800.0f);
    uartUsb.write(str, strlen(str));
}
 
void pause()
{
    char receivedChar = '\0';
    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);
        if (receivedChar == 'q') {
            paused = !paused;
        }
    }
}
 

void keypadTask()
{
    char key = matrixKeypadUpdate();
    if (key == '\0') return;
 

    if (key == '#') {
        printEventLog();
        return;
    }
 

    if (alarmOn) {
        codeEntry[codeIndex] = key;
        codeIndex++;
        codeEntry[codeIndex] = '\0';
 
        if (codeIndex >= 5) {
 
            if (codeEntry[0]=='1' && codeEntry[1]=='2' &&
                codeEntry[2]=='3' && codeEntry[3]=='4' &&
                codeEntry[4]=='5') {
 
                uartUsb.write("Alarm deactivated.\r\n",
                              strlen("Alarm deactivated.\r\n"));
                alarmOn          = false;
                alarmDeactivated = true;
                alarmCause[0]    = '\0';
                lcdAlarmCause[0] = '\0';
                buzzer.input();
 

                lcdClear();
                displayCharPositionWrite(0, 0); displayStringWrite("Alarm Deactivated");
                displayCharPositionWrite(0, 1); displayStringWrite("System Normal   ");
 
            } else {
                uartUsb.write("Wrong code. Try again.\r\n",
                              strlen("Wrong code. Try again.\r\n"));
 
                lcdClear();
                displayCharPositionWrite(0, 0); displayStringWrite("Wrong Code");
                displayCharPositionWrite(0, 1); displayStringWrite("Enter 5-digit code   ");
            }
            codeIndex = 0;
        }
        return;
    }
 
    if (key == '4') {
        lcdShowGasState();
        return;
    }
 
    if (key == '5') {
        lcdShowTempState();
        return;
    }
}
 
void logEvent(const char* msg)
{
    sprintf(evLog0, "%s", evLog1);
    sprintf(evLog1, "%s", evLog2);
    sprintf(evLog2, "%s", evLog3);
    sprintf(evLog3, "%s", evLog4);
    sprintf(evLog4, "%s", msg);
    if (numEvents < 5) numEvents++;
}
 
void printEventLog()
{
    uartUsb.write("\r\nEvent Log\r\n", strlen("\r\nEvent Log\r\n"));
    if (numEvents == 0) uartUsb.write("No events yet.\r\n", strlen("No events yet.\r\n"));
    if (numEvents >= 1) { uartUsb.write("1: ", 3); uartUsb.write(evLog4, strlen(evLog4)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 2) { uartUsb.write("2: ", 3); uartUsb.write(evLog3, strlen(evLog3)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 3) { uartUsb.write("3: ", 3); uartUsb.write(evLog2, strlen(evLog2)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 4) { uartUsb.write("4: ", 3); uartUsb.write(evLog1, strlen(evLog1)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 5) { uartUsb.write("5: ", 3); uartUsb.write(evLog0, strlen(evLog0)); uartUsb.write("\r\n", 2); }
}
 
void matrixKeypadInit()
{
    matrixKeypadState = MATRIX_KEYPAD_SCANNING;
    for (int i = 0; i < 4; i++) {
        keypadColPins[i].mode(PullUp);
    }
}
 
char matrixKeypadScan()
{
    for (int r = 0; r < 4; r++) {
        for (int i = 0; i < 4; i++) keypadRowPins[i] = ON;
        keypadRowPins[r] = OFF;
        for (int c = 0; c < 4; c++) {
            if (keypadColPins[c] == OFF) {
                return matrixKeypadIndexToCharArray[r * 4 + c];
            }
        }
    }
    return '\0';
}
 
char matrixKeypadUpdate()
{
    char keyDetected = '\0';
    char keyReleased = '\0';
 
    switch (matrixKeypadState) {
 
    case MATRIX_KEYPAD_SCANNING:
        keyDetected = matrixKeypadScan();
        if (keyDetected != '\0') {
            matrixKeypadLastKeyPressed = keyDetected;
            accumulatedDebounceTime    = 0;
            matrixKeypadState          = MATRIX_KEYPAD_DEBOUNCE;
        }
        break;
 
    case MATRIX_KEYPAD_DEBOUNCE:
        if (accumulatedDebounceTime >= 40) {
            keyDetected = matrixKeypadScan();
            if (keyDetected == matrixKeypadLastKeyPressed) {
                matrixKeypadState = MATRIX_KEYPAD_KEY_HOLD_PRESSED;
            } else {
                matrixKeypadState = MATRIX_KEYPAD_SCANNING;
            }
        }
        accumulatedDebounceTime += 10;
        break;
 
    case MATRIX_KEYPAD_KEY_HOLD_PRESSED:
        keyDetected = matrixKeypadScan();
        if (keyDetected != matrixKeypadLastKeyPressed) {
            if (keyDetected == '\0') {
                keyReleased = matrixKeypadLastKeyPressed;
            }
            matrixKeypadState = MATRIX_KEYPAD_SCANNING;
        }
        break;
 
    default:
        matrixKeypadInit();
        break;
    }
 
    return keyReleased;}
