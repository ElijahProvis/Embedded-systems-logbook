#include "mbed.h"
#include "arm_book_lib.h"
void code()
{
    DigitalOut greenled(LED1);
    DigitalOut redled(LED3);
    DigitalOut blueled(LED2);
    DigitalIn button0(D2);
    DigitalIn button1(D3);
    DigitalIn button2(D4);
    DigitalIn button3(D5);
    DigitalIn button4(D6);
    DigitalIn button5(D7);
    DigitalIn enterButton(BUTTON1);
    button0.mode(PullDown);
    button1.mode(PullDown);
    button2.mode(PullDown);
    button3.mode(PullDown);
    button4.mode(PullDown);
    button5.mode(PullDown);
    greenled = 0;
    redled = 0;
    blueled = 0;
    int wrongatt = 0;
    bool warned = false;
    int lockdownCount = 0;
    while (true) {
        if (button0 && button1 && button2 && button3 && enterButton) {
            ThisThread::sleep_for(300ms);
            greenled = 1;
            redled = 0;
            blueled = 0;
            wrongatt = 0;
            warned = false;
        }
        if (!(button0 && button1 && button2 && button3) && enterButton) {
            ThisThread::sleep_for(300ms);
            greenled = 0;
            redled = 1;
            wrongatt = wrongatt+1;
        }
        if (wrongatt == 3 && !warned) {
            for (int i = 0; i < 30; i++) {
                redled = !redled;
                ThisThread::sleep_for(500ms);
                redled = !redled;
                ThisThread::sleep_for(500ms);
            }
            redled = 0;
            warned = true;
        }
        if (wrongatt == 4 && warned) {
            lockdownCount++;
            redled = 1;
            while (true) {
                if (button2 && button3 && button4 && button5 && enterButton) {
                    ThisThread::sleep_for(300ms);
                    redled = 0;
                    blueled = 0;
                    greenled = 0;
                    for (int i = 0; i < lockdownCount; i++) {
                        greenled = 1;
                        ThisThread::sleep_for(500ms);
                        greenled = 0;
                        ThisThread::sleep_for(500ms);
                    }
                    wrongatt = 0;
                    warned = false;
                    break;
                }
                blueled = !blueled;
                ThisThread::sleep_for(250ms);
            }
        }
    }
}
int main()
{
    code();
}
