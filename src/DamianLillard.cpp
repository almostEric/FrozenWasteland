#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "StateVariableFilter.h"

using namespace std;

#define BANDS 4
#define FREQUENCIES 3
#define numFilters 8

struct DamianLillard : Module {
	typedef float T;

	enum ParamIds {
		FREQ_1_CUTOFF_PARAM,
		FREQ_2_CUTOFF_PARAM,
		FREQ_3_CUTOFF_PARAM,
		FREQ_1_CV_ATTENUVERTER_PARAM,
		FREQ_2_CV_ATTENUVERTER_PARAM,
		FREQ_3_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SIGNAL_IN,
		FREQ_1_CUTOFF_INPUT,		
		FREQ_2_CUTOFF_INPUT,		
		FREQ_3_CUTOFF_INPUT,		
		BAND_1_RETURN_INPUT,		
		BAND_2_RETURN_INPUT,
		BAND_3_RETURN_INPUT,
		BAND_4_RETURN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		BAND_1_OUTPUT,
		BAND_2_OUTPUT,
		BAND_3_OUTPUT,
		BAND_4_OUTPUT,
		MIX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LEARN_LIGHT,
		NUM_LIGHTS
	};
	float freq[FREQUENCIES] = {0};
	float lastFreq[FREQUENCIES] = {0};
	float output[BANDS] = {0};

    StateVariableFilterState<T> filterStates[numFilters];
    StateVariableFilterParams<T> filterParams[numFilters];


	int bandOffset = 0;

