#include <kipr/botball/botball.h>
#include <kipr/wait_for/wait_for.h>
#include <kipr/analog/analog.hpp>
#include <kipr/create/create.h>
#include <kipr/camera/camera.h>
#include <kipr/time/time.h>
#include <iostream>
#include <thread>
#include "drivers/tiramisu/ball_sorter/ball_sorter.hpp"
#include "drivers/tiramisu/arm/arm.hpp"
#include "drivers/tiramisu/cam_funcs/cam_funcs.hpp"
#include "routines/create.hpp"
#include "global_objects.hpp"
#include "term_colors.h"
#include "player.hpp"

#define BALL_MOTOR_PORT 1
#define BALL_SERVO_PIN 0
#define BALL_END_PIN 9

#define ARM_ELLBOW_PIN 0
#define ARM_SHOULDER_PIN 3 // + up
#define ARM_GRAB_PIN 1 // 2047 closed
#define ARM_WRIST_PIN 2 // 0 up
#define ARM_MAX_PIN 8
#define ARM_MIN_PIN 7
#define LIGHT_PIN 5
#define DIST_PIN 0
#define LINE_PIN 0

#define CAM_LOOP_N 20

#define LIGHT_CALIB_ACCURACY 100
int ambient_light = -1;
int light_range = -1;


namespace go
{
    kipr::analog::Analog *line;
    BallSorter *balls;
    TINav *nav;
    Arm *arm;
}



