#include "audioencoder.h"

AudioEncoder::AudioEncoder()
	: m_ctx(nullptr)
	, m_frame(nullptr)
	, m_done(false)
{

}

AudioEncoder::~AudioEncoder()
{
	uninit();
}

int AudioEncoder::initAAC(int channels, int sampleRate, int bitRate)
{
	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
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
	m_ctx->sample_rate = sampleRate;
	m_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	m_ctx->channels = channels;
	m_ctx->channel_layout = av_get_default_channel_layout(m_ctx->channels);
	int ret = avcodec_open2(m_ctx, nullptr, nullptr);
	if (ret < 0)
	{
		return -3;
	}

	m_frame = av_frame_alloc();
	m_frame->format = AV_SAMPLE_FMT_S16;
	m_frame->channels = m_ctx->channels;
	m_frame->channel_layout = av_get_default_channel_layout(m_frame->channels);
	m_frame->nb_samples = m_ctx->frame_size;
	ret = av_frame_get_buffer(m_frame, 0);
	if (ret != 0)
	{
		return -4;
	}

	return 0;
}

int AudioEncoder::initLearPCM(int channels, int sampleRate, int frameSize)
{
	m_ctx = avcodec_alloc_context3(nullptr);
	if (!m_ctx)
	{
		return -1;
	}

	m_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	m_ctx->codec = nullptr;
	m_ctx->codec_id = AV_CODEC_ID_PCM_S16LE;
	m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	m_ctx->sample_rate = sampleRate;
	m_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	m_ctx->bit_rate = 16 * channels * sampleRate;
	m_ctx->channels = channels;
	m_ctx->channel_layout = av_get_default_channel_layout(m_ctx->channels);
	m_ctx->frame_size = frameSize;
	m_ctx->time_base.num = 1;
	m_ctx->time_base.den = sampleRate;

	m_frame = av_frame_alloc();
	m_frame->format = AV_SAMPLE_FMT_S16;
	m_frame->channels = m_ctx->channels;
	m_frame->channel_layout = av_get_default_channel_layout(m_frame->channels);
	m_frame->nb_samples = m_ctx->frame_size;
	int ret = av_frame_get_buffer(m_frame, 0);
	if (ret != 0)
	{
		return -2;
	}

	return 0;
}

void AudioEncoder::uninit()
{
	if (m_frame)
	{
		av_frame_free(&m_frame);
	}

	if (m_ctx)
	{
		avcodec_free_context(&m_ctx);
	}
}

AVCodecContext* AudioEncoder::getContext()
{
	return m_ctx;
}

int AudioEncoder::encode(uint8_t* pcm, int size, int streamIndex, int64_t pts, int64_t timeBase, std::vector<AVPacket*>& packets)
{
	if (!m_ctx || !m_frame)
	{
		return -1;
	}

	pts = av_rescale_q(pts, AVRational{ 1, (int)timeBase }, m_ctx->time_base);
	m_frame->pts = pts;

	int ret = 0;
	if (m_ctx->codec_id == AV_CODEC_ID_PCM_S16LE)
	{
		if (m_done)
		{
			return AVERROR_EOF;
		}

		if (!pcm)
		{
			m_done = true;
			return AVERROR_EOF;
		}

		ret = avcodec_fill_audio_frame(m_frame, m_ctx->channels, m_ctx->sample_fmt, pcm, size, 1);
		if (ret <= 0)
		{
			return -2;
		}

		AVPacket* pkt = av_packet_alloc();
		pkt->size = size;
		unsigned s = 0;
		av_fast_padded_malloc(&pkt->data, &s, size);
		av_init_packet(pkt);
		memcpy(pkt->data, (const short*)m_frame->data[0], m_frame->nb_samples * m_ctx->channels * av_get_bits_per_sample(m_ctx->codec_id) / 8);

		pkt->stream_index = streamIndex;
		pkt->pts = m_frame->pts;
		pkt->dts = m_frame->pts;
		pkt->duration = av_rescale_q(m_frame->nb_samples, AVRational { 1, m_ctx->sample_rate }, m_ctx->time_base);

		packets.push_back(pkt);

		return 0;
	}

	if (pcm)
	{
		ret = avcodec_fill_audio_frame(m_frame, m_ctx->channels, m_ctx->sample_fmt, pcm, size, 1);
		if (ret <= 0)
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
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);

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
