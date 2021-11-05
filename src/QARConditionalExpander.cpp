#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 18
#define MAX_DIVIDE_COUNT 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT

struct QARConditionalExpander : Module {
	enum ParamIds {
		TRACK_1_CONDITIONAL_ENABLED_PARAM,
		TRACK_2_CONDITIONAL_ENABLED_PARAM,
		TRACK_3_CONDITIONAL_ENABLED_PARAM,
		TRACK_4_CONDITIONAL_ENABLED_PARAM,
		DIVIDE_COUNT_1_PARAM,
		DIVIDE_COUNT_ATTEN_1_PARAM = DIVIDE_COUNT_1_PARAM + MAX_STEPS,
		CONDITIONAL_MODE_PARAM = DIVIDE_COUNT_ATTEN_1_PARAM + MAX_STEPS,
		STEP_OR_DIV_PARAM = CONDITIONAL_MODE_PARAM + MAX_STEPS,
		NUM_PARAMS
	};
	enum InputIds {
		DIVIDE_COUNT_1_INPUT,
		CONDITIONAL_MODE_1_INPUT = DIVIDE_COUNT_1_INPUT + MAX_STEPS,
		NUM_INPUTS = CONDITIONAL_MODE_1_INPUT + MAX_STEPS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		CONNECTED_LIGHT,
		TRACK_1_CONDITIONAL_ENABELED_LIGHT,
		TRACK_2_CONDITIONAL_ENABELED_LIGHT,
		TRACK_3_CONDITIONAL_ENABELED_LIGHT,
		TRACK_4_CONDITIONAL_ENABELED_LIGHT,
        USING_DIVS_LIGHT,
		CONDITIONAL_MODE_1_LIGHT,
		NUM_LIGHTS = CONDITIONAL_MODE_1_LIGHT + (MAX_STEPS * 3)
	};


	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

	bool trackDirty[TRACK_COUNT] = {0};

	
	dsp::SchmittTrigger stepDivTrigger,trackConditionalTrigger[TRACK_COUNT],conditionalModeTrigger[MAX_STEPS];
	bool stepsOrDivs;
	bool trackConditionalSelected[TRACK_COUNT];
	bool conditionalMode[MAX_STEPS];

	float lastDivideCount[MAX_STEPS] = {0};

	float sceneData[NBR_SCENES][59] = {{0}};
	int sceneChangeMessage = 0;

	//percentages
	float stepConditionalPercentage[MAX_STEPS] = {0};

	bool QARExpanderDisconnectReset = true;

	QARConditionalExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		
		for(int i =0;i<TRACK_COUNT;i++) {
			configButton(TRACK_1_CONDITIONAL_ENABLED_PARAM + i ,"Enable Track " + std::to_string(i+1));
			trackConditionalSelected[i] = true;
		}

        for (int i = 0; i < MAX_STEPS; i++) {
            configParam(DIVIDE_COUNT_1_PARAM + i, 1.0f, 16.0f, 1.0f,"Step "  + std::to_string(i+1) + " Divide Count");
			configParam(DIVIDE_COUNT_ATTEN_1_PARAM + i, -1.0, 1.0, 0.0,"Step "  + std::to_string(i+1) + " Divide Count CV Attenuation","%",0,100);

			configButton(CONDITIONAL_MODE_PARAM + i ,"Step "  + std::to_string(i+1) + " Conditional Mode");

			configInput(DIVIDE_COUNT_1_INPUT+i, "Step "  + std::to_string(i+1) + " Divide Count");
			configInput(CONDITIONAL_MODE_1_INPUT+i, "Step "  + std::to_string(i+1) + " Conditional Mode");
		}


