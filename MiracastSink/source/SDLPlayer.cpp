#ifdef USE_SDL_PLAYER

#include "SDLPlayer.h"
#include "MiracastDevice.h"
#include "UIBCManager.h"

// 自定义事件类型
enum {
    SDL_EVENT_CREATE_WINDOW = SDL_USEREVENT
};

std::atomic<int> SDLPlayer::s_instanceCount = 0;

SDLPlayer::SDLPlayer(MiracastDevice* device)
    : m_device(device), m_window(nullptr), m_renderer(nullptr), m_texture(nullptr),
      m_videoWidth(0), m_videoHeight(0)
{
    {
        std::lock_guard<std::mutex> lock(m_initMutex);
        if (s_instanceCount == 0) {
            SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
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
    SDL_DestroyTexture(m_texture);
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_CloseAudioDevice(m_audioDevice);

    {
        std::lock_guard<std::mutex> lock(m_initMutex);
        if (--s_instanceCount == 0) {
            SDL_Quit();
        }
    }

    avcodec_free_context(&m_audioCodecContext);
    avcodec_free_context(&m_videoCodecContext);
}

void SDLPlayer::Init(const std::string& name)
{
    m_winName = name;

    // 创建音频设备
    m_audioSpec.freq = 48000;
    m_audioSpec.format = AUDIO_S16SYS;
    m_audioSpec.channels = 2;
    m_audioSpec.samples = 1024;
    m_audioSpec.callback = nullptr;
    m_audioSpec.userdata = nullptr;

    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &m_audioSpec, nullptr, 0);
    if (m_audioDevice == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_PauseAudioDevice(m_audioDevice, 0); // 恢复音频设备播放

    // 初始化解码器
    InitDecoder();

    // 启动处理线程
    m_audioThread = std::thread(&SDLPlayer::AudioThreadFunc, this);
    m_videoThread = std::thread(&SDLPlayer::VideoThreadFunc, this);
}

void SDLPlayer::Play()
{
    m_quit = false;
    // SDL main loop
    while (!m_quit) {
        HandleEvents();
        HandleCustomEvents();
        Render();
        SDL_Delay(10); // Delay to reduce CPU usage
    }
}

void SDLPlayer::Stop()
{
    m_quit = true;
    m_audioCV.notify_all();
    m_videoCV.notify_all();

    if (m_audioThread.joinable()) m_audioThread.join();
    if (m_videoThread.joinable()) m_videoThread.join();
}

void SDLPlayer::ProcessVideo(uint8_t* buffer, int bufSize)
{
    {
        std::lock_guard<std::mutex> lock(m_videoMutex);
        m_videoQueue.push(std::vector<uint8_t>(buffer, buffer + bufSize));
    }
    m_videoCV.notify_one();
}

void SDLPlayer::ProcessAudio(uint8_t* buffer, int bufSize)
{
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        m_audioQueue.push(std::vector<uint8_t>(buffer, buffer + bufSize));
    }
    m_audioCV.notify_one();
}

void SDLPlayer::InitDecoder()
{
    // 初始化音频解码器
    const AVCodec* audioCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    m_audioCodecContext = avcodec_alloc_context3(audioCodec);
    avcodec_open2(m_audioCodecContext, audioCodec, nullptr);
    if (m_audioCodecContext->sample_rate == 0) {
        m_audioCodecContext->sample_rate = 48000;
        m_audioCodecContext->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    }

    // 初始化视频解码器
    const AVCodec* videoCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    m_videoCodecContext = avcodec_alloc_context3(videoCodec);
    avcodec_open2(m_videoCodecContext, videoCodec, nullptr);

}

void SDLPlayer::VideoThreadFunc()
{
    while (!m_quit) {
        std::vector<uint8_t> data;
        {
            std::unique_lock<std::mutex> lock(m_videoMutex);
            m_videoCV.wait(lock, [this] { return !m_videoQueue.empty() || m_quit; });
            if (!m_videoQueue.empty()) {
                data = std::move(m_videoQueue.front());
                m_videoQueue.pop();
            }
        }

        if (!data.empty()) {
            DecodeAndQueueVideo(data);
        }
    }
}

void SDLPlayer::AudioThreadFunc()
{
    while (!m_quit) {
        std::vector<uint8_t> data;
        {
            std::unique_lock<std::mutex> lock(m_audioMutex);
            m_audioCV.wait(lock, [this] { return !m_audioQueue.empty() || m_quit; });

            if (!m_audioQueue.empty()) {
                data = std::move(m_audioQueue.front());
                m_audioQueue.pop();
            }
        }

        if (!data.empty()) {
            DecodeAndQueueAudio(data);
        }
    }
}

void SDLPlayer::DecodeAndQueueVideo(const std::vector<uint8_t>& data)
{
    AVPacket* packet = av_packet_alloc();
    packet->data = const_cast<uint8_t*>(data.data());
    packet->size = data.size();

    if (avcodec_send_packet(m_videoCodecContext, packet) >= 0) {
        AVFrame* frame = av_frame_alloc();
        if (avcodec_receive_frame(m_videoCodecContext, frame) >= 0) {
            std::lock_guard<std::mutex> lock(m_renderMutex);

            // create window
            if (frame->width != m_videoWidth || frame->height != m_videoHeight) {
                m_videoWidth = frame->width;
                m_videoHeight = frame->height;

                CreateWindowEvent event;
                event.w = m_videoWidth;
                event.h = m_videoHeight;
                PushCustomEvent(event);
                ClearQueue(m_renderQueue);
            }

            m_renderQueue.push(frame);
        }
    }
    av_packet_free(&packet);
}

void SDLPlayer::DecodeAndQueueAudio(const std::vector<uint8_t>& data)
{
    AVPacket* packet = av_packet_alloc();
    packet->data = const_cast<uint8_t*>(data.data());
    packet->size = data.size();

    if (avcodec_send_packet(m_audioCodecContext, packet) >= 0) {
        AVFrame* frame = av_frame_alloc();
        if (avcodec_receive_frame(m_audioCodecContext, frame) >= 0) {
            // 音频重采样
            AVChannelLayout outChannelLayout = AV_CHANNEL_LAYOUT_STEREO;
            AVChannelLayout inChannelLayout;
            inChannelLayout = m_audioCodecContext->ch_layout;
            SwrContext* swrCtx = swr_alloc(); 
            swr_alloc_set_opts2(&swrCtx, &outChannelLayout, AV_SAMPLE_FMT_S16, 48000,
                &inChannelLayout, m_audioCodecContext->sample_fmt, m_audioCodecContext->sample_rate, 0, NULL);

            swr_init(swrCtx);

            int outSamples = av_rescale_rnd(swr_get_delay(swrCtx, frame->sample_rate) + frame->nb_samples, frame->sample_rate, frame->sample_rate, AV_ROUND_UP);
            uint8_t* output[2] = { nullptr };
            av_samples_alloc(output, nullptr, 2, outSamples, AV_SAMPLE_FMT_S16, 0);

            int convertedSamples = swr_convert(swrCtx, output, outSamples, (const uint8_t**)frame->data, frame->nb_samples);
            int dataSize = av_samples_get_buffer_size(nullptr, 2, convertedSamples, AV_SAMPLE_FMT_S16, 1);
            std::vector<uint8_t> audioBuffer(dataSize);
            memcpy(audioBuffer.data(), output[0], dataSize);
            SDL_QueueAudio(m_audioDevice, audioBuffer.data(), audioBuffer.size());

            av_freep(&output[0]);
            swr_free(&swrCtx);
        }
        av_frame_free(&frame);
    }
    av_packet_free(&packet);
}

void SDLPlayer::ClearQueue(std::queue<AVFrame*>& queue)
{
    while (!queue.empty()) {
        AVFrame* frame = queue.front();
        queue.pop();
        av_frame_free(&frame);
    }
}

void SDLPlayer::CreateWindowAndRenderer(int width, int height)
{
    if(m_texture != nullptr)
        SDL_DestroyTexture(m_texture);
    if(m_renderer != nullptr)
        SDL_DestroyRenderer(m_renderer);
    if(m_window != nullptr)
        SDL_DestroyWindow(m_window);
    // 创建窗口、渲染器和纹理
    m_window = SDL_CreateWindow(m_winName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return;
    }

    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        width, height);
    if (!m_texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return;
    }
}

