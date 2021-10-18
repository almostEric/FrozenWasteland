#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define OCTAVE_RANGE 5
#define NBR_OCTAVES 11
#define NBR_NOTES 12

#define EXPANDER_MOTHER_SEND_MESSAGE_COUNT 3 + 2 //3 chords plus curent note and octave
#define EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT 7 + 11*12


struct PNOctaveProbabilityExpander : Module {
	enum ParamIds {
		NOTE_SELECT_PARAM,
		OCTAVE_PROBABILITY_PARAM = NOTE_SELECT_PARAM + NBR_NOTES,
		OCTAVE_PROBABILITY_CV_ATTENUVERTER_PARAM = OCTAVE_PROBABILITY_PARAM + NBR_OCTAVES,
		NUM_PARAMS = OCTAVE_PROBABILITY_CV_ATTENUVERTER_PARAM + NBR_OCTAVES
	};
	enum InputIds {
		NOTE_SELECT_INPUT,
		OCTAVE_PROBABILITY_INPUT = NOTE_SELECT_INPUT + NBR_NOTES,
		NUM_INPUTS = OCTAVE_PROBABILITY_INPUT + NBR_OCTAVES
	};
	enum OutputIds {
		// TEST1_OUTPUT,
		// TEST2_OUTPUT,
		// TEST3_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
        NOTE_SELECTED_LIGHT,
		NUM_LIGHTS = NOTE_SELECTED_LIGHT + NBR_NOTES
	};

	const std::string noteNames[NBR_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};

    bool noteActive[NBR_NOTES] = {1};
	float octaveProbability[NBR_OCTAVES] = {0};
	float calculatedProbability[NBR_OCTAVES] = {0};
    float weightTotal = 0;

	int currentNote = -1;
	int currentOctave = -1;
	
	// Expander
	float leftMessages[2][EXPANDER_MOTHER_SEND_MESSAGE_COUNT + EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT] = {};
	float rightMessages[2][EXPANDER_MOTHER_SEND_MESSAGE_COUNT + EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT] = {};


	
	dsp::SchmittTrigger noteActiveTrigger[NBR_NOTES];

	//probabilities
	float octaveProbabilityPercentage[NBR_OCTAVES] = {0};
	
	PNOctaveProbabilityExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
        for(int i=0;i<NBR_OCTAVES;i++) {
            configParam(OCTAVE_PROBABILITY_PARAM+i, 0.0f, 1.0f, 0.0f,"Octave " + std::to_string(5-i) + " Probability","%",0,100);
            configParam(OCTAVE_PROBABILITY_CV_ATTENUVERTER_PARAM+i, -1.0, 1.0, 0.0,"Octave " + std::to_string(5-i) + " Probability CV Attenuation","%",0,100);
			configInput(OCTAVE_PROBABILITY_INPUT+i, "Octave " + std::to_string(5-i) + " Probability");
        }

