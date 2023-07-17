#include "FrozenWasteland.hpp"
#include "dsp-compressor/SimpleEnvelope.h"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"

#include <ctime>

	static constexpr float MIN_TIME = 1e-3f;
	static constexpr float MAX_TIME = 10.f;
	static constexpr float LAMBDA_BASE = MAX_TIME / MIN_TIME;


using simd::float_4;


// Accurate only on [0, 1]
template <typename T>
T sin2pi_pade_05_7_6(T x) {
	x -= 0.5f;
	return (T(-6.28319) * x + T(35.353) * simd::pow(x, 3) - T(44.9043) * simd::pow(x, 5) + T(16.0951) * simd::pow(x, 7))
	       / (1 + T(0.953136) * simd::pow(x, 2) + T(0.430238) * simd::pow(x, 4) + T(0.0981408) * simd::pow(x, 6));
}

template <typename T>
T sin2pi_pade_05_5_4(T x) {
	x -= 0.5f;
	return (T(-6.283185307) * x + T(33.19863968) * simd::pow(x, 3) - T(32.44191367) * simd::pow(x, 5))
	       / (1 + T(1.296008659) * simd::pow(x, 2) + T(0.7028072946) * simd::pow(x, 4));
}

template <typename T>
T expCurve(T x) {
	return (3 + x * (-13 + 5 * x)) / (3 + 2 * x);
}


template <int OVERSAMPLE, int QUALITY, typename T>
struct VoltageControlledOscillator {
	bool analog = false;
	bool soft = false;
	bool syncEnabled = false;
	// For optimizing in serial code
	int channels = 0;

	T lastSyncValue = 0.f;
	T phase = 0.f;
	T lastPhase = 0.f;
	T cycleComplete;
	T freq = 0.f;
	T pulseWidth = 0.5f;
	T syncDirection = 1.f;

	dsp::TRCFilter<T> sqrFilter;

	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sqrMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sawMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> triMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sinMinBlep;

	T sqrValue = 0.f;
	T sawValue = 0.f;
	T triValue = 0.f;
	T sinValue = 0.f;


