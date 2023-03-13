/**
 * @file ball_sorter.hpp
 * @author Nilusink
 * @brief driver for the ball sorter
 * @version 1.0
 * @date 2023-03-07
 * 
 * @copyright Copyright frenchbakery (c) 2023
 * 
 */
#include <kipr/digital/digital.hpp>
#include <kiprplus/smooth_servo.hpp>
#include <kiprplus/pid_motor.hpp>


class BallSorter
{
    public:
        typedef kp::PIDMotor Motor;
        typedef kp::SmoothServo Servo;
        typedef kipr::digital::Digital Button;

    protected:
        Motor turn_motor;
        Servo push_servo;
        Button end_switch;

        const int calibrate_speed = 200;

        // positions
        const int initial_servo_pos = 0;
        const int servo_up = 0;

        const int motor_down = 0;
        const int motor_drop = 0;

    public:
        BallSorter(int motor_pin, int servo_pin, int switch_pin);

        void calibrate();

        /**
         * @brief drop sorter to deck
         * 
         */
        void toDeck();

        /**
         * @brief set sorter to ball drop position
         * 
         */
        void toDropPosition();

        /**
         * @brief completely raise the sorter
         * 
         */
        void raise();

        /**
         * @brief push the red ball
         * 
         * @param reset reset pusher on finish
         */
        void pushBall(bool reset=false);

        /**
         * @brief reset the pusher
         * 
         */
        void resetPusher();

        /**
         * @brief set the servo speed
         * 
         * @param speed servo ticks per second
         */
        void setServoSpeed(int speed);

        // void setMotorSpeed(int speed);  // not implemented by melektron yet
};