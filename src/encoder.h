#include <Arduino.h>
#include <Bounce2.h>

enum RotationDirection {
    NONE,
    CLOCKWISE,
    COUNTERCLOCKWISE
};

class RotaryEncoder
{

private:
    int CLK;
    int DT;
    int SW;
    Bounce bounce;

    int CLK_value = 0;
    int prev_CLK_value = 0;
    
    RotationDirection direction;

public:
    RotaryEncoder(int clk, int dt, int sw)
        : CLK(clk), DT(dt), SW(sw) {
        pinMode(CLK, INPUT);
        pinMode(DT, INPUT);
        pinMode(SW, INPUT);

        bounce.attach(SW);

        prev_CLK_value = digitalRead(CLK);
    }

    void loop() {
        bounce.update();

        CLK_value = digitalRead(CLK);

        if (CLK_value != prev_CLK_value && CLK_value == HIGH) {
            direction = digitalRead(DT) ? COUNTERCLOCKWISE : CLOCKWISE;
        } else {
            direction = NONE;
        }

        prev_CLK_value = CLK_value;
    }

    bool isPressed() {
        return bounce.fell();
    }

    RotationDirection getDirection() {
        return this->direction;
    }
};