void SDLPlayer::PushCustomEvent(CreateWindowEvent event)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_customEventQueue.push(event);
}

void SDLPlayer::HandleCustomEvents()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_customEventQueue.empty()) {
        CreateWindowEvent event = m_customEventQueue.front();
        m_customEventQueue.pop();

        CreateWindowAndRenderer(m_videoWidth, m_videoHeight); 
    }
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
            m_quit = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                m_quit = true;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            OnMouseDown(event.button);
            break;
        case SDL_MOUSEBUTTONUP:
            OnMouseUp(event.button);
            break;
        case SDL_MOUSEMOTION:
            if(IsEventForWindow(event, m_window))
                OnMouseMove(event.motion);
            break;
        case SDL_MOUSEWHEEL:
            OnMouseWheel(event.wheel);
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
#ifdef _DEBUG
    std::cout << "Mouse Button Down: " << (int)buttonEvent.button << " at ("
        << buttonEvent.x << ", " << buttonEvent.y << ")" << std::endl;
#endif

    char xdiff, ydiff;
    xdiff = (char)(buttonEvent.x - m_lastX);
    ydiff = (char)(buttonEvent.y - m_lastY);

    // 处理鼠标按下事件
    if (buttonEvent.button == SDL_BUTTON_LEFT) {
        if (m_device->GetUIBCCategory() == UIBC_HIDC) {
            m_device->SendHIDMouse(kMouseLeftButtonDown, xdiff, ydiff, 0);
        }
        else if (m_device->GetUIBCCategory() == UIBC_Generic) {
            rapidjson::Document root;
            root.SetObject();
            root.AddMember("input_type", GENERIC_TOUCH_DOWN, root.GetAllocator());
            root.AddMember("fingers", 1, root.GetAllocator());
            rapidjson::Value touchArrayValue(rapidjson::kArrayType);

            rapidjson::Value touchValue(rapidjson::kObjectType);
            touchValue.AddMember("touch_id", 0, root.GetAllocator());
            touchValue.AddMember("x", buttonEvent.x, root.GetAllocator());
            touchValue.AddMember("y", buttonEvent.y, root.GetAllocator());

            touchArrayValue.PushBack(touchValue, root.GetAllocator());
            root.AddMember("touch_data", touchArrayValue, root.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            root.Accept(writer);
            std::string inEventDesc = buffer.GetString();

            std::cout << "Generic Json: " << inEventDesc << std::endl;

            m_device->SendGenericTouch(inEventDesc.c_str(), 1, 1);
        }
    }
    else if (buttonEvent.button == SDL_BUTTON_MIDDLE) {
        if (m_device->GetUIBCCategory() == UIBC_HIDC) {
            m_device->SendHIDMouse(kMouseMiddleButtonDown, xdiff, ydiff, 0);
        }
        else if (m_device->GetUIBCCategory() == UIBC_Generic) {

        }
    }
    else if (buttonEvent.button == SDL_BUTTON_RIGHT) {
        if (m_device->GetUIBCCategory() == UIBC_HIDC) {
            m_device->SendHIDMouse(kMouseRightButtonDown, xdiff, ydiff, 0);
        }
        else if (m_device->GetUIBCCategory() == UIBC_Generic) {

        }
    }

    m_lastX = buttonEvent.x;
    m_lastY = buttonEvent.y;
}

