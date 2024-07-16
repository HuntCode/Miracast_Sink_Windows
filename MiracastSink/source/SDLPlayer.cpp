#include "pch.h"

#include "SDLPlayer.h"



#define windowWidth 960

#define windowHeight 570



SDLPlayer::SDLPlayer(int videoWidth, int videoHeight)

    : videoWidth(videoWidth), videoHeight(videoHeight)

{

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {

        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;

        return;

    }



    // 初始化 FFmpeg

    av_register_all();



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

    window = SDL_CreateWindow("SDL Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,

        windowWidth, windowHeight, SDL_WINDOW_SHOWN);

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



SDLPlayer::~SDLPlayer()

{

    quit = true;

    audioCV.notify_all();

    videoCV.notify_all();



    if (audioThread.joinable()) audioThread.join();

    if (videoThread.joinable()) videoThread.join();

    if (playThread.joinable()) playThread.join();



    SDL_DestroyTexture(texture);

    SDL_DestroyRenderer(renderer);

    SDL_DestroyWindow(window);

    SDL_CloseAudioDevice(audioDevice);

    SDL_Quit();



    avcodec_free_context(&audioCodecContext);

    avcodec_free_context(&videoCodecContext);

    //swr_free(&swrContext);

    //sws_freeContext(swsContext);

}



void SDLPlayer::Play()

{

    playThread = std::thread(&SDLPlayer::PlayThreadFunc, this);

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

    AVCodec* audioCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);

    audioCodecContext = avcodec_alloc_context3(audioCodec);

    avcodec_open2(audioCodecContext, audioCodec, nullptr);

    if (audioCodecContext->sample_rate == 0) {

        audioCodecContext->sample_rate = 44100;

        audioCodecContext->channels = 2;

        audioCodecContext->channel_layout = av_get_default_channel_layout(2);

    }



    // 初始化视频解码器

    AVCodec* videoCodec = avcodec_find_decoder(AV_CODEC_ID_H264);

    videoCodecContext = avcodec_alloc_context3(videoCodec);

    avcodec_open2(videoCodecContext, videoCodec, nullptr);



}

void SDLPlayer::PlayThreadFunc() {

    // SDL main loop

    while (!quit) {

        HandleEvents();

        Render();

        SDL_Delay(10); // Delay to reduce CPU usage

    }

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



void SDLPlayer::HandleEvents()

{

    SDL_Event event;

    while (SDL_PollEvent(&event)) {

        if (event.type == SDL_QUIT) {

            quit = true;

        }

    }

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