#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../core/Guard.hpp"
extern "C" {
	#include "../config.h"
	#include "../localisation/localisation.h"
	#include "../OpenRCT2.h"
	#include "../platform/platform.h"
	#include "../rct2.h"
	#include "audio.h"
}
#include "mixer.h"
#include <cmath>
#include "../core/Math.hpp"
#include "../core/Util.hpp"

Mixer gMixer;

Source::~Source()
{

}

unsigned long Source::GetSome(unsigned long offset, const uint8** data, unsigned long length)
{
	if (offset >= Length()) {
		return 0;
	}
	unsigned long size = length;
	if (offset + length > Length()) {
		size = Length() - offset;
	}
	return Read(offset, data, size);
}

unsigned long Source::Length()
{
	return length;
}

const AudioFormat& Source::Format()
{
	return format;
}

Source_Null::Source_Null()
{
	length = 0;
}

unsigned long Source_Null::Read(unsigned long offset, const uint8** data, unsigned long length)
{
	return 0;
}

Source_Sample::Source_Sample()
{
	data = 0;
	length = 0;
	issdlwav = false;
}

Source_Sample::~Source_Sample()
{
	Unload();
}

unsigned long Source_Sample::Read(unsigned long offset, const uint8** data, unsigned long length)
{
	*data = &Source_Sample::data[offset];
	return length;
}

bool Source_Sample::LoadWAV(const char* filename)
{
	log_verbose("Source_Sample::LoadWAV(%s)", filename);

	Unload();
	SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
	if (rw == NULL) {
		log_verbose("Error loading %s", filename);
		return false;
	}

	SDL_AudioSpec audiospec;
	memset(&audiospec, 0, sizeof(audiospec));
	SDL_AudioSpec* spec = SDL_LoadWAV_RW(rw, false, &audiospec, &data, (Uint32*)&length);
	SDL_RWclose(rw);

	if (spec != NULL) {
		format.freq = spec->freq;
		format.format = spec->format;
		format.channels = spec->channels;
		issdlwav = true;
	} else {
		log_verbose("Error loading %s, unsupported WAV format", filename);
		return false;
	}

	return true;
}

bool Source_Sample::LoadCSS1(const char *filename, unsigned int offset)
{
	log_verbose("Source_Sample::LoadCSS1(%s, %d)", filename, offset);

	Unload();
	SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
	if (rw == NULL) {
		log_verbose("Unable to load %s", filename);
		return false;
	}

	// n3ds port: the reading is a function of its own, for Mixer::LoadEffects
	bool loaded = LoadCSS1(rw, offset);
	SDL_RWclose(rw);
	return loaded;
}

// Reads sound number 'offset' from a css1.dat that is open
bool Source_Sample::LoadCSS1(SDL_RWops* rw, unsigned int offset)
{
	Unload();
	SDL_RWseek(rw, 0, RW_SEEK_SET);
	Uint32 numsounds;
	SDL_RWread(rw, &numsounds, sizeof(numsounds), 1);
	if (offset > numsounds) {
		return false;
	}
	SDL_RWseek(rw, offset * 4, RW_SEEK_CUR);
	Uint32 soundoffset;
	SDL_RWread(rw, &soundoffset, sizeof(soundoffset), 1);
	SDL_RWseek(rw, soundoffset, RW_SEEK_SET);
	Uint32 soundsize;
	SDL_RWread(rw, &soundsize, sizeof(soundsize), 1);
	length = soundsize;
#pragma pack(push, 1)
	struct WaveFormatEx
	{
		Uint16 encoding;
		Uint16 channels;
		Uint32 frequency;
		Uint32 byterate;
		Uint16 blockalign;
		Uint16 bitspersample;
		Uint16 extrasize;
	} waveformat;
#pragma pack(pop)
	assert_struct_size(waveformat, 18);
	SDL_RWread(rw, &waveformat, sizeof(waveformat), 1);
	format.freq = waveformat.frequency;
	format.format = AUDIO_S16LSB;
	format.channels = waveformat.channels;
	data = new (std::nothrow) uint8[length];
	if (!data) {
		log_verbose("Unable to allocate data");
		return false;
	}
	SDL_RWread(rw, data, length, 1);
	return true;
}

void Source_Sample::Unload()
{
	if (data) {
		if (issdlwav) {
			SDL_FreeWAV(data);
		} else {
			delete[] data;
		}
		data = 0;
	}
	issdlwav = false;
	length = 0;
}

