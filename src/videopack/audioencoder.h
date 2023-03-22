#pragma once

#include <vector>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class AudioEncoder
{
public:
	AudioEncoder();
	~AudioEncoder();

	int initAAC(int channels, int sampleRate, int bitRate);
	void uninit();

	AVCodecContext* getContext();

	int encode(uint8_t* pcm, int size, int streamIndex, int64_t pts, int64_t timeBase, std::vector<AVPacket*>& packets);

private:
	AVCodecContext* m_ctx;
	AVFrame*		m_frame;
};
