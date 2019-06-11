#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define PASSTHROUGH_VARIABLE_COUNT 21

struct QuadRhythmExpander : Module {
	enum ParamIds {
		TRACK_1_PROBABILITY_ENABLED_PARAM,
		TRACK_2_PROBABILITY_ENABLED_PARAM,
		TRACK_3_PROBABILITY_ENABLED_PARAM,
		TRACK_4_PROBABILITY_ENABLED_PARAM,
		PROBABILITY_1_PARAM,
		PROBABILITY_2_PARAM,
		PROBABILITY_3_PARAM,
		PROBABILITY_4_PARAM,
		PROBABILITY_ATTEN_1_PARAM,
		PROBABILITY_ATTEN_2_PARAM,
		PROBABILITY_ATTEN_3_PARAM,
		PROBABILITY_ATTEN_4_PARAM,
		TRACK_1_SWING_ENABLED_PARAM,
		TRACK_2_SWING_ENABLED_PARAM,
		TRACK_3_SWING_ENABLED_PARAM,
		TRACK_4_SWING_ENABLED_PARAM,
		SWING_1_PARAM,
		SWING_2_PARAM,
		SWING_3_PARAM,
		SWING_4_PARAM,
		SWING_ATTEN_1_PARAM,
		SWING_ATTEN_2_PARAM,
		SWING_ATTEN_3_PARAM,
		SWING_ATTEN_4_PARAM,
		STEPDIV_1,
		STEP_OR_DIV_PARAM = STEPDIV_1 + MAX_STEPS,
		NUM_PARAMS
	};
	enum InputIds {
		PROBABILITY_1_INPUT,
		PROBABILITY_2_INPUT,
		PROBABILITY_3_INPUT,
		PROBABILITY_4_INPUT,
		SWING_1_INPUT,
		SWING_2_INPUT,
		SWING_3_INPUT,
		SWING_4_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		CONNECTED_LIGHT,
		TRACK_1_PROBABILITY_ENABELED_LIGHT,
		TRACK_2_PROBABILITY_ENABELED_LIGHT,
		TRACK_3_PROBABILITY_ENABELED_LIGHT,
		TRACK_4_PROBABILITY_ENABELED_LIGHT,
		TRACK_1_SWING_ENABELED_LIGHT,
		TRACK_2_SWING_ENABELED_LIGHT,
		TRACK_3_SWING_ENABELED_LIGHT,
		TRACK_4_SWING_ENABELED_LIGHT,
		STEPDIV_1_LIGHT,
		NUM_LIGHTS = STEPDIV_1 + MAX_STEPS
	};


	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + PASSTHROUGH_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + PASSTHROUGH_VARIABLE_COUNT] = {};// mother will write into here

	
	dsp::SchmittTrigger stepDivTrigger[MAX_STEPS],trackProbabilityTrigger[TRACK_COUNT],trackSwingTrigger[TRACK_COUNT];
	bool stepDivSelected[MAX_STEPS];
	bool trackProbabilitySelected[TRACK_COUNT];
	bool trackSwingSelected[TRACK_COUNT];

	QuadRhythmExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(PROBABILITY_1_PARAM, 0.0, 1.0, 1.0,"Track 1 Probability","%",0,100);
		configParam(PROBABILITY_ATTEN_1_PARAM, -1.0, 1.0, 0.0,"Track 1 Probability CV Attenuation","%",0,100);

		configParam(PROBABILITY_2_PARAM, 0.0, 1.0, 1.0,"Track 2 Probability","%",0,100);
		configParam(PROBABILITY_ATTEN_2_PARAM, -1.0, 1.0, 0.0,"Track 2 Probability CV Attenuation","%",0,100);

		configParam(PROBABILITY_3_PARAM, 0.0, 1.0, 1.0,"Track 3 Probability","%",0,100);
		configParam(PROBABILITY_ATTEN_3_PARAM, -1.0, 1.0, 0.0,"Track 3 Probability CV Attenuation","%",0,100);

		configParam(PROBABILITY_4_PARAM, 0.0, 1.0, 1.0,"Track 4 Probability","%",0,100);
		configParam(PROBABILITY_ATTEN_4_PARAM, -1.0, 1.0, 0.0,"Track 4 Probability CV Attenuation","%",0,100);

		configParam(SWING_1_PARAM, -0.5, 0.5, 0.0, "Track 1 Swing","%",0,100);
		configParam(SWING_ATTEN_1_PARAM, -1.0, 1.0, 0.0,"Track 1 Swing CV Attenuation","%",0,100);

		configParam(SWING_2_PARAM, -0.5, 0.5, 0.0, "Track 2 Swing","%",0,100);
		configParam(SWING_ATTEN_2_PARAM, -1.0, 1.0, 0.0,"Track 2 Swing CV Attenuation","%",0,100);

		configParam(SWING_3_PARAM, -0.5, 0.5, 0.0, "Track 3 Swing","%",0,100);
		configParam(SWING_ATTEN_3_PARAM, -1.0, 1.0, 0.0,"Track 3 Swing CV Attenuation","%",0,100);

		configParam(SWING_4_PARAM, -0.5, 0.5, 0.0, "Track 4 Swing","%",0,100);
		configParam(SWING_ATTEN_4_PARAM, -1.0, 1.0, 0.0,"Track 4 Swing CV Attenuation","%",0,100);

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;

		for(int i =0;i<MAX_STEPS;i++) {
			configParam(STEPDIV_1 + i, 0.0, 1.0, 0.0);
			stepDivSelected[i] = false;
		}

		for(int i =0;i<TRACK_COUNT;i++) {
			configParam(TRACK_1_PROBABILITY_ENABLED_PARAM + i, 0.0, 1.0, 0.0);
			configParam(TRACK_1_SWING_ENABLED_PARAM + i, 0.0, 1.0, 0.0);
			trackProbabilitySelected[i] = true;
			trackSwingSelected[i] = true;
		}

		configParam(STEP_OR_DIV_PARAM, 0.0, 1.0, 1.0);
		
        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		for(int i=0;i<MAX_STEPS;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "stepDivActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) stepDivSelected[i]));			
		}
		

		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackProbabilityActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackProbabilitySelected[i]));			
		}

		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackSwingActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackSwingSelected[i]));			
		}
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		
		char buf[100];			
		for(int i=0;i<MAX_STEPS;i++) {
			strcpy(buf, "stepDivActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				stepDivSelected[i] = json_integer_value(sumJ);			
			}
		}	

		for(int i=0;i<TRACK_COUNT;i++) {
			strcpy(buf, "trackProbabilityActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				trackProbabilitySelected[i] = json_integer_value(sumJ);			
			}
		}	

		for(int i=0;i<TRACK_COUNT;i++) {
			strcpy(buf, "trackSwingActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				trackSwingSelected[i] = json_integer_value(sumJ);			
			}
		}
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackProbabilityTrigger[i].process(params[TRACK_1_PROBABILITY_ENABLED_PARAM+i].getValue())) {
				trackProbabilitySelected[i] = !trackProbabilitySelected[i];
			}
			if (trackSwingTrigger[i].process(params[TRACK_1_SWING_ENABLED_PARAM+i].getValue())) {
				trackSwingSelected[i] = !trackSwingSelected[i];
			}
			lights[TRACK_1_PROBABILITY_ENABELED_LIGHT+i].value = trackProbabilitySelected[i];
			lights[TRACK_1_SWING_ENABELED_LIGHT+i].value = trackSwingSelected[i];
		}

		for(int i=0; i< MAX_STEPS; i++) {
			if (stepDivTrigger[i].process(params[STEPDIV_1+i].getValue())) {
				stepDivSelected[i] = !stepDivSelected[i];
			}
			lights[STEPDIV_1_LIGHT+i].value = stepDivSelected[i];
		}		

		
	

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQuadRhythmExpander));
		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			// To Mother
			float *producerMessage = reinterpret_cast<float*>(leftExpander.producerMessage);
			producerMessage[0] = params[STEP_OR_DIV_PARAM].getValue();

			//Initalize
			for (int i = 0; i < TRACK_COUNT; i++) {
				for (int j = 0; j < MAX_STEPS; j++) {
					producerMessage[1 + i * MAX_STEPS + j] = 1.0;
					producerMessage[1 + (i + TRACK_COUNT) * MAX_STEPS + j] = 0.0;
				}
			}

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQuadRhythmExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *message = (float*) rightExpander.module->leftExpander.consumerMessage;								
				for(int i = 0; i < TRACK_COUNT; i++) {
					for(int j = 0; j < MAX_STEPS; j++) { // Assign probabilites and swing
						producerMessage[1 + i * MAX_STEPS + j] = message[1 + i * MAX_STEPS + j];
						producerMessage[1 + (i + TRACK_COUNT) * MAX_STEPS + j] = message[1 + (i + TRACK_COUNT) * MAX_STEPS  + j];
					} 
				}


				bool slaveQARsPresent = rightExpander.module->model == modelQuadAlgorithmicRhythm;				
				//QAR Pass through
				for(int i = 0; i < PASSTHROUGH_VARIABLE_COUNT; i++) {
					producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = message[MAX_STEPS * TRACK_COUNT * 2 + 1 + i];
				}
				//Pass through if there are QARs somewhere
				producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + TRACK_COUNT * 4 + 3] = producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + TRACK_COUNT * 4 + 3] || slaveQARsPresent; 
			}
		
			for (int i = 0; i < TRACK_COUNT; i++) {
				for (int j = 0; j < MAX_STEPS; j++) {
					if(trackProbabilitySelected[i] && stepDivSelected[j]) {
						producerMessage[1 + i * MAX_STEPS + j] = clamp(params[PROBABILITY_1_PARAM+i].getValue() + (inputs[PROBABILITY_1_INPUT + i].isConnected() ? inputs[PROBABILITY_1_INPUT + i].getVoltage() / 10 * params[PROBABILITY_ATTEN_1_PARAM + i].getValue() : 0.0f),0.0,1.0f);
					} 
					if(trackSwingSelected[i] && stepDivSelected[j]) {
						producerMessage[1 + (i + TRACK_COUNT) * MAX_STEPS + j] = clamp(params[SWING_1_PARAM+i].getValue() + (inputs[SWING_1_INPUT + i].isConnected() ? inputs[SWING_1_INPUT + i].getVoltage() / 10 * params[SWING_ATTEN_1_PARAM + i].getValue() : 0.0f),-1.0,1.0);
					} 
				}
			}			
						
			// From Mother	
			bool masterQARsPresent = leftExpander.module->model == modelQuadRhythmExpander;
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			for(int i = 0; i < PASSTHROUGH_VARIABLE_COUNT;i++) {
				consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = messagesFromMother[MAX_STEPS * TRACK_COUNT * 2 + 1 + i];
			}	
			consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + TRACK_COUNT * 4 + 4] = consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + TRACK_COUNT * 4 + 4] || masterQARsPresent; 

			leftExpander.messageFlipRequested = true;
		
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
		for(int i =0;i<MAX_STEPS;i++) {			
			stepDivSelected[i] = false;
		}

		for(int i =0;i<TRACK_COUNT;i++) {
            params[PROBABILITY_1_PARAM+i].setValue(1);
            params[SWING_1_PARAM+i].setValue(0);
			trackProbabilitySelected[i] = true;
			trackSwingSelected[i] = true;
		}
	}
};

