#include "pch.h"
#include "SDLPlayer.h"
#include "MiracastSink.h"

#define windowWidth 960
#define windowHeight 570
#define MY_DEFINE_REFRESH_EVENT     (SDL_USEREVENT + 1)

std::atomic<int> SDLPlayer::s_instanceCount = 0;

SDLPlayer::SDLPlayer(int videoWidth, int videoHeight, std::shared_ptr<MiracastSink> sink)
    : m_sink(sink), videoWidth(videoWidth), videoHeight(videoHeight)
{
    {
        std::lock_guard<std::mutex> lock(m_initMutex);
        if (s_instanceCount == 0) {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
                std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
                return;
            }
        }
        ++s_instanceCount;
    }
}

SDLPlayer::~SDLPlayer()
{
    Stop();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(audioDevice);

    {
        std::lock_guard<std::mutex> lock(m_initMutex);
        if (--s_instanceCount == 0) {
            SDL_Quit();
        }
    }

    avcodec_free_context(&audioCodecContext);
    avcodec_free_context(&videoCodecContext);
}

void SDLPlayer::Init(const std::string& name)
{
    // 创建音频设备
    audioSpec.freq = 44100;
    audioSpec.format = AUDIO_S16SYS;
    audioSpec.channels = 2;
    audioSpec.samples = 1024;
    audioSpec.callback = nullptr;
    audioSpec.userdata = nullptr;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
    if (audioDevice == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_PauseAudioDevice(audioDevice, 0); // 恢复音频设备播放

    // 创建窗口、渲染器和纹理
    window = SDL_CreateWindow(name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        videoWidth, videoHeight);
    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return;
    }

    // 初始化解码器
    InitDecoder();

    // 启动处理线程
    audioThread = std::thread(&SDLPlayer::AudioThreadFunc, this);
    videoThread = std::thread(&SDLPlayer::VideoThreadFunc, this);
}

void SDLPlayer::Play()
{
    quit = false;
    // SDL main loop
    while (!quit) {
        HandleEvents();
        Render();
        SDL_Delay(10); // Delay to reduce CPU usage
    }
}

void SDLPlayer::Stop()
{
    quit = true;
    audioCV.notify_all();
    videoCV.notify_all();

    if (audioThread.joinable()) audioThread.join();
    if (videoThread.joinable()) videoThread.join();
}

void SDLPlayer::ProcessVideo(uint8_t* buffer, int bufSize)
{
    {
        std::lock_guard<std::mutex> lock(videoMutex);
        videoQueue.push(std::vector<uint8_t>(buffer, buffer + bufSize));
    }
    videoCV.notify_one();
}

void SDLPlayer::ProcessAudio(uint8_t* buffer, int bufSize)
{
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        audioQueue.push(std::vector<uint8_t>(buffer, buffer + bufSize));
    }
    audioCV.notify_one();
}

void SDLPlayer::InitDecoder()
{
    // 初始化音频解码器
    const AVCodec* audioCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    audioCodecContext = avcodec_alloc_context3(audioCodec);
    avcodec_open2(audioCodecContext, audioCodec, nullptr);
    if (audioCodecContext->sample_rate == 0) {
        audioCodecContext->sample_rate = 44100;
        //audioCodecContext->channels = 2;
        //audioCodecContext->channel_layout = av_get_default_channel_layout(2);
    }

    // 初始化视频解码器
    const AVCodec* videoCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    videoCodecContext = avcodec_alloc_context3(videoCodec);
    avcodec_open2(videoCodecContext, videoCodec, nullptr);

}

void SDLPlayer::VideoThreadFunc()
{
    while (!quit) {
        std::vector<uint8_t> data;
        {
            std::unique_lock<std::mutex> lock(videoMutex);
            videoCV.wait(lock, [this] { return !videoQueue.empty() || quit; });
            if (!videoQueue.empty()) {
                data = std::move(videoQueue.front());
                videoQueue.pop();
            }
        }

        if (!data.empty()) {
            DecodeAndQueueVideo(data);
        }
    }
}

void SDLPlayer::AudioThreadFunc()
{
    while (!quit) {
        std::vector<uint8_t> data;
        {
            std::unique_lock<std::mutex> lock(audioMutex);
            audioCV.wait(lock, [this] { return !audioQueue.empty() || quit; });

            if (!audioQueue.empty()) {
                data = std::move(audioQueue.front());
                audioQueue.pop();
            }
        }

        if (!data.empty()) {
            DecodeAndQueueAudio(data);
        }
    }
}

void SDLPlayer::DecodeAndQueueVideo(const std::vector<uint8_t>& data)
{
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = const_cast<uint8_t*>(data.data());
    packet.size = data.size();

    if (avcodec_send_packet(videoCodecContext, &packet) >= 0) {
        AVFrame* frame = av_frame_alloc();
        if (avcodec_receive_frame(videoCodecContext, frame) >= 0) {
            std::lock_guard<std::mutex> lock(renderMutex);
            renderQueue.push(frame);
        }
    }
    av_packet_unref(&packet);
}

