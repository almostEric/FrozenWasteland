#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 12
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * 3 + TRACK_LEVEL_PARAM_COUNT

struct QARProbabilityExpander : Module {
	enum ParamIds {
		TRACK_1_PROBABILITY_ENABLED_PARAM,
		TRACK_2_PROBABILITY_ENABLED_PARAM,
		TRACK_3_PROBABILITY_ENABLED_PARAM,
		TRACK_4_PROBABILITY_ENABLED_PARAM,
		PROBABILITY_1_PARAM,
		PROBABILITY_ATTEN_1_PARAM = PROBABILITY_1_PARAM + MAX_STEPS,
		PROBABILITY_GROUP_MODE_1_PARAM = PROBABILITY_ATTEN_1_PARAM + MAX_STEPS, 
		STEP_OR_DIV_PARAM = PROBABILITY_GROUP_MODE_1_PARAM + MAX_STEPS,
		NUM_PARAMS
	};
	enum InputIds {
		PROBABILITY_1_INPUT,
		NUM_INPUTS = PROBABILITY_1_INPUT + MAX_STEPS
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
        USING_DIVS_LIGHT,
		PROBABILITY_GROUP_MODE_1_LIGHT,
		NUM_LIGHTS = PROBABILITY_GROUP_MODE_1_LIGHT + MAX_STEPS * 3
	};
	enum ProbabilityGroupTriggerModes {
		NONE_PGTM,
		GROUP_MODE_PGTM,
	};


	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

	
	dsp::SchmittTrigger stepDivTrigger,trackProbabilityTrigger[TRACK_COUNT],probabiltyGroupModeTrigger[MAX_STEPS];
	bool stepsOrDivs;
	bool trackProbabilitySelected[TRACK_COUNT];
	int probabilityGroupMode[MAX_STEPS] = {0};

	QARProbabilityExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		
		for(int i =0;i<TRACK_COUNT;i++) {
			configParam(TRACK_1_PROBABILITY_ENABLED_PARAM + i, 0.0, 1.0, 0.0);
			trackProbabilitySelected[i] = true;
		}

        for (int i = 0; i < MAX_STEPS; i++) {
            configParam(PROBABILITY_1_PARAM + i, -0.0f, 1.0f, 1.0f,"Step "  + std::to_string(i+1) + " Probability","%",0,100);
			configParam(PROBABILITY_ATTEN_1_PARAM + i, -1.0, 1.0, 0.0,"Step "  + std::to_string(i+1) + " Probability CV Attenuation","%",0,100);
			configParam(PROBABILITY_GROUP_MODE_1_PARAM + i, 0, 1.0, 0.0);
		}


		configParam(STEP_OR_DIV_PARAM, 0.0, 1.0, 0.0);
		
        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
        json_object_set_new(rootJ, "stepsOrDivs", json_integer((bool) stepsOrDivs));

		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackProbabilityActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackProbabilitySelected[i]));			
		}
		for(int i=0;i<MAX_STEPS;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "groupProbabilityMode");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) probabilityGroupMode[i]));			
		}
		
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

        json_t *sdJ = json_object_get(rootJ, "stepsOrDivs");
		if (sdJ)
			stepsOrDivs = json_integer_value(sdJ);

		for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "trackProbabilityActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				trackProbabilitySelected[i] = json_integer_value(sumJ);			
			}
		}	
		for(int i=0;i<MAX_STEPS;i++) {
			char buf[100];
			strcpy(buf, "groupProbabilityMode");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				probabilityGroupMode[i] = json_integer_value(sumJ);			
			}
		}	

	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackProbabilityTrigger[i].process(params[TRACK_1_PROBABILITY_ENABLED_PARAM+i].getValue())) {
				trackProbabilitySelected[i] = !trackProbabilitySelected[i];
			}
			lights[TRACK_1_PROBABILITY_ENABELED_LIGHT+i].value = trackProbabilitySelected[i];
		}
	    if (stepDivTrigger.process(params[STEP_OR_DIV_PARAM].getValue())) {
            stepsOrDivs = !stepsOrDivs;
        }
        lights[USING_DIVS_LIGHT].value = stepsOrDivs;

		for(int i=0; i< MAX_STEPS; i++) {
			if (probabiltyGroupModeTrigger[i].process(params[PROBABILITY_GROUP_MODE_1_PARAM+i].getValue())) {
				probabilityGroupMode[i] = (probabilityGroupMode[i] + 1) % 2;
			}
			lights[PROBABILITY_GROUP_MODE_1_LIGHT + i*3].value = probabilityGroupMode[i] == 2;
			lights[PROBABILITY_GROUP_MODE_1_LIGHT + i*3 + 1].value = 0;
			lights[PROBABILITY_GROUP_MODE_1_LIGHT + i*3 + 2].value = probabilityGroupMode[i] > 0;
		}
		

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWellFormedRhythmExpander || leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelPWAlgorithmicExpander));
		if (motherPresent) {
			// To Mother
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

			//Initalize
			for (int i = 0; i < PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT; i++) {
                messagesToMother[i] = 0.0;
			}

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARWellFormedRhythmExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                if(rightExpander.module->model != modelQuadAlgorithmicRhythm) { // Get QRE values							
					for(int i = 0; i < PASSTHROUGH_OFFSET; i++) {
                        messagesToMother[i] = messagesFromExpander[i];
					}
				}

				//QAR Pass through left
				for(int i = 0; i < PASSTHROUGH_LEFT_VARIABLE_COUNT; i++) {
					messagesToMother[PASSTHROUGH_OFFSET + i] = messagesFromExpander[PASSTHROUGH_OFFSET + i];
				}

				//QAR Pass through right
				for(int i = 0; i < PASSTHROUGH_RIGHT_VARIABLE_COUNT;i++) {
					messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + i] = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + i];
				}	

				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
		
			for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackProbabilitySelected[i]) {
                    messagesToMother[TRACK_COUNT * 3 + i] = stepsOrDivs ? 2 : 1;
    				for (int j = 0; j < MAX_STEPS; j++) {
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + i * MAX_STEPS + j] = clamp(params[PROBABILITY_1_PARAM+j].getValue() + (inputs[PROBABILITY_1_INPUT + j].isConnected() ? inputs[PROBABILITY_1_INPUT + j].getVoltage() / 10 * params[PROBABILITY_ATTEN_1_PARAM + j].getValue() : 0.0f),0.0,1.0f);
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT) + i * MAX_STEPS + j] = probabilityGroupMode[j];
					} 					 
				} 
			}			
			
			leftExpander.module->rightExpander.messageFlipRequested = true;
		
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
        stepsOrDivs = false;

		for(int i =0;i<TRACK_COUNT;i++) {
            params[PROBABILITY_1_PARAM+i].setValue(1);
			trackProbabilitySelected[i] = true;
		}

		// for(int i = 0; i < PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT; i++) {
		// 	producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0;
		// 	consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0;
		// }
	}
};

