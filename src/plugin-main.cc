#include "obs-module.h"
#include "plugin-support.h"
#include "FormantShifterLoggerInterface.h"
#include "FormantShifter.h"
#include "StaffPad/TimeAndPitch.h"
#include <utility>
#include <optional>

using namespace std;
using namespace staffpad;

namespace {

const uint32_t FFT_SIZE = 4096;

class FormantShifterLoggerMock : public FormantShifterLoggerInterface {
public:
	void NewSamplesComing(int sampleCount) override {}

public:
	void Log(int value, const char* name) const override {}
	void Log(const float* samples, size_t size, const char* name) const override {}
	void Log(const std::complex<float>* samples, size_t size, const char* name,
		const std::function<float(const std::complex<float>&)>& transform)
		const override {}
	void ProcessFinished(std::complex<float>* spectrum, size_t fftSize) override {}
private:
   /* No private members. */
};

struct Data {
	FormantShifter mFormantShifter;
	optional<TimeAndPitch> mPitchShifter;
	//size_t channels;
};

const char *get_name(void *type_data)
{
	return "Pitch Shifter";
}

void update(void *data, obs_data_t *settings);

void *create(obs_data_t *settings, obs_source_t *source)
{
	FormantShifterLoggerMock formantShifterLoggerMock;
	Data *data = new (bmalloc(sizeof (Data))) Data {
		FormantShifter (48000, 0.002, formantShifterLoggerMock),
	};

	auto cb = [data](double factor, std::complex<float>* spectrum, const float* magnitude)  {
		//data->mFormantShifter.Process(magnitude, spectrum, factor);
	};
	data->mPitchShifter.emplace(FFT_SIZE, true, std::move(cb));
	data->mPitchShifter->setup(2, MAX_AUDIO_CHANNELS * 2048);

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
	//for (const auto &p: data->ui.get_prop_info()) {
		//*p.zone = static_cast<FAUSTFLOAT>(obs_data_get_double(settings, p.name));
	//}
}

obs_audio_data *filter_audio(void *data_, obs_audio_data *audio)
{
	auto data{static_cast<Data *>(data_)};
	auto adata{reinterpret_cast<float **>(audio->data)};
	data->mPitchShifter->processPitchShift(adata, audio->frames, 1.05);
	return audio;
}

struct obs_source_info pitch_shift = {
	.id = "PITCH_SHIFT",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = get_name,
	.create = create,
	.destroy = destroy,
	.update = update,
	.filter_audio = filter_audio,
};

} // namespace


OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
bool obs_module_load(void)
{
	obs_register_source(&pitch_shift);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
