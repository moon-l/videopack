#pragma once

#include <string>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class Muxer
{
public:
	Muxer();
	~Muxer();

	int init(const char* url);
	void uninit();

	int addStream(AVCodecContext* ctx);

	int open();

	int getAudioStreamIndex();
	int getVideoStreamIndex();

	int sendHeader();
	int sendPacket(AVPacket* pkt);
	int sendTrailer();

private:
	AVFormatContext*	m_ctx;
	std::string			m_url;

	AVCodecContext*		m_audioCtx;
	AVCodecContext*		m_videoCtx;

	AVStream*			m_audioStream;
	AVStream*			m_videoStream;

	int					m_audioIndex;
	int					m_videoIndex;
};
