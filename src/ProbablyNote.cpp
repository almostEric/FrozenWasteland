 #include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "dsp-noise/noise.hpp"

#include <sstream>
#include <iomanip>
#include <time.h>



#define MAX_NOTES 12
#define MAX_SCALES 12

using namespace frozenwasteland::dsp;

struct ProbablyNote : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		DISTRIBUTION_PARAM,
		DISTRIBUTION_CV_ATTENUVERTER_PARAM,
        SCALE_PARAM,
		SCALE_CV_ATTENUVERTER_PARAM,
        KEY_PARAM,
		KEY_CV_ATTENUVERTER_PARAM,
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,
        OCTAVE_WRAPAROUND_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NUM_PARAMS = NOTE_WEIGHT_PARAM + MAX_NOTES
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		DISTRIBUTION_INPUT,
        SCALE_INPUT,
        KEY_INPUT,
        OCTAVE_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
        NOTE_WEIGHT_INPUT,
		NUM_INPUTS = NOTE_WEIGHT_INPUT + MAX_NOTES
	};
	enum OutputIds {
		QUANT_OUTPUT,
		ORIG_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		DISTRIBUTION_GAUSSIAN_LIGHT,
        OCTAVE_WRAPAROUND_LIGHT,
        NOTE_ACTIVE_LIGHT,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_NOTES
	};


	const char* noteNames[MAX_NOTES] = {"A","A#/Bb","B","C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab"};
	const char* scaleNames[MAX_NOTES] = {"Chromatic","Whole Tone","Aeolian (minor)","Locrian","Ionian (Major)","Dorian","Phrygian","Lydian","Mixolydian","Gypsy","Hungarian","Blues"};
    const float scaleNotes[MAX_SCALES][MAX_NOTES] = {
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,0,1,0,1,0,1,0,1,0,1,0},
        {1,0,0.2,0.5,0,0.4,0,0.8,0.2,0,0.2,0},
        {1,0.2,0,0.5,0,0.4,0.8,0,0.2,0,0.2,0},
        {1,0,0.2,0,0.5,0.4,0,0.8,0,0.2,0,0.2},
        {1,0,0.2,0.5,0,0.4,0,0.8,0,0.2,0.2,0},
        {1,0.2,0,0.5,0,0.4,0,0.8,0.2,0,0.2,0},
        {1,0,0.2,0,0.5,0,0.4,0.8,0,0.2,0,0.2},
        {1,0,0.2,0,0.5,0.4,0,0.8,0,0.2,0.2,0},
        {1,0.2,0,0,0.5,0.5,0,0.8,0.2,0,0,0.2},
        {1,0,0.2,0.5,0,0,0.3,0.8,0.2,0,0,0.2},
        {1,0,0,0.5,0,0.4,0,0.8,0,0,0.2,0},
    };
    

	
	dsp::SchmittTrigger clockTrigger,octaveWrapAroundTrigger,noteActiveTrigger[MAX_NOTES]; 
    GaussianNoiseGenerator _gauss;
 
    bool octaveWrapAround = false;
    bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteInitialProbability[MAX_NOTES] = {0.0f};
    float currentScaleNotes[MAX_NOTES] = {0.0};
	float actualProbability[MAX_NOTES] = {0.0};

    int scale = 0;
    int lastScale = -1;
    int key = 0;
    int lastKey = -1;
    int octave = 0;
    int spread = 0;
	float focus = 0; 
	int currentNote = 0;
	int probabilityNote = 0;
	int lastNote = -1;
	int lastSpread = -1;
	float lastFocus = -1;

    

	ProbablyNote() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNote::SPREAD_PARAM, 0.0, 6.0, 0.0,"Spread");
        configParam(ProbablyNote::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNote::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNote::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNote::SCALE_PARAM, 0.0, 11.0, 0.0,"Scale");
        configParam(ProbablyNote::SCALE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Scale CV Attenuation","%",0,100);
		configParam(ProbablyNote::KEY_PARAM, 0.0, 11.0, 0.0,"Key");
        configParam(ProbablyNote::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNote::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
        configParam(ProbablyNote::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNote::OCTAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Octave Wraparound");

        srand(time(NULL));

        for(int i=0;i<MAX_NOTES;i++) {
            configParam(ProbablyNote::NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
            configParam(ProbablyNote::NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight");		
        }
	}

    float lerp(float v0, float v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,float randomIn) {
        float weightTotal = 0.0f;
        for(int i = 0;i <MAX_NOTES; i++) {
            weightTotal += weights[i];
        }

        int chosenWeight = -1;        
        float rnd = randomIn * weightTotal;
        for(int i = 0;i <MAX_NOTES;i++ ) {
            if(rnd < weights[i]) {
                chosenWeight = i;
                break;
            }
            rnd -= weights[i];
        }
        return chosenWeight;
    }
	
	
	void process(const ProcessArgs &args) override {
	
		
        if (octaveWrapAroundTrigger.process(params[OCTAVE_WRAPAROUND_PARAM].getValue())) {
			octaveWrapAround = !octaveWrapAround;
		}		
		lights[OCTAVE_WRAPAROUND_LIGHT].value = octaveWrapAround;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,6.0f);

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,11.0f);
        if(scale != lastScale) {
            for(int i = 0;i<MAX_NOTES;i++) {
                currentScaleNotes[i] = scaleNotes[scale][i];
            }
        }

        key = clamp(params[KEY_PARAM].getValue() + (inputs[KEY_INPUT].getVoltage() * MAX_NOTES / 10.0 * params[KEY_CV_ATTENUVERTER_PARAM].getValue()),0.0f,11.0f);


        if(key != lastKey || scale != lastScale) {
            for(int i = 0; i < MAX_NOTES;i++) {
                int noteValue = (i + key - 3) % MAX_NOTES; // 3 is the offset from C to A
                if(noteValue < 0)
                    noteValue +=MAX_NOTES;
                noteActive[noteValue] = currentScaleNotes[i] > 0.0;
				noteScaleProbability[noteValue] = currentScaleNotes[i];
            }
        }

        octave = clamp(params[OCTAVE_PARAM].getValue() + (inputs[OCTAVE_INPUT].getVoltage() * 0.4 * params[OCTAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);



        double noteIn = inputs[NOTE_INPUT].getVoltage();
        double octaveIn = std::floor(noteIn);
        double fractionalValue = noteIn - octaveIn;
        double lastDif = 1.0f;    
        for(int i = 0;i<MAX_NOTES;i++) {            
			double currentDif = std::abs((i / 12.0) - fractionalValue);
			if(currentDif < lastDif) {
				lastDif = currentDif;
				currentNote = i;
			}            
        }

		if(lastNote != currentNote || lastSpread != spread || lastFocus != focus) {
			for(int i = 0; i<MAX_NOTES;i++) {
				noteInitialProbability[i] = 0.0;
			}
			noteInitialProbability[currentNote] = 1.0;
			for(int i=1;i<=spread;i++) {
				int noteAbove = (currentNote + i) % MAX_NOTES;
				int noteBelow = (currentNote - i);
				if(noteBelow < 0)
                    noteBelow +=MAX_NOTES;

				float initialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
				
				noteInitialProbability[noteAbove] = initialProbability;				
				noteInitialProbability[noteBelow] = initialProbability;				
			}
			lastNote = currentNote;
			lastSpread = spread;
			lastFocus = focus;
		}

		lastScale = scale;
        lastKey = key;
        for(int i=0;i<MAX_NOTES;i++) {
            if (noteActiveTrigger[i].process( params[NOTE_ACTIVE_PARAM+i].getValue())) {
                noteActive[i] = !noteActive[i];
            }

			float userProbability;
			if(noteActive[i])
	            userProbability = clamp(noteScaleProbability[i] + params[NOTE_WEIGHT_PARAM+i].getValue() + (inputs[NOTE_WEIGHT_INPUT].getVoltage() / 10.0f),0.0f,1.0f);    
			else 
				userProbability = 0.0;

			lights[NOTE_ACTIVE_LIGHT+i].value = userProbability;    
			actualProbability[i] = noteInitialProbability[i] * userProbability; 
        }

		if( inputs[TRIGGER_INPUT].active ) {
			if (clockTrigger.process(inputs[TRIGGER_INPUT].value) ) {		
				float rnd = ((float) rand()/RAND_MAX);
				if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
					rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f;
				}	
			
				int randomNote = weightedProbability(actualProbability,rnd);
				if(randomNote == -1) { //Couldn't find a note, so find first active
					bool noteOk = false;
					int notesSearched = 0;
					randomNote = currentNote; 
					do {
						randomNote = (randomNote + 1) % MAX_NOTES;
						notesSearched +=1;
						noteOk = noteActive[randomNote] || notesSearched >= MAX_NOTES;
					} while(!noteOk);
				}


				probabilityNote = randomNote;
				float octaveAdjust = 0.0;
				if(!octaveWrapAround) {
					if(randomNote > currentNote && randomNote - currentNote > spread)
						octaveAdjust = -1.0;
					if(randomNote < currentNote && currentNote - randomNote > spread)
						octaveAdjust = 1.0;
				}

				double quantitizedNoteCV = octaveIn + (randomNote / 12.0) + octave + octaveAdjust; 
				outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV);
        
			} 
		}

	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void ProbablyNote::onReset() {
	clockTrigger.reset();
}




struct ProbablyNoteDisplay : TransparentWidget {
	ProbablyNote *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawKey(const DrawArgs &args, Vec pos, int key) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->noteNames[key]);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawScale(const DrawArgs &args, Vec pos, int scale) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->scaleNames[scale]);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawOctave(const DrawArgs &args, Vec pos, int octave) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", octave);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	

	void drawNoteRange(const DrawArgs &args, float *noteInitialProbability) 
	{		
		// Draw indicator
		for(int i = 0; i<MAX_NOTES;i++) {
			float opacity = noteInitialProbability[i] * 255;
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			if(i == module->probabilityNote) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			}
			switch(i) {
				case 0:
				//C
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,330);
				nvgLineTo(args.vg,142, 330);
				nvgLineTo(args.vg,142, 302);
				nvgLineTo(args.vg,62, 302);
				nvgLineTo(args.vg,62, 315);		
				nvgLineTo(args.vg,0, 315);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 1:
				//C#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,314);
				nvgLineTo(args.vg,62,314);
				nvgLineTo(args.vg,62,290);		
				nvgLineTo(args.vg,0,290);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 2:
				//D
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,302);
				nvgLineTo(args.vg,142,302);
				nvgLineTo(args.vg,142,274);		
				nvgLineTo(args.vg,62,274);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 3:
				//D#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,286);
				nvgLineTo(args.vg,62,286);
				nvgLineTo(args.vg,62,262);		
				nvgLineTo(args.vg,0,262);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 4:
				//E
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,274);
				nvgLineTo(args.vg,142,274);
				nvgLineTo(args.vg,142,246);		
				nvgLineTo(args.vg,0,246);
				nvgLineTo(args.vg,0, 261);		
				nvgLineTo(args.vg,62, 261);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 5:
				//F 
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,246);
				nvgLineTo(args.vg,142, 246);
				nvgLineTo(args.vg,142, 218);
				nvgLineTo(args.vg,62, 218);
				nvgLineTo(args.vg,62, 231);		
				nvgLineTo(args.vg,0, 231);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 6:
				//F#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,230);
				nvgLineTo(args.vg,62,230);
				nvgLineTo(args.vg,62,206);		
				nvgLineTo(args.vg,0,206);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 7:
				//G
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,218);
				nvgLineTo(args.vg,142,218);
				nvgLineTo(args.vg,142,190);		
				nvgLineTo(args.vg,62,190);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 8:
				//G#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,202);
				nvgLineTo(args.vg,62,202);
				nvgLineTo(args.vg,62,176);		
				nvgLineTo(args.vg,0,176);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 9:
				//A
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,190);
				nvgLineTo(args.vg,142,190);
				nvgLineTo(args.vg,142,162);		
				nvgLineTo(args.vg,62,162);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 10:
				//A#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,0,174);
				nvgLineTo(args.vg,62,174);
				nvgLineTo(args.vg,62,148);		
				nvgLineTo(args.vg,0,148);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 11:
				//B
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,162);
				nvgLineTo(args.vg,142,162);
				nvgLineTo(args.vg,142,134);		
				nvgLineTo(args.vg,0,134);
				nvgLineTo(args.vg,0,149);		
				nvgLineTo(args.vg,62,149);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

			}
		}

	

	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

		drawScale(args, Vec(4,84), module->scale);
		drawKey(args, Vec(76,84), module->key);
		//drawOctave(args, Vec(66, 280), module->octave);
		drawNoteRange(args, module->noteInitialProbability);
	}
};

