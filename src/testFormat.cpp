extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

void testFormat()
{
    av_log_set_level(AV_LOG_DEBUG);

    auto url = "D:/music/test.mp3";

    AVFormatContext* fmt;
    auto ret = avformat_open_input(&fmt, url, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot open %s format : %s", url, av_err2str(ret));
        exit(EXIT_FAILURE);
    }

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot find stream info : %s", av_err2str(ret));
        avformat_close_input(&fmt);
        exit(EXIT_FAILURE);
    }

    av_dump_format(fmt, 0, url, 0);

    ret = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot find best stream: %s", av_err2str(ret));
        avformat_close_input(&fmt);
        exit(EXIT_FAILURE);
    }

    avformat_close_input(&fmt);
}