		configButton(STEP_OR_DIV_PARAM ,"Step/Beat Mode");
		
        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
        json_object_set_new(rootJ, "stepsOrDivs", json_integer((bool) stepsOrDivs));

		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackConditionalActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackConditionalSelected[i]));			
		}

		for(int i=0;i<MAX_STEPS;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "conditionalMode");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_boolean(conditionalMode[i]));			
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
			strcpy(buf, "trackConditionalActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				trackConditionalSelected[i] = json_integer_value(sumJ);			
			}
		}	

		for(int i=0;i<MAX_STEPS;i++) {
			char buf[100];
			strcpy(buf, "conditionalMode");
			strcat(buf, stepNames[i]);

			json_t *cmJ = json_object_get(rootJ, buf);
			if (cmJ) {
				conditionalMode[i] = json_boolean_value(cmJ);			
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
			sceneData[scene][trackNumber+1] = trackConditionalSelected[trackNumber];
		}
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			sceneData[scene][stepNumber+5] = params[DIVIDE_COUNT_1_PARAM+stepNumber].getValue();
			sceneData[scene][stepNumber+23] = params[DIVIDE_COUNT_ATTEN_1_PARAM+stepNumber].getValue();
			sceneData[scene][stepNumber+41] = conditionalMode[stepNumber];
		}
	}

	void loadScene(int scene) {
		stepsOrDivs = sceneData[scene][0];
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			trackConditionalSelected[trackNumber] = sceneData[scene][trackNumber+1];
		}
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			params[DIVIDE_COUNT_1_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+5]);
			params[DIVIDE_COUNT_ATTEN_1_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+23]);
			conditionalMode[stepNumber] = sceneData[scene][stepNumber+41];
		}
	}


	void process(const ProcessArgs &args) override {
		bool isDirty = false;
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackConditionalTrigger[i].process(params[TRACK_1_CONDITIONAL_ENABLED_PARAM+i].getValue())) {
				trackConditionalSelected[i] = !trackConditionalSelected[i];
				isDirty = true;
			}
			lights[TRACK_1_CONDITIONAL_ENABELED_LIGHT+i].value = trackConditionalSelected[i];
		}
	    if (stepDivTrigger.process(params[STEP_OR_DIV_PARAM].getValue())) {
            stepsOrDivs = !stepsOrDivs;
			isDirty = true;
        }
        lights[USING_DIVS_LIGHT].value = stepsOrDivs;
		for(int i=0; i< MAX_STEPS; i++) {
			if (conditionalModeTrigger[i].process(params[CONDITIONAL_MODE_PARAM+i].getValue() + inputs[CONDITIONAL_MODE_1_INPUT+i].getVoltage())) {
				conditionalMode[i] = !conditionalMode[i];
				isDirty = true;
			}
			lights[CONDITIONAL_MODE_1_LIGHT+i*3].value = conditionalMode[i];
			lights[CONDITIONAL_MODE_1_LIGHT+i*3+1].value = 0;
			lights[CONDITIONAL_MODE_1_LIGHT+i*3+2].value = conditionalMode[i];
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
                if(trackConditionalSelected[i]) {
                    messagesToMother[TRACK_COUNT * 16 + i] = stepsOrDivs ? 2 : 1;
    				for (int j = 0; j < MAX_STEPS; j++) {
						int divideCount = clamp(params[DIVIDE_COUNT_1_PARAM+j].getValue() + (inputs[DIVIDE_COUNT_1_INPUT + j].isConnected() ? inputs[DIVIDE_COUNT_1_INPUT + j].getVoltage() * 1.6 * params[DIVIDE_COUNT_ATTEN_1_PARAM + j].getValue() : 0.0f),1.0,16.0f);
						if(divideCount != lastDivideCount[j]) {
							isDirty = true;
						}
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 4) + (i * MAX_STEPS) + j] = divideCount;
						stepConditionalPercentage[j] = (divideCount-1.0)/(MAX_DIVIDE_COUNT-1.0);
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 5) + (i * MAX_STEPS) + j] = conditionalMode[j];
					} 					 
				} 
				messagesToMother[i] = isDirty || trackDirty[i];
			}			
			
			leftExpander.module->rightExpander.messageFlipRequested = true;		
		} else {
			for (int j = 0; j < MAX_STEPS; j++) {
				stepConditionalPercentage[j] = 0;
			} 	
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
        stepsOrDivs = false;

		for(int i =0;i<TRACK_COUNT;i++) {
            params[DIVIDE_COUNT_1_PARAM+i].setValue(1);
			trackConditionalSelected[i] = true;
		}
	}
};

