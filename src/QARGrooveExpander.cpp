#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/buttons.hpp"


#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 4
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 12
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT


struct QARGrooveExpander : Module {

    enum ParamIds {
		TRACK_1_GROOVE_ENABLED_PARAM,
		TRACK_2_GROOVE_ENABLED_PARAM,
		TRACK_3_GROOVE_ENABLED_PARAM,
		TRACK_4_GROOVE_ENABLED_PARAM,
		GROOVE_LENGTH_SAME_AS_TRACK_PARAM,
        GROOVE_LENGTH_PARAM,
        GROOVE_LENGTH_CV_PARAM,
        GROOVE_AMOUNT_PARAM,
        GROOVE_AMOUNT_CV_PARAM,
        SWING_RANDOMNESS_PARAM,
        SWING_RANDOMNESS_CV_PARAM,
		RANDOM_DISTRIBUTION_PATTERN_PARAM,
		STEP_1_SWING_AMOUNT_PARAM,
		STEP_1_SWING_CV_ATTEN_PARAM = STEP_1_SWING_AMOUNT_PARAM + MAX_STEPS,
        STEP_OR_DIV_PARAM = STEP_1_SWING_CV_ATTEN_PARAM + MAX_STEPS,
		NUM_PARAMS
	};

	enum InputIds {
        GROOVE_LENGTH_INPUT,
        GROOVE_AMOUNT_INPUT,
        SWING_RANDOMNESS_INPUT,
		STEP_1_SWING_AMOUNT_INPUT,
		NUM_INPUTS = STEP_1_SWING_AMOUNT_INPUT + MAX_STEPS
	};

	enum OutputIds {
		NUM_OUTPUTS
	};

	enum LightIds {
		CONNECTED_LIGHT,
		TRACK_1_GROOVE_ENABELED_LIGHT,
		TRACK_2_GROOVE_ENABELED_LIGHT,
		TRACK_3_GROOVE_ENABELED_LIGHT,
		TRACK_4_GROOVE_ENABELED_LIGHT,
        USING_DIVS_LIGHT,
        GROOVE_IS_TRACK_LENGTH_LIGHT,
        GAUSSIAN_DISTRIBUTION_LIGHT,
		NUM_LIGHTS 
	};



	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

	float grooveLength;

	float sceneData[NBR_SCENES][49] = {{0}};
	int sceneChangeMessage = 0;


    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	
	dsp::SchmittTrigger stepDivTrigger,grooveLengthTrigger,randomDistributionTrigger,trackGrooveTrigger[TRACK_COUNT];
	bool trackGrooveSelected[TRACK_COUNT];
    bool stepsOrDivs, grooveIsTrackLength,gaussianDistribution;

    int stepGrooveType[TRACK_COUNT] = {0};
    float grooveAmount[TRACK_COUNT] = {1.0f};
	
	QARGrooveExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
        for (int i = 0; i < MAX_STEPS; i++) {
            configParam(STEP_1_SWING_AMOUNT_PARAM + i, -0.5f, 0.5f, 0.0f,"Step "  + std::to_string(i+1) + " Swing","%",0,100);
			configParam(STEP_1_SWING_CV_ATTEN_PARAM + i, -1.0, 1.0, 0.0,"Step "  + std::to_string(i+1) + " Swing CV Attenuation","%",0,100);
		}	
        
        configParam(GROOVE_LENGTH_PARAM, 1.0f, MAX_STEPS, MAX_STEPS,"Groove Length");
        configParam(GROOVE_LENGTH_CV_PARAM, -1.0, 1.0, 0.0,"Groove Length CV Attenuation","%",0,100);

        configParam(GROOVE_AMOUNT_PARAM, 0.0, 1.0, 1.0,"Groove Amount","%",0,100);
        configParam(GROOVE_AMOUNT_CV_PARAM, -1.0, 1.0, 0.0,"Groove Amount CV Attenuation","%",0,100);		

        configParam(SWING_RANDOMNESS_PARAM, 0.0, 1.0, 0.0,"Groove Randomness","%",0,100);
        configParam(SWING_RANDOMNESS_CV_PARAM, -1.0, 1.0, 0.0,"Groove Randomness CV Attenuation","%",0,100);		

