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

	dsp::SchmittTrigger trackHierarchicalTrigger[TRACK_COUNT],trackComplementTrigger[TRACK_COUNT];
	bool trackHierachical[TRACK_COUNT] = {0};
	int trackComplement[TRACK_COUNT] = {0};
	

    float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	
	float extraParameterValue[TRACK_COUNT] = {0};
	
	QARWellFormedRhythmExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		        
        configParam(TRACK_1_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 1 - Ratio");
        configParam(TRACK_2_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 2 - Ratio");
        configParam(TRACK_3_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 3 - Ratio");
        configParam(TRACK_4_EXTRA_VALUE_PARAM, 0.0f, 1.0, 0.5,"Track 4 - Ratio");
        
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
        
        // json_object_set_new(rootJ, "constantTime", json_integer((bool) constantTime));
		// json_object_set_new(rootJ, "masterTrack", json_integer((int) masterTrack));
		// json_object_set_new(rootJ, "chainMode", json_integer((int) chainMode));
		// json_object_set_new(rootJ, "muted", json_integer((bool) muted));

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

		// json_t *ctJ = json_object_get(rootJ, "constantTime");
		// if (ctJ)
		// 	constantTime = json_integer_value(ctJ);

		// json_t *mtJ = json_object_get(rootJ, "masterTrack");
		// if (mtJ)
		// 	masterTrack = json_integer_value(mtJ);

		// json_t *cmJ = json_object_get(rootJ, "chainMode");
		// if (cmJ)
		// 	chainMode = json_integer_value(cmJ);

		// json_t *mutedJ = json_object_get(rootJ, "muted");
		// if (mutedJ)
		// 	muted = json_integer_value(mutedJ);
	}


	void process(const ProcessArgs &args) override {
		for(int i=0; i< TRACK_COUNT; i++) {            
            float t = clamp(params[TRACK_1_EXTRA_VALUE_PARAM+i].getValue() + (inputs[TRACK_1_EXTRA_VALUE_INPUT+i].isConnected() ? inputs[TRACK_1_EXTRA_VALUE_INPUT+i].getVoltage() / 10.0f : 0.0f ),0.0f,0.999f);
			extraParameterValue[i] = 1/(1-t);
			
			if (trackHierarchicalTrigger[i].process(params[TRACK_1_HIERARCHICAL_PARAM+i].getValue())) {
				trackHierachical[i] = !trackHierachical[i];
			}
			lights[TRACK_1_HIERARCHICAL_LIGHT + i].value = trackHierachical[i];

			if (trackComplementTrigger[i].process(params[TRACK_1_COMPLEMENT_PARAM+i].getValue())) {
				trackComplement[i] = (trackComplement[i] + 1) % 3;
			}
			lights[TRACK_1_COMPLEMENT_LIGHT + (i * 3) + 0].value = trackComplement[i] == 2;
			lights[TRACK_1_COMPLEMENT_LIGHT + (i * 3) + 1].value = trackComplement[i] > 0;
		}        

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || leftExpander.module->model == modelQuadAlgorithmicRhythm));
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
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQuadAlgorithmicRhythm));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                if(rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARWarpedSpaceExpander ) { // Get QRE values							
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


            for (int i = 0; i < TRACK_COUNT; i++) {
                messagesToMother[i] = extraParameterValue[i];
				messagesToMother[TRACK_COUNT + i] = trackHierachical[i];
				messagesToMother[TRACK_COUNT * 2 + i] = trackComplement[i];
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
            addParam(createParam<RoundFWKnob>(Vec(12, 59 + i * 72), module, QARWellFormedRhythmExpander::TRACK_1_EXTRA_VALUE_PARAM+i));
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
 