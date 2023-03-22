#include "muxer.h"

Muxer::Muxer()
    : m_ctx(nullptr)
    , m_audioCtx(nullptr)
    , m_videoCtx(nullptr)
    , m_audioStream(nullptr)
    , m_videoStream(nullptr)
    , m_audioIndex(-1)
    , m_videoIndex(-1)
{
}

Muxer::~Muxer()
{
    uninit();
}

int Muxer::init(const char* url)
{
    int ret = avformat_alloc_output_context2(&m_ctx, nullptr, nullptr, url);
    if (ret < 0)
    {
        return -1;
    }

    m_url = url;

    return 0;
}

void Muxer::uninit()
{
    if (m_ctx)
    {
        avformat_close_input(&m_ctx);
    }

    m_url.clear();

    m_audioCtx = nullptr;
    m_audioStream = nullptr;
    m_audioIndex = -1;

    m_videoCtx = nullptr;
    m_videoStream = nullptr;
    m_videoIndex = -1;
}

int Muxer::addStream(AVCodecContext* ctx)
{
    if (!m_ctx || !ctx)
    {
        return -1;
    }

    AVStream* stream = avformat_new_stream(m_ctx, nullptr);
    if (!stream)
    {
        return -2;
    }

    avcodec_parameters_from_context(stream->codecpar, ctx);
    av_dump_format(m_ctx, 0, m_url.data(), 1);

    if (ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        m_audioCtx = ctx;
        m_audioStream = stream;
        m_audioIndex = stream->index;
    }
    else if (ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        m_videoCtx = ctx;
        m_videoStream = stream;
        m_videoIndex = stream->index;
    }

    return 0;
}

int Muxer::open()
{
    if (!m_ctx)
    {
        return -1;
    }

    int ret = avio_open(&m_ctx->pb, m_url.data(), AVIO_FLAG_WRITE);
    if (ret < 0)
    {
        return -2;
    }

    return 0;
}

int Muxer::getAudioStreamIndex()
{
    return m_audioIndex;
}

int Muxer::getVideoStreamIndex()
{
    return m_videoIndex;
}

int Muxer::sendHeader()
{
    if (!m_ctx)
    {
        return -1;
    }

    int ret = avformat_write_header(m_ctx, nullptr);
    if (ret < 0)
    {
        return -2;
    }

    return 0;
}

int Muxer::sendPacket(AVPacket* pkt)
{
    if (!pkt || pkt->size <= 0 || !pkt->data)
    {
        if (pkt)
        {
            av_packet_free(&pkt);
        }

        return -1;
    }

    AVRational tbSrc;
    AVRational tbDst;
    if (m_videoStream && m_videoCtx && pkt->stream_index == m_videoIndex)
    {
        tbSrc = m_videoCtx->time_base;
        tbDst = m_videoStream->time_base;
    }
    else if (m_audioStream && m_audioCtx && pkt->stream_index == m_audioIndex)
    {
        tbSrc = m_audioCtx->time_base;
        tbDst = m_audioStream->time_base;
    }

    pkt->pts = av_rescale_q(pkt->pts, tbSrc, tbDst);
    pkt->dts = av_rescale_q(pkt->dts, tbSrc, tbDst);
    pkt->duration = av_rescale_q(pkt->duration, tbSrc, tbDst);

    int ret = av_interleaved_write_frame(m_ctx, pkt);

    av_packet_free(&pkt);
    if (ret != 0)
    {
        return 0;
    }
    return 0;
}

int Muxer::sendTrailer()
{

    if (!m_ctx)
    {
        return -1;
    }

    int ret = av_write_trailer(m_ctx);
    if (ret < 0)
    {
        return -2;
    }

    return 0;
}
