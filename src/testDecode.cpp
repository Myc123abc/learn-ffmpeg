/**
 * @file libavcodec audio decoding API usage example
 * @example testDecode.cpp
 *
 * Decode data from an MP3 input file and play it with xaudio2.
 */

extern "C" 
{
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <stdio.h>

#include <xaudio2.h>

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

static void jumpID3Tag(FILE* file)
{
    uint8_t header[10];
    fread(header, 1, 10, file);

    if (memcmp(header, "ID3", 3) == 0)
    {
        int tagSize = (header[6] << 21 | (header[7] << 14) |
                       header[8] << 7) | header[9];
        fseek(file, tagSize, SEEK_CUR);
    }
}

static void decode(AVCodecContext* decCtx, AVPacket* pkt, AVFrame* frame, std::vector<uint8_t>& pcm)
{
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
            return;
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
}

int main()
{
    av_log_set_level(AV_LOG_DEBUG);

    //
    // Perpare file
    //
    auto filePath = L"D:/music/四季ノ唄.mp3";

    // check file type
    auto fileType = getFileType(filePath);
    exitIf(str_tolower(fileType) != L"mp3", "Is not a mp3 file");

    // open file
    FILE* file;
    exitIf(_wfopen_s(&file, filePath, L"rb"), "Failed to open file");

    // jump ID3 tag part of mp3 file
    jumpID3Tag(file);


    //
    // Initialize decoder and parser
    //

    // get decoder
    auto decoder = avcodec_find_decoder(AV_CODEC_ID_MP3);
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

    std::vector<uint8_t> pcm; // receive decoded data
    
    // allocate packet and frame 
    auto pkt = av_packet_alloc();
    exitIf(!pkt, "Could not allocate packet");
    auto frame = av_frame_alloc();
    exitIf(!frame, "Could not allocate frame");

    constexpr int BufferSize = 20480;
    constexpr int RefillThresh = 4096;

    uint8_t buffer[BufferSize + RefillThresh]; // restore encode data
    uint8_t* data = buffer;

    // decode
    auto readSize = fread(buffer, 1, BufferSize, file);
    while (readSize > 0)
    {
        // parse data to packet
        auto ret = av_parser_parse2(parser, decCtx, &pkt->data, &pkt->size,
                                    data, readSize,
                                    AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        exitIf(ret < 0, "Error while parsing");
        data += ret;
        readSize -= ret;

        // decode packet data to frame
        if (pkt->size)
        {
            decode(decCtx, pkt, frame, pcm);
        }

        // ensure enough data for decoding 
        // Because the remaining data maybe not enough to support decode again,
        // if not have thresh maybe cause die loop.
        if (readSize < RefillThresh)
        {
            memmove(buffer, data, readSize);
            data = buffer;
            auto len = fread(data + readSize, 1, BufferSize - readSize, file);
            if (len > 0)
            {
                readSize += len;
            }
        }
    }

    // flush the decoder, just like flush std::cout
    pkt->data = nullptr;
    pkt->size = 0;
    decode(decCtx, pkt, frame, pcm);

    
    //
    // XAudio2
    //
    exitIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    IXAudio2* xaudio2;
    exitIfFailed(XAudio2Create(&xaudio2));

    IXAudio2MasteringVoice* masterVoice;
    exitIfFailed(xaudio2->CreateMasteringVoice(&masterVoice));
    
    auto fmt = av_get_packed_sample_fmt(decCtx->sample_fmt);
    exitIf(fmt != AV_SAMPLE_FMT_FLT, "This mp3 file's sample format is not float point.");

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels  = decCtx->ch_layout.nb_channels;
    wfx.nSamplesPerSec = decCtx->sample_rate;
    wfx.wBitsPerSample = av_get_bytes_per_sample(fmt) * 8;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = pcm.size();
    buf.pAudioData = pcm.data();

    IXAudio2SourceVoice* sourceVoice;
    exitIfFailed(xaudio2->CreateSourceVoice(&sourceVoice, &wfx));
    exitIfFailed(sourceVoice->SubmitSourceBuffer(&buf));
    
    exitIfFailed(sourceVoice->Start());

    while (true) ;
    
    sourceVoice->DestroyVoice();
    masterVoice->DestroyVoice();
    xaudio2->Release();

    CoUninitialize();    

    fclose(file);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_parser_close(parser);
    avcodec_free_context(&decCtx);
}