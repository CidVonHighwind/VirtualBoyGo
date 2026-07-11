#pragma once

#include <sys/types.h>
#include <string>

namespace ButtonMapper {

    struct MappedButton {
        bool IsSet = false;
        // Gamepad = 0; LTouch = 1; RTouch = 2
        int InputDevice;
        int ButtonIndex;
    };

    struct MappedButtons {
        MappedButton Buttons[2];
    };

    const int DeviceGamepad =       0;
    const int DeviceLeftTouch =     1;
    const int DeviceRightTouch =    2;

    const uint32_t EmuButton_A =                0;
    const uint32_t EmuButton_B =                1;
    const uint32_t EmuButton_RThumb =           2;
    const uint32_t EmuButton_RShoulder =        3;

    const uint32_t EmuButton_LeftStickUp =      4;
    const uint32_t EmuButton_LeftStickDown =    5;
    const uint32_t EmuButton_LeftStickLeft =    6;
    const uint32_t EmuButton_LeftStickRight =   7;

    const uint32_t EmuButton_X =                8;
    const uint32_t EmuButton_Y =                9;
    const uint32_t EmuButton_LThumb =           10;
    const uint32_t EmuButton_LShoulder =        11;

    const uint32_t EmuButton_U1 =               12;
    const uint32_t EmuButton_U2 =               13;
    const uint32_t EmuButton_U3 =               14;
    const uint32_t EmuButton_U4 =               15;

    const uint32_t EmuButton_Up =               16;
    const uint32_t EmuButton_Down =             17;
    const uint32_t EmuButton_Left =             18;
    const uint32_t EmuButton_Right =            19;

    const uint32_t EmuButton_Enter =            20;
    const uint32_t EmuButton_Back =             21;
    const uint32_t EmuButton_RightStickUp =     22;
    const uint32_t EmuButton_RightStickDown =   23;

    const uint32_t EmuButton_RightStickLeft =   24;
    const uint32_t EmuButton_RightStickRight =  25;
    const uint32_t EmuButton_GripTrigger =      26;
    const uint32_t EmuButton_L2 =               27;

    const uint32_t EmuButton_R2 =               28;
    const uint32_t EmuButton_Trigger =          29;
    const uint32_t EmuButton_U5 =               30;
    const uint32_t EmuButton_Joystick =         31;

    const int EmuButtonCount = 32;
    const uint32_t ButtonMapping[] = {
            0x00000001, 0x00000002, 0x00000004, 0x00000008,
            0x00000010, 0x00000020, 0x00000040, 0x00000080,
            0x00000100, 0x00000200, 0x00000400, 0x00000800,
            0x00001000, 0x00002000, 0x00004000, 0x00008000,
            0x00010000, 0x00020000, 0x00040000, 0x00080000,
            0x00100000, 0x00200000, 0x00400000, 0x00800000,
            0x01000000, 0x02000000, 0x04000000, 0x08000000,
            0x10000000, 0x20000000, 0x40000000, 0x80000000,};

    // gamepad button names; ltouch button names; rtouch button names
    const std::string MapButtonStr[32 * 3] = {
                                        "A", "B", "RThumb", "RBumper",
                                        "LStick-Up", "LStick-Down", "LStick-Left", "LStick-Right",
                                        "X", "Y", "LThumb", "LBumper",
                                        "B1", "B2", "B3", "B4",
                                        "Up", "Down", "Left", "Right",
                                        "Enter", "Back", "RStick-Up", "RStick-Down",
                                        "RStick-Left", "RStick-Right", "GripTrigger", "L2",
                                        "R2", "Trigger", "B5", "Joystick",

                                        "LTouch-A", "LTouch-B", "LTouch-RThumb", "LTouch-RShoulder",
                                        "L1", "L2", "L3", "L4",
                                        "LTouch-X", "LTouch-Y", "LTouch-LThumb", "LTouch-LShoulder",
                                        "L5", "L6", "L7", "L8",
                                        "LTouch-Up", "LTouch-Down", "LTouch-Left", "LTouch-Right",
                                        "LTouch-Enter", "LTouch-Back", "L9", "L10",
                                        "L11", "L12", "LTouch-GripTrigger", "L13",
                                        "L14", "LTouch-Trigger", "L15", "LTouch-Joystick",

                                        "RTouch-A", "RTouch-B", "RTouch-RThumb", "RTouch-RShoulder",
                                        "R1", "R2", "R3", "R4",
                                        "RTouch-X", "RTouch-Y", "RTouch-LThumb", "RTouch-LShoulder",
                                        "R5", "R6", "R7", "R8",
                                        "RTouch-Up", "RTouch-Down", "RTouch-Left", "RTouch-Right",
                                        "RTouch-Enter", "RTouch-Back", "R9", "R10",
                                        "R11", "R12", "RTouch-GripTrigger", "R13",
                                        "R14", "RTouch-Trigger", "R15", "RTouch-Joystick"};
}