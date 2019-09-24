 #include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "dsp-noise/noise.hpp"
#include "osdialog.h"
#include <sstream>
#include <iomanip>
#include <time.h>

#include <iostream>
#include <fstream>
#include <string>

#define MAX_NOTES 12
#define MAX_SCALES 12
#define MAX_JINS 9


using namespace frozenwasteland::dsp;

struct ProbablyNoteArabic : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		DISTRIBUTION_PARAM,
		DISTRIBUTION_CV_ATTENUVERTER_PARAM,
        SHIFT_PARAM,
		SHIFT_CV_ATTENUVERTER_PARAM,
        LOWER_JINS_PARAM,
		LOWER_JINS_CV_ATTENUVERTER_PARAM,
        UPPER_JINS_PARAM,
		UPPER_JINS_CV_ATTENUVERTER_PARAM,
        KEY_PARAM,
		KEY_CV_ATTENUVERTER_PARAM,
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,
        WRITE_SCALE_PARAM,
        OCTAVE_WRAPAROUND_PARAM,
		SHIFT_SCALING_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NUM_PARAMS = NOTE_WEIGHT_PARAM + MAX_NOTES
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		DISTRIBUTION_INPUT,
		SHIFT_INPUT,
        LOWER_JINS_INPUT,
        UPPER_JINS_INPUT,
        KEY_INPUT,
        OCTAVE_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
        NOTE_WEIGHT_INPUT,
		NUM_INPUTS = NOTE_WEIGHT_INPUT + MAX_NOTES
	};
	enum OutputIds {
		QUANT_OUTPUT,
		WEIGHT_OUTPUT,
		NOTE_CHANGE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		DISTRIBUTION_GAUSSIAN_LIGHT,
        OCTAVE_WRAPAROUND_LIGHT,
		JUST_INTONATION_LIGHT,
		SHIFT_LOGARITHMIC_SCALE_LIGHT,
        NOTE_ACTIVE_LIGHT,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_NOTES*2
	};


	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
	const char* scaleNames[MAX_JINS] = {"Ajam","Bayati","Hijaz","Kurd","Nahawand","Nikriz","Rast","Saba","Sikah"};
	const char* arabicScaleNames[MAX_JINS] = {"عجم","بياتي","حجاز","كرد","نهاوند","نكريز","راست","صبا","سيكاه"};
	float defaultScaleNoteWeighting[MAX_SCALES][MAX_NOTES] = {
		{1,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,1,0,1,0,1,0,1,0,1,0},
		{1,0,0.2,0.5,0,0.4,0,0.8,0.2,0,0.3,0},
		{1,0.2,0,0.5,0,0.4,0.8,0,0.2,0,0.3,0},
		{1,0,0.2,0,0.5,0.4,0,0.8,0,0.2,0,0.3},
		{1,0,0.2,0.5,0,0.4,0,0.8,0,0.2,0.3,0},
		{1,0.2,0,0.5,0,0.4,0,0.8,0.2,0,0.3,0},
		{1,0,0.2,0,0.5,0,0.4,0.8,0,0.2,0,0.3},
		{1,0,0.2,0,0.5,0.4,0,0.8,0,0.2,0.3,0},
		{1,0.2,0,0,0.5,0.5,0,0.8,0.2,0,0,0.3},
		{1,0,0.2,0.5,0,0,0.3,0.8,0.2,0,0,0.3},
		{1,0,0,0.5,0,0.4,0,0.8,0,0,0.3,0},
	}; 
    float scaleNoteWeighting[MAX_SCALES][MAX_NOTES]; 

	const char* tempermentNames[MAX_NOTES] = {"Equal","Just"};
    double noteTemperment[MAX_NOTES] = {
        0,111.73,203.91,315.64,386.61,498.04,582.51,701.955,813.69,884.36,996.09,1088.27
    };
    

	
	dsp::SchmittTrigger clockTrigger,writeScaleTrigger,octaveWrapAroundTrigger,tempermentTrigger,shiftScalingTrigger,noteActiveTrigger[MAX_NOTES]; 
	dsp::PulseGenerator noteChangePulse;
    GaussianNoiseGenerator _gauss;
 
    bool octaveWrapAround = false;
    bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteInitialProbability[MAX_NOTES] = {0.0f};
    float currentScaleNoteWeighting[MAX_NOTES] = {0.0};
	float actualProbability[MAX_NOTES] = {0.0};


    int scale = 0;
    int lastScale = -1;
    int key = 0;
    int lastKey = -1;
    int octave = 0;
	int weightShift = 0;
	int lastWeightShift = 0;
    int spread = 0;
	float focus = 0; 
	int currentNote = 0;
	int probabilityNote = 0;
	int lastQuantizedCV = 0;
	int lastNote = -1;
	int lastSpread = -1;
	float lastFocus = -1;
	bool justIntonation = false;
	bool shiftLogarithmic = false;

	std::string lastPath;
    

	ProbablyNoteArabic() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteArabic::SPREAD_PARAM, 0.0, 6.0, 0.0,"Spread");
        configParam(ProbablyNoteArabic::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteArabic::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNoteArabic::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNoteArabic::SHIFT_PARAM, -6.0, 6.0, 0.0,"Weight Shift");
        configParam(ProbablyNoteArabic::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteArabic::LOWER_JINS_PARAM, 0.0, 8.0, 0.0,"Lower Jins");
        configParam(ProbablyNoteArabic::LOWER_JINS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Lower Jins CV Attenuation","%",0,100);
		configParam(ProbablyNoteArabic::UPPER_JINS_PARAM, 0.0, 8.0, 0.0,"Upper Jins");
        configParam(ProbablyNoteArabic::UPPER_JINS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Upper Jins CV Attenuation","%",0,100);
		//configParam(ProbablyNoteArabic::KEY_PARAM, 0.0, 11.0, 0.0,"Key");
        //configParam(ProbablyNoteArabic::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNoteArabic::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
        configParam(ProbablyNoteArabic::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNoteArabic::OCTAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Octave Wraparound");
		

        srand(time(NULL));

        for(int i=0;i<MAX_NOTES;i++) {
            configParam(ProbablyNoteArabic::NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
            configParam(ProbablyNoteArabic::NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight");		
        }

		onReset();
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

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "octaveWrapAround", json_integer((int) octaveWrapAround));
		json_object_set_new(rootJ, "justIntonation", json_integer((int) justIntonation));
		json_object_set_new(rootJ, "shiftLogarithmic", json_integer((int) shiftLogarithmic));

		for(int i=0;i<MAX_SCALES;i++) {
			for(int j=0;j<MAX_NOTES;j++) {
				char buf[100];
				char notebuf[100];
				strcpy(buf, "scaleWeight-");
				strcat(buf, scaleNames[i]);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				json_object_set_new(rootJ, buf, json_real((float) scaleNoteWeighting[i][j]));
			}
		}
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *sumO = json_object_get(rootJ, "octaveWrapAround");
		if (sumO) {
			octaveWrapAround = json_integer_value(sumO);			
		}

		json_t *sumT = json_object_get(rootJ, "justIntonation");
		if (sumT) {
			justIntonation = json_integer_value(sumT);			
		}

		json_t *sumL = json_object_get(rootJ, "shiftLogarithmic");
		if (sumL) {
			shiftLogarithmic = json_integer_value(sumL);			
		}


		for(int i=0;i<MAX_SCALES;i++) {
			for(int j=0;j<MAX_NOTES;j++) {
				char buf[100];
				char notebuf[100];
				strcpy(buf, "scaleWeight-");
				strcat(buf, scaleNames[i]);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				json_t *sumJ = json_object_get(rootJ, buf);
				if (sumJ) {
					scaleNoteWeighting[i][j] = json_real_value(sumJ);
				}
			}
		}		
	}
	

	void process(const ProcessArgs &args) override {
	
        if (writeScaleTrigger.process(params[WRITE_SCALE_PARAM].getValue())) {
			//Move everything back to shift 0 before saving
			int restoreShift =  -lastWeightShift;
			float shiftWeights[MAX_NOTES];
			if(restoreShift != 0) {
				for(int i=0;i<MAX_NOTES;i++) {
					if(noteActive[i]) {
						int noteCount = 0;
						int newIndex = i;
						int offset = 1;
						if(restoreShift < 0)
							offset = -1;
						bool noteFound = false;
						do {
							newIndex = (newIndex + offset) % MAX_NOTES;
							if (newIndex < 0)
								newIndex+=MAX_NOTES;
							if(noteActive[newIndex])
								noteCount+=1;
							if(noteCount >= std::abs(restoreShift))
								noteFound = true;
						} while(!noteFound);
						shiftWeights[newIndex] = params[NOTE_WEIGHT_PARAM+i].getValue();
					} else {
						shiftWeights[i] = params[NOTE_WEIGHT_PARAM+i].getValue();
					}
				}
			} else {
				for(int i=0;i<MAX_NOTES;i++) {
					shiftWeights[i] = params[NOTE_WEIGHT_PARAM+i].getValue();
				}
			}
			//Now adjust for key
			for(int i=0;i<MAX_NOTES;i++) {
				if(noteActive[i]) {
					scaleNoteWeighting[scale][i] = shiftWeights[i];
				} else {
					scaleNoteWeighting[scale][i] = 0.0f;
				} 			
			}			
		}		

		
		
        if (octaveWrapAroundTrigger.process(params[OCTAVE_WRAPAROUND_PARAM].getValue())) {
			octaveWrapAround = !octaveWrapAround;
		}		
		lights[OCTAVE_WRAPAROUND_LIGHT].value = octaveWrapAround;

        if (shiftScalingTrigger.process(params[SHIFT_SCALING_PARAM].getValue())) {
			shiftLogarithmic = !shiftLogarithmic;
		}		
		lights[SHIFT_LOGARITHMIC_SCALE_LIGHT].value = shiftLogarithmic;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,6.0f);

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

        
        // scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,11.0f);
        // if(scale != lastScale) {
        //     for(int i = 0;i<MAX_NOTES;i++) {
        //         currentScaleNoteWeighting[i] = scaleNoteWeighting[scale][i];
        //     }
        // }

        key = clamp(params[KEY_PARAM].getValue() + (inputs[KEY_INPUT].getVoltage() * MAX_NOTES / 10.0 * params[KEY_CV_ATTENUVERTER_PARAM].getValue()),0.0f,11.0f);


        if(key != lastKey || scale != lastScale) {
            for(int i = 0; i < MAX_NOTES;i++) {
                int noteValue = (i + key) % MAX_NOTES;
                if(noteValue < 0)
                    noteValue +=MAX_NOTES;
                noteActive[noteValue] = currentScaleNoteWeighting[i] > 0.0;
				noteScaleProbability[noteValue] = currentScaleNoteWeighting[i];
				params[NOTE_WEIGHT_PARAM+noteValue].setValue(noteScaleProbability[noteValue]);
            }
			lastScale = scale;
    	    lastKey = key;
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

		weightShift = params[SHIFT_PARAM].getValue();
		if(shiftLogarithmic) {
			double inputShift = inputs[SHIFT_INPUT].getVoltage() * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			inputShift = std::modf(inputShift, &unusedIntPart);
			inputShift = (std::pow(2,inputShift)-1.0) * 12;
			weightShift += inputShift;
		} else {
			weightShift += inputs[SHIFT_INPUT].getVoltage() * 2.2 * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
		}
		weightShift = clamp(weightShift,-11,11);
		if(lastWeightShift != weightShift) {
			int actualShift = weightShift - lastWeightShift;
			float shiftWeights[MAX_NOTES];
	        for(int i=0;i<MAX_NOTES;i++) {
				if(noteActive[i]) {
					int noteCount = 0;
					int newIndex = i;
					int offset = 1;
					if(actualShift < 0)
						offset = -1;
					bool noteFound = false;
					do {
						newIndex = (newIndex + offset) % MAX_NOTES;
						if (newIndex < 0)
							newIndex+=MAX_NOTES;
						if(noteActive[newIndex])
							noteCount+=1;
						if(noteCount >= std::abs(actualShift))
							noteFound = true;
					} while(!noteFound);
					shiftWeights[newIndex] = params[NOTE_WEIGHT_PARAM+i].getValue();
				} else {
					shiftWeights[i] = params[NOTE_WEIGHT_PARAM+i].getValue();
				}
			}
			for(int i=0;i<MAX_NOTES;i++) {
				params[NOTE_WEIGHT_PARAM+i].setValue(shiftWeights[i]);
			}
			lastWeightShift = weightShift;
		}

        for(int i=0;i<MAX_NOTES;i++) {
            if (noteActiveTrigger[i].process( params[NOTE_ACTIVE_PARAM+i].getValue())) {
                noteActive[i] = !noteActive[i];
            }

			float userProbability;
			if(noteActive[i]) {
	            userProbability = clamp(params[NOTE_WEIGHT_PARAM+i].getValue() + (inputs[NOTE_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);    
				lights[NOTE_ACTIVE_LIGHT+i*2].value = userProbability;    
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 0;    
			}
			else { 
				userProbability = 0.0;
				lights[NOTE_ACTIVE_LIGHT+i*2].value = 0;    
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 1;    
			}

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

				double quantitizedNoteCV;
				if(!justIntonation) {
					quantitizedNoteCV = randomNote / 12.0; 
				} else {
					int notePosition = randomNote - key;
					if(notePosition < 0) {
						notePosition += MAX_NOTES;
						octaveAdjust -=1;
					}
					quantitizedNoteCV =(noteTemperment[notePosition] / 1200.0) + (key /12.0); 
				}
				quantitizedNoteCV += octaveIn + octave + octaveAdjust; 
				outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV);
				outputs[WEIGHT_OUTPUT].setVoltage(clamp((params[NOTE_WEIGHT_PARAM+randomNote].getValue() + (inputs[NOTE_WEIGHT_INPUT+randomNote].getVoltage() / 10.0f) * 10.0f),0.0f,10.0f));
				if(lastQuantizedCV != quantitizedNoteCV) {
					noteChangePulse.trigger(1e-3);	
					lastQuantizedCV = quantitizedNoteCV;
				}        
			}
			outputs[NOTE_CHANGE_OUTPUT].setVoltage(noteChangePulse.process(1.0 / args.sampleRate) ? 10.0 : 0);
		}

	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void ProbablyNoteArabic::onReset() {
	clockTrigger.reset();
	for(int i = 0;i<MAX_SCALES;i++) {
		for(int j=0;j<MAX_NOTES;j++) {
			scaleNoteWeighting[i][j] = defaultScaleNoteWeighting[i][j];
		}
	}
}




struct ProbablyNoteArabicDisplay : TransparentWidget {
	ProbablyNoteArabic *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteArabicDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawKey(const DrawArgs &args, Vec pos, int key, int weightShift) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		char text[128];
		if(weightShift != 0) { 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xff));
			int shiftedKey = (key + weightShift) % MAX_NOTES;
			if (shiftedKey < 0)
				shiftedKey += MAX_NOTES;
			snprintf(text, sizeof(text), "%s -> %s", module->noteNames[key], module->noteNames[shiftedKey]);
		}
		else {
			nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
			snprintf(text, sizeof(text), "%s", module->noteNames[key]);
		}
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawScale(const DrawArgs &args, Vec pos, int scale, bool shifted) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		if(shifted) 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xff));
		else
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
			const float rotate90 = (M_PI) / 2.0;

			float opacity = noteInitialProbability[i] * 255;
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			if(i == module->probabilityNote) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			}
			switch(i) {
				case 0:
				//C
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,330);
				nvgLineTo(args.vg,142, 330);
				nvgLineTo(args.vg,142, 302);
				nvgLineTo(args.vg,62, 302);
				nvgArc(args.vg,50.5,302,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,50.5, 314);		
				nvgLineTo(args.vg,1, 314);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 1:
				//C#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,314.5);
				nvgLineTo(args.vg,50.5,314.5);
				nvgArc(args.vg,50.5,302,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				//nvgLineTo(args.vg,62,290);		
				nvgLineTo(args.vg,1,289.5);
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
				nvgArc(args.vg,50.5,274,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,1,286.5);
				nvgLineTo(args.vg,1,289.5);
				nvgArc(args.vg,50.5,302,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 3:
				//D#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,286.5);
				nvgLineTo(args.vg,50.5,286.5);
				nvgArc(args.vg,50.5,274,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,1,261.5);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 4:
				//E
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,274);
				nvgLineTo(args.vg,142,274);
				nvgLineTo(args.vg,142,246);		
				nvgLineTo(args.vg,1,246);
				nvgLineTo(args.vg,1, 261.5);		
				nvgLineTo(args.vg,50.5, 261.5);
				nvgArc(args.vg,50.5,274,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 5:
				//F 
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,246);
				nvgLineTo(args.vg,142, 246);
				nvgLineTo(args.vg,142, 218);
				nvgLineTo(args.vg,62, 218);
				nvgArc(args.vg,50.5,218,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,50.5, 230.5);		
				nvgLineTo(args.vg,1, 230.5);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 6:
				//F#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,230.5);
				nvgLineTo(args.vg,50.5,230.5);
				nvgArc(args.vg,50.5,218,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				//nvgLineTo(args.vg,62,206);		
				nvgLineTo(args.vg,1,205.5);
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
				nvgArc(args.vg,50.5,190,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,1,202.5);
				nvgLineTo(args.vg,1,205.5);
				nvgArc(args.vg,50.5,218,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 8:
				//G#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,202.5);
				nvgLineTo(args.vg,50.5,202.5);
				nvgArc(args.vg,50.5,190,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,1,178);
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
				nvgArc(args.vg,50.5,162,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,1,174.5);
				nvgLineTo(args.vg,1,177.5);
				nvgArc(args.vg,50.5,190,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 10:
				//A#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,1,174.5);
				nvgLineTo(args.vg,50.5,174.5);
				nvgArc(args.vg,50.5,162,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,1,150);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 11:
				//B
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,62,162);
				nvgLineTo(args.vg,142,162);
				nvgLineTo(args.vg,142,134);		
				nvgLineTo(args.vg,1,134);
				nvgLineTo(args.vg,1,149.5);		
				nvgLineTo(args.vg,50.5,149.5);
				nvgArc(args.vg,50.5,162,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

			}
		}

	

	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawScale(args, Vec(4,83), module->scale, module->weightShift != 0);
		drawKey(args, Vec(72,83), module->key, module->weightShift);
		//drawOctave(args, Vec(66, 280), module->octave);
		drawNoteRange(args, module->noteInitialProbability);
	}
};

