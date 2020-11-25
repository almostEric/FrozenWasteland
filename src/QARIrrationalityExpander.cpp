#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 18
#define ACTUAL_MAX_STEPS 73
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 4
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 12
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT


struct QARIrrationalityExpander : Module {

    enum ParamIds {
		TRACK_1_IR_ENABLED_PARAM,
		TRACK_2_IR_ENABLED_PARAM,
		TRACK_3_IR_ENABLED_PARAM,
		TRACK_4_IR_ENABLED_PARAM,
		IR_ON_OFF_PARAM,
		IR_START_POS_PARAM,
		IR_START_POS_CV_ATTENUVETER_PARAM,
        IR_NUM_STEPS_PARAM,
        IR_NUM_STEPS_CV_ATTENUVETER_PARAM,
		IR_RATIO_PARAM,
		IR_RATIO_CV_ATTENUVETER_PARAM,
		STEP_OR_DIV_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
        IR_ON_OFF_INPUT,
        IR_START_POS_INPUT,
        IR_NUM_STEPS_INPUT,
		IR_RATIO_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		NUM_OUTPUTS
	};

	enum LightIds {
		CONNECTED_LIGHT,		
		TRACK_1_IR_ENABELED_LIGHT,
		TRACK_2_IR_ENABELED_LIGHT,
		TRACK_3_IR_ENABELED_LIGHT,
		TRACK_4_IR_ENABELED_LIGHT,
		IR_ON_LIGHT,
		USING_DIVS_LIGHT,
		NUM_LIGHTS 
	};



	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

    
	dsp::SchmittTrigger trackIRTrigger[TRACK_COUNT],irEnableTrigger,stepDivTrigger;
	bool trackIRSelected[TRACK_COUNT],irEnabled = false;
	bool stepsOrDivs = false;

	float irPos;
	float irNbrSteps;
	float irRatio;


	float sceneData[NBR_SCENES][12] = {{0}};
	int sceneChangeMessage = 0;


			
	
	QARIrrationalityExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		        
        configParam(IR_START_POS_PARAM, 1.0f, ACTUAL_MAX_STEPS-2, 0.0,"Start Position");
        configParam(IR_START_POS_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Start Position CV Attenuation","%",0,100);

        configParam(IR_NUM_STEPS_PARAM, 3.0, MAX_STEPS-1, 0,"# Steps");
        configParam(IR_NUM_STEPS_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"# Steps CV Attenuation","%",0,100);		

        configParam(IR_RATIO_PARAM, 2.0, MAX_STEPS-2, 0,"Ratio");
        configParam(IR_RATIO_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Ratio CV Attenuation","%",0,100);		
        
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		json_object_set_new(rootJ, "irEnabled", json_integer((bool) irEnabled));
		json_object_set_new(rootJ, "stepsOrDivs", json_integer((bool) stepsOrDivs));

		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackIRActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackIRSelected[i]));			
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

        json_t *ireJ = json_object_get(rootJ, "irEnabled");
		if (ireJ)
			irEnabled = json_integer_value(ireJ);			

        json_t *sdJ = json_object_get(rootJ, "stepsOrDivs");
		if (sdJ)
			stepsOrDivs = json_integer_value(sdJ);


