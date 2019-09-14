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
#define MAX_TEMPERMENTS 2

using namespace frozenwasteland::dsp;

struct ProbablyNote : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		DISTRIBUTION_PARAM,
		DISTRIBUTION_CV_ATTENUVERTER_PARAM,
        SHIFT_PARAM,
		SHIFT_CV_ATTENUVERTER_PARAM,
        SCALE_PARAM,
		SCALE_CV_ATTENUVERTER_PARAM,
        KEY_PARAM,
		KEY_CV_ATTENUVERTER_PARAM,
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,
        WRITE_SCALE_PARAM,
        OCTAVE_WRAPAROUND_PARAM,
		TEMPERMENT_PARAM,
		SHIFT_SCALING_PARAM,
		KEY_SCALING_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NUM_PARAMS = NOTE_WEIGHT_PARAM + MAX_NOTES
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		DISTRIBUTION_INPUT,
		SHIFT_INPUT,
        SCALE_INPUT,
        KEY_INPUT,
        OCTAVE_INPUT,
		TEMPERMENT_INPUT,
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
        KEY_LOGARITHMIC_SCALE_LIGHT,
        NOTE_ACTIVE_LIGHT,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_NOTES*2
	};

	// Expander
	float consumerMessage[8] = {};// this module must read from here
	float producerMessage[8] = {};// mother will write into here



	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
	const char* scaleNames[MAX_NOTES] = {"Chromatic","Whole Tone","Aeolian (minor)","Locrian","Ionian (Major)","Dorian","Phrygian","Lydian","Mixolydian","Gypsy","Hungarian","Blues"};
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
    double noteTemperment[MAX_TEMPERMENTS][MAX_NOTES] = {
        {0,100,200,300,400,500,600,700,800,900,1000,1100},
        {0,111.73,203.91,315.64,386.61,498.04,582.51,701.955,813.69,884.36,996.09,1088.27},
    };
    

	
	dsp::SchmittTrigger clockTrigger,writeScaleTrigger,octaveWrapAroundTrigger,tempermentTrigger,shiftScalingTrigger,keyScalingTrigger,noteActiveTrigger[MAX_NOTES]; 
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
	int transposedKey = 0;
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
	bool keyLogarithmic = false;

	bool generateChords = false;
	float dissonance5Prbability = 0.0;
	float dissonance7Prbability = 0.0;
	float suspensionProbability = 0.0;
	float inversionProbability = 0.0;
	float externalDissonance5Random = 0.0;
	float externalDissonance7Random = 0.0;
	float externalSuspensionRandom = 0.0;


	std::string lastPath;
    

	ProbablyNote() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNote::SPREAD_PARAM, 0.0, 6.0, 0.0,"Spread");
        configParam(ProbablyNote::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNote::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNote::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNote::SHIFT_PARAM, -11.0, 11.0, 0.0,"Weight Shift");
        configParam(ProbablyNote::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNote::SCALE_PARAM, 0.0, 11.0, 0.0,"Scale");
        configParam(ProbablyNote::SCALE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Scale CV Attenuation","%",0,100);
		configParam(ProbablyNote::KEY_PARAM, 0.0, 11.0, 0.0,"Key");
        configParam(ProbablyNote::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNote::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
        configParam(ProbablyNote::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNote::OCTAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Octave Wraparound");
		configParam(ProbablyNote::TEMPERMENT_PARAM, 0.0, 1.0, 0.0,"Just Intonation");

        srand(time(NULL));

        for(int i=0;i<MAX_NOTES;i++) {
            configParam(ProbablyNote::NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
            configParam(ProbablyNote::NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight");		
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

	double quantizedCVValue(int note, int key, bool useJustIntonation) {
		if(!useJustIntonation) {
			return (note / 12.0); 
		} else {
			int octaveAdjust = 0;
			int notePosition = note - key;
			if(notePosition < 0) {
				notePosition += MAX_NOTES;
				octaveAdjust -=1;
			}
			return (noteTemperment[1][notePosition] / 1200.0) + (key /12.0) + octaveAdjust; 
		}
	}

	int nextActiveNote(int note,int offset) {
		if(offset == 0)
			return note;
		int offsetNote = note;
		bool noteOk = false;
		int noteCount = 0;
		do {
			offsetNote = (offsetNote + 1) % MAX_NOTES;
			if(noteActive[offsetNote]) {
				noteCount +=1;
			}
			noteOk = noteCount == offset;
		} while(!noteOk);
		return offsetNote;
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

		if (tempermentTrigger.process(params[TEMPERMENT_PARAM].getValue() + inputs[TEMPERMENT_INPUT].getVoltage())) {
			justIntonation = !justIntonation;
		}		
		lights[JUST_INTONATION_LIGHT].value = justIntonation;
		
        if (octaveWrapAroundTrigger.process(params[OCTAVE_WRAPAROUND_PARAM].getValue())) {
			octaveWrapAround = !octaveWrapAround;
		}		
		lights[OCTAVE_WRAPAROUND_LIGHT].value = octaveWrapAround;

        if (shiftScalingTrigger.process(params[SHIFT_SCALING_PARAM].getValue())) {
			shiftLogarithmic = !shiftLogarithmic;
		}		
		lights[SHIFT_LOGARITHMIC_SCALE_LIGHT].value = shiftLogarithmic;

        if (keyScalingTrigger.process(params[KEY_SCALING_PARAM].getValue())) {
			keyLogarithmic = !keyLogarithmic;
		}		
		lights[KEY_LOGARITHMIC_SCALE_LIGHT].value = keyLogarithmic;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,6.0f);

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

        
        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,11.0f);
        if(scale != lastScale) {
            for(int i = 0;i<MAX_NOTES;i++) {
                currentScaleNoteWeighting[i] = scaleNoteWeighting[scale][i];
            }
        }

        key = params[KEY_PARAM].getValue();
		if(keyLogarithmic) {
			double inputKey = inputs[KEY_INPUT].getVoltage() * params[KEY_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			inputKey = std::modf(inputKey, &unusedIntPart);
			inputKey = std::ceil(inputKey * 12);
			key += (int)inputKey;
		} else {
			key += inputs[KEY_INPUT].getVoltage() * MAX_NOTES / 10.0 * params[KEY_CV_ATTENUVERTER_PARAM].getValue();
		}
        key = clamp(key,0,11);

        if(key != lastKey || scale != lastScale) {
            for(int i = 0; i < MAX_NOTES;i++) {
                int noteValue = (i + key) % MAX_NOTES;
                if(noteValue < 0)
                    noteValue +=MAX_NOTES;
                noteActive[noteValue] = currentScaleNoteWeighting[i] > 0.0;
				noteScaleProbability[noteValue] = currentScaleNoteWeighting[i];
				params[NOTE_WEIGHT_PARAM+noteValue].setValue(noteScaleProbability[noteValue]);
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

		weightShift = params[SHIFT_PARAM].getValue();
		if(shiftLogarithmic) {
			double inputShift = inputs[SHIFT_INPUT].getVoltage() * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			inputShift = std::modf(inputShift, &unusedIntPart);
			inputShift = std::ceil(inputShift * 12);

			int desiredKey = (key + (int)inputShift) % MAX_NOTES;
			if (desiredKey < 0)
				desiredKey += MAX_NOTES;

			//Find nearest active note to shift amount
			while (!noteActive[desiredKey]) {
				desiredKey = (desiredKey + 1) % MAX_NOTES;				
			}

			//Count how many active notes it takes to get there
			int noteCount = 0;
			while (desiredKey != key) {
				desiredKey -= 1;
				if(desiredKey < 0)
					desiredKey += MAX_NOTES;
				if(noteActive[desiredKey]) 
					noteCount +=1;
			}

			weightShift += noteCount;
		} else {
			weightShift += inputs[SHIFT_INPUT].getVoltage() * 2.2 * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
		}
		weightShift = clamp(weightShift,-11,11);

	

		//Shift Probabilities
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

		//Find New Key
		if(scale != lastScale || key != lastKey || weightShift != lastWeightShift) {
			if(weightShift != 0) {
				int noteCount = 0;
				int newIndex = key;
				int offset = 1;
				if(weightShift < 0)
					offset = -1;
				bool noteFound = false;
				do {
					newIndex = (newIndex + offset) % MAX_NOTES;
					if (newIndex < 0)
						newIndex+=MAX_NOTES;
					if(noteActive[newIndex])
						noteCount+=1;
					if(noteCount >= std::abs(weightShift))
						noteFound = true;
				} while(!noteFound);
				transposedKey = newIndex;
			} else {
				transposedKey = key;
			}

			lastKey = key;
			lastScale = scale;
			lastWeightShift = weightShift;
		}


		//Get Expander Info
		if(rightExpander.module && rightExpander.module->model == modelPNChordExpander) {	
			generateChords = true;		
			float *message = (float*) rightExpander.module->leftExpander.consumerMessage;
			dissonance5Prbability = message[0];
			dissonance7Prbability = message[1];
			suspensionProbability = message[2];
			externalDissonance5Random = message[4];
			externalDissonance7Random = message[5];
			externalSuspensionRandom = message[6];
		} else {
			generateChords = false;
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

				double quantitizedNoteCV = quantizedCVValue(randomNote,key,justIntonation);
				// if(!justIntonation) {
				// 	quantitizedNoteCV = randomNote / 12.0; 
				// } else {
				// 	int notePosition = randomNote - key;
				// 	if(notePosition < 0) {
				// 		notePosition += MAX_NOTES;
				// 		octaveAdjust -=1;
				// 	}
				// 	quantitizedNoteCV =(noteTemperment[1][notePosition] / 1200.0) + (key /12.0); 
				// }
				quantitizedNoteCV += octaveIn + octave + octaveAdjust;
				
				
				//Chord Stuff
				if(!generateChords) { 
					outputs[QUANT_OUTPUT].setChannels(1);
					outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV,0);
				} else {
					outputs[QUANT_OUTPUT].setChannels(4);
					outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV,0);

					float rndDissonance5 = ((float) rand()/RAND_MAX);
					if(externalDissonance5Random != -1)
						rndDissonance5 = externalDissonance5Random;

					float rndDissonance7 = ((float) rand()/RAND_MAX);
					if(externalDissonance7Random != -1)
						rndDissonance7 = externalDissonance7Random;

					float rndSuspension = ((float) rand()/RAND_MAX);
					if(externalSuspensionRandom != -1)
						rndSuspension = externalSuspensionRandom;
					//float rndInversion = ((float) rand()/RAND_MAX);

					int secondNote = nextActiveNote(randomNote,2);
					if(rndSuspension < suspensionProbability) {
						float secondOrFourth = ((float) rand()/RAND_MAX);
						if(secondOrFourth > 0.5) {
							secondNote = nextActiveNote(randomNote,3);
						} else {
							secondNote = nextActiveNote(randomNote,1);
						}					
					}
					int secondNoteOctave = 0;
					if(secondNote < randomNote) {
						secondNoteOctave +=1;
					}

					int thirdNote = nextActiveNote(randomNote,4);
					if(rndDissonance5 < dissonance5Prbability) {
						float flatOrSharp = ((float) rand()/RAND_MAX);
						int offset = -1;
						if(flatOrSharp > 0.5) {
							offset = 1;
						}
						thirdNote = (thirdNote + offset) % MAX_NOTES;
						if(thirdNote < 0)
							thirdNote += MAX_NOTES;
					}
					int thirdNoteOctave = 0;
					if(thirdNote < randomNote) {
						thirdNoteOctave +=1;
					}

					int fourthNote = nextActiveNote(randomNote,6);
					if(rndDissonance7 < dissonance7Prbability) {
						float flatOrSharp = ((float) rand()/RAND_MAX);
						int offset = -1;
						if(flatOrSharp > 0.5) {
							offset = 1;
						}
						fourthNote = (fourthNote + offset) % MAX_NOTES;
						if(fourthNote < 0)
							fourthNote += MAX_NOTES;
					}
					int fourthNoteOctave = 0;
					if(fourthNote < randomNote) {
						fourthNoteOctave +=1;
					}

					outputs[QUANT_OUTPUT].setVoltage((double)secondNote/12.0 + octaveIn + octave + octaveAdjust + secondNoteOctave,1);
					outputs[QUANT_OUTPUT].setVoltage((double)thirdNote/12.0 + octaveIn + octave + octaveAdjust + thirdNoteOctave,2);
					outputs[QUANT_OUTPUT].setVoltage((double)fourthNote/12.0 + octaveIn + octave + octaveAdjust + fourthNoteOctave,3);
				}

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

void ProbablyNote::onReset() {
	clockTrigger.reset();
	for(int i = 0;i<MAX_SCALES;i++) {
		for(int j=0;j<MAX_NOTES;j++) {
			scaleNoteWeighting[i][j] = defaultScaleNoteWeighting[i][j];
		}
	}
}




struct ProbablyNoteDisplay : TransparentWidget {
	ProbablyNote *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawKey(const DrawArgs &args, Vec pos, int key, int transposedKey) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		char text[128];
		if(key != transposedKey) { 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xff));
			snprintf(text, sizeof(text), "%s -> %s", module->noteNames[key], module->noteNames[transposedKey]);
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

		drawScale(args, Vec(4,84), module->lastScale, module->lastWeightShift != 0);
		drawKey(args, Vec(72,84), module->lastKey, module->transposedKey);
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



		addInput(createInput<FWPortInSmall>(Vec(8, 345), module, ProbablyNote::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 345), module, ProbablyNote::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(70, 345), module, ProbablyNote::EXTERNAL_RANDOM_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,25), module, ProbablyNote::SHIFT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,51), module, ProbablyNote::SHIFT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 29), module, ProbablyNote::SHIFT_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(68,25), module, ProbablyNote::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,51), module, ProbablyNote::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 29), module, ProbablyNote::SPREAD_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(128, 25), module, ProbablyNote::DISTRIBUTION_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,51), module, ProbablyNote::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 29), module, ProbablyNote::DISTRIBUTION_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNote::SCALE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNote::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNote::SCALE_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(68,86), module, ProbablyNote::KEY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,112), module, ProbablyNote::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 90), module, ProbablyNote::KEY_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(128,86), module, ProbablyNote::OCTAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,112), module, ProbablyNote::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 90), module, ProbablyNote::OCTAVE_INPUT));

		addParam(createParam<TL1105>(Vec(15, 113), module, ProbablyNote::WRITE_SCALE_PARAM));


		addParam(createParam<LEDButton>(Vec(10, 48), module, ProbablyNote::SHIFT_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 49.5), module, ProbablyNote::SHIFT_LOGARITHMIC_SCALE_LIGHT));

		addParam(createParam<LEDButton>(Vec(70, 113), module, ProbablyNote::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(71.5, 114.5), module, ProbablyNote::KEY_LOGARITHMIC_SCALE_LIGHT));


		addParam(createParam<LEDButton>(Vec(130, 113), module, ProbablyNote::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(131.5, 114.5), module, ProbablyNote::OCTAVE_WRAPAROUND_LIGHT));

		addParam(createParam<LEDButton>(Vec(155, 286), module, ProbablyNote::TEMPERMENT_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(156.5, 287.5), module, ProbablyNote::JUST_INTONATION_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(156, 307), module, ProbablyNote::TEMPERMENT_INPUT));



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
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 308.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+0));
		addParam(createParam<LEDButton>(Vec(44, 293), module, ProbablyNote::NOTE_ACTIVE_PARAM+1));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 294.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+2));
		addParam(createParam<LEDButton>(Vec(72, 279), module, ProbablyNote::NOTE_ACTIVE_PARAM+2));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 280.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+4));
		addParam(createParam<LEDButton>(Vec(44, 265), module, ProbablyNote::NOTE_ACTIVE_PARAM+3));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 266.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+6));
		addParam(createParam<LEDButton>(Vec(72, 251), module, ProbablyNote::NOTE_ACTIVE_PARAM+4));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 252.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+8));
		addParam(createParam<LEDButton>(Vec(72, 223), module, ProbablyNote::NOTE_ACTIVE_PARAM+5));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 224.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+10));
		addParam(createParam<LEDButton>(Vec(44, 209), module, ProbablyNote::NOTE_ACTIVE_PARAM+6));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 210.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+12));
		addParam(createParam<LEDButton>(Vec(72, 195), module, ProbablyNote::NOTE_ACTIVE_PARAM+7));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 196.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+14));
		addParam(createParam<LEDButton>(Vec(44, 181), module, ProbablyNote::NOTE_ACTIVE_PARAM+8));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 182.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+16));
		addParam(createParam<LEDButton>(Vec(72, 167), module, ProbablyNote::NOTE_ACTIVE_PARAM+9));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 168.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+18));
		addParam(createParam<LEDButton>(Vec(44, 153), module, ProbablyNote::NOTE_ACTIVE_PARAM+10));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(45.5, 154.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+20));
		addParam(createParam<LEDButton>(Vec(72, 139), module, ProbablyNote::NOTE_ACTIVE_PARAM+11));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(73.5, 140.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+22));

		

		addOutput(createOutput<FWPortInSmall>(Vec(159, 345),  module, ProbablyNote::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(130, 345),  module, ProbablyNote::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(100, 345),  module, ProbablyNote::NOTE_CHANGE_OUTPUT));

	}

	// struct PNLoadScaleItem : MenuItem {
	// 	ProbablyNote *module;
	// 	void onAction(const event::Action &e) override {
	// 	}
	// };

	// struct PNSaveScaleItem : MenuItem {
	// 	ProbablyNote *module;
	// 	void onAction(const event::Action &e) override {
	// 	}
	// };

	// struct PNDeleteScaleItem : MenuItem {
	// 	ProbablyNote *module;
	// 	void onAction(const event::Action &e) override {
	// 	}
	// };

	// struct PNLoadScaleGroupItem : MenuItem {
	// 	ProbablyNote *module;
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
	// 	ProbablyNote *module;
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

	// 	ProbablyNote *module = dynamic_cast<ProbablyNote*>(this->module);
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
Model *modelProbablyNote = createModel<ProbablyNote, ProbablyNoteWidget>("ProbablyNote");
    