void SDLPlayer::OnMouseUp(const SDL_MouseButtonEvent& buttonEvent)
{
#ifdef _DEBUG
    std::cout << "Mouse Button Up: " << (int)buttonEvent.button << " at ("
        << buttonEvent.x << ", " << buttonEvent.y << ")" << std::endl;
#endif

    char xdiff, ydiff;
    xdiff = (char)(buttonEvent.x - m_lastX);
    ydiff = (char)(buttonEvent.y - m_lastY);

    // 处理鼠标释放事件
    if (buttonEvent.button == SDL_BUTTON_LEFT) {
        if (m_device->GetUIBCCategory() == UIBC_HIDC) {
            m_device->SendHIDMouse(kMouseLeftButtonUp, xdiff, ydiff, 0);
        }
        else if (m_device->GetUIBCCategory() == UIBC_Generic) {
            rapidjson::Document root;
            root.SetObject();
            root.AddMember("input_type", GENERIC_TOUCH_UP, root.GetAllocator());
            root.AddMember("fingers", 1, root.GetAllocator());
            rapidjson::Value touchArrayValue(rapidjson::kArrayType);
            
            rapidjson::Value touchValue(rapidjson::kObjectType);
            touchValue.AddMember("touch_id", 0, root.GetAllocator());
            touchValue.AddMember("x", buttonEvent.x, root.GetAllocator());
            touchValue.AddMember("y", buttonEvent.y, root.GetAllocator());
            
            touchArrayValue.PushBack(touchValue, root.GetAllocator());
            root.AddMember("touch_data", touchArrayValue, root.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            root.Accept(writer);
            std::string inEventDesc = buffer.GetString();

            m_device->SendGenericTouch(inEventDesc.c_str(), 1, 1);
        }
    }
    else if (buttonEvent.button == SDL_BUTTON_MIDDLE) {
        if (m_device->GetUIBCCategory() == UIBC_HIDC) {
            m_device->SendHIDMouse(kMouseMiddleButtonUp, xdiff, ydiff, 0);
        }
        else if (m_device->GetUIBCCategory() == UIBC_Generic) {

        }
    }
    else if (buttonEvent.button == SDL_BUTTON_RIGHT) {
        if (m_device->GetUIBCCategory() == UIBC_HIDC) {
            m_device->SendHIDMouse(kMouseRightButtonUp, xdiff, ydiff, 0);
        }
        else if (m_device->GetUIBCCategory() == UIBC_Generic) {

        }
    }

    m_lastX = buttonEvent.x;
    m_lastY = buttonEvent.y;
}

void SDLPlayer::OnMouseMove(const SDL_MouseMotionEvent& motionEvent)
{
    //std::cout << "SDLPlayer: " << this << " Mouse Move to (" << motionEvent.x << ", " << motionEvent.y << ")" << std::endl;

    char xdiff, ydiff;
    xdiff = (char)(motionEvent.x - m_lastX);
    ydiff = (char)(motionEvent.y - m_lastY);
    if (m_device->GetUIBCCategory() == UIBC_HIDC) {
        m_device->SendHIDMouse(kMouseMotion, xdiff, ydiff, 0);
    }
    else if (m_device->GetUIBCCategory() == UIBC_Generic) {
        rapidjson::Document root;
        root.SetObject();
        root.AddMember("input_type", GENERIC_TOUCH_MOVE, root.GetAllocator());
        root.AddMember("fingers", 1, root.GetAllocator());
        rapidjson::Value touchArrayValue(rapidjson::kArrayType);

        rapidjson::Value touchValue(rapidjson::kObjectType);
        touchValue.AddMember("touch_id", 0, root.GetAllocator());
        touchValue.AddMember("x", motionEvent.x, root.GetAllocator());
        touchValue.AddMember("y", motionEvent.y, root.GetAllocator());

        touchArrayValue.PushBack(touchValue, root.GetAllocator());
        root.AddMember("touch_data", touchArrayValue, root.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        root.Accept(writer);
        std::string inEventDesc = buffer.GetString();

        m_device->SendGenericTouch(inEventDesc.c_str(), 1, 1);
    }
    m_lastX = motionEvent.x;
    m_lastY = motionEvent.y;
}

void SDLPlayer::OnMouseWheel(const SDL_MouseWheelEvent& wheelEvent)
{
    if (m_device->GetUIBCCategory() == UIBC_HIDC) {
        m_device->SendHIDMouse(kMouseWheel, 0, 0, wheelEvent.y);
    }
}

void SDLPlayer::OnKeyDown(const SDL_KeyboardEvent& keyEvent)
{
#ifdef _DEBUG
    std::cout << "Key Down: " << SDL_GetKeyName(keyEvent.keysym.sym) << std::endl;
#endif

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
    if (m_device->GetUIBCCategory() == UIBC_HIDC) {
        m_device->SendHIDKeyboard(KEYBOARD_DOWN, modType, keyboardValue);
    }
}

void SDLPlayer::OnKeyUp(const SDL_KeyboardEvent& keyEvent)
{
#ifdef _DEBUG
    std::cout << "Key Up: " << SDL_GetKeyName(keyEvent.keysym.sym) << std::endl;
#endif

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
    if (m_device->GetUIBCCategory() == UIBC_HIDC) {
        m_device->SendHIDKeyboard(KEYBOARD_UP, modType, keyboardValue);
    }
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
    std::lock_guard<std::mutex> lock(m_renderMutex);

    if (!m_renderQueue.empty()) {
        AVFrame* frame = m_renderQueue.front();
        m_renderQueue.pop();

        SDL_UpdateYUVTexture(m_texture, nullptr,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);

        SDL_RenderClear(m_renderer);
        SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
        SDL_RenderPresent(m_renderer);

        av_frame_free(&frame);
    }
}

#endif // USE_SDL_PLAYER