		for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "trackIRActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
			    trackIRSelected[i] = json_integer_value(sumJ);			
			}
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
		sceneData[scene][0] = irEnabled;
		sceneData[scene][1] = stepsOrDivs;
		sceneData[scene][2] = params[IR_START_POS_PARAM].getValue();
		sceneData[scene][3] = params[IR_NUM_STEPS_PARAM].getValue();
		sceneData[scene][4] = params[IR_RATIO_PARAM].getValue();
		sceneData[scene][5] = params[IR_START_POS_CV_ATTENUVETER_PARAM].getValue();
		sceneData[scene][6] = params[IR_NUM_STEPS_CV_ATTENUVETER_PARAM].getValue();
		sceneData[scene][7] = params[IR_RATIO_CV_ATTENUVETER_PARAM].getValue();
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			sceneData[scene][trackNumber+8] = trackIRSelected[trackNumber];
		}
	}

	void loadScene(int scene) {
		irEnabled = sceneData[scene][0];
		stepsOrDivs = sceneData[scene][1];
		params[IR_START_POS_PARAM].setValue(sceneData[scene][2]);
		params[IR_NUM_STEPS_PARAM].setValue(sceneData[scene][3]);
		params[IR_RATIO_PARAM].setValue(sceneData[scene][4]);
		params[IR_START_POS_CV_ATTENUVETER_PARAM].setValue(sceneData[scene][5]);
		params[IR_NUM_STEPS_CV_ATTENUVETER_PARAM].setValue(sceneData[scene][6]);
		params[IR_RATIO_CV_ATTENUVETER_PARAM].setValue(sceneData[scene][7]);
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			trackIRSelected[trackNumber] = sceneData[scene][trackNumber+8];
		}
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackIRTrigger[i].process(params[TRACK_1_IR_ENABLED_PARAM+i].getValue())) {
				trackIRSelected[i] = !trackIRSelected[i];
			}
			lights[TRACK_1_IR_ENABELED_LIGHT+i].value = trackIRSelected[i];
		}        
		if (irEnableTrigger.process(params[IR_ON_OFF_PARAM].getValue() + inputs[IR_ON_OFF_INPUT].getVoltage())) {
			irEnabled = !irEnabled;
		}
		lights[IR_ON_LIGHT].value = irEnabled;

		if (stepDivTrigger.process(params[STEP_OR_DIV_PARAM].getValue())) {
			stepsOrDivs = !stepsOrDivs;
		}
		lights[USING_DIVS_LIGHT].value = stepsOrDivs;


		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWellFormedRhythmExpander || 
								leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || 
								leftExpander.module->model == modelQARIrrationalityExpander || leftExpander.module->model == modelPWAlgorithmicExpander));
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
											rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                if(rightExpander.module->model != modelQuadAlgorithmicRhythm) { // Get QRE values							
					std::copy(messagesFromExpander,messagesFromExpander + PASSTHROUGH_OFFSET,messagesToMother);		
				}

				//QAR Pass through left
				std::copy(messagesFromExpander+PASSTHROUGH_OFFSET,messagesFromExpander+PASSTHROUGH_OFFSET+PASSTHROUGH_LEFT_VARIABLE_COUNT,messagesToMother+PASSTHROUGH_OFFSET);		

				//QAR Pass through right
				std::copy(messagesFromMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT,messagesFromMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT,
									messageToExpander+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT);			

				rightExpander.module->leftExpander.messageFlipRequested = true;
			}


            irPos = clamp(params[IR_START_POS_PARAM].getValue() + std::floor(inputs[IR_START_POS_INPUT].isConnected() ? inputs[IR_START_POS_INPUT].getVoltage() * 0.6f * params[IR_START_POS_CV_ATTENUVETER_PARAM].getValue() : 0.0f),1.0,ACTUAL_MAX_STEPS-2.0);
            irNbrSteps = clamp(params[IR_NUM_STEPS_PARAM].getValue() + std::floor(inputs[IR_NUM_STEPS_INPUT].isConnected() ? inputs[IR_NUM_STEPS_INPUT].getVoltage() / 1.8 * params[IR_NUM_STEPS_CV_ATTENUVETER_PARAM].getValue() : 0.0f),3.0f,ACTUAL_MAX_STEPS-1.0);
            irRatio = clamp(params[IR_RATIO_PARAM].getValue() + std::floor(inputs[IR_RATIO_INPUT].isConnected() ? inputs[IR_RATIO_INPUT].getVoltage() / 1.8 * params[IR_RATIO_CV_ATTENUVETER_PARAM].getValue() : 0.0f),2.0f,ACTUAL_MAX_STEPS-2.0);
			//float ratio = std::min(irRatio/irNbrSteps,1.0f);

				// fprintf(stderr, "%f %f %f %f\n", irPos, irNbrSteps,irRatio,ratio );
            for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackIRSelected[i] && irEnabled) {
					int openMessageSlot = MAX_STEPS;
					for (int j = 0; j < MAX_STEPS-2; j+=3) { // skip
						if(messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 3) + (i * MAX_STEPS) + j] == 0) {
							openMessageSlot = j;
							break;
						}
					}				
					if(openMessageSlot < MAX_STEPS) {
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 3) + (i * MAX_STEPS) + openMessageSlot] = stepsOrDivs ? -irPos : irPos;  //negative indicates DIVs
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 3) + (i * MAX_STEPS) + openMessageSlot+1] = irNbrSteps;	
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 3) + (i * MAX_STEPS) + openMessageSlot+2] = irRatio;	
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

		for(int i =0;i<TRACK_COUNT;i++) {
			trackIRSelected[i] = true;
		}
	}
};



