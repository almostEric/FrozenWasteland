#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 18
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT

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

	bool trackDirty[TRACK_COUNT] = {0};

	
	dsp::SchmittTrigger stepDivTrigger,trackProbabilityTrigger[TRACK_COUNT],probabiltyGroupModeTrigger[MAX_STEPS];
	bool stepsOrDivs;
	bool trackProbabilitySelected[TRACK_COUNT];
	int probabilityGroupMode[MAX_STEPS] = {0};

	float lastProbability[MAX_STEPS] = {0};

	float sceneData[NBR_SCENES][59] = {{0}};
	int sceneChangeMessage = 0;

	//percentages
	float stepProbabilityPercentage[MAX_STEPS] = {0};

	bool isDirty = false;
	bool QARExpanderDisconnectReset = true;

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
		
		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<59;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_object_set_new(rootJ, buf.c_str(), json_real(sceneData[scene][i]));
			}
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

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<59;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_t *sdJ = json_object_get(rootJ, buf.c_str());
				if (json_real_value(sdJ)) {
					sceneData[scene][i] = json_real_value(sdJ);
				}
			}
		}
	}

	void saveScene(int scene) {
		sceneData[scene][0] = stepsOrDivs;
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			sceneData[scene][trackNumber+1] = trackProbabilitySelected[trackNumber];
		}
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			sceneData[scene][stepNumber+5] = params[PROBABILITY_1_PARAM+stepNumber].getValue();
			sceneData[scene][stepNumber+23] = params[PROBABILITY_ATTEN_1_PARAM+stepNumber].getValue();
			sceneData[scene][stepNumber+41] = params[PROBABILITY_GROUP_MODE_1_PARAM+stepNumber].getValue();
		}
	}

	void loadScene(int scene) {
		stepsOrDivs = sceneData[scene][0];
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			trackProbabilitySelected[trackNumber] = sceneData[scene][trackNumber+1];
		}
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			 params[PROBABILITY_1_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+5]);
			 params[PROBABILITY_ATTEN_1_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+23]);
			 params[PROBABILITY_GROUP_MODE_1_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+41]);
		}
	}


	void process(const ProcessArgs &args) override {
		isDirty = false;
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackProbabilityTrigger[i].process(params[TRACK_1_PROBABILITY_ENABLED_PARAM+i].getValue())) {
				trackProbabilitySelected[i] = !trackProbabilitySelected[i];
				isDirty = true;
			}
			lights[TRACK_1_PROBABILITY_ENABELED_LIGHT+i].value = trackProbabilitySelected[i];
		}
	    if (stepDivTrigger.process(params[STEP_OR_DIV_PARAM].getValue())) {
            stepsOrDivs = !stepsOrDivs;
			isDirty = true;
        }
        lights[USING_DIVS_LIGHT].value = stepsOrDivs;

		for(int i=0; i< MAX_STEPS; i++) {
			if (probabiltyGroupModeTrigger[i].process(params[PROBABILITY_GROUP_MODE_1_PARAM+i].getValue())) {
				probabilityGroupMode[i] = (probabilityGroupMode[i] + 1) % 2;
				isDirty = true;
			}
			lights[PROBABILITY_GROUP_MODE_1_LIGHT + i*3].value = probabilityGroupMode[i] == 2;
			lights[PROBABILITY_GROUP_MODE_1_LIGHT + i*3 + 1].value = 0;
			lights[PROBABILITY_GROUP_MODE_1_LIGHT + i*3 + 2].value = probabilityGroupMode[i] > 0;
		}
		

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWellFormedRhythmExpander || 
								leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || 
								leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelQARIrrationalityExpander ||
								leftExpander.module->model == modelQARConditionalExpander || 
								leftExpander.module->model == modelPWAlgorithmicExpander));
		if (motherPresent) {
			// To Mother
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

			sceneChangeMessage = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT+ 0];
			if(sceneChangeMessage >= 20) {
				saveScene(sceneChangeMessage-20);
			} else if (sceneChangeMessage >=10) {
				loadScene(sceneChangeMessage-10);
			}


			//Initalize
			std::fill(messagesToMother, messagesToMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT, 0.0);

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARWellFormedRhythmExpander || rightExpander.module->model == modelQARGrooveExpander || 
											rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARIrrationalityExpander || 
											rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm ||
											rightExpander.module->model == modelQARConditionalExpander));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                if(rightExpander.module->model != modelQuadAlgorithmicRhythm) { // Get QRE values		
					std::copy(messagesFromExpander,messagesFromExpander + PASSTHROUGH_OFFSET,messagesToMother);		
				}
				for(int i=0;i<TRACK_COUNT;i++) {
					trackDirty[i] = messagesFromExpander[i] || (!QARExpanderDisconnectReset);
				}

				QARExpanderDisconnectReset = true;

				//QAR Pass through left
				std::copy(messagesFromExpander+PASSTHROUGH_OFFSET,messagesFromExpander+PASSTHROUGH_OFFSET+PASSTHROUGH_LEFT_VARIABLE_COUNT,messagesToMother+PASSTHROUGH_OFFSET);		

				//QAR Pass through right
				std::copy(messagesFromMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT,messagesFromMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT,
									messageToExpander+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT);			

				rightExpander.module->leftExpander.messageFlipRequested = true;
			} else {
				std::fill(trackDirty,trackDirty+TRACK_COUNT,0);
				isDirty = QARExpanderDisconnectReset;
				QARExpanderDisconnectReset = false;
			}
		
			for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackProbabilitySelected[i]) {
                    messagesToMother[TRACK_COUNT * 4 + i] = stepsOrDivs ? 2 : 1;
    				for (int j = 0; j < MAX_STEPS; j++) {
						float probability = clamp(params[PROBABILITY_1_PARAM+j].getValue() + (inputs[PROBABILITY_1_INPUT + j].isConnected() ? inputs[PROBABILITY_1_INPUT + j].getVoltage() / 10 * params[PROBABILITY_ATTEN_1_PARAM + j].getValue() : 0.0f),0.0,1.0f);
						if(probability != lastProbability[j]) {
							isDirty = true;
						}
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + i * MAX_STEPS + j] = probability;
						stepProbabilityPercentage[j] = probability;
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT) + i * MAX_STEPS + j] = probabilityGroupMode[j];
					} 					 
				} 
				messagesToMother[i] = isDirty || trackDirty[i];
			}			
			
			leftExpander.module->rightExpander.messageFlipRequested = true;		
		} else {
			for (int j = 0; j < MAX_STEPS; j++) {
				stepProbabilityPercentage[j] = 0;
			} 	
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



         for(int i=0; i<MAX_STEPS / 3;i++) {
			ParamWidget* stepCol1Param = createParam<RoundSmallFWKnob>(Vec(10, 30 + i*46), module, QARProbabilityExpander::PROBABILITY_1_PARAM + i);
			if (module) {
				dynamic_cast<RoundSmallFWKnob*>(stepCol1Param)->percentage = &module->stepProbabilityPercentage[i];
			}
			addParam(stepCol1Param);							
			ParamWidget* stepCol2Param = createParam<RoundSmallFWKnob>(Vec(72, 30 + i*46), module, QARProbabilityExpander::PROBABILITY_1_PARAM + i + (MAX_STEPS / 3));
			if (module) {
				dynamic_cast<RoundSmallFWKnob*>(stepCol2Param)->percentage = &module->stepProbabilityPercentage[i + (MAX_STEPS / 3)];
			}
			addParam(stepCol2Param);							
			ParamWidget* stepCol3Param = createParam<RoundSmallFWKnob>(Vec(134, 30 + i*46), module, QARProbabilityExpander::PROBABILITY_1_PARAM + i + (MAX_STEPS * 2 / 3));
			if (module) {
				dynamic_cast<RoundSmallFWKnob*>(stepCol3Param)->percentage = &module->stepProbabilityPercentage[i + (MAX_STEPS * 2 / 3)];
			}
			addParam(stepCol3Param);							


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

		addParam(createParam<LEDButton>(Vec(12, 324), module, QARProbabilityExpander::STEP_OR_DIV_PARAM));
        addChild(createLight<LargeLight<BlueLight>>(Vec(13.5, 325.5), module, QARProbabilityExpander::USING_DIVS_LIGHT));

		for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(76 + i*24, 320), module, QARProbabilityExpander::TRACK_1_PROBABILITY_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(77.5 + i*24, 321.5), module, QARProbabilityExpander::TRACK_1_PROBABILITY_ENABELED_LIGHT + i));

		}


	}
};

Model *modelQARProbabilityExpander = createModel<QARProbabilityExpander, QARProbabilityExpanderWidget>("QARProbabilityExpander");