struct ProbablyNoteWidget : ModuleWidget {
	ProbablyNoteWidget(ProbablyNote *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNote.svg")));


		{
			ProbablyNoteDisplay *display = new ProbablyNoteDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(10, 30), module, ProbablyNote::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(40, 30), module, ProbablyNote::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(40, 55), module, ProbablyNote::EXTERNAL_RANDOM_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(70,30), module, ProbablyNote::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(96,56), module, ProbablyNote::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(98, 34), module, ProbablyNote::SPREAD_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(128, 30), module, ProbablyNote::DISTRIBUTION_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,56), module, ProbablyNote::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 34), module, ProbablyNote::DISTRIBUTION_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNote::SCALE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNote::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNote::SCALE_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(70,86), module, ProbablyNote::KEY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(96,112), module, ProbablyNote::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(98, 90), module, ProbablyNote::KEY_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(128,86), module, ProbablyNote::OCTAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,112), module, ProbablyNote::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 90), module, ProbablyNote::OCTAVE_INPUT));


		addParam(createParam<LEDButton>(Vec(130, 113), module, ProbablyNote::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(131.5, 114.5), module, ProbablyNote::OCTAVE_WRAPAROUND_LIGHT));


        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,306), module, ProbablyNote::NOTE_WEIGHT_PARAM+0));			
		addInput(createInput<FWPortInSmall>(Vec(118, 307), module, ProbablyNote::NOTE_WEIGHT_INPUT+0));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,292), module, ProbablyNote::NOTE_WEIGHT_PARAM+1));			
		addInput(createInput<FWPortInSmall>(Vec(2, 293), module, ProbablyNote::NOTE_WEIGHT_INPUT+1));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,278), module, ProbablyNote::NOTE_WEIGHT_PARAM+2));			
		addInput(createInput<FWPortInSmall>(Vec(118, 279), module, ProbablyNote::NOTE_WEIGHT_INPUT+2));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,264), module, ProbablyNote::NOTE_WEIGHT_PARAM+3));			
		addInput(createInput<FWPortInSmall>(Vec(2, 265), module, ProbablyNote::NOTE_WEIGHT_INPUT+3));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,250), module, ProbablyNote::NOTE_WEIGHT_PARAM+4));			
		addInput(createInput<FWPortInSmall>(Vec(118, 251), module, ProbablyNote::NOTE_WEIGHT_INPUT+4));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,222), module, ProbablyNote::NOTE_WEIGHT_PARAM+5));			
		addInput(createInput<FWPortInSmall>(Vec(118, 223), module, ProbablyNote::NOTE_WEIGHT_INPUT+5));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,208), module, ProbablyNote::NOTE_WEIGHT_PARAM+6));			
		addInput(createInput<FWPortInSmall>(Vec(2, 209), module, ProbablyNote::NOTE_WEIGHT_INPUT+6));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,194), module, ProbablyNote::NOTE_WEIGHT_PARAM+7));			
		addInput(createInput<FWPortInSmall>(Vec(118, 195), module, ProbablyNote::NOTE_WEIGHT_INPUT+7));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,180), module, ProbablyNote::NOTE_WEIGHT_PARAM+8));			
		addInput(createInput<FWPortInSmall>(Vec(2, 181), module, ProbablyNote::NOTE_WEIGHT_INPUT+8));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,166), module, ProbablyNote::NOTE_WEIGHT_PARAM+9));			
		addInput(createInput<FWPortInSmall>(Vec(118, 167), module, ProbablyNote::NOTE_WEIGHT_INPUT+9));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,152), module, ProbablyNote::NOTE_WEIGHT_PARAM+10));			
		addInput(createInput<FWPortInSmall>(Vec(2, 153), module, ProbablyNote::NOTE_WEIGHT_INPUT+10));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,138), module, ProbablyNote::NOTE_WEIGHT_PARAM+11));			
		addInput(createInput<FWPortInSmall>(Vec(118, 139), module, ProbablyNote::NOTE_WEIGHT_INPUT+11));


		addParam(createParam<LEDButton>(Vec(72, 307), module, ProbablyNote::NOTE_ACTIVE_PARAM+0));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 308.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+0));
		addParam(createParam<LEDButton>(Vec(44, 293), module, ProbablyNote::NOTE_ACTIVE_PARAM+1));
		addChild(createLight<LargeLight<GreenLight>>(Vec(45.5, 294.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+1));
		addParam(createParam<LEDButton>(Vec(72, 279), module, ProbablyNote::NOTE_ACTIVE_PARAM+2));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 280.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+2));
		addParam(createParam<LEDButton>(Vec(44, 265), module, ProbablyNote::NOTE_ACTIVE_PARAM+3));
		addChild(createLight<LargeLight<GreenLight>>(Vec(45.5, 266.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+3));
		addParam(createParam<LEDButton>(Vec(72, 251), module, ProbablyNote::NOTE_ACTIVE_PARAM+4));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 252.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+4));
		addParam(createParam<LEDButton>(Vec(72, 223), module, ProbablyNote::NOTE_ACTIVE_PARAM+5));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 224.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+5));
		addParam(createParam<LEDButton>(Vec(44, 209), module, ProbablyNote::NOTE_ACTIVE_PARAM+6));
		addChild(createLight<LargeLight<GreenLight>>(Vec(45.5, 210.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+6));
		addParam(createParam<LEDButton>(Vec(72, 195), module, ProbablyNote::NOTE_ACTIVE_PARAM+7));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 196.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+7));
		addParam(createParam<LEDButton>(Vec(44, 181), module, ProbablyNote::NOTE_ACTIVE_PARAM+8));
		addChild(createLight<LargeLight<GreenLight>>(Vec(45.5, 182.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+8));
		addParam(createParam<LEDButton>(Vec(72, 167), module, ProbablyNote::NOTE_ACTIVE_PARAM+9));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 168.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+9));
		addParam(createParam<LEDButton>(Vec(44, 153), module, ProbablyNote::NOTE_ACTIVE_PARAM+10));
		addChild(createLight<LargeLight<GreenLight>>(Vec(45.5, 154.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+10));
		addParam(createParam<LEDButton>(Vec(72, 139), module, ProbablyNote::NOTE_ACTIVE_PARAM+11));
		addChild(createLight<LargeLight<GreenLight>>(Vec(73.5, 140.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+11));

		

		addOutput(createOutput<FWPortInSmall>(Vec(21, 345),  module, ProbablyNote::QUANT_OUTPUT));
		//addOutput(createOutput<FWPortInSmall>(Vec(51, 345),  module, ProbablyNote::ORIG_OUTPUT));

	}
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNote = createModel<ProbablyNote, ProbablyNoteWidget>("ProbablyNote");
    