bool Source_Sample::Convert(AudioFormat format)
{
	if(Source_Sample::format.format != format.format || Source_Sample::format.channels != format.channels || Source_Sample::format.freq != format.freq){
		SDL_AudioCVT cvt;
		if (SDL_BuildAudioCVT(&cvt, Source_Sample::format.format, Source_Sample::format.channels, Source_Sample::format.freq, format.format, format.channels, format.freq) < 0) {
			return false;
		}
		cvt.len = length;
		cvt.buf = (Uint8*)new uint8[cvt.len * cvt.len_mult];
		memcpy(cvt.buf, data, length);
		if (SDL_ConvertAudio(&cvt) < 0) {
			delete[] cvt.buf;
			return false;
		}
#ifdef __3DS__
		// n3ds port: SDL needs len * len_mult bytes while converting (several times the result).
		// Keep only the converted bytes instead of holding the whole work buffer for every sample.
		if ((uint32)cvt.len_cvt < (uint32)(cvt.len * cvt.len_mult)) {
			uint8 *shrunk = new (std::nothrow) uint8[cvt.len_cvt];
			if (shrunk != nullptr) {
				memcpy(shrunk, cvt.buf, cvt.len_cvt);
				delete[] cvt.buf;
				cvt.buf = shrunk;
			}
		}
#endif
		Unload();
		data = cvt.buf;
		length = cvt.len_cvt;
		Source_Sample::format = format;
		return true;
	}
	return false;
}

Source_SampleStream::~Source_SampleStream()
{
	Unload();
}

unsigned long Source_SampleStream::Read(unsigned long offset, const uint8** data, unsigned long length)
{
	if (length > buffersize) {
		if (buffer) {
			delete[] buffer;
		}
		buffer = new (std::nothrow) uint8[length];
		if (!buffer) {
			return 0;
		}
		buffersize = length;
	}
	Sint64 currentposition = SDL_RWtell(rw);
	if (currentposition == -1) {
		return 0;
	}
	if (currentposition - databegin != offset) {
		Sint64 newposition = SDL_RWseek(rw, databegin + offset, SEEK_SET);
		if (newposition == -1) {
			return 0;
		}
		currentposition = newposition;
	}
	*data = buffer;
	size_t read = SDL_RWread(rw, buffer, 1, length);
	if (read == (size_t)-1) {
		return 0;
	}
	return (unsigned long)read;
}

bool Source_SampleStream::LoadWAV(SDL_RWops* rw)
{
	Unload();
	if (rw == NULL) {
		return false;
	}
	Source_SampleStream::rw = rw;
	Uint32 chunk_id = SDL_ReadLE32(rw);
	const Uint32 RIFF = 0x46464952;
	if (chunk_id != RIFF) {
		log_verbose("Not a WAV file");
		return false;
	}
	Uint32 chunk_size = SDL_ReadLE32(rw);
	(void)chunk_size;
	Uint32 chunk_format = SDL_ReadLE32(rw);
	const Uint32 WAVE = 0x45564157;
	if (chunk_format != WAVE) {
		log_verbose("Not in WAVE format");
		return false;
	}
	const Uint32 FMT = 0x20746D66;
	Uint32 fmtchunk_size = FindChunk(rw, FMT);
	if (!fmtchunk_size) {
		log_verbose("Could not find FMT chunk");
		return false;
	}
	Uint64 chunkstart = SDL_RWtell(rw);
#pragma pack(push, 1)
	struct WaveFormat
	{
		Uint16 encoding;
		Uint16 channels;
		Uint32 frequency;
		Uint32 byterate;
		Uint16 blockalign;
		Uint16 bitspersample;
	} waveformat;
#pragma pack(pop)
	assert_struct_size(waveformat, 16);
	SDL_RWread(rw, &waveformat, sizeof(waveformat), 1);
	SDL_RWseek(rw, chunkstart + fmtchunk_size, RW_SEEK_SET);
	const Uint16 pcmformat = 0x0001;
	if (waveformat.encoding != pcmformat) {
		log_verbose("Not in proper format");
		return false;
	}
	format.freq = waveformat.frequency;
	switch (waveformat.bitspersample) {
		case 8:
			format.format = AUDIO_U8;
			break;
		case 16:
			format.format = AUDIO_S16LSB;
			break;
		default:
			log_verbose("Invalid bits per sample");
			return false;
			break;
	}
	format.channels = waveformat.channels;
	const Uint32 DATA = 0x61746164;
	Uint32 datachunk_size = FindChunk(rw, DATA);
	if (!datachunk_size) {
		log_verbose("Could not find DATA chunk");
		return false;
	}
	length = datachunk_size;
	databegin = SDL_RWtell(rw);
	return true;
}

