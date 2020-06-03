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
#include <vector>

#define POLYPHONY 16
#define MAX_FACTORS 5
#define MAX_PITCHES 128 //Not reall a hard limit but to prevent getting silly
#define MAX_NOTES 11
#define NUM_SHIFT_MODES 3
#define TRIGGER_DELAY_SAMPLES 5

using namespace frozenwasteland::dsp;

struct EFPitch {
  uint64_t numerator;
  uint64_t denominator;
  float pitch;
  float dissonance;
  bool operator<(const EFPitch& a) const {
    return pitch < a.pitch;
  }
};


struct ProbablyNoteEF : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		SLANT_PARAM,
		SLANT_CV_ATTENUVERTER_PARAM,
		DISTRIBUTION_PARAM,
		DISTRIBUTION_CV_ATTENUVERTER_PARAM,
        KEY_PARAM,
		KEY_CV_ATTENUVERTER_PARAM,
        DISSONANCE_PARAM,
		DISSONANCE_CV_ATTENUVERTER_PARAM,
        SHIFT_PARAM,
		SHIFT_CV_ATTENUVERTER_PARAM,
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,
        RESET_SCALE_PARAM,
        OCTAVE_WRAPAROUND_PARAM,
		SHIFT_SCALING_PARAM,
		KEY_SCALING_PARAM,
		FACTOR_3_STEP_PARAM,
		FACTOR_5_STEP_PARAM,
		FACTOR_7_STEP_PARAM,
		FACTOR_11_STEP_PARAM,
		FACTOR_13_STEP_PARAM,
		FACTOR_3_STEP_CV_ATTENUVERTER_PARAM,
		FACTOR_5_STEP_CV_ATTENUVERTER_PARAM,
		FACTOR_7_STEP_CV_ATTENUVERTER_PARAM,
		FACTOR_11_STEP_CV_ATTENUVERTER_PARAM,
		FACTOR_13_STEP_CV_ATTENUVERTER_PARAM,
		WEIGHT_SCALING_PARAM,	
		PITCH_RANDOMNESS_PARAM,
		PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM,
		PITCH_RANDOMNESS_GAUSSIAN_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NUM_PARAMS = NOTE_WEIGHT_PARAM + MAX_NOTES
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		SLANT_INPUT,
		DISTRIBUTION_INPUT,
        KEY_INPUT,
        DISSONANCE_INPUT,
		SHIFT_INPUT,
        OCTAVE_INPUT,
        FACTOR_3_INPUT,
        FACTOR_5_INPUT,
        FACTOR_7_INPUT,
        FACTOR_11_INPUT,
        FACTOR_13_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
		OCTAVE_WRAP_INPUT,
		PITCH_RANDOMNESS_INPUT,
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
		SHIFT_MODE_LIGHT,
        KEY_LOGARITHMIC_SCALE_LIGHT = SHIFT_MODE_LIGHT + 3,
		PITCH_RANDOMNESS_GAUSSIAN_LIGHT,
        NOTE_ACTIVE_LIGHT,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_NOTES*2
	};
	enum ShiftModes {
		SHIFT_STEP_PER_VOLT,
		SHIFT_STEP_BY_V_OCTAVE_RELATIVE,
		SHIFT_STEP_BY_V_OCTAVE_ABSOLUTE,
	};



	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
    const int factors[MAX_FACTORS] = {3,5,7,11,13};
		
	
	dsp::SchmittTrigger clockTrigger,resetScaleTrigger,octaveWrapAroundTrigger,shiftScalingTrigger,keyScalingTrigger,pitchRandomnessGaussianTrigger,noteActiveTrigger[MAX_NOTES]; 
	dsp::PulseGenerator noteChangePulse[POLYPHONY];
    GaussianNoiseGenerator _gauss;
 
    bool octaveWrapAround = false;
    bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteInitialProbability[POLYPHONY][MAX_PITCHES] = {{0.0f}};
    float currentScaleNoteWeighting[MAX_NOTES] = {0.0f};
	bool currentScaleNoteStatus[MAX_NOTES] = {false};
	float actualProbability[POLYPHONY][MAX_NOTES] = {{0.0f}};
	int controlIndex[MAX_NOTES] = {0};

    uint8_t steps[MAX_FACTORS] = {0};
    uint8_t lastSteps[MAX_FACTORS] = {0};
    uint8_t actualSteps[MAX_FACTORS] = {0};

	bool triggerDelayEnabled = false;
	float triggerDelay[TRIGGER_DELAY_SAMPLES] = {0};
	int triggerDelayIndex = 0;


    std::vector<EFPitch> efPitches;
	int scaleSize = 0;

    int scale = 0;
    int lastScale = -1;
    int key = 0;
    int lastKey = -1;
    int transposedKey = 0;
	int octave = 0;
	int weightShift = 0;
	int lastWeightShift = 0;
    float spread = 0;
	float upperSpread = 0.0;
	float lowerSpread = 0.0;
	float slant = 0;
	float focus = 0; 
	float dissonanceProbability = 0; 
	int currentNote[POLYPHONY] = {0};
	int probabilityNote[POLYPHONY] = {0};
	double lastQuantizedCV[POLYPHONY] = {0.0};
	bool noteChange = false;
	bool resetTriggered = false;
	int lastNote[POLYPHONY] = {-1};
	float lastSpread = -1;
	float lastSlant = -1;
	float lastFocus = -1;
	float lastDissonanceProbability = -1;
	int shiftMode = 0;
	bool keyLogarithmic = false;
	bool pitchRandomGaussian = false;

	int currentPolyphony = 1;

	
	ProbablyNoteEF() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteEF::SPREAD_PARAM, 0.0, 1.0, 0.0,"Spread");
        configParam(ProbablyNoteEF::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteEF::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant");
        configParam(ProbablyNoteEF::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteEF::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNoteEF::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNoteEF::SHIFT_PARAM, -12.0, 12.0, 0.0,"Weight Shift");
        configParam(ProbablyNoteEF::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteEF::KEY_PARAM, 0.0, 10.0, 0.0,"Key");
        configParam(ProbablyNoteEF::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNoteEF::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
		configParam(ProbablyNoteEF::DISSONANCE_PARAM, -1.0, 1.0, 0.0,"Consonance/Dissonance Balance","%",0,100);
        configParam(ProbablyNoteEF::DISSONANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Dissonance CV Attenuation","%",0,100); 
        configParam(ProbablyNoteEF::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNoteEF::OCTAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Octave Wraparound");
		configParam(ProbablyNoteEF::WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Weight Scaling","%",0,100);
		configParam(ProbablyNoteEF::PITCH_RANDOMNESS_PARAM, 0.0, 10.0, 0.0,"Randomize Pitch Amount"," Cents");
        configParam(ProbablyNoteEF::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Randomize Pitch Amount CV Attenuation","%",0,100);


        srand(time(NULL));

        for(int i=0;i<MAX_FACTORS;i++) {
            configParam(ProbablyNoteEF::FACTOR_3_STEP_PARAM + i, 0.0, 12.0, 0.0,"Step Count");		
            configParam(ProbablyNoteEF::FACTOR_3_STEP_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Step Count CV Attenuverter");		
        }

		onReset();
	}

    double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,float scaling,float randomIn) {
        double weightTotal = 0.0f;
		double linearWeight, logWeight, weight;
            
        for(int i = 0;i <scaleSize; i++) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

        int chosenWeight = -1;        
        double rnd = randomIn * weightTotal;
        for(int i = 0;i <scaleSize;i++ ) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);

            if(rnd < weight) {
                chosenWeight = i;
                break;
            }
            rnd -= weights[i];
        }
        return chosenWeight;
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



    void BuildDerivedScale()
    {
        efPitches.clear();
        efPitches.resize(0);

        //EFPitch efPitch = new EFPitch();
        EFPitch efPitch;
        efPitch.numerator = 1;
        efPitch.denominator = 1;
		efPitch.pitch = 0.0;
		efPitch.dissonance = 0.0;
        efPitches.push_back(efPitch);

        //Check that we aren't getting too crazy
        uint16_t nbrPitches = 1;
        for(uint8_t f = 0; f< MAX_FACTORS;f++) {
            if(nbrPitches >= MAX_PITCHES) {
                steps[f] = 0;
            }
            int proposedNbrPitches = nbrPitches * (steps[f]+1);
            actualSteps[f] = proposedNbrPitches >= MAX_PITCHES ? (MAX_PITCHES / nbrPitches)-1  : steps[f]; 
            nbrPitches = nbrPitches * (actualSteps[f]+1);;
        }
 
        //Calculate Numerators (basically all factors multiplied by each other)
        for (uint8_t f = 0; f < MAX_FACTORS; f++) {
            uint16_t n = efPitches.size();
            //fprintf(stderr, "f: %i n: %i  \n", f, n);
            for (int s = 1; s <= actualSteps[f]; s++) {
                for (int i = 0; i < n; i++) {
                    EFPitch efPitch;
                    efPitch.numerator = (uint64_t) (efPitches[i].numerator * std::pow(factors[f], s));                    
                    efPitches.push_back(efPitch);
                }
            }
        }


        nbrPitches = efPitches.size(); //Actual Number of pitches
        //Calculate denomintors, octave scale if necessary
        //Computer pitch and dissonance
        for (uint16_t index = 1; index < nbrPitches; index++)
        {
            uint64_t intervalValue = efPitches[index].numerator;
            uint64_t denominator = GetDenominator(intervalValue);

            float ratio = float(intervalValue) / denominator;
            //double adjustedNumerator = currentNumerator;
            uint64_t adjustedDenominator = denominator;
            while (ratio > 2) {
                adjustedDenominator = adjustedDenominator * 2;
                ratio = float(intervalValue) / adjustedDenominator;
            };
            efPitches[index].denominator = adjustedDenominator;
            float pitchInCents = 1200 * std::log2f(ratio);
            efPitches[index].pitch = pitchInCents;
            float dissonance = CalculateDissonance(intervalValue,adjustedDenominator);
            efPitches[index].dissonance = dissonance;        
        }

        std::sort(efPitches.begin(), efPitches.end());

        fprintf(stderr, "Number of Pitches: %i\n",nbrPitches);
		scaleSize = nbrPitches;
        // for (uint16_t index = 0; index < nbrPitches; index++)
        // {
        //     EFPitch currentPitch = efPitches[index];
        //     fprintf(stderr, "%i %i %i %f %f \n", index, currentPitch.numerator, currentPitch.denominator, currentPitch.pitch, currentPitch.dissonance);
        // }

    }


    

    uint64_t GetDenominator(uint64_t numerator)
    {
        uint64_t denominator = 2;
        uint64_t newDenominator;
        while ((newDenominator = denominator * 2) < numerator)
        {
            denominator = newDenominator;
        }

        return denominator;
    }

    float CalculateDissonance(uint64_t numerator, uint64_t denominator)
    {
        float lcm = LCM(numerator, denominator);
        return std::log2f(lcm);
    }

    //least common multiple
    uint64_t LCM(uint64_t a, uint64_t b)
    {
        return (a * b) / GCD(a, b);
    }

    //Greatest common divisor
    uint64_t GCD(uint64_t a, uint64_t b)
    {
        if (b == 0)
            return a;
        return GCD(b, a % b);
    }

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "triggerDelayEnabled", json_integer((bool) triggerDelayEnabled));
		json_object_set_new(rootJ, "octaveWrapAround", json_integer((int) octaveWrapAround));
		json_object_set_new(rootJ, "shiftMode", json_integer((int) shiftMode));
		json_object_set_new(rootJ, "keyLogarithmic", json_integer((int) keyLogarithmic));
		json_object_set_new(rootJ, "pitchRandomGaussian", json_integer((int) pitchRandomGaussian));

		// for(int i=0;i<MAX_SCALES;i++) {
		// 	for(int j=0;j<MAX_NOTES;j++) {
		// 		char buf[100];
		// 		char notebuf[100];
		// 		strcpy(buf, "scaleWeight-");
		// 		strcat(buf, scaleNames[i]);
		// 		strcat(buf, ".");
		// 		sprintf(notebuf, "%i", j);
		// 		strcat(buf, notebuf);
		// 		json_object_set_new(rootJ, buf, json_real((float) scaleNoteWeighting[i][j]));

		// 		char buf2[100];
		// 		char notebuf2[100];
		// 		strcpy(buf2, "scaleStatus-");
		// 		strcat(buf2, scaleNames[i]);
		// 		strcat(buf2, ".");
		// 		sprintf(notebuf2, "%i", j);
		// 		strcat(buf2, notebuf2);
		// 		json_object_set_new(rootJ, buf2, json_integer((int) scaleNoteStatus[i][j]));
		// 	}
		// }
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *ctTd = json_object_get(rootJ, "triggerDelayEnabled");
		if (ctTd)
			triggerDelayEnabled = json_integer_value(ctTd);


		json_t *sumO = json_object_get(rootJ, "octaveWrapAround");
		if (sumO) {
			octaveWrapAround = json_integer_value(sumO);			
		}

		json_t *sumL = json_object_get(rootJ, "shiftMode");
		if (sumL) {
			shiftMode = json_integer_value(sumL);			
		}

		json_t *sumK = json_object_get(rootJ, "keyLogarithmic");
		if (sumK) {
			keyLogarithmic = json_integer_value(sumK);			
		}

		json_t *sumPg = json_object_get(rootJ, "pitchRandomGaussian");
		if (sumPg) {
			pitchRandomGaussian = json_integer_value(sumPg);			
		}


		// for(int i=0;i<MAX_SCALES;i++) {
		// 	for(int j=0;j<MAX_NOTES;j++) {
		// 		char buf[100];
		// 		char notebuf[100];
		// 		strcpy(buf, "scaleWeight-");
		// 		strcat(buf, scaleNames[i]);
		// 		strcat(buf, ".");
		// 		sprintf(notebuf, "%i", j);
		// 		strcat(buf, notebuf);
		// 		json_t *sumJ = json_object_get(rootJ, buf);
		// 		if (sumJ) {
		// 			scaleNoteWeighting[i][j] = json_real_value(sumJ);
		// 		}

		// 		char buf2[100];
		// 		char notebuf2[100];
		// 		strcpy(buf2, "scaleStatus-");
		// 		strcat(buf2, scaleNames[i]);
		// 		strcat(buf2, ".");
		// 		sprintf(notebuf2, "%i", j);
		// 		strcat(buf2, notebuf2);
		// 		json_t *sumJ2 = json_object_get(rootJ, buf2);
		// 		if (sumJ2) {
		// 			scaleNoteStatus[i][j] = json_integer_value(sumJ2);
		// 		}
		// 	}
		// }		
	}
	

	void process(const ProcessArgs &args) override {
	
        if (resetScaleTrigger.process(params[RESET_SCALE_PARAM].getValue())) {
			resetTriggered = true;
			lastWeightShift = 0;			
			// for(int j=0;j<MAX_NOTES;j++) {
			// 	scaleNoteWeighting[scale][j] = defaultScaleNoteWeighting[scale][j];
			// 	scaleNoteStatus[scale][j] = defaultScaleNoteStatus[scale][j];
			// 	currentScaleNoteWeighting[j] = defaultScaleNoteWeighting[scale][j];
			// 	currentScaleNoteStatus[j] =	defaultScaleNoteStatus[scale][j];		
			// }					
		}		

		
        if (octaveWrapAroundTrigger.process(params[OCTAVE_WRAPAROUND_PARAM].getValue() + inputs[OCTAVE_WRAP_INPUT].getVoltage())) {
			octaveWrapAround = !octaveWrapAround;
		}		
		lights[OCTAVE_WRAPAROUND_LIGHT].value = octaveWrapAround;

        if (shiftScalingTrigger.process(params[SHIFT_SCALING_PARAM].getValue())) {
			shiftMode = (shiftMode + 1) % NUM_SHIFT_MODES;
		}		
		lights[SHIFT_MODE_LIGHT].value = 0;
		lights[SHIFT_MODE_LIGHT+1].value = (shiftMode == SHIFT_STEP_BY_V_OCTAVE_ABSOLUTE ? 1 : 0);
		lights[SHIFT_MODE_LIGHT+2].value = (shiftMode == SHIFT_STEP_BY_V_OCTAVE_RELATIVE ? 1 : 0);


		if (keyScalingTrigger.process(params[KEY_SCALING_PARAM].getValue())) {
			keyLogarithmic = !keyLogarithmic;
		}		
		lights[KEY_LOGARITHMIC_SCALE_LIGHT].value = keyLogarithmic;


		if (pitchRandomnessGaussianTrigger.process(params[PITCH_RANDOMNESS_GAUSSIAN_PARAM].getValue())) {
			pitchRandomGaussian = !pitchRandomGaussian;
		}		
		lights[PITCH_RANDOMNESS_GAUSSIAN_LIGHT].value = pitchRandomGaussian;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() / 10.0f * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

		slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

        dissonanceProbability = clamp(params[DISSONANCE_PARAM].getValue() + (inputs[DISSONANCE_INPUT].getVoltage() / 10.0f * params[DISSONANCE_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
        
        //scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,9.0f);

        bool scaleChange = false;
        for(int i=0;i<MAX_FACTORS;i++) {
            steps[i] = clamp(params[FACTOR_3_STEP_PARAM+ i].getValue() + (inputs[FACTOR_3_INPUT + i].getVoltage() * 1.2f * params[FACTOR_3_STEP_CV_ATTENUVERTER_PARAM+i].getValue()),0.0f,12.0f);
            scaleChange = scaleChange || (steps[i] != lastSteps[i]);
            lastSteps[i] = steps[i];
        }
        if(scaleChange) {
            //fprintf(stderr, "New Scale Needed\n");
            BuildDerivedScale();
        }

        


		key = params[KEY_PARAM].getValue();
		if(keyLogarithmic) {
			double inputKey = inputs[KEY_INPUT].getVoltage() * params[KEY_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			inputKey = std::modf(inputKey, &unusedIntPart);
			inputKey = std::round(inputKey * 12);
			key += (int)inputKey;
		} else {
			key += inputs[KEY_INPUT].getVoltage() * MAX_NOTES / 10.0 * params[KEY_CV_ATTENUVERTER_PARAM].getValue();
		}
        key = clamp(key,0,10);
       
        octave = clamp(params[OCTAVE_PARAM].getValue() + (inputs[OCTAVE_INPUT].getVoltage() * 0.4 * params[OCTAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);

		currentPolyphony = inputs[NOTE_INPUT].getChannels();

		outputs[NOTE_CHANGE_OUTPUT].setChannels(currentPolyphony);
		outputs[WEIGHT_OUTPUT].setChannels(currentPolyphony);
		outputs[QUANT_OUTPUT].setChannels(currentPolyphony);

		noteChange = false;
		double octaveIn[POLYPHONY];
		for(int channel = 0;channel<currentPolyphony;channel++) {
			double noteIn = inputs[NOTE_INPUT].getVoltage(channel);
			octaveIn[channel] = std::floor(noteIn);
			double fractionalValue = noteIn - octaveIn[channel] - (float(key)/12.0); //Might need to subtract instead
			if(fractionalValue < 0.0)
				fractionalValue += 1.0;

											              //fprintf(stderr, "%i %f \n", key, fractionalValue);

			double lastDif = 1.0f;    
			for(int i = 0;i<scaleSize;i++) {            
				double currentDif = std::abs((efPitches[i].pitch / 1200.0) - fractionalValue);
				if(currentDif < lastDif) {
					lastDif = currentDif;
					currentNote[channel] = i;
				}            
			}
			if(currentNote[channel] != lastNote[channel]) {
				noteChange = true;
				lastNote[channel] = currentNote[channel];
			}
		}

		if(noteChange || lastSpread != spread || lastSlant != slant || lastFocus != focus || dissonanceProbability != lastDissonanceProbability) {
			float actualSpread = spread * float(scaleSize) / 2.0;
			upperSpread = std::ceil(actualSpread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil(actualSpread * std::min(1.0-slant,1.0));

			float whatIsDissonant = 7.0; // lower than this is consonant
			for(int channel = 0;channel<currentPolyphony;channel++) {
				for(int i = 0; i<scaleSize;i++) {
					noteInitialProbability[channel][i] = 0.0;
				}
				noteInitialProbability[channel][currentNote[channel]] = 1.0;

			
				for(int i=1;i<=actualSpread;i++) {
					int noteAbove = (currentNote[channel] + i) % scaleSize;
					int noteBelow = (currentNote[channel] - i);
					if(noteBelow < 0)
						noteBelow +=scaleSize;

					EFPitch upperPitch = efPitches[noteAbove];
					EFPitch lowerPitch = efPitches[noteBelow];
					float upperDissonance = upperPitch.dissonance;
					float lowerDissonance = lowerPitch.dissonance;
					float upperNoteDissonanceProbabilityAdjustment = 1.0;
					float lowerNoteDissonanceProbabilityAdjustment = 1.0;
					if(dissonanceProbability < 0) {
						if(upperDissonance > whatIsDissonant) {
							upperNoteDissonanceProbabilityAdjustment = (1.0f-std::min(upperDissonance/100.0,1.0)) * (1.0 + dissonanceProbability);
						}
						if(lowerDissonance > whatIsDissonant) {
							lowerNoteDissonanceProbabilityAdjustment = (1.0f-std::min(lowerDissonance/100.0,1.0)) * (1.0 + dissonanceProbability);
						}
					}					
					if(dissonanceProbability > 0) {
						if(upperDissonance < whatIsDissonant) {
							upperNoteDissonanceProbabilityAdjustment = std::min((upperDissonance / whatIsDissonant) + 0.5f,1.0f)  * (1.0 - dissonanceProbability);
							              //fprintf(stderr, "%f %f \n", upperDissonance, upperNoteDissonanceProbabilityAdjustment);
						}
						if(lowerDissonance < whatIsDissonant) {
							lowerNoteDissonanceProbabilityAdjustment = std::min((lowerDissonance / whatIsDissonant) + 0.5f,1.0f) * (1.0 - dissonanceProbability);
						}
					}					
					

					float upperInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/actualSpread) * upperNoteDissonanceProbabilityAdjustment; 
					float lowerInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/actualSpread) * lowerNoteDissonanceProbabilityAdjustment; 

					noteInitialProbability[channel][noteAbove] = i <= upperSpread ? upperInitialProbability : 0.0f;				
					noteInitialProbability[channel][noteBelow] = i <= lowerSpread ? lowerInitialProbability : 0.0f;				
				}
			}
			lastSpread = spread;
			lastSlant = slant;
			lastFocus = focus;
			lastDissonanceProbability = dissonanceProbability; 
		}

		weightShift = params[SHIFT_PARAM].getValue();
		if(shiftMode != SHIFT_STEP_PER_VOLT && inputs[SHIFT_INPUT].isConnected()) {
			double inputShift = inputs[SHIFT_INPUT].getVoltage() * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			inputShift = std::modf(inputShift, &unusedIntPart);
			inputShift = std::round(inputShift * 12);

			int desiredKey;
			if(shiftMode == SHIFT_STEP_BY_V_OCTAVE_RELATIVE) {
				desiredKey = (key + (int)inputShift) % MAX_NOTES;	
				if(desiredKey < 0)
					desiredKey += MAX_NOTES;
			} else {
				 desiredKey = (int)inputShift;
			}
			
			//Find nearest active note to shift amount
			int noteCount = 0;
			while (!noteActive[desiredKey] && noteCount < MAX_NOTES) {
				desiredKey = (desiredKey + 1) % MAX_NOTES;	
				noteCount+=1;			
			}

			//Count how many active notes it takes to get there
			noteCount = 0;
			while (desiredKey != key && noteCount < MAX_NOTES) {
				desiredKey -= 1;
				if(desiredKey < 0)
					desiredKey += MAX_NOTES;
				if(noteActive[desiredKey]) 
					noteCount +=1;
			}

			weightShift += noteCount;
		} else {
			weightShift += inputs[SHIFT_INPUT].getVoltage() * 2.0 * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
		}
		weightShift = clamp(weightShift,-10,10);

		//Process scales, keys and weights
		if(scale != lastScale || key != lastKey || weightShift != lastWeightShift || resetTriggered) {

		

			lastKey = key;
			lastWeightShift = weightShift;
		}
        

        
	

		if( inputs[TRIGGER_INPUT].active ) {
			float currentTriggerInput = inputs[TRIGGER_INPUT].getVoltage();
			triggerDelay[triggerDelayIndex] = currentTriggerInput;
			int delayedIndex = (triggerDelayIndex + 1) % TRIGGER_DELAY_SAMPLES;
			float triggerInputValue = triggerDelayEnabled ? triggerDelay[delayedIndex] : currentTriggerInput;
			triggerDelayIndex = delayedIndex;

			if (clockTrigger.process(triggerInputValue) ) {		
				for(int channel=0;channel<currentPolyphony;channel++) {
					float rnd = ((float) rand()/RAND_MAX);
					if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
						int randomPolyphony = inputs[EXTERNAL_RANDOM_INPUT].getChannels(); //Use as many random channels as possible
						int randomChannel = channel;
						if(randomPolyphony <= channel) {
							randomChannel = randomPolyphony - 1;
						}
						rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage(randomChannel) / 10.0f;
					}	
			
					int randomNote = weightedProbability(noteInitialProbability[channel],params[WEIGHT_SCALING_PARAM].getValue(), rnd);
					if(randomNote == -1) { //Couldn't find a note, so find first active
						bool noteOk = false;
						int notesSearched = 0;
						randomNote = currentNote[channel]; 
						do {
							randomNote = (randomNote + 1) % scaleSize;
							notesSearched +=1;
							noteOk = noteActive[randomNote] || notesSearched >= scaleSize;
						} while(!noteOk);
					}


					probabilityNote[channel] = randomNote;
					float octaveAdjust = 0.0;
					if(!octaveWrapAround) {
						if(randomNote > currentNote[channel] && randomNote - currentNote[channel] > upperSpread)
							octaveAdjust = -1.0;
						if(randomNote < currentNote[channel] && currentNote[channel] - randomNote > lowerSpread)
							octaveAdjust = 1.0;
					}

					int notePosition = randomNote;				
					// int notePosition = randomNote - key;
					// if(notePosition < 0) {
					// 	notePosition += MAX_NOTES;
					// 	octaveAdjust -=1;
					// }

					double quantitizedNoteCV = (efPitches[notePosition].pitch / 1200.0) + (key * 146.308 / 1200.0); 
              //fprintf(stderr, "%i \n", notePosition);

					float pitchRandomness = 0;
					float randomRange = clamp(params[PITCH_RANDOMNESS_PARAM].getValue() + (inputs[PITCH_RANDOMNESS_INPUT].getVoltage() * params[PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM].getValue()),0.0,10.0);
					if(pitchRandomGaussian) {
						bool gaussOk = false; // don't want values that are beyond our mean
						float gaussian;
						do {
							gaussian= _gauss.next();
							gaussOk = gaussian >= -1 && gaussian <= 1;
						} while (!gaussOk);
						pitchRandomness = (2.0 - gaussian) * randomRange / 1200.0;
					} else {
						pitchRandomness = (1.0 - ((double) rand()/RAND_MAX)) * randomRange / 600.0;
					}

					quantitizedNoteCV += (octaveIn[channel] + octave + octaveAdjust); 
					//quantitizedNoteCV += (octaveIn[channel] + octave + octaveAdjust) * octaveFrequency; 
					outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV + pitchRandomness, channel);
					outputs[WEIGHT_OUTPUT].setVoltage(clamp((params[NOTE_WEIGHT_PARAM+randomNote].getValue() + (inputs[NOTE_WEIGHT_INPUT+randomNote].getVoltage() / 10.0f) * 10.0f),0.0f,10.0f),channel);
					if(lastQuantizedCV[channel] != quantitizedNoteCV) {
						noteChangePulse[channel].trigger(1e-3);	
						lastQuantizedCV[channel] = quantitizedNoteCV;
					}     
				}   
			}
			for(int channel=0;channel<currentPolyphony;channel++) {
				outputs[NOTE_CHANGE_OUTPUT].setVoltage(noteChangePulse[channel].process(1.0 / args.sampleRate) ? 10.0 : 0, channel);
			}
		}

	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void ProbablyNoteEF::onReset() {
	clockTrigger.reset();

	for(int i=0;i<TRIGGER_DELAY_SAMPLES;i++) {
		triggerDelay[i] = 0.0f;
	}
	triggerDelayEnabled = false;
	
}




struct ProbablyNoteEFDisplay : TransparentWidget {
	ProbablyNoteEF *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteEFDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawKey(const DrawArgs &args, Vec pos, int key) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		if(key < 0)
			return;

		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "%s", module->noteNames[key]);
		              //fprintf(stderr, "%s\n", module->noteNames[key]);

		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawFactors(const DrawArgs &args, Vec pos, bool shifted) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
        nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
        

		if(shifted) 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xff));
		else
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
        for(int i=0;i<MAX_FACTORS;i++) {
            snprintf(text, sizeof(text), "%i", module->actualSteps[i]);
            nvgText(args.vg, pos.x + i*30, pos.y, text, NULL);
        }
	}

    void drawScale(const DrawArgs &args, Vec pos) {

        for(uint64_t i=0;i<module->efPitches.size();i++) {
            float pitch = module->efPitches[i].pitch;
            float dissonance = module->efPitches[i].dissonance;
			uint8_t opacity =  std::max(255.0f * module->noteInitialProbability[0][i] ,60.0f);
			if(i == module->probabilityNote[0]) 
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			else
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, opacity));
            nvgBeginPath(args.vg);
              //fprintf(stderr, "i:%llu p:%f d:%f\n", i, pitch,dissonance);
            nvgRect(args.vg,(pitch/8.0)+15,215.5,1,-74.0+std::min((dissonance*2.0),70.0));
            nvgFill(args.vg);
        }
	}


	

	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawFactors(args, Vec(42,240), module->weightShift != 0);
        drawScale(args,Vec(	35,156));
		drawKey(args, Vec(40,82), module->lastKey);
	}
};

