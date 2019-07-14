#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_TAPS 16
#define NUM_GROOVES 14
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * 2 + 1

struct QuadGrooveExpander : Module {

    enum ParamIds {
		TRACK_1_GROOVE_ENABLED_PARAM,
		TRACK_2_GROOVE_ENABLED_PARAM,
		TRACK_3_GROOVE_ENABLED_PARAM,
		TRACK_4_GROOVE_ENABLED_PARAM,
		GROOVE_TYPE_1_PARAM,
		GROOVE_TYPE_2_PARAM,
		GROOVE_TYPE_3_PARAM,
		GROOVE_TYPE_4_PARAM,
		GROOVE_TYPE_ATTEN_1_PARAM,
		GROOVE_TYPE_ATTEN_2_PARAM,
		GROOVE_TYPE_ATTEN_3_PARAM,
		GROOVE_TYPE_ATTEN_4_PARAM,
		GROOVE_AMOUNT_1_PARAM,
		GROOVE_AMOUNT_2_PARAM,
		GROOVE_AMOUNT_3_PARAM,
		GROOVE_AMOUNT_4_PARAM,
		GROOVE_AMOUNT_ATTEN_1_PARAM,
		GROOVE_AMOUNT_ATTEN_2_PARAM,
		GROOVE_AMOUNT_ATTEN_3_PARAM,
		GROOVE_AMOUNT_ATTEN_4_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		GROOVE_TYPE_1_INPUT,
		GROOVE_TYPE_2_INPUT,
		GROOVE_TYPE_3_INPUT,
		GROOVE_TYPE_4_INPUT,
		GROOVE_AMOUNT_1_INPUT,
		GROOVE_AMOUNT_2_INPUT,
		GROOVE_AMOUNT_3_INPUT,
		GROOVE_AMOUNT_4_INPUT,
		NUM_INPUTS
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
		NUM_LIGHTS 
	};



	const char* stepNames[MAX_STEPS] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18"};
    const char* grooveNames[NUM_GROOVES] = {"Straight","Swing","Hard Swing","Reverse Swing","Alternate Swing","Accelerando","Ritardando","Waltz Time","Half Swing","Roller Coaster","Quintuple","Random 1","Random 2","Random 3"};

	const float stepGroovePatterns[NUM_GROOVES][NUM_TAPS] = {
		{0.0f,0.0f,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0f}, // Straight time
		{0.25,0.0,0.25,0.0,0.25,0.0,0.25,0.0,0.25,0.0,0.25,0.0,0.25,0.0,0.25,0.0}, // Swing
		{0.50,0.0,0.50,0.0,0.50,0.0,0.50,0.0,0.50,0.0,0.50,0.0,0.50,0.0,0.50,0.0}, // Hard Swing
		{-0.50,0.0,-0.50,0.0,-0.50,0.0,-0.50,0.0,-0.50,0.0,-0.50,0.0,-0.50,0.0,-0.50,0.0}, // Reverse Swing
		{0.25,0.0,0.0,0.0,0.25,0.0,0.0,0.0,0.25,0.0,0.0,0.0,0.25,0.0,0.0,0.0}, // Alternate Swing
		{0.5,0.46875,0.4375,0.40625,0.375,0.34375,0.3125,0.28125,0.25,0.21875,0.1875,0.15625,0.125,0.09375,0.0625,0.03125}, // Accelerando
		{0.0,0.03125,0.0625,0.09375,0.125,0.15625,0.1875,0.21875,0.25,0.28125,0.3125,0.34375,0.375,0.40625,0.4375,0.46875}, // Ritardando
		{0.25,0.75,0.25,0.0,0.25,0.75,0.25,0.0,0.25,0.75,0.25,0.0,0.25,0.75,0.25,0.0}, // Waltz Time
		{0.5,0.0,0.5,0.0,0.0,0.0,0.0,0.0,0.5,0.0,0.5,0.0,0.0,0.0,0.0,0.0}, // Half Swing
		{0.0,0.0,0.5,0.0,0.0,0.5,0.5,0.5,0,0.0,0.5,-0.5,0.5,-0.5,0.5,-0.5}, // Roller Coaster
		{0.75,0.5,0.25,0.0,0.75,0.5,0.25,0.0,0.75,0.5,0.25,0.0,0.75,0.5,0.25,0.0}, // Quintuple
		{0.25,0.75,0.0,0.25,0.0,0.5,0.25,0.5,0.0,0.25,0.0,0.0,0.5,0.0,0.75,0.0}, // Uniform Random 1
		{0.25,0.75,0.25,0.5,0.0,0.0,0.5,0.75,0.0,0.25,0.75,0.75,0.0,0.25,0.75,0.5}, // Uniform Random 2
		{0.75,0.0,0.25,0.75,0.25,0.5,0.75,0.5,0.75,0.5,0.75,0.0,0.75,0.0,0.5,0.0}, // Uniform Random 3
	};

