#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"
#include "dsp-noise/noise.hpp"
#include "osdialog.h"
#include <sstream>
#include <iomanip>
#include <time.h>

#include <iostream>
#include <fstream>
#include <string>

#define POLYPHONY 16
#define MAX_NOTES 13
#define MAX_SCALES 10
#define MAX_TEMPERMENTS 2
#define NUM_SHIFT_MODES 3
#define TRIGGER_DELAY_SAMPLES 5

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
		PITCH_RANDOMNESS_PARAM,
		PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM,
		PITCH_RANDOMNESS_GAUSSIAN_PARAM,
		TRIGGER_MODE_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NON_REPEATABILITY_PARAM = NOTE_WEIGHT_PARAM + MAX_NOTES,
		NON_REPEATABILITY_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS
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
		PITCH_RANDOMNESS_INPUT,
        NOTE_WEIGHT_INPUT,
		NON_REPEATABILITY_INPUT = NOTE_WEIGHT_INPUT + MAX_NOTES,
		NUM_INPUTS
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
		SHIFT_MODE_LIGHT,
        KEY_LOGARITHMIC_SCALE_LIGHT = SHIFT_MODE_LIGHT + 3,
		TRITAVE_MAPPING_LIGHT,
		PITCH_RANDOMNESS_GAUSSIAN_LIGHT,
		TRIGGER_POLYPHONIC_LIGHT,
        NOTE_ACTIVE_LIGHT,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_NOTES*2
	};
	enum QuantizeModes {
		QUANTIZE_CLOSEST,
		QUANTIZE_LOWER,
		QUANTIZE_UPPER,
	};
	enum ShiftModes {
		SHIFT_STEP_PER_VOLT,
		SHIFT_STEP_BY_V_OCTAVE_RELATIVE,
		SHIFT_STEP_BY_V_OCTAVE_ABSOLUTE,
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
	bool defaultScaleNoteStatus[MAX_SCALES][MAX_NOTES] = {
		{1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,1,1,1,0,1,1,0,1,1,0,1},
		{1,1,1,0,1,1,0,1,1,0,1,1,0},
		{1,1,0,1,1,0,1,1,0,1,1,0,1},
		{1,0,1,1,0,1,1,0,1,1,0,1,1},
		{1,1,0,1,1,0,1,1,0,1,1,1,0},
		{1,0,1,1,0,1,1,0,1,1,1,0,1},
		{1,1,0,1,1,0,1,1,1,0,1,1,0},
		{1,0,1,1,0,1,1,1,0,1,1,0,1},
		{1,1,0,1,1,1,0,1,1,0,1,1,0}
	}; 
		

    float scaleNoteWeighting[MAX_SCALES][MAX_NOTES]; 
	bool scaleNoteStatus[MAX_SCALES][MAX_NOTES];

	const char* tempermentNames[MAX_NOTES] = {"Equal","Just"};
    double noteTemperment[MAX_TEMPERMENTS][MAX_NOTES] = {
        {0,146,293,439,585,732,878,1024,1170,1317,1463,1609,1756},
        {0,133,301.85,435,583,737,884,1018,1165,1319,1467,1600,1769},
    };
    
	const double tritaveFrequency = 1.5849625;
	
	dsp::SchmittTrigger clockTrigger[POLYPHONY],clockModeTrigger,resetScaleTrigger,tritaveWrapAroundTrigger,
						tempermentTrigger,tritaveMappingTrigger,shiftScalingTrigger,keyScalingTrigger,
						pitchRandomnessGaussianTrigger,noteActiveTrigger[MAX_NOTES]; 

	dsp::PulseGenerator noteChangePulse[POLYPHONY];
    GaussianNoiseGenerator _gauss;
 
    bool tritaveWrapAround = false;
	bool triggerPolyphonic = false;
    bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteInitialProbability[POLYPHONY][MAX_NOTES] = {{0.0f}};
    float currentScaleNoteWeighting[MAX_NOTES] = {0.0f};
	bool currentScaleNoteStatus[MAX_NOTES] = {false};
	float actualProbability[POLYPHONY][MAX_NOTES] = {{0.0f}};
	int controlIndex[MAX_NOTES] = {0};

	bool triggerDelayEnabled = false;
	float triggerDelay[POLYPHONY][TRIGGER_DELAY_SAMPLES] = {{0}};
	int triggerDelayIndex[POLYPHONY] = {0};



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
	float nonRepeat = 0;
	int lastRandomNote[POLYPHONY] = {-1}; 
	int currentNote[POLYPHONY] = {0};
	int probabilityNote[POLYPHONY] = {0};
	double lastQuantizedCV[POLYPHONY] = {0.0};
	bool noteChange = false;
	bool resetTriggered = false;
	int lastNote[POLYPHONY] = {-1};
	int lastSpread = -1;
	float lastSlant = -1;
	float lastFocus = -1;
	bool justIntonation = false;
	int shiftMode = 0;
	bool keyLogarithmic = false;
	bool tritaveMapping = true;
	bool pitchRandomGaussian = false;

	int quantizeMode = QUANTIZE_CLOSEST;

	int currentPolyphony = 1;

	std::string lastPath;
    
	//percentages
	float spreadPercentage = 0;
	float slantPercentage = 0;
	float distributionPercentage = 0;
	float nonRepeatPercentage = 0;
	float scalePercentage = 0;
	float keyPercentage = 0;
	float shiftPercentage = 0;
	float tritavePercentage = 0;
	float pitchRandomnessPercentage = 0;
	float weightPercentage[MAX_NOTES] = {0};

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
		configParam(ProbablyNoteBP::TEMPERMENT_PARAM, 0.0, 1.0, 0.0,"Just Intonation");
		configParam(ProbablyNoteBP::WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Weight Scaling","%",0,100);
		configParam(ProbablyNoteBP::PITCH_RANDOMNESS_PARAM, 0.0, 10.0, 0.0,"Randomize Pitch Amount"," Cents");
        configParam(ProbablyNoteBP::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Randomize Pitch Amount CV Attenuation","%",0,100);
		configParam(ProbablyNoteBP::NON_REPEATABILITY_PARAM, 0.0, 1.0, 0.0,"Non Repeat Probability"," %",0,100);
        configParam(ProbablyNoteBP::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Non Repeat Probability CV Attenuation","%",0,100);


		configButton(OCTAVE_TO_TRITAVE_PARAM,"Map Octave to Tritave");
		configButton(TRITAVE_WRAPAROUND_PARAM,"Tritave Wraparound");
		configButton(TEMPERMENT_PARAM,"Just Intonation");
		configButton(RESET_SCALE_PARAM,"Reset Scale Weights");
		configButton(TRIGGER_MODE_PARAM,"Trigger Mode");
		configButton(KEY_SCALING_PARAM,"Key Scaling Mode");
		configButton(SHIFT_SCALING_PARAM,"Weight Scaling Mode");
		configButton(PITCH_RANDOMNESS_GAUSSIAN_PARAM,"Gaussian Randomness");



        srand(time(NULL));


        for(int i=0;i<MAX_NOTES;i++) {
            // configParam(NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
			std::string noteName( noteNames[i] );
            configParam(NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight: " + noteName);					
			configButton(NOTE_ACTIVE_PARAM + i,"Note Active: " + noteName);
			configInput(NOTE_WEIGHT_INPUT+i, "Note Weight: " + noteName);
        }

		configInput(NOTE_INPUT, "Unquantized CV");
		configInput(SPREAD_INPUT, "Spread");
		configInput(SLANT_INPUT, "Slant");
		configInput(DISTRIBUTION_INPUT, "Focus");
		configInput(SHIFT_INPUT, "Shift");
		configInput(SCALE_INPUT, "Scale");
		configInput(KEY_INPUT, "Key");
		configInput(TRITAVE_INPUT, "Tritave");
		configInput(TEMPERMENT_INPUT, "Tempermanet");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(EXTERNAL_RANDOM_INPUT, "External Random");
		configInput(TRITAVE_WRAP_INPUT, "Tritave Wrap");
		configInput(PITCH_RANDOMNESS_INPUT, "Pitch Randomness");
		configInput(NOTE_WEIGHT_INPUT, "Left");
		configInput(NON_REPEATABILITY_INPUT, "Note Non-Repeat Probability");

		configOutput(QUANT_OUTPUT, "Quantized CV");
		configOutput(WEIGHT_OUTPUT, "Note Weight");
		configOutput(NOTE_CHANGE_OUTPUT, "Note Changed");
		
		onReset();
	}

    double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,float scaling,float randomIn) {
        double weightTotal = 0.0f;
		double linearWeight, logWeight, weight;
            
        for(int i = 0;i <MAX_NOTES; i++) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

        int chosenWeight = -1;        
        double rnd = randomIn * weightTotal;
        for(int i = 0;i <MAX_NOTES;i++ ) {
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

		json_object_set_new(rootJ, "triggerDelayEnabled", json_integer((bool) triggerDelayEnabled));
		json_object_set_new(rootJ, "tritaveWrapAround", json_integer((int) tritaveWrapAround));
		json_object_set_new(rootJ, "justIntonation", json_integer((int) justIntonation));
		json_object_set_new(rootJ, "shiftMode", json_integer((int) shiftMode));
		json_object_set_new(rootJ, "keyLogarithmic", json_integer((int) keyLogarithmic));
		json_object_set_new(rootJ, "tritaveMapping", json_integer((int) tritaveMapping));
		json_object_set_new(rootJ, "pitchRandomGaussian", json_integer((int) pitchRandomGaussian));
		json_object_set_new(rootJ, "triggerPolyphonic", json_integer((int) triggerPolyphonic)); 
		json_object_set_new(rootJ, "quantizeMode", json_integer((int) quantizeMode)); 

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

				char buf2[100];
				char notebuf2[100];
				strcpy(buf2, "scaleStatus-");
				strcat(buf2, scaleNames[i]);
				strcat(buf2, ".");
				sprintf(notebuf2, "%i", j);
				strcat(buf2, notebuf2);
				json_object_set_new(rootJ, buf2, json_integer((int) scaleNoteStatus[i][j]));
			}
		}
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *ctTd = json_object_get(rootJ, "triggerDelayEnabled");
		if (ctTd)
			triggerDelayEnabled = json_integer_value(ctTd);


		json_t *sumO = json_object_get(rootJ, "tritaveWrapAround");
		if (sumO) {
			tritaveWrapAround = json_integer_value(sumO);			
		}

		json_t *sumT = json_object_get(rootJ, "justIntonation");
		if (sumT) {
			justIntonation = json_integer_value(sumT);			
		}

		json_t *sumL = json_object_get(rootJ, "shiftMode");
		if (sumL) {
			shiftMode = json_integer_value(sumL);			
		}

		json_t *sumK = json_object_get(rootJ, "keyLogarithmic");
		if (sumK) {
			keyLogarithmic = json_integer_value(sumK);			
		}

		json_t *sumTt = json_object_get(rootJ, "tritaveMapping");
		if (sumTt) {
			tritaveMapping = json_integer_value(sumTt);			
		}

		json_t *sumPg = json_object_get(rootJ, "pitchRandomGaussian");
		if (sumPg) {
			pitchRandomGaussian = json_integer_value(sumPg);			
		}

		json_t *sumTp = json_object_get(rootJ, "triggerPolyphonic");
		if (sumTp) {
			triggerPolyphonic = json_integer_value(sumTp);			
		}

		json_t *sumQm = json_object_get(rootJ, "quantizeMode");
		if (sumQm) {
			quantizeMode = json_integer_value(sumQm);			
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

				char buf2[100];
				char notebuf2[100];
				strcpy(buf2, "scaleStatus-");
				strcat(buf2, scaleNames[i]);
				strcat(buf2, ".");
				sprintf(notebuf2, "%i", j);
				strcat(buf2, notebuf2);
				json_t *sumJ2 = json_object_get(rootJ, buf2);
				if (sumJ2) {
					scaleNoteStatus[i][j] = json_integer_value(sumJ2);
				}
			}
		}		

		resetTriggered = true;
	}
	

	void process(const ProcessArgs &args) override {

		if (clockModeTrigger.process(params[TRIGGER_MODE_PARAM].getValue())) {
			triggerPolyphonic = !triggerPolyphonic;
		}		
		lights[TRIGGER_POLYPHONIC_LIGHT].value = triggerPolyphonic;

	
        if (resetScaleTrigger.process(params[RESET_SCALE_PARAM].getValue())) {
			resetTriggered = true;
			lastWeightShift = 0;			
			for(int j=0;j<MAX_NOTES;j++) {
				scaleNoteWeighting[scale][j] = defaultScaleNoteWeighting[scale][j];
				scaleNoteStatus[scale][j] = defaultScaleNoteStatus[scale][j];
				currentScaleNoteWeighting[j] = defaultScaleNoteWeighting[scale][j];
				currentScaleNoteStatus[j] =	defaultScaleNoteStatus[scale][j];		
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
			shiftMode = (shiftMode + 1) % NUM_SHIFT_MODES;
		}		
		lights[SHIFT_MODE_LIGHT].value = 0;
		lights[SHIFT_MODE_LIGHT+1].value = (shiftMode == SHIFT_STEP_BY_V_OCTAVE_ABSOLUTE ? 1 : 0);
		lights[SHIFT_MODE_LIGHT+2].value = (shiftMode == SHIFT_STEP_BY_V_OCTAVE_RELATIVE ? 1 : 0);


		if (keyScalingTrigger.process(params[KEY_SCALING_PARAM].getValue())) {
			keyLogarithmic = !keyLogarithmic;
		}		
		lights[KEY_LOGARITHMIC_SCALE_LIGHT].value = keyLogarithmic;

		if (tritaveMappingTrigger.process(params[OCTAVE_TO_TRITAVE_PARAM].getValue())) {
			tritaveMapping = !tritaveMapping;
		}		
		lights[TRITAVE_MAPPING_LIGHT].value = tritaveMapping;

		if (pitchRandomnessGaussianTrigger.process(params[PITCH_RANDOMNESS_GAUSSIAN_PARAM].getValue())) {
			pitchRandomGaussian = !pitchRandomGaussian;
		}		
		lights[PITCH_RANDOMNESS_GAUSSIAN_LIGHT].value = pitchRandomGaussian;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,7.0f);
		spreadPercentage = spread / 7.0f;

		slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
		slantPercentage = slant;

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		distributionPercentage = focus;

        nonRepeat = clamp(params[NON_REPEATABILITY_PARAM].getValue() + (inputs[NON_REPEATABILITY_INPUT].getVoltage() / 10.0f * params[NON_REPEATABILITY_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		nonRepeatPercentage = nonRepeat;

        
        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,9.0f);
		scalePercentage = scale / 9.0f;



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
		keyPercentage = key / 12.0;
       
        tritave = clamp(params[TRITAVE_PARAM].getValue() + (inputs[TRITAVE_INPUT].getVoltage() * 0.4 * params[TRITAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);
		tritavePercentage = (tritave + 4.0f) / 8.0f;

		currentPolyphony = inputs[NOTE_INPUT].getChannels();

		outputs[NOTE_CHANGE_OUTPUT].setChannels(currentPolyphony);
		outputs[WEIGHT_OUTPUT].setChannels(currentPolyphony);
		outputs[QUANT_OUTPUT].setChannels(currentPolyphony);

		noteChange = false;
		double tritaveIn[POLYPHONY];
		for(int channel = 0;channel<currentPolyphony;channel++) {
			double noteIn = inputs[NOTE_INPUT].getVoltage(channel);
			if(!tritaveMapping) {
				noteIn = noteIn / tritaveFrequency;
			}
			tritaveIn[channel] = std::floor(noteIn);
			double fractionalValue = std::abs(noteIn - tritaveIn[channel]);
			double lastDif = 99.0f;    
			for(int i = 0;i<MAX_NOTES;i++) {
				double lowNote = (i / 13.0);
				double highNote = ((i+1) / 13.0);
				double median = (lowNote + highNote) / 2.0;
				            
				double lowNoteDif = std::abs(lowNote - fractionalValue);
				double highNoteDif = std::abs(highNote - fractionalValue);
				double medianDif = std::abs(median - fractionalValue);

				double currentDif;
				bool direction = lowNoteDif < highNoteDif;
				int note;
				switch(quantizeMode) {
					case QUANTIZE_CLOSEST :
					default:
						currentDif = medianDif;
						note = direction ? i : (i + 1) % MAX_NOTES;
						break;
					case QUANTIZE_LOWER :
						currentDif = lowNoteDif;
						note = i;
						break;
					case QUANTIZE_UPPER :
						currentDif = highNoteDif;
						note = (i + 1) % MAX_NOTES;
						break;
				}

				if(currentDif < lastDif) {
					lastDif = currentDif;
					currentNote[channel] = note;
				}            
			}
			if(currentNote[channel] != lastNote[channel]) {
				noteChange = true;
				lastNote[channel] = currentNote[channel];
			}
		}

		if(noteChange || lastSpread != spread || lastSlant != slant || lastFocus != focus) {
			upperSpread = std::ceil((float)spread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil((float)spread * std::min(1.0-slant,1.0));

			for(int channel = 0;channel<currentPolyphony;channel++) {
				for(int i = 0; i<MAX_NOTES;i++) {
					noteInitialProbability[channel][i] = 0.0;
				}
				noteInitialProbability[channel][currentNote[channel]] = 1.0;
			
				for(int i=1;i<=spread;i++) {
					int noteAbove = (currentNote[channel] + i) % MAX_NOTES;
					int noteBelow = (currentNote[channel] - i);
					if(noteBelow < 0)
						noteBelow +=MAX_NOTES;

					float upperInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
					float lowerInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 

					noteInitialProbability[channel][noteAbove] = i <= upperSpread ? upperInitialProbability : 0.0f;				
					noteInitialProbability[channel][noteBelow] = i <= lowerSpread ? lowerInitialProbability : 0.0f;				
				}
			}
			lastSpread = spread;
			lastSlant = slant;
			lastFocus = focus;
		}

		weightShift = params[SHIFT_PARAM].getValue();
		if(shiftMode != SHIFT_STEP_PER_VOLT && inputs[SHIFT_INPUT].isConnected()) {
			double inputShift = inputs[SHIFT_INPUT].getVoltage() * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
			double unusedIntPart;
			if(!tritaveMapping) {
				inputShift = inputShift / tritaveFrequency;
			}
			inputShift = std::modf(inputShift, &unusedIntPart);
			inputShift = std::round(inputShift * 13);

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
			weightShift += inputs[SHIFT_INPUT].getVoltage() * 2.2 * params[SHIFT_CV_ATTENUVERTER_PARAM].getValue();
		}
		weightShift = clamp(weightShift,-12,12);
		shiftPercentage = weightShift / 12.0f;

		//Process scales, keys and weights
		if(scale != lastScale || key != lastKey || weightShift != lastWeightShift || resetTriggered) {

			for(int i=0;i<MAX_NOTES;i++) {
				currentScaleNoteWeighting[i] = scaleNoteWeighting[scale][i];
				currentScaleNoteStatus[i] = scaleNoteStatus[scale][i];
				int noteValue = (i + key) % MAX_NOTES;
				if(noteValue < 0)
					noteValue +=MAX_NOTES;
				noteActive[noteValue] = currentScaleNoteStatus[i];
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
				weightPercentage[i] = userProbability;
				lights[NOTE_ACTIVE_LIGHT+i*2].value = userProbability;
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 0;    
			}
			else { 
				userProbability = 0.0;
				weightPercentage[i] = 0;
				lights[NOTE_ACTIVE_LIGHT+i*2].value = 0;    
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 1;    	
			}

			for(int channel = 0; channel < currentPolyphony; channel++) {
				actualProbability[channel][actualTarget] = noteInitialProbability[channel][actualTarget] * userProbability; 
			}

			int scalePosition = controlIndex[controlOffset] - key;
			if (scalePosition < 0)
				scalePosition += MAX_NOTES;
			scaleNoteWeighting[scale][i] = params[NOTE_WEIGHT_PARAM+scalePosition].getValue(); 
			scaleNoteStatus[scale][i] = noteActive[controlOffset];
        }
        

		float randomRange = clamp(params[PITCH_RANDOMNESS_PARAM].getValue() + (inputs[PITCH_RANDOMNESS_INPUT].getVoltage() * params[PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM].getValue()),0.0,10.0);
		pitchRandomnessPercentage = randomRange / 10.0;

		if( inputs[TRIGGER_INPUT].active ) {
			bool triggerFired = false;

			for(int channel=0;channel<currentPolyphony;channel++) {
				float currentTriggerInput = inputs[TRIGGER_INPUT].getVoltage(channel);
				triggerDelay[channel][triggerDelayIndex[channel]] = currentTriggerInput;
				int delayedIndex = (triggerDelayIndex[channel] + 1) % TRIGGER_DELAY_SAMPLES;
				float triggerInputValue = triggerDelayEnabled ? triggerDelay[channel][delayedIndex] : currentTriggerInput;
				triggerDelayIndex[channel] = delayedIndex;

				triggerFired = triggerPolyphonic ? clockTrigger[channel].process(triggerInputValue) : 
									channel == 0 ? clockTrigger[0].process(triggerInputValue) : triggerFired;

				if (triggerFired) {
					float rnd = ((float) rand()/RAND_MAX);
					if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
						int randomPolyphony = inputs[EXTERNAL_RANDOM_INPUT].getChannels(); //Use as many random channels as possible
						int randomChannel = channel;
						if(randomPolyphony <= channel) {
							randomChannel = randomPolyphony - 1;
						}
						rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage(randomChannel) / 10.0f;
					}

					float repeatProbability = ((double) rand()/RAND_MAX);
					if (spread > 0 && nonRepeat > 0.0 && repeatProbability < nonRepeat && lastRandomNote[channel] >= 0) {
						actualProbability[channel][lastRandomNote[channel]] = 0; //Last note has no chance of repeating 						
					}
			
					int randomNote = weightedProbability(actualProbability[channel],params[WEIGHT_SCALING_PARAM].getValue(), rnd);
					if(randomNote == -1) { //Couldn't find a note, so find first active
						bool noteOk = false;
						int notesSearched = 0;
						randomNote = currentNote[channel]; 
						do {
							randomNote = (randomNote + 1) % MAX_NOTES;
							notesSearched +=1;
							noteOk = noteActive[randomNote] || notesSearched >= MAX_NOTES;
						} while(!noteOk);
					}					
					lastRandomNote[channel] = randomNote; // for repeatability

					probabilityNote[channel] = randomNote;
					float tritaveAdjust = 0.0;
					if(!tritaveWrapAround) {
						if(randomNote > currentNote[channel] && randomNote - currentNote[channel] > upperSpread)
							tritaveAdjust = -1.0;
						if(randomNote < currentNote[channel] && currentNote[channel] - randomNote > lowerSpread)
							tritaveAdjust = 1.0;
					}

					
					int tempermentIndex = justIntonation ? 1 : 0;
					int notePosition = randomNote - key;
					if(notePosition < 0) {
						notePosition += MAX_NOTES;
						tritaveAdjust -=1;
					}
					double quantitizedNoteCV = (noteTemperment[tempermentIndex][notePosition] / 1200.0) + (key * 146.308 / 1200.0); 

					float pitchRandomness = 0;
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

					quantitizedNoteCV += (tritaveIn[channel] + tritave + tritaveAdjust) * tritaveFrequency; 
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

void ProbablyNoteBP::onReset() {
	for(int i=0;i<POLYPHONY;i++) {
		clockTrigger[i].reset();
		for(int j=0;j<TRIGGER_DELAY_SAMPLES;j++) {
			triggerDelay[i][j] = 0.0f;
		}
	}
	triggerDelayEnabled = false;


	for(int i = 0;i<MAX_SCALES;i++) {
		for(int j=0;j<MAX_NOTES;j++) {
			scaleNoteWeighting[i][j] = defaultScaleNoteWeighting[i][j];
			scaleNoteStatus[i][j] = defaultScaleNoteStatus[i][j];
			if(i == scale) {
				currentScaleNoteWeighting[j] = defaultScaleNoteWeighting[i][j];
				currentScaleNoteStatus[j] = defaultScaleNoteStatus[i][j];
			}
		}
	}
}




struct ProbablyNoteBPDisplay : TransparentWidget {
	ProbablyNoteBP *module;
	int frame = 0;
	std::shared_ptr<Font> font;
	std::string fontPath;

	ProbablyNoteBPDisplay() {
		fontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
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
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
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
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->scaleNames[scale]);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawTritave(const DrawArgs &args, Vec pos, int tritave) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
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
			if(actualTarget == module->probabilityNote[0]) {
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
			//nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
			nvgFillColor(args.vg, nvgRGBA(0xdf, 0xdf, 0xdf, 0xff));

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
		font = APP->window->loadFont(fontPath);

		if (!module)
			return; 

		drawScale(args, Vec(10,82), module->scale, module->weightShift != 0);
		drawKey(args, Vec(74,82), module->lastKey, module->transposedKey);
		//drawTritave(args, Vec(66, 280), module->tritave);
		drawNoteRange(args, module->noteInitialProbability[0], module->key);
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
		addInput(createInput<FWPortInSmall>(Vec(90, 345), module, ProbablyNoteBP::EXTERNAL_RANDOM_INPUT));

		ParamWidget* spreadParam = createParam<RoundSmallFWSnapKnob>(Vec(10,25), module, ProbablyNoteBP::SPREAD_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(spreadParam)->percentage = &module->spreadPercentage;
		}
		addParam(spreadParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(36,51), module, ProbablyNoteBP::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(38, 29), module, ProbablyNoteBP::SPREAD_INPUT));

		ParamWidget* slantParam = createParam<RoundSmallFWKnob>(Vec(67,25), module, ProbablyNoteBP::SLANT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->percentage = &module->slantPercentage;
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->biDirectional = true;
		}
		addParam(slantParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(93,51), module, ProbablyNoteBP::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(95, 29), module, ProbablyNoteBP::SLANT_INPUT));

		ParamWidget* distributionParam = createParam<RoundSmallFWKnob>(Vec(124, 25), module, ProbablyNoteBP::DISTRIBUTION_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(distributionParam)->percentage = &module->distributionPercentage;
		}
		addParam(distributionParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(150,51), module, ProbablyNoteBP::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(152, 29), module, ProbablyNoteBP::DISTRIBUTION_INPUT));

		ParamWidget* nonRepeatParam = createParam<RoundSmallFWKnob>(Vec(181, 25), module, ProbablyNoteBP::NON_REPEATABILITY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(nonRepeatParam)->percentage = &module->nonRepeatPercentage;
		}
		addParam(nonRepeatParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(207,51), module, ProbablyNoteBP::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(209, 29), module, ProbablyNoteBP::NON_REPEATABILITY_INPUT));

		ParamWidget* scaleParam = createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNoteBP::SCALE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(scaleParam)->percentage = &module->scalePercentage;
		}
		addParam(scaleParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNoteBP::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNoteBP::SCALE_INPUT));

		addParam(createParam<TL1105>(Vec(15, 115), module, ProbablyNoteBP::RESET_SCALE_PARAM));

		ParamWidget* keyParam = createParam<RoundSmallFWSnapKnob>(Vec(68,86), module, ProbablyNoteBP::KEY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(keyParam)->percentage = &module->keyPercentage;
		}
		addParam(keyParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,112), module, ProbablyNoteBP::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 90), module, ProbablyNoteBP::KEY_INPUT));

		addParam(createParam<LEDButton>(Vec(70, 113), module, ProbablyNoteBP::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(71.5, 114.5), module, ProbablyNoteBP::KEY_LOGARITHMIC_SCALE_LIGHT));

        ParamWidget* shiftParam = createParam<RoundSmallFWSnapKnob>(Vec(133,85), module, ProbablyNoteBP::SHIFT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(shiftParam)->percentage = &module->shiftPercentage;
			dynamic_cast<RoundSmallFWSnapKnob*>(shiftParam)->biDirectional = true;
		}
		addParam(shiftParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(159,112), module, ProbablyNoteBP::SHIFT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(161, 90), module, ProbablyNoteBP::SHIFT_INPUT));

		addParam(createParam<LEDButton>(Vec(135, 113), module, ProbablyNoteBP::SHIFT_SCALING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(136.5, 114.5), module, ProbablyNoteBP::SHIFT_MODE_LIGHT));

		ParamWidget* tritaveParam = createParam<RoundSmallFWSnapKnob>(Vec(188,86), module, ProbablyNoteBP::TRITAVE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(tritaveParam)->percentage = &module->tritavePercentage;
		}
		addParam(tritaveParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,112), module, ProbablyNoteBP::TRITAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 90), module, ProbablyNoteBP::TRITAVE_INPUT));






		addParam(createParam<LEDButton>(Vec(214, 143), module, ProbablyNoteBP::TRITAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(215.5, 144.5), module, ProbablyNoteBP::TRITAVE_WRAPAROUND_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 144), module, ProbablyNoteBP::TRITAVE_WRAP_INPUT));

		addParam(createParam<LEDButton>(Vec(8, 145), module, ProbablyNoteBP::OCTAVE_TO_TRITAVE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(9.5, 146.5), module, ProbablyNoteBP::TRITAVE_MAPPING_LIGHT));

		addParam(createParam<LEDButton>(Vec(214, 180), module, ProbablyNoteBP::TEMPERMENT_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(215.5, 181.5), module, ProbablyNoteBP::JUST_INTONATION_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 181), module, ProbablyNoteBP::TEMPERMENT_INPUT)); 


		ParamWidget* pitchRandomnessParam = createParam<RoundSmallFWKnob>(Vec(188,216), module, ProbablyNoteBP::PITCH_RANDOMNESS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(pitchRandomnessParam)->percentage = &module->pitchRandomnessPercentage;
		}
		addParam(pitchRandomnessParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,242), module, ProbablyNoteBP::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 220), module, ProbablyNoteBP::PITCH_RANDOMNESS_INPUT));
		addParam(createParam<LEDButton>(Vec(191, 250), module, ProbablyNoteBP::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(192.5, 251.5), module, ProbablyNoteBP::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));


		addParam(createParam<RoundReallySmallFWKnob>(Vec(202,292), module, ProbablyNoteBP::WEIGHT_SCALING_PARAM));	

		addParam(createParam<LEDButton>(Vec(62, 345), module, ProbablyNoteBP::TRIGGER_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(63.5, 346.5), module, ProbablyNoteBP::TRIGGER_POLYPHONIC_LIGHT));




		for(int i=0;i<MAX_NOTES;i++) {
			double position = 2.0 * M_PI / MAX_NOTES * i  - M_PI / 2.0; // Rotate 90 degrees

			double x= cos(position) * 54.0 + 90.0;
			double y= sin(position) * 54.0 + 230.5;

			//Rotate inputs 1 degrees
			ParamWidget* weightParam = createParam<RoundReallySmallFWKnob>(Vec(x,y), module, ProbablyNoteBP::NOTE_WEIGHT_PARAM+i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[i];
			}
			addParam(weightParam);							

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

	struct TriggerDelayItem : MenuItem {
		ProbablyNoteBP *module;
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

		ProbablyNoteBP *module = dynamic_cast<ProbablyNoteBP*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());
		{
      		OptionsMenuItem* mi = new OptionsMenuItem("Quantize Mode");
			mi->addItem(OptionMenuItem("Closet", [module]() { return module->quantizeMode == 0; }, [module]() { module->quantizeMode = 0; }));
			mi->addItem(OptionMenuItem("Round Lower", [module]() { return module->quantizeMode == 1; }, [module]() { module->quantizeMode = 1; }));
			mi->addItem(OptionMenuItem("Round Upper", [module]() { return module->quantizeMode == 2; }, [module]() { module->quantizeMode = 2; }));
			menu->addChild(mi);
		}

		TriggerDelayItem *triggerDelayItem = new TriggerDelayItem();
		triggerDelayItem->module = module;
		menu->addChild(triggerDelayItem);
		

	}
	
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteBP = createModel<ProbablyNoteBP, ProbablyNoteBPWidget>("ProbablyNoteBP");
    