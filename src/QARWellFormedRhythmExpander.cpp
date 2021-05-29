#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT


struct QARWellFormedRhythmExpander : Module {

    enum ParamIds {
		TRACK_1_EXTRA_VALUE_PARAM,
		TRACK_2_EXTRA_VALUE_PARAM,
		TRACK_3_EXTRA_VALUE_PARAM,
		TRACK_4_EXTRA_VALUE_PARAM,
		TRACK_1_HIERARCHICAL_PARAM,
		TRACK_2_HIERARCHICAL_PARAM,
		TRACK_3_HIERARCHICAL_PARAM,
		TRACK_4_HIERARCHICAL_PARAM,
		TRACK_1_COMPLEMENT_PARAM,
		TRACK_2_COMPLEMENT_PARAM,
		TRACK_3_COMPLEMENT_PARAM,
		TRACK_4_COMPLEMENT_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
        TRACK_1_EXTRA_VALUE_INPUT,
        TRACK_2_EXTRA_VALUE_INPUT,
        TRACK_3_EXTRA_VALUE_INPUT,
        TRACK_4_EXTRA_VALUE_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		NUM_OUTPUTS
	};

	enum LightIds {
		TRACK_1_HIERARCHICAL_LIGHT,
		TRACK_2_HIERARCHICAL_LIGHT,
		TRACK_3_HIERARCHICAL_LIGHT,
		TRACK_4_HIERARCHICAL_LIGHT,
		TRACK_1_COMPLEMENT_LIGHT,
		TRACK_2_COMPLEMENT_LIGHT = TRACK_1_COMPLEMENT_LIGHT + 3,
		TRACK_3_COMPLEMENT_LIGHT = TRACK_2_COMPLEMENT_LIGHT + 3,
		TRACK_4_COMPLEMENT_LIGHT = TRACK_3_COMPLEMENT_LIGHT + 3,
		NUM_LIGHTS = TRACK_4_COMPLEMENT_LIGHT + 3 
	};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

	bool trackDirty[TRACK_COUNT] = {0};


	dsp::SchmittTrigger trackHierarchicalTrigger[TRACK_COUNT],trackComplementTrigger[TRACK_COUNT];
	float extraParameterValue[TRACK_COUNT] = {0};
	bool trackHierachical[TRACK_COUNT] = {0};
	int trackComplement[TRACK_COUNT] = {0};

	int lastExtraParameterValue[TRACK_COUNT] = {0};

	float sceneData[NBR_SCENES][12] = {{0}};
	int sceneChangeMessage = 0;


	bool isDirty = false;
	bool QARExpanderDisconnectReset = true;


