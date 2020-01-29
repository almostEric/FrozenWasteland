#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "dsp-noise/noise.hpp"
#include "filters/biquad.h"

using namespace frozenwasteland::dsp;

struct EverlastingGlottalStopper : Module {
	enum ParamIds {
		FREQUENCY_PARAM,
		TIME_OPEN_PARAM,
		TIME_CLOSED_PARAM,
		BREATHINESS_PARAM,
		FM_CV_ATTENUVERTER_PARAM,
		TIME_OPEN_CV_ATTENUVERTER_PARAM,
		TIME_CLOSED_CV_ATTENUVERTER_PARAM,
		BREATHINESS_CV_ATTENUVERTER_PARAM,
		DEEMPHASIS_FILTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,		
		FM_INPUT,		
		TIME_OPEN_INPUT,		
		TIME_CLOSED_INPUT,	
		BREATHINESS_INPUT,
		DEEMPHASIS_FILTER_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOICE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		DEEMPHASIS_FILTER_LIGHT,
		NUM_LIGHTS
	};

	Biquad* deemphasisFilter;
	GaussianNoiseGenerator _gauss;
	float phase = 0.0;
	bool demphasisFilterActive = false;

	dsp::SchmittTrigger demphasisFilterTrigger; 

	EverlastingGlottalStopper() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQUENCY_PARAM, -54.0f, 54.0f, 0.0f,"Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(TIME_OPEN_PARAM, 0.01f, 1.0f, 0.5f, "Time Open","%",0,100);
		configParam(TIME_CLOSED_PARAM, 0.0f, 0.9f, 0.0f, "Time Closed","%",0,100);
		configParam(BREATHINESS_PARAM, 0.0f, 0.9f, 0.0f, "Breathiness","%",0,100);
		configParam(FM_CV_ATTENUVERTER_PARAM, 0.0, 1.0, 0, "FM CV Attenuation","%",0,100);
		configParam(TIME_OPEN_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0, "Time Open CV Attenuation","%",0,100);
		configParam(TIME_CLOSED_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0, "Time Closed CV Attenuation","%",0,100);
		configParam(BREATHINESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0, "Breathiness CV Attenuation","%",0,100);
		//addParam(createParam<CKSS>(Vec(123, 300), module, EverlastingGlottalStopper::DEEMPHASIS_FILTER_PARAM, 0.0, 1.0, 0));

		float sampleRate = APP->engine->getSampleRate();
		deemphasisFilter = new Biquad(bq_type_lowpass, 2000 / sampleRate, 1, 0);
	}


	void process(const ProcessArgs &args) override;
};



float Rosenburg(float timeOpening, float timeOpen, float phase) {
	float out;
	if(phase < timeOpening) {
		out = 0.5f * (1.0f - cosf(M_PI * phase / timeOpening));
	} else if (phase < timeOpen) {	
		out = cosf(M_PI * (phase - timeOpening) / (timeOpen - timeOpening) / 2);
	} else {
		out = 0.0f;
	}

	return out;
}

float HanningWindow(float phase) {
	return 0.5f * (1 - cosf(2 * M_PI * phase));
}

inline float quadraticBipolarEG(float x) {
	float x2 = x*x;
	return (x >= 0.f) ? x2 : -x2;
}

