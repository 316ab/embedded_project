#include "mbed.h"

// --- Pin Definitions ---
DigitalOut latchPin(PB_5);    // Latch
DigitalOut clockPin(PA_8);    // Shift Clock
DigitalOut dataPin(PA_9);     // Serial Data
DigitalIn  resetBtn(PA_1);    // Reset Button (active low)
DigitalIn  modeBtn(PB_0);     // Mode Button (active low)
AnalogIn   analogPot(PA_0);   // Potentiometer

// --- Segment Encodings (0-9, active-low, common anode) ---
const uint8_t DIGIT_SEGMENTS[10] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 
    0x92, 0x82, 0xF8, 0x80, 0x90
};
// --- Digit Select (active-low) ---
const uint8_t DIGIT_ENABLE[4] = { 0xF1, 0xF2, 0xF4, 0xF8 };

// --- State Variables ---
volatile int elapsedSeconds = 0;
volatile bool displayNeedsUpdate = true;
volatile int activeDigit = 0;

// --- Timers ---
Ticker secondTicker;
Ticker displayTicker;

// --- Mode Enum ---
enum DisplayMode { SHOW_TIME, SHOW_VOLTAGE };

// --- Function Prototypes ---
void initialize();
void incrementSeconds();
void triggerDisplayUpdate();
void shiftOut(uint8_t segments, uint8_t digitSel);
void updateDisplay(DisplayMode mode);

// --- Initialization ---
void initialize() {
    resetBtn.mode(PullUp);
    modeBtn.mode(PullUp);
    secondTicker.attach(&incrementSeconds, 1.0f);      // 1 Hz
    displayTicker.attach(&triggerDisplayUpdate, 0.002); // 2 ms
}

// --- ISR: Increment Time ---
void incrementSeconds() {
    elapsedSeconds = (elapsedSeconds + 1) % 6000; // wrap at 99:59
}

// --- ISR: Set Display Update Flag ---
void triggerDisplayUpdate() {
    displayNeedsUpdate = true;
}

// --- Shift Out Data to 7-Segment Display ---
void shiftOut(uint8_t segments, uint8_t digitSel) {
    latchPin = 0;
    for (int bit = 7; bit >= 0; --bit) {
        dataPin = (segments >> bit) & 1;
        clockPin = 0; clockPin = 1;
    }
    for (int bit = 7; bit >= 0; --bit) {
        dataPin = (digitSel >> bit) & 1;
        clockPin = 0; clockPin = 1;
    }
    latchPin = 1;
}

// --- Display Update Logic ---
void updateDisplay(DisplayMode mode) {
    uint8_t segmentData = 0xFF;
    uint8_t digitSel = 0xFF;

    if (mode == SHOW_TIME) {
        int mins = elapsedSeconds / 60;
        int secs = elapsedSeconds % 60;
        switch (activeDigit) {
            case 0: segmentData = DIGIT_SEGMENTS[mins / 10]; digitSel = DIGIT_ENABLE[0]; break;
            case 1: segmentData = DIGIT_SEGMENTS[mins % 10] & 0x7F; digitSel = DIGIT_ENABLE[1]; break; // Add colon
            case 2: segmentData = DIGIT_SEGMENTS[secs / 10]; digitSel = DIGIT_ENABLE[2]; break;
            case 3: segmentData = DIGIT_SEGMENTS[secs % 10]; digitSel = DIGIT_ENABLE[3]; break;
        }
    } else { // SHOW_VOLTAGE
        float voltage = analogPot.read() * 3.3f;
        int cv = (int)(voltage * 100.0f);
        if (cv > 999) cv = 999;
        int intPart = cv / 100;
        int fracPart = cv % 100;
        switch (activeDigit) {
            case 0: segmentData = DIGIT_SEGMENTS[intPart] & 0x7F; digitSel = DIGIT_ENABLE[0]; break; // Add decimal
            case 1: segmentData = DIGIT_SEGMENTS[fracPart / 10]; digitSel = DIGIT_ENABLE[1]; break;
            case 2: segmentData = DIGIT_SEGMENTS[fracPart % 10]; digitSel = DIGIT_ENABLE[2]; break;
            case 3: segmentData = 0xFF; digitSel = DIGIT_ENABLE[3]; break; // Blank
        }
    }
    shiftOut(segmentData, digitSel);
    activeDigit = (activeDigit + 1) % 4;
}

// --- Main Loop ---
int main() {
    initialize();

    DisplayMode currentMode = SHOW_TIME;
    int prevReset = 1, prevMode = 1;

    while (true) {
        // --- Button Handling ---
        int resetState = resetBtn.read();
        if (resetState == 0 && prevReset == 1) {
            elapsedSeconds = 0;
        }
        prevReset = resetState;

        int modeState = modeBtn.read();
        currentMode = (modeState == 0) ? SHOW_VOLTAGE : SHOW_TIME;
        prevMode = modeState;

        // --- Display Refresh ---
        if (displayNeedsUpdate) {
            displayNeedsUpdate = false;
            updateDisplay(currentMode);
        }
    }
}