struct ProbablyNoteEFWidget : ModuleWidget {
	ProbablyNoteEFWidget(ProbablyNoteEF *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteEF.svg")));


		{
			ProbablyNoteEFDisplay *display = new ProbablyNoteEFDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(8, 345), module, ProbablyNoteEF::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(43, 345), module, ProbablyNoteEF::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(80, 345), module, ProbablyNoteEF::EXTERNAL_RANDOM_INPUT));

        addParam(createParam<RoundSmallFWKnob>(Vec(23,25), module, ProbablyNoteEF::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(49,51), module, ProbablyNoteEF::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(51, 29), module, ProbablyNoteEF::SPREAD_INPUT));

        addParam(createParam<RoundSmallFWKnob>(Vec(98,25), module, ProbablyNoteEF::SLANT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(124,51), module, ProbablyNoteEF::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(126, 29), module, ProbablyNoteEF::SLANT_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(168, 25), module, ProbablyNoteEF::DISTRIBUTION_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(194,51), module, ProbablyNoteEF::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(196, 29), module, ProbablyNoteEF::DISTRIBUTION_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNoteEF::KEY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNoteEF::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNoteEF::KEY_INPUT));

		addParam(createParam<LEDButton>(Vec(15, 113), module, ProbablyNoteEF::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(16.5, 114.5), module, ProbablyNoteEF::KEY_LOGARITHMIC_SCALE_LIGHT));

