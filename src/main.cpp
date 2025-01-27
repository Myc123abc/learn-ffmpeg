extern "C"
{
#include <libavutil/log.h>
}

int main()
{
    av_log_set_level(AV_LOG_DEBUG);

    auto msg = "Test log";
    av_log(nullptr, AV_LOG_INFO, "%s\n", msg);
}