        configParam(STEP_OR_DIV_PARAM, 0.0, 1.0, 0.0);
		configParam(GROOVE_LENGTH_SAME_AS_TRACK_PARAM, 0.0, 1.0, 0.0);
		configParam(RANDOM_DISTRIBUTION_PATTERN_PARAM, 0.0, 1.0, 0.0);

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

        json_object_set_new(rootJ, "stepsOrDivs", json_integer((bool) stepsOrDivs));
        json_object_set_new(rootJ, "grooveIsTrackLength", json_integer((bool) grooveIsTrackLength));
        json_object_set_new(rootJ, "gaussianDistribution", json_integer((bool) gaussianDistribution));

		
		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackGrooveActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackGrooveSelected[i]));			
		}

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<49;i++) {
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

        json_t *glJ = json_object_get(rootJ, "grooveIsTrackLength");
		if (glJ)
			grooveIsTrackLength = json_integer_value(glJ);

        json_t *gdJ = json_object_get(rootJ, "gaussianDistribution");
		if (gdJ)
			gaussianDistribution = json_integer_value(gdJ);

			
		for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "trackGrooveActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
			    trackGrooveSelected[i] = json_integer_value(sumJ);			
			}
		}

		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<49;i++) {
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
		sceneData[scene][1] = grooveIsTrackLength;
		sceneData[scene][2] = gaussianDistribution;			
		sceneData[scene][3] = params[GROOVE_LENGTH_PARAM].getValue();
		sceneData[scene][4] = params[GROOVE_AMOUNT_PARAM].getValue();
		sceneData[scene][5] = params[SWING_RANDOMNESS_PARAM].getValue();
		sceneData[scene][6] = params[GROOVE_LENGTH_CV_PARAM].getValue();
		sceneData[scene][7] = params[GROOVE_AMOUNT_CV_PARAM].getValue();
		sceneData[scene][8] = params[SWING_RANDOMNESS_CV_PARAM].getValue();
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			sceneData[scene][trackNumber+9] = trackGrooveSelected[trackNumber];
		}
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			sceneData[scene][stepNumber+12] = params[STEP_1_SWING_AMOUNT_PARAM+stepNumber].getValue();
			sceneData[scene][stepNumber+30] = params[STEP_1_SWING_AMOUNT_PARAM+stepNumber].getValue();
		}
	}

	void loadScene(int scene) {
		stepsOrDivs = sceneData[scene][0];
		grooveIsTrackLength = sceneData[scene][1];
		gaussianDistribution = sceneData[scene][2];
		params[GROOVE_LENGTH_PARAM].setValue(sceneData[scene][3]);
		params[GROOVE_AMOUNT_PARAM].setValue(sceneData[scene][4]);
		params[SWING_RANDOMNESS_PARAM].setValue(sceneData[scene][5]);
		params[GROOVE_LENGTH_CV_PARAM].setValue(sceneData[scene][6]);
		params[GROOVE_AMOUNT_CV_PARAM].setValue(sceneData[scene][7]);
		params[SWING_RANDOMNESS_CV_PARAM].setValue(sceneData[scene][8]);
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			trackGrooveSelected[trackNumber] = sceneData[scene][trackNumber+9];
		}
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			 params[STEP_1_SWING_AMOUNT_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+12]);
			 params[STEP_1_SWING_CV_ATTEN_PARAM+stepNumber].setValue(sceneData[scene][stepNumber+30]);
		}
	}


	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackGrooveTrigger[i].process(params[TRACK_1_GROOVE_ENABLED_PARAM+i].getValue())) {
				trackGrooveSelected[i] = !trackGrooveSelected[i];
			}
			lights[TRACK_1_GROOVE_ENABELED_LIGHT+i].value = trackGrooveSelected[i];
		}

	    if (stepDivTrigger.process(params[STEP_OR_DIV_PARAM].getValue())) {
            stepsOrDivs = !stepsOrDivs;
        }
        lights[USING_DIVS_LIGHT].value = stepsOrDivs;

	    if (grooveLengthTrigger.process(params[GROOVE_LENGTH_SAME_AS_TRACK_PARAM].getValue())) {
            grooveIsTrackLength = !grooveIsTrackLength;
        }
        lights[GROOVE_IS_TRACK_LENGTH_LIGHT].value = grooveIsTrackLength;

	    if (randomDistributionTrigger.process(params[RANDOM_DISTRIBUTION_PATTERN_PARAM].getValue())) {
             gaussianDistribution = !gaussianDistribution;
        }
        lights[GAUSSIAN_DISTRIBUTION_LIGHT].value = gaussianDistribution;


        

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


            grooveLength = clamp(params[GROOVE_LENGTH_PARAM].getValue() + (inputs[GROOVE_LENGTH_INPUT].isConnected() ? inputs[GROOVE_LENGTH_INPUT].getVoltage() * 1.8f * params[GROOVE_LENGTH_CV_PARAM].getValue() : 0.0f),1.0,18.0f);
            float grooveAmount = clamp(params[GROOVE_AMOUNT_PARAM].getValue() + (inputs[GROOVE_AMOUNT_INPUT].isConnected() ? inputs[GROOVE_AMOUNT_INPUT].getVoltage() / 10 * params[GROOVE_AMOUNT_CV_PARAM].getValue() : 0.0f),0.0,1.0f);
            float randomAmount = clamp(params[SWING_RANDOMNESS_PARAM].getValue() + (inputs[SWING_RANDOMNESS_INPUT].isConnected() ? inputs[SWING_RANDOMNESS_INPUT].getVoltage() / 10 * params[SWING_RANDOMNESS_CV_PARAM].getValue() : 0.0f),0.0,1.0f);
            for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackGrooveSelected[i]) {
                    messagesToMother[TRACK_COUNT * 4 + i] = stepsOrDivs ? 2 : 1;
                    messagesToMother[TRACK_COUNT * 5 + i] = grooveLength;
                    messagesToMother[TRACK_COUNT * 6 + i] = grooveIsTrackLength;
                    messagesToMother[TRACK_COUNT * 7 + i] = randomAmount;
                    messagesToMother[TRACK_COUNT * 8 + i] = gaussianDistribution;
                    
    				for (int j = 0; j < MAX_STEPS; j++) {
                        float initialSwingAmount = clamp(params[STEP_1_SWING_AMOUNT_PARAM+j].getValue() + (inputs[STEP_1_SWING_AMOUNT_INPUT + j].isConnected() ? inputs[STEP_1_SWING_AMOUNT_INPUT + j].getVoltage() / 10 * params[STEP_1_SWING_CV_ATTEN_PARAM + j].getValue() : 0.0f),-0.5,0.5f);
						messagesToMother[TRACK_LEVEL_PARAM_COUNT + (MAX_STEPS * TRACK_COUNT * 2) + (i * MAX_STEPS) + j] = lerp(0,initialSwingAmount,grooveAmount);
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
        grooveIsTrackLength = false;
        gaussianDistribution = false;

		for(int i =0;i<TRACK_COUNT;i++) {
			trackGrooveSelected[i] = true;
		}
	}
};



