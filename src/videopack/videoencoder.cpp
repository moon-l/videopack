#include "videoencoder.h"

extern "C"
{
#include "libavutil/imgutils.h"
}

VideoEncoder::VideoEncoder()
    : m_ctx(nullptr)
    , m_frame(nullptr)
    , m_dict(nullptr)
{
}

VideoEncoder::~VideoEncoder()
{
    uninit();
}

int VideoEncoder::initH264(int width, int height, int fps, int bitRate)
{
	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		return -1;
	}

	m_ctx = avcodec_alloc_context3(codec);
	if (!m_ctx)
	{
		return -2;
	}

	m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	m_ctx->bit_rate = bitRate;
	m_ctx->width = width;
	m_ctx->height = height;
	m_ctx->framerate.num = fps;
	m_ctx->framerate.den = 1;
	m_ctx->time_base.num = 1;
	m_ctx->time_base.den = fps;
	m_ctx->gop_size = 12;
	m_ctx->max_b_frames = 0;
	m_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	int ret = avcodec_open2(m_ctx, nullptr, &m_dict);
	if (ret < 0)
	{
		return -3;
	}

	m_frame = av_frame_alloc();
	if (!m_frame)
	{
		return -4;
	}

	m_frame->width = m_ctx->width;
	m_frame->height = m_ctx->height;
	m_frame->format = m_ctx->pix_fmt;

    return 0;
}

void VideoEncoder::uninit()
{
	if (m_frame)
	{
		av_frame_free(&m_frame);
	}

	if (m_dict)
	{
		av_dict_free(&m_dict);
	}

	if (m_ctx)
	{
		avcodec_free_context(&m_ctx);
	}
}

AVCodecContext* VideoEncoder::getContext()
{
    return m_ctx;
}

int VideoEncoder::encoder(uint8_t* yuv, int size, int streamIndex, int64_t pts, int64_t timeBase, std::vector<AVPacket*>& packets)
{
	if (!m_ctx || !m_frame)
	{
		return -1;
	}

	int ret = 0;

	pts = av_rescale_q(pts, AVRational{ 1, (int)timeBase }, m_ctx->time_base);
	m_frame->pts = pts;

	if (yuv)
	{
		int n = av_image_fill_arrays(m_frame->data, m_frame->linesize, yuv, (AVPixelFormat)m_frame->format, m_frame->width, m_frame->height, 1);
		if (n != size)
		{
			return -2;
		}

		ret = avcodec_send_frame(m_ctx, m_frame);
	}
	else
	{
		ret = avcodec_send_frame(m_ctx, nullptr);
	}

	if (ret != 0)
	{
		return -3;
	}

	while (true)
	{
		AVPacket* pkt = av_packet_alloc();
		ret = avcodec_receive_packet(m_ctx, pkt);
		pkt->stream_index = streamIndex;
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			ret = 0;
			av_packet_free(&pkt);
			break;
		}
		else if (ret < 0)
		{
			av_packet_free(&pkt);
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);

			ret = -4;
		}
		packets.push_back(pkt);
	}

	return ret;
}