int main()
{
    int current_stack = 0;
    go::nav = new TINav;
    go::nav->initialize();

    go::line = new kipr::analog::Analog(LINE_PIN);

    camera_open_device_model_at_res(0, BLACK_2017, Resolution::MED_RES);
    camera_load_config("cubes.conf");

    double bat_perc;
    {
        std::lock_guard lock(kp::CreateMotor::create_access_mutex);
        bat_perc = 100 * ((float)get_create_battery_charge() / get_create_battery_capacity());
    }
    printf("create battery: %s%.1lf%%%s\n", (bat_perc > 30 ? CLR_GREEN : CLR_RED), bat_perc, CLR_RESET);

    go::balls = new BallSorter(BALL_MOTOR_PORT, BALL_SERVO_PIN, BALL_END_PIN);

    kipr::digital::Digital dist(DIST_PIN);
    kipr::analog::Analog light(LIGHT_PIN);

    go::arm = new Arm(
        ARM_ELLBOW_PIN,
        ARM_SHOULDER_PIN,
        ARM_WRIST_PIN,
        ARM_GRAB_PIN,
        ARM_MAX_PIN,
        ARM_MIN_PIN
    );

    std::cout << "initialized\n";


    std::thread bal_cal_thread(&BallSorter::calibrate, go::balls);
    std::cout << "thread on\n";
    go::arm->moveGripperTo(40);
    go::arm->calibrate();

    
    // home create
    align_wall();
    go::nav->driveDistance(-5);
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();

    reset_position();

    go::arm->moveGripperTo(50);

    std::cout << CLR_BLUE << "calibrated" << CLR_RESET << std::endl;
    go::arm->park();

    std::cout << CLR_BLUE << "parked" << CLR_RESET << std::endl;

    // calibrate ligtht sensor
    int sum = 0;
    int now_val;
    int max_value = 1;
    int min_value = 4086;
    for (int sensor_value_index = 0; sensor_value_index < LIGHT_CALIB_ACCURACY; sensor_value_index++)
    {
        now_val = light.value();
        sum += now_val;
        max_value = std::max(now_val, max_value);
        min_value = std::min(now_val, min_value);

        msleep(20);
    }
    ambient_light = sum / LIGHT_CALIB_ACCURACY;  // average light value
    light_range = max_value - min_value;

    // wait for threads
    if (bal_cal_thread.joinable())
        bal_cal_thread.join();

    // while (light.value() > ambient_light - (light_range * .7)) msleep(10);
    int trash;
    std::cout << CLR_GREEN << "start? " << CLR_RESET;
    std::cin.get();
    std::cout << std::endl;

    // prepare to shutdown
    int start_time = seconds();
    shut_down_in(115);

    go::arm->moveWristToRelativeAngle(90);
    msleep(2000);
    go::arm->moveGripperTo(95);
    msleep(2000);

    sequences::remove_first_pom();
    go::nav->startSequence();

    std::cout << CLR_BLUE << "waiting for pom sequence" << CLR_RESET << std::endl;
    msleep(5500);

    go::arm->moveShoulderToAngle(go::arm->shoulder_90);
    go::arm->moveEllbowTo(70);
    go::arm->moveGripperTo(90);
    go::arm->moveWristToRelativeAngle(90);

    msleep(7000);
    go::arm->moveEllbowTo(97);
    go::arm->moveGripperTo(0);
    go::arm->moveWristToRelativeAngle(-57);
    go::arm->awaitAllDone();

    go::nav->awaitSequenceComplete();
    go::arm->awaitAllDone();

    // rotate to cube
    double off_a;
    for (int i = 0; i < CAM_LOOP_N; i++)
    {
        off_a = Cam::look_at(0);
        if (off_a != 69420.f)
            break;
    }

    if (off_a == 69420.f)
    {   // cube not found
        go::arm->moveGripperTo(100);
        std::cout << CLR_RED << "Cube 1 not found!" << CLR_RESET << std::endl;

        // std::cout << CLR_GREEN << "MATSE: " << CLR_RESET;
        // std::cin >> trash;
        // std::cout << std::endl;

        sequences::yellow_cubes::alternate_drop_first_cube();
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();
    }
    else
    {   // cube found
        std::cout << "moving by " << off_a * (180 / M_PI) << "°\n";

        go::nav->rotateBy(off_a);
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();
        std::cout << "moved by " << off_a * (180 / M_PI) << "°\n";

        go::nav->driveDistance(12);
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();

        std::cout << "gripping\n";

        go::arm->moveGripperTo(100);;
        go::arm->awaitGripperDone();

        msleep(200);

        go::arm->moveWristToRelativeAngle(0);
        go::arm->awaitWristDone();

        go::nav->driveDistance(-12);
        go::nav->rotateBy(-off_a);
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();

        // std::cout << CLR_GREEN << "MATSE: " << CLR_RESET;
        // std::cin >> trash;
        // std::cout << std::endl;

        sequences::yellow_cubes::alternate_drop_first_cube();
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();

        // sequences::yellow_cubes::drop_first_cube();
        // std::cout << CLR_BLUE << "waiting for cube drop 1 sequence" << CLR_RESET << std::endl;
        // go::nav->startSequence();
        // go::nav->awaitSequenceComplete();

        // sequences::yellow_cubes::stack_cube(current_stack);
        // current_stack++;
   }


    sequences::balls_pickup_position();

    // wait for 60 seconds
    std::cout << "\e[?25l";
    int delta;
    for (;;)
    {
        delta = seconds() - start_time;
        if (delta >= 60)
            break;
        
        std::cout << "naggl in: " << 60 - delta << "          \r";
        msleep(20);

    }
    std::cout << "\e[?25h";

    sequences::funktion_punkt_naggl();
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();

    std::cout << CLR_GREEN << "continue? " << CLR_RESET;
    std::cin.get();
    std::cout << std::endl;

    sequences::balls_drop_position();

    // prepare ball sorter
    go::balls->toDropPosition();
    go::balls->waitForMotor();

    // rotate over the tube
    go::nav->rotateBy(M_PI_4);
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();

    // drop ball
    go::balls->pushBall(false);
    go::balls->waitForServo();

    // move back a bit
    go::nav->driveDistance(-10);
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();

    // put pusher down and push tube into area
    go::balls->toDeck();
    go::balls->waitForMotor();
    go::nav->driveDistance(40);
    go::nav->driveDistance(-20);

    // drop other balls in area
    go::nav->rotateBy(-M_PI_4);
    go::nav->driveDistance(20);
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();
    go::balls->resetPusher();
    go::balls->waitForMotor();

    return 0;

    go::arm->moveShoulderTo(5);
    go::arm->moveEllbowTo(50);
    go::arm->moveWristToRelativeAngle(90);
    go::arm->awaitAllDone();
    go::arm->moveGripperTo(100);

    sequences::yellow_cubes::goto_second_cube();
    std::cout << CLR_BLUE << "waiting for cube 2 sequence" << CLR_RESET << std::endl;
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();

    go::arm->unpark();
    go::arm->moveGripperTo(0);
    go::arm->awaitAllDone();

    // rotate to cube
    for (int i = 0; i < CAM_LOOP_N; i++)
    {
        off_a = Cam::look_at(0);
        if (off_a != 69420.f)
            break;
    }

    if (off_a == 69420.f)
    {   // cube not found
        std::cout << CLR_RED << "Cube 2 not found!" << CLR_RESET << std::endl;
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();
    }
    else
    {   // cube found
        std::cout << "moving by " << off_a * (180 / M_PI) << "°\n";

        go::nav->rotateBy(off_a);
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();
        std::cout << "moved by " << off_a * (180 / M_PI) << "°\n";

        go::nav->driveDistance(18);
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();

        std::cout << "gripping\n";

        go::arm->moveGripperTo(95);;
        go::arm->awaitGripperDone();

        msleep(200);


        go::arm->moveWristToRelativeAngle(30);
        go::arm->awaitWristDone();

        go::nav->rotateBy(-off_a);
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();

        sequences::yellow_cubes::drop_second_cube();
        std::cout << CLR_BLUE << "waiting for cube drop 2 sequence" << CLR_RESET << std::endl;
        go::nav->startSequence();
        go::nav->awaitSequenceComplete();

        sequences::yellow_cubes::stack_cube(current_stack);
        current_stack++;
    }

    go::arm->awaitAllDone();
    go::nav->startSequence();
    go::nav->awaitSequenceComplete();

    // go::arm->awaitAllDone();

    // go::arm->moveShoulderToAngle(110);
    // go::arm->moveEllbowTo(30);
    // go::arm->moveWristToRelativeAngle(-20);

    // if (1)
    // {
    //     create_full();
    //     Cam::look_at(0);
    //     create_drive_straight(40);
    //     while (dist.value() < 1800)
    //     {
    //         std::cout << "dist: " << dist.value() <<  "\n";
    //         create_drive_straight(50);
    //         Cam::look_at(YELLOW_CHANNEL, true);
    //     }

    //     create_drive_straight(0);
    //     go::arm->moveGripperTo(90);
    //     go::arm->awaitGripperDone();

    //     go::arm->moveWristToRelativeAngle(30);
    //     go::arm->moveEllbowTo(50);

    //     msleep(200);

    //     create_drive_straight(-500);
    //     msleep(3000);

    //     create_drive_straight(0);
    //     go::arm->awaitAllDone();

    //     create_stop();
    // }
    // else
    // {
    //     std::cout << "couldn't connect to create";
    // }
    create_disconnect();

    return 0;
}