	//percentages
	float extraValuePercentage[TRACK_COUNT] = {0};

    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}


	QARWellFormedRhythmExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		        
        configParam(TRACK_1_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 1 - Ratio");
        configParam(TRACK_2_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 2 - Ratio");
        configParam(TRACK_3_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 3 - Ratio");
        configParam(TRACK_4_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 4 - Ratio");

        configParam(TRACK_1_HIERARCHICAL_PARAM, 0.0f, 1.0, 0.0,"Track 1 - Hierarchical");
        configParam(TRACK_2_HIERARCHICAL_PARAM, 0.0f, 1.0, 0.0,"Track 2 - Hierarchical");
        configParam(TRACK_3_HIERARCHICAL_PARAM, 0.0f, 1.0, 0.0,"Track 3 - Hierarchical");
        configParam(TRACK_4_HIERARCHICAL_PARAM, 0.0f, 1.0, 0.0,"Track 4 - Hierarchical");

        configParam(TRACK_1_COMPLEMENT_PARAM, 0.0f, 1.0, 0.0,"Track 1 - Complimentary");
        configParam(TRACK_2_COMPLEMENT_PARAM, 0.0f, 1.0, 0.0,"Track 2 - Complimentary");
        configParam(TRACK_3_COMPLEMENT_PARAM, 0.0f, 1.0, 0.0,"Track 3 - Complimentary");
        configParam(TRACK_4_COMPLEMENT_PARAM, 0.0f, 1.0, 0.0,"Track 4 - Complimentary");

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
        for(int i=0;i<TRACK_COUNT;i++) {
			std::string buf = "hierarchical-" + std::to_string(i);
			json_object_set(rootJ, buf.c_str(),json_integer(trackHierachical[i]));
			buf = "complement-" + std::to_string(i);
			json_object_set(rootJ, buf.c_str(),json_integer(trackComplement[i]));
        }

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<12;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_object_set_new(rootJ, buf.c_str(), json_real(sceneData[scene][i]));
			}
		}
        
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

        for(int i=0;i<TRACK_COUNT;i++) {
			std::string buf = "hierarchical-" + std::to_string(i);
            json_t *ctHl = json_object_get(rootJ, buf.c_str());
            if (ctHl)
                trackHierachical[i] = json_integer_value(ctHl);
			buf = "complement-" + std::to_string(i);
            json_t *ctCl = json_object_get(rootJ, buf.c_str());
            if (ctCl)
                trackComplement[i] = json_integer_value(ctCl);
        }

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<12;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_t *sdJ = json_object_get(rootJ, buf.c_str());
				if (json_real_value(sdJ)) {
					sceneData[scene][i] = json_real_value(sdJ);
				}
			}
		}
	}

	void saveScene(int scene) {
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			sceneData[scene][trackNumber+0] = params[TRACK_1_EXTRA_VALUE_PARAM+trackNumber].getValue();
			sceneData[scene][trackNumber+4] = params[TRACK_1_HIERARCHICAL_PARAM+trackNumber].getValue();
			sceneData[scene][trackNumber+8] = params[TRACK_1_COMPLEMENT_PARAM+trackNumber].getValue();
		}
	}

	void loadScene(int scene) {
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			params[TRACK_1_EXTRA_VALUE_PARAM+trackNumber].setValue(sceneData[scene][trackNumber+0]);
			params[TRACK_1_HIERARCHICAL_PARAM+trackNumber].setValue(sceneData[scene][trackNumber+4]);
			params[TRACK_1_COMPLEMENT_PARAM+trackNumber].setValue(sceneData[scene][trackNumber+8]);
		}
	}


	void process(const ProcessArgs &args) override {
		isDirty = false;
		for(int i=0; i< TRACK_COUNT; i++) {            
            float t = clamp(params[TRACK_1_EXTRA_VALUE_PARAM+i].getValue() + (inputs[TRACK_1_EXTRA_VALUE_INPUT+i].isConnected() ? inputs[TRACK_1_EXTRA_VALUE_INPUT+i].getVoltage() / 10.0f : 0.0f ),0.0f,0.999f);
			extraValuePercentage[i] = t;
			extraParameterValue[i] = 1/(1-t);
			if(extraParameterValue[i] != lastExtraParameterValue[i]) {
				isDirty = true;
				lastExtraParameterValue[i] = extraParameterValue[i];
			}
			
			if (trackHierarchicalTrigger[i].process(params[TRACK_1_HIERARCHICAL_PARAM+i].getValue())) {
				trackHierachical[i] = !trackHierachical[i];
				isDirty = true;
			}
			lights[TRACK_1_HIERARCHICAL_LIGHT + i].value = trackHierachical[i];

			if (trackComplementTrigger[i].process(params[TRACK_1_COMPLEMENT_PARAM+i].getValue())) {
				trackComplement[i] = (trackComplement[i] + 1) % 3;
				isDirty = true;
			}
			lights[TRACK_1_COMPLEMENT_LIGHT + (i * 3) + 0].value = trackComplement[i] == 2;
			lights[TRACK_1_COMPLEMENT_LIGHT + (i * 3) + 1].value = trackComplement[i] > 0;
		}        

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWellFormedRhythmExpander || 
								leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || 
								leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelQARIrrationalityExpander ||
								leftExpander.module->model == modelQARConditionalExpander || 
								leftExpander.module->model == modelPWAlgorithmicExpander));
		//lights[CONNECTED_LIGHT].value = motherPresent;
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
                messagesToMother[TRACK_COUNT + i] = extraParameterValue[i];
				messagesToMother[TRACK_COUNT * 2 + i] = trackHierachical[i];
				messagesToMother[TRACK_COUNT * 3 + i] = trackComplement[i];
				messagesToMother[i] = isDirty || trackDirty[i];
			}
					
			leftExpander.module->rightExpander.messageFlipRequested = true;
		
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

};



struct QARWellFormedRhythmExpanderDisplay : TransparentWidget {
	QARWellFormedRhythmExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	QARWellFormedRhythmExpanderDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}



	


	void draw(const DrawArgs &args) override {
		if (!module)
			return; 		
	}
};

struct QARWellFormedRhythmExpanderWidget : ModuleWidget {
	QARWellFormedRhythmExpanderWidget(QARWellFormedRhythmExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARWellFormedRhythmExpander.svg")));

		{
			QARWellFormedRhythmExpanderDisplay *display = new QARWellFormedRhythmExpanderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}


		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	
        

        for(int i=0;i<TRACK_COUNT; i++) {
			ParamWidget* extraValueParam = createParam<RoundFWKnob>(Vec(12, 59 + i * 72), module, QARWellFormedRhythmExpander::TRACK_1_EXTRA_VALUE_PARAM+i);
			if (module) {
				dynamic_cast<RoundFWKnob*>(extraValueParam)->percentage = &module->extraValuePercentage[i];
			}
			addParam(extraValueParam);							
            addInput(createInput<FWPortInSmall>(Vec(49, 64 + i * 72), module, QARWellFormedRhythmExpander::TRACK_1_EXTRA_VALUE_INPUT+i));

			if(i > 0) {
				addParam(createParam<LEDButton>(Vec(10, 103 + i*72), module, QARWellFormedRhythmExpander::TRACK_1_HIERARCHICAL_PARAM+i));
				addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 104.5 + i*72), module, QARWellFormedRhythmExpander::TRACK_1_HIERARCHICAL_LIGHT+i));

				addParam(createParam<LEDButton>(Vec(48, 103 + i*72), module, QARWellFormedRhythmExpander::TRACK_1_COMPLEMENT_PARAM+i));
				addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(49.5, 104.5 + i*72), module, QARWellFormedRhythmExpander::TRACK_1_COMPLEMENT_LIGHT+i*3));
			}

		}



    
	}
};

Model *modelQARWellFormedRhythmExpander = createModel<QARWellFormedRhythmExpander, QARWellFormedRhythmExpanderWidget>("QARWellFormedRhythmExpander");
 