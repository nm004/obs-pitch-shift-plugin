#include <media-io/audio-resampler.h>
#include "obs-module.h"
#include "plugin-support.h"
#include "FormantShifterLoggerInterface.h"
#include "FormantShifter.h"
#include "StaffPad/TimeAndPitch.h"
#include <cmath>
#include <utility>
#include <optional>

using namespace std;
using namespace staffpad;

namespace {

namespace pitchshift {

const int CHANNELS = 2;
const uint32_t FFT_SIZE = 4096;
const char *S_PITCH_RATIO_SEMITONE = "Pitch Ratio Semitone";
const char *S_FORMANT_PRESERVATION = "Formant Preservation";

class FormantShifterLoggerMock : public FormantShifterLoggerInterface {
public:
	void NewSamplesComing(int sampleCount) override {}
	void Log(int value, const char* name) const override {}
	void Log(const float* samples, size_t size, const char* name) const override {}
	void Log(const std::complex<float>* samples, size_t size, const char* name,
		const std::function<float(const std::complex<float>&)>& transform)
		const override {}
	void ProcessFinished(std::complex<float>* spectrum, size_t fftSize) override {}
};

struct Data {
	FormantShifterLoggerMock mLogger;
	FormantShifter mFormantShifter;
	optional<TimeAndPitch> mPitchShifter;
	//size_t channels;
	float pitch_ratio;
	bool preserve_formants;

	Data(int sampleRate)
		: mLogger()
		, mFormantShifter(sampleRate, 0.002, mLogger)
		, pitch_ratio(1.0f)
		, preserve_formants(false)
	{}
};

const char *get_name(void *type_data)
{
	return "Pitch Shifter";
}

void update(void *data, obs_data_t *settings);

void *create(obs_data_t *settings, obs_source_t *source)
{
	auto *audio = obs_get_audio();
	int sampleRate = audio_output_get_sample_rate(audio);

	Data *data = new (bmalloc(sizeof (Data))) Data(sampleRate);

	auto cb = [data](double factor, std::complex<float>* spectrum, const float* magnitude)  {
		data->mFormantShifter.Process(magnitude, spectrum, factor);
	};
	data->mPitchShifter.emplace(FFT_SIZE, true, std::move(cb));
	data->mPitchShifter->setup(CHANNELS, MAX_AV_PLANES * 2048);

	update(data, settings);
	return data;
}

void destroy(void *data_)
{
	auto data{static_cast<Data *>(data_)};
	data->~Data();
	bfree(data_);
}

void update(void *data_, obs_data_t *settings)
{
	auto data{static_cast<Data *>(data_)};
	float semitone = obs_data_get_double(settings, S_PITCH_RATIO_SEMITONE);
	float pitch_ratio = exp2(semitone / 12);
	data->pitch_ratio = pitch_ratio;

	bool preserve = obs_data_get_bool(settings, S_FORMANT_PRESERVATION);
	if (data->preserve_formants != preserve) {
		data->preserve_formants = preserve;
		if (preserve) {
			data->mFormantShifter.Reset(FFT_SIZE);
		} else {
			data->mFormantShifter.Reset();
		}
	}
}

void activate(void *data_)
{
	auto data{static_cast<Data *>(data_)};

	data->mPitchShifter->reset();
	if (data->preserve_formants) {
		data->mFormantShifter.Reset(FFT_SIZE);
	} else {
		data->mFormantShifter.Reset();
	}
	data->mPitchShifter->setTimeStretchAndPitchFactor(1.0, data->pitch_ratio);
	int latency = data->mPitchShifter->getLatencySamples();
	vector silenceBuffer(latency, 0.0f);
	data->mPitchShifter->feedAudio(vector(CHANNELS, silenceBuffer.data()).data(), latency);
}

obs_audio_data *filter_audio(void *data_, obs_audio_data *audio)
{
	auto data{static_cast<Data *>(data_)};
	auto adata{reinterpret_cast<float **>(audio->data)};
	data->mPitchShifter->processPitchShift(adata, audio->frames, data->pitch_ratio);
	return audio;
}

void get_defaults2(void * /* type_data */, obs_data_t *settings)
{
    obs_data_set_default_double(settings, S_PITCH_RATIO_SEMITONE, 0);
	obs_data_set_default_bool(settings, S_FORMANT_PRESERVATION, false);
}

obs_properties_t *get_properties2(void * /* data */, void * /* type_data */)
{
 	auto ppts{obs_properties_create()};
 	obs_property_t *prop;
 	// Conversion will not work well if ratio is below -10.
	prop = obs_properties_add_float_slider(ppts, S_PITCH_RATIO_SEMITONE, "Pitch Semitone", -12, 12, 0.1);
	obs_property_float_set_suffix(prop, " semitone");
	prop = obs_properties_add_bool(ppts, S_FORMANT_PRESERVATION, "Preserve Formant");
	return ppts;
}

struct obs_source_info filter = {
	.id = "PITCH_SHIFTER_FROM_AUDACITY",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = get_name,
	.create = create,
	.destroy = destroy,
	.update = update,
	.activate = activate,
	.filter_audio = filter_audio,
	.get_defaults2 = get_defaults2,
	.get_properties2 = get_properties2,
};

} // namespace pitchshift

} // namespace

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	obs_register_source(&pitchshift::filter);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