struct QARConditionalExpanderWidget : ModuleWidget {
	QARConditionalExpanderWidget(QARConditionalExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARConditionalExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



         for(int i=0; i<MAX_STEPS / 3;i++) {
			ParamWidget* stepCol1Param = createParam<RoundSmallFWSnapKnob>(Vec(10, 30 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_1_PARAM + i);
			if (module) {
				dynamic_cast<RoundSmallFWSnapKnob*>(stepCol1Param)->percentage = &module->stepConditionalPercentage[i];
			}
			addParam(stepCol1Param);							
			ParamWidget* stepCol2Param = createParam<RoundSmallFWSnapKnob>(Vec(72, 30 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_1_PARAM + i + (MAX_STEPS / 3));
			if (module) {
				dynamic_cast<RoundSmallFWSnapKnob*>(stepCol2Param)->percentage = &module->stepConditionalPercentage[i + (MAX_STEPS / 3)];
			}
			addParam(stepCol2Param);							
			ParamWidget* stepCol3Param = createParam<RoundSmallFWSnapKnob>(Vec(134, 30 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_1_PARAM + i + (MAX_STEPS * 2 / 3));
			if (module) {
				dynamic_cast<RoundSmallFWSnapKnob*>(stepCol3Param)->percentage = &module->stepConditionalPercentage[i + (MAX_STEPS * 2 / 3)];
			}
			addParam(stepCol3Param);							


		    addParam(createParam<RoundReallySmallFWKnob>(Vec(36, 50 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_ATTEN_1_PARAM + i));
		    addParam(createParam<RoundReallySmallFWKnob>(Vec(98, 50 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_ATTEN_1_PARAM + i + (MAX_STEPS / 3)));
		    addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 50 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_ATTEN_1_PARAM + i + (MAX_STEPS * 2 / 3)));
    
    		addInput(createInput<FWPortInReallySmall>(Vec(40, 33 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_1_INPUT + i));
    		addInput(createInput<FWPortInReallySmall>(Vec(102, 33 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_1_INPUT + i + (MAX_STEPS / 3)));
    		addInput(createInput<FWPortInReallySmall>(Vec(164, 33 + i*46), module, QARConditionalExpander::DIVIDE_COUNT_1_INPUT + i + (MAX_STEPS * 2 / 3)));

			addParam(createParam<LEDButton>(Vec(16, 53 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_PARAM+i));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(17.5, 54.5 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_1_LIGHT + (i*3)));
			addParam(createParam<LEDButton>(Vec(78, 53 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_PARAM+i+(MAX_STEPS / 3)));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(79.5, 54.5 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_1_LIGHT + (i*3) + MAX_STEPS));
			addParam(createParam<LEDButton>(Vec(140, 53 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_PARAM+i + (MAX_STEPS * 2 / 3)));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(141.5, 54.5 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_1_LIGHT + (i*3) + MAX_STEPS * 2));


    		addInput(createInput<FWPortInReallySmall>(Vec(4, 57 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_1_INPUT + i));
    		addInput(createInput<FWPortInReallySmall>(Vec(66, 57 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_1_INPUT + i + (MAX_STEPS / 3)));
    		addInput(createInput<FWPortInReallySmall>(Vec(128, 57 + i*46), module, QARConditionalExpander::CONDITIONAL_MODE_1_INPUT + i + (MAX_STEPS * 2 / 3)));

        }
	

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadRhythmExpander::CONNECTED_LIGHT));

		addParam(createParam<LEDButton>(Vec(12, 324), module, QARConditionalExpander::STEP_OR_DIV_PARAM));
        addChild(createLight<LargeLight<BlueLight>>(Vec(13.5, 325.5), module, QARConditionalExpander::USING_DIVS_LIGHT));

		for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(76 + i*24, 320), module, QARConditionalExpander::TRACK_1_CONDITIONAL_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(77.5 + i*24, 321.5), module, QARConditionalExpander::TRACK_1_CONDITIONAL_ENABELED_LIGHT + i));

		}


	}
};

Model *modelQARConditionalExpander = createModel<QARConditionalExpander, QARConditionalExpanderWidget>("QARConditionalExpander");