Uint32 Source_SampleStream::FindChunk(SDL_RWops* rw, Uint32 wanted_id)
{
	Uint32 subchunk_id = SDL_ReadLE32(rw);
	Uint32 subchunk_size = SDL_ReadLE32(rw);
	if (subchunk_id == wanted_id) {
		return subchunk_size;
	}
	const Uint32 FACT = 0x74636166;
	const Uint32 LIST = 0x5453494c;
	const Uint32 BEXT = 0x74786562;
	const Uint32 JUNK = 0x4B4E554A;
	while (subchunk_id == FACT || subchunk_id == LIST || subchunk_id == BEXT || subchunk_id == JUNK) {
		SDL_RWseek(rw, subchunk_size, RW_SEEK_CUR);
		subchunk_id = SDL_ReadLE32(rw);
		subchunk_size = SDL_ReadLE32(rw);
		if (subchunk_id == wanted_id) {
			return subchunk_size;
		}
	}
	return 0;
}

void Source_SampleStream::Unload()
{
	if (rw) {
		SDL_RWclose(rw);
		rw = NULL;
	}
	length = 0;
	if (buffer) {
		delete[] buffer;
		buffer = 0;
	}
	buffersize = 0;
}

Channel::Channel()
{
	SetRate(1);
	SetVolume(SDL_MIX_MAXVOLUME);
	SetPan(0.5f);
}

Channel::~Channel()
{
	if (resampler) {
		speex_resampler_destroy(resampler);
		resampler = 0;
	}
	if (deletesourceondone) {
		delete source;
	}
}

void Channel::Play(Source& source, int loop = MIXER_LOOP_NONE)
{
	Channel::source = &source;
	Channel::loop = loop;
	offset = 0;
	done = false;
}

void Channel::SetRate(double rate)
{
	Channel::rate = rate;
	if (Channel::rate < 0.001) {
		Channel::rate = 0.001;
	}
}

void Channel::SetVolume(int volume)
{
	Channel::volume = volume;
	if (volume > SDL_MIX_MAXVOLUME) {
		Channel::volume = SDL_MIX_MAXVOLUME;
	}
	if (volume < 0) {
		Channel::volume = 0;
	}
}

void Channel::SetPan(float pan)
{
	Channel::pan = pan;
	if (pan > 1) {
		Channel::pan = 1;
	}
	if (pan < 0) {
		Channel::pan = 0;
	}
	double decibels = (std::abs(Channel::pan - 0.5) * 2.0) * 100.0;
	double attenuation = pow(10, decibels / 20.0);
	if (Channel::pan <= 0.5) {
		volume_l = 1.0;
		volume_r = float(1.0 / attenuation);
	} else {
		volume_r = 1.0;
		volume_l = float(1.0 / attenuation);
	}
}

bool Channel::IsPlaying()
{
	return !done;
}

unsigned long Channel::GetOffset()
{
	return offset;
}

bool Channel::SetOffset(unsigned long offset)
{
	if (source && offset < source->Length()) {
		int samplesize = source->Format().channels * source->Format().BytesPerSample();
		Channel::offset = (offset / samplesize) * samplesize;
		return true;
	}
	return false;
}

void Channel::SetGroup(int group)
{
	Channel::group = group;
}

Mixer::Mixer()
{
	effectbuffer = 0;
	volume = 1;
	for (size_t i = 0; i < Util::CountOf(css1sources); i++) {
		css1sources[i] = 0;
	}
	for (size_t i = 0; i < Util::CountOf(musicsources); i++) {
		musicsources[i] = 0;
	}
}

void Mixer::Init(const char* device)
{
#ifdef __3DS__
	// n3ds port: not when the sound effects were loaded ahead (N3dsPreloadEffects): Close frees
	// them, and nothing else is open yet
	bool effectsPreloaded = n3dsEffectsPreloaded;
	n3dsEffectsPreloaded = false;
	if (!effectsPreloaded)
#endif
	Close();
	SDL_AudioSpec want, have;
	SDL_zero(want);
#ifdef __3DS__
	// n3ds port: all RCT2 sounds and music are 22050 Hz. Mixing at 44100 Hz doubles the memory of
	// the pre-converted sound effects and the per-frame mixing work without improving quality.
	want.freq = 22050;
#else
	want.freq = 44100;
#endif
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = Callback;
	want.userdata = this;
	deviceid = SDL_OpenAudioDevice(device, 0, &want, &have, 0);
	format.format = have.format;
	format.channels = have.channels;
	format.freq = have.freq;
#ifdef __3DS__
	if (!effectsPreloaded)
#endif
	LoadEffects();
	effectbuffer = new uint8[(have.samples * format.BytesPerSample() * format.channels)];
	SDL_PauseAudioDevice(deviceid, 0);
}