struct ProbablyNoteArabicWidget : ModuleWidget {
	ProbablyNoteArabicWidget(ProbablyNoteArabic *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteArabic.svg")));


		{
			ProbablyNoteArabicDisplay *display = new ProbablyNoteArabicDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(8, 345), module, ProbablyNoteArabic::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 345), module, ProbablyNoteArabic::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(70, 345), module, ProbablyNoteArabic::EXTERNAL_RANDOM_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,25), module, ProbablyNoteArabic::SHIFT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,51), module, ProbablyNoteArabic::SHIFT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 29), module, ProbablyNoteArabic::SHIFT_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(68,25), module, ProbablyNoteArabic::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,51), module, ProbablyNoteArabic::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 29), module, ProbablyNoteArabic::SPREAD_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(128, 25), module, ProbablyNoteArabic::DISTRIBUTION_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,51), module, ProbablyNoteArabic::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 29), module, ProbablyNoteArabic::DISTRIBUTION_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNoteArabic::LOWER_JINS_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNoteArabic::LOWER_JINS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNoteArabic::LOWER_JINS_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(68,86), module, ProbablyNoteArabic::KEY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,112), module, ProbablyNoteArabic::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 90), module, ProbablyNoteArabic::KEY_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(128,86), module, ProbablyNoteArabic::OCTAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,112), module, ProbablyNoteArabic::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 90), module, ProbablyNoteArabic::OCTAVE_INPUT));

		addParam(createParam<TL1105>(Vec(15, 113), module, ProbablyNoteArabic::WRITE_SCALE_PARAM));


		addParam(createParam<LEDButton>(Vec(10, 48), module, ProbablyNoteArabic::SHIFT_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 49.5), module, ProbablyNoteArabic::SHIFT_LOGARITHMIC_SCALE_LIGHT));
		

		addParam(createParam<LEDButton>(Vec(130, 113), module, ProbablyNoteArabic::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(131.5, 114.5), module, ProbablyNoteArabic::OCTAVE_WRAPAROUND_LIGHT));




        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,306), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+0));			
		addInput(createInput<FWPortInSmall>(Vec(118, 307), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+0));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,292), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+1));			
		addInput(createInput<FWPortInSmall>(Vec(2, 293), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+1));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,278), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+2));			
		addInput(createInput<FWPortInSmall>(Vec(118, 279), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+2));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,264), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+3));			
		addInput(createInput<FWPortInSmall>(Vec(2, 265), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+3));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,250), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+4));			
		addInput(createInput<FWPortInSmall>(Vec(118, 251), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+4));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,222), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+5));			
		addInput(createInput<FWPortInSmall>(Vec(118, 223), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+5));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,208), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+6));			
		addInput(createInput<FWPortInSmall>(Vec(2, 209), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+6));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,194), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+7));			
		addInput(createInput<FWPortInSmall>(Vec(118, 195), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+7));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,180), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+8));			
		addInput(createInput<FWPortInSmall>(Vec(2, 181), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+8));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,166), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+9));			
		addInput(createInput<FWPortInSmall>(Vec(118, 167), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+9));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(22,152), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+10));			
		addInput(createInput<FWPortInSmall>(Vec(2, 153), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+10));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,138), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+11));			
		addInput(createInput<FWPortInSmall>(Vec(118, 139), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+11));


		addParam(createParam<LEDButton>(Vec(72, 307), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+0));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 308.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+0));
		addParam(createParam<LEDButton>(Vec(44, 293), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+1));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 294.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+2));
		addParam(createParam<LEDButton>(Vec(72, 279), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+2));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 280.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+4));
		addParam(createParam<LEDButton>(Vec(44, 265), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+3));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 266.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+6));
		addParam(createParam<LEDButton>(Vec(72, 251), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+4));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 252.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+8));
		addParam(createParam<LEDButton>(Vec(72, 223), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+5));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 224.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+10));
		addParam(createParam<LEDButton>(Vec(44, 209), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+6));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 210.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+12));
		addParam(createParam<LEDButton>(Vec(72, 195), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+7));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 196.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+14));
		addParam(createParam<LEDButton>(Vec(44, 181), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+8));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 182.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+16));
		addParam(createParam<LEDButton>(Vec(72, 167), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+9));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 168.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+18));
		addParam(createParam<LEDButton>(Vec(44, 153), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+10));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 154.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+20));
		addParam(createParam<LEDButton>(Vec(72, 139), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+11));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 140.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+22));

		

		addOutput(createOutput<FWPortInSmall>(Vec(160, 345),  module, ProbablyNoteArabic::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(130, 345),  module, ProbablyNoteArabic::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(100, 345),  module, ProbablyNoteArabic::NOTE_CHANGE_OUTPUT));

	}

	// struct PNLoadScaleItem : MenuItem {
	// 	ProbablyNoteArabic *module;
	// 	void onAction(const event::Action &e) override {
	// 	}
	// };

	// struct PNSaveScaleItem : MenuItem {
	// 	ProbablyNoteArabic *module;
	// 	void onAction(const event::Action &e) override {
	// 	}
	// };

	// struct PNDeleteScaleItem : MenuItem {
	// 	ProbablyNoteArabic *module;
	// 	void onAction(const event::Action &e) override {
	// 	}
	// };

	// struct PNLoadScaleGroupItem : MenuItem {
	// 	ProbablyNoteArabic *module;
	// 	void onAction(const event::Action &e) override {
	// 		std::string dir = module->lastPath.empty() ? asset::user("") : rack::string::directory(module->lastPath);
	// 		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
	// 		if (path) {
	// 			//module->loadSample(path);
	// 			free(path);
	// 		}
	// 	}
	// };

	// struct PNSaveScaleGroupItem : MenuItem {
	// 	ProbablyNoteArabic *module;
	// 	void onAction(const event::Action &e) override {
	// 		std::string dir = module->lastPath.empty() ? asset::user("") : rack::string::directory(module->lastPath);
	// 		char *path = osdialog_file(OSDIALOG_SAVE, dir.c_str(), NULL, NULL);
	// 		if (path) {
	// 			//module->loadSample(path);
	// 			std::ofstream myfile (path);
	// 			if (myfile.is_open())
	// 			{
	// 				myfile << "This is a line.\n";
	// 				myfile << "This is another line.\n";
	// 				myfile.close();
	// 			}
	// 			free(path);
	// 		}
	// 	}
	// };


	
	
	// void appendContextMenu(Menu *menu) override {
	// 	MenuLabel *spacerLabel = new MenuLabel();
	// 	menu->addChild(spacerLabel);

	// 	ProbablyNoteArabic *module = dynamic_cast<ProbablyNoteArabic*>(this->module);
	// 	assert(module);

	// 	MenuLabel *themeLabel = new MenuLabel();
	// 	themeLabel->text = "Scales";
	// 	menu->addChild(themeLabel);

	// 	PNLoadScaleItem *pnLoadScaleItem = new PNLoadScaleItem();
	// 	pnLoadScaleItem->text = "Load Scale";// 
	// 	pnLoadScaleItem->module = module;
	// 	menu->addChild(pnLoadScaleItem);

	// 	PNDeleteScaleItem *pnDeleteScaleItem = new PNDeleteScaleItem();
	// 	pnDeleteScaleItem->text = "Delete Scale";// 
	// 	pnDeleteScaleItem->module = module;
	// 	menu->addChild(pnDeleteScaleItem);

	// 	PNLoadScaleGroupItem *pnLoadScaleGroupItem = new PNLoadScaleGroupItem();
	// 	pnLoadScaleGroupItem->text = "Load Scale Group";// 
	// 	pnLoadScaleGroupItem->module = module;
	// 	menu->addChild(pnLoadScaleGroupItem);

	// 	PNSaveScaleGroupItem *pnSaveScaleGroupItem = new PNSaveScaleGroupItem();
	// 	pnSaveScaleGroupItem->text = "Save Scale Group";// 
	// 	pnSaveScaleGroupItem->module = module;
	// 	menu->addChild(pnSaveScaleGroupItem);


			
	// }
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteArabic = createModel<ProbablyNoteArabic, ProbablyNoteArabicWidget>("ProbablyNoteArabic");
    