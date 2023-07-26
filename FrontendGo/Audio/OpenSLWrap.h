#pragma once

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define BUFFER_SIZE 10000

class OpenSLWrapper {

private:
    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    SLObjectItf outputMixObject;

    // buffer queue player interfaces
    SLObjectItf bqPlayerObject = NULL;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLVolumeItf bqPlayerVolume;

    // Double buffering.
    short empytBuffer[10000]{0};
    short buffer[5][BUFFER_SIZE]{{0}};
    SLuint32 bufferSize[5];

    int curBuffer = 0;

    int lastBuffer = 0;

public:
    void Init();

    void SetBuffer(unsigned short *audio, unsigned sampleCount);

    void StartPlaying();

    void Shutdown();
};