struct QuadRhythmExpanderWidget : ModuleWidget {
	QuadRhythmExpanderWidget(QuadRhythmExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadRhythmExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	
		addParam(createParam<RoundFWKnob>(Vec(22, 41), module, QuadRhythmExpander::PROBABILITY_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 71), module, QuadRhythmExpander::PROBABILITY_ATTEN_1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(22, 98), module, QuadRhythmExpander::PROBABILITY_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 128), module, QuadRhythmExpander::PROBABILITY_ATTEN_2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(22, 155), module, QuadRhythmExpander::PROBABILITY_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 185), module, QuadRhythmExpander::PROBABILITY_ATTEN_3_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(22, 212), module, QuadRhythmExpander::PROBABILITY_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 242), module, QuadRhythmExpander::PROBABILITY_ATTEN_4_PARAM));

		addParam(createParam<RoundFWKnob>(Vec(122, 41), module, QuadRhythmExpander::SWING_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(161, 71), module, QuadRhythmExpander::SWING_ATTEN_1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(122, 98), module, QuadRhythmExpander::SWING_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(161, 128), module, QuadRhythmExpander::SWING_ATTEN_2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(122, 155), module, QuadRhythmExpander::SWING_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(161, 185), module, QuadRhythmExpander::SWING_ATTEN_3_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(122, 212), module, QuadRhythmExpander::SWING_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(161, 242), module, QuadRhythmExpander::SWING_ATTEN_4_PARAM));

		addParam(createParam<CKSS>(Vec(12, 277), module, QuadRhythmExpander::STEP_OR_DIV_PARAM));

		addInput(createInput<PJ301MPort>(Vec(60, 44), module, QuadRhythmExpander::PROBABILITY_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(60, 101), module, QuadRhythmExpander::PROBABILITY_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(60, 158), module, QuadRhythmExpander::PROBABILITY_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(60, 215), module, QuadRhythmExpander::PROBABILITY_4_INPUT));

		addInput(createInput<PJ301MPort>(Vec(160, 44), module, QuadRhythmExpander::SWING_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(160, 101), module, QuadRhythmExpander::SWING_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(160, 158), module, QuadRhythmExpander::SWING_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(160, 215), module, QuadRhythmExpander::SWING_4_INPUT));


		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadRhythmExpander::CONNECTED_LIGHT));

		for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(26, 73 + i*57), module, QuadRhythmExpander::TRACK_1_PROBABILITY_ENABLED_PARAM + i));
			addChild(createLight<MediumLight<BlueLight>>(Vec(30, 77 + i*57), module, QuadRhythmExpander::TRACK_1_PROBABILITY_ENABELED_LIGHT + i));

			addParam(createParam<LEDButton>(Vec(126, 73 + i*57), module, QuadRhythmExpander::TRACK_1_SWING_ENABLED_PARAM + i));
			addChild(createLight<MediumLight<BlueLight>>(Vec(130	, 77 + i*57), module, QuadRhythmExpander::TRACK_1_SWING_ENABELED_LIGHT + i));
		}

		for(int i=0;i<MAX_STEPS / 2; i++) {
			addParam(createParam<LEDButton>(Vec(12 + (i*21), 310), module, QuadRhythmExpander::STEPDIV_1 + i));
			addChild(createLight<MediumLight<BlueLight>>(Vec(16 + (i*21), 314), module, QuadRhythmExpander::STEPDIV_1_LIGHT + i));

			addParam(createParam<LEDButton>(Vec(12 + (i*21), 332), module, QuadRhythmExpander::STEPDIV_1 + i + (MAX_STEPS / 2)));
			addChild(createLight<MediumLight<BlueLight>>(Vec(16 + (i*21), 336), module, QuadRhythmExpander::STEPDIV_1_LIGHT + i + (MAX_STEPS / 2)));

		}

	}
};

Model *modelQuadRhythmExpander = createModel<QuadRhythmExpander, QuadRhythmExpanderWidget>("QuadRhythmExpander");
