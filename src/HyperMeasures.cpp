//#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"


#define NUM_RHYTHMS 4


struct HyperMeasures : Module {
	enum ParamIds {
		RHYTHM_1_NUMERATOR_PARAM,
		RHYTHM_1_NUMERATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_1_DENOMINATOR_PARAM,
		RHYTHM_1_DENOMINATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_2_NUMERATOR_PARAM,
		RHYTHM_2_NUMERATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_2_DENOMINATOR_PARAM,
		RHYTHM_2_DENOMINATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_3_NUMERATOR_PARAM,
		RHYTHM_3_NUMERATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_3_DENOMINATOR_PARAM,
		RHYTHM_3_DENOMINATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_4_NUMERATOR_PARAM,
		RHYTHM_4_NUMERATOR_CV_ATTUENUVERTER_PARAM,
		RHYTHM_4_DENOMINATOR_PARAM,
		RHYTHM_4_DENOMINATOR_CV_ATTUENUVERTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RHYTHM_1_NUMERATOR_CV_INPUT,
		RHYTHM_1_DENOMINATOR_CV_INPUT,
		RHYTHM_2_NUMERATOR_CV_INPUT,
		RHYTHM_2_DENOMINATOR_CV_INPUT,
		RHYTHM_3_NUMERATOR_CV_INPUT,
		RHYTHM_3_DENOMINATOR_CV_INPUT,
		RHYTHM_4_NUMERATOR_CV_INPUT,
		RHYTHM_4_DENOMINATOR_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		RHYTHM_1_OUTPUT,
		RHYTHM_2_OUTPUT,
		RHYTHM_3_OUTPUT,
		RHYTHM_4_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLOCK_LIGHT,
		RHYTHM_1_LIGHT,
		RHYTHM_2_LIGHT,
		RHYTHM_3_LIGHT,
		RHYTHM_4_LIGHT,
		NUM_LIGHTS
	};	

	dsp::SchmittTrigger clockTrigger;
	dsp::PulseGenerator rhythmPulse[NUM_RHYTHMS];
	float numerator[NUM_RHYTHMS] = {};
	float denominator[NUM_RHYTHMS] = {};
	float rhythmDuration[NUM_RHYTHMS] = {};
	float elapsedTime[NUM_RHYTHMS] = {};
	
	int division = 0;
	float time = 0.0;
	float duration = 0;
	bool secondClockReceived = false;


	


	HyperMeasures() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for(int i = 0;i<NUM_RHYTHMS;i++) {
			configParam(RHYTHM_1_NUMERATOR_PARAM + i*4, 1.0, 37.0, 4.0,"Numerator");
			configParam(RHYTHM_1_NUMERATOR_CV_ATTUENUVERTER_PARAM+ i*4, -1.0, 1.0, 0.0,"Numerator CV Attenuation","%",0,100);
			configParam(RHYTHM_1_DENOMINATOR_PARAM+ i*4, 1.0, 37.0, 4.0,"Denominator");
			configParam(RHYTHM_1_DENOMINATOR_CV_ATTUENUVERTER_PARAM+ i*4, -1.0, 1.0, 0.0,"Denominator CV Attenuation","%",0,100);
		}
	}
	void process(const ProcessArgs &args) override {

		time += 1.0 / args.sampleRate;
		if(inputs[CLOCK_INPUT].isConnected()) {
			if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				if(secondClockReceived) {
					duration = time;
				}
				time = 0;
				secondClockReceived = true;			
				//secondClockReceived = !secondClockReceived;			
			}
			lights[CLOCK_LIGHT].value = time > (duration/2.0);
		}
		
		for(int i = 0; i<NUM_RHYTHMS;i++) {
			numerator[i] = clamp(params[RHYTHM_1_NUMERATOR_PARAM+i*4].getValue() + (inputs[RHYTHM_1_NUMERATOR_CV_INPUT + i*2].isConnected() ? inputs[RHYTHM_1_NUMERATOR_CV_INPUT + i*2].getVoltage() * 3.7 * params[RHYTHM_1_NUMERATOR_CV_ATTUENUVERTER_PARAM + i*4].getValue() : 0.0f),1.0f,37.0f);
			denominator[i] = clamp(params[RHYTHM_1_DENOMINATOR_PARAM+i*4].getValue()+ (inputs[RHYTHM_1_DENOMINATOR_CV_INPUT + i*2].isConnected() ? inputs[RHYTHM_1_DENOMINATOR_CV_INPUT + i*2].getVoltage() * 3.7 * params[RHYTHM_1_DENOMINATOR_CV_ATTUENUVERTER_PARAM + i*4].getValue() : 0.0f),1.0f,37.0f);
			float rhythmLength = numerator[i]/denominator[i] * duration;
			elapsedTime[i] += 1.0/args.sampleRate;
			if(elapsedTime[i] >= rhythmLength && rhythmLength > 0) {
				rhythmPulse[i].trigger(1e-3);				
				elapsedTime[i] = 0.0;
			}
			outputs[RHYTHM_1_OUTPUT + i].setVoltage(rhythmPulse[i].process(1.0 / args.sampleRate) ? 10.0 : 0);	

		}




		// outputs[SIN_OUTPUT].setVoltage(sinOutputValue);
		// outputs[TRI_OUTPUT].setVoltage(triOutputValue);
		// outputs[SAW_OUTPUT].setVoltage(sawOutputValue);
		// outputs[SQR_OUTPUT].setVoltage( sqrOutputValue);
			
	}

	// void reset() override {
	// 	division = 0;
	// }

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