void SDLPlayer::DecodeAndQueueAudio(const std::vector<uint8_t>& data)
{
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = const_cast<uint8_t*>(data.data());
    packet.size = data.size();

    if (avcodec_send_packet(audioCodecContext, &packet) >= 0) {
        AVFrame* frame = av_frame_alloc();
        if (avcodec_receive_frame(audioCodecContext, frame) >= 0) {
            // 音频重采样
            SwrContext* swrCtx = swr_alloc_set_opts(nullptr,
                AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, frame->sample_rate,
                frame->channel_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                0, nullptr);
            swr_init(swrCtx);

            int outSamples = av_rescale_rnd(swr_get_delay(swrCtx, frame->sample_rate) + frame->nb_samples, frame->sample_rate, frame->sample_rate, AV_ROUND_UP);
            uint8_t* output[2] = { nullptr };
            av_samples_alloc(output, nullptr, 2, outSamples, AV_SAMPLE_FMT_S16, 0);

            int convertedSamples = swr_convert(swrCtx, output, outSamples, (const uint8_t**)frame->data, frame->nb_samples);
            int dataSize = av_samples_get_buffer_size(nullptr, 2, convertedSamples, AV_SAMPLE_FMT_S16, 1);
            std::vector<uint8_t> audioBuffer(dataSize);
            memcpy(audioBuffer.data(), output[0], dataSize);
            SDL_QueueAudio(audioDevice, audioBuffer.data(), audioBuffer.size());

            av_freep(&output[0]);
            swr_free(&swrCtx);
        }
        av_frame_free(&frame);
    }
    av_packet_unref(&packet);
}

// 判断事件是否属于指定窗口
bool SDLPlayer::IsEventForWindow(const SDL_Event& e, SDL_Window* window) 
{
    return e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP
        ? e.motion.windowID == SDL_GetWindowID(window)
        : e.window.windowID == SDL_GetWindowID(window);
}

void SDLPlayer::HandleEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        //std::cout << "event.type " << event.type << std::endl;
        switch (event.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                quit = true;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            OnMouseDown(event.button);
            break;
        case SDL_MOUSEBUTTONUP:
            OnMouseUp(event.button);
            break;
        case SDL_MOUSEMOTION:
            if(IsEventForWindow(event, window))
                OnMouseMove(event.motion);
            break;
        case SDL_KEYDOWN:
            OnKeyDown(event.key);
            break;
        case SDL_KEYUP:
            OnKeyUp(event.key);
            break;
        default:
            break;
        }
    }
}

void SDLPlayer::OnMouseDown(const SDL_MouseButtonEvent& buttonEvent)
{
    std::cout << "Mouse Button Down: " << (int)buttonEvent.button << " at ("
        << buttonEvent.x << ", " << buttonEvent.y << ")" << std::endl;

    char xdiff, ydiff;
    xdiff = (char)(buttonEvent.x - last_x);
    ydiff = (char)(buttonEvent.y - last_y);

    m_sink->SendHIDMouse(MOUSE_BUTTON_DOWN, xdiff, ydiff);
    last_x = buttonEvent.x;
    last_y = buttonEvent.y;
}

void SDLPlayer::OnMouseUp(const SDL_MouseButtonEvent& buttonEvent)
{
    std::cout << "Mouse Button Up: " << (int)buttonEvent.button << " at ("
        << buttonEvent.x << ", " << buttonEvent.y << ")" << std::endl;

    char xdiff, ydiff;
    xdiff = (char)(buttonEvent.x - last_x);
    ydiff = (char)(buttonEvent.y - last_y);

    m_sink->SendHIDMouse(MOUSE_BUTTON_UP, xdiff, ydiff);
    last_x = buttonEvent.x;
    last_y = buttonEvent.y;
}

void SDLPlayer::OnMouseMove(const SDL_MouseMotionEvent& motionEvent)
{
    //std::cout << "SDLPlayer: " << this << " Mouse Move to (" << motionEvent.x << ", " << motionEvent.y << ")" << std::endl;

    char xdiff, ydiff;
    xdiff = (char)(motionEvent.x - last_x);
    ydiff = (char)(motionEvent.y - last_y);

    m_sink->SendHIDMouse(MOUSE_MOTION, xdiff, ydiff);
    last_x = motionEvent.x;
    last_y = motionEvent.y;
}

void SDLPlayer::OnKeyDown(const SDL_KeyboardEvent& keyEvent)
{
    std::cout << "Key Down: " << SDL_GetKeyName(keyEvent.keysym.sym) << std::endl;
    // 检查修饰键状态
    int modType = MODIFY_NONE;
    SDL_Keymod mod = SDL_GetModState();
    if (mod & KMOD_SHIFT) {
        std::cout << "Shift is pressed" << std::endl;
        modType = MODIFY_SHIFT;
    }
    if (mod & KMOD_CTRL) {
        std::cout << "Ctrl is pressed" << std::endl;
        modType = MODIFY_CTRL;
    }
    if (mod & KMOD_ALT) {
        std::cout << "Alt is pressed" << std::endl;
        modType = MODIFY_ALT;
    }

    unsigned short keyboardValue = KeyCodeToKeyValue(keyEvent.keysym.sym);
    m_sink->SendHIDKeyboard(KEYBOARD_DOWN, modType, keyboardValue);
}