	DamianLillard() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(FREQ_1_CUTOFF_PARAM, 0, 0.5, .25,"Cutoff Frequency 1","Hz",560,15);
		configParam(FREQ_2_CUTOFF_PARAM, 0.25, 0.75, .5,"Cutoff Frequency 2","Hz",560,15);
		configParam(FREQ_3_CUTOFF_PARAM, 0.5, 1.0, .75,"Cutoff Frequency 3","Hz",560,15);
		configParam(FREQ_1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Cutoff Frequency 1 CV Attenuation","%",0,100);
		configParam(FREQ_2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Cutoff Frequency 2 CV Attenuation","%",0,100);
		configParam(FREQ_3_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Cutoff Frequency 3 CV Attenuation","%",0,100);

		filterParams[0].setMode(StateVariableFilterParams<T>::Mode::LowPass);
		filterParams[1].setMode(StateVariableFilterParams<T>::Mode::LowPass);
		filterParams[2].setMode(StateVariableFilterParams<T>::Mode::HiPass);
		filterParams[3].setMode(StateVariableFilterParams<T>::Mode::LowPass);
		filterParams[4].setMode(StateVariableFilterParams<T>::Mode::HiPass);
		filterParams[5].setMode(StateVariableFilterParams<T>::Mode::LowPass);
		filterParams[6].setMode(StateVariableFilterParams<T>::Mode::HiPass);
		filterParams[7].setMode(StateVariableFilterParams<T>::Mode::HiPass);

		for (int i = 0; i < numFilters; ++i) {
	        filterParams[i].setQ(0.5); 	
	        filterParams[i].setFreq(T(.1));
	    }
	}

	void reConfigParam (int paramId, float minValue, float maxValue, float defaultValue) {
		ParamQuantity *pq = paramQuantities[paramId];
		pq->minValue = minValue;
		pq->maxValue = maxValue;
		pq->defaultValue = defaultValue;		
	}


	void process(const ProcessArgs &args) override {
		
		float signalIn = inputs[SIGNAL_IN].getVoltage();
		float out = 0.0;

		const float minCutoff = 15.0;
		const float maxCutoff = 8400.0;

		reConfigParam(FREQ_1_CUTOFF_PARAM, 0, params[FREQ_1_CUTOFF_PARAM+1].getValue(), .25);
		reConfigParam(FREQ_2_CUTOFF_PARAM, params[FREQ_1_CUTOFF_PARAM+0].getValue(), params[FREQ_1_CUTOFF_PARAM+2].getValue(), .5);
		reConfigParam(FREQ_3_CUTOFF_PARAM, params[FREQ_1_CUTOFF_PARAM+1].getValue(), 1.0, .75);

		
		for (int i=0; i<FREQUENCIES;i++) {
			float cutoffExp = params[FREQ_1_CUTOFF_PARAM+i].getValue() + inputs[FREQ_1_CUTOFF_INPUT+i].getVoltage() * params[FREQ_1_CV_ATTENUVERTER_PARAM+i].getValue() / 10.0f; //I'm reducing range of CV to make it more useful
			cutoffExp = clamp(cutoffExp, 0.0f, 1.0f);
			freq[i] = minCutoff * powf(maxCutoff / minCutoff, cutoffExp);

			//Prevent band overlap
			if(i > 0 && freq[i] < lastFreq[i-1]) {
				freq[i] = lastFreq[i-1]+1;
			}
			if(i<FREQUENCIES-1 && freq[i] > lastFreq[i+1]) {
				freq[i] = lastFreq[i+1]-1;
			}

			if(freq[i] != lastFreq[i]) {
				float Fc = freq[i] / args.sampleRate;
				if(i==0) 
					filterParams[0].setFreq(T(Fc));
				if(i==2) 
					filterParams[7].setFreq(T(Fc));
				filterParams[i*2 + 1].setFreq(T(Fc));
				filterParams[i*2 + 2].setFreq(T(Fc));
				lastFreq[i] = freq[i];
			}
		}

		output[0] = StateVariableFilter<T>::run(StateVariableFilter<T>::run(signalIn, filterStates[0], filterParams[0]), filterStates[1], filterParams[1]);
		output[1] = StateVariableFilter<T>::run(StateVariableFilter<T>::run(signalIn, filterStates[2], filterParams[2]), filterStates[3], filterParams[3]);
		output[2] = StateVariableFilter<T>::run(StateVariableFilter<T>::run(signalIn, filterStates[4], filterParams[4]), filterStates[5], filterParams[5]);
		output[3] = StateVariableFilter<T>::run(StateVariableFilter<T>::run(signalIn, filterStates[6], filterParams[6]), filterStates[7], filterParams[7]);

		for(int i=0; i<BANDS; i++) {		
			outputs[BAND_1_OUTPUT+i].setVoltage(output[i]);

			if(inputs[BAND_1_RETURN_INPUT+i].isConnected()) {
				out += inputs[BAND_1_RETURN_INPUT+i].getVoltage();
			} else {
				out += output[i];
			}
		}

		outputs[MIX_OUTPUT].setVoltage(out / 2.0); 
		
	}


};

	

struct DamianLillardBandDisplay : TransparentWidget {
	DamianLillard *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	DamianLillardBandDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

	void drawFrequency(const DrawArgs &args, Vec pos, float cutoffFrequency) {
		nvgFontSize(args.vg, 13);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 4.0f", cutoffFrequency);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x + 36, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

		for(int i=0;i<FREQUENCIES;i++) {
			drawFrequency(args, Vec(i * 50.0, box.size.y - 77), module->freq[i]);
		}
	}
};

struct DamianLillardWidget : ModuleWidget {
	DamianLillardWidget(DamianLillard *module) {

		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DamianLillard.svg")));
		
		{
			DamianLillardBandDisplay *offsetDisplay = new DamianLillardBandDisplay();
			offsetDisplay->module = module;
			offsetDisplay->box.pos = Vec(15, 10);
			offsetDisplay->box.size = Vec(box.size.x, 140);
			addChild(offsetDisplay);
		}

		addParam(createParam<RoundFWKnob>(Vec(18, 84), module, DamianLillard::FREQ_1_CUTOFF_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(69, 84), module, DamianLillard::FREQ_2_CUTOFF_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(120, 84), module, DamianLillard::FREQ_3_CUTOFF_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(21, 146), module, DamianLillard::FREQ_1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(72, 146), module, DamianLillard::FREQ_2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(123, 146), module, DamianLillard::FREQ_3_CV_ATTENUVERTER_PARAM));



		addInput(createInput<PJ301MPort>(Vec(20, 117), module, DamianLillard::FREQ_1_CUTOFF_INPUT));
		addInput(createInput<PJ301MPort>(Vec(71, 117), module, DamianLillard::FREQ_2_CUTOFF_INPUT));
		addInput(createInput<PJ301MPort>(Vec(122, 117), module, DamianLillard::FREQ_3_CUTOFF_INPUT));

		addInput(createInput<PJ301MPort>(Vec(30, 330), module, DamianLillard::SIGNAL_IN));


		addInput(createInput<PJ301MPort>(Vec(10, 255), module, DamianLillard::BAND_1_RETURN_INPUT));
		addInput(createInput<PJ301MPort>(Vec(50, 255), module, DamianLillard::BAND_2_RETURN_INPUT));
		addInput(createInput<PJ301MPort>(Vec(90, 255), module, DamianLillard::BAND_3_RETURN_INPUT));
		addInput(createInput<PJ301MPort>(Vec(130, 255), module, DamianLillard::BAND_4_RETURN_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(10, 215), module, DamianLillard::BAND_1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(50, 215), module, DamianLillard::BAND_2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(90, 215), module, DamianLillard::BAND_3_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(130, 215), module, DamianLillard::BAND_4_OUTPUT));

		addOutput(createOutput<PJ301MPort>(Vec(109, 330), module, DamianLillard::MIX_OUTPUT));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};


Model *modelDamianLillard = createModel<DamianLillard, DamianLillardWidget>("DamianLillard");