#ifdef __3DS__
// n3ds port: at the start of the game the sound effects are loaded before the sound device is
// opened, while the HOME menu still shows its start-up logo: the device cannot be opened before
// the HOME menu has handed over (n3ds.c __appInit). In two steps, because the HOME menu needs
// the SD card to itself for a moment before it hands over, and the game should then be busy
// with something that does not read the card:
// N3dsReadEffectsFile reads the effects' file into memory, early (openrct2_initialise);
// N3dsPreloadEffects makes the effects out of it, which is computing only, as the last thing
// before the game waits for the hand-over (rct2_init).
void Mixer::N3dsReadEffectsFile()
{
	SDL_RWops* file = SDL_RWFromFile(get_file_path(PATH_ID_CSS1), "rb");
	if (file == nullptr) return;

	Sint64 fileSize = SDL_RWsize(file);
	if (fileSize > 0) {
		// Not from the linear heap, the place for large short-lived blocks: the screens'
		// buffers are taken from there next, and show what was in it until the first frame
		void* fileData = malloc((size_t)fileSize);
		if (fileData != nullptr && SDL_RWread(file, fileData, (size_t)fileSize, 1) == 1) {
			n3dsEffectsFile = fileData;
			n3dsEffectsFileSize = (size_t)fileSize;
		} else {
			free(fileData);
		}
	}
	SDL_RWclose(file);
}

// The effects are converted to the format Init asks the device for, which is the one it gets:
// SDL_OpenAudioDevice is told to allow no changes and converts by itself if the device differs.
void Mixer::N3dsPreloadEffects()
{
	format.format = AUDIO_S16SYS;
	format.channels = 2;
	format.freq = 22050;
	LoadEffects();
	n3dsEffectsPreloaded = true;
}
#endif

// n3ds port: out of Init, a function of its own
void Mixer::LoadEffects()
{
	const char* filename = get_file_path(PATH_ID_CSS1);
#ifdef __3DS__
	// n3ds port: the effects are taken from the file in memory (5.4 MB, read at once).
	// LoadCSS1 opens the file for each of the 63 effects, and on a 3DS every opening searches
	// the folder and fills the 64 KB buffer of fopen, as does the seek to the effect: 2.7 s at
	// start-up, 0.9 s this way.
	if (n3dsEffectsFile == nullptr) {
		N3dsReadEffectsFile();
	}
	SDL_RWops* memory = nullptr;
	if (n3dsEffectsFile != nullptr) {
		memory = SDL_RWFromConstMem(n3dsEffectsFile, (int)n3dsEffectsFileSize);
	}
#endif
	for (int i = 0; i < (int)Util::CountOf(css1sources); i++) {
		Source_Sample* source_sample = new Source_Sample;
		bool loaded;
#ifdef __3DS__
		if (memory != nullptr)
			loaded = source_sample->LoadCSS1(memory, i);
		else
#endif
		loaded = source_sample->LoadCSS1(filename, i);
		if (loaded) {
			source_sample->Convert(format); // convert to audio output format, saves some cpu usage but requires a bit more memory, optional
			css1sources[i] = source_sample;
		} else {
			css1sources[i] = &source_null;
			delete source_sample;
		}
	}
#ifdef __3DS__
	if (memory != nullptr)
		SDL_RWclose(memory);
	free(n3dsEffectsFile);
	n3dsEffectsFile = nullptr;
#endif
}

void Mixer::Close()
{
	Lock();
	while (channels.begin() != channels.end()) {
		delete *(channels.begin());
		channels.erase(channels.begin());
	}
	Unlock();
	SDL_CloseAudioDevice(deviceid);
	for (size_t i = 0; i < Util::CountOf(css1sources); i++) {
		if (css1sources[i] && css1sources[i] != &source_null) {
			delete css1sources[i];
			css1sources[i] = 0;
		}
	}
	for (size_t i = 0; i < Util::CountOf(musicsources); i++) {
		if (musicsources[i] && musicsources[i] != &source_null) {
			delete musicsources[i];
			musicsources[i] = 0;
		}
	}
	if (effectbuffer) {
		delete[] effectbuffer;
		effectbuffer = 0;
	}
}