	// Expander
	float consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_OFFSET + 1 + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here

    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	
	dsp::SchmittTrigger trackGrooveTrigger[TRACK_COUNT];
	bool trackGrooveSelected[TRACK_COUNT];

    int stepGrooveType[TRACK_COUNT] = {0};
    float grooveAmount[TRACK_COUNT] = {1.0f};
	
	QuadGrooveExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
        for (int i = 0; i < TRACK_COUNT; i++) {
            configParam(GROOVE_TYPE_1_PARAM + i, 0.0f, 13.0f, 0.0f,"Track "  + std::to_string(i+1) + " Groove Type");
            configParam(GROOVE_TYPE_ATTEN_1_PARAM + i, -1.0, 1.0, 0.0,"Track " +  std::to_string(i+1) + " Groove Type CV Attenuation","%",0,100);

            configParam(GROOVE_AMOUNT_1_PARAM + i, 0.0f, 1.0f, 1.0f,"Track " +  std::to_string(i+1) +" Groove Amount","%",0,100);
		    configParam(GROOVE_AMOUNT_ATTEN_1_PARAM + i, -1.0, 1.0, 0.0,"Track " +  std::to_string(i+1) +"  Groove Amount CV Attenuation","%",0,100);		
		}

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;

		
        onReset();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		for(int i=0;i<TRACK_COUNT;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "trackGrooveActive");
			strcat(buf, stepNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) trackGrooveSelected[i]));			
		}
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
			
		for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "trackGrooveActive");
			strcat(buf, stepNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				trackGrooveSelected[i] = json_integer_value(sumJ);			
			}
		}
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {
			if (trackGrooveTrigger[i].process(params[TRACK_1_GROOVE_ENABLED_PARAM+i].getValue())) {
				trackGrooveSelected[i] = !trackGrooveSelected[i];
			}
			lights[TRACK_1_GROOVE_ENABELED_LIGHT+i].value = trackGrooveSelected[i];

            stepGrooveType[i] = (int)clamp(params[GROOVE_TYPE_1_PARAM + i].getValue() + (inputs[GROOVE_TYPE_1_INPUT + i].isConnected() ?  inputs[GROOVE_TYPE_1_INPUT + i].getVoltage() * 1.3f * params[GROOVE_TYPE_ATTEN_1_PARAM + i].getValue() : 0.0f),0.0f,13.0);
            grooveAmount[i] = clamp(params[GROOVE_AMOUNT_1_PARAM + i].getValue() + (inputs[GROOVE_AMOUNT_1_INPUT + i].isConnected() ? inputs[GROOVE_AMOUNT_1_INPUT + i].getVoltage() / 10.0f * params[GROOVE_AMOUNT_ATTEN_1_PARAM + i].getValue() : 0.0f),0.0f,1.0f);        
		}


		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQuadRhythmExpander || leftExpander.module->model == modelQuadGrooveExpander));
		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			// To Mother
			float *producerMessage = (float*) leftExpander.producerMessage;

            producerMessage[0] = 1; // Always Step Mode

			//Initalize
			for (int i = 0; i < TRACK_COUNT; i++) {
				for (int j = 0; j < MAX_STEPS; j++) {
					producerMessage[1 + i * MAX_STEPS + j] = 1.0;
					producerMessage[1 + (i + TRACK_COUNT) * MAX_STEPS + j] = 0.0;
				}
			}
			for(int i = 0; i < PASSTHROUGH_LEFT_VARIABLE_COUNT; i++) {
				producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0.0;
			}

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQuadGrooveExpander || rightExpander.module->model == modelQuadRhythmExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *message = (float*) rightExpander.module->leftExpander.consumerMessage;	

				if(rightExpander.module->model == modelQuadGrooveExpander || rightExpander.module->model == modelQuadRhythmExpander) { // Get QRE values							
					for(int i = 0; i < TRACK_COUNT; i++) {
						for(int j = 0; j < MAX_STEPS; j++) { // Assign probabilites and swing
							producerMessage[1 + i * MAX_STEPS + j] = message[1 + i * MAX_STEPS + j];
							producerMessage[1 + (i + TRACK_COUNT) * MAX_STEPS + j] = message[1 + (i + TRACK_COUNT) * MAX_STEPS  + j];
						} 
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
		
			for (int i = 0; i < TRACK_COUNT; i++) {
                if(trackGrooveSelected[i]) {
                    for (int j = 0; j < 16; j++) {  //Only 16 steps defined for now

                        float swing = lerp(stepGroovePatterns[0][j],stepGroovePatterns[stepGrooveType[i]][j],grooveAmount[i]); //Balance between straight time and groove
			
                        producerMessage[1 + (i + TRACK_COUNT) * MAX_STEPS + j] = swing;
                    }
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
			trackGrooveSelected[i] = true;
		}

		for(int i = 0; i < PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT; i++) {
			producerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0;
			consumerMessage[MAX_STEPS * TRACK_COUNT * 2 + 1 + i] = 0;
		}
	}
};

struct QGEStatusDisplay : TransparentWidget {
	QuadGrooveExpander *module;
	int frame = 0;
	std::shared_ptr<Font> fontNumbers,fontText;

	

	QGEStatusDisplay() {
		fontNumbers = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
		fontText = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}

	
	void drawGrooveType(const DrawArgs &args, Vec pos, int *grooveType) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
        for(int i = 0; i<TRACK_COUNT; i++) {
            char text[128];
            snprintf(text, sizeof(text), "%s", module->grooveNames[grooveType[i]]);
            nvgText(args.vg, pos.x, pos.y + i * 78, text, NULL);
        }
	}

	

	void draw(const DrawArgs &args) override {
		if (!module)
			return;		
        drawGrooveType(args, Vec(37,61), module->stepGrooveType);
	}
};

