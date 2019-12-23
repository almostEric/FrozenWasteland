//This code and artwork is based on BEFACO's Rampage module.
//Modifed December, 2019. Still under a GNU GPL v3 License



#include "FrozenWasteland.hpp"
#include <stdlib.h>
#include <time.h>



static float shapeDelta(float delta, float tau, float shape) {
	float lin = sgn(delta) * 10.f / tau;
	if (shape < 0.f) {
		float log = sgn(delta) * 40.f / tau / (std::fabs(delta) + 1.f);
		return crossfade(lin, log, -shape * 0.95f);
	}
	else {
		float exp = M_E * delta / tau;
		return crossfade(lin, exp, shape * 0.90f);
	}
}


struct DrunkenRampage : Module {
	enum ParamIds {
		RANGE_A_PARAM,
		RANGE_B_PARAM,
		SHAPE_A_PARAM,
		SHAPE_B_PARAM,
		TRIGG_A_PARAM,
		TRIGG_B_PARAM,
		RISE_A_PARAM,
		RISE_B_PARAM,
		FALL_A_PARAM,
		FALL_B_PARAM,
		CYCLE_A_PARAM,
		CYCLE_B_PARAM,
		BALANCE_PARAM,
		BAC_A_PARAM,
		BAC_B_PARAM,
		AWARENESS_A_PARAM,
		AWARENESS_B_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN_A_INPUT,
		IN_B_INPUT,
		TRIGG_A_INPUT,
		TRIGG_B_INPUT,
		RISE_CV_A_INPUT,
		RISE_CV_B_INPUT,
		FALL_CV_A_INPUT,
		FALL_CV_B_INPUT,
		EXP_CV_A_INPUT,
		EXP_CV_B_INPUT,
		CYCLE_A_INPUT,
		CYCLE_B_INPUT,
		BAC_CV_A_INPUT,
		BAC_CV_B_INPUT,
		AWARENESS_A_CV_INPUT,
		AWARENESS_B_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		RISING_A_OUTPUT,
		RISING_B_OUTPUT,
		FALLING_A_OUTPUT,
		FALLING_B_OUTPUT,
		EOC_A_OUTPUT,
		EOC_B_OUTPUT,
		OUT_A_OUTPUT,
		OUT_B_OUTPUT,
		COMPARATOR_OUTPUT,
		MIN_OUTPUT,
		MAX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		COMPARATOR_LIGHT,
		MIN_LIGHT,
		MAX_LIGHT,
		OUT_A_LIGHT,
		OUT_B_LIGHT,
		RISING_A_LIGHT,
		RISING_B_LIGHT,
		FALLING_A_LIGHT,
		FALLING_B_LIGHT,
		NUM_LIGHTS
	};

	float out[2] = {};
	float lastIn[2] = {};
	float target[2] = {};
 	bool gate[2] = {};
	dsp::SchmittTrigger trigger[2];
	dsp::PulseGenerator endOfCyclePulse[2];

