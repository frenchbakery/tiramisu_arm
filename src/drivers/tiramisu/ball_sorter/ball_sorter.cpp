/**
 * @file ball_sorter.hpp
 * @author Nilusink
 * @version 1.0
 * @date 2023-03-07
 * 
 * @copyright Copyright frenchbakery (c) 2023
 * 
 */
#include <kipr/time/time.h>
#include "ball_sorter.hpp"


BallSorter::BallSorter(int motor_pin, int servo_pin, int switch_pin)
    : turn_motor(motor_pin), push_servo(servo_pin, initial_servo_pos), end_switch(switch_pin)
{
    push_servo.enable();
};


void BallSorter::calibrate()
{
    // reset servo
    resetPusher();

    // reset motor
    raise();
}


void BallSorter::toDeck()
{
    // adjust motor position
    turn_motor.enablePositionControl();
    turn_motor.setAbsoluteTarget(motor_down);
}

void BallSorter::toDropPosition()
{
    // adjust motor position
    turn_motor.enablePositionControl();
    turn_motor.setAbsoluteTarget(motor_drop);
}

void BallSorter::raise()
{
    // motor on until end switch
    turn_motor.moveAtVelocity(-calibrate_speed);

    while (!end_switch.value()) { msleep(10); };
    turn_motor.off();

    turn_motor.clearPositionCounter();
}


void BallSorter::pushBall(bool reset)
{
    push_servo.setPosition(servo_up);

    if (reset)
    {
        push_servo.waitUntilComleted();
        resetPusher();
    }
}

void BallSorter::resetPusher()
{
    push_servo.setPosition(initial_servo_pos);
}


void BallSorter::setServoSpeed(int speed)
{
    push_servo.setSpeed(speed);
}