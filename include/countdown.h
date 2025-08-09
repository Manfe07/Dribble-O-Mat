#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include <Arduino.h>

class Countdown
{
private:
    unsigned long duration; // Countdown-Dauer in ms
    unsigned long endTime;  // Zeitpunkt, wann der Countdown endet
    bool running;

public:
    Countdown(unsigned long durationMs);

    void set(unsigned long ms);
    void start();
    void stop();
    bool isRunning();
    unsigned long timeLeft();
};


Countdown::Countdown(unsigned long durationMs)
{
    Countdown::duration = durationMs;
    Countdown::endTime = 0;
    Countdown::running = false;
}


void Countdown::set(unsigned long ms)
{
    Countdown::duration = ms;
    if (running)
        Countdown::endTime = millis() + Countdown::duration;
}


void Countdown::start()
{
    Countdown::endTime = millis() + Countdown::duration;
    running = true;
}


void Countdown::stop()
{
    running = false;
}


bool Countdown::isRunning()
{
    if (Countdown::running){
        if (Countdown::endTime <= millis())
        {
            Countdown::running = false; // Countdown has ended
            return false;
        }        
    }
    return Countdown::running;
}


unsigned long Countdown::timeLeft()
{
    if (!Countdown::running)
        return 0;
    unsigned long timeLeft = Countdown::endTime - millis();
    return (timeLeft > 250) ? timeLeft : 0;
}   

#endif