	DrunkenRampage() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RANGE_A_PARAM, 0.0, 2.0, 0.0, "Ch 1 range");
		configParam(SHAPE_A_PARAM, -1.0, 1.0, 0.0, "Ch 1 shape");
		configParam(TRIGG_A_PARAM, 0.0, 1.0, 0.0, "Ch 1 trigger");
		configParam(RISE_A_PARAM, 0.0, 1.0, 0.0, "Ch 1 rise time");
		configParam(FALL_A_PARAM, 0.0, 1.0, 0.0, "Ch 1 fall time");
		configParam(CYCLE_A_PARAM, 0.0, 1.0, 0.0, "Ch 1 cycle");
		configParam(RANGE_B_PARAM, 0.0, 2.0, 0.0, "Ch 2 range");
		configParam(SHAPE_B_PARAM, -1.0, 1.0, 0.0, "Ch 2 shape");
		configParam(TRIGG_B_PARAM, 0.0, 1.0, 0.0, "Ch 2 trigger");
		configParam(RISE_B_PARAM, 0.0, 1.0, 0.0, "Ch 2 rise time");
		configParam(FALL_B_PARAM, 0.0, 1.0, 0.0, "Ch 2 fall time");
		configParam(CYCLE_B_PARAM, 0.0, 1.0, 0.0, "Ch 2 cycle");
		configParam(BALANCE_PARAM, 0.0, 1.0, 0.5, "Balance");
		configParam(BAC_A_PARAM, 0.0, 1.0, 0.0, "Ch 1 BAC","%",0,100);
		configParam(BAC_B_PARAM, 0.0, 1.0, 0.0, "Ch 2 BAC","%",0,100);
		configParam(AWARENESS_A_PARAM, 0.0, 1.0, 0.0, "Ch 1 awareness");
		configParam(AWARENESS_B_PARAM, 0.0, 1.0, 0.0, "Ch 2 awareness");

		srand(time(NULL));
	}

	void process(const ProcessArgs &args) override {
		for (int c = 0; c < 2; c++) {
			float in = inputs[IN_A_INPUT + c].getVoltage();
			if (trigger[c].process(params[TRIGG_A_PARAM + c].getValue() * 10.0 + inputs[TRIGG_A_INPUT + c].getVoltage() / 2.0)) {
				gate[c] = true;
			}
			if (gate[c]) {
				in = 10.0;
			}

			float bac = params[BAC_A_PARAM + c].getValue() + inputs[BAC_CV_A_INPUT + c].getVoltage() / 10.0;

			if(std::abs(in - lastIn[c]) > 1e-3) {
				float lagDelta = in - out[c];
				target[c] = in + (lagDelta * bac * ((double) rand()/RAND_MAX));
				lastIn[c] = in;
			}
			float shape = params[SHAPE_A_PARAM + c].getValue();
			float delta = target[c] - out[c];

			// Integrator
			float minTime;
			switch ((int) params[RANGE_A_PARAM + c].getValue()) {
				case 0: minTime = 1e-2; break;
				case 1: minTime = 1e-3; break;
				default: minTime = 1e-1; break;
			}

			bool rising = false;
			bool falling = false;

			if (delta > 0) {
				// Rise
				float riseCv = params[RISE_A_PARAM + c].getValue() - inputs[EXP_CV_A_INPUT + c].getVoltage() / 10.0 + inputs[RISE_CV_A_INPUT + c].getVoltage() / 10.0;
				riseCv = clamp(riseCv, 0.0f, 1.0f);
				float rise = minTime * std::pow(2.0, riseCv * 10.0);
				out[c] += shapeDelta(delta, rise, shape) * args.sampleTime;
				rising = (target[c] - out[c] > 1e-3);
				if (!rising) {
					gate[c] = false;
				}
			}
			else if (delta < 0) {
				// Fall
				float fallCv = params[FALL_A_PARAM + c].getValue() - inputs[EXP_CV_A_INPUT + c].getVoltage() / 10.0 + inputs[FALL_CV_A_INPUT + c].getVoltage() / 10.0;
				fallCv = clamp(fallCv, 0.0f, 1.0f);
				float fall = minTime * std::pow(2.0, fallCv * 10.0);
				out[c] += shapeDelta(delta, fall, shape) * args.sampleTime;
				falling = (target[c] - out[c] < -1e-3);
				if (!falling && !(in - out[c] < -1e-3)) {
					// End of cycle, check if we should turn the gate back on (cycle mode)
					endOfCyclePulse[c].trigger(1e-3);
					if (params[CYCLE_A_PARAM + c].getValue() * 10.0 + inputs[CYCLE_A_INPUT + c].getVoltage() >= 4.0) {
						gate[c] = true;
					}
				}
			}
			else {
				gate[c] = false;
			}

			if (!rising && !falling) {
				out[c] = target[c];
				lastIn[c] = -99; //Find a better number to reset
			}

			outputs[RISING_A_OUTPUT + c].setVoltage((rising ? 10.0 : 0.0));
			outputs[FALLING_A_OUTPUT + c].setVoltage((falling ? 10.0 : 0.0));
			lights[RISING_A_LIGHT + c].setSmoothBrightness(rising ? 1.0 : 0.0, args.sampleTime);
			lights[FALLING_A_LIGHT + c].setSmoothBrightness(falling ? 1.0 : 0.0, args.sampleTime);
			outputs[EOC_A_OUTPUT + c].setVoltage((endOfCyclePulse[c].process(args.sampleTime) ? 10.0 : 0.0));
			outputs[OUT_A_OUTPUT + c].setVoltage(out[c]);
			lights[OUT_A_LIGHT + c].setSmoothBrightness(out[c] / 10.0, args.sampleTime);
		}

		// Logic
		float balance = params[BALANCE_PARAM].getValue();
		float a = out[0];
		float b = out[1];
		if (balance < 0.5)
			b *= 2.0 * balance;
		else if (balance > 0.5)
			a *= 2.0 * (1.0 - balance);
		outputs[COMPARATOR_OUTPUT].setVoltage((b > a ? 10.0 : 0.0));
		outputs[MIN_OUTPUT].setVoltage(std::min(a, b));
		outputs[MAX_OUTPUT].setVoltage(std::max(a, b));
		// Lights
		lights[COMPARATOR_LIGHT].setSmoothBrightness(outputs[COMPARATOR_OUTPUT].value / 10.0, args.sampleTime);
		lights[MIN_LIGHT].setSmoothBrightness(outputs[MIN_OUTPUT].value / 10.0, args.sampleTime);
		lights[MAX_LIGHT].setSmoothBrightness(outputs[MAX_OUTPUT].value / 10.0, args.sampleTime);
	}
};