struct QuadGrooveExpanderWidget : ModuleWidget {
	QuadGrooveExpanderWidget(QuadGrooveExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadGrooveExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        {
			QGEStatusDisplay *display = new QGEStatusDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 385);
			addChild(display);
		}
	
		addParam(createParam<RoundFWSnapKnob>(Vec(32, 70), module, QuadGrooveExpander::GROOVE_TYPE_1_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(70, 101), module, QuadGrooveExpander::GROOVE_TYPE_ATTEN_1_PARAM));
		addParam(createParam<RoundFWSnapKnob>(Vec(32, 148), module, QuadGrooveExpander::GROOVE_TYPE_2_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(70, 179), module, QuadGrooveExpander::GROOVE_TYPE_ATTEN_2_PARAM));
		addParam(createParam<RoundFWSnapKnob>(Vec(32, 226), module, QuadGrooveExpander::GROOVE_TYPE_3_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(70, 257), module, QuadGrooveExpander::GROOVE_TYPE_ATTEN_3_PARAM));
		addParam(createParam<RoundFWSnapKnob>(Vec(32, 304), module, QuadGrooveExpander::GROOVE_TYPE_4_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(70, 335), module, QuadGrooveExpander::GROOVE_TYPE_ATTEN_4_PARAM));

		addParam(createParam<RoundFWKnob>(Vec(129, 70), module, QuadGrooveExpander::GROOVE_AMOUNT_1_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(167, 101), module, QuadGrooveExpander::GROOVE_AMOUNT_ATTEN_1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(129, 148), module, QuadGrooveExpander::GROOVE_AMOUNT_2_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(167, 179), module, QuadGrooveExpander::GROOVE_AMOUNT_ATTEN_2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(129, 226), module, QuadGrooveExpander::GROOVE_AMOUNT_3_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(167, 257), module, QuadGrooveExpander::GROOVE_AMOUNT_ATTEN_3_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(129, 304), module, QuadGrooveExpander::GROOVE_AMOUNT_4_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(167, 335), module, QuadGrooveExpander::GROOVE_AMOUNT_ATTEN_4_PARAM));


		addInput(createInput<PJ301MPort>(Vec(68, 74), module, QuadGrooveExpander::GROOVE_TYPE_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(68, 152), module, QuadGrooveExpander::GROOVE_TYPE_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(68, 230), module, QuadGrooveExpander::GROOVE_TYPE_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(68, 308), module, QuadGrooveExpander::GROOVE_TYPE_4_INPUT));

		addInput(createInput<PJ301MPort>(Vec(165, 74), module, QuadGrooveExpander::GROOVE_AMOUNT_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(165, 152), module, QuadGrooveExpander::GROOVE_AMOUNT_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(165, 230), module, QuadGrooveExpander::GROOVE_AMOUNT_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(165, 308), module, QuadGrooveExpander::GROOVE_AMOUNT_4_INPUT));


		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		for(int i=0;i<TRACK_COUNT; i++) {
			addParam(createParam<LEDButton>(Vec(13, 48 + i*78), module, QuadGrooveExpander::TRACK_1_GROOVE_ENABLED_PARAM + i));
			addChild(createLight<MediumLight<BlueLight>>(Vec(17, 52 + i*78), module, QuadGrooveExpander::TRACK_1_GROOVE_ENABELED_LIGHT + i));
		}


	}
};

Model *modelQuadGrooveExpander = createModel<QuadGrooveExpander, QuadGrooveExpanderWidget>("QuadGrooveExpander");