        for(int i=0;i<NBR_NOTES;i++) {
			configInput(NOTE_SELECT_INPUT+i, "Note " + noteNames[i] + " Select");
			configButton(NOTE_SELECT_PARAM+i,"Note " + noteNames[i] + " Select");
		}
	
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
    }

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

        for(int i=0;i<NBR_NOTES;i++) {
            std::string buf = "noteActive-" + std::to_string(i);
            json_object_set_new(rootJ, buf.c_str(), json_boolean(noteActive[i]));
        }		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

        for(int i=0;i<NBR_NOTES;i++) {
            std::string buf = "noteActive-" + std::to_string(i);
            json_t *naJ = json_object_get(rootJ, buf.c_str());
            if (naJ) {
                noteActive[i] = json_boolean_value(naJ);               
            }
        }
	}
	

	void process(const ProcessArgs &args) override {


        for(int i=0;i<NBR_NOTES;i++) {
            if(noteActiveTrigger[i].process(params[NOTE_SELECT_PARAM + i].getValue() + inputs[NOTE_SELECT_INPUT + i].getVoltage())) {
                noteActive[i] = !noteActive[i];
            }
            lights[NOTE_SELECTED_LIGHT+i].value = noteActive[i] ? 1 : 0;
        }
		
        float tempWeightTotal = 0;
        for(int i=0;i<NBR_OCTAVES;i++) {
            octaveProbability[i] = clamp(params[OCTAVE_PROBABILITY_PARAM+i].getValue() + (inputs[OCTAVE_PROBABILITY_INPUT+i].getVoltage() * params[OCTAVE_PROBABILITY_CV_ATTENUVERTER_PARAM+i].getValue() / 10.0),0.0f,1.0f);
			octaveProbabilityPercentage[i] = octaveProbability[i];
            tempWeightTotal += octaveProbability[i];
        }
        weightTotal = tempWeightTotal;

		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelProbablyNote || leftExpander.module->model == modelPNChordExpander
													|| leftExpander.module->model == modelPNOctaveProbabilityExpander));
		if (motherPresent) {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

			//Initalize
			std::fill(messagesToMother, messagesToMother+EXPANDER_MOTHER_SEND_MESSAGE_COUNT + EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT, 0.0);

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPNOctaveProbabilityExpander));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				
				//PN Pass through left
				std::copy(messagesFromExpander,messagesFromExpander+EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT,messagesToMother);		

				//PN Pass through right
				std::copy(messagesFromMother+EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT,messagesFromMother+EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + EXPANDER_MOTHER_SEND_MESSAGE_COUNT,
									messageToExpander+EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT);			

				rightExpander.module->leftExpander.messageFlipRequested = true;
			}

			for(int i = 0;i < NBR_NOTES;i++) {
				if(noteActive[i]) {
					for (int j = 0;j<NBR_OCTAVES;j++) {
						messagesToMother[7 + i*NBR_OCTAVES + j] = octaveProbability[j];
					}
				}
			}

			leftExpander.module->rightExpander.messageFlipRequested = true;


			// From Mother	
			currentNote = messagesFromMother[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 3]; 
			currentOctave = messagesFromMother[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 4]; 
			// fprintf(stderr, "current note:%i current octave:%i\n",currentNote,currentOctave);
		}
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


struct PNOctaveProbabilityExpanderDisplay : TransparentWidget {
	PNOctaveProbabilityExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;
	std::string fontPath;

	PNOctaveProbabilityExpanderDisplay() {
		fontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
	}

    void drawAverages(const DrawArgs &args, Vec pos) {

        float total = module->weightTotal;
		float largestProb = *std::max_element(module->octaveProbability, module->octaveProbability + NBR_OCTAVES);
		float maxPixel = 140.5-(largestProb/total * 100.0);
		// fprintf(stderr, "max pixel:%f\n",maxPixel);
        if(total > 0) {
            for(int i=0;i<NBR_OCTAVES;i++) {
    	        nvgBeginPath(args.vg);
                float probability = module->octaveProbability[i] / total * 100.0;
				// nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
				NVGpaint paint = nvgLinearGradient(args.vg,0,maxPixel,0,140,nvgRGBA(0xff, 0xff, 0x20, 0xff), nvgRGBA(0xff, 0xff, 0x20, 0x3f));
				if(module->currentNote > 0 && module->noteActive[module->currentNote] && module->currentOctave == i ) {
					// nvgFillColor(args.vg, nvgRGBA(0x47, 0xff, 0x20, 0xff));
					paint = nvgLinearGradient(args.vg,0,maxPixel,0,140,nvgRGBA(0x47, 0xff, 0x20, 0xff), nvgRGBA(0x47, 0xff, 0x20, 0x1f));
				}
				nvgFillPaint(args.vg, paint);
                nvgRect(args.vg, (NBR_OCTAVES-i) * 9 - 3.0 ,140.5-probability,9,probability);
	            nvgFill(args.vg);
            }
        }
	}

	void draw(const DrawArgs &args) override {
		font = APP->window->loadFont(fontPath);

		if (!module)
			return; 
        drawAverages(args,Vec(0, 0));
	}
};