void SDLPlayer::OnKeyUp(const SDL_KeyboardEvent& keyEvent)
{
    std::cout << "Key Up: " << SDL_GetKeyName(keyEvent.keysym.sym) << std::endl;
    // 检查修饰键状态
    int modType = MODIFY_NONE;
    if (keyEvent.keysym.sym == SDLK_LSHIFT || keyEvent.keysym.sym == SDLK_RSHIFT) {
        modType = MODIFY_SHIFT;
    }
    if (keyEvent.keysym.sym == SDLK_LCTRL || keyEvent.keysym.sym == SDLK_RCTRL) {
        modType = MODIFY_CTRL;
    }
    if (keyEvent.keysym.sym == SDLK_LALT || keyEvent.keysym.sym == SDLK_RALT) {
        modType = MODIFY_ALT;
    }
    
    unsigned short keyboardValue = KeyCodeToKeyValue(keyEvent.keysym.sym);
    m_sink->SendHIDKeyboard(KEYBOARD_UP, modType, keyboardValue);
}

unsigned short SDLPlayer::KeyCodeToKeyValue(const SDL_Keycode&code)
{
    // 如果是ASCII值，直接返回，如果是特殊按键，如caps lock，需要转换
    unsigned short keyboardValue = code;
    switch (code)
    {
    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
    case SDLK_F9:
    case SDLK_F10:
    case SDLK_F11:
    case SDLK_F12:
        keyboardValue = HH_FUNCTION_F1 + code - SDLK_F1;
        break;
    case SDLK_LEFT:
        keyboardValue = HH_SPECIAL_LEFT_ARROW;
        break;
    case SDLK_RIGHT:
        keyboardValue = HH_SPECIAL_RIGHT_ARROW;
        break;
    case SDLK_UP:
        keyboardValue = HH_SPECIAL_UP_ARROW;
        break;
    case SDLK_DOWN:
        keyboardValue = HH_SPECIAL_DOWN_ARROW;
        break;
    case SDLK_INSERT:
        keyboardValue = HH_SPECIAL_INSERT;
        break;
    case SDLK_HOME:
        keyboardValue = HH_SPECIAL_HOME;
        break;
    case SDLK_END:
        keyboardValue = HH_SPECIAL_END;
        break;
    case SDLK_PAGEUP:
        keyboardValue = HH_SPECIAL_PAGEUP;
        break;
    case SDLK_PAGEDOWN:
        keyboardValue = HH_SPECIAL_PAGEDOWN;
        break;
    case SDLK_CAPSLOCK:
        keyboardValue = HH_SPECIAL_CAPS_LOCK;
        break;
    case SDLK_NUMLOCKCLEAR:
        keyboardValue = HH_SPECIAL_NUM_LOCK;
        break;
    case SDLK_PRINTSCREEN:
        keyboardValue = HH_SPECIAL_PRINT_SCREEN;
        break;
    case SDLK_SCROLLLOCK:
        keyboardValue = HH_SPECIAL_SCROLL_LOCK;
        break;
    case SDLK_PAUSE:
        keyboardValue = HH_SPECIAL_PAUSE;
        break;
    case SDLK_KP_DIVIDE:
        keyboardValue = HH_SPECIAL_KP_DIVIDE;
        break;
    case SDLK_KP_MULTIPLY:
        keyboardValue = HH_SPECIAL_KP_MULTIPLY;
        break;
    case SDLK_KP_MINUS:
        keyboardValue = HH_SPECIAL_KP_MINUS;
        break;
    case SDLK_KP_PLUS:
        keyboardValue = HH_SPECIAL_KP_PLUS;
        break;
    case SDLK_KP_ENTER:
        keyboardValue = HH_SPECIAL_KP_ENTER;
        break;
    case SDLK_KP_1:
    case SDLK_KP_2:
    case SDLK_KP_3:
    case SDLK_KP_4:
    case SDLK_KP_5:
    case SDLK_KP_6:
    case SDLK_KP_7:
    case SDLK_KP_8:
    case SDLK_KP_9:
        keyboardValue = HH_SPECIAL_KP_1 + code - SDLK_KP_1;
        break;
    case SDLK_KP_0:
        keyboardValue = HH_SPECIAL_KP_0;
        break;
    case SDLK_KP_PERIOD:
        keyboardValue = HH_SPECIAL_KP_PERIOD;
        break;
    default:
        break;
    }

    return keyboardValue;
}

void SDLPlayer::Render()
{
    std::lock_guard<std::mutex> lock(renderMutex);

    if (!renderQueue.empty()) {
        AVFrame* frame = renderQueue.front();
        renderQueue.pop();

        SDL_UpdateYUVTexture(texture, nullptr,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        av_frame_free(&frame);
    }
}
