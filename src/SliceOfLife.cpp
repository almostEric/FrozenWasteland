#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"


using rack::simd::float_4;


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
struct SSVoltageControlledOscillator {
	bool analog = false;
	bool soft = false;
	bool syncEnabled = false;
	// For optimizing in serial code
	int channels = 0;

	T lastSyncValue = 0.f;
	T phase = 0.f;
	T freq;
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

	void setPitch(T pitch, T linearFM) {
		freq = abs((dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30) / 1073741824) + linearFM);
	}

	void setPulseWidth(T pulseWidth) {
		const float pwMin = 0.01f;
		this->pulseWidth = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, T syncValue) {
		// Advance phase
		T deltaPhase = simd::clamp(freq * deltaTime, 1e-6f, 0.35f);
		if (soft) {
			// Reverse direction
			deltaPhase *= syncDirection;
		}
		else {
			// Reset back to forward
			syncDirection = 1.f;
		}
		phase += deltaPhase;
		// Wrap phase
		phase -= simd::floor(phase);

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
			// v = x;
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


struct SliceOfLife : Module {
	enum ParamIds {
		POS_VCO_MODE_PARAM,
		POS_VCO_SYNC_PARAM,
		POS_VCO_FREQ_PARAM,
		POS_VCO_FINE_PARAM,
        POS_VCO_FM_MODE_PARAM,
		POS_VCO_FM_PARAM,
        POS_SIN_VCA_PARAM,
        POS_TRI_VCA_PARAM,
        POS_SAW_VCA_PARAM,
		NEG_VCO_MODE_PARAM,
		NEG_VCO_SYNC_PARAM,
		NEG_VCO_FREQ_PARAM,
		NEG_VCO_FINE_PARAM,
        NEG_VCO_FM_MODE_PARAM,
		NEG_VCO_FM_PARAM,
        NEG_SIN_VCA_PARAM,
        NEG_TRI_VCA_PARAM,
        NEG_SAW_VCA_PARAM,
		SIS_VCO_SYNC_PARAM,
		SIS_VCO_FREQ_PARAM,
		SIS_VCO_FINE_PARAM,
        SIS_VCO_FM_1_MODE_PARAM,
		SIS_VCO_FM_1_PARAM,
        SIS_VCO_FM_2_MODE_PARAM,
		SIS_VCO_FM_2_PARAM,
		SIS_VCO_PW_PARAM,
		SIS_VCO_PWM_PARAM,
		SIS_SKEW_AMOUNT_PARAM,
		PW_SKEW_PARAM,
		CV_LOCK_PARAM,
		SYNC_LOCK_PARAM,
		VCA_BIAS_PARAM,
		MIX_PARAM,
		VCA_CV_DEPTH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		POS_VCO_PITCH_INPUT,
		POS_VCO_FM_INPUT,
		POS_VCO_SYNC_INPUT,
        POS_VCO_SIN_VCA_INPUT,
        POS_VCO_TRI_VCA_INPUT,
        POS_VCO_SAW_VCA_INPUT,
		NEG_VCO_PITCH_INPUT,
		NEG_VCO_FM_INPUT,
		NEG_VCO_SYNC_INPUT,
        NEG_VCO_SIN_VCA_INPUT,
        NEG_VCO_TRI_VCA_INPUT,
        NEG_VCO_SAW_VCA_INPUT,
		SIS_VCO_PITCH_INPUT,
		SIS_VCO_FM_1_INPUT,
		SIS_VCO_FM_2_INPUT,
		SIS_VCO_PW_INPUT,
		SIS_VCO_SYNC_INPUT,
		SIS_SKEW_AMOUNT_INPUT,
		VCA_CV_INPUT,
		MIX_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POS_SIN_OUTPUT,
		POS_TRI_OUTPUT,
		POS_SAW_OUTPUT,
		POS_MIX_OUTPUT,
		NEG_SIN_OUTPUT,
		NEG_TRI_OUTPUT,
		NEG_SAW_OUTPUT,
		NEG_MIX_OUTPUT,
		SIS_TRI_OUTPUT,
		SIS_SAW_OUTPUT,
		SIS_SQR_OUTPUT,
		GLU_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		POS_VCO_FM_MODE_LIGHT,
		NEG_VCO_FM_MODE_LIGHT = POS_VCO_FM_MODE_LIGHT + 3,
		SIS_VCO_FM_1_MODE_LIGHT = NEG_VCO_FM_MODE_LIGHT + 3,
		SIS_VCO_FM_2_MODE_LIGHT = SIS_VCO_FM_1_MODE_LIGHT + 3,
		PW_SKEW_LIGHT = SIS_VCO_FM_2_MODE_LIGHT + 3,
		CV_LOCK_LIGHT = PW_SKEW_LIGHT + 3,
		SYNC_LOCK_LIGHT = CV_LOCK_LIGHT + 3,
		NUM_LIGHTS = SYNC_LOCK_LIGHT + 3
	};


	SSVoltageControlledOscillator<16, 16, float_4> positiveOscillators[4];
	SSVoltageControlledOscillator<16, 16, float_4> negativeOscillators[4];
	SSVoltageControlledOscillator<16, 16, float_4> scissorOscillators[4];
	dsp::ClockDivider lightDivider;

	dsp::SchmittTrigger posVCOFMModeTrigger,negVCOFMModeTrigger,sisVCOFM1ModeTrigger,sisVCOFM2ModeTrigger,pwSkewTrigger,cvLockTrigger,syncLockTrigger;

    float_4 positiveOutput[4];
    float_4 negativeOutput[4];
    float_4 scissorOutput[4];
    float_4 scissorPW[4];
    float_4 scissorCV[4];
    float_4 scissorSkew[4];
	float_4 vcaBias[4];
	float_4 dnaBias[4];
    float_4 glueOutput[4];

	bool posVCOFMMode = false;
	bool negVCOFMMode = false;
	bool sisVCOFM1Mode = false;
	bool sisVCOFM2Mode = false;

	int pwSkew = 0;
	bool cvLock = false;
	bool syncLock = false;

	//percentages
	float posVcoFreqParamPercentage = 0;
	float posSinVCAPercentage = 0;
	float posTriVCAPercentage = 0;
	float posSawVCAPercentage = 0;
	float negSinVCAPercentage = 0;
	float negTriVCAPercentage = 0;
	float negSawVCAPercentage = 0;
	float sisPWPercentage = 0;
	float sisSkewPercentage = 0;

	float vcaBiasPercentage = 0;
	float mixPercentage = 0;



	SliceOfLife() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(POS_VCO_MODE_PARAM, 0.f, 1.f, 1.f, "Positive VCO - Analog mode");
		configParam(POS_VCO_SYNC_PARAM, 0.f, 1.f, 1.f, "Positive VCO - Hard sync");
		configParam(POS_VCO_FREQ_PARAM, -54.f, 54.f, 0.f, "Positive VCO - Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(POS_VCO_FINE_PARAM, -1.f, 1.f, 0.f, "Positive VCO - Fine frequency");
		configParam(POS_VCO_FM_PARAM, 0.f, 1.f, 0.f, "Positive VCO - Frequency modulation", "%", 0.f, 100.f);
		configParam(POS_SIN_VCA_PARAM, -1.f, 1.f, 0.f, "Positive Sin Level","%",0.f,100.f);
		configParam(POS_TRI_VCA_PARAM, -1.f, 1.f, 0.f, "Positive Triangle Level","%",0.f,100.f);
		configParam(POS_SAW_VCA_PARAM, -1.f, 1.f, 0.f, "Positive Sawtooth Level","%",0.f,100.f);

		configParam(NEG_VCO_MODE_PARAM, 0.f, 1.f, 1.f, "Negative VCO - Analog mode");
		configParam(NEG_VCO_SYNC_PARAM, 0.f, 1.f, 1.f, "Negative VCO - Hard sync");
		configParam(NEG_VCO_FREQ_PARAM, -54.f, 54.f, 0.f, "Negative VCO - Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(NEG_VCO_FINE_PARAM, -1.f, 1.f, 0.f, "Negative VCO - Fine frequency");
		configParam(NEG_VCO_FM_PARAM, 0.f, 1.f, 0.f, "Negative VCO - Frequency modulation", "%", 0.f, 100.f);
		configParam(NEG_SIN_VCA_PARAM, -1.f, 1.f, 0.f, "Negative Sin Level","%",0.f,100.f);
		configParam(NEG_TRI_VCA_PARAM, -1.f, 1.f, 0.f, "Negative Triangle Level","%",0.f,100.f);
		configParam(NEG_SAW_VCA_PARAM, -1.f, 1.f, 0.f, "Negative Sawtooth Level","%",0.f,100.f);

		configParam(SIS_VCO_SYNC_PARAM, 0.f, 1.f, 1.f, "Scissor VCO - Hard sync");
		configParam(SIS_VCO_FREQ_PARAM, -54.f, 54.f, 0.f, "Scissor VCO - Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(SIS_VCO_FINE_PARAM, -1.f, 1.f, 0.f, "Scissor VCO - Fine frequency");
		configParam(SIS_VCO_FM_1_PARAM, 0.f, 1.f, 0.f, "Scissor VCO - Frequency modulation 1", "%", 0.f, 100.f);
		configParam(SIS_VCO_PW_PARAM, 0.01f, 0.99f, 0.5f, "Scissor VCO - Pulse width", "%", 0.f, 100.f);
		configParam(SIS_VCO_PWM_PARAM, 0.f, 1.f, 0.f, "Scissor VCO - Pulse width modulation", "%", 0.f, 100.f);
		configParam(SIS_SKEW_AMOUNT_PARAM, 0.f, 4.f, 2.f, "Pulse Width Skew Amount", "%", 0.f, 100.f);


		configParam(VCA_BIAS_PARAM, 0.f, 1.f, 0.f, "VCA Bias", "%", 0.f, 100.f);
		configParam(MIX_PARAM, -1.f, 1.f, 0.f, "Mix", "%", 0.f, 100.f);
		configParam(VCA_CV_DEPTH_PARAM, 0.f, 1.f, 0.f, "VCA CV Attenuation", "%", 0.f, 100.f);


		lightDivider.setDivision(16);
	}

		json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "pwSkew", json_integer(pwSkew));
		json_object_set_new(rootJ, "cvLock", json_boolean(cvLock));
		json_object_set_new(rootJ, "syncLock", json_boolean(syncLock));
		json_object_set_new(rootJ, "posVCOFMMode", json_boolean(posVCOFMMode));
		json_object_set_new(rootJ, "negVCOFMMode", json_boolean(negVCOFMMode));
		json_object_set_new(rootJ, "sisVCOFM1Mode", json_boolean(sisVCOFM1Mode));
		json_object_set_new(rootJ, "sisVCOFM2Mode", json_boolean(sisVCOFM2Mode));
		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *pwsJ = json_object_get(rootJ, "pwSkew");
		if (pwsJ)
			pwSkew = json_integer_value(pwsJ);

		json_t *cvLJ = json_object_get(rootJ, "cvLock");
		if (cvLJ)
			cvLock = json_boolean_value(cvLJ);

		json_t *slJ = json_object_get(rootJ, "syncLock");
		if (slJ) {
			syncLock = json_boolean_value(slJ);			
		}

		json_t *pfmmJ = json_object_get(rootJ, "posVCOFMMode");
		if (pfmmJ) {
			posVCOFMMode = json_boolean_value(pfmmJ);			
		}

		json_t *nfmmJ = json_object_get(rootJ, "negVCOFMMode");
		if (nfmmJ) {
			negVCOFMMode = json_boolean_value(nfmmJ);			
		}

		json_t *sfm1mJ = json_object_get(rootJ, "sisVCOFM1Mode");
		if (sfm1mJ) {
			sisVCOFM1Mode = json_boolean_value(sfm1mJ);			
		}

		json_t *sfm2mJ = json_object_get(rootJ, "sisVCOFM2Mode");
		if (sfm2mJ) {
			sisVCOFM2Mode = json_boolean_value(sfm2mJ);			
		}
	}


	void process(const ProcessArgs& args) override {

		if (posVCOFMModeTrigger.process(params[POS_VCO_FM_MODE_PARAM].getValue())) {
			posVCOFMMode = !posVCOFMMode;
		}		
		lights[POS_VCO_FM_MODE_LIGHT].value = posVCOFMMode;
		lights[POS_VCO_FM_MODE_LIGHT+1].value = posVCOFMMode;

		if (negVCOFMModeTrigger.process(params[NEG_VCO_FM_MODE_PARAM].getValue())) {
			negVCOFMMode = !negVCOFMMode;
		}		
		lights[NEG_VCO_FM_MODE_LIGHT].value = negVCOFMMode;
		lights[NEG_VCO_FM_MODE_LIGHT+1].value = negVCOFMMode;

		if (sisVCOFM1ModeTrigger.process(params[SIS_VCO_FM_1_MODE_PARAM].getValue())) {
			sisVCOFM1Mode = !sisVCOFM1Mode;
		}		
		lights[SIS_VCO_FM_1_MODE_LIGHT].value = sisVCOFM1Mode;
		lights[SIS_VCO_FM_1_MODE_LIGHT+1].value = sisVCOFM1Mode;

		if (sisVCOFM2ModeTrigger.process(params[SIS_VCO_FM_2_MODE_PARAM].getValue())) {
			sisVCOFM2Mode = !sisVCOFM2Mode;
		}		
		lights[SIS_VCO_FM_2_MODE_LIGHT].value = sisVCOFM2Mode;
		lights[SIS_VCO_FM_2_MODE_LIGHT+1].value = sisVCOFM2Mode;

		if (pwSkewTrigger.process(params[PW_SKEW_PARAM].getValue())) {
			pwSkew = (pwSkew + 1) % 3;
		}		
		switch (pwSkew) {
			case 0 :
				lights[PW_SKEW_LIGHT].value = 0;
				lights[PW_SKEW_LIGHT+1].value = 0;
				lights[PW_SKEW_LIGHT+2].value = 0;
				break;
			case 1 :
				lights[PW_SKEW_LIGHT].value = 0;
				lights[PW_SKEW_LIGHT+1].value = 1;
				lights[PW_SKEW_LIGHT+2].value = 1;
				break;
			case 2 :
				lights[PW_SKEW_LIGHT].value = 1;
				lights[PW_SKEW_LIGHT+1].value = 0.3;
				lights[PW_SKEW_LIGHT+2].value = 0;
				break;
		}

		if (cvLockTrigger.process(params[CV_LOCK_PARAM].getValue())) {
			cvLock = !cvLock;
		}		
		lights[CV_LOCK_LIGHT+1].value = cvLock;
		lights[CV_LOCK_LIGHT+2].value = cvLock;

		if (syncLockTrigger.process(params[SYNC_LOCK_PARAM].getValue())) {
			syncLock = !syncLock;
		}		
		lights[SYNC_LOCK_LIGHT+1].value = syncLock;
		lights[SYNC_LOCK_LIGHT+2].value = syncLock;


		float vcaBiasParam = params[VCA_BIAS_PARAM].getValue();
		float mixParam = params[MIX_PARAM].getValue();
		float vcaBiasCVParam = params[VCA_CV_DEPTH_PARAM].getValue();

        //Process Scissor
		float freqParam = params[SIS_VCO_FREQ_PARAM].getValue() / 12.f;
		freqParam += dsp::quadraticBipolar(params[SIS_VCO_FINE_PARAM].getValue()) * 3.f / 12.f;
		float fm1Param = dsp::quadraticBipolar(params[SIS_VCO_FM_1_PARAM].getValue());
		float fm2Param = dsp::quadraticBipolar(params[SIS_VCO_FM_2_PARAM].getValue());

		float skewAmountParam = params[SIS_SKEW_AMOUNT_PARAM].getValue();


		int channels = std::max(inputs[SIS_VCO_PITCH_INPUT].getChannels(), 1);
		int masterChanels = channels;
		for (int c = 0; c < channels; c += 4) {
			auto oscillator = &scissorOscillators[c / 4];
			oscillator->channels = std::min(channels - c, 4);
			oscillator->analog = false;
			oscillator->soft = false;

			float_4 pitch = freqParam;
			float_4 linearFM = 0;
			float_4 expFM = 0;
			float_4 vOct = inputs[SIS_VCO_PITCH_INPUT].getVoltageSimd<float_4>(c);
			pitch += vOct;
			scissorCV[c] = vOct;
			if (inputs[SIS_VCO_FM_1_INPUT].isConnected()) {
				if(sisVCOFM1Mode) {
					expFM = fm1Param * inputs[SIS_VCO_FM_1_INPUT].getPolyVoltageSimd<float_4>(c);
					scissorCV[c] += expFM;
					pitch += expFM;
				}
				else
					linearFM = fm1Param * inputs[SIS_VCO_FM_1_INPUT].getPolyVoltageSimd<float_4>(c) * 10000.0;
			}
			if (inputs[SIS_VCO_FM_2_INPUT].isConnected()) {
				if(sisVCOFM2Mode) {
					expFM = fm2Param * inputs[SIS_VCO_FM_2_INPUT].getPolyVoltageSimd<float_4>(c);
					scissorCV[c] += expFM;
					pitch += expFM;
				}
				else
					linearFM += fm2Param * inputs[SIS_VCO_FM_2_INPUT].getPolyVoltageSimd<float_4>(c) * 10000.0;
			}
			oscillator->setPitch(pitch,linearFM);
			scissorPW[c] = simd::clamp(params[SIS_VCO_PW_PARAM].getValue() + params[SIS_VCO_PWM_PARAM].getValue() * inputs[SIS_VCO_PW_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f,0.f,1.f);

		// fprintf(stderr,"pw: %f \n",scissorPW[c][0]);


			oscillator->setPulseWidth(scissorPW[c]);

			oscillator->syncEnabled = inputs[SIS_VCO_SYNC_INPUT].isConnected();
			oscillator->process(args.sampleTime, inputs[SIS_VCO_SYNC_INPUT].getPolyVoltageSimd<float_4>(c));


			vcaBias[c] = simd::clamp(vcaBiasParam + (vcaBiasCVParam * inputs[VCA_CV_INPUT].getVoltageSimd<float_4>(c) / 2.5f),0.f,1.f);
			scissorSkew[c] = simd::clamp(skewAmountParam + (inputs[SIS_SKEW_AMOUNT_INPUT].getVoltageSimd<float_4>(c) / 2.5f),0.f,4.f);


			if(c == 0) {
				sisPWPercentage = scissorPW[0][0];
				sisSkewPercentage = scissorSkew[0][0] / 4.0;
			}

// fprintf(stderr,"vcb: %f \n",vcaBias[c][0]);

            scissorOutput[c] = oscillator->sqr();
            outputs[SIS_SQR_OUTPUT].setVoltageSimd(scissorOutput[c] * 5.f, c);
            outputs[SIS_TRI_OUTPUT].setVoltageSimd(oscillator->tri() * 5.f, c);
            outputs[SIS_SAW_OUTPUT].setVoltageSimd(oscillator->saw() * 5.f, c);

		}
		outputs[SIS_SQR_OUTPUT].setChannels(channels);


        //Process Positive
		freqParam = params[POS_VCO_FREQ_PARAM].getValue() / 12.f;
		freqParam += dsp::quadraticBipolar(params[POS_VCO_FINE_PARAM].getValue()) * 3.f / 12.f;
		float fmParam = dsp::quadraticBipolar(params[POS_VCO_FM_PARAM].getValue());

		float sinVCAParam = params[POS_SIN_VCA_PARAM].getValue();
		float triVCAParam = params[POS_TRI_VCA_PARAM].getValue();
		float sawVCAParam = params[POS_SAW_VCA_PARAM].getValue();

		channels = std::max(inputs[POS_VCO_PITCH_INPUT].getChannels(), masterChanels);
		for (int c = 0; c < channels; c += 4) {
			auto* oscillator = &positiveOscillators[c / 4];
			oscillator->channels = std::min(channels - c, 4);
			oscillator->analog = false;
			//oscillator->soft = params[POS_VCO_SYNC_PARAM].getValue() <= 0.f;
			oscillator->soft = false;

			float_4 pitch = freqParam;
			float_4 linearFM = 0;
			pitch += inputs[POS_VCO_PITCH_INPUT].getVoltageSimd<float_4>(c);
			if(cvLock) {
				pitch+=scissorCV[c];
			}
			if (inputs[POS_VCO_FM_INPUT].isConnected()) {
				if(posVCOFMMode)
					pitch += fmParam * inputs[POS_VCO_FM_INPUT].getPolyVoltageSimd<float_4>(c);
				else
					linearFM = fmParam * inputs[POS_VCO_FM_INPUT].getPolyVoltageSimd<float_4>(c) * 10000.0;
			}
			if(pwSkew == 1) {
				pitch -= (scissorPW[c] - 0.5) * scissorSkew[c]; //0.5 is scaling factor - maybe make it a parameter?
			} else if (pwSkew == 2) {
				pitch += (scissorPW[c] - 0.5) * scissorSkew[c]; //0.5 is scaling factor - maybe make it a parameter?
			}
			oscillator->setPitch(pitch,linearFM);

			if(!syncLock) {
				oscillator->syncEnabled = inputs[POS_VCO_SYNC_INPUT].isConnected();
				oscillator->process(args.sampleTime, inputs[POS_VCO_SYNC_INPUT].getPolyVoltageSimd<float_4>(c));
			} else {
				oscillator->syncEnabled = true;
				oscillator->process(args.sampleTime, scissorOutput[c]);
			}

            float_4 sinVCA4 = simd::clamp(sinVCAParam + inputs[POS_VCO_SIN_VCA_INPUT].getVoltageSimd<float_4>(c) / 10.f,-1.f,1.f);
            float_4 triVCA4 = simd::clamp(triVCAParam + inputs[POS_VCO_TRI_VCA_INPUT].getVoltageSimd<float_4>(c) / 10.f,-1.f,1.f);
            float_4 sawVCA4 = simd::clamp(sawVCAParam + inputs[POS_VCO_SAW_VCA_INPUT].getVoltageSimd<float_4>(c) / 10.f,-1.f,1.f);

			if(c == 0) {
				posSinVCAPercentage = sinVCA4[0];
				posTriVCAPercentage = triVCA4[0];
				posSawVCAPercentage = sawVCA4[0];
			}

			float_4 posScissor = simd::clamp(simd::clamp(scissorOutput[c],0.f,1.f) + vcaBias[c] ,0.f,1.f);
			float_4 sinVCA4Processed = simd::ifelse(sinVCA4 <= 0, (sinVCA4 + 1.f) * posScissor, simd::clamp(sinVCA4+posScissor,0.f,1.f));
			float_4 triVCA4Processed = simd::ifelse(triVCA4 <= 0, (triVCA4 + 1.f) * posScissor, simd::clamp(triVCA4+posScissor,0.f,1.f));
			float_4 sawVCA4Processed = simd::ifelse(sawVCA4 <= 0, (sawVCA4 + 1.f) * posScissor, simd::clamp(sawVCA4+posScissor,0.f,1.f));

            float_4 totalVCA4 = simd::clamp(sinVCA4Processed + triVCA4Processed + sawVCA4Processed,1.f,3.f);


			float_4 posSin = simd::abs(oscillator->sin());
			float_4 posTri = simd::abs(oscillator->tri());
			float_4 posSaw = oscillator->saw();
			posSaw = simd::ifelse(posSaw < 0,posSaw+1,posSaw);

            outputs[POS_SIN_OUTPUT].setVoltageSimd(posSin * 5.f, c);
            outputs[POS_TRI_OUTPUT].setVoltageSimd(posTri * 5.f, c);
            outputs[POS_SAW_OUTPUT].setVoltageSimd(posSaw * 5.0f, c);

            // positiveOutput[c] = (posSin * sinVCA4 + posTri * triVCA4 + posSaw * sawVCA4) / totalVCA4;  

            positiveOutput[c] = ((posSin * sinVCA4Processed) + 
								(posTri * triVCA4Processed) + 
								(posSaw * sawVCA4Processed)) / totalVCA4;  

// fprintf(stderr,"so: %f  po:%f \n",posScissor[0],positiveOutput[c][0]);

            outputs[POS_MIX_OUTPUT].setVoltageSimd(positiveOutput[c] * 5.f, c);
		}
		outputs[POS_MIX_OUTPUT].setChannels(channels);

        //Process Negative
		freqParam = params[NEG_VCO_FREQ_PARAM].getValue() / 12.f;
		freqParam += dsp::quadraticBipolar(params[NEG_VCO_FINE_PARAM].getValue()) * 3.f / 12.f;
		fmParam = dsp::quadraticBipolar(params[NEG_VCO_FM_PARAM].getValue());

		sinVCAParam = params[NEG_SIN_VCA_PARAM].getValue();
		triVCAParam = params[NEG_TRI_VCA_PARAM].getValue();
		sawVCAParam = params[NEG_SAW_VCA_PARAM].getValue();

		channels = std::max(inputs[NEG_VCO_PITCH_INPUT].getChannels(), masterChanels);
		for (int c = 0; c < channels; c += 4) {
			auto* oscillator = &negativeOscillators[c / 4];
			oscillator->channels = std::min(channels - c, 4);
			oscillator->analog = false;
			oscillator->soft = false;

			float_4 pitch = freqParam;
			float_4 linearFM = 0;
			pitch += inputs[NEG_VCO_PITCH_INPUT].getVoltageSimd<float_4>(c);
			if(cvLock) {
				pitch+=scissorCV[c];
			}
			if (inputs[NEG_VCO_FM_INPUT].isConnected()) {
				if(negVCOFMMode)
					pitch += fmParam * inputs[NEG_VCO_FM_INPUT].getPolyVoltageSimd<float_4>(c);
				else
					linearFM = fmParam * inputs[NEG_VCO_FM_INPUT].getPolyVoltageSimd<float_4>(c) * 10000.0;
			}
			if(pwSkew == 1) {
				pitch += (scissorPW[c] - 0.5) * scissorSkew[c]; //0.5 is scaling factor - maybe make it a parameter?
			} else if (pwSkew ==2 ) {
				pitch -= (scissorPW[c] - 0.5) * scissorSkew[c]; //0.5 is scaling factor - maybe make it a parameter?
			}

			oscillator->setPitch(pitch,linearFM);

			if(!syncLock) {
				oscillator->syncEnabled = inputs[NEG_VCO_SYNC_INPUT].isConnected();
				oscillator->process(args.sampleTime, inputs[NEG_VCO_SYNC_INPUT].getPolyVoltageSimd<float_4>(c));
			} else {
				oscillator->syncEnabled = true;
				oscillator->process(args.sampleTime, scissorOutput[c]);
			}

            float_4 sinVCA4 = simd::clamp(sinVCAParam + inputs[NEG_VCO_SIN_VCA_INPUT].getVoltageSimd<float_4>(c) / 10.f,-1.f,1.f);
            float_4 triVCA4 = simd::clamp(triVCAParam + inputs[NEG_VCO_TRI_VCA_INPUT].getVoltageSimd<float_4>(c) / 10.f,-1.f,1.f);
            float_4 sawVCA4 = simd::clamp(sawVCAParam + inputs[NEG_VCO_SAW_VCA_INPUT].getVoltageSimd<float_4>(c) / 10.f,-1.f,1.f);

			if(c == 0) {
				negSinVCAPercentage = sinVCA4[0];
				negTriVCAPercentage = triVCA4[0];
				negSawVCAPercentage = sawVCA4[0];
			}

			float_4 negScissor = -simd::clamp(simd::clamp(scissorOutput[c],-1.f,0.f) - vcaBias[c],-1.f,0.f);
			float_4 sinVCA4Processed = simd::ifelse(sinVCA4 <= 0, (sinVCA4 + 1.f) * negScissor, simd::clamp(sinVCA4+negScissor,0.f,1.f));
			float_4 triVCA4Processed = simd::ifelse(triVCA4 <= 0, (triVCA4 + 1.f) * negScissor, simd::clamp(triVCA4+negScissor,0.f,1.f));
			float_4 sawVCA4Processed = simd::ifelse(sawVCA4 <= 0, (sawVCA4 + 1.f) * negScissor, simd::clamp(sawVCA4+negScissor,0.f,1.f));

            float_4 totalVCA4 = simd::clamp(sinVCA4Processed + triVCA4Processed + sawVCA4Processed,1.f,3.f);


			float_4 negSin = -simd::abs(oscillator->sin());
			float_4 negTri = -simd::abs(oscillator->tri());
			float_4 negSaw = oscillator->saw();
			negSaw = simd::ifelse(negSaw > 0,negSaw-1,negSaw);

            outputs[NEG_SIN_OUTPUT].setVoltageSimd(negSin * 5.f, c);
            outputs[NEG_TRI_OUTPUT].setVoltageSimd(negTri * 5.f, c);
            outputs[NEG_SAW_OUTPUT].setVoltageSimd(negSaw * 5.0f, c);

            // negativeOutput[c] = (negSin * sinVCA4 + negTri * triVCA4 + negSaw * sawVCA4) / totalVCA4;  
            negativeOutput[c] = ((negSin * sinVCA4Processed) + 
								(negTri * triVCA4Processed) + 
								(negSaw * sawVCA4Processed)) / totalVCA4;  



            outputs[NEG_MIX_OUTPUT].setVoltageSimd(negativeOutput[c] * 5.f, c);
		}
		outputs[NEG_MIX_OUTPUT].setChannels(channels);

		for (int c = 0; c < masterChanels; c += 4) {
            glueOutput[c] = positiveOutput[c] + negativeOutput[c];
            outputs[GLU_OUTPUT].setVoltageSimd(glueOutput[c] * 5.f, c);
		}

		outputs[GLU_OUTPUT].setChannels(channels);

	}
};


struct SliceOfLifeWidget : ModuleWidget {
	SliceOfLifeWidget(SliceOfLife* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SliceOfLife.svg")));

		addChild(createWidget<ScrewSilver>(Vec(5, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 20, 0)));
		addChild(createWidget<ScrewSilver>(Vec(5, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 20, 365)));


		// addParam(createParam<CKSS>(Vec(119, 77), module, SliceOfLife::SIS_VCO_SYNC_PARAM));

		addParam(createParam<RoundHugeFWKnob>(Vec(47, 35), module, SliceOfLife::SIS_VCO_FREQ_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(13, 75), module, SliceOfLife::SIS_VCO_FINE_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(101, 75), module, SliceOfLife::SIS_VCO_FM_1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(101, 125), module, SliceOfLife::SIS_VCO_FM_2_PARAM));
		ParamWidget* sisVCOPWParam = createParam<RoundFWKnob>(Vec(13, 138), module, SliceOfLife::SIS_VCO_PW_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(sisVCOPWParam)->percentage = &module->sisPWPercentage;
		}
		addParam(sisVCOPWParam);							
		addParam(createParam<RoundFWKnob>(Vec(13, 188), module, SliceOfLife::SIS_VCO_PWM_PARAM));

		ParamWidget* sisVCOSkewAmtParam = createParam<RoundFWKnob>(Vec(73, 188), module, SliceOfLife::SIS_SKEW_AMOUNT_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(sisVCOSkewAmtParam)->percentage = &module->sisSkewPercentage;
		}
		addParam(sisVCOSkewAmtParam);							
		addInput(createInput<FWPortInSmall>(Vec(108, 198), module, SliceOfLife::SIS_SKEW_AMOUNT_INPUT));

		
	
		addParam(createParam<LEDButton>(Vec(133, 79), module, SliceOfLife::SIS_VCO_FM_1_MODE_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(134.5, 80.5), module, SliceOfLife::SIS_VCO_FM_1_MODE_LIGHT));

		addParam(createParam<LEDButton>(Vec(133, 129), module, SliceOfLife::SIS_VCO_FM_2_MODE_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(134.5, 130.5), module, SliceOfLife::SIS_VCO_FM_2_MODE_LIGHT));

		addInput(createInput<FWPortInSmall>(Vec(11, 306), module, SliceOfLife::SIS_VCO_PITCH_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(41, 306), module, SliceOfLife::SIS_VCO_FM_1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(71, 306), module, SliceOfLife::SIS_VCO_FM_2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(101, 306), module, SliceOfLife::SIS_VCO_SYNC_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(131, 306), module, SliceOfLife::SIS_VCO_PW_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(14, 340), module, SliceOfLife::SIS_SQR_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(39, 340), module, SliceOfLife::SIS_TRI_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(64, 340), module, SliceOfLife::SIS_SAW_OUTPUT));

		addParam(createParam<LEDButton>(Vec(20, 265), module, SliceOfLife::PW_SKEW_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(21.5, 266.5), module, SliceOfLife::PW_SKEW_LIGHT));

		addParam(createParam<LEDButton>(Vec(70, 265), module, SliceOfLife::CV_LOCK_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(71.5, 266.5), module, SliceOfLife::CV_LOCK_LIGHT));

		addParam(createParam<LEDButton>(Vec(120, 265), module, SliceOfLife::SYNC_LOCK_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(121.5, 266.5), module, SliceOfLife::SYNC_LOCK_LIGHT));



		// addParam(createParam<CKSS>(Vec(15 + 150, 77), module, SliceOfLife::POS_VCO_MODE_PARAM));
		// addParam(createParam<CKSS>(Vec(119 + 150, 77), module, SliceOfLife::POS_VCO_SYNC_PARAM));

		ParamWidget* posVcoFreqParam = createParam<RoundLargeFWKnob>(Vec(62 + 150,35), module, SliceOfLife::POS_VCO_FREQ_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(posVcoFreqParam)->percentage = &module->posVcoFreqParamPercentage;
		}
		addParam(posVcoFreqParam);							
		addParam(createParam<RoundFWKnob>(Vec(23 + 150, 50), module, SliceOfLife::POS_VCO_FINE_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(107 + 150, 50), module, SliceOfLife::POS_VCO_FM_PARAM));

		ParamWidget* posSinVcaParam = createParam<RoundFWKnob>(Vec(13 + 150, 95), module, SliceOfLife::POS_SIN_VCA_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(posSinVcaParam)->percentage = &module->posSinVCAPercentage;
			dynamic_cast<RoundFWKnob*>(posSinVcaParam)->biDirectional = true;			
		}
		addParam(posSinVcaParam);							

		ParamWidget* posTriVcaParam = createParam<RoundFWKnob>(Vec(68 + 150, 95), module, SliceOfLife::POS_TRI_VCA_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(posTriVcaParam)->percentage = &module->posTriVCAPercentage;
			dynamic_cast<RoundFWKnob*>(posTriVcaParam)->biDirectional = true;			
		}
		addParam(posTriVcaParam);							

		ParamWidget* posSawVcaParam = createParam<RoundFWKnob>(Vec(123 + 150, 95), module, SliceOfLife::POS_SAW_VCA_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(posSawVcaParam)->percentage = &module->posSawVCAPercentage;
			dynamic_cast<RoundFWKnob*>(posSawVcaParam)->biDirectional = true;			
		}
		addParam(posSawVcaParam);							

		addInput(createInput<FWPortInSmall>(Vec(42 + 150, 115), module, SliceOfLife::POS_VCO_SIN_VCA_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(98 + 150, 115), module, SliceOfLife::POS_VCO_TRI_VCA_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(152 + 150, 115), module, SliceOfLife::POS_VCO_SAW_VCA_INPUT));

		addParam(createParam<LEDButton>(Vec(139 + 150, 54), module, SliceOfLife::POS_VCO_FM_MODE_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(140.5 + 150, 55.5), module, SliceOfLife::POS_VCO_FM_MODE_LIGHT));



		addInput(createInput<FWPortInSmall>(Vec(26 + 150, 147), module, SliceOfLife::POS_VCO_PITCH_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(60 + 150, 147), module, SliceOfLife::POS_VCO_FM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(95 + 150, 147), module, SliceOfLife::POS_VCO_SYNC_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(14 + 120, 340), module, SliceOfLife::POS_SIN_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(39 + 120, 340), module, SliceOfLife::POS_TRI_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(64 + 120, 340), module, SliceOfLife::POS_SAW_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(89 + 120, 340), module, SliceOfLife::POS_MIX_OUTPUT));

        // addParam(createParam<CKSS>(Vec(15 + 300, 77), module, SliceOfLife::NEG_VCO_MODE_PARAM));
		// addParam(createParam<CKSS>(Vec(119 + 300, 77), module, SliceOfLife::NEG_VCO_SYNC_PARAM));

		addParam(createParam<RoundLargeFWKnob>(Vec(62 + 150, 194), module, SliceOfLife::NEG_VCO_FREQ_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(23 + 150, 209), module, SliceOfLife::NEG_VCO_FINE_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(107 + 150, 209), module, SliceOfLife::NEG_VCO_FM_PARAM));

		ParamWidget* negSinVcaParam = createParam<RoundFWKnob>(Vec(13 + 150, 254), module, SliceOfLife::NEG_SIN_VCA_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(negSinVcaParam)->percentage = &module->negSinVCAPercentage;
			dynamic_cast<RoundFWKnob*>(negSinVcaParam)->biDirectional = true;			
		}
		addParam(negSinVcaParam);							

		ParamWidget* negTriVcaParam = createParam<RoundFWKnob>(Vec(68 + 150, 254), module, SliceOfLife::NEG_TRI_VCA_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(negTriVcaParam)->percentage = &module->negTriVCAPercentage;
			dynamic_cast<RoundFWKnob*>(negTriVcaParam)->biDirectional = true;			
		}
		addParam(negTriVcaParam);							

		ParamWidget* negSawVcaParam = createParam<RoundFWKnob>(Vec(123 + 150, 254), module, SliceOfLife::NEG_SAW_VCA_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(negSawVcaParam)->percentage = &module->negSawVCAPercentage;
			dynamic_cast<RoundFWKnob*>(negSawVcaParam)->biDirectional = true;			
		}
		addParam(negSawVcaParam);							




        addInput(createInput<FWPortInSmall>(Vec(42 + 150, 274), module, SliceOfLife::NEG_VCO_SIN_VCA_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(98 + 150, 274)	, module, SliceOfLife::NEG_VCO_TRI_VCA_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(152 + 150, 274), module, SliceOfLife::NEG_VCO_SAW_VCA_INPUT));

		addParam(createParam<LEDButton>(Vec(139 + 150, 213), module, SliceOfLife::NEG_VCO_FM_MODE_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(140.5 + 150, 214.5), module, SliceOfLife::NEG_VCO_FM_MODE_LIGHT));

		addInput(createInput<FWPortInSmall>(Vec(26 + 150, 306), module, SliceOfLife::NEG_VCO_PITCH_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(60 + 150, 306), module, SliceOfLife::NEG_VCO_FM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(95 + 150, 306), module, SliceOfLife::NEG_VCO_SYNC_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(14 + 230, 340), module, SliceOfLife::NEG_SIN_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(39 + 230, 340), module, SliceOfLife::NEG_TRI_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(64 + 230, 340), module, SliceOfLife::NEG_SAW_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(89 + 230, 340), module, SliceOfLife::NEG_MIX_OUTPUT));


		addParam(createParam<RoundFWKnob>(Vec(345, 50), module, SliceOfLife::VCA_BIAS_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(345, 100), module, SliceOfLife::MIX_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(335, 150), module, SliceOfLife::VCA_CV_DEPTH_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(365, 155), module, SliceOfLife::MIX_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(365, 155), module, SliceOfLife::VCA_CV_INPUT));


		addOutput(createOutput<FWPortOutSmall>(Vec(360, 340), module, SliceOfLife::GLU_OUTPUT));


		// addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(99, 42.5f), module, SliceOfLife::PHASE_LIGHT));
	}
};


Model* modelSliceOfLife = createModel<SliceOfLife, SliceOfLifeWidget>("SliceOfLife");