struct QARProbabilityExpanderWidget : ModuleWidget {
	QARProbabilityExpanderWidget(QARProbabilityExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARProbabilityExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		addParam(createParam<LEDButton>(Vec(12, 324), module, QARProbabilityExpander::STEP_OR_DIV_PARAM));

         for(int i=0; i<MAX_STEPS / 3;i++) {
		    addParam(createParam<RoundSmallFWKnob>(Vec(10, 30 + i*46), module, QARProbabilityExpander::PROBABILITY_1_PARAM + i));
		    addParam(createParam<RoundSmallFWKnob>(Vec(72, 30 + i*46), module, QARProbabilityExpander::PROBABILITY_1_PARAM + i + (MAX_STEPS / 3)));
		    addParam(createParam<RoundSmallFWKnob>(Vec(134, 30 + i*46), module, QARProbabilityExpander::PROBABILITY_1_PARAM + i + (MAX_STEPS * 2 / 3)));

		    addParam(createParam<RoundReallySmallFWKnob>(Vec(36, 50 + i*46), module, QARProbabilityExpander::PROBABILITY_ATTEN_1_PARAM + i));
		    addParam(createParam<RoundReallySmallFWKnob>(Vec(98, 50 + i*46), module, QARProbabilityExpander::PROBABILITY_ATTEN_1_PARAM + i + (MAX_STEPS / 3)));
		    addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 50 + i*46), module, QARProbabilityExpander::PROBABILITY_ATTEN_1_PARAM + i + (MAX_STEPS * 2 / 3)));
    
    		addInput(createInput<FWPortInReallySmall>(Vec(40, 33 + i*46), module, QARProbabilityExpander::PROBABILITY_1_INPUT + i));
    		addInput(createInput<FWPortInReallySmall>(Vec(102, 33 + i*46), module, QARProbabilityExpander::PROBABILITY_1_INPUT + i + (MAX_STEPS / 3)));
    		addInput(createInput<FWPortInReallySmall>(Vec(164, 33 + i*46), module, QARProbabilityExpander::PROBABILITY_1_INPUT + i + (MAX_STEPS * 2 / 3)));

			addParam(createParam<LEDButton>(Vec(12, 55 + i*46), module, QARProbabilityExpander::PROBABILITY_GROUP_MODE_1_PARAM + i));
		    addParam(createParam<LEDButton>(Vec(74, 55 + i*46), module, QARProbabilityExpander::PROBABILITY_GROUP_MODE_1_PARAM + i + (MAX_STEPS / 3)));
		    addParam(createParam<LEDButton>(Vec(136, 55 + i*46), module, QARProbabilityExpander::PROBABILITY_GROUP_MODE_1_PARAM + i + (MAX_STEPS * 2 / 3)));

			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(13.5, 56.5 + i*46), module, QARProbabilityExpander::PROBABILITY_GROUP_MODE_1_LIGHT + i*3));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(75.5, 56.5 + i*46), module, QARProbabilityExpander::PROBABILITY_GROUP_MODE_1_LIGHT + i*3 + MAX_STEPS));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(137.5, 56.5 + i*46), module, QARProbabilityExpander::PROBABILITY_GROUP_MODE_1_LIGHT + i*3 + (MAX_STEPS * 2)));
        }
	

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadRhythmExpander::CONNECTED_LIGHT));

        addChild(createLight<LargeLight<BlueLight>>(Vec(13.5, 325.5), module, QARProbabilityExpander::USING_DIVS_LIGHT));

		for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(76 + i*24, 320), module, QARProbabilityExpander::TRACK_1_PROBABILITY_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(77.5 + i*24, 321.5), module, QARProbabilityExpander::TRACK_1_PROBABILITY_ENABELED_LIGHT + i));

		}


	}
};

Model *modelQARProbabilityExpander = createModel<QARProbabilityExpander, QARProbabilityExpanderWidget>("QARProbabilityExpander");