struct QARGrooveExpanderDisplay : TransparentWidget {
	QARGrooveExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	QARGrooveExpanderDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}



	void drawActiveGrooveSteps(const DrawArgs &args, int grooveLength) 
	{		


		// Draw indicator for upper Ajnas
		nvgStrokeColor(args.vg, nvgRGBA(0x2d, 0xc3, 0xff, 0xff));
		nvgStrokeWidth(args.vg, 2);
		for(int i = 0; i<grooveLength;i++) {

			float x= (i / 6) * 65 + 15.0;
			float y= (i % 6) * 45 + 51.0;

			nvgBeginPath(args.vg);
			nvgCircle(args.vg,x,y,6.0);
			nvgClosePath(args.vg);		
			nvgStroke(args.vg);
		}

	}


	void draw(const DrawArgs &args) override {
		if (!module)
			return; 


		drawActiveGrooveSteps(args,(int)(module->grooveLength));
	}
};

struct QARGrooveExpanderWidget : ModuleWidget {
	QARGrooveExpanderWidget(QARGrooveExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARGrooveExpander.svg")));

		{
			QARGrooveExpanderDisplay *display = new QARGrooveExpanderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}


		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		


         for(int i=0; i<MAX_STEPS / 3;i++) {
		    addParam(createParam<RoundSmallFWKnob>(Vec(22, 30 + i*45), module, QARGrooveExpander::STEP_1_SWING_AMOUNT_PARAM + i));
		    addParam(createParam<RoundSmallFWKnob>(Vec(87, 30 + i*45), module, QARGrooveExpander::STEP_1_SWING_AMOUNT_PARAM + i + (MAX_STEPS / 3)));
		    addParam(createParam<RoundSmallFWKnob>(Vec(152, 30 + i*45), module, QARGrooveExpander::STEP_1_SWING_AMOUNT_PARAM + i + (MAX_STEPS * 2 / 3)));

		    addParam(createParam<RoundReallySmallFWKnob>(Vec(46, 48 + i*45), module, QARGrooveExpander::STEP_1_SWING_CV_ATTEN_PARAM + i));
		    addParam(createParam<RoundReallySmallFWKnob>(Vec(111, 48 + i*45), module, QARGrooveExpander::STEP_1_SWING_CV_ATTEN_PARAM + i + (MAX_STEPS / 3)));
		    addParam(createParam<RoundReallySmallFWKnob>(Vec(176, 48 + i*45), module, QARGrooveExpander::STEP_1_SWING_CV_ATTEN_PARAM + i + (MAX_STEPS * 2 / 3)));
    
    		addInput(createInput<FWPortInReallySmall>(Vec(50, 33 + i*45), module, QARGrooveExpander::STEP_1_SWING_AMOUNT_INPUT + i));
    		addInput(createInput<FWPortInReallySmall>(Vec(115, 33 + i*45), module, QARGrooveExpander::STEP_1_SWING_AMOUNT_INPUT + i + (MAX_STEPS / 3)));
    		addInput(createInput<FWPortInReallySmall>(Vec(180, 33 + i*45), module, QARGrooveExpander::STEP_1_SWING_AMOUNT_INPUT + i + (MAX_STEPS * 2 / 3)));

        }

         for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(75 + i*24, 296), module, QARGrooveExpander::TRACK_1_GROOVE_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(76.5 + i*24, 298.5), module, QARGrooveExpander::TRACK_1_GROOVE_ENABELED_LIGHT + i));

		}

        addParam(createParam<LEDButton>(Vec(12, 312), module, QARGrooveExpander::STEP_OR_DIV_PARAM));

        addParam(createParam<LEDButton>(Vec(26, 341), module, QARGrooveExpander::GROOVE_LENGTH_SAME_AS_TRACK_PARAM));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(42, 326), module, QARGrooveExpander::GROOVE_LENGTH_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(69, 342), module, QARGrooveExpander::GROOVE_LENGTH_CV_PARAM));

        addParam(createParam<RoundSmallFWKnob>(Vec(100, 326), module, QARGrooveExpander::GROOVE_AMOUNT_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(127, 342), module, QARGrooveExpander::GROOVE_AMOUNT_CV_PARAM));

        addParam(createParam<LEDButton>(Vec(175, 351), module, QARGrooveExpander::RANDOM_DISTRIBUTION_PATTERN_PARAM));

        addParam(createParam<RoundSmallFWKnob>(Vec(167, 326), module, QARGrooveExpander::SWING_RANDOMNESS_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(194, 342), module, QARGrooveExpander::SWING_RANDOMNESS_CV_PARAM));
	
	   

        addInput(createInput<FWPortInSmall>(Vec(70, 319), module, QARGrooveExpander::GROOVE_LENGTH_INPUT));
        addInput(createInput<FWPortInSmall>(Vec(128, 319), module, QARGrooveExpander::GROOVE_AMOUNT_INPUT));
        addInput(createInput<FWPortInSmall>(Vec(195, 319), module, QARGrooveExpander::SWING_RANDOMNESS_INPUT));


        addChild(createLight<LargeLight<BlueLight>>(Vec(13.5, 313.5), module, QARGrooveExpander::USING_DIVS_LIGHT));
        addChild(createLight<LargeLight<GreenLight>>(Vec(27.5, 342.5), module, QARGrooveExpander::GROOVE_IS_TRACK_LENGTH_LIGHT));
        addChild(createLight<LargeLight<GreenLight>>(Vec(176.5, 352.5), module, QARGrooveExpander::GAUSSIAN_DISTRIBUTION_LIGHT));

        

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		


	}
};

Model *modelQARGrooveExpander = createModel<QARGrooveExpander, QARGrooveExpanderWidget>("QARGrooveExpander");
 