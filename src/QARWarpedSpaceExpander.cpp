#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 73
#define EXPANDER_MAX_STEPS 18
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define QAR_GRID_VALUES MAX_STEPS
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT + QAR_GRID_VALUES


struct QARWarpedSpaceExpander : Module {

    enum ParamIds {
		TRACK_1_WARP_ENABLED_PARAM,
		TRACK_2_WARP_ENABLED_PARAM,
		TRACK_3_WARP_ENABLED_PARAM,
		TRACK_4_WARP_ENABLED_PARAM,
		WARP_AMOUNT_PARAM,
        WARP_AMOUNT_CV_ATTENUVETER_PARAM,
        WARP_POSITION_PARAM,
        WARP_POSITION_CV_ATTENUVETER_PARAM,
        WARP_LENGTH_PARAM,
        WARP_LENGTH_CV_ATTENUVETER_PARAM,
		WS_ON_OFF_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
        WARP_AMOUNT_INPUT,
        WARP_POSITION_INPUT,
		WARP_LENGTH_INPUT,
        WS_ON_OFF_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		NUM_OUTPUTS
	};

	enum LightIds {
		CONNECTED_LIGHT,
		TRACK_1_WARP_ENABELED_LIGHT,
		TRACK_2_WARP_ENABELED_LIGHT,
		TRACK_3_WARP_ENABELED_LIGHT,
		TRACK_4_WARP_ENABELED_LIGHT,
		WS_ON_LIGHT,
		NUM_LIGHTS 
	};



