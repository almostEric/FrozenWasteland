#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 12
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * 3 + TRACK_LEVEL_PARAM_COUNT


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
		NUM_PARAMS
	};

	enum InputIds {
        WARP_AMOUNT_INPUT,
        WARP_POSITION_INPUT,
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
		NUM_LIGHTS 
	};



	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};

    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	
	dsp::SchmittTrigger trackWarpTrigger[TRACK_COUNT];
	bool trackWarpSelected[TRACK_COUNT];

	
	QARWarpedSpaceExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		        
        configParam(WARP_AMOUNT_PARAM, 1.0f, 6.0, 1.0,"Warp Amount");
        configParam(WARP_AMOUNT_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Amount CV Attenuation","%",0,100);

        configParam(WARP_POSITION_PARAM, 0.0, MAX_STEPS-1, 0,"Warp Position");
        configParam(WARP_POSITION_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Position CV Attenuation","%",0,100);		

        
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackWarpActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackWarpSelected[i]));			
		}
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
			
		for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "trackWarpActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
			    trackWarpSelected[i] = json_integer_value(sumJ);			
			}
		}
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackWarpTrigger[i].process(params[TRACK_1_WARP_ENABLED_PARAM+i].getValue())) {
				trackWarpSelected[i] = !trackWarpSelected[i];
			}
			lights[TRACK_1_WARP_ENABELED_LIGHT+i].value = trackWarpSelected[i];
		}        

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARAdvancedRhythmsExpander || leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelPWAlgorithmicExpander));
		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			// To Mother
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;


//USE MEMSSET!
			//Initalize
			for (int i = 0; i < PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT; i++) {
                messagesToMother[i] = 0.0;
			}

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARAdvancedRhythmsExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                if(rightExpander.module->model != modelQuadAlgorithmicRhythm) { // Get QRE values							
					for(int i = 0; i < PASSTHROUGH_OFFSET; i++) {
                        messagesToMother[i] = messagesFromExpander[i];
					}
				}


//USE MEMCPY!
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


            float warpAmount = clamp(params[WARP_AMOUNT_PARAM].getValue() + (inputs[WARP_AMOUNT_INPUT].isConnected() ? inputs[WARP_AMOUNT_INPUT].getVoltage() * 0.6f * params[WARP_AMOUNT_CV_ATTENUVETER_PARAM].getValue() : 0.0f),1.0,6.0);
            float warpPosition = clamp(params[WARP_POSITION_PARAM].getValue() + (inputs[WARP_POSITION_INPUT].isConnected() ? inputs[WARP_POSITION_INPUT].getVoltage() / 1.8 * params[WARP_POSITION_CV_ATTENUVETER_PARAM].getValue() : 0.0f),0.0f,17.0);
            for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackWarpSelected[i]) {
                    messagesToMother[TRACK_COUNT * 9 + i] = 1;
                    messagesToMother[TRACK_COUNT * 10 + i] = warpAmount;                    
                    messagesToMother[TRACK_COUNT * 11 + i] = warpPosition;                    
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
			trackWarpSelected[i] = true;
		}
	}
};



struct QARWarpedSpaceExpanderDisplay : TransparentWidget {
	QARWarpedSpaceExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	QARWarpedSpaceExpanderDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}



	


	void draw(const DrawArgs &args) override {
		if (!module)
			return; 		
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


	
        

         for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(7 + i*24, 298), module, QARWarpedSpaceExpander::TRACK_1_WARP_ENABLED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(8.5 + i*24, 299.5), module, QARWarpedSpaceExpander::TRACK_1_WARP_ENABELED_LIGHT + i));
		}


        addParam(createParam<RoundFWKnob>(Vec(22, 59), module, QARWarpedSpaceExpander::WARP_AMOUNT_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(57, 64), module, QARWarpedSpaceExpander::WARP_AMOUNT_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(54, 87), module, QARWarpedSpaceExpander::WARP_AMOUNT_CV_ATTENUVETER_PARAM));

        addParam(createParam<RoundFWSnapKnob>(Vec(22, 159), module, QARWarpedSpaceExpander::WARP_POSITION_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(57, 164), module, QARWarpedSpaceExpander::WARP_POSITION_INPUT));
        addParam(createParam<RoundSmallFWKnob>(Vec(54, 187), module, QARWarpedSpaceExpander::WARP_POSITION_CV_ATTENUVETER_PARAM));

    
	}
};

Model *modelQARWarpedSpaceExpander = createModel<QARWarpedSpaceExpander, QARWarpedSpaceExpanderWidget>("QARWarpedSpaceExpander");
 