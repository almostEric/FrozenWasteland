#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

struct LowFrequencyOscillator {
	float phase = 0.0;
	float pw = 0.5;
	float freq = 1.0;
	bool offset = false;
	bool invert = false;
	dsp::SchmittTrigger resetTrigger;
	LowFrequencyOscillator() {}
	void setPitch(float pitch) {
		pitch = fminf(pitch, 8.0);
		freq = powf(2.0, pitch);
	}
	void setPulseWidth(float pw_) {
		const float pwMin = 0.01;
		pw = clamp(pw_, pwMin, 1.0f - pwMin);
	}
	void setReset(float reset) {
		if (resetTrigger.process(reset)) {
			phase = 0.0;
		}
	}
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5);
		phase += deltaPhase;
		if (phase >= 1.0)
			phase -= 1.0;
	}
	float sin() {
		if (offset)
			return 1.0 - cosf(2*M_PI * phase) * (invert ? -1.0 : 1.0);
		else
			return sinf(2*M_PI * phase) * (invert ? -1.0 : 1.0);
	}
	float tri(float x) {
		return 4.0 * fabsf(x - roundf(x));
	}
	float tri() {
		if (offset)
			return tri(invert ? phase - 0.5 : phase);
		else
			return -1.0 + tri(invert ? phase - 0.25 : phase - 0.75);
	}
	float saw(float x) {
		return 2.0 * (x - roundf(x));
	}
	float saw() {
		if (offset)
			return invert ? 2.0 * (1.0 - phase) : 2.0 * phase;
		else
			return saw(phase) * (invert ? -1.0 : 1.0);
	}
	float sqr() {
		float sqr = (phase < pw) ^ invert ? 1.0 : -1.0;
		return offset ? sqr + 1.0 : sqr;
	}
	float light() {
		return sinf(2*M_PI * phase);
	}
};



struct QuantussyCell : Module {
	enum ParamIds {
		FREQ_PARAM,
		CV_ATTENUVERTER_PARAM,
		LOW_LIMIT_PARAM,
		HI_LIMIT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CASTLE_INPUT,
		CV_INPUT,
		CV_AMOUNT_INPUT,
		LOW_LIMIT_CV_INPUT,
		HI_LIMIT_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CASTLE_OUTPUT,
		SIN_OUTPUT,
		TRI_OUTPUT,
		SAW_OUTPUT,
		SQR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};


	LowFrequencyOscillator oscillator;


	//Stuff for S&Hs
	dsp::SchmittTrigger _castleTrigger, _cvTrigger;
	float _value1, _value2;
	//Castle S&H is #1, CV #2

	//percentages
	float cvAttenuverterPercentage = 0;
	float lowLimitPercentage = 0;
	float hiLimitPercentage = 0;

	QuantussyCell() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(FREQ_PARAM, -3.0, 3.0, 0.0,"Frequency", " Hz", 2, 1);
		configParam(CV_ATTENUVERTER_PARAM, -1.0, 1.0, 1.0,"CV Attenuator","%",0,100);	


		configParam(LOW_LIMIT_PARAM, -10.0, 10.0, -10.0,"Low Lmit", " Hz", 2, 1);
		configParam(HI_LIMIT_PARAM, -10.0, 10.0, 10.0,"High Limit", " Hz", 2, 1);

	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void QuantussyCell::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;

	float lowLimit = clamp(params[LOW_LIMIT_PARAM].getValue() + (inputs[LOW_LIMIT_CV_INPUT].getVoltage()),-10.0f,10.0f);
	lowLimitPercentage = (lowLimit + 10.0) / 20.0;
	float hiLimit = clamp(params[HI_LIMIT_PARAM].getValue() + (inputs[HI_LIMIT_CV_INPUT].getVoltage()),-10.0f,10.0f);
	hiLimitPercentage = (hiLimit + 10.0) / 20.0;
	float pitch = clamp(params[FREQ_PARAM].getValue()  + _value2,lowLimit,hiLimit);

