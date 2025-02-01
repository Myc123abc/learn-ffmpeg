/**
 * @file libavcodec audio decoding API usage example
 * @example testDecode.cpp
 *
 * Decode data from an MP3 input file and play it with xaudio2.
 */

extern "C" 
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/file.h>
}

#include "AudioInfo.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <queue>
#include <chrono>
#include <exception>

#include <stdio.h>
#include <assert.h>

#include <xaudio2.h>

constexpr int StreamingBufferSize = 65536;
constexpr int MaxBufferCount      = 3;

static void exitIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        exit(EXIT_FAILURE);
    }
}

static void exitIf(bool b, std::string_view msg)
{
    if (b)
    {
        fprintf(stderr, "%s\n", msg.data());
        exit(EXIT_FAILURE);
    }
}

static std::wstring getFileType(std::wstring filename)
{
    auto pos = filename.find_last_of(L".");
    return filename.substr(pos + 1, filename.size() - pos);
}

static std::wstring str_tolower(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return std::tolower(c); }
                  );
    return s;
}

static int getID3TagSize(uint8_t* buffer)
{
    uint8_t header[10];
    memcpy(header, buffer, 10);

    if (memcmp(header, "ID3", 3) == 0)
    {
        return (header[6] << 21 | (header[7] << 14) |
                header[8] << 7) | header[9];
    }

    return 0;
}

static std::vector<uint8_t> decode(AVCodecContext* decCtx, AVPacket* pkt, AVFrame* frame)
{
    std::vector<uint8_t> pcm;

    // send the packet with the compressed data to the decoder
    auto ret = avcodec_send_packet(decCtx, pkt);
    exitIf(ret < 0, "Error submitting the packet to the decoder");

    // read all the output frames
    while (ret >= 0)
    {
        // decode compressed data to the frame
        ret = avcodec_receive_frame(decCtx, frame);
        if (ret == AVERROR(EAGAIN) || // the remaining data in the packet 
                                      // is not enough to decode a complete frame
            ret == AVERROR_EOF)       // end of file
        {
            return pcm;
        }
        else
        {
            exitIf(ret < 0, "Error during decoding");
        }

        // get number of bytes per sample
        auto dataSize = av_get_bytes_per_sample(decCtx->sample_fmt);
        exitIf(dataSize < 0, "Failed to calculate data size");

        // store decoded data
        for (int i = 0; i < frame->nb_samples; ++i)
        {
            for (int ch = 0; ch < decCtx->ch_layout.nb_channels; ++ch)
            {
                pcm.insert(pcm.end(),
                           frame->data[ch] + dataSize * i,
                           frame->data[ch] + dataSize * (i + 1));
            }
        }
    }

    return pcm;
}

int main()
{
    av_log_set_level(AV_LOG_DEBUG);

    //
    // XAudio2
    //
    exitIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    IXAudio2* xaudio2;
    exitIfFailed(XAudio2Create(&xaudio2));

    IXAudio2MasteringVoice* masterVoice;
    exitIfFailed(xaudio2->CreateMasteringVoice(&masterVoice));
    

    //
    // Read File
    //
    auto     filename   = "D:/music/四季ノ唄.mp3";
    uint8_t* fileBuffer = nullptr;
    size_t   fileSize   = 0;
    auto ret = av_file_map((char*)filename, &fileBuffer, &fileSize, 0, nullptr);
    exitIf(ret < 0, "file map error");

    // get format
    AVFormatContext* fmtCtx = nullptr;
    ret = avformat_open_input(&fmtCtx, filename, nullptr, nullptr);
    exitIf(ret < 0, "format open error");

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    exitIf(ret < 0, "find stream info error");

    // get audio info
    AudioInfo audioInfo(fmtCtx);
    
    // create source voice
    IXAudio2SourceVoice* sourceVoice;
    auto wfx = audioInfo.getWaveFormat();
    exitIfFailed(xaudio2->CreateSourceVoice(&sourceVoice, &wfx));

    // jump id3 tag
    uint8_t* dataPtr  = fileBuffer;
    size_t   dataSize = fileSize;
    if (audioInfo.type() == "mp3")
    {
        // get ID3 tag size of mp3 file
        auto id3Size = getID3TagSize(fileBuffer);
        dataPtr += id3Size;
        dataSize -= id3Size;
    }


    //
    // Initialize decoder and parser
    //

    // get decoder
    auto decoder = avcodec_find_decoder(audioInfo.codecID());
    exitIf(!decoder, "MP3 decoder not found");

    // allocate decoder context
    auto decCtx = avcodec_alloc_context3(decoder);
    exitIf(!decCtx, "Could not allocate audio decoder context");

    // open decoder
    exitIf(avcodec_open2(decCtx, decoder, nullptr) < 0, "Could not open decoder");

    // get parser
    auto parser = av_parser_init(decoder->id);
    exitIf(!parser, "Parser not found");

    
    //
    // Decode
    //

    // allocate packet and frame 
    auto pkt = av_packet_alloc();
    exitIf(!pkt, "Could not allocate packet");
    auto frame = av_frame_alloc();
    exitIf(!frame, "Could not allocate frame");

    constexpr int BufferSize = 20480;
    constexpr int RefillThresh = 4096;

    // uint8_t buffer[BufferSize + RefillThresh]; // restore encode data
    // uint8_t* data = buffer;

    struct Packet
    {
        std::vector<uint8_t> pcm;
        bool isUsed  = false;
        bool isUsing = false;
    };

    std::queue<Packet> pkts;  // decoded packets
    std::queue<Packet> bufPkts;
    // decode
    // auto readSize = fread(buffer, 1, 100, file);
    while (dataSize > 0)
    {
        // parse data to packet
        auto ret = av_parser_parse2(parser, decCtx, &pkt->data, &pkt->size,
                                    dataPtr, dataSize,
                                    AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        exitIf(ret < 0, "Error while parsing");
        dataPtr += ret;
        dataSize -= ret;

        // decode packet data to frame
        if (pkt->size)
        {
            pkts.emplace(Packet{ decode(decCtx, pkt, frame) });
        }


        // ensure bufPkts have 3 pkts
        if (bufPkts.size() < 3)
        {
            bufPkts.push(pkts.front());
            pkts.pop();
        }

        auto pkt = pkts.front();
        if (!pkt.isUsing)
        {
            XAUDIO2_BUFFER buf = {};
            buf.AudioBytes = pkt.data.size();
            buf.pAudioData = pkt.data.data();
            exitIfFailed(sourceVoice->SubmitSourceBuffer(&buf));
            exitIfFailed(sourceVoice->Start());
        }
    }

    // flush the decoder, just like flush std::cout
    pkt->data = nullptr;
    pkt->size = 0;
    pkts.emplace(Packet{ decode(decCtx, pkt, frame) });
    

    while (true) ;
    
    sourceVoice->DestroyVoice();
    masterVoice->DestroyVoice();
    xaudio2->Release();

    CoUninitialize();    

    // fclose(file);

    av_file_unmap(fileBuffer, fileSize);

    avformat_close_input(&fmtCtx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_parser_close(parser);
    avcodec_free_context(&decCtx);
}