struct DrunkenRampageWidget : ModuleWidget {
	DrunkenRampageWidget(DrunkenRampage *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DrunkenRampage.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<BefacoSwitch>(Vec(133, 32), module, DrunkenRampage::RANGE_A_PARAM));
		addParam(createParam<BefacoTinyKnob>(Vec(27, 90), module, DrunkenRampage::SHAPE_A_PARAM));
		addParam(createParam<BefacoTinyKnob>(Vec(97, 130), module, DrunkenRampage::BAC_A_PARAM));
		addParam(createParam<BefacoPush>(Vec(117, 82), module, DrunkenRampage::TRIGG_A_PARAM));
		addParam(createParam<BefacoSlidePot>(Vec(16, 135), module, DrunkenRampage::RISE_A_PARAM));
		addParam(createParam<BefacoSlidePot>(Vec(57, 135), module, DrunkenRampage::FALL_A_PARAM));
		addParam(createParam<BefacoSwitch>(Vec(146, 238), module, DrunkenRampage::CYCLE_A_PARAM));
		addParam(createParam<BefacoSwitch>(Vec(192, 32), module, DrunkenRampage::RANGE_B_PARAM));
		addParam(createParam<BefacoTinyKnob>(Vec(305, 90), module, DrunkenRampage::SHAPE_B_PARAM));
		addParam(createParam<BefacoTinyKnob>(Vec(235, 130), module, DrunkenRampage::BAC_B_PARAM));
		addParam(createParam<BefacoPush>(Vec(215, 82), module, DrunkenRampage::TRIGG_B_PARAM));
		addParam(createParam<BefacoSlidePot>(Vec(287, 135), module, DrunkenRampage::RISE_B_PARAM));
		addParam(createParam<BefacoSlidePot>(Vec(328, 135), module, DrunkenRampage::FALL_B_PARAM));
		addParam(createParam<BefacoSwitch>(Vec(186, 238), module, DrunkenRampage::CYCLE_B_PARAM));
		addParam(createParam<Davies1900hWhiteKnob>(Vec(162, 76), module, DrunkenRampage::BALANCE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(14, 30), module, DrunkenRampage::IN_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(97, 37), module, DrunkenRampage::TRIGG_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(7, 268), module, DrunkenRampage::RISE_CV_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(67, 268), module, DrunkenRampage::FALL_CV_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(39, 297), module, DrunkenRampage::EXP_CV_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(109, 297), module, DrunkenRampage::BAC_CV_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(147, 290), module, DrunkenRampage::CYCLE_A_INPUT));
		addInput(createInput<PJ301MPort>(Vec(319, 30), module, DrunkenRampage::IN_B_INPUT));
		addInput(createInput<PJ301MPort>(Vec(237, 37), module, DrunkenRampage::TRIGG_B_INPUT));
		addInput(createInput<PJ301MPort>(Vec(266, 268), module, DrunkenRampage::RISE_CV_B_INPUT));
		addInput(createInput<PJ301MPort>(Vec(327, 268), module, DrunkenRampage::FALL_CV_B_INPUT));
		addInput(createInput<PJ301MPort>(Vec(297, 297), module, DrunkenRampage::EXP_CV_B_INPUT));
		addInput(createInput<PJ301MPort>(Vec(227, 297), module, DrunkenRampage::BAC_CV_B_INPUT));
		addInput(createInput<PJ301MPort>(Vec(188, 290), module, DrunkenRampage::CYCLE_B_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(8, 326), module, DrunkenRampage::RISING_A_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(68, 326), module, DrunkenRampage::FALLING_A_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(149, 326), module, DrunkenRampage::EOC_A_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(147, 195), module, DrunkenRampage::OUT_A_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(267, 326), module, DrunkenRampage::RISING_B_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(327, 326), module, DrunkenRampage::FALLING_B_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(185, 326), module, DrunkenRampage::EOC_B_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(187, 195), module, DrunkenRampage::OUT_B_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(167, 133), module, DrunkenRampage::COMPARATOR_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(134, 157), module, DrunkenRampage::MIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(200, 157), module, DrunkenRampage::MAX_OUTPUT));

		addChild(createLight<SmallLight<RedLight>>(Vec(176, 167), module, DrunkenRampage::COMPARATOR_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(168, 174), module, DrunkenRampage::MIN_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(186, 174), module, DrunkenRampage::MAX_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(171, 185), module, DrunkenRampage::OUT_A_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(183, 185), module, DrunkenRampage::OUT_B_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(16.5, 312), module, DrunkenRampage::RISING_A_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(76.5, 312), module, DrunkenRampage::FALLING_A_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(275.5, 312), module, DrunkenRampage::RISING_B_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(335.5, 312), module, DrunkenRampage::FALLING_B_LIGHT));
	}
};


Model *modelDrunkenRampage = createModel<DrunkenRampage, DrunkenRampageWidget>("DrunkenRampage");
