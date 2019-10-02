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

#define MAX_NOTES 13
#define MAX_SCALES 10
#define MAX_TEMPERMENTS 2

using namespace frozenwasteland::dsp;

struct ProbablyNoteBP : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		SLANT_PARAM,
		SLANT_CV_ATTENUVERTER_PARAM,
		DISTRIBUTION_PARAM,
		DISTRIBUTION_CV_ATTENUVERTER_PARAM,
        SHIFT_PARAM,
		SHIFT_CV_ATTENUVERTER_PARAM,
        SCALE_PARAM,
		SCALE_CV_ATTENUVERTER_PARAM,
        KEY_PARAM,
		KEY_CV_ATTENUVERTER_PARAM,
        TRITAVE_PARAM,
		TRITAVE_CV_ATTENUVERTER_PARAM,
        RESET_SCALE_PARAM,
        TRITAVE_WRAPAROUND_PARAM,
		OCTAVE_TO_TRITAVE_PARAM,
		TEMPERMENT_PARAM,
		SHIFT_SCALING_PARAM,
		KEY_SCALING_PARAM,
		WEIGHT_SCALING_PARAM,
		MASTER_WEIGHT_ADJUST_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NUM_PARAMS = NOTE_WEIGHT_PARAM + MAX_NOTES
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		SLANT_INPUT,
		DISTRIBUTION_INPUT,
		SHIFT_INPUT,
        SCALE_INPUT,
        KEY_INPUT,
        TRITAVE_INPUT,
		TEMPERMENT_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
		TRITAVE_WRAP_INPUT,
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
        TRITAVE_WRAPAROUND_LIGHT,
		JUST_INTONATION_LIGHT,
		SHIFT_LOGARITHMIC_SCALE_LIGHT,
        KEY_LOGARITHMIC_SCALE_LIGHT,
		TRITAVE_MAPPING_LIGHT,
        NOTE_ACTIVE_LIGHT,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_NOTES*2
	};


	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","E","F","F#/Gb","G","H","H#/Jb","J","A","A#/Bb","B"};
	const char* scaleNames[MAX_SCALES] = {"Chromatic","Lambda 1","Lambda 2","Lambda 3","Lambda 4","Lambda 5","Lambda 6","Lambda 7","Lambda 8","Lambda 9"};
	float defaultScaleNoteWeighting[MAX_SCALES][MAX_NOTES] = {
		{1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,0.2,0.5,0.4,0,0.8,0.2,0,0.3,0.2,0,0.2},
		{1,0.2,0.5,0,0.4,0.8,0,0.2,0.3,0,0.2,0.2,0},
		{1,0.2,0,0.5,0.4,0,0.8,0.2,0,0.3,0.2,0,0.2},
		{1,0,0.2,0.5,0,0.4,0.8,0,0.2,0.3,0,0.2,0.2},
		{1,0.2,0,0.5,0.4,0,0.8,0.2,0,0.3,0.2,0.2,0},
		{1,0,0.2,0.5,0,0.4,0.8,0,0.2,0.3,0.2,0,0.2},
		{1,0.2,0,0.5,0.4,0,0.8,0.2,0.3,0,0.2,0.2,0},
		{1,0,0.2,0.5,0,0.4,0.8,0.2,0,0.3,0.2,0,0.2},
		{1,0.2,0,0.5,0.4,0.8,0,0.2,0.3,0,0.2,0.2,0}
	}; 
		

    float scaleNoteWeighting[MAX_SCALES][MAX_NOTES]; 

	const char* tempermentNames[MAX_NOTES] = {"Equal","Just"};
    double noteTemperment[MAX_TEMPERMENTS][MAX_NOTES] = {
        {0,146,293,439,585,732,878,1024,1170,1317,1463,1609,1756},
        {0,133,301.85,435,583,737,884,1018,1165,1319,1467,1600,1769},
    };
    
	const double tritaveFrequency = 1.5849625;
	
	dsp::SchmittTrigger clockTrigger,resetScaleTrigger,tritaveWrapAroundTrigger,tempermentTrigger,tritaveMappingTrigger,shiftScalingTrigger,keyScalingTrigger,noteActiveTrigger[MAX_NOTES]; 
	dsp::PulseGenerator noteChangePulse;
    GaussianNoiseGenerator _gauss;
 
    bool tritaveWrapAround = false;
    bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteInitialProbability[MAX_NOTES] = {0.0f};
    float currentScaleNoteWeighting[MAX_NOTES] = {0.0};
	float actualProbability[MAX_NOTES] = {0.0};
	int controlIndex[MAX_NOTES] = {0};


    int scale = 0;
    int lastScale = -1;
    int key = 0;
    int lastKey = -1;
    int transposedKey = 0;
	int tritave = 0;
	int weightShift = 0;
	int lastWeightShift = 0;
    int spread = 0;
	float upperSpread = 0.0;
	float lowerSpread = 0.0;
	float slant = 0;
	float focus = 0; 
	int currentNote = 0;
	int probabilityNote = 0;
	double lastQuantizedCV = 0;
	bool resetTriggered = false;
	int lastNote = -1;
	int lastSpread = -1;
	float lastSlant = -1;
	float lastFocus = -1;
	bool justIntonation = false;
	bool shiftLogarithmic = false;
	bool keyLogarithmic = false;
	bool tritaveMapping = true;


	std::string lastPath;
    

	ProbablyNoteBP() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteBP::SPREAD_PARAM, 0.0, 6.0, 0.0,"Spread");
        configParam(ProbablyNoteBP::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteBP::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant");
        configParam(ProbablyNoteBP::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteBP::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNoteBP::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNoteBP::SHIFT_PARAM, -12.0, 12.0, 0.0,"Weight Shift");
        configParam(ProbablyNoteBP::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteBP::SCALE_PARAM, 0.0, 9.0, 0.0,"Scale");
        configParam(ProbablyNoteBP::SCALE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Scale CV Attenuation","%",0,100);
		configParam(ProbablyNoteBP::KEY_PARAM, 0.0, 12.0, 0.0,"Key");
        configParam(ProbablyNoteBP::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNoteBP::TRITAVE_PARAM, -4.0, 4.0, 0.0,"Tritave");
        configParam(ProbablyNoteBP::TRITAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Tritave CV Attenuation","%",0,100);
		configParam(ProbablyNoteBP::TRITAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Tritave Wraparound");
		configParam(ProbablyNoteBP::TEMPERMENT_PARAM, 0.0, 1.0, 0.0,"Just Intonation");
		configParam(ProbablyNoteBP::WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Weight Scaling","%",0,100);

        srand(time(NULL));

        for(int i=0;i<MAX_NOTES;i++) {
            configParam(ProbablyNoteBP::NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
            configParam(ProbablyNoteBP::NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight");		
        }

		onReset();
	}

    float lerp(float v0, float v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,float scaling,float randomIn) {
        float weightTotal = 0.0f;
		float linearWeight, logWeight, weight;
            
        for(int i = 0;i <MAX_NOTES; i++) {
			linearWeight = weights[i];
			logWeight = std::log10(weights[i]*10 + 1);
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

        int chosenWeight = -1;        
        float rnd = randomIn * weightTotal;
        for(int i = 0;i <MAX_NOTES;i++ ) {
			linearWeight = weights[i];
			logWeight = std::log10(weights[i]*10 + 1);
			weight = lerp(linearWeight,logWeight,scaling);

            if(rnd < weight) {
                chosenWeight = i;
                break;
            }
            rnd -= weights[i];
        }
        return chosenWeight;
    }

	double quantizedCVValue(int note, int key, bool useJustIntonation) {
		int tempermemtIndex = useJustIntonation ? 1 : 0;

		int tritaveAdjust = 0;
		int notePosition = note - key;
		if(notePosition < 0) {
			notePosition += MAX_NOTES;
			tritaveAdjust -=1;
		}
		return (noteTemperment[tempermemtIndex][notePosition] / 1200.0) + (key / 13.0) + tritaveAdjust; 
	}

		int nextActiveNote(int note,int offset) {
		if(offset == 0)
			return note;
		int offsetNote = note;
		bool noteOk = false;
		int noteCount = 0;
		int notesSearched = 0;
		do {
			offsetNote = (offsetNote + 1) % MAX_NOTES;
			notesSearched +=1;
			if(noteActive[offsetNote]) {
				noteCount +=1;
			}
			noteOk = noteCount == offset;
		} while(!noteOk && notesSearched < MAX_NOTES);
		return offsetNote;
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "tritaveWrapAround", json_integer((int) tritaveWrapAround));
		json_object_set_new(rootJ, "justIntonation", json_integer((int) justIntonation));
		json_object_set_new(rootJ, "shiftLogarithmic", json_integer((int) shiftLogarithmic));
		json_object_set_new(rootJ, "keyLogarithmic", json_integer((int) keyLogarithmic));
		json_object_set_new(rootJ, "tritaveMapping", json_integer((int) tritaveMapping));

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

		json_t *sumO = json_object_get(rootJ, "tritaveWrapAround");
		if (sumO) {
			tritaveWrapAround = json_integer_value(sumO);			
		}

		json_t *sumT = json_object_get(rootJ, "justIntonation");
		if (sumT) {
			justIntonation = json_integer_value(sumT);			
		}

		json_t *sumL = json_object_get(rootJ, "shiftLogarithmic");
		if (sumL) {
			shiftLogarithmic = json_integer_value(sumL);			
		}

		json_t *sumK = json_object_get(rootJ, "keyLogarithmic");
		if (sumK) {
			keyLogarithmic = json_integer_value(sumK);			
		}

		json_t *sumTt = json_object_get(rootJ, "tritaveMapping");
		if (sumTt) {
			tritaveMapping = json_integer_value(sumTt);			
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
	
        if (resetScaleTrigger.process(params[RESET_SCALE_PARAM].getValue())) {
			resetTriggered = true;
			lastWeightShift = 0;			
			for(int j=0;j<MAX_NOTES;j++) {
				scaleNoteWeighting[scale][j] = defaultScaleNoteWeighting[scale][j];
				currentScaleNoteWeighting[j] = defaultScaleNoteWeighting[scale][j];;
			}					
		}		

		if (tempermentTrigger.process(params[TEMPERMENT_PARAM].getValue() + inputs[TEMPERMENT_INPUT].getVoltage())) {
			justIntonation = !justIntonation;
		}		
		lights[JUST_INTONATION_LIGHT].value = justIntonation;
		
        if (tritaveWrapAroundTrigger.process(params[TRITAVE_WRAPAROUND_PARAM].getValue() + inputs[TRITAVE_WRAP_INPUT].getVoltage())) {
			tritaveWrapAround = !tritaveWrapAround;
		}		
		lights[TRITAVE_WRAPAROUND_LIGHT].value = tritaveWrapAround;

        if (shiftScalingTrigger.process(params[SHIFT_SCALING_PARAM].getValue())) {
			shiftLogarithmic = !shiftLogarithmic;
		}		
		lights[SHIFT_LOGARITHMIC_SCALE_LIGHT].value = shiftLogarithmic;

		if (keyScalingTrigger.process(params[KEY_SCALING_PARAM].getValue())) {
			keyLogarithmic = !keyLogarithmic;
		}		
		lights[KEY_LOGARITHMIC_SCALE_LIGHT].value = keyLogarithmic;

		if (tritaveMappingTrigger.process(params[OCTAVE_TO_TRITAVE_PARAM].getValue())) {
			tritaveMapping = !tritaveMapping;
		}		
		lights[TRITAVE_MAPPING_LIGHT].value = tritaveMapping;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,7.0f);

		slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

        
        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,9.0f);



		key = params[KEY_PARAM].getValue();
		if(keyLogarithmic) {
			double inputKey = inputs[KEY_INPUT].getVoltage() * params[KEY_CV_ATTENUVERTER_PARAM].getValue();
			if(!tritaveMapping) {
				inputKey = inputKey / tritaveFrequency;
			}
			double unusedIntPart;
			inputKey = std::modf(inputKey, &unusedIntPart);
			inputKey = std::round(inputKey * 13);
			key += (int)inputKey;
		} else {
			key += inputs[KEY_INPUT].getVoltage() * MAX_NOTES / 10.0 * params[KEY_CV_ATTENUVERTER_PARAM].getValue();
		}
        key = clamp(key,0,12);
       
        tritave = clamp(params[TRITAVE_PARAM].getValue() + (inputs[TRITAVE_INPUT].getVoltage() * 0.4 * params[TRITAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);

        double noteIn = inputs[NOTE_INPUT].getVoltage();
		double tritaveIn;
		if(!tritaveMapping) {
			noteIn = noteIn / tritaveFrequency;
		}
		tritaveIn= std::floor(noteIn);
        double fractionalValue = noteIn - tritaveIn;
        double lastDif = 1.0f;    
        for(int i = 0;i<MAX_NOTES;i++) {            
			double currentDif = std::abs((i / 13.0) - fractionalValue);
			if(currentDif < lastDif) {
				lastDif = currentDif;
				currentNote = i;
			}            
        }

		if(lastNote != currentNote || lastSpread != spread || lastSlant != slant || lastFocus != focus) {
			for(int i = 0; i<MAX_NOTES;i++) {
				noteInitialProbability[i] = 0.0;
			}
			noteInitialProbability[currentNote] = 1.0;
			upperSpread = std::ceil((float)spread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil((float)spread * std::min(1.0-slant,1.0));

			

			for(int i=1;i<=spread;i++) {
				int noteAbove = (currentNote + i) % MAX_NOTES;
				int noteBelow = (currentNote - i);
				if(noteBelow < 0)
                    noteBelow +=MAX_NOTES;

				float upperInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
				float lowerInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 

				noteInitialProbability[noteAbove] = i <= upperSpread ? upperInitialProbability : 0.0f;				
				noteInitialProbability[noteBelow] = i <= lowerSpread ? lowerInitialProbability : 0.0f;				
			}
			lastNote = currentNote;
			lastSpread = spread;
			lastSlant = slant;
			lastFocus = focus;
		}

		weightShift = params[SHIFT_PARAM].getValue();
		if(shiftLogarithmic && inputs[SHIFT_INPUT].isConnected()) {
			double inputShift = inputs[SHIFT_INPUT].getVoltage() * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			if(!tritaveMapping) {
				inputShift = inputShift / tritaveFrequency;
			}
			inputShift = std::modf(inputShift, &unusedIntPart);
			inputShift = std::round(inputShift * 13);

			int desiredKey = inputShift;
			
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
		weightShift = clamp(weightShift,-12,12);

		//Process scales, keys and weights
		if(scale != lastScale || key != lastKey || weightShift != lastWeightShift || resetTriggered) {

			for(int i=0;i<MAX_NOTES;i++) {
				currentScaleNoteWeighting[i] = scaleNoteWeighting[scale][i];
				int noteValue = (i + key) % MAX_NOTES;
				if(noteValue < 0)
					noteValue +=MAX_NOTES;
				noteActive[noteValue] = currentScaleNoteWeighting[i] > 0.0;
				noteScaleProbability[noteValue] = currentScaleNoteWeighting[i];
			}	

			float shiftWeights[MAX_NOTES];
	        for(int i=0;i<MAX_NOTES;i++) {
				int newIndex = i;
				if(noteActive[i]) {
					int noteCount = 0;
					int offset = 0;
					if(weightShift > 0)
						offset = 1;
					else if(weightShift < 0)
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
					shiftWeights[newIndex] = noteScaleProbability[i];
					controlIndex[i] = newIndex;
				} else {
					shiftWeights[i] = noteScaleProbability[i];
					controlIndex[i] = i;					
				}
				if(i == key) {
					transposedKey = newIndex;
				}
			}
			for(int i=0;i<MAX_NOTES;i++) {
				int noteValue = (i + key) % MAX_NOTES;
				params[NOTE_WEIGHT_PARAM + i].setValue(shiftWeights[noteValue]);
			}	

			lastKey = key;
			lastScale = scale;
			lastWeightShift = weightShift;
			resetTriggered = false;
		}

		for(int i=0;i<MAX_NOTES;i++) {
			int controlOffset = (i + key) % MAX_NOTES;
			int actualTarget = controlOffset;
            if (noteActiveTrigger[i].process( params[NOTE_ACTIVE_PARAM+i].getValue())) {
                noteActive[actualTarget] = !noteActive[actualTarget];             }	

			float userProbability;
			if(noteActive[actualTarget]) {
	            userProbability = clamp(params[NOTE_WEIGHT_PARAM+i].getValue() + (inputs[NOTE_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);    
				lights[NOTE_ACTIVE_LIGHT+i*2].value = userProbability;    
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 0;    
			}
			else { 
				userProbability = 0.0;
				lights[NOTE_ACTIVE_LIGHT+i*2].value = 0;    
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 1;    	
			}

			actualProbability[actualTarget] = noteInitialProbability[actualTarget] * userProbability; 

			int scalePosition = controlIndex[controlOffset] - key;
			if (scalePosition < 0)
				scalePosition += MAX_NOTES;
			scaleNoteWeighting[scale][i] = noteActive[controlOffset] ? params[NOTE_WEIGHT_PARAM+scalePosition].getValue() : 0.0; 
        }
        

		if( inputs[TRIGGER_INPUT].active ) {
			if (clockTrigger.process(inputs[TRIGGER_INPUT].value) ) {		
				float rnd = ((float) rand()/RAND_MAX);
				if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
					rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f;
				}	
			
				int randomNote = weightedProbability(actualProbability,params[WEIGHT_SCALING_PARAM].getValue(), rnd);
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
				float tritaveAdjust = 0.0;
				if(!tritaveWrapAround) {
					if(randomNote > currentNote && randomNote - currentNote > upperSpread)
						tritaveAdjust = -1.0;
					if(randomNote < currentNote && currentNote - randomNote > lowerSpread)
						tritaveAdjust = 1.0;
				}

				
				int tempermentIndex = justIntonation ? 1 : 0;
				int notePosition = randomNote - key;
				if(notePosition < 0) {
					notePosition += MAX_NOTES;
					tritaveAdjust -=1;
				}
				double quantitizedNoteCV = (noteTemperment[tempermentIndex][notePosition] / 1200.0) + (key * 146.308 / 1200.0); 

				quantitizedNoteCV += (tritaveIn + tritave + tritaveAdjust) * tritaveFrequency; 
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

void ProbablyNoteBP::onReset() {
	clockTrigger.reset();
	for(int i = 0;i<MAX_SCALES;i++) {
		for(int j=0;j<MAX_NOTES;j++) {
			scaleNoteWeighting[i][j] = defaultScaleNoteWeighting[i][j];
		}
	}
}




struct ProbablyNoteBPDisplay : TransparentWidget {
	ProbablyNoteBP *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteBPDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawKey(const DrawArgs &args, Vec pos, int key, int transposedKey) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		if(key < 0)
			return;

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

	void drawTritave(const DrawArgs &args, Vec pos, int tritave) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", tritave);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	

	void drawNoteRange(const DrawArgs &args, float *noteInitialProbability, int key) 
	{		
		float notePosition[MAX_NOTES][3] = {
			{103,163,NVG_ALIGN_LEFT},
			{138,180,NVG_ALIGN_LEFT},
			{166,208,NVG_ALIGN_CENTER},
			{174,242,NVG_ALIGN_CENTER},
			{167,277,NVG_ALIGN_CENTER},
			{148,303,NVG_ALIGN_RIGHT},
			{116,320,NVG_ALIGN_RIGHT},
			{78,314,NVG_ALIGN_RIGHT},
			{50,292,NVG_ALIGN_RIGHT},
			{28,262,NVG_ALIGN_CENTER}, 
			{26,227,NVG_ALIGN_CENTER},
			{36,193,NVG_ALIGN_LEFT},
			{68,172,NVG_ALIGN_LEFT}
		};

		// Draw indicator
		for(int i = 0; i<MAX_NOTES;i++) {
			const float rotate90 = (M_PI) / 2.0;

			int actualTarget = (i + key) % MAX_NOTES;

			float opacity = noteInitialProbability[actualTarget] * 255;
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			if(actualTarget == module->probabilityNote) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			}

			double startDegree = (M_PI * 2.0 * ((double)i - 0.5) / MAX_NOTES) - rotate90;
			double endDegree = (M_PI * 2.0 * ((double)i + 0.5) / MAX_NOTES) - rotate90;
			

			nvgBeginPath(args.vg);
			nvgArc(args.vg,100,240,85.0,startDegree,endDegree,NVG_CW);
			double x= cos(endDegree) * 65.0 + 100.0;
			double y= sin(endDegree) * 65.0 + 240;
			nvgLineTo(args.vg,x,y);
			nvgArc(args.vg,100,240,65.0,endDegree,startDegree,NVG_CCW);
			nvgClosePath(args.vg);		
			nvgFill(args.vg);

			nvgFontSize(args.vg, 8);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));

			char text[128];
			snprintf(text, sizeof(text), "%s", module->noteNames[actualTarget]);
			x= notePosition[i][0];
			y= notePosition[i][1];
			int align = (int)notePosition[i][2];

			nvgTextAlign(args.vg,align);
			nvgText(args.vg, x, y, text, NULL);

		}

	

	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawScale(args, Vec(4,83), module->scale, module->weightShift != 0);
		drawKey(args, Vec(72,84), module->lastKey, module->transposedKey);
		//drawTritave(args, Vec(66, 280), module->tritave);
		drawNoteRange(args, module->noteInitialProbability, module->key);
	}
};

struct ProbablyNoteBPWidget : ModuleWidget {
	ProbablyNoteBPWidget(ProbablyNoteBP *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteBP.svg")));


		{
			ProbablyNoteBPDisplay *display = new ProbablyNoteBPDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(8, 345), module, ProbablyNoteBP::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(43, 345), module, ProbablyNoteBP::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(80, 345), module, ProbablyNoteBP::EXTERNAL_RANDOM_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(23,25), module, ProbablyNoteBP::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(49,51), module, ProbablyNoteBP::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(51, 29), module, ProbablyNoteBP::SPREAD_INPUT));

        addParam(createParam<RoundSmallFWKnob>(Vec(98,25), module, ProbablyNoteBP::SLANT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(124,51), module, ProbablyNoteBP::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(126, 29), module, ProbablyNoteBP::SLANT_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(168, 25), module, ProbablyNoteBP::DISTRIBUTION_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(194,51), module, ProbablyNoteBP::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(196, 29), module, ProbablyNoteBP::DISTRIBUTION_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNoteBP::SCALE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNoteBP::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNoteBP::SCALE_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(68,86), module, ProbablyNoteBP::KEY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,112), module, ProbablyNoteBP::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 90), module, ProbablyNoteBP::KEY_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(128,85), module, ProbablyNoteBP::SHIFT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,112), module, ProbablyNoteBP::SHIFT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 90), module, ProbablyNoteBP::SHIFT_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(188,86), module, ProbablyNoteBP::TRITAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,112), module, ProbablyNoteBP::TRITAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 90), module, ProbablyNoteBP::TRITAVE_INPUT));

		addParam(createParam<TL1105>(Vec(15, 113), module, ProbablyNoteBP::RESET_SCALE_PARAM));


		addParam(createParam<LEDButton>(Vec(130, 113), module, ProbablyNoteBP::SHIFT_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(131.5, 114.5), module, ProbablyNoteBP::SHIFT_LOGARITHMIC_SCALE_LIGHT));

		addParam(createParam<LEDButton>(Vec(70, 113), module, ProbablyNoteBP::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(71.5, 114.5), module, ProbablyNoteBP::KEY_LOGARITHMIC_SCALE_LIGHT));


		addParam(createParam<LEDButton>(Vec(214, 143), module, ProbablyNoteBP::TRITAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(215.5, 144.5), module, ProbablyNoteBP::TRITAVE_WRAPAROUND_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 144), module, ProbablyNoteBP::TRITAVE_WRAP_INPUT));

		addParam(createParam<LEDButton>(Vec(8, 145), module, ProbablyNoteBP::OCTAVE_TO_TRITAVE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(9.5, 146.5), module, ProbablyNoteBP::TRITAVE_MAPPING_LIGHT));

		addParam(createParam<LEDButton>(Vec(214, 180), module, ProbablyNoteBP::TEMPERMENT_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(215.5, 181.5), module, ProbablyNoteBP::JUST_INTONATION_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 181), module, ProbablyNoteBP::TEMPERMENT_INPUT)); 

		addParam(createParam<RoundReallySmallFWKnob>(Vec(202,217), module, ProbablyNoteBP::WEIGHT_SCALING_PARAM));	





		for(int i=0;i<MAX_NOTES;i++) {
			double position = 2.0 * M_PI / MAX_NOTES * i  - M_PI / 2.0; // Rotate 90 degrees

			double x= cos(position) * 54.0 + 90.0;
			double y= sin(position) * 54.0 + 230.5;

			//Rotate inputs 1 degrees
			addParam(createParam<RoundReallySmallFWKnob>(Vec(x,y), module, ProbablyNoteBP::NOTE_WEIGHT_PARAM+i));			
			x= cos(position + (M_PI / 180.0 * 1.0)) * 36.0 + 94.0;
			y= sin(position + (M_PI / 180.0 * 1.0)) * 36.0 + 235.0;
			addInput(createInput<FWPortInReallySmall>(Vec(x, y), module, ProbablyNoteBP::NOTE_WEIGHT_INPUT+i));

			//Rotate buttons 5 degrees
			x= cos(position - (M_PI / 180.0 * 5.0)) * 75.0 + 91.0;
			y= sin(position - (M_PI / 180.0 * 5.0)) * 75.0 + 231.0;
			addParam(createParam<LEDButton>(Vec(x, y), module, ProbablyNoteBP::NOTE_ACTIVE_PARAM+i));
			addChild(createLight<LargeLight<GreenRedLight>>(Vec(x+1.5, y+1.5), module, ProbablyNoteBP::NOTE_ACTIVE_LIGHT+i*2));

		}

      

		addOutput(createOutput<FWPortInSmall>(Vec(205, 345),  module, ProbablyNoteBP::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(172, 345),  module, ProbablyNoteBP::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(134, 345),  module, ProbablyNoteBP::NOTE_CHANGE_OUTPUT));

	}

	
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteBP = createModel<ProbablyNoteBP, ProbablyNoteBPWidget>("ProbablyNoteBP");
    