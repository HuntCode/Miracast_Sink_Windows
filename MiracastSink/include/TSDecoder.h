#include <stdint.h>
#include <functional>

typedef void (*DecodeCallBack)(int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);
typedef void(*PackLossCallBack)(int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

class TSDecoder {
public:
    TSDecoder() {

    }
    TSDecoder(void* ptr) {

    }
    ~TSDecoder(){}
    int input(const uint8_t* data, size_t bytes);

    void setOnDecode(std::function<void(int, int, int, int64_t, int64_t, const void*, size_t)> cb);

    void setOnPacketLoss(std::function<void(int)> cb);
};