	const char* stepNames[EXPANDER_MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

	bool trackDirty[TRACK_COUNT] = {0};


	float sceneData[NBR_SCENES][11] = {{0}};
	int sceneChangeMessage = 0;

	float warpAmount = 0;
	float warpPosition = 0;
	float warpLength = 0;

	float lastWarpAmount = 0;
	float lastWarpPosition = 0;
	float lastWarpLength = 0;

	bool QARExpanderDisconnectReset = true;

    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	
	dsp::SchmittTrigger trackWarpTrigger[TRACK_COUNT],wsEnableTrigger;
	bool trackWarpSelected[TRACK_COUNT],wsEnabled = true;

	//percentages
	float warpAmountPercentage = 0;
	float warpPositionPercentage = 0;
	float warpLengthPercentage = 0;

	
	QARWarpedSpaceExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		        
        configParam(WARP_AMOUNT_PARAM, 1.0f, 6.0, 1.0,"Warp Amount");
        configParam(WARP_AMOUNT_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Amount CV Attenuation","%",0,100);
		configInput(WARP_AMOUNT_INPUT, "Warp Amount");

        configParam(WARP_POSITION_PARAM, 0.0, MAX_STEPS-1, 0,"Warp Position");
        configParam(WARP_POSITION_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Position CV Attenuation","%",0,100);		
		configInput(WARP_POSITION_INPUT, "Warp Position");

        configParam(WARP_LENGTH_PARAM, 1.0, MAX_STEPS, 1,"Warp Length");
        configParam(WARP_LENGTH_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Length CV Attenuation","%",0,100);		
		configInput(WARP_LENGTH_INPUT, "Warp Length");

		configButton(WS_ON_OFF_PARAM ,"Enable Warping");

		for(int i =0;i<TRACK_COUNT;i++) {
			configButton(TRACK_1_WARP_ENABLED_PARAM + i ,"Enable Track " + std::to_string(i+1));
		}

        
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "wsEnabled", json_boolean(wsEnabled));

		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackWarpActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackWarpSelected[i]));			
		}

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<8;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_object_set_new(rootJ, buf.c_str(), json_real(sceneData[scene][i]));
			}
		}
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *wseJ = json_object_get(rootJ, "wsEnabled");
		if (wseJ)
			wsEnabled = json_boolean_value(wseJ);			

		for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "trackWarpActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
			    trackWarpSelected[i] = json_integer_value(sumJ);			
			}
		}

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<8;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_t *sdJ = json_object_get(rootJ, buf.c_str());
				if (json_real_value(sdJ)) {
					sceneData[scene][i] = json_real_value(sdJ);
				}
			}
		}
	}

	void saveScene(int scene) {
		sceneData[scene][0] = wsEnabled;
		sceneData[scene][1] = params[WARP_AMOUNT_PARAM].getValue();
		sceneData[scene][2] = params[WARP_AMOUNT_CV_ATTENUVETER_PARAM].getValue();
		sceneData[scene][3] = params[WARP_POSITION_PARAM].getValue();
		sceneData[scene][4] = params[WARP_POSITION_CV_ATTENUVETER_PARAM].getValue();
		sceneData[scene][5] = params[WARP_LENGTH_PARAM].getValue();
		sceneData[scene][6] = params[WARP_LENGTH_CV_ATTENUVETER_PARAM].getValue();
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			sceneData[scene][trackNumber+7] = trackWarpSelected[trackNumber];
		}
	}

	void loadScene(int scene) {
		wsEnabled = sceneData[scene][0];
		params[WARP_AMOUNT_PARAM].setValue(sceneData[scene][1]);
		params[WARP_AMOUNT_CV_ATTENUVETER_PARAM].setValue(sceneData[scene][2]);
		params[WARP_POSITION_PARAM].setValue(sceneData[scene][3]);
		params[WARP_POSITION_CV_ATTENUVETER_PARAM].setValue(sceneData[scene][4]);
		params[WARP_LENGTH_PARAM].setValue(sceneData[scene][5]);
		params[WARP_LENGTH_CV_ATTENUVETER_PARAM].setValue(sceneData[scene][6]);
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			trackWarpSelected[trackNumber] = sceneData[scene][trackNumber+7];
		}
	}


	void process(const ProcessArgs &args) override {
		bool isDirty = false;
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackWarpTrigger[i].process(params[TRACK_1_WARP_ENABLED_PARAM+i].getValue())) {
				trackWarpSelected[i] = !trackWarpSelected[i];
				isDirty = true;
			}
			lights[TRACK_1_WARP_ENABELED_LIGHT+i].value = trackWarpSelected[i];
		}        
		if (wsEnableTrigger.process(params[WS_ON_OFF_PARAM].getValue() + inputs[WS_ON_OFF_INPUT].getVoltage())) {
			wsEnabled = !wsEnabled;
			isDirty = true;
		}
		lights[WS_ON_LIGHT].value = wsEnabled;

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWellFormedRhythmExpander || 
								leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || 
								leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelQARIrrationalityExpander || 
								leftExpander.module->model == modelQARConditionalExpander || leftExpander.module->model == modelQARGridControlExpander ||
								leftExpander.module->model == modelQARGridControlExpander));
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
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARWellFormedRhythmExpander || 
											rightExpander.module->model == modelQARGrooveExpander || 
											rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARIrrationalityExpander || 
											rightExpander.module->model == modelQARWarpedSpaceExpander || 
											rightExpander.module->model == modelQARConditionalExpander || 
											rightExpander.module->model == modelQuadAlgorithmicRhythm ||
											rightExpander.module->model == modelQARGridControlExpander));
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
				isDirty = isDirty || QARExpanderDisconnectReset;
				QARExpanderDisconnectReset = false;
			}


            warpAmount = clamp(params[WARP_AMOUNT_PARAM].getValue() + (inputs[WARP_AMOUNT_INPUT].isConnected() ? inputs[WARP_AMOUNT_INPUT].getVoltage() * 0.6f * params[WARP_AMOUNT_CV_ATTENUVETER_PARAM].getValue() : 0.0f),1.0,6.0);
			warpAmountPercentage = (warpAmount - 1.0) / 5.0;
			if(warpAmount != lastWarpAmount) {
				isDirty = true;
				lastWarpAmount = warpAmount;
			}
            warpPosition = clamp(params[WARP_POSITION_PARAM].getValue() + (inputs[WARP_POSITION_INPUT].isConnected() ? inputs[WARP_POSITION_INPUT].getVoltage() / (MAX_STEPS / 10.0) * params[WARP_POSITION_CV_ATTENUVETER_PARAM].getValue() : 0.0f),0.0f,MAX_STEPS-1.0);
			warpPositionPercentage = warpPosition / (MAX_STEPS -1.0);
			if(warpPosition != lastWarpPosition) {
				isDirty = true;
				lastWarpPosition = warpPosition;
			}
            warpLength = clamp(params[WARP_LENGTH_PARAM].getValue() + (inputs[WARP_LENGTH_INPUT].isConnected() ? inputs[WARP_LENGTH_INPUT].getVoltage() / (MAX_STEPS / 10.0) * params[WARP_LENGTH_CV_ATTENUVETER_PARAM].getValue() : 0.0f),0.0f,MAX_STEPS-1.0);
			warpLengthPercentage = warpLength / (MAX_STEPS -1.0);
			if(warpLength != lastWarpLength) {
				isDirty = true;
				lastWarpLength = warpLength;
			}
            for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackWarpSelected[i] && wsEnabled) {
                    messagesToMother[TRACK_COUNT * 10 + i] = 1;
                    messagesToMother[TRACK_COUNT * 11 + i] = warpAmount;                    
                    messagesToMother[TRACK_COUNT * 12 + i] = warpPosition;                    
                    messagesToMother[TRACK_COUNT * 13 + i] = warpLength;                    
				} 
				messagesToMother[i] = isDirty || trackDirty[i];
			}
					
			leftExpander.module->rightExpander.messageFlipRequested = true;
		
		} else {
			warpAmountPercentage = 0;
			warpPositionPercentage = 0;
			warpLengthPercentage = 0;			
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {

		for(int i =0;i<TRACK_COUNT;i++) {
			trackWarpSelected[i] = true;
		}
	}
};