	oscillator.setPitch(pitch);
	oscillator.step(1.0 / args.sampleRate);

	outputs[SIN_OUTPUT].setVoltage(5.0 * oscillator.sin());
	outputs[TRI_OUTPUT].setVoltage(5.0 * oscillator.tri());
	outputs[SAW_OUTPUT].setVoltage(5.0 * oscillator.saw());

	float squareOutput = 5.0 * oscillator.sqr(); //Used a lot :)
	outputs[SQR_OUTPUT].setVoltage(squareOutput);



	//Process Castle
	if (_castleTrigger.process(squareOutput)) {
		if (inputs[CASTLE_INPUT].isConnected()) {
			_value1 = inputs[CASTLE_INPUT].getVoltage();
		}
		else {
			_value1 = 0; //Maybe at some point add a default noise source, but not for now
		}
	}
	outputs[CASTLE_OUTPUT].setVoltage(_value1);

	//Process CV
	float attenuverting = clamp(params[CV_ATTENUVERTER_PARAM].getValue() + (inputs[CV_AMOUNT_INPUT].getVoltage() / 10.0f),-1.0f,1.0f);
	cvAttenuverterPercentage = attenuverting;
	if (_cvTrigger.process(squareOutput)) {
		if (inputs[CV_INPUT].isConnected()) {
			_value2 = inputs[CV_INPUT].getVoltage() * attenuverting;
		}
		else {
			_value2 = 0; //Maybe at some point add a default noise source, but not for now
		}	
	}


	lights[BLINK_LIGHT].setSmoothBrightness(fmaxf(0.0, oscillator.light()), deltaTime);

}

struct QuantussyCellWidget : ModuleWidget {
	QuantussyCellWidget(QuantussyCell *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuantussyCell.svg")));
		
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, 0)));	
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundLargeFWKnob>(Vec(28, 54), module, QuantussyCell::FREQ_PARAM));							
		ParamWidget* cvAttenuverterParam = createParam<RoundFWKnob>(Vec(13, 180), module, QuantussyCell::CV_ATTENUVERTER_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(cvAttenuverterParam)->percentage = &module->cvAttenuverterPercentage;
			dynamic_cast<RoundFWKnob*>(cvAttenuverterParam)->biDirectional = true;
		}
		addParam(cvAttenuverterParam);							

		ParamWidget* lowLimitParam = createParam<RoundSmallFWKnob>(Vec(5, 90), module, QuantussyCell::LOW_LIMIT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(lowLimitParam)->percentage = &module->lowLimitPercentage;
		}
		addParam(lowLimitParam);							
		addInput(createInput<FWPortInSmall>(Vec(8, 116), module, QuantussyCell::LOW_LIMIT_CV_INPUT));
		ParamWidget* hiLimitParam = createParam<RoundSmallFWKnob>(Vec(60, 90), module, QuantussyCell::HI_LIMIT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(hiLimitParam)->percentage = &module->hiLimitPercentage;
		}
		addParam(hiLimitParam);							
		addInput(createInput<FWPortInSmall>(Vec(63, 116), module, QuantussyCell::HI_LIMIT_CV_INPUT));


		addInput(createInput<PJ301MPort>(Vec(5, 154), module, QuantussyCell::CASTLE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(50, 203), module, QuantussyCell::CV_INPUT));
		addInput(createInput<PJ301MPort>(Vec(15, 212), module, QuantussyCell::CV_AMOUNT_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(60, 154), module, QuantussyCell::CASTLE_OUTPUT));

		addOutput(createOutput<PJ301MPort>(Vec(15, 255), module, QuantussyCell::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(50, 255), module, QuantussyCell::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(15, 301), module, QuantussyCell::SQR_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(50, 301), module, QuantussyCell::SAW_OUTPUT));


		addChild(createLight<LargeLight<BlueLight>>(Vec(69,58), module, QuantussyCell::BLINK_LIGHT));
	}
};

Model *modelQuantussyCell = createModel<QuantussyCell, QuantussyCellWidget>("QuantussyCell");
