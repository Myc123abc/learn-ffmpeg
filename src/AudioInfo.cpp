#include "AudioInfo.hpp"

#include "assert.h"

AudioInfo::AudioInfo(AVFormatContext* ctx)
{
    assert(ctx->nb_streams > 0);
    assert(ctx->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO);

    auto codecParam = ctx->streams[0]->codecpar;

    _type         = ctx->iformat->name;
    _codecID      = codecParam->codec_id;
    _sampleFormat = (AVSampleFormat)codecParam->format;
    _sampleSize   = av_get_bytes_per_sample(_sampleFormat) * 8; 
    _sampleRate   = codecParam->sample_rate;
    _channelsNum  = codecParam->ch_layout.nb_channels;
    _bitRate      = codecParam->bit_rate;
    _frameSize    = codecParam->frame_size;
    _formatTag    = getFormatTag();
    _duration     = getDuration(ctx);
    _startTime    = ctx->start_time;
}

uint16_t AudioInfo::getFormatTag()
{
    switch (_codecID)
    {
    case AV_CODEC_ID_MP3: return WAVE_FORMAT_IEEE_FLOAT;
    default:              return WAVE_FORMAT_UNKNOWN;
    }
}

WAVEFORMATEX AudioInfo::getWaveFormat()
{
    WAVEFORMATEX wfx    = {};
    wfx.wFormatTag      = _formatTag;
    wfx.nChannels       = _channelsNum;
    wfx.nSamplesPerSec  = _sampleRate;
    wfx.wBitsPerSample  = _sampleSize;
    wfx.nBlockAlign     = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    return wfx;
}

uint64_t AudioInfo::getDuration(AVFormatContext* ctx)
{
    int64_t hours, mins, secs, us, ms;
    int64_t duration = ctx->duration + (ctx->duration <= INT64_MAX - 5000 ? 5000 : 0);
    secs  = duration / AV_TIME_BASE;
    us    = duration % AV_TIME_BASE;
    ms = (100 * us) / AV_TIME_BASE;
    return secs * 1000 + ms;
}