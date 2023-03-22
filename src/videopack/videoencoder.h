#pragma once

#include <vector>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class VideoEncoder
{
public:
	VideoEncoder();
	~VideoEncoder();

	int initH264(int width, int height, int fps, int bitRate);
	void uninit();

	AVCodecContext* getContext();

	int encoder(uint8_t* yuv, int size, int streamIndex, int64_t pts, int64_t timeBase, std::vector<AVPacket*>& packets);

private:
	AVCodecContext* m_ctx;
	AVFrame*		m_frame;
	AVDictionary*	m_dict;
};
