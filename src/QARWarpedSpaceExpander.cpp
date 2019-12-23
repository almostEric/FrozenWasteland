#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 9
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
	float consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_OFFSET + 1 + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here

    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	
	dsp::SchmittTrigger trackWarpTrigger[TRACK_COUNT];
	bool trackWarpSelected[TRACK_COUNT];

	
	QARWarpedSpaceExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		        
        configParam(WARP_AMOUNT_PARAM, 1.0f, 6.0, 1.0,"Warp Amount");
        configParam(WARP_POSITION_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Amount CV Attenuation","%",0,100);

        configParam(WARP_POSITION_PARAM, 0.0, MAX_STEPS-1, 0,"Warp Position");
        configParam(WARP_POSITION_CV_ATTENUVETER_PARAM, -1.0, 1.0, 0.0,"Warp Position CV Attenuation","%",0,100);		

        
		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;

		
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

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || leftExpander.module->model == modelQARWarpedSpaceExpander));
		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			// To Mother
			float *producerMessage = (float*) leftExpander.producerMessage;

			//Initalize
			for (int i = 0; i < PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT; i++) {
                producerMessage[i] = 0.0;
			}

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *message = (float*) rightExpander.module->leftExpander.consumerMessage;	

                if(rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARWarpedSpaceExpander ) { // Get QRE values							
					for(int i = 0; i < PASSTHROUGH_OFFSET; i++) {
                        producerMessage[i] = message[i];
					}
				}

				//QAR Pass through left
				for(int i = 0; i < PASSTHROUGH_LEFT_VARIABLE_COUNT; i++) {
					producerMessage[PASSTHROUGH_OFFSET + i] = message[PASSTHROUGH_OFFSET + i];
				}

				//QAR Pass through right
				float *messagesFromMother = (float*)leftExpander.consumerMessage;
				float *messageToSlave = (float*)(rightExpander.module->leftExpander.producerMessage);	
				for(int i = 0; i < PASSTHROUGH_RIGHT_VARIABLE_COUNT;i++) {
					messageToSlave[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + i] = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + i];
				}	
			}


            float warpAmount = clamp(params[WARP_AMOUNT_PARAM].getValue() + (inputs[WARP_AMOUNT_INPUT].isConnected() ? inputs[WARP_AMOUNT_INPUT].getVoltage() * 0.6f * params[WARP_AMOUNT_CV_ATTENUVETER_PARAM].getValue() : 0.0f),1.0,6.0);
            float warpPosition = clamp(params[WARP_POSITION_PARAM].getValue() + (inputs[WARP_POSITION_INPUT].isConnected() ? inputs[WARP_POSITION_INPUT].getVoltage() / 1.8 * params[WARP_POSITION_CV_ATTENUVETER_PARAM].getValue() : 0.0f),0.0f,17.0);
            for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackWarpSelected[i]) {
                    producerMessage[TRACK_COUNT * 6 + i] = 1;
                    producerMessage[TRACK_COUNT * 7 + i] = warpAmount;                    
                    producerMessage[TRACK_COUNT * 8 + i] = warpPosition;                    
				} 
			}
		
			// From Mother	
			
			
			leftExpander.messageFlipRequested = true;
		
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {

		for(int i =0;i<TRACK_COUNT;i++) {
            //params[SWING_1_PARAM+i].setValue(0);
			trackWarpSelected[i] = true;
		}

		for(int i = 0; i < PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT; i++) {
			producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0;
			consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0;
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

        
        

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		


	}
};

Model *modelQARWarpedSpaceExpander = createModel<QARWarpedSpaceExpander, QARWarpedSpaceExpanderWidget>("QARWarpedSpaceExpander");
 