void Mixer::Lock()
{
	N3DS_PERF(N3DS_PERF_MIXER_WAIT, SDL_LockAudioDevice(deviceid));
}

void Mixer::Unlock()
{
	SDL_UnlockAudioDevice(deviceid);
}

Channel* Mixer::Play(Source& source, int loop, bool deleteondone, bool deletesourceondone)
{
	Lock();
	Channel* newchannel = new (std::nothrow) Channel;
	if (newchannel) {
		newchannel->Play(source, loop);
		newchannel->deleteondone = deleteondone;
		newchannel->deletesourceondone = deletesourceondone;
		channels.push_back(newchannel);
	}
	Unlock();
	return newchannel;
}

void Mixer::Stop(Channel& channel)
{
	Lock();
	channel.stopping = true;
	Unlock();
}

bool Mixer::LoadMusic(size_t pathId)
{
	if (pathId >= Util::CountOf(musicsources)) {
		return false;
	}
	if (!musicsources[pathId]) {
		const char* filename = get_file_path((int)pathId);
		Source_Sample* source_sample = new Source_Sample;
		if (source_sample->LoadWAV(filename)) {
			musicsources[pathId] = source_sample;
			return true;
		} else {
			delete source_sample;
			musicsources[pathId] = &source_null;
			return false;
		}
	} else {
		return true;
	}
}

void Mixer::SetVolume(float volume)
{
	Mixer::volume = volume;
}

void SDLCALL Mixer::Callback(void* arg, uint8* stream, int length)
{
	Mixer* mixer = (Mixer*)arg;
#ifdef __3DS__
	// n3ds port: performance log (n3ds.c)
	uint64 perfBegin = platform_n3ds_perf_begin();
#endif
	memset(stream, 0, length);
	std::list<Channel*>::iterator i = mixer->channels.begin();
	while (i != mixer->channels.end()) {
		mixer->MixChannel(*(*i), stream, length);
		if (((*i)->done && (*i)->deleteondone) || (*i)->stopping) {
			delete (*i);
			i = mixer->channels.erase(i);
		} else {
			i++;
		}
	}
#ifdef __3DS__
	platform_n3ds_perf_end(N3DS_PERF_AUDIO, perfBegin);
#endif
}

#ifdef __3DS__
// n3ds port: changes the speed of a sound by linear interpolation between its samples (frames of
// `channels` samples each). Used instead of the Speex resampler, see MixChannel.
static void ResampleLinearS16(const sint16* in, int in_len, sint16* out, int out_len, int channels)
{
	if (in_len <= 0 || out_len <= 0) {
		return;
	}
	// Position in the input, 16.16 fixed point
	uint32 step = (uint32)(((uint64)in_len << 16) / out_len);
	uint32 position = 0;
	for (int i = 0; i < out_len; i++) {
		int index = position >> 16;
		int fraction = (position >> 1) & 0x7FFF;
		const sint16* a = &in[index * channels];
		const sint16* b = (index + 1 < in_len) ? a + channels : a;
		for (int c = 0; c < channels; c++) {
			*out++ = (sint16)(a[c] + (((b[c] - a[c]) * fraction) >> 15));
		}
		position += step;
	}
}
#endif

