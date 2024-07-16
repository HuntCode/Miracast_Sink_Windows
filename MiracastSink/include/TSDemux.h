/*
WXMedia通用接口
*/
#ifndef _WXUTILS_API_H_
#define _WXUTILS_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined WIN32 || defined ANDROID
#define WXMEDIA_API
#endif

// MPEG-TS  数据处理
WXMEDIA_API void*  TSDemuxCreate();

WXMEDIA_API void   TSDemuxDestroy(void *ptr);

// 返回1 表示可以获得视频数据
// 返回2 表示可以获得音频数据
#define TS_TYPE_VIDEO 1
#define TS_TYPE_AUDIO 2

WXMEDIA_API int    TSDemuxWriteData(void *ptr, const uint8_t *buf, int buf_size);

WXMEDIA_API void   TSDemuxGetExtraData(void *ptr, uint8_t **buf, int *buf_size);

WXMEDIA_API void   TSDemuxGetVideoData(void *ptr, uint8_t **buf, int *buf_size);

WXMEDIA_API void   TSDemuxGetAudioData(void *ptr, uint8_t **buf, int *buf_size);

#ifdef __cplusplus
}
#endif

#endif
