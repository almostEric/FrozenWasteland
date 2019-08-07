#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define FORMANT_COUNT 5

struct VoxInhumanaExpander : Module {
	enum ParamIds {
		Q_1_PARAM,
		Q_2_PARAM,
		Q_3_PARAM,
		Q_4_PARAM,
		Q_5_PARAM,
        Q_1_ATTENUVERTER_PARAM,
        Q_2_ATTENUVERTER_PARAM,
        Q_3_ATTENUVERTER_PARAM,
        Q_4_ATTENUVERTER_PARAM,
        Q_5_ATTENUVERTER_PARAM,
		SLOPE_1_PARAM,
		SLOPE_2_PARAM,
		SLOPE_3_PARAM,
		SLOPE_4_PARAM,
		SLOPE_5_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		Q_1_INPUT,
		Q_2_INPUT,
		Q_3_INPUT,
		Q_4_INPUT,
        Q_5_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
        FORMANT_1_SLOPE_LIGHT,
        FORMANT_2_SLOPE_LIGHT,
        FORMANT_3_SLOPE_LIGHT,
        FORMANT_4_SLOPE_LIGHT,
        FORMANT_5_SLOPE_LIGHT,
		NUM_LIGHTS
	};


	const char* formantNames[FORMANT_COUNT] {"1","2","3","4","5"};

	// Expander
	float consumerMessage[FORMANT_COUNT * 2] = {};// this module must read from here
	float producerMessage[FORMANT_COUNT * 2] = {};// mother will write into here

	
	dsp::SchmittTrigger slopeTrigger[FORMANT_COUNT];
	bool twelveDbSlopeSelected[FORMANT_COUNT];

	VoxInhumanaExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
        configParam(Q_1_PARAM, -30.0, 100.0, 0,"Formant 1 Resonance");
		configParam(Q_2_PARAM, -30.0, 100.0, 0,"Formant 2 Resonance");
		configParam(Q_3_PARAM, -30.0, 100.0, 0,"Formant 3 Resonance");
		configParam(Q_4_PARAM, -30.0, 100.0, 0,"Formant 4 Resonance");
		configParam(Q_5_PARAM, -30.0, 100.0, 0,"Formant 5 Resonance");
		configParam(Q_1_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Formant 1 Resonance CV Attenuation","%",0,100);
		configParam(Q_2_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Formant 2 Resonance CV Attenuation","%",0,100);
		configParam(Q_3_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Formant 3 Resonance CV Attenuation","%",0,100);
		configParam(Q_4_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Formant 4 Resonance CV Attenuation","%",0,100);
		configParam(Q_5_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Formant 5 Resonance CV Attenuation","%",0,100);
        configParam(SLOPE_1_PARAM, 0, 1, 0,"Formant 1 6/12db Slope");
        configParam(SLOPE_2_PARAM, 0, 1, 0,"Formant 2 6/12db Slope");
        configParam(SLOPE_3_PARAM, 0, 1, 0,"Formant 3 6/12db Slope");
        configParam(SLOPE_4_PARAM, 0, 1, 0,"Formant 4 6/12db Slope");
        configParam(SLOPE_5_PARAM, 0, 1, 0,"Formant 5 6/12db Slope");


		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;

		for(int i =0;i<FORMANT_COUNT;i++) {
			twelveDbSlopeSelected[i] = false;
		}

        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		for(int i=0;i<FORMANT_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "twelveDbSlope");
			strcat(buf, formantNames[i]);
			json_object_set_new(rootJ, buf, json_integer((bool) twelveDbSlopeSelected[i]));			
		}
				
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		
		char buf[100];			
		for(int i=0;i<FORMANT_COUNT;i++) {
			strcpy(buf, "twelveDbSlope");
			strcat(buf, formantNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				twelveDbSlopeSelected[i] = json_integer_value(sumJ);			
			}
		}	
		
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i< FORMANT_COUNT; i++) {
			if (slopeTrigger[i].process(params[SLOPE_1_PARAM+i].getValue())) {
				twelveDbSlopeSelected[i] = !twelveDbSlopeSelected[i];
			}
			
			lights[FORMANT_1_SLOPE_LIGHT+i].value = twelveDbSlopeSelected[i];			
		}

		bool motherPresent = (leftExpander.module && leftExpander.module->model == modelVoxInhumana);
		
		if (motherPresent) {
			// To Mother
			float *producerMessage = (float*) leftExpander.producerMessage;
			
			
			for (int i = 0; i < FORMANT_COUNT; i++) {
                producerMessage[i * 2] = clamp(params[Q_1_PARAM+i].getValue() + (inputs[Q_1_INPUT + i].isConnected() ? inputs[Q_1_INPUT + i].getVoltage() * 10 * params[Q_2_ATTENUVERTER_PARAM + i].getValue() : 0.0f),1.0,100.0f);
                producerMessage[i * 2 + 1] = twelveDbSlopeSelected[i];                 
			}			
						
			// From Mother	
			
			
			leftExpander.messageFlipRequested = true;
		
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
		for(int i =0;i<FORMANT_COUNT;i++) {			
			twelveDbSlopeSelected[i] = false;
		}
	}
};

struct VoxInhumanaExpanderWidget : ModuleWidget {
	VoxInhumanaExpanderWidget(VoxInhumanaExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VoxInhumanaExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	
		addParam(createParam<RoundFWKnob>(Vec(10, 160), module, VoxInhumanaExpander::Q_1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(10, 195), module, VoxInhumanaExpander::Q_2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(10, 230), module, VoxInhumanaExpander::Q_3_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(10, 265), module, VoxInhumanaExpander::Q_4_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(10, 300), module, VoxInhumanaExpander::Q_5_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 162), module, VoxInhumanaExpander::Q_1_ATTENUVERTER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 197), module, VoxInhumanaExpander::Q_2_ATTENUVERTER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 232), module, VoxInhumanaExpander::Q_3_ATTENUVERTER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 267), module, VoxInhumanaExpander::Q_4_ATTENUVERTER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 302), module, VoxInhumanaExpander::Q_5_ATTENUVERTER_PARAM));

		addParam(createParam<LEDButton>(Vec(48, 40), module, VoxInhumanaExpander::SLOPE_1_PARAM));
		addParam(createParam<LEDButton>(Vec(48, 60), module, VoxInhumanaExpander::SLOPE_2_PARAM));
		addParam(createParam<LEDButton>(Vec(48, 80), module, VoxInhumanaExpander::SLOPE_3_PARAM));
		addParam(createParam<LEDButton>(Vec(48, 100), module, VoxInhumanaExpander::SLOPE_4_PARAM));
		addParam(createParam<LEDButton>(Vec(48, 120), module, VoxInhumanaExpander::SLOPE_5_PARAM));

        addInput(createInput<PJ301MPort>(Vec(45, 162), module, VoxInhumanaExpander::Q_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 197), module, VoxInhumanaExpander::Q_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 232), module, VoxInhumanaExpander::Q_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 267), module, VoxInhumanaExpander::Q_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 302), module, VoxInhumanaExpander::Q_5_INPUT));

		addChild(createLight<LargeLight<BlueLight>>(Vec(49.5, 41.5), module, VoxInhumanaExpander::FORMANT_1_SLOPE_LIGHT));		
		addChild(createLight<LargeLight<BlueLight>>(Vec(49.5, 61.5), module, VoxInhumanaExpander::FORMANT_2_SLOPE_LIGHT));		
		addChild(createLight<LargeLight<BlueLight>>(Vec(49.5, 81.5), module, VoxInhumanaExpander::FORMANT_3_SLOPE_LIGHT));		
		addChild(createLight<LargeLight<BlueLight>>(Vec(49.5, 101.5), module, VoxInhumanaExpander::FORMANT_4_SLOPE_LIGHT));		
		addChild(createLight<LargeLight<BlueLight>>(Vec(49.5, 121.5), module, VoxInhumanaExpander::FORMANT_5_SLOPE_LIGHT));		
	
	}
};

Model *modelVoxInhumanaExpander = createModel<VoxInhumanaExpander, VoxInhumanaExpanderWidget>("VoxInhumanaExpander");
