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
#include <assert.h>

#include <xaudio2.h>


constexpr int BufferSize = 20480;
constexpr int RefillThresh = 4096;
constexpr int StreamingBufferSize = 4096;
// constexpr int StreamingBufferSize = 65536;
constexpr int MaxBufferCount = 3;

static uint8_t g_buffers[MaxBufferCount][StreamingBufferSize]; 

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

static int decode(AVCodecContext* decCtx, AVPacket* pkt, AVFrame* frame, uint8_t* buffer, int bufferSize)
{
    int storeSize = 0;
    int remainingSize = bufferSize;

    // temporary buffer, store last decode remaining decoded data
    static std::vector<uint8_t> tmpBuf;


    //
    // Last Remaining Decoded Data Processing
    //

    // If temporary have data, directly storing to buffer.
    if (!tmpBuf.empty())
    {
        // If tempoaray remaining data size is too big, 
        // only fill full buffer, other part remaining.
        if (tmpBuf.size() >= bufferSize)
        {
            memcpy(buffer, tmpBuf.data(), bufferSize);
            tmpBuf.assign(tmpBuf.begin() + bufferSize, tmpBuf.end());
            return bufferSize;
        }
        // If buffer is enough store temporary buffer size,
        // store these and continue to store new decoded data.
        else
        {
            memcpy(buffer, tmpBuf.data(), tmpBuf.size());
            storeSize += tmpBuf.size();
            remainingSize -= tmpBuf.size();
            tmpBuf.clear();
        }
    }


    //
    // Decode
    //

    // Send the packet with the compressed data to the decoder.
    auto parseSize = avcodec_send_packet(decCtx, pkt);
    exitIf(parseSize < 0, "Error submitting the packet to the decoder");

    bool hasRemainingDataInPacket = false;

label_packetProcessingAgain:
    // Read all the output frames.
    while (parseSize >= 0)
    {
        // Decode compressed data to the frame.
        auto decodeSize = avcodec_receive_frame(decCtx, frame);
        if (decodeSize == AVERROR(EAGAIN) || // The remaining data in the packet 
                                             // is not enough to decode a complete frame.
            decodeSize == AVERROR_EOF)       // End of file
        {
            return storeSize;
        }
        else
        {
            exitIf(decodeSize < 0, "Error during decoding");
        }

        // Get number of bytes per sample
        auto sampleSize = av_get_bytes_per_sample(decCtx->sample_fmt);
        exitIf(sampleSize < 0, "Failed to calculate data size");


        //
        // Frame Data OverFlow Processing
        //

        auto frameSize = frame->nb_samples * frame->ch_layout.nb_channels * sampleSize;

        // If frameSize is bigger than remaining size of buffer,
        // fill full buffer and store remaining part of frame data to temporary buffer
        // for next decode to use.
        if (frameSize >= remainingSize)
        {
            if (!hasRemainingDataInPacket)
            {
                // Store decoded data
                for (int i = 0; i < frame->nb_samples; ++i)
                {
                    for (int ch = 0; ch < decCtx->ch_layout.nb_channels; ++ch)
                    {
                        if (sampleSize > remainingSize)
                        {
                            tmpBuf.insert(tmpBuf.end(),
                                          frame->data[ch] + sampleSize * i,
                                          frame->data[ch] + sampleSize * (i + 1));
                        }
                        else
                        {
                            memcpy(buffer + storeSize, frame->data[ch] + sampleSize * i, sampleSize);
                            storeSize += sampleSize;
                            remainingSize -= sampleSize;
                        }
                    }
                }

                hasRemainingDataInPacket = true;
                goto label_packetProcessingAgain;
            }
            else
            {
                // Buffer is full case, 
                // the remaining packet data decoding to temporary buffer. 
                for (int i = 0; i < frame->nb_samples; ++i)
                {
                    for (int ch = 0; ch < decCtx->ch_layout.nb_channels; ++ch)
                    {
                            tmpBuf.insert(tmpBuf.end(),
                                          frame->data[ch] + sampleSize * i,
                                          frame->data[ch] + sampleSize * (i + 1));
                    }
                }

                return storeSize;
            }
        }
        else
        {
            // Store decoded data
            for (int i = 0; i < frame->nb_samples; ++i)
            {
                for (int ch = 0; ch < decCtx->ch_layout.nb_channels; ++ch)
                {
                    memcpy(buffer + storeSize, frame->data[ch] + sampleSize * i, sampleSize);
                    storeSize += sampleSize;
                    remainingSize -= sampleSize;
                }
            }
        }
    }

    return storeSize;
}

int main2()
{
    //
    // XAudio2
    //
    exitIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    IXAudio2* xaudio2;
    exitIfFailed(XAudio2Create(&xaudio2));

    IXAudio2MasteringVoice* masterVoice;
    exitIfFailed(xaudio2->CreateMasteringVoice(&masterVoice));
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
    
    // allocate packet and frame 
    auto pkt = av_packet_alloc();
    exitIf(!pkt, "Could not allocate packet");
    auto frame = av_frame_alloc();
    exitIf(!frame, "Could not allocate frame");

    uint8_t buffer[BufferSize + RefillThresh]; // restore encode data
    uint8_t* data = buffer;

    int storeSize = 0;
    std::vector<uint8_t> pcm;
    // decode
    auto readSize = fread(buffer, 1, 100, file);
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
            storeSize = decode(decCtx, pkt, frame, g_buffers[0], StreamingBufferSize);
            pcm.insert(pcm.end(), g_buffers[0], g_buffers[0] + storeSize);
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
    storeSize = decode(decCtx, pkt, frame, g_buffers[0], StreamingBufferSize);
    pcm.insert(pcm.end(), g_buffers[0], g_buffers[0] + storeSize);
    
    //
    // XAudio2
    //
    
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