void Mixer::MixChannel(Channel& channel, uint8* data, int length)
{
	// Do not mix channel if channel is a sound and sound is disabled
	if (channel.group == MIXER_GROUP_SOUND && !gConfigSound.sound_enabled) {
		return;
	}

	if (channel.source && channel.source->Length() > 0 && !channel.done) {
		AudioFormat streamformat = channel.source->Format();
		int loaded = 0;
		SDL_AudioCVT cvt;
		cvt.len_ratio = 1;
		do {
			int samplesize = format.channels * format.BytesPerSample();
			int samples = length / samplesize;
			int samplesloaded = loaded / samplesize;
			double rate = 1;
			if (format.format == AUDIO_S16SYS) {
				rate = channel.rate;
			}
			int samplestoread = (int)((samples - samplesloaded) * rate);
			int lengthloaded = 0;
			if (channel.offset < channel.source->Length()) {
				bool mustconvert = false;
				if (MustConvert(*channel.source)) {
					if (SDL_BuildAudioCVT(&cvt, streamformat.format, streamformat.channels, streamformat.freq, Mixer::format.format, Mixer::format.channels, Mixer::format.freq) == -1) {
						break;
					}
					mustconvert = true;
				}

				const uint8* datastream = 0;
				int toread = (int)(samplestoread / cvt.len_ratio) * samplesize;
				int readfromstream = (channel.source->GetSome(channel.offset, &datastream, toread));
				if (readfromstream == 0) {
					break;
				}

				uint8* dataconverted = 0;
				const uint8* tomix = 0;

				if (mustconvert) {
					// tofix: there seems to be an issue with converting audio using SDL_ConvertAudio in the callback vs preconverted, can cause pops and static depending on sample rate and channels
					if (Convert(cvt, datastream, readfromstream, &dataconverted)) {
						tomix = dataconverted;
						lengthloaded = cvt.len_cvt;
					} else {
						break;
					}
				} else {
					tomix = datastream;
					lengthloaded = readfromstream;
				}

				bool effectbufferloaded = false;
				if (rate != 1 && format.format == AUDIO_S16SYS) {
					int in_len = (int)((double)lengthloaded / samplesize);
#ifdef __3DS__
					// n3ds port: linear interpolation instead of the Speex resampler, which is
					// far too slow for the 3DS (measured: zoomed out, when up to 28 vehicle
					// sounds play, mixing took longer than the sound it made, the sound broke
					// up and the game waited up to 140 ms per frame for the mixer's lock).
					int out_len = samples - samplesloaded;
					if (readfromstream != toread) {
						// reached the end of the sound: what its rate makes of the rest
						out_len = Math::Min(out_len, (int)(in_len / rate));
					}
					ResampleLinearS16((const sint16*)tomix, in_len, (sint16*)effectbuffer, out_len, format.channels);
#else
					int out_len = samples;
					if (!channel.resampler) {
						channel.resampler = speex_resampler_init(format.channels, format.freq, format.freq, 0, 0);
					}
					if (readfromstream == toread) {
						// use buffer lengths for conversion ratio so that it fits exactly
						speex_resampler_set_rate(channel.resampler, in_len, samples - samplesloaded);
					} else {
						// reached end of stream so we cant use buffer length as resampling ratio
						speex_resampler_set_rate(channel.resampler, format.freq, (int)(format.freq * (1 / rate)));
					}
					speex_resampler_process_interleaved_int(channel.resampler, (const spx_int16_t*)tomix, (spx_uint32_t*)&in_len, (spx_int16_t*)effectbuffer, (spx_uint32_t*)&out_len);
#endif
					effectbufferloaded = true;
					tomix = effectbuffer;
					lengthloaded = (out_len * samplesize);
				}

				if (channel.pan != 0.5f && format.channels == 2) {
					if (!effectbufferloaded) {
						memcpy(effectbuffer, tomix, lengthloaded);
						effectbufferloaded = true;
						tomix = effectbuffer;
					}
					switch (format.format) {
						case AUDIO_S16SYS:
							EffectPanS16(channel, (sint16*)effectbuffer, lengthloaded / samplesize);
							break;
						case AUDIO_U8:
							EffectPanU8(channel, (uint8*)effectbuffer, lengthloaded / samplesize);
							break;
					}
				}

				int mixlength = lengthloaded;
				if (loaded + mixlength > length) {
					mixlength = length - loaded;
				}

				float volumeadjust = volume;
				volumeadjust *= (gConfigSound.master_volume / 100.0f);
				switch (channel.group) {
				case MIXER_GROUP_SOUND:
					volumeadjust *= (gConfigSound.sound_volume / 100.0f);

					// Cap sound volume on title screen so music is more audible
					if (gScreenFlags & SCREEN_FLAGS_TITLE_DEMO) {
						volumeadjust = Math::Min(volumeadjust, 0.75f);
					}
					break;
				case MIXER_GROUP_RIDE_MUSIC:
					volumeadjust *= (gConfigSound.ride_music_volume / 100.0f);
					break;
				}
				int startvolume = (int)(channel.oldvolume * volumeadjust);
				int endvolume = (int)(channel.volume * volumeadjust);
				if (channel.stopping) {
					endvolume = 0;
				}
				int mixvolume = (int)(channel.volume * volumeadjust);
				if (startvolume != endvolume) {
					// fade between volume levels to smooth out sound and minimize clicks from sudden volume changes
					if (!effectbufferloaded) {
						memcpy(effectbuffer, tomix, lengthloaded);
						effectbufferloaded = true;
						tomix = effectbuffer;
					}
					mixvolume = SDL_MIX_MAXVOLUME; // set to max since we are adjusting the volume ourselves
					int fadelength = mixlength / format.BytesPerSample();
					switch (format.format) {
						case AUDIO_S16SYS:
							EffectFadeS16((sint16*)effectbuffer, fadelength, startvolume, endvolume);
							break;
						case AUDIO_U8:
							EffectFadeU8((uint8*)effectbuffer, fadelength, startvolume, endvolume);
							break;
					}
				}

				SDL_MixAudioFormat(&data[loaded], tomix, format.format, mixlength, mixvolume);

				if (dataconverted) {
					delete[] dataconverted;
				}

				channel.offset += readfromstream;
			}

			loaded += lengthloaded;

			if (channel.loop != 0 && channel.offset >= channel.source->Length()) {
				if (channel.loop != -1) {
					channel.loop--;
				}
				channel.offset = 0;
			}
		} while(loaded < length && channel.loop != 0 && !channel.stopping);

		channel.oldvolume = channel.volume;
		channel.oldvolume_l = channel.volume_l;
		channel.oldvolume_r = channel.volume_r;
		if (channel.loop == 0 && channel.offset >= channel.source->Length()) {
			channel.done = true;
		}
	}
}

