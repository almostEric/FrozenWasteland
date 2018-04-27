#include "FrozenWasteland.hpp"
#include "dsp/digital.hpp"


struct EverlastingGlottalStopper : Module {
	enum ParamIds {
		FREQUENCY_PARAM,
		TIME_OPEN_PARAM,
		TIME_CLOSED_PARAM,
		FM_CV_ATTENUVERTER_PARAM,
		TIME_OPEN_CV_ATTENUVERTER_PARAM,
		TIME_CLOSED_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,		
		FM_INPUT,		
		TIME_OPEN_INPUT,		
		TIME_CLOSED_INPUT,		
		NUM_INPUTS
	};
	enum OutputIds {
		VOICE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LEARN_LIGHT,
		NUM_LIGHTS
	};

	EverlastingGlottalStopper() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
	}

	float phase = 0.0;

	void step() override;
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


void EverlastingGlottalStopper::step() {
	
	float pitch = params[FREQUENCY_PARAM].value + inputs[FM_INPUT].value * params[FM_CV_ATTENUVERTER_PARAM].value;	
	float freq = powf(2.0, pitch);

	float timeOpening = clamp(params[TIME_OPEN_PARAM].value + inputs[TIME_OPEN_INPUT].value * params[TIME_OPEN_CV_ATTENUVERTER_PARAM].value,0.01f,1.0f);
	float timeClosed = clamp(params[TIME_CLOSED_PARAM].value + inputs[TIME_CLOSED_INPUT].value * params[TIME_CLOSED_CV_ATTENUVERTER_PARAM].value,0.0f,1.0f);
	float timeOpen = clamp(1.0-timeClosed,timeOpening,1.0);

	float dt = 1.0 / engineGetSampleRate();
	float deltaPhase = fminf(freq * dt, 0.5);
	phase += deltaPhase;
	if (phase >= 1.0) {
		phase -= 1.0;
	}

	float out = Rosenburg(timeOpening,timeOpen,phase);


	outputs[VOICE_OUTPUT].value = out * 10.0f - 5.0f;
	
}




struct EverlastingGlottalStopperWidget : ModuleWidget {
	EverlastingGlottalStopperWidget(EverlastingGlottalStopper *module);
};

EverlastingGlottalStopperWidget::EverlastingGlottalStopperWidget(EverlastingGlottalStopper *module) : ModuleWidget(module) {
	box.size = Vec(15*9, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/EverlastingGlottalStopper.svg")));
		addChild(panel);
	}
	
	

	addParam(ParamWidget::create<RoundHugeBlackKnob>(Vec(42, 60), module, EverlastingGlottalStopper::FREQUENCY_PARAM, 5.0f, 11.0f, 7.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(18, 215), module, EverlastingGlottalStopper::TIME_OPEN_PARAM, 0.01f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(91, 215), module, EverlastingGlottalStopper::TIME_CLOSED_PARAM, 0.0f, 0.9f, 0.0f));
	addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(91, 164), module, EverlastingGlottalStopper::FM_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0));
	addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(20, 275), module, EverlastingGlottalStopper::TIME_OPEN_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0));
	addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(90, 275), module, EverlastingGlottalStopper::TIME_CLOSED_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0));



	addInput(Port::create<PJ301MPort>(Vec(19, 135), Port::INPUT, module, EverlastingGlottalStopper::PITCH_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(90, 135), Port::INPUT, module, EverlastingGlottalStopper::FM_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(20, 247), Port::INPUT, module, EverlastingGlottalStopper::TIME_OPEN_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(93, 247), Port::INPUT, module, EverlastingGlottalStopper::TIME_CLOSED_INPUT));

	addOutput(Port::create<PJ301MPort>(Vec(57, 317), Port::OUTPUT, module, EverlastingGlottalStopper::VOICE_OUTPUT));

	addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model *modelEverlastingGlottalStopper = Model::create<EverlastingGlottalStopper, EverlastingGlottalStopperWidget>("Frozen Wasteland", "EverlastingGlottalStopper", "Everlasting Glottal Stopper", FILTER_TAG);
