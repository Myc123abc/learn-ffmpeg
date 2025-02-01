extern "C"
{
#include <libavformat/avformat.h>
}
#include <filesystem>
 
void testFile()
{
    av_log_set_level(AV_LOG_DEBUG);

    auto path = "D:\\music\\四季ノ唄.mp3";

    AVIOContext* ctx;
    int ret;

    ret = avio_open(&ctx, path, AVIO_FLAG_READ);
    if (ret < 0)
    {
//        av_log(nullptr, AV_LOG_ERROR, "Can't open %s: %s\n", path, av_err2str(ret));
        exit(EXIT_FAILURE);
    }

    ret = avio_close(ctx);
    if (ret < 0)
    {
//        av_log(nullptr, AV_LOG_ERROR, "Can't close %s: %s\n", path, av_err2str(ret));
        exit(EXIT_FAILURE);
    }

    auto dir = "D:\\music";

    if (std::filesystem::exists(dir))
    {
        if (!std::filesystem::is_directory(dir))
        {
//            av_log(nullptr, AV_LOG_ERROR, "%s is not a directory: %s\n", dir, av_err2str(ret));
            exit(EXIT_FAILURE);
        } 
    }

    AVIODirContext* dirCtx;
    ret = avio_open_dir(&dirCtx, "./", nullptr);
    if (ret < 0)
    {
//        av_log(nullptr, AV_LOG_ERROR, "Can't open %s: %s\n", dir, av_err2str(ret));
        exit(EXIT_FAILURE);
    }
}