void EverlastingGlottalStopper::process(const ProcessArgs &args) {
	
	if (demphasisFilterTrigger.process(params[DEEMPHASIS_FILTER_PARAM].getValue() || inputs[DEEMPHASIS_FILTER_INPUT].getVoltage())) {
		demphasisFilterActive = !demphasisFilterActive;		
	}
	lights[DEEMPHASIS_FILTER_LIGHT].value = demphasisFilterActive;

	float pitch = params[FREQUENCY_PARAM].getValue();	
	float pitchCv = 12.0f * inputs[PITCH_INPUT].getVoltage();
	float fm = 0;
	if (inputs[FM_INPUT].isConnected()) {
		//pitchCv += dsp::quadraticBipolar(params[FM_CV_ATTENUVERTER_PARAM].getValue()) * 12.0f * inputs[FM_INPUT].getVoltage();
		fm = params[FM_CV_ATTENUVERTER_PARAM].getValue() * inputs[FM_INPUT].getVoltage() * 1000.0;
	}

	pitch += pitchCv;
		// Note C4
	float freq = (261.626f * powf(2.0f, pitch / 12.0f)) + fm;

	//float pitch = params[FREQUENCY_PARAM].getValue() + inputs[FM_INPUT].getVoltage() * params[FM_CV_ATTENUVERTER_PARAM].getValue();	
	//float freq = powf(2.0, pitch);

	float timeOpening = clamp(params[TIME_OPEN_PARAM].getValue() + inputs[TIME_OPEN_INPUT].getVoltage() * params[TIME_OPEN_CV_ATTENUVERTER_PARAM].getValue(),0.01f,1.0f);
	float timeClosed = clamp(params[TIME_CLOSED_PARAM].getValue() + inputs[TIME_CLOSED_INPUT].getVoltage() * params[TIME_CLOSED_CV_ATTENUVERTER_PARAM].getValue(),0.0f,1.0f);
	float timeOpen = clamp(1.0-timeClosed,timeOpening,1.0);

	float dt = 1.0 / args.sampleRate;
	float deltaPhase = fminf(freq * dt, 0.5);
	phase += deltaPhase;
	if (phase >= 1.0) {
		phase -= 1.0;
	}

	float out = Rosenburg(timeOpening,timeOpen,phase);
	float noiseLevel = clamp(params[BREATHINESS_PARAM].getValue() + inputs[BREATHINESS_INPUT].getVoltage() * params[BREATHINESS_CV_ATTENUVERTER_PARAM].getValue(),0.0f,1.0f);
 	//Noise level follows glottal wave
	// noise = _gauss.next() * out * noiseLevel;
	float noise = _gauss.next() / 5.0 * noiseLevel * HanningWindow(phase);

	out = out + noise;
	if(demphasisFilterActive) {
		out = deemphasisFilter->process(out);
	}

	outputs[VOICE_OUTPUT].setVoltage(out * 10.0f  - 5.0f);
	
}




struct EverlastingGlottalStopperWidget : ModuleWidget {
	EverlastingGlottalStopperWidget(EverlastingGlottalStopper *module)  {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/EverlastingGlottalStopper.svg")));
		
		
		

		addParam(createParam<RoundFWKnob>(Vec(44, 60), module, EverlastingGlottalStopper::FREQUENCY_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(12, 180), module, EverlastingGlottalStopper::TIME_OPEN_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(52, 180), module, EverlastingGlottalStopper::TIME_CLOSED_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(86, 180), module, EverlastingGlottalStopper::BREATHINESS_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(88, 132), module, EverlastingGlottalStopper::FM_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(12, 228), module, EverlastingGlottalStopper::TIME_OPEN_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(52, 228), module, EverlastingGlottalStopper::TIME_CLOSED_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(88, 228), module, EverlastingGlottalStopper::BREATHINESS_CV_ATTENUVERTER_PARAM));
		

		addParam(createParam<LEDButton>(Vec(15, 275), module, EverlastingGlottalStopper::DEEMPHASIS_FILTER_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(16.5, 276.5), module, EverlastingGlottalStopper::DEEMPHASIS_FILTER_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(38, 275), module, EverlastingGlottalStopper::DEEMPHASIS_FILTER_INPUT));


		addInput(createInput<FWPortInSmall>(Vec(24, 110), module, EverlastingGlottalStopper::PITCH_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(90, 110), module, EverlastingGlottalStopper::FM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(14, 207), module, EverlastingGlottalStopper::TIME_OPEN_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(54, 207), module, EverlastingGlottalStopper::TIME_CLOSED_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(89, 207), module, EverlastingGlottalStopper::BREATHINESS_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(52, 330), module, EverlastingGlottalStopper::VOICE_OUTPUT));

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};


Model *modelEverlastingGlottalStopper = createModel<EverlastingGlottalStopper, EverlastingGlottalStopperWidget>("EverlastingGlottalStopper");
