#ifndef HH_SDLPLAYER_H
#define HH_SDLPLAYER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <iostream>
#include <vector>


class SDLPlayer {
public:
    SDLPlayer(int videoWidth, int videoHeight);
    ~SDLPlayer();

    void Play();

    void ProcessVideo(uint8_t* buffer, int bufSize);
    void ProcessAudio(uint8_t* buffer, int bufSize);

private:
    void InitDecoder();
    
    void PlayThreadFunc();
    void VideoThreadFunc();
    void AudioThreadFunc();

    void DecodeAndQueueVideo(const std::vector<uint8_t>& data);
    void DecodeAndQueueAudio(const std::vector<uint8_t>& data);

    void HandleEvents();
    void Render();

    // 成员变量和方法
    SDL_AudioSpec audioSpec;
    SDL_AudioDeviceID audioDevice;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    std::queue<std::vector<uint8_t>> audioQueue;
    std::queue<std::vector<uint8_t>> videoQueue;
   // std::queue<std::vector<uint8_t>> renderQueue;
    std::queue<AVFrame*> renderQueue;
    std::mutex audioMutex;
    std::mutex videoMutex;
    std::mutex renderMutex;
    std::condition_variable audioCV;
    std::condition_variable videoCV;
    std::atomic<bool> quit{ false };
    const int videoWidth;
    const int videoHeight;
    AVCodecContext* audioCodecContext = nullptr;
    AVCodecContext* videoCodecContext = nullptr;
    //SwrContext* swrContext = nullptr;
    //SwsContext* swsContext = nullptr;
    std::thread audioThread;
    std::thread videoThread;
    std::thread playThread;
};

#endif // HH_SDLPLAYER_H