void Mixer::EffectPanS16(Channel& channel, sint16* data, int length)
{
#ifdef __3DS__
	// n3ds port: the same in 16.16 fixed point. Converting every sample to float and back is
	// slow on the 3DS, and this runs for every vehicle sound.
	if (length <= 0) {
		return;
	}
	sint32 left = (sint32)(channel.oldvolume_l * 65536.0f);
	sint32 right = (sint32)(channel.oldvolume_r * 65536.0f);
	const sint32 leftStep = (sint32)((channel.volume_l - channel.oldvolume_l) * 65536.0f / (length * 2));
	const sint32 rightStep = (sint32)((channel.volume_r - channel.oldvolume_r) * 65536.0f / (length * 2));
	for (int i = 0; i < length * 2; i += 2) {
		data[i] = (sint16)((data[i] * left) >> 16);
		data[i + 1] = (sint16)((data[i + 1] * right) >> 16);
		left += leftStep;
		right += rightStep;
	}
#else
	const float dt = 1.0f / (length * 2);
	float left_volume = channel.oldvolume_l;
	float right_volume = channel.oldvolume_r;
	const float d_left = dt * (channel.volume_l - channel.oldvolume_l);
	const float d_right = dt * (channel.volume_r - channel.oldvolume_r);

	for (int i = 0; i < length * 2; i += 2) {
		data[i] = (sint16)(data[i] * left_volume);
		data[i + 1] = (sint16)(data[i + 1] * right_volume);
		left_volume += d_left;
		right_volume += d_right;
	}
#endif
}

void Mixer::EffectPanU8(Channel& channel, uint8* data, int length)
{
	for (int i = 0; i < length * 2; i += 2) {
		float t = (float)i / (length * 2);
		data[i] = (uint8)(data[i] * ((1.0 - t) * channel.oldvolume_l + t * channel.volume_l));
		data[i + 1] = (uint8)(data[i + 1] * ((1.0 - t) * channel.oldvolume_r + t * channel.volume_r));
	}
}

void Mixer::EffectFadeS16(sint16* data, int length, int startvolume, int endvolume)
{
#ifdef __3DS__
	// n3ds port: the same in fixed point (the original divides in float for every sample).
	// The volume (0..SDL_MIX_MAXVOLUME = 128) is kept as 8.24, and applied as 1.15.
	if (length <= 0) {
		return;
	}
	sint32 volume = startvolume * (1 << 17);
	const sint32 step = (endvolume - startvolume) * (1 << 17) / length;
	for (int i = 0; i < length; i++) {
		data[i] = (sint16)((data[i] * (volume >> 9)) >> 15);
		volume += step;
	}
#else
	float startvolume_f = (float)startvolume / SDL_MIX_MAXVOLUME;
	float endvolume_f = (float)endvolume / SDL_MIX_MAXVOLUME;
	for (int i = 0; i < length; i++) {
		float t = (float)i / length;
		data[i] = (sint16)(data[i] * ((1 - t) * startvolume_f + t * endvolume_f));
	}
#endif
}

void Mixer::EffectFadeU8(uint8* data, int length, int startvolume, int endvolume)
{
	float startvolume_f = (float)startvolume / SDL_MIX_MAXVOLUME;
	float endvolume_f = (float)endvolume / SDL_MIX_MAXVOLUME;
	for (int i = 0; i < length; i++) {
		float t = (float)i / length;
		data[i] = (uint8)(data[i] * ((1 - t) * startvolume_f + t * endvolume_f));
	}
}

bool Mixer::MustConvert(Source& source)
{
	const AudioFormat sourceformat = source.Format();
	if (sourceformat.format != format.format || sourceformat.channels != format.channels || sourceformat.freq != format.freq) {
		return true;
	}
	return false;
}