	void setPulseWidth(T pulseWidth) {
		const float pwMin = 0.01f;
		this->pulseWidth = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, T syncValue) {
		// Advance phase
		T deltaPhase = simd::clamp(freq * deltaTime, 0.f, 0.35f);
		if (soft) {
			// Reverse direction
			deltaPhase *= syncDirection;
		}
		else {
			// Reset back to forward
			syncDirection = 1.f;
		}
		lastPhase = phase;
		phase += deltaPhase;
		// Wrap phase
		phase -= simd::floor(phase);
		cycleComplete = phase < lastPhase;

		// Jump sqr when crossing 0, or 1 if backwards
		T wrapPhase = (syncDirection == -1.f) & 1.f;
		T wrapCrossing = (wrapPhase - (phase - deltaPhase)) / deltaPhase;
		int wrapMask = simd::movemask((0 < wrapCrossing) & (wrapCrossing <= 1.f));
		if (wrapMask) {
			for (int i = 0; i < channels; i++) {
				if (wrapMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = wrapCrossing[i] - 1.f;
					T x = mask & (2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Jump sqr when crossing `pulseWidth`
		T pulseCrossing = (pulseWidth - (phase - deltaPhase)) / deltaPhase;
		int pulseMask = simd::movemask((0 < pulseCrossing) & (pulseCrossing <= 1.f));
		if (pulseMask) {
			for (int i = 0; i < channels; i++) {
				if (pulseMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = pulseCrossing[i] - 1.f;
					T x = mask & (-2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Jump saw when crossing 0.5
		T halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;
		int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
		if (halfMask) {
			for (int i = 0; i < channels; i++) {
				if (halfMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = halfCrossing[i] - 1.f;
					T x = mask & (-2.f * syncDirection);
					sawMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Detect sync
		// Might be NAN or outside of [0, 1) range
		if (syncEnabled) {
			T deltaSync = syncValue - lastSyncValue;
			T syncCrossing = -lastSyncValue / deltaSync;
			lastSyncValue = syncValue;
			T sync = (0.f < syncCrossing) & (syncCrossing <= 1.f) & (syncValue >= 0.f);
			int syncMask = simd::movemask(sync);
			if (syncMask) {
				if (soft) {
					syncDirection = simd::ifelse(sync, -syncDirection, syncDirection);
				}
				else {
					T newPhase = simd::ifelse(sync, (1.f - syncCrossing) * deltaPhase, phase);
					// Insert minBLEP for sync
					for (int i = 0; i < channels; i++) {
						if (syncMask & (1 << i)) {
							T mask = simd::movemaskInverse<T>(1 << i);
							float p = syncCrossing[i] - 1.f;
							T x;
							x = mask & (sqr(newPhase) - sqr(phase));
							sqrMinBlep.insertDiscontinuity(p, x);
							x = mask & (saw(newPhase) - saw(phase));
							sawMinBlep.insertDiscontinuity(p, x);
							x = mask & (tri(newPhase) - tri(phase));
							triMinBlep.insertDiscontinuity(p, x);
							x = mask & (sin(newPhase) - sin(phase));
							sinMinBlep.insertDiscontinuity(p, x);
						}
					}
					phase = newPhase;
				}
			}
		}

		// Square
		sqrValue = sqr(phase);
		sqrValue += sqrMinBlep.process();

		if (analog) {
			sqrFilter.setCutoffFreq(20.f * deltaTime);
			sqrFilter.process(sqrValue);
			sqrValue = sqrFilter.highpass() * 0.95f;
		}

		// Saw
		sawValue = saw(phase);
		sawValue += sawMinBlep.process();

		// Tri
		triValue = tri(phase);
		triValue += triMinBlep.process();

		// Sin
		sinValue = sin(phase);
		sinValue += sinMinBlep.process();
	}

	T sin(T phase) {
		T v;
		if (analog) {
			// Quadratic approximation of sine, slightly richer harmonics
			T halfPhase = (phase < 0.5f);
			T x = phase - simd::ifelse(halfPhase, 0.25f, 0.75f);
			v = 1.f - 16.f * simd::pow(x, 2);
			v *= simd::ifelse(halfPhase, 1.f, -1.f);
		}
		else {
			v = sin2pi_pade_05_5_4(phase);
			// v = sin2pi_pade_05_7_6(phase);
			// v = simd::sin(2 * T(M_PI) * phase);
		}
		return v;
	}
	T sin() {
		return sinValue;
	}

	T tri(T phase) {
		T v;
		if (analog) {
			T x = phase + 0.25f;
			x -= simd::trunc(x);
			T halfX = (x >= 0.5f);
			x *= 2;
			x -= simd::trunc(x);
			v = expCurve(x) * simd::ifelse(halfX, 1.f, -1.f);
		}
		else {
			v = 1 - 4 * simd::fmin(simd::fabs(phase - 0.25f), simd::fabs(phase - 1.25f));
		}
		return v;
	}
	T tri() {
		return triValue;
	}

	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		if (analog) {
			v = -expCurve(x);
		}
		else {
			v = 2 * x - 1;
		}
		return v;
	}

	T saw() {
		return sawValue;
	}

	T sqr(T phase) {
		T v = simd::ifelse(phase < pulseWidth, 1.f, -1.f);
		return v;
	}
	T sqr() {
		return sqrValue;
	}

	T light() {
		return simd::sin(2 * T(M_PI) * phase);
	}
};


struct FormalDinner : Module {
	enum ParamIds {
		SYNC_PARAM,
		FREQ_PARAM,
		GLOBAL_FM_PARAM,
		PRIMARY_FM_PARAM,
		FORMANT_MIX_PARAM,
		FORMANT_MIX_CV_ATTENUVERTER_PARAM,
		FORMANT_XY_BALANCE_PARAM,
		FORMANT_XY_BALANCE_CV_ATTENUVERTER_PARAM,
		FORMANT_X_BALANCE_PARAM,
		FORMANT_X_BALANCE_CV_ATTENUVERTER_PARAM,
		FORMANT_Y_BALANCE_PARAM,
		FORMANT_Y_BALANCE_CV_ATTENUVERTER_PARAM,
		FORMANT_X1_RATIO_PARAM,
		FORMANT_X1_RATIO_CV_ATTENUVERTER_PARAM,
		FORMANT_X1_DAMP_PARAM,
		FORMANT_X1_DAMP_CV_ATTENUVERTER_PARAM,
		FORMANT_X1_WARP_PARAM,
		FORMANT_X1_WARP_CV_ATTENUVERTER_PARAM,
		FORMANT_X1_WAVE_TYPE_PARAM,
		FORMANT_Y1_RATIO_PARAM,
		FORMANT_Y1_RATIO_CV_ATTENUVERTER_PARAM,
		FORMANT_Y1_DAMP_PARAM,
		FORMANT_Y1_DAMP_CV_ATTENUVERTER_PARAM,
		FORMANT_Y1_WARP_PARAM,
		FORMANT_Y1_WARP_CV_ATTENUVERTER_PARAM,
		FORMANT_Y1_WAVE_TYPE_PARAM,
		FORMANT_X2_RATIO_PARAM,
		FORMANT_X2_RATIO_CV_ATTENUVERTER_PARAM,
		FORMANT_X2_DAMP_PARAM,
		FORMANT_X2_DAMP_CV_ATTENUVERTER_PARAM,
		FORMANT_X2_WARP_PARAM,
		FORMANT_X2_WARP_CV_ATTENUVERTER_PARAM,
		FORMANT_X2_WAVE_TYPE_PARAM,
		FORMANT_Y2_RATIO_PARAM,
		FORMANT_Y2_RATIO_CV_ATTENUVERTER_PARAM,
		FORMANT_Y2_DAMP_PARAM,
		FORMANT_Y2_DAMP_CV_ATTENUVERTER_PARAM,
		FORMANT_Y2_WARP_PARAM,
		FORMANT_Y2_WARP_CV_ATTENUVERTER_PARAM,
		FORMANT_Y2_WAVE_TYPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		PRIMARY_FM_INPUT,
		GLOBAL_FM_INPUT,
		SYNC_INPUT,
		FORMANT_MIX_INPUT,
		FORMANT_XY_BALANCE_INPUT,
		FORMANT_X_BALANCE_INPUT,
		FORMANT_Y_BALANCE_INPUT,
		FORMANT_X1_RATIO_INPUT,
		FORMANT_X1_DAMP_INPUT,
		FORMANT_X1_WARP_INPUT,
		FORMANT_X1_WAVE_TYPE_INPUT,
		FORMANT_Y1_RATIO_INPUT,
		FORMANT_Y1_DAMP_INPUT,
		FORMANT_Y1_WARP_INPUT,
		FORMANT_Y1_WAVE_TYPE_INPUT,
		FORMANT_X2_RATIO_INPUT,
		FORMANT_X2_DAMP_INPUT,
		FORMANT_X2_WARP_INPUT,
		FORMANT_X2_WAVE_TYPE_INPUT,
		FORMANT_Y2_RATIO_INPUT,
		FORMANT_Y2_DAMP_INPUT,
		FORMANT_Y2_WARP_INPUT,
		FORMANT_Y2_WAVE_TYPE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MAIN_OUTPUT,
		PRIMARY_OUTPUT,
		FORMANT_X1_OUTPUT,
		FORMANT_Y1_OUTPUT,
		FORMANT_X2_OUTPUT,
		FORMANT_Y2_OUTPUT,
		ENVELOPE_X1_OUTPUT,
		ENVELOPE_Y1_OUTPUT,
		ENVELOPE_X2_OUTPUT,
		ENVELOPE_Y2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		SOFT_LIGHT,
		FORMANT_X1_WAVE_TYPE_LIGHT,
		FORMANT_Y1_WAVE_TYPE_LIGHT,
		FORMANT_X2_WAVE_TYPE_LIGHT,
		FORMANT_Y2_WAVE_TYPE_LIGHT,
		NUM_LIGHTS
	};

	VoltageControlledOscillator<16, 16, float_4> oscillators[4];
	VoltageControlledOscillator<16, 16, float_4> formantX1Oscillators[4];
	VoltageControlledOscillator<16, 16, float_4> formantX2Oscillators[4];
	VoltageControlledOscillator<16, 16, float_4> formantY1Oscillators[4];
	VoltageControlledOscillator<16, 16, float_4> formantY2Oscillators[4];
	
	dsp::SchmittTrigger softSyncTrigger,formantX1WaveShapeTrigger,formantY1WaveShapeTrigger,formantX2WaveShapeTrigger,formantY2WaveShapeTrigger;


	float_4 formantX1Envelopes[4] = {1.f};
	float_4 formantY1Envelopes[4] = {1.f};
	float_4 formantX2Envelopes[4] = {1.f};
	float_4 formantY2Envelopes[4] = {1.f};


	bool formantX1WaveShape;
	bool formantY1WaveShape;
	bool formantX2WaveShape;
	bool formantY2WaveShape;
	int warpRange = 4;
	bool softSync = false;


	//percentages
	float freqPercentage = 0;
	float globalFMPercentage =0;
	float primaryFMPercentage = 0;

	float formantMixPercentage = 0;
	float formantXYBalancePercentage = 0;
	float formantXBalancePercentage = 0;
	float formantYBalancePercentage = 0;

	float formantX1RatioPercentage = 0;
	float formantX1DampPercentage = 0;
	float formantX1WarpPercentage = 0;

	float formantY1RatioPercentage = 0;
	float formantY1DampPercentage = 0;
	float formantY1WarpPercentage = 0;

	float formantX2RatioPercentage = 0;
	float formantX2DampPercentage = 0;
	float formantX2WarpPercentage = 0;

	float formantY2RatioPercentage = 0;
	float formantY2DampPercentage = 0;
	float formantY2WarpPercentage = 0;


	FormalDinner() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(SYNC_PARAM, 0.f, 1.f, 1.f, "Sync mode", {"Soft", "Hard"});

		configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(PRIMARY_FM_PARAM, -1.f, 1.f, 0.f, "Primary VCO FM", "%", 0.f, 100.f);
		configParam(GLOBAL_FM_PARAM, -1.f, 1.f, 0.f, "Global FM", "%", 0.f, 100.f);

		configParam(FORMANT_X1_RATIO_PARAM, 1.f, 240.f, 2.f, "X1 Ratio", "x");
		configParam(FORMANT_X1_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X1 Ratio CV Attenuation","%",0,100);
		configParam(FORMANT_X1_DAMP_PARAM, 0.f, 1.f, 0.f, "X1 Damp", "%", 0.f, 100.f);
		configParam(FORMANT_X1_DAMP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X1 Damp CV Attenuation","%",0,100);
		configParam(FORMANT_X1_WARP_PARAM, -1.f, 1.f, 0.f, "X1 Warp", "%", 0.f, 100.f);
		configParam(FORMANT_X1_WARP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X1 Warp CV Attenuation","%",0,100);
		configButton(FORMANT_X1_WAVE_TYPE_PARAM,"X1 Wave Type");

		configParam(FORMANT_Y1_RATIO_PARAM, 1.f, 240.f, 2.f, "Y1 Ratio", "x");
		configParam(FORMANT_Y1_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y1 Ratio CV Attenuation","%",0,100);
		configParam(FORMANT_Y1_DAMP_PARAM, 0.f, 1.f, 0.f, "Y1 Damp", "%", 0.f, 100.f);
		configParam(FORMANT_Y1_DAMP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y1 Damp CV Attenuation","%",0,100);
		configParam(FORMANT_Y1_WARP_PARAM, -1.f, 1.f, 0.f, "Y1 Warp", "%", 0.f, 100.f);
		configParam(FORMANT_Y1_WARP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y1 Warp CV Attenuation","%",0,100);
		configButton(FORMANT_Y1_WAVE_TYPE_PARAM,"Y1 Wave Type");

		configParam(FORMANT_X2_RATIO_PARAM, 1.f, 240.f, 2.f, "X2 Ratio", "x");
		configParam(FORMANT_X2_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X2 Ratio CV Attenuation","%",0,100);
		configParam(FORMANT_X2_DAMP_PARAM, 0.f, 1.f, 0.f, "X2 Damp", "%", 0.f, 100.f);
		configParam(FORMANT_X2_DAMP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X2 Damp CV Attenuation","%",0,100);
		configParam(FORMANT_X2_WARP_PARAM, -1.f, 1.f, 0.f, "X2 Warp", "%", 0.f, 100.f);
		configParam(FORMANT_X2_WARP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X2 Warp CV Attenuation","%",0,100);
		configButton(FORMANT_X2_WAVE_TYPE_PARAM,"X2 Wave Type");

		configParam(FORMANT_Y2_RATIO_PARAM, 1.f, 240.f, 2.f, "Y2 Ratio", "x");
		configParam(FORMANT_Y2_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y2 Ratio CV Attenuation","%",0,100);
		configParam(FORMANT_Y2_DAMP_PARAM, 0.f, 1.f, 0.f, "Y2 Damp", "%", 0.f, 100.f);
		configParam(FORMANT_Y2_DAMP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y2 Damp CV Attenuation","%",0,100);
		configParam(FORMANT_Y2_WARP_PARAM, -1.f, 1.f, 0.f, "Y2 Warp", "%", 0.f, 100.f);
		configParam(FORMANT_Y2_WARP_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y2 Warp CV Attenuation","%",0,100);
		configButton(FORMANT_Y2_WAVE_TYPE_PARAM,"Y2 Wave Type");


		configParam(FORMANT_MIX_PARAM, 0.f, 1.f, 0.f, "Formants Mix", "%", 0.f, 100.f);
		configParam(FORMANT_MIX_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Formants Mix CV Attenuation","%",0,100);
		configParam(FORMANT_XY_BALANCE_PARAM, 0.f, 1.f, 0.5f, "X-Y Formants Balance", "%", 0.f, 100.f);
		configParam(FORMANT_XY_BALANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X-Y Formants Balance CV Attenuation","%",0,100);
		configParam(FORMANT_X_BALANCE_PARAM, 0.f, 1.f, 0.5f, "X Formants Balance", "%", 0.f, 100.f);
		configParam(FORMANT_X_BALANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X Formants Balance CV Attenuation","%",0,100);
		configParam(FORMANT_Y_BALANCE_PARAM, 0.f, 1.f, 0.5f, "Y Formants Balance", "%", 0.f, 100.f);
		configParam(FORMANT_Y_BALANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y Formants Balance CV Attenuation","%",0,100);



		configInput(PITCH_INPUT, "1V/octave pitch");
		configInput(PRIMARY_FM_INPUT, "Primary VCO Frequency modulation");
		configInput(GLOBAL_FM_INPUT, "Global Frequency modulation");
		configInput(SYNC_INPUT, "Sync");

		configInput(FORMANT_MIX_INPUT, "Formants Mix");
		configInput(FORMANT_XY_BALANCE_INPUT, "X-Y Formants Balance");
		configInput(FORMANT_X_BALANCE_INPUT, "X Formant Balance");
		configInput(FORMANT_Y_BALANCE_INPUT, "Y Formant Balannce");

		configInput(FORMANT_X1_RATIO_INPUT, "X1 Ratio");
		configInput(FORMANT_X1_DAMP_INPUT, "X1 Damp");
		configInput(FORMANT_X1_WARP_INPUT, "X1 Warp");

		configInput(FORMANT_Y1_RATIO_INPUT, "Y1 Ratio");
		configInput(FORMANT_Y1_DAMP_INPUT, "Y1 Damp");
		configInput(FORMANT_Y1_WARP_INPUT, "Y1 Warp");

		configInput(FORMANT_X2_RATIO_INPUT, "X2 Ratio");
		configInput(FORMANT_X2_DAMP_INPUT, "X2 Damp");
		configInput(FORMANT_X2_WARP_INPUT, "X2 Warp");

		configInput(FORMANT_Y2_RATIO_INPUT, "Y2 Ratio");
		configInput(FORMANT_Y2_DAMP_INPUT, "Y2 Damp");
		configInput(FORMANT_Y2_WARP_INPUT, "Y2 Warp");


		configOutput(MAIN_OUTPUT, "Main");
		configOutput(PRIMARY_OUTPUT, "Primary VCO");

		configOutput(FORMANT_X1_OUTPUT, "Formant X1 VCO");
		configOutput(FORMANT_Y1_OUTPUT, "Formant Y1 VCO");
		configOutput(FORMANT_X2_OUTPUT, "Formant X2 VCO");
		configOutput(FORMANT_Y2_OUTPUT, "Formant Y2 VCO");

		configOutput(ENVELOPE_X1_OUTPUT, "X1 Envelope");
		configOutput(ENVELOPE_Y1_OUTPUT, "Y1 Envelope");
		configOutput(ENVELOPE_X2_OUTPUT, "X2 Envelope");
		configOutput(ENVELOPE_Y2_OUTPUT, "Y2 Envelope");
	}


	json_t* dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "softSync", json_integer(softSync));
		json_object_set_new(rootJ, "formantX1WaveShape", json_integer(formantX1WaveShape));
		json_object_set_new(rootJ, "formantY1WaveShape", json_integer(formantY1WaveShape));
		json_object_set_new(rootJ, "formantX2WaveShape", json_integer(formantX2WaveShape));
		json_object_set_new(rootJ, "formantY2WaveShape", json_integer(formantY2WaveShape));
		json_object_set_new(rootJ, "warpRange", json_integer(warpRange));


		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {
		json_t *ssJ = json_object_get(rootJ, "softSync");
		if (ssJ) {
			softSync = json_integer_value(ssJ);			
		}	
		json_t *fx1wsJ = json_object_get(rootJ, "formantX1WaveShape");
		if (fx1wsJ) {
			formantX1WaveShape = json_integer_value(fx1wsJ);			
		}	
		json_t *fy1wsJ = json_object_get(rootJ, "formantY1WaveShape");
		if (fy1wsJ) {
			formantY1WaveShape = json_integer_value(fy1wsJ);			
		}	
		json_t *fx2wsJ = json_object_get(rootJ, "formantX2WaveShape");
		if (fx2wsJ) {
			formantX2WaveShape = json_integer_value(fx2wsJ);			
		}	
		json_t *fy2wsJ = json_object_get(rootJ, "formantY2WaveShape");
		if (fy2wsJ) {
			formantY2WaveShape = json_integer_value(fy2wsJ);			
		}	
		json_t *wrJ = json_object_get(rootJ, "warpRange");
		if (wrJ) {
			warpRange = json_integer_value(wrJ);			
		}	
	}


	void process(const ProcessArgs& args) override {
		float freqParam = params[FREQ_PARAM].getValue() / 12.f;
		float primaryFmParam = params[PRIMARY_FM_PARAM].getValue();
		float globalFmParam = params[GLOBAL_FM_PARAM].getValue();
		if (softSyncTrigger.process(params[SYNC_PARAM].getValue())) {
			softSync = !softSync;
		}

		float formantMixParam = params[FORMANT_MIX_PARAM].getValue();
		float formantXYBalanceParam = params[FORMANT_XY_BALANCE_PARAM].getValue();
		float formantXBalanceParam = params[FORMANT_X_BALANCE_PARAM].getValue();
		float formantYBalanceParam = params[FORMANT_Y_BALANCE_PARAM].getValue();


		float formantX1RatioParam = params[FORMANT_X1_RATIO_PARAM].getValue();
		float formantX1DampParam = params[FORMANT_X1_DAMP_PARAM].getValue();
		float formantX1WarpParam = params[FORMANT_X1_WARP_PARAM].getValue();
		if (formantX1WaveShapeTrigger.process(params[FORMANT_X1_WAVE_TYPE_PARAM].getValue())) {
			formantX1WaveShape = !formantX1WaveShape;
		}


		float formantY1RatioParam = params[FORMANT_Y1_RATIO_PARAM].getValue();
		float formantY1DampParam = params[FORMANT_Y1_DAMP_PARAM].getValue();
		float formantY1WarpParam = params[FORMANT_Y1_WARP_PARAM].getValue();
		if (formantY1WaveShapeTrigger.process(params[FORMANT_Y1_WAVE_TYPE_PARAM].getValue())) {
			formantY1WaveShape = !formantY1WaveShape;
		}

		float formantX2RatioParam = params[FORMANT_X2_RATIO_PARAM].getValue();
		float formantX2DampParam = params[FORMANT_X2_DAMP_PARAM].getValue();
		float formantX2WarpParam = params[FORMANT_X2_WARP_PARAM].getValue();
		if (formantX2WaveShapeTrigger.process(params[FORMANT_X2_WAVE_TYPE_PARAM].getValue())) {
			formantX2WaveShape = !formantX2WaveShape;
		}

		float formantY2RatioParam = params[FORMANT_Y2_RATIO_PARAM].getValue();
		float formantY2DampParam = params[FORMANT_Y2_DAMP_PARAM].getValue();
		float formantY2WarpParam = params[FORMANT_Y2_WARP_PARAM].getValue();
		if (formantY2WaveShapeTrigger.process(params[FORMANT_Y2_WAVE_TYPE_PARAM].getValue())) {
			formantY2WaveShape = !formantY2WaveShape;
		}


		lights[SOFT_LIGHT].value = softSync ? 1.0 : 0.0;
		lights[FORMANT_X1_WAVE_TYPE_LIGHT].value = formantX1WaveShape ? 1.0 : 0.0;
		lights[FORMANT_Y1_WAVE_TYPE_LIGHT].value = formantY1WaveShape ? 1.0 : 0.0;
		lights[FORMANT_X2_WAVE_TYPE_LIGHT].value = formantX2WaveShape ? 1.0 : 0.0;
		lights[FORMANT_Y2_WAVE_TYPE_LIGHT].value = formantY2WaveShape ? 1.0 : 0.0;


		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			auto& oscillator = oscillators[c / 4];
			oscillator.channels = std::min(channels - c, 4);
			// removed
			oscillator.analog = true;
			oscillator.soft = softSync;

			// Get frequency	
			float_4 pitch = freqParam + inputs[PITCH_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 freq;

			freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);
			freq += dsp::FREQ_C4 * inputs[PRIMARY_FM_INPUT].getPolyVoltageSimd<float_4>(c) * primaryFmParam;
			float_4 globalFm = dsp::FREQ_C4 * inputs[GLOBAL_FM_INPUT].getPolyVoltageSimd<float_4>(c) * globalFmParam;

			freq = clamp(freq, 0.f, args.sampleRate / 2.f);
			oscillator.freq = freq;

			oscillator.syncEnabled = inputs[SYNC_INPUT].isConnected();
			float_4 sync = inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c);
			oscillator.process(args.sampleTime, sync);

			float_4 primeVcoOut = oscillator.sin();
			float_4 envelope = (1.f - oscillator.phase);

			float_4 formantFreq = freq + globalFm;


			auto& formantX1Oscillator = formantX1Oscillators[c / 4];
			float_4 formantX1Ratio = simd::clamp(formantX1RatioParam + inputs[FORMANT_X1_RATIO_INPUT].getPolyVoltageSimd<float_4>(c) * 48.f * params[FORMANT_X1_RATIO_CV_ATTENUVERTER_PARAM].getValue(),1.f,240.f);
			float_4 formantX1Damp = simd::clamp(formantX1DampParam + inputs[FORMANT_X1_DAMP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_X1_DAMP_CV_ATTENUVERTER_PARAM].getValue(),0.01f,1.f);
			float_4 formantX1Warp = simd::clamp(formantX1WarpParam + inputs[FORMANT_X1_WARP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_X1_WARP_CV_ATTENUVERTER_PARAM].getValue(),-1.f,1.f);

			float_4 formantAdjustedX1Ratio = formantX1Ratio + simd::ifelse(formantX1Warp >=0,envelope * formantX1Warp,(1.f-envelope) * -formantX1Warp) * formantX1Ratio * warpRange;

			formantX1Oscillator.freq = formantFreq * formantAdjustedX1Ratio;
			formantX1Oscillator.phase = simd::ifelse(oscillator.cycleComplete != 0 , -1.f , formantX1Oscillator.phase);
			formantX1Oscillator.process(args.sampleTime, 0.f);
			float_4 formantX1Out = simd::ifelse(formantX1WaveShape,formantX1Oscillator.sqr() * 0.5f,formantX1Oscillator.sin()); // No clue why sqr needs to be halved

			auto& formantX1Envelope = formantX1Envelopes[c/4];
			formantX1Envelope = simd::ifelse(oscillator.cycleComplete != 0 , 1.f , simd::clamp(formantX1Envelope,0.f,1.f));


			auto& formantY1Oscillator = formantY1Oscillators[c / 4];
			float_4 formantY1Ratio = simd::clamp(formantY1RatioParam + inputs[FORMANT_Y1_RATIO_INPUT].getPolyVoltageSimd<float_4>(c) * 48.f * params[FORMANT_Y1_RATIO_CV_ATTENUVERTER_PARAM].getValue() ,1.f,240.f); 
			float_4 formantY1Damp = simd::clamp(formantY1DampParam + inputs[FORMANT_Y1_DAMP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_Y1_DAMP_CV_ATTENUVERTER_PARAM].getValue(),0.01f,1.f);
			float_4 formantY1Warp = simd::clamp(formantY1WarpParam + inputs[FORMANT_Y1_WARP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f  * params[FORMANT_Y1_WARP_CV_ATTENUVERTER_PARAM].getValue(),-1.f,1.f);

			float_4 formantAdjustedY1Ratio = formantY1Ratio + simd::ifelse(formantY1Warp >=0,envelope * formantY1Warp,(1.f-envelope) * -formantY1Warp) * formantY1Ratio * warpRange;

			formantY1Oscillator.freq = formantFreq * formantAdjustedY1Ratio;
			formantY1Oscillator.phase = simd::ifelse(oscillator.cycleComplete != 0 , -1.f , formantY1Oscillator.phase);
			formantY1Oscillator.process(args.sampleTime, 0.f);
			float_4 formantY1Out =  simd::ifelse(formantY1WaveShape,formantY1Oscillator.saw(),formantY1Oscillator.sin());

			auto& formantY1Envelope = formantY1Envelopes[c/4];
			formantY1Envelope = simd::ifelse(oscillator.cycleComplete != 0 , 1.f , simd::clamp(formantY1Envelope,0.f,1.f));


			auto& formantX2Oscillator = formantX2Oscillators[c / 4];
			float_4 formantX2Ratio = simd::clamp(formantX2RatioParam + inputs[FORMANT_X2_RATIO_INPUT].getPolyVoltageSimd<float_4>(c) * 48.f * params[FORMANT_X2_RATIO_CV_ATTENUVERTER_PARAM].getValue(),1.f,240.f);
			float_4 formantX2Damp = simd::clamp(formantX2DampParam + inputs[FORMANT_X2_DAMP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_X2_DAMP_CV_ATTENUVERTER_PARAM].getValue(),0.01f,1.f);
			float_4 formantX2Warp = simd::clamp(formantX2WarpParam + inputs[FORMANT_X2_WARP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_X2_WARP_CV_ATTENUVERTER_PARAM].getValue(),-1.f,1.f);

			float_4 formantAdjustedX2Ratio = formantX2Ratio + simd::ifelse(formantX2Warp >=0,envelope * formantX2Warp,(1.f-envelope) * -formantX2Warp) * formantX2Ratio * warpRange;

			formantX2Oscillator.freq = formantFreq * formantAdjustedX2Ratio;
			formantX2Oscillator.phase = simd::ifelse(oscillator.cycleComplete != 0 , -1.f , formantX2Oscillator.phase);
			formantX2Oscillator.process(args.sampleTime, 0.f);
			float_4 formantX2Out =  simd::ifelse(formantX2WaveShape,formantX2Oscillator.sqr() * 0.5f,formantX2Oscillator.sin());

			auto& formantX2Envelope = formantX2Envelopes[c/4];
			formantX2Envelope = simd::ifelse(oscillator.cycleComplete != 0 , 1.f , simd::clamp(formantX2Envelope,0.f,1.f));



			auto& formantY2Oscillator = formantY2Oscillators[c / 4];
			float_4 formantY2Ratio = simd::clamp(formantY2RatioParam + inputs[FORMANT_Y2_RATIO_INPUT].getPolyVoltageSimd<float_4>(c) * 48.f * params[FORMANT_Y2_RATIO_CV_ATTENUVERTER_PARAM].getValue(),1.f,240.f);
			float_4 formantY2Damp = simd::clamp(formantY2DampParam + inputs[FORMANT_Y2_DAMP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_Y2_DAMP_CV_ATTENUVERTER_PARAM].getValue(),0.01f,1.f);
			float_4 formantY2Warp = simd::clamp(formantY2WarpParam + inputs[FORMANT_Y2_WARP_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_Y2_WARP_CV_ATTENUVERTER_PARAM].getValue(),-1.f,1.f);

			float_4 formantAdjustedY2Ratio = formantY2Ratio + simd::ifelse(formantY2Warp >=0,envelope * formantY2Warp,(1.f-envelope) * -formantY2Warp) * formantY2Ratio * warpRange;
 
			formantY2Oscillator.freq = formantFreq * formantAdjustedY2Ratio;
			formantY2Oscillator.phase = simd::ifelse(oscillator.cycleComplete != 0 , -1.f , formantY2Oscillator.phase);
			formantY2Oscillator.process(args.sampleTime, 0.f);
			float_4 formantY2Out =  simd::ifelse(formantY2WaveShape,formantY2Oscillator.saw(),formantY2Oscillator.sin());;

			auto& formantY2Envelope = formantY2Envelopes[c/4];
			formantY2Envelope = simd::ifelse(oscillator.cycleComplete != 0 , 1.f , simd::clamp(formantY2Envelope,0.f,1.f));


			float_4 formantMix = simd::clamp(formantMixParam + inputs[FORMANT_MIX_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_MIX_CV_ATTENUVERTER_PARAM].getValue(),0.f,1.f);
			float_4 formantXYBalance = simd::clamp(formantXYBalanceParam + inputs[FORMANT_XY_BALANCE_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_XY_BALANCE_CV_ATTENUVERTER_PARAM].getValue(),0.f,1.f);
			float_4 formantXBalance = simd::clamp(formantXBalanceParam + inputs[FORMANT_X_BALANCE_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_X_BALANCE_CV_ATTENUVERTER_PARAM].getValue(),0.f,1.f);
			float_4 formantYBalance = simd::clamp(formantYBalanceParam + inputs[FORMANT_Y_BALANCE_INPUT].getPolyVoltageSimd<float_4>(c) / 5.f * params[FORMANT_Y_BALANCE_CV_ATTENUVERTER_PARAM].getValue(),0.f,1.f);


			float_4 formantXOut = (formantX1Out * formantX1Envelope * (1.f - formantXBalance)) + ((formantX2Out * formantX2Envelope * formantXBalance));
			float_4 formantYOut = (formantY1Out * formantY1Envelope * (1.f - formantYBalance)) + ((formantY2Out * formantY2Envelope * formantYBalance));
			float_4 formantOut = (formantXOut * (1.f - formantXYBalance)) + ((formantYOut * formantXYBalance));

			float_4 mainOut = (primeVcoOut * (1.f - formantMix)) + (formantOut * formantMix);

			// Set output
			if (outputs[MAIN_OUTPUT].isConnected())
				outputs[MAIN_OUTPUT].setVoltageSimd(5.f * mainOut, c);

			if (outputs[PRIMARY_OUTPUT].isConnected())
				outputs[PRIMARY_OUTPUT].setVoltageSimd(5.f * primeVcoOut, c);


			if (outputs[FORMANT_X1_OUTPUT].isConnected())
				outputs[FORMANT_X1_OUTPUT].setVoltageSimd(5.f * formantX1Out, c);

			if (outputs[FORMANT_Y1_OUTPUT].isConnected())
				outputs[FORMANT_Y1_OUTPUT].setVoltageSimd(5.f * formantY1Out, c);

			if (outputs[FORMANT_X2_OUTPUT].isConnected())
				outputs[FORMANT_X2_OUTPUT].setVoltageSimd(5.f * formantX2Out, c);

			if (outputs[FORMANT_Y2_OUTPUT].isConnected())
				outputs[FORMANT_Y2_OUTPUT].setVoltageSimd(5.f * formantY2Out, c);


			if (outputs[ENVELOPE_X1_OUTPUT].isConnected())
				outputs[ENVELOPE_X1_OUTPUT].setVoltageSimd(10.f * formantX1Envelope, c);

			if (outputs[ENVELOPE_Y1_OUTPUT].isConnected())	 
				outputs[ENVELOPE_Y1_OUTPUT].setVoltageSimd(10.f * formantY1Envelope, c);

			if (outputs[ENVELOPE_X2_OUTPUT].isConnected())
				outputs[ENVELOPE_X2_OUTPUT].setVoltageSimd(10.f * formantX2Envelope, c);

			if (outputs[ENVELOPE_Y2_OUTPUT].isConnected())
				outputs[ENVELOPE_Y2_OUTPUT].setVoltageSimd(10.f * formantY2Envelope, c);


			//Percentages
			if(c== 0) {
				freqPercentage = freqParam;
				primaryFMPercentage = primaryFmParam;
				globalFMPercentage = globalFmParam;

				formantMixPercentage = formantMix[0];
				formantXYBalancePercentage = formantXYBalance[0];
				formantXBalancePercentage = formantXBalance[0];
				formantYBalancePercentage = formantYBalance[0];


				formantX1RatioPercentage = formantX1Ratio[0] / 240.f;
				formantX1DampPercentage = formantX1Damp[0];
				formantX1WarpPercentage = formantX1Warp[0];

				formantY1RatioPercentage = formantY1Ratio[0] / 240.f;
				formantY1DampPercentage = formantY1Damp[0];
				formantY1WarpPercentage = formantY1Warp[0];

				formantX2RatioPercentage = formantX2Ratio[0] / 240.f;
				formantX2DampPercentage = formantX2Damp[0];
				formantX2WarpPercentage = formantX2Warp[0];

				formantY2RatioPercentage = formantY2Ratio[0] / 240.f;
				formantY2DampPercentage = formantY2Damp[0];
				formantY2WarpPercentage = formantY2Warp[0];
			}


			//Damping Envelopes
			float_4 lambda = 0.5f * freq / formantX1Damp * (args.sampleTime);
			formantX1Envelope -= lambda; 
			lambda = 0.5f * freq / formantY1Damp * (args.sampleTime);
			formantY1Envelope -= lambda; 
			lambda = 0.5f * freq / formantX2Damp * (args.sampleTime);
			formantX2Envelope -= lambda; 
			lambda = 0.5f * freq / formantY2Damp * (args.sampleTime);
			formantY2Envelope -= lambda; 

		}

		outputs[PRIMARY_OUTPUT].setChannels(channels);
		outputs[MAIN_OUTPUT].setChannels(channels);

		outputs[FORMANT_X1_OUTPUT].setChannels(channels);
		outputs[FORMANT_Y1_OUTPUT].setChannels(channels);
		outputs[FORMANT_X2_OUTPUT].setChannels(channels);
		outputs[FORMANT_Y2_OUTPUT].setChannels(channels);

		outputs[ENVELOPE_X1_OUTPUT].setChannels(channels);
		outputs[ENVELOPE_Y1_OUTPUT].setChannels(channels);
		outputs[ENVELOPE_X2_OUTPUT].setChannels(channels);
		outputs[ENVELOPE_Y2_OUTPUT].setChannels(channels);

	}
};


struct FormalDinnerWidget : ModuleWidget {
	FormalDinnerWidget(FormalDinner* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/FormalDinner.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		//Primary VCO
		ParamWidget* freqParam = createParam<RoundFWKnob>(Vec(5, 25), module, FormalDinner::FREQ_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(freqParam)->percentage = &module->freqPercentage;
		}
		addParam(freqParam);							

		ParamWidget* primaryFMParam = createParam<RoundFWKnob>(Vec(75, 25), module, FormalDinner::PRIMARY_FM_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(primaryFMParam)->percentage = &module->primaryFMPercentage;
			dynamic_cast<RoundFWKnob*>(primaryFMParam)->biDirectional = true;
		}
		addParam(primaryFMParam);							

		ParamWidget* globalFMParam = createParam<RoundFWKnob>(Vec(145, 25), module, FormalDinner::GLOBAL_FM_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(globalFMParam)->percentage = &module->globalFMPercentage;
			dynamic_cast<RoundFWKnob*>(globalFMParam)->biDirectional = true;
		}
		addParam(globalFMParam);


		addParam(createParam<LEDButton>(Vec(235, 25), module, FormalDinner::SYNC_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(236.5, 26.5), module, FormalDinner::SOFT_LIGHT));

		
		
		//Formant Balancing
		ParamWidget* formantMixParam = createParam<RoundFWKnob>(Vec(5, 70), module, FormalDinner::FORMANT_MIX_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantMixParam)->percentage = &module->formantMixPercentage;
		}
		addParam(formantMixParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(40, 70), module, FormalDinner::FORMANT_MIX_CV_ATTENUVERTER_PARAM));
		
		ParamWidget* formantXYBalanceParam = createParam<RoundFWKnob>(Vec(75, 70), module, FormalDinner::FORMANT_XY_BALANCE_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantXYBalanceParam)->percentage = &module->formantXYBalancePercentage;
		}
		addParam(formantXYBalanceParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(110, 70), module, FormalDinner::FORMANT_XY_BALANCE_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantXBalanceParam = createParam<RoundFWKnob>(Vec(145, 70), module, FormalDinner::FORMANT_X_BALANCE_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantXBalanceParam)->percentage = &module->formantXBalancePercentage;
		}
		addParam(formantXBalanceParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(180, 70), module, FormalDinner::FORMANT_X_BALANCE_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantYBalanceParam = createParam<RoundFWKnob>(Vec(215, 70), module, FormalDinner::FORMANT_Y_BALANCE_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantYBalanceParam)->percentage = &module->formantYBalancePercentage;
		}
		addParam(formantYBalanceParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(250, 70), module, FormalDinner::FORMANT_Y_BALANCE_CV_ATTENUVERTER_PARAM));




	
		//Formant X1
		ParamWidget* formantX1RatioParam = createParam<RoundFWKnob>(Vec(15, 203), module, FormalDinner::FORMANT_X1_RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantX1RatioParam)->percentage = &module->formantX1RatioPercentage;
		}
		addParam(formantX1RatioParam);
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(50, 203), module, FormalDinner::FORMANT_X1_RATIO_CV_ATTENUVERTER_PARAM));


		ParamWidget* formantX1DampParam = createParam<RoundFWKnob>(Vec(75, 185), module, FormalDinner::FORMANT_X1_DAMP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantX1DampParam)->percentage = &module->formantX1DampPercentage;
		}
		addParam(formantX1DampParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(110, 185), module, FormalDinner::FORMANT_X1_DAMP_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantX1WarpParam = createParam<RoundFWKnob>(Vec(75, 221), module, FormalDinner::FORMANT_X1_WARP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantX1WarpParam)->percentage = &module->formantX1WarpPercentage;
			dynamic_cast<RoundFWKnob*>(formantX1WarpParam)->biDirectional = true;
		}
		addParam(formantX1WarpParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(110, 221), module, FormalDinner::FORMANT_X1_WARP_CV_ATTENUVERTER_PARAM));

		addParam(createParam<LEDButton>(Vec(25, 233), module, FormalDinner::FORMANT_X1_WAVE_TYPE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(26.5, 234.5), module, FormalDinner::FORMANT_X1_WAVE_TYPE_LIGHT));


		//Formant Y1
		ParamWidget* formantY1RatioParam = createParam<RoundFWKnob>(Vec(90, 130), module, FormalDinner::FORMANT_Y1_RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantY1RatioParam)->percentage = &module->formantY1RatioPercentage;
		}
		addParam(formantY1RatioParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(125, 130), module, FormalDinner::FORMANT_Y1_RATIO_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantY1DampParam = createParam<RoundFWKnob>(Vec(150, 112), module, FormalDinner::FORMANT_Y1_DAMP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantY1DampParam)->percentage = &module->formantY1DampPercentage;
		}
		addParam(formantY1DampParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(185, 112), module, FormalDinner::FORMANT_Y1_DAMP_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantY1WarpParam = createParam<RoundFWKnob>(Vec(150, 148), module, FormalDinner::FORMANT_Y1_WARP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantY1WarpParam)->percentage = &module->formantY1WarpPercentage;
			dynamic_cast<RoundFWKnob*>(formantY1WarpParam)->biDirectional = true;
		}		
		addParam(formantY1WarpParam);		
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(185, 148), module, FormalDinner::FORMANT_Y1_WARP_CV_ATTENUVERTER_PARAM));

		addParam(createParam<LEDButton>(Vec(100, 160), module, FormalDinner::FORMANT_Y1_WAVE_TYPE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(101.5, 161.5), module, FormalDinner::FORMANT_Y1_WAVE_TYPE_LIGHT));



		//Formant X2
		ParamWidget* formantX2RatioParam = createParam<RoundFWKnob>(Vec(160, 203), module, FormalDinner::FORMANT_X2_RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantX2RatioParam)->percentage = &module->formantX2RatioPercentage;
		}
		addParam(formantX2RatioParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(195, 203), module, FormalDinner::FORMANT_X2_RATIO_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantX2DampParam = createParam<RoundFWKnob>(Vec(220, 185), module, FormalDinner::FORMANT_X2_DAMP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantX2DampParam)->percentage = &module->formantX2DampPercentage;
		}
		addParam(formantX2DampParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(255, 185), module, FormalDinner::FORMANT_X2_DAMP_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantX2WarpParam = createParam<RoundFWKnob>(Vec(220, 221), module, FormalDinner::FORMANT_X2_WARP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantX2WarpParam)->percentage = &module->formantX2WarpPercentage;
			dynamic_cast<RoundFWKnob*>(formantX2WarpParam)->biDirectional = true;
		}
		addParam(formantX2WarpParam);				
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(255, 221), module, FormalDinner::FORMANT_X2_WARP_CV_ATTENUVERTER_PARAM));

		addParam(createParam<LEDButton>(Vec(170, 233), module, FormalDinner::FORMANT_X2_WAVE_TYPE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(171.5, 234.5), module, FormalDinner::FORMANT_X2_WAVE_TYPE_LIGHT));



		//Formant Y2	
		ParamWidget* formantY2RatioParam = createParam<RoundFWKnob>(Vec(90, 275), module, FormalDinner::FORMANT_Y2_RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantY2RatioParam)->percentage = &module->formantY2RatioPercentage;
		}
		addParam(formantY2RatioParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(125, 275), module, FormalDinner::FORMANT_Y2_RATIO_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantY2DampParam = createParam<RoundFWKnob>(Vec(150, 257), module, FormalDinner::FORMANT_Y2_DAMP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(formantY2DampParam)->percentage = &module->formantY2DampPercentage;
		}
		addParam(formantY2DampParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(185, 257), module, FormalDinner::FORMANT_Y2_DAMP_CV_ATTENUVERTER_PARAM));

		ParamWidget* formantY2WarpParam = createParam<RoundFWKnob>(Vec(150, 293), module, FormalDinner::FORMANT_Y2_WARP_PARAM);
		if (module) {	
			dynamic_cast<RoundFWKnob*>(formantY2WarpParam)->percentage = &module->formantY2WarpPercentage;
			dynamic_cast<RoundFWKnob*>(formantY2WarpParam)->biDirectional = true;
		}
		addParam(formantY2WarpParam);			
		addParam(createParam<RoundExtremelySmallFWKnob>(Vec(185, 293), module, FormalDinner::FORMANT_Y2_WARP_CV_ATTENUVERTER_PARAM));

		addParam(createParam<LEDButton>(Vec(100, 305), module, FormalDinner::FORMANT_Y2_WAVE_TYPE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(101.5, 306.5), module, FormalDinner::FORMANT_Y2_WAVE_TYPE_LIGHT));



		addInput(createInput<FWPortInSmall>(Vec(103, 43), module, FormalDinner::PRIMARY_FM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(173, 43), module, FormalDinner::GLOBAL_FM_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(33, 43), module, FormalDinner::PITCH_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(235, 43), module, FormalDinner::SYNC_INPUT));


		addInput(createInput<FWPortInSmall>(Vec(33, 90), module, FormalDinner::FORMANT_MIX_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(103, 90), module, FormalDinner::FORMANT_XY_BALANCE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(173, 90), module, FormalDinner::FORMANT_X_BALANCE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(243, 90), module, FormalDinner::FORMANT_Y_BALANCE_INPUT));


		addInput(createInput<FWPortInSmall>(Vec(48, 221), module, FormalDinner::FORMANT_X1_RATIO_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(108, 203), module, FormalDinner::FORMANT_X1_DAMP_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(108, 239), module, FormalDinner::FORMANT_X1_WARP_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(123, 148), module, FormalDinner::FORMANT_Y1_RATIO_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(183, 130), module, FormalDinner::FORMANT_Y1_DAMP_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(183, 166), module, FormalDinner::FORMANT_Y1_WARP_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(193, 221), module, FormalDinner::FORMANT_X2_RATIO_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(253, 203), module, FormalDinner::FORMANT_X2_DAMP_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(253, 239), module, FormalDinner::FORMANT_X2_WARP_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(123, 293), module, FormalDinner::FORMANT_Y2_RATIO_INPUT));	
		addInput(createInput<FWPortInSmall>(Vec(183, 275), module, FormalDinner::FORMANT_Y2_DAMP_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(183, 311), module, FormalDinner::FORMANT_Y2_WARP_INPUT));


		addOutput(createOutputCentered<FWPortOutSmall>(Vec(270, 350), module, FormalDinner::MAIN_OUTPUT));
		addOutput(createOutputCentered<FWPortOutSmall>(Vec(245, 350), module, FormalDinner::PRIMARY_OUTPUT));

		addOutput(createOutputCentered<FWPortOutSmall>(Vec(17.5, 350), module, FormalDinner::FORMANT_X1_OUTPUT));
		addOutput(createOutputCentered<FWPortOutSmall>(Vec(42.5, 350), module, FormalDinner::ENVELOPE_X1_OUTPUT));

		addOutput(createOutputCentered<FWPortOutSmall>(Vec(72.5, 350), module, FormalDinner::FORMANT_Y1_OUTPUT));
		addOutput(createOutputCentered<FWPortOutSmall>(Vec(97.5, 350), module, FormalDinner::ENVELOPE_Y1_OUTPUT));

		addOutput(createOutputCentered<FWPortOutSmall>(Vec(127.5, 350), module, FormalDinner::FORMANT_X2_OUTPUT));
		addOutput(createOutputCentered<FWPortOutSmall>(Vec(152.5, 350), module, FormalDinner::ENVELOPE_X2_OUTPUT));

		addOutput(createOutputCentered<FWPortOutSmall>(Vec(182.5, 350), module, FormalDinner::FORMANT_Y2_OUTPUT));
		addOutput(createOutputCentered<FWPortOutSmall>(Vec(207.5, 350), module, FormalDinner::ENVELOPE_Y2_OUTPUT));

	}
	
	void appendContextMenu(Menu *menu) override {
		// MenuLabel *spacerLabel = new MenuLabel();
		// menu->addChild(spacerLabel);

		FormalDinner *module = dynamic_cast<FormalDinner*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());
		{
      		OptionsMenuItem* mi = new OptionsMenuItem("Warp Range");
			mi->addItem(OptionMenuItem("2x", [module]() { return module->warpRange == 2; }, [module]() { module->warpRange = 2; }));
			mi->addItem(OptionMenuItem("4x", [module]() { return module->warpRange == 4; }, [module]() { module->warpRange = 4; }));
			mi->addItem(OptionMenuItem("8x", [module]() { return module->warpRange == 8; }, [module]() { module->warpRange = 8; }));
			mi->addItem(OptionMenuItem("16x", [module]() { return module->warpRange == 16; }, [module]() { module->warpRange = 16; }));
			mi->addItem(OptionMenuItem("32x", [module]() { return module->warpRange == 32; }, [module]() { module->warpRange = 32; }));
			menu->addChild(mi);
		}			
	}

};


Model* modelFormalDinner = createModel<FormalDinner, FormalDinnerWidget>("FormalDinner");
