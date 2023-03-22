#include "videoencoder.h"
#include "audioencoder.h"
#include "muxer.h"

#define VIDEO_WIDTH		1920
#define VIDEO_HEIGHT	1080
#define VIDEO_FPS		25
#define VIDEO_BIT_RATE	10 * 1000 * 1000

#define AUDIO_CHANNELS		2
#define AUDIO_SAMPLE_RATE	44100
#define AUDIO_BIT_RATE		128 * 1000

#define TIME_BASE		1000000
#define AUDIO_FRAME_SIZE	3528

int main(int argc, char** argv)
{
	char* i420Path = "F:\\temp\\v.i420";
	char* pcmPath = "F:\\temp\\a.pcm";

	char* outPath = "F:\\temp\\out.flv";

	FILE* i420fp = fopen(i420Path, "rb");
	FILE* pcmfp = fopen(pcmPath, "rb");
	if (!i420fp || !pcmfp)
	{
		printf("fail to open input files\n");
		return -1;
	}

	int ret = 0;

	VideoEncoder vEncoder;
	ret = vEncoder.initH264(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS, VIDEO_BIT_RATE);
	if (ret < 0)
	{
		printf("fail to init video encoder, ret=%d\n", ret);
		return -1;
	}

	AudioEncoder aEncoder;
	ret = aEncoder.initAAC(AUDIO_CHANNELS, AUDIO_SAMPLE_RATE, AUDIO_BIT_RATE);
	if (ret < 0)
	{
		printf("fail to init audio encoder, ret=%d\n", ret);
		return -1;
	}

	Muxer muxer;
	ret = muxer.init(outPath);
	if (ret < 0)
	{
		printf("fail to init muxer, ret=%d\n", ret);
		return -1;
	}

	ret = muxer.addStream(vEncoder.getContext());
	if (ret < 0)
	{
		printf("fail to add video stream, ret=%d\n", ret);
		return -1;
	}

	ret = muxer.addStream(aEncoder.getContext());
	if (ret < 0)
	{
		printf("fail to add audio stream, ret=%d\n", ret);
		return -1;
	}

	int videoBufSize = VIDEO_WIDTH * (VIDEO_HEIGHT + VIDEO_HEIGHT / 2);
	uint8_t* videoBuf = (uint8_t*)malloc(videoBufSize);
	if (!videoBuf)
	{
		printf("fail to alloc video buffer, size=%d\n", videoBufSize);
		return -1;
	}

	int audioBufSize = av_samples_get_buffer_size(nullptr, aEncoder.getContext()->channels, aEncoder.getContext()->frame_size, aEncoder.getContext()->sample_fmt, 1);
	uint8_t* audioBuf = (uint8_t*)malloc(audioBufSize);
	if (!audioBuf)
	{
		printf("fail to alloc audio buffer, size=%d\n", audioBufSize);
		return -1;
	}

	printf("record begin\n");

	ret = muxer.open();
	if (ret < 0)
	{
		printf("fail to open muxer, ret=%d\n", ret);
		return -1;
	}

	ret = muxer.sendHeader();
	if (ret < 0)
	{
		printf("fail to write header, ret=%d\n", ret);
		return -1;
	}

	bool encodeAudio = true, encodeVideo = true;
	std::vector<AVPacket*> pkts;
	double apts = 0, vpts = 0;

	unsigned ats = 0, vts = 0;

	unsigned n = fread(&ats, 1, 4, pcmfp);
	if (n < 4)
	{
		printf("bad audio file");
		return -1;
	}
	apts = ats * 1000;

	n = fread(&vts, 1, 4, i420fp);
	if (n < 4)
	{
		printf("bad video file");
		return -1;
	}
	vpts = vts * 1000;

	int aleft = AUDIO_FRAME_SIZE;
	int aneed = audioBufSize;
	uint8_t* abeg = audioBuf;
	int count = 0;

	while (encodeAudio || encodeVideo)
	{
		if (encodeVideo && (apts > vpts || !encodeAudio))
		{
			// encode video
			n = fread(videoBuf, 1, videoBufSize, i420fp);
			if (n < videoBufSize)
			{
				encodeVideo = false;
			}

			if (encodeVideo)
			{
				ret = vEncoder.encoder(videoBuf, videoBufSize, muxer.getVideoStreamIndex(), vpts, TIME_BASE, pkts);
			}
			else
			{
				ret = vEncoder.encoder(nullptr, 0, muxer.getVideoStreamIndex(), vpts, TIME_BASE, pkts);
			}

			if (ret >= 0)
			{
				for (auto iter = pkts.begin(); iter != pkts.end(); iter++)
				{
					ret = muxer.sendPacket(*iter);
				}
			}
			pkts.clear();

			n = fread(&vts, 1, 4, i420fp);
			if (n < 4)
			{
				encodeVideo = false;
			}
			else
			{
				vpts = vts * 1000;
			}
		}
		else if (encodeAudio)
		{
			// encode audio
			abeg = audioBuf;
			aneed = audioBufSize;
			while (aneed > 0)
			{
				count = aneed > aleft ? aleft : aneed;
				n = fread(abeg, 1, count, pcmfp);
				if (n < count)
				{
					encodeAudio = false;
					break;
				}

				abeg += count;
				aneed -= count;
				aleft -= count;

				if (aleft == 0)
				{
					n = fread(&ats, 1, 4, pcmfp);
					if (n < 4)
					{
						encodeAudio = false;
						break;
					}

					aleft = AUDIO_FRAME_SIZE;
				}
			}

			if (aneed == 0)
			{
				ret = aEncoder.encode(audioBuf, audioBufSize, muxer.getAudioStreamIndex(), apts, TIME_BASE, pkts);
			}
			else
			{
				ret = aEncoder.encode(nullptr, 0, muxer.getAudioStreamIndex(), apts, TIME_BASE, pkts);
			}

			if (ret >= 0)
			{
				for (auto iter = pkts.begin(); iter != pkts.end(); iter++)
				{
					ret = muxer.sendPacket(*iter);
				}
			}
			pkts.clear();

			if (aleft < AUDIO_FRAME_SIZE)
			{
				apts = ats * 1000 + 1.0 * 1000 * (AUDIO_FRAME_SIZE - aleft) / AUDIO_CHANNELS / 2 / AUDIO_SAMPLE_RATE;
			}
		}
	}

	ret = muxer.sendTrailer();
	if (ret < 0)
	{
		printf("fail to write trailer, ret=%d\n", ret);
		return -1;
	}

	printf("record finish\n");

	if (i420fp)
	{
		fclose(i420fp);
	}

	if (pcmfp)
	{
		fclose(pcmfp);
	}

	return 0;
}