struct PNOctaveProbabilityExpanderWidget : ModuleWidget {
	PNOctaveProbabilityExpanderWidget(PNOctaveProbabilityExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PNOctaveProbabilityExpander.svg")));

		{
			PNOctaveProbabilityExpanderDisplay *display = new PNOctaveProbabilityExpanderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(100, 100);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		addInput(createInput<FWPortInReallySmall>(Vec(62, 330), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+0));
		addParam(createParam<LEDButton>(Vec(77, 327), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+0));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 328.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+0));

		addInput(createInput<FWPortInReallySmall>(Vec(23, 316), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+1));
		addParam(createParam<LEDButton>(Vec(38, 313), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+1));
		addChild(createLight<LargeLight<GreenLight>>(Vec(39.5, 314.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+1));

		addInput(createInput<FWPortInReallySmall>(Vec(62, 302), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+2));
		addParam(createParam<LEDButton>(Vec(77, 299), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+2));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 300.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+2));

		addInput(createInput<FWPortInReallySmall>(Vec(23, 288), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+3));
		addParam(createParam<LEDButton>(Vec(38, 285), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+3));
		addChild(createLight<LargeLight<GreenLight>>(Vec(39.5, 286.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+3));

		addInput(createInput<FWPortInReallySmall>(Vec(62, 274), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+4));
		addParam(createParam<LEDButton>(Vec(77, 271), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+4));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 272.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+4));

		addInput(createInput<FWPortInReallySmall>(Vec(62, 246), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+5));
		addParam(createParam<LEDButton>(Vec(77, 243), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+5));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 244.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+5));

		addInput(createInput<FWPortInReallySmall>(Vec(23, 232), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+6));
		addParam(createParam<LEDButton>(Vec(38, 229), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+6));
		addChild(createLight<LargeLight<GreenLight>>(Vec(39.5, 230.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+6));

		addInput(createInput<FWPortInReallySmall>(Vec(62, 218), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+7));
		addParam(createParam<LEDButton>(Vec(77, 215), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+7));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 216.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+7));

		addInput(createInput<FWPortInReallySmall>(Vec(23, 204), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+8));
		addParam(createParam<LEDButton>(Vec(38, 201), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+8));
		addChild(createLight<LargeLight<GreenLight>>(Vec(39.5, 202.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+8));

		addInput(createInput<FWPortInReallySmall>(Vec(62, 190), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+9));
		addParam(createParam<LEDButton>(Vec(77, 187), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+9));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 188.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+9));

		addInput(createInput<FWPortInReallySmall>(Vec(23, 176), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+10));
		addParam(createParam<LEDButton>(Vec(38, 173), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+10));
		addChild(createLight<LargeLight<GreenLight>>(Vec(39.5, 174.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+10));

		addInput(createInput<FWPortInReallySmall>(Vec(62, 162), module, PNOctaveProbabilityExpander::NOTE_SELECT_INPUT+11));
		addParam(createParam<LEDButton>(Vec(77, 159), module, PNOctaveProbabilityExpander::NOTE_SELECT_PARAM+11));
		addChild(createLight<LargeLight<GreenLight>>(Vec(78.5, 160.5), module, PNOctaveProbabilityExpander::NOTE_SELECTED_LIGHT+11));


        for(int i=0;i<NBR_OCTAVES;i++) {
			ParamWidget* octaveProbabilityParam = createParam<RoundSmallFWKnob>(Vec(125,35 + i*30), module, PNOctaveProbabilityExpander::OCTAVE_PROBABILITY_PARAM+i);
			if (module) {
				dynamic_cast<RoundSmallFWKnob*>(octaveProbabilityParam)->percentage = &module->octaveProbabilityPercentage[i];
			}
			addParam(octaveProbabilityParam);							
            addInput(createInput<FWPortInSmall>(Vec(155, 40 + i*30), module, PNOctaveProbabilityExpander::OCTAVE_PROBABILITY_INPUT + i));
            addParam(createParam<RoundReallySmallFWKnob>(Vec(177,38+i*30), module, PNOctaveProbabilityExpander::OCTAVE_PROBABILITY_CV_ATTENUVERTER_PARAM+i));			

        }
	}
};

Model *modelPNOctaveProbabilityExpander = createModel<PNOctaveProbabilityExpander, PNOctaveProbabilityExpanderWidget>("PNOctaveProbabilityExpander");