        addParam(createParam<RoundSmallFWKnob>(Vec(68,86), module, ProbablyNoteEF::DISSONANCE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,112), module, ProbablyNoteEF::DISSONANCE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 90), module, ProbablyNoteEF::DISSONANCE_INPUT));


        addParam(createParam<RoundSmallFWSnapKnob>(Vec(133,85), module, ProbablyNoteEF::SHIFT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(159,112), module, ProbablyNoteEF::SHIFT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(161, 90), module, ProbablyNoteEF::SHIFT_INPUT));

		addParam(createParam<LEDButton>(Vec(135, 113), module, ProbablyNoteEF::SHIFT_SCALING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(136.5, 114.5), module, ProbablyNoteEF::SHIFT_MODE_LIGHT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(188,86), module, ProbablyNoteEF::OCTAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,112), module, ProbablyNoteEF::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 90), module, ProbablyNoteEF::OCTAVE_INPUT));






		addParam(createParam<LEDButton>(Vec(214, 143), module, ProbablyNoteEF::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(215.5, 144.5), module, ProbablyNoteEF::OCTAVE_WRAPAROUND_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 144), module, ProbablyNoteEF::OCTAVE_WRAP_INPUT));




		addParam(createParam<RoundSmallFWKnob>(Vec(188,216), module, ProbablyNoteEF::PITCH_RANDOMNESS_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,242), module, ProbablyNoteEF::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 220), module, ProbablyNoteEF::PITCH_RANDOMNESS_INPUT));
		addParam(createParam<LEDButton>(Vec(191, 250), module, ProbablyNoteEF::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(192.5, 251.5), module, ProbablyNoteEF::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));


		addParam(createParam<RoundReallySmallFWKnob>(Vec(202,292), module, ProbablyNoteEF::WEIGHT_SCALING_PARAM));	


        for(int i=0;i<MAX_FACTORS;i++) {
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(i*30+22,245), module, ProbablyNoteEF::FACTOR_3_STEP_PARAM+i));
            addParam(createParam<RoundReallySmallFWKnob>(Vec(i*30+22,300), module, ProbablyNoteEF::FACTOR_3_STEP_CV_ATTENUVERTER_PARAM+i));
    		addInput(createInput<FWPortInSmall>(Vec(i*30+24, 275), module, ProbablyNoteEF::FACTOR_3_INPUT+i));
        }   




		// for(int i=0;i<MAX_NOTES;i++) {
		// 	double position = 2.0 * M_PI / MAX_NOTES * i  - M_PI / 2.0; // Rotate 90 degrees

		// 	double x= cos(position) * 54.0 + 90.0;
		// 	double y= sin(position) * 54.0 + 230.5;

		// 	//Rotate inputs 1 degrees
		// 	addParam(createParam<RoundReallySmallFWKnob>(Vec(x,y), module, ProbablyNoteEF::NOTE_WEIGHT_PARAM+i));			
		// 	x= cos(position + (M_PI / 180.0 * 1.0)) * 36.0 + 94.0;
		// 	y= sin(position + (M_PI / 180.0 * 1.0)) * 36.0 + 235.0;
		// 	addInput(createInput<FWPortInReallySmall>(Vec(x, y), module, ProbablyNoteEF::NOTE_WEIGHT_INPUT+i));

		// 	//Rotate buttons 5 degrees
		// 	x= cos(position - (M_PI / 180.0 * 5.0)) * 75.0 + 91.0;
		// 	y= sin(position - (M_PI / 180.0 * 5.0)) * 75.0 + 231.0;
		// 	addParam(createParam<LEDButton>(Vec(x, y), module, ProbablyNoteEF::NOTE_ACTIVE_PARAM+i));
		// 	addChild(createLight<LargeLight<GreenRedLight>>(Vec(x+1.5, y+1.5), module, ProbablyNoteEF::NOTE_ACTIVE_LIGHT+i*2));

		// }

      

		addOutput(createOutput<FWPortInSmall>(Vec(205, 345),  module, ProbablyNoteEF::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(172, 345),  module, ProbablyNoteEF::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(134, 345),  module, ProbablyNoteEF::NOTE_CHANGE_OUTPUT));

	}

	struct TriggerDelayItem : MenuItem {
		ProbablyNoteEF *module;
		void onAction(const event::Action &e) override {
			module->triggerDelayEnabled = !module->triggerDelayEnabled;
		}
		void step() override {
			text = "Trigger Delay";
			rightText = (module->triggerDelayEnabled) ? "✔" : "";
		}
	};

	
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		ProbablyNoteEF *module = dynamic_cast<ProbablyNoteEF*>(this->module);
		assert(module);

		TriggerDelayItem *triggerDelayItem = new TriggerDelayItem();
		triggerDelayItem->module = module;
		menu->addChild(triggerDelayItem);

	}
	
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteEF = createModel<ProbablyNoteEF, ProbablyNoteEFWidget>("ProbablyNoteEF");
    