struct QARIrrationalityExpanderDisplay : TransparentWidget {
	QARIrrationalityExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	QARIrrationalityExpanderDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}

	void drawStartPosition(const DrawArgs &args, Vec pos, int startPosition) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", startPosition);  
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawLength(const DrawArgs &args, Vec pos, int length) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", length);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}


	void drawRatio(const DrawArgs &args, Vec pos, int ratio) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", ratio);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}


	void draw(const DrawArgs &args) override {
		if (!module)
			return; 		

		drawStartPosition(args, Vec(92, 78), module->irPos);
		drawLength(args, Vec(92, 158), module->irNbrSteps);
		drawRatio(args, Vec(92, 238), module->irRatio);
	}
};




struct QARIrrationalityExpanderWidget : ModuleWidget {
	QARIrrationalityExpanderWidget(QARIrrationalityExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARIrrationalityExpander.svg")));

		{
			QARIrrationalityExpanderDisplay *display = new QARIrrationalityExpanderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}


		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	
		addParam(createParam<LEDButton>(Vec(12, 294), module, QARIrrationalityExpander::STEP_OR_DIV_PARAM));
        addChild(createLight<LargeLight<BlueLight>>(Vec(13.5, 295.5), module, QARIrrationalityExpander::USING_DIVS_LIGHT));

		addParam(createParam<LEDButton>(Vec(52, 294), module, QARIrrationalityExpander::IR_ON_OFF_PARAM));
        addChild(createLight<LargeLight<GreenLight>>(Vec(53.5, 295.5), module, QARIrrationalityExpander::IR_ON_LIGHT));
        addInput(createInput<FWPortInSmall>(Vec(77, 294), module, QARIrrationalityExpander::IR_ON_OFF_INPUT));


         for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(7 + i*24, 333), module, QARIrrationalityExpander::TRACK_1_IR_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(8.5 + i*24, 334.5), module, QARIrrationalityExpander::TRACK_1_IR_ENABELED_LIGHT + i));
		}


        addParam(createParam<RoundFWSnapKnob>(Vec(12, 59), module, QARIrrationalityExpander::IR_START_POS_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(47, 64), module, QARIrrationalityExpander::IR_START_POS_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(44, 87), module, QARIrrationalityExpander::IR_START_POS_CV_ATTENUVETER_PARAM));

        addParam(createParam<RoundFWSnapKnob>(Vec(12, 139), module, QARIrrationalityExpander::IR_NUM_STEPS_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(47, 144), module, QARIrrationalityExpander::IR_NUM_STEPS_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(44, 167), module, QARIrrationalityExpander::IR_NUM_STEPS_CV_ATTENUVETER_PARAM));

        addParam(createParam<RoundFWSnapKnob>(Vec(12, 219), module, QARIrrationalityExpander::IR_RATIO_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(47, 224), module, QARIrrationalityExpander::IR_RATIO_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(44, 247), module, QARIrrationalityExpander::IR_RATIO_CV_ATTENUVETER_PARAM));

    
	}
};

Model *modelQARIrrationalityExpander = createModel<QARIrrationalityExpander, QARIrrationalityExpanderWidget>("QARIrrationalityExpander");
 