struct HyperMeasuresDisplay : TransparentWidget {
	HyperMeasures *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	HyperMeasuresDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}

	void drawParameter(const DrawArgs &args, Vec pos, float parameter) {
		nvgFontSize(args.vg, 18);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 2.0f", parameter);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		for(int i=0;i<NUM_RHYTHMS;i++) {
			drawParameter(args, Vec(50, 50 + i * 75), module->numerator[i]);
			drawParameter(args, Vec(120, 50 + i * 75), module->denominator[i]);
		}
	}
};

struct HyperMeasuresWidget : ModuleWidget {
	HyperMeasuresWidget(HyperMeasures *module) {

		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/HyperMeasures.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			HyperMeasuresDisplay *display = new HyperMeasuresDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 320);
			addChild(display);
		}

		for(int i = 0;i<NUM_RHYTHMS;i++) {
			addParam(createParam<RoundLargeFWSnapKnob>(Vec(15, 25 + (i*75)), module, HyperMeasures::RHYTHM_1_NUMERATOR_PARAM + i*4));
			addParam(createParam<RoundSmallFWKnob>(Vec(12, 68 + (i*75)), module, HyperMeasures::RHYTHM_1_NUMERATOR_CV_ATTUENUVERTER_PARAM + i*4));
			addParam(createParam<RoundLargeFWSnapKnob>(Vec(85, 25 + (i*75)), module, HyperMeasures::RHYTHM_1_DENOMINATOR_PARAM + i*4));
			addParam(createParam<RoundSmallFWKnob>(Vec(82, 68 + (i*75)), module, HyperMeasures::RHYTHM_1_DENOMINATOR_CV_ATTUENUVERTER_PARAM + i*4));
	
		
			addInput(createInput<PJ301MPort>(Vec(42, 66 + (i*75)), module, HyperMeasures::RHYTHM_1_NUMERATOR_CV_INPUT + i));
			addInput(createInput<PJ301MPort>(Vec(112, 66 + (i*75)), module, HyperMeasures::RHYTHM_1_DENOMINATOR_CV_INPUT + i));

			addOutput(createOutput<PJ301MPort>(Vec(150, 56 + (i*75)), module, HyperMeasures::RHYTHM_1_OUTPUT + i));

			//addChild(createLight<MediumLight<BlueLight>>(Vec(17, 156), module, HyperMeasures::RHYTHM_1_LIGHT + i));			
		}		

		addInput(createInput<PJ301MPort>(Vec(82, 340), module, HyperMeasures::CLOCK_INPUT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(112, 340), module, HyperMeasures::CLOCK_LIGHT));	
	}

};



Model *modelHyperMeasures = createModel<HyperMeasures, HyperMeasuresWidget>("HyperMeasures");
