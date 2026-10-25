
#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"

struct MixerChannel {
	const int16_t *ptr;
	const int16_t *end;
	int panL;
	int panR;
	uint8_t panType;
	bool stereo;
#ifdef GNW
	const struct SssPcm *pcm; /* played in place: decode from pcm->gnwData */
#endif
};

struct Mixer {

	static const int kMixingQueueSize = 32;

	void (*_lock)(int);

	MixerChannel _mixingQueue[kMixingQueueSize];
	int _mixingQueueSize;

	Mixer();
	~Mixer();

	void queue(const int16_t *ptr, const int16_t *end, int panType, int panL, int panR, bool stereo
#ifdef GNW
		, const struct SssPcm *pcm = 0
#endif
		);

	void mix(int16_t *buf, int len);
#ifdef GNW
	void mixChannel(int16_t *buf, int len, const MixerChannel *channel, int panL, int panR);
#endif
};

struct MixerLock {
	Mixer *_mix;
	MixerLock(Mixer *mix)
		: _mix(mix) {
		_mix->_lock(1);
	}
	~MixerLock() {
		_mix->_lock(0);
	}
};

#endif
