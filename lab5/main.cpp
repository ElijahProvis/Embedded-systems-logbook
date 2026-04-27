#include "mbed.h"
#include "arm_book_lib.h"



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
char alarmCause[64]   = "";

char codeEntry[5] = "0000";
int  codeIndex    = 0;

char evLog0[64] = "";
char evLog1[64] = "";
char evLog2[64] = "";
char evLog3[64] = "";
char evLog4[64] = "";
int  numEvents  = 0;

int tickCount = 0;



void alarmstate();
void printStatus( float temp, float gas, float pot );
void pause();
void keypadTask();
void logEvent( const char* msg );
void printEventLog();
void matrixKeypadInit();
char matrixKeypadScan();
char matrixKeypadUpdate();


int main()
{
    buzzer.mode(OpenDrain);
    buzzer.input();
    matrixKeypadInit();

    while (true) {
        pause();
        keypadTask();

        tickCount++;
        if (tickCount >= 50) {  // 50 x 10ms = 500ms
            tickCount = 0;
            alarmstate();
        }

        delay(10);
    }
}



void alarmstate()
    float temp = lm35.read() * 330.0;
    float gas  = mq2.read() * 2.0;
    float pot  = potentiometer.read();

    float templimit = 25.0 + pot * 12.0;
    float gaslimit  = pot * 0.55;         

    tempalarm = (temp > templimit);
    gasalarm  = (gas  > gaslimit);

   
    if (!tempalarm && !gasalarm) {
        alarmDeactivated = false;
    }


    if ((tempalarm || gasalarm) && !alarmOn && !alarmDeactivated) {
        alarmOn = true;
        char msg[64] = "";
        if (tempalarm && gasalarm) {
            sprintf(msg, "BOTH: T=%.1fC G=%.0fppm", temp, (gas / 1.1) * 800.0);
            sprintf(alarmCause, "Alarm on cause: temperature and gas\r\n");
        }
        if (tempalarm && !gasalarm) {
            sprintf(msg, "TEMP: %.1fC > %.1fC", temp, templimit);
            sprintf(alarmCause, "Alarm on cause: temperature\r\n");
        }
        if (gasalarm && !tempalarm) {
            sprintf(msg, "GAS: %.0fppm > %.0fppm", (gas / 1.1) * 800.0, (gaslimit / 0.55) * 800.0);
            sprintf(alarmCause, "Alarm on cause: gas\r\n");
        }
        logEvent(msg);
    }

    if (alarmOn) {
        buzzer.output();
        buzzer = 0;
        uartUsb.write(alarmCause, strlen(alarmCause));
        uartUsb.write("Enter 4-Digit Code to Deactivate\r\n",
            strlen("Enter 4-Digit Code to Deactivate\r\n"));
    }

    if (!alarmOn) {
        buzzer.input();
        if (!paused) {
            printStatus(temp, gas, pot);
        }
    }
}

void printStatus( float temp, float gas, float pot )
{
    char str[200];
    sprintf(str, "Temp: %.2f C\nGas: %.0f ppm\nTemp Limit: %.2f C\nGas Limit: %.0f ppm\nSystem Normal\r\n",
        temp,
        (gas / 1.1) * 800.0,
        25.0 + pot * 12.0,
        (pot * 0.55 / 0.55) * 800.0);
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

        if (codeIndex >= 4) {
            if (codeEntry[0]=='1' && codeEntry[1]=='2' &&
                codeEntry[2]=='3' && codeEntry[3]=='4') {
                uartUsb.write("Correct! Alarm deactivated.\r\n",
                    strlen("Correct! Alarm deactivated.\r\n"));
                alarmOn          = false;
                alarmDeactivated = true;
                alarmCause[0]    = '\0';
                buzzer.input();
            } else {
                uartUsb.write("Wrong code. Try again.\r\n",
                    strlen("Wrong code. Try again.\r\n"));
            }
            codeIndex = 0;
        }
    }
}



void logEvent( const char* msg )
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
    uartUsb.write("\r\n--- Event Log ---\r\n", strlen("\r\n--- Event Log ---\r\n"));
    if (numEvents == 0) uartUsb.write("No events yet.\r\n", strlen("No events yet.\r\n"));
    if (numEvents >= 1) { uartUsb.write("1: ", 3); uartUsb.write(evLog4, strlen(evLog4)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 2) { uartUsb.write("2: ", 3); uartUsb.write(evLog3, strlen(evLog3)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 3) { uartUsb.write("3: ", 3); uartUsb.write(evLog2, strlen(evLog2)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 4) { uartUsb.write("4: ", 3); uartUsb.write(evLog1, strlen(evLog1)); uartUsb.write("\r\n", 2); }
    if (numEvents >= 5) { uartUsb.write("5: ", 3); uartUsb.write(evLog0, strlen(evLog0)); uartUsb.write("\r\n", 2); }
    uartUsb.write("-----------------\r\n\r\n", strlen("-----------------\r\n\r\n"));
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

    return keyReleased;
}
