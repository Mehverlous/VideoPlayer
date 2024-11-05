#include <stdio.h>
#include <stdlib.h>

#include <portaudio.h>

static void checkError(PaError err){
    if(err != paNoError){
        printf("PortAudio error: %s", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}

int main(){
    PaError err; 
    err = Pa_Initialize();
    checkError(err);

    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0){
        printf("Error getting device count\n");
        exit(EXIT_FAILURE);
    }else if (numDevices == 0){
        printf("There are no available audio devices on this machine");
        exit(EXIT_SUCCESS);
    }

    const PaDeviceInfo* deviceInfo;
    for (int i = 0; i < numDevices; i++){
        deviceInfo = Pa_GetDeviceInfo(i);
        printf("Device: %d \n", i);
        printf("    name: %s\n", deviceInfo->name);
        printf("    maxInputChannels: %d\n", deviceInfo->maxInputChannels);
        printf("    maxOutputChannels: %d\n", deviceInfo->maxOutputChannels);
        printf("    defaultSampleRate: %f\n", deviceInfo->defaultSampleRate);
    }

    err = Pa_Terminate();
    checkError(err);

    return 0;
}