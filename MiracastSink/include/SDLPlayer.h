#ifdef USE_SDL_PLAYER

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

// 自定义事件结构体
struct CreateWindowEvent {
    //const char* title;
    //int x, y, w, h;
    int w;
    int h;
};

class MiracastDevice;
class SDLPlayer {
public:
    SDLPlayer(MiracastDevice* device);
    ~SDLPlayer();

    void Init(const std::string & name);
    void Play();
    void Stop();

    void ProcessVideo(uint8_t* buffer, int bufSize);
    void ProcessAudio(uint8_t* buffer, int bufSize);

private:
    void InitDecoder();
    void VideoThreadFunc();
    void AudioThreadFunc();
    void DecodeAndQueueVideo(const std::vector<uint8_t>& data);
    void DecodeAndQueueAudio(const std::vector<uint8_t>& data);

    void ClearQueue(std::queue<AVFrame*>& queue);

    void CreateWindowAndRenderer(int width, int height);

    void PushCustomEvent(CreateWindowEvent event);
    void HandleCustomEvents();

    bool IsEventForWindow(const SDL_Event& e, SDL_Window* window);
    void HandleEvents();
    void OnMouseDown(const SDL_MouseButtonEvent& buttonEvent);
    void OnMouseUp(const SDL_MouseButtonEvent& buttonEvent);
    void OnMouseMove(const SDL_MouseMotionEvent& motionEvent);
    void OnMouseWheel(const SDL_MouseWheelEvent& wheelEvent);
    void OnKeyDown(const SDL_KeyboardEvent& keyEvent);
    void OnKeyUp(const SDL_KeyboardEvent& keyEvent);
    unsigned short KeyCodeToKeyValue(const SDL_Keycode& code);
    void Render();

    static std::atomic<int> s_instanceCount;
    std::mutex m_initMutex;

    std::string m_winName;
    MiracastDevice* m_device;
    int m_lastX = 0, m_lastY = 0;
    // 成员变量和方法
    SDL_AudioSpec m_audioSpec;
    SDL_AudioDeviceID m_audioDevice;
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
    std::queue<std::vector<uint8_t>> m_audioQueue;
    std::queue<std::vector<uint8_t>> m_videoQueue;

    std::queue<AVFrame*> m_renderQueue;
    std::mutex m_audioMutex;
    std::mutex m_videoMutex;
    std::mutex m_renderMutex;
    std::condition_variable m_audioCV;
    std::condition_variable m_videoCV;
    std::atomic<bool> m_quit{ false };
    int m_videoWidth;
    int m_videoHeight;
    AVCodecContext* m_audioCodecContext = nullptr;
    AVCodecContext* m_videoCodecContext = nullptr;

    std::thread m_audioThread;
    std::thread m_videoThread;

    std::queue<CreateWindowEvent> m_customEventQueue;
    std::mutex m_queueMutex;
};

#endif // HH_SDLPLAYER_H

#endif // USE_SDL_PLAYER