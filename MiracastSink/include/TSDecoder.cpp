#include "TSDecoder.h"

int TSDecoder::input(const uint8_t * data, size_t bytes)
{
    return 0;
}

void TSDecoder::setOnDecode(std::function<void(int, int, int, int64_t, int64_t, const void*, size_t)> cb)
{
}

void TSDecoder::setOnPacketLoss(std::function<void(int)> cb)
{
}

