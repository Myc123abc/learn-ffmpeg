#pragma once

extern "C"
{
#include <libavformat/avformat.h>
}

#include <xaudio2.h>

#include <string>

class AudioInfo
{
public:
    AudioInfo(AVFormatContext* ctx);

    WAVEFORMATEX getWaveFormat();

    const std::string_view type() { return _type; }
    AVCodecID codecID() { return _codecID; }

private:
    uint16_t getFormatTag();
    uint64_t getDuration(AVFormatContext* ctx); 

    std::string    _type;
    AVCodecID      _codecID;
    AVSampleFormat _sampleFormat;
    uint16_t       _sampleSize;
    int64_t        _bitRate;
    int            _sampleRate;
    int            _channelsNum;
    int            _frameSize;
    uint16_t       _formatTag;
    uint64_t       _duration;
    uint64_t       _startTime;
};