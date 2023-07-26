/************************************************************************************

Filename    :   TalkToJava.cpp
Content     :   Thread and JNI management for making java calls in the background
Created     :   February 26, 2014
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "TalkToJava.h"

#include <stdlib.h>
#include <string.h>

#include "Misc/Log.h"

#include "OVR_Std.h"

namespace OVRFW {

void TalkToJava::Init(JavaVM& javaVM_, TalkToJavaInterface& javaIF_) {
    Jvm = &javaVM_;
    Interface = &javaIF_;

    // spawn the VR thread
    TtjThread = std::thread(&TalkToJava::TtjThreadFunction, this);
}

TalkToJava::~TalkToJava() {
    if (TtjThread.joinable()) {
        // Get the background thread to kill itself.
        ALOG("TtjMessageQueue.PostPrintf( quit )");
        TtjMessageQueue.PostPrintf("quit");
        TtjThread.join();
    }
}

/*
 * TtjThreadFunction
 *
 * Continuously waits for command messages and processes them.
 */
void TalkToJava::TtjThreadFunction() {
    // The Java VM needs to be attached on each thread that will use it.
    ALOG("TalkToJava: Jvm->AttachCurrentThread");
    ovr_AttachCurrentThread(Jvm, &Jni, NULL);

    // Process all queued messages
    for (;;) {
        const char* msg = TtjMessageQueue.GetNextMessage();
        if (!msg) {
            // Go dormant until something else arrives.
            TtjMessageQueue.SleepUntilMessage();
            continue;
        }

        if (strcmp(msg, "quit") == 0) {
            free((void*)msg);
            break;
        }

        // Set up a local frame with room for at least 100
        // local references that will be auto-freed.
        Jni->PushLocalFrame(100);

        // Let whoever initialized us do what they want.
        Interface->TtjCommand(*Jni, msg);

        // If we don't clean up exceptions now, later
        // calls may fail.
        if (Jni->ExceptionOccurred()) {
            Jni->ExceptionClear();
            ALOG("JNI exception after: %s", msg);
        }

        // Free any local references
        Jni->PopLocalFrame(NULL);

        free((void*)msg);
    }

    ALOG("TalkToJava: Jvm->DetachCurrentThread");
    ovr_DetachCurrentThread(Jvm);
}

} // namespace OVRFW
