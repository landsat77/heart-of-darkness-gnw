
#include "mixer.h"
#ifdef GNW
#include "fileio.h"
#include "resource.h"
#endif
#include "util.h"

static void nullMixerLock(int lock) {
}

Mixer::Mixer()
	: _lock(nullMixerLock) {
	memset(_mixingQueue, 0, sizeof(_mixingQueue));
	_mixingQueueSize = 0;
}

Mixer::~Mixer() {
}

void Mixer::queue(const int16_t *ptr, const int16_t *end, int panType, int panL, int panR, bool stereo
#ifdef GNW
		, const struct SssPcm *pcm
#endif
		) {
	if (_mixingQueueSize >= kMixingQueueSize) {
		warning("MixingQueue overflow %d", _mixingQueueSize);
		return;
	}
	MixerChannel *channel = &_mixingQueue[_mixingQueueSize];
	channel->ptr = ptr;
	channel->end = end;
	channel->panL = panL;
	channel->panR = panR;
	channel->panType = panType;
	channel->stereo = stereo;
#ifdef GNW
	channel->pcm = (pcm && pcm->gnwData) ? pcm : 0;
#endif
	++_mixingQueueSize;
}

static int gain(int v) {
	return v + (v / 8) + (v / 16);
}

template <bool stereo, int panning>
static void mixS16(int16_t *dst, const int16_t *src, int len, int panL, int panR) {

	static const int kPanBits = 14; // 0..16384

	for (int j = 0; j < len; j += 2, dst += 2) {
		const int16_t sampleL = *src++;
		const int16_t sampleR = stereo ? *src++ : sampleL;
		if (panning != 1) {
			dst[0] = CLIP(dst[0] + gain(((panL * sampleL) >> kPanBits)), -32768, 32767);
		}
		if (panning != 2) {
			dst[1] = CLIP(dst[1] + gain(((panR * sampleR) >> kPanBits)), -32768, 32767);
		}
	}
}

void Mixer::mix(int16_t *buf, int len) {
	// stereo s16
	assert((len & 1) == 0);
	if (_mixingQueueSize == 0) {
		return;
	}
	for (int i = 0; i < _mixingQueueSize; ++i) {
		const MixerChannel *channel = &_mixingQueue[i];
		const int panL = channel->panL;
		const int panR = channel->panR;
#ifdef GNW
		/* PCM in place: the .sss strides decoded here (each stride: 256
		   16-bit values, then one 8-bit index per sample) */
		static int16_t decoded[2 * 1792 * 2];
		if (channel->pcm) {
			const SssPcm *pcm = channel->pcm;
			const int count = channel->stereo ? len : len / 2;
			const uint32_t words = pcm->strideSize - 256 * sizeof(int16_t);
			uint32_t pos = channel->ptr - pcm->ptr;
			const uint32_t total = pcm->pcmSize / sizeof(int16_t);
			for (int k = 0; k < count && k < (int)(sizeof(decoded) / sizeof(decoded[0])); ++k, ++pos) {
				if (pos >= total) {
					decoded[k] = 0;
					continue;
				}
				const uint8_t *stride = pcm->gnwData + (pos / words) * pcm->strideSize;
				const uint8_t *v = stride + stride[256 * sizeof(int16_t) + pos % words] * sizeof(int16_t);
				decoded[k] = (int16_t)(v[0] | (v[1] << 8));
			}
			MixerChannel copy = *channel;
			copy.ptr = decoded;
			copy.end = decoded + count;
			copy.pcm = 0;
			channel = &copy;
			mixChannel(buf, len, channel, panL, panR);
			continue;
		}
		mixChannel(buf, len, channel, panL, panR);
	}
}

void Mixer::mixChannel(int16_t *buf, int len, const MixerChannel *channel, int panL, int panR) {
	{
#endif
		if (channel->stereo) {
			assert(channel->ptr + len <= channel->end);
			switch (channel->panType) {
			case 1:
				mixS16<true, 1>(buf, channel->ptr, len, panL, panR);
				break;
			case 2:
				mixS16<true, 2>(buf, channel->ptr, len, panL, panR);
				break;
			default:
				mixS16<true, 0>(buf, channel->ptr, len, panL, panR);
				break;
			}
		} else {
			assert(channel->ptr + len / 2 <= channel->end);
			switch (channel->panType) {
			case 1:
				mixS16<false, 1>(buf, channel->ptr, len, panL, panR);
				break;
			case 2:
				mixS16<false, 2>(buf, channel->ptr, len, panL, panR);
				break;
			default:
				mixS16<false, 0>(buf, channel->ptr, len, panL, panR);
				break;
			}
		}
	}
}