bool Mixer::Convert(SDL_AudioCVT& cvt, const uint8* data, unsigned long length, uint8** dataout)
{
	if (length == 0 || cvt.len_mult == 0) {
		return false;
	}
	cvt.len = length;
	cvt.buf = (Uint8*)new uint8[cvt.len * cvt.len_mult];
	memcpy(cvt.buf, data, length);
	if (SDL_ConvertAudio(&cvt) < 0) {
		delete[] cvt.buf;
		return false;
	}
	*dataout = cvt.buf;
	return true;
}

void Mixer_Init(const char* device)
{
	if (gOpenRCT2Headless) return;

	gMixer.Init(device);
}

#ifdef __3DS__
void Mixer_N3dsReadEffectsFile()
{
	if (gOpenRCT2Headless) return;

	gMixer.N3dsReadEffectsFile();
}

void Mixer_N3dsPreloadEffects()
{
	if (gOpenRCT2Headless) return;

	gMixer.N3dsPreloadEffects();
}
#endif

void* Mixer_Play_Effect(size_t id, int loop, int volume, float pan, double rate, int deleteondone)
{
	if (gOpenRCT2Headless) return 0;

	if (!gConfigSound.sound_enabled) {
		return 0;
	}
	if (id >= Util::CountOf(gMixer.css1sources)) {
		log_error("Tried to play an invalid sound id. %i", id);
		return 0;
	}
	gMixer.Lock();
	Channel* channel = gMixer.Play(*gMixer.css1sources[id], loop, deleteondone != 0, false);
	if (channel) {
		channel->SetVolume(volume);
		channel->SetPan(pan);
		channel->SetRate(rate);
	}
	gMixer.Unlock();
	return channel;
}

void Mixer_Stop_Channel(void* channel)
{
	if (gOpenRCT2Headless) return;

	gMixer.Stop(*(Channel*)channel);
}

void Mixer_Channel_Volume(void* channel, int volume)
{
	if (gOpenRCT2Headless) return;

	gMixer.Lock();
	((Channel*)channel)->SetVolume(volume);
	gMixer.Unlock();
}

void Mixer_Channel_Pan(void* channel, float pan)
{
	if (gOpenRCT2Headless) return;

	gMixer.Lock();
	((Channel*)channel)->SetPan(pan);
	gMixer.Unlock();
}

void Mixer_Channel_Rate(void* channel, double rate)
{
	if (gOpenRCT2Headless) return;

	gMixer.Lock();
	((Channel*)channel)->SetRate(rate);
	gMixer.Unlock();
}

int Mixer_Channel_IsPlaying(void* channel)
{
	if (gOpenRCT2Headless) return false;

	return ((Channel*)channel)->IsPlaying();
}

unsigned long Mixer_Channel_GetOffset(void* channel)
{
	if (gOpenRCT2Headless) return 0;

	return ((Channel*)channel)->GetOffset();
}

int Mixer_Channel_SetOffset(void* channel, unsigned long offset)
{
	if (gOpenRCT2Headless) return 0;

	return ((Channel*)channel)->SetOffset(offset);
}

void Mixer_Channel_SetGroup(void* channel, int group)
{
	if (gOpenRCT2Headless) return;

	((Channel*)channel)->SetGroup(group);
}

void* Mixer_Play_Music(int pathId, int loop, int streaming)
{
	if (gOpenRCT2Headless) return 0;

	if (streaming) {
		const utf8 *filename = get_file_path(pathId);

		SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
		if (rw != NULL) {
			Source_SampleStream* source_samplestream = new Source_SampleStream;
			if (source_samplestream->LoadWAV(rw)) {
				Channel* channel = gMixer.Play(*source_samplestream, loop, false, true);
				if (!channel) {
					delete source_samplestream;
				} else {
					channel->SetGroup(MIXER_GROUP_RIDE_MUSIC);
				}
				return channel;
			} else {
				delete source_samplestream;
			}
		}
	} else {
		if (gMixer.LoadMusic(pathId)) {
			Channel* channel = gMixer.Play(*gMixer.musicsources[pathId], MIXER_LOOP_INFINITE, false, false);
			if (channel) {
				channel->SetGroup(MIXER_GROUP_RIDE_MUSIC);
			}
			return channel;
		}
	}
	return NULL;
}

void Mixer_SetVolume(float volume)
{
	if (gOpenRCT2Headless) return;

	gMixer.SetVolume(volume);
}