struct QARWarpedSpaceExpanderDisplay : TransparentWidget {
	QARWarpedSpaceExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;
	std::string fontPath;

	QARWarpedSpaceExpanderDisplay() {
		fontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
	}

	void drawWarp(const DrawArgs &args, Vec pos, float warp) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %1.2f", warp);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
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


	void draw(const DrawArgs &args) override {
		font = APP->window->loadFont(fontPath);

		if (!module)
			return; 		

		drawWarp(args, Vec(97, 78), module->warpAmount);
		drawStartPosition(args, Vec(97, 158), module->warpPosition);
		drawLength(args, Vec(97, 238), module->warpLength);
	}

};

struct QARWarpedSpaceExpanderWidget : ModuleWidget {
	QARWarpedSpaceExpanderWidget(QARWarpedSpaceExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARWarpedSpaceExpander.svg")));

		{
			QARWarpedSpaceExpanderDisplay *display = new QARWarpedSpaceExpanderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}


		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	
		addParam(createParam<LEDButton>(Vec(52, 294), module, QARWarpedSpaceExpander::WS_ON_OFF_PARAM));
        addChild(createLight<LargeLight<GreenLight>>(Vec(53.5, 295.5), module, QARWarpedSpaceExpander::WS_ON_LIGHT));
        addInput(createInput<FWPortInSmall>(Vec(77, 294), module, QARWarpedSpaceExpander::WS_ON_OFF_INPUT));

         for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(7 + i*24, 333), module, QARWarpedSpaceExpander::TRACK_1_WARP_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(8.5 + i*24, 334.5), module, QARWarpedSpaceExpander::TRACK_1_WARP_ENABELED_LIGHT + i));
		}


		ParamWidget* warpAmountParam = createParam<RoundFWKnob>(Vec(12, 59), module, QARWarpedSpaceExpander::WARP_AMOUNT_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(warpAmountParam)->percentage = &module->warpAmountPercentage;
		}
		addParam(warpAmountParam);							
        addInput(createInput<FWPortInSmall>(Vec(47, 64), module, QARWarpedSpaceExpander::WARP_AMOUNT_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(44, 87), module, QARWarpedSpaceExpander::WARP_AMOUNT_CV_ATTENUVETER_PARAM));

		ParamWidget* warpPositionParam = createParam<RoundFWSnapKnob>(Vec(12, 139), module, QARWarpedSpaceExpander::WARP_POSITION_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(warpPositionParam)->percentage = &module->warpPositionPercentage;
		}
		addParam(warpPositionParam);							
        addInput(createInput<FWPortInSmall>(Vec(47, 144), module, QARWarpedSpaceExpander::WARP_POSITION_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(44, 167), module, QARWarpedSpaceExpander::WARP_POSITION_CV_ATTENUVETER_PARAM));

		ParamWidget* warpLengthParam = createParam<RoundFWSnapKnob>(Vec(12, 219), module, QARWarpedSpaceExpander::WARP_LENGTH_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(warpLengthParam)->percentage = &module->warpLengthPercentage;
		}
		addParam(warpLengthParam);							
        addInput(createInput<FWPortInSmall>(Vec(47, 224), module, QARWarpedSpaceExpander::WARP_LENGTH_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(44, 247), module, QARWarpedSpaceExpander::WARP_LENGTH_CV_ATTENUVETER_PARAM));

    
	}
};

Model *modelQARWarpedSpaceExpander = createModel<QARWarpedSpaceExpander, QARWarpedSpaceExpanderWidget>("QARWarpedSpaceExpander");
 