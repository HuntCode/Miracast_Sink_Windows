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

class MiracastSink;
class SDLPlayer {
public:
    SDLPlayer(int videoWidth, int videoHeight, MiracastSink* sink);
    ~SDLPlayer();

    void Init(const std::string & name);
    void Play();

    void ProcessVideo(uint8_t* buffer, int bufSize);
    void ProcessAudio(uint8_t* buffer, int bufSize);

private:
    void InitDecoder();
    
    void VideoThreadFunc();
    void AudioThreadFunc();

    void DecodeAndQueueVideo(const std::vector<uint8_t>& data);
    void DecodeAndQueueAudio(const std::vector<uint8_t>& data);

    void HandleEvents();
    void OnMouseDown(const SDL_MouseButtonEvent& buttonEvent);
    void OnMouseUp(const SDL_MouseButtonEvent& buttonEvent);
    void OnMouseMove(const SDL_MouseMotionEvent& motionEvent);
    void OnKeyDown(const SDL_KeyboardEvent& keyEvent);
    void OnKeyUp(const SDL_KeyboardEvent& keyEvent);
    unsigned short KeyCodeToKeyValue(const SDL_Keycode& code);
    void Render();

    MiracastSink* m_sink;
    int last_x = 0, last_y = 0;
    // 成员变量和方法
    SDL_AudioSpec audioSpec;
    SDL_AudioDeviceID audioDevice;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    std::queue<std::vector<uint8_t>> audioQueue;
    std::queue<std::vector<uint8_t>> videoQueue;

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

    std::thread audioThread;
    std::thread videoThread;
};

#endif // HH_SDLPLAYER_H
