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
#define MAX_PRIME_NUMBERS 46 //Up to 200
#define MAX_FACTORS 10
#define MAX_PITCHES 128 //Not really a hard limit but to prevent getting silly
#define MAX_SCALES 42
#define MAX_NOTES 12
#define NUM_SHIFT_MODES 3
#define TRIGGER_DELAY_SAMPLES 5

#define NBR_ALGORITHMS 3
#define NBR_SCALE_MAPPING 3

//GOLOMB RULER
#define NUM_RULERS 36
#define MAX_DIVISIONS 27
//PERFECT BALANCE
#define NUM_PB_PATTERNS 115
#define MAX_PB_SIZE 73



using namespace frozenwasteland::dsp;

struct EFPitch {
  uint64_t numerator;
  uint64_t denominator;
  float ratio;
  float pitch;
  float dissonance;
  float weighting;
  bool inUse;
  bool operator<(const EFPitch& a) const {
    return pitch < a.pitch;
  }
};


struct ProbablyNoteMN : Module {
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
		NOTE_REDUCTION_ALGORITHM_PARAM,
		NUMBER_OF_NOTES_PARAM,
		NUMBER_OF_NOTES_CV_ATTENUVERTER_PARAM,
		SCALE_MAPPING_PARAM,
		SCALE_PARAM,
		SCALE_CV_ATTENUVERTER_PARAM,
		USE_SCALE_WEIGHTING_PARAM,
		FACTOR_1_PARAM,
		FACTOR_1_CV_ATTENUVERTER_PARAM = FACTOR_1_PARAM + MAX_FACTORS,
		FACTOR_NUMERATOR_1_STEP_PARAM = FACTOR_1_CV_ATTENUVERTER_PARAM + MAX_FACTORS,
		FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM = FACTOR_NUMERATOR_1_STEP_PARAM + MAX_FACTORS,
		FACTOR_DENOMINATOR_1_STEP_PARAM = FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM + MAX_FACTORS,
		FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM = FACTOR_DENOMINATOR_1_STEP_PARAM + MAX_FACTORS,
		WEIGHT_SCALING_PARAM = FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM + MAX_FACTORS,	
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
		NOTE_REDUCTION_ALGORITHM_INPUT,
		SCALE_MAPPING_INPUT,
		SCALE_INPUT,
		USE_SCALE_WEIGHTING_INPUT,
		NUMBER_OF_NOTES_INPUT,
		FACTOR_1_INPUT,
        FACTOR_NUMERATOR_STEP_1_INPUT = FACTOR_1_INPUT + MAX_FACTORS,
        FACTOR_DENOMINATOR_STEP_1_INPUT = FACTOR_NUMERATOR_STEP_1_INPUT + MAX_FACTORS,
		TRIGGER_INPUT = FACTOR_DENOMINATOR_STEP_1_INPUT + MAX_FACTORS,
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
		NOTE_REDUCTION_ALGORITHM_LIGHT,
		SCALE_MAPPING_LIGHT = NOTE_REDUCTION_ALGORITHM_LIGHT + 3,
		SCALE_WEIGHTING_LIGHT = SCALE_MAPPING_LIGHT + 3,
		DISTRIBUTION_GAUSSIAN_LIGHT = SCALE_WEIGHTING_LIGHT + 3,
        OCTAVE_WRAPAROUND_LIGHT,
		SHIFT_MODE_LIGHT,
        KEY_LOGARITHMIC_SCALE_LIGHT = SHIFT_MODE_LIGHT + 3,
		PITCH_RANDOMNESS_GAUSSIAN_LIGHT,
		NUM_LIGHTS 
	};
	enum Algorithms {
		EUCLIDEAN_ALGO,
		GOLUMB_RULER_ALGO,
		PERFECT_BALANCE_ALGO,
	};
	enum ScaleMapping {
		NO_SCALE_MAPPING,
		SPREAD_SCALE_MAPPING,
		REPEAT_SCALE_MAPPING,
	};

	enum ShiftModes {
		SHIFT_STEP_PER_VOLT,
		SHIFT_STEP_BY_V_OCTAVE_RELATIVE,
		SHIFT_STEP_BY_V_OCTAVE_ABSOLUTE,
	};


	const char* algorithms[NBR_ALGORITHMS] = {"Euclidean","Golumb Ruler","Perfect Balance"};


	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
    const int primeNumbers[MAX_PRIME_NUMBERS] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199};

	//GOLOMB RULER PATTERNS
    const int rulerOrders[NUM_RULERS] = {1,2,3,4,5,5,6,6,6,6,7,7,7,7,7,8,9,10,11,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
	const int rulerLengths[NUM_RULERS] = {0,1,3,6,11,11,17,17,17,17,25,25,25,25,25,34,44,55,72,72,85,106,127,151,177,199,216,246,283,333,356,372,425,480,492,553};
	const std::string rulerNames[NUM_RULERS] = {"1","2","3","4","5a","5b","6a","6b","6c","6d","7a","7b","7c","7d","7e","8","9","10","11a","11b","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27"};
	const int rulers[NUM_RULERS][MAX_DIVISIONS] = {{0},
												   {0,1},
												   {0,1,3},
												   {0,1,4,6},
												   {0,1,4,9,11},
												   {0,2,7,8,11},
												   {0,1,4,10,12,17},
												   {0,1,4,10,15,17},
												   {0,1,8,11,13,17},
												   {0,1,8,12,14,17},
												   {0,1,4,10,18,23,25},
												   {0,1,7,11,20,23,25},
												   {0,1,11,16,19,23,25},
												   {0,2,3,10,16,21,25},
												   {0,2,7,13,21,22,25},
												   {0,1,4,9,15,22,32,34},
												   {0,1,5,12,25,27,35,41,44},
												   {0,1,6,10,23,26,34,41,53,55},
												   {0,1,4,13,28,33,47,54,64,70,72},
												   {0,1,9,19,24,31,52,56,58,69,72},
												   {0,2,6,24,29,40,43,55,68,75,76,85},
												   {0,2,5,25,37,43,59,70,85,89,98,99,106},
												   {0,4,6,20,35,52,59,77,78,86,89,99,122,127},
												   {0,4,20,30,57,59,62,76,100,111,123,136,144,145,151},
												   {0,1,4,11,26,32,56,68,76,115,117,134,150,163,168,177},
												   {0,5,7,17,52,56,67,80,81,100,122,138,159,165,168,191,199},
												   {0,2,10,22,53,56,82,83,89,98,130,148,153,167,188,192,205,216},
												   {0,1,6,25,32,72,100,108,120,130,153,169,187,190,204,231,233,242,246},
												   {0,1,8,11,68,77,94,116,121,156,158,179,194,208,212,228,240,253,259,283},
												   {0,2,24,56,77,82,83,95,129,144,179,186,195,255,265,285,293,296,310,329,333},
												   {0,1,9,14,43,70,106,122,124,128,159,179,204,223,253,263,270,291,330,341,353,356},
												   {0,3,7,17,61,66,91,99,114,159,171,199,200,226,235,246,277,316,329,348,350,366,372},
												   {0,9,33,37,38,97,122,129,140,142,152,191,205,208,252,278,286,326,332,353,368,384,403,425},
												   {0,12,29,39,72,91,146,157,160,161,166,191,207,214,258,290,316,354,372,394,396,431,459,467,480},
												   {0,1,33,83,104,110,124,163,185,200,203,249,251,258,314,318,343,356,386,430,440,456,464,475,487,492},
												   {0,3,15,41,66,95,97,106,142,152,220,221,225,242,295,330,338,354,382,388,402,415,486,504,523,546,553}};


		
	//PERFECT BALANCE PATTERNS
    const int pbPatternOrders[NUM_PB_PATTERNS] = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,29,6,7,7,8,8,9,31,37,41,8,9,9,9,10,10,10,10,10,11,11,11,11,11,12,12,12,13,43,47,53,59,61,67,13,13,13,15,15,16,16,16,16,16,16,16,16,16,16,16,10,17,17,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,19,19,19,20,20,22,22,22,25,71,73};
	const int pbPatternLengths[NUM_PB_PATTERNS] = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,29,30,30,30,30,30,30,31,37,41,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,47,53,59,61,67,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,71,73};
	const std::string pbPatternNames[NUM_PB_PATTERNS] = {"2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","27","29","6-in-30","7a-in-30","7b-in-30","8b-in-30","8a-in-30","9-in-30","31","37","41",
	"8-in-42","9a-in-42","9b-in-42","9c-in-42","10a-in-42","10b-in-42","10c-in-42","10d-in-42","10e-in-42","11a-in-42","11b-in-42","11c-in-42","11d-in-42","11e-in-42","12a-in-42","12b-in-42","12c-in-42","13-in-42","43","47","53","59","61","67",
	"13a-in-70","13b-in-70","13c-in-70","15a-in-70","15b-in-70","16a-in-70","16b-in-70","16c-in-70","16d-in-70","16e-in-70","16f-in-70","16g-in-70","16h-in-70","16i-in-70","16j-in-70","16k-in-70","10-in-70","17a-in-70","17b-in-70","17c-in-70","17d-in-70","17e-in-70","17f-in-70","17g-in-70","17h-in-70","17i-in-70","17j-in-70","18a-in-70","18b-in-70","18c-in-70","18d-in-70","18e-in-70","18f-in-70","18g-in-70","18h-in-70","18i-in-70","18j-in-70","19a-in-70","19b-in-70","19c-in-70","19d-in-70","19e-in-70","19f-in-70","19g-in-70","19h-in-70","19i-in-70","19j-in-70","19k-in-70","20a-in-70","20b-in-70","22a-in-70","22b-in-70","22c-in-70","25-in-70","71","73"};
	const int pbPatterns[NUM_PB_PATTERNS][MAX_PB_SIZE] = {
															{0,1},
															{0,1,2},
															{0,1,2,3},
															{0,1,2,3,4},
															{0,1,2,3,4,5},
															{0,1,2,3,4,5,6},
															{0,1,2,3,4,5,6,7},
															{0,1,2,3,4,5,6,7,8},
															{0,1,2,3,4,5,6,7,8,9},
															{0,1,2,3,4,5,6,7,8,9,10},
															{0,1,2,3,4,5,6,7,8,9,10,11},
															{0,1,2,3,4,5,6,7,8,9,10,11,12},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28},
															{0,10,11,17,23,29},
															{0,10,11,17,18,28,29},
															{0,6,12,13,19,23,29},
															{0,6,10,12,16,22,23,29},
															{0,10,11,12,18,22,28,29},
															{0,6,10,12,16,18,22,28,29},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40},
															{0,12,13,18,24,30,36,41},
															{0,12,13,18,23,24,36,37,41},
															{0,11,12,17,23,28,29,40,41},
															{0,8,13,14,19,25,31,36,37},
															{0,12,13,17,18,23,31,36,37,41},
															{0,11,12,17,22,23,28,36,40,41},
															{0,11,12,16,17,28,29,30,40,41},
															{0,11,12,13,17,25,30,31,36,41},
															{0,6,11,12,17,23,28,29,34,40},
															{0,11,12,16,17,22,28,30,36,40,41},
															{0,11,12,13,17,23,25,31,36,37,41},
															{0,10,11,12,22,23,24,28,36,40,41},
															{0,8,13,14,18,19,24,32,36,37,38},
															{0,6,11,12,17,22,23,28,34,36,40},
															{0,10,11,12,16,22,24,28,30,36,40,41},
															{0,8,12,13,14,18,24,26,32,36,37,38},
															{0,6,11,12,16,17,22,28,30,34,36,40},
															{0,6,10,11,12,16,22,24,28,30,34,36,40},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42}, 
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66},
															{0,14,17,26,27,28,37,40,54,56,57,67,68},
															{0,10,14,24,27,28,37,38,47,56,57,66,67},
															{0,10,13,19,27,30,33,41,47,50,60,61,69},
															{0,10,13,16,26,27,30,36,40,46,50,56,60,66,69},
															{0,9,17,19,27,28,29,37,39,47,56,57,59,67,69},
															{0,16,17,19,27,28,29,30,39,47,56,57,58,59,67,69},
															{0,14,16,17,26,27,28,30,40,44,54,56,57,58,67,68},
															{0,10,14,16,24,27,28,30,38,44,47,56,57,58,66,67},
															{0,10,13,16,26,27,30,36,39,40,50,53,56,66,67,69},
															{0,10,13,16,19,27,30,33,36,46,47,50,56,60,66,69},
															{0,10,13,14,23,24,27,33,37,43,47,53,56,57,66,67},
															{0,10,13,14,22,23,24,33,36,42,50,52,53,56,64,66},
															{0,10,11,13,19,27,30,33,39,41,47,50,53,61,67,69},
															{0,9,17,18,19,27,28,29,37,46,47,56,57,59,60,69},
															{0,9,10,18,19,28,29,32,38,42,46,52,56,60,66,69},
															{0,8,11,17,20,28,30,31,39,40,48,50,58,59,67,68},
															{0,14,17,27,28,37,47,56,57,67},
															{0,16,17,26,27,28,29,30,39,40,56,57,58,59,67,68,69},
															{0,10,16,19,27,28,29,30,38,39,47,56,57,58,66,67,69},
															{0,10,13,16,26,27,29,30,39,40,43,53,56,57,66,67,69},
															{0,10,13,16,19,27,30,33,36,39,47,50,53,56,66,67,69},
															{0,10,13,16,19,27,29,30,33,43,46,47,56,57,60,66,69},
															{0,10,13,14,16,24,27,30,33,43,44,47,53,56,57,66,67},
															{0,10,11,13,14,22,23,24,33,41,42,50,51,52,53,61,64},
															{0,9,10,18,19,27,28,29,37,38,46,47,56,57,60,66,69},
															{0,9,10,13,19,23,29,32,33,42,43,46,52,56,60,66,69},
															{0,8,16,17,19,27,28,30,36,39,47,50,56,58,59,67,69},
															{0,16,17,18,26,27,28,29,30,40,46,56,57,58,59,60,68,69},
															{0,10,16,18,19,27,28,29,30,38,46,47,56,57,58,60,66,69},
															{0,10,13,16,19,27,29,30,33,39,43,47,53,56,57,66,67,69},
															{0,10,13,14,23,24,26,27,36,37,40,50,53,54,56,64,66,67},
															{0,10,13,14,16,24,27,30,33,36,44,47,50,53,56,64,66,67},
															{0,10,11,14,20,22,23,24,33,34,42,50,51,52,53,61,62,64},
															{0,10,11,13,14,23,24,27,33,37,41,47,50,51,53,61,64,67},
															{0,9,10,13,19,23,27,29,33,37,43,46,47,56,57,60,66,69},
															{0,8,11,17,19,27,28,30,31,39,41,47,50,58,59,61,67,69},
															{0,8,9,17,18,26,27,28,36,37,40,46,50,56,59,60,68,69},
															{0,10,16,18,26,27,28,29,30,38,40,46,56,57,58,60,66,68,69},
															{0,10,14,16,24,26,27,28,30,38,40,44,54,56,57,58,66,67,68},
															{0,10,13,14,16,24,26,27,30,36,40,44,50,53,54,56,64,66,67},
															{0,9,10,13,19,23,27,29,33,37,39,43,47,53,56,57,66,67,69},
															{0,9,10,12,18,26,28,29,32,38,40,42,46,52,56,60,66,68,69},
															{0,9,10,11,13,19,23,27,33,37,39,41,47,50,51,53,61,67,69},
															{0,8,16,17,18,26,27,28,30,36,40,46,50,56,58,59,60,68,69},
															{0,8,11,14,17,20,28,30,31,34,40,44,48,50,54,58,64,67,68},
															{0,8,11,12,14,20,22,28,31,34,40,42,48,50,51,54,62,64,68},
															{0,8,10,16,18,19,27,28,30,36,38,46,47,50,56,58,60,66,69},
															{0,8,9,12,18,22,26,28,32,36,40,42,46,50,56,59,60,68,69},
															{0,8,10,16,18,26,27,28,30,36,38,40,46,50,56,58,60,66,68,69},
															{0,8,10,11,14,20,24,28,30,34,38,40,44,48,50,54,58,64,67,68},
															{0,8,10,14,16,24,26,27,28,30,36,38,40,44,50,54,56,58,64,66,67,68},
															{0,8,9,10,12,18,22,26,28,32,36,38,40,42,46,50,52,56,60,66,68,69},
															{0,6,9,10,12,18,20,26,28,29,32,38,40,42,46,48,52,56,60,62,66,68},
															{0,6,8,10,14,16,20,24,26,28,30,34,36,38,40,44,48,50,54,56,58,64,66,67,68},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70},
															{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72}};



	const char* scaleNames[MAX_SCALES] = {"Chromatic","Whole Tone","Ionian M","Dorian","Phrygian","Lydian","Mixolydian","Aeolian m","Locrian","Acoustic", "Altered", "Augmented", "Bebop Dominant", "Blues",  
											"Enigmatic","Flamenco","Gypsy","Half diminished","Harmonic Major","Harmonic Minor","Hirajoshi",
											"Hungarian","Miyako-bushi", "Insen", "Iwato", "Lydian Augmented",  "Bebob Major", "Locrian Major",
											"Pentatonic Major","Melodic Minor","Pentatonic Minor-Yo","Neapoliltan Major", "Neapolitan Minor", "Octatonic 1",  "Octatonic 2",
											"Persian","Phrygian Dominant","Prometheus", "Harmonics", "Tritone", "Tritone 2S", "Ukrainian Dorian"};
	float defaultScaleNoteWeighting[MAX_SCALES][MAX_NOTES] = {
		{1,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,1,0,1,0,1,0,1,0,1,0},
		{1,0,0.2,0,0.5,0.4,0,0.8,0,0.2,0,0.3},
		{1,0,0.2,0.5,0,0.4,0,0.8,0,0.2,0.3,0},
		{1,0.2,0,0.5,0,0.4,0,0.8,0.2,0,0.3,0},
		{1,0,0.2,0,0.5,0,0.4,0.8,0,0.2,0,0.3},
		{1,0,0.2,0,0.5,0.4,0,0.8,0,0.2,0.3,0},
		{1,0,0.2,0.5,0,0.4,0,0.8,0.2,0,0.3,0},
		{1,0.2,0,0.5,0,0.4,0.8,0,0.2,0,0.3,0},
		{1.0,0.0,0.2,0.0,0.3,0.0,0.5,0.8,0.0,0.2,0.3,0.0},
		{1.0,0.2,0.0,0.3,0.5,0.0,0.8,0.0,0.2,0.0,0.3,0.0},
		{1.0,0.0,0.0,0.2,0.3,0.0,0.0,0.5,0.8,0.0,0.0,0.2},
		{1.0,0.0,0.2,0.0,0.3,0.5,0.0,0.8,0.0,0.2,0.3,0.2},
		{1.0,0.0,0.0,0.2,0.0,0.3,0.5,0.8,0.0,0.0,0.2,0.0},
		{1.0,0.2,0.0,0.0,0.3,0.0,0.5,0.0,0.8,0.0,0.2,0.3},
		{1.0,0.2,0.0,0.0,0.3,0.5,0.0,0.8,0.2,0.0,0.0,0.3},
		{1.0,0.0,0.2,0.3,0.0,0.0,0.5,0.8,0.2,0.0,0.3,0.0},
		{1.0,0.0,0.2,0.3,0.0,0.5,0.8,0.0,0.2,0.0,0.3,0.0},
		{1.0,0.0,0.2,0.0,0.3,0.5,0.0,0.8,0.2,0.0,0.0,0.3},
		{1.0,0.0,0.2,0.3,0.0,0.5,0.0,0.8,0.2,0.0,0.0,0.3},
		{1.0,0.0,0.0,0.0,0.2,0.0,0.3,0.5,0.0,0.0,0.0,0.8},
		{1.0,0.0,0.2,0.3,0.0,0.0,0.5,0.8,0.2,0.0,0.0,0.3},
		{1.0,0.2,0.0,0.0,0.0,0.3,0.0,0.5,0.8,0.0,0.0,0.0},
		{1.0,0.2,0.0,0.0,0.0,0.3,0.0,0.5,0.0,0.0,0.0,0.8},
		{1.0,0.2,0.0,0.0,0.0,0.3,0.5,0.0,0.0,0.0,0.8,0.0},
		{1.0,0.0,0.2,0.0,0.3,0.0,0.5,0.0,0.8,0.2,0.0,0.3},
		{1.0,0.0,0.2,0.0,0.3,0.5,0.0,0.8,0.2,0.0,0.3,0.2},
		{1.0,0.0,0.2,0.0,0.3,0.5,0.8,0.0,0.2,0.0,0.3,0.0},
		{1.0,0.0,0.2,0.0,0.3,0.0,0.0,0.5,0.0,0.8,0.0,0.0},
		{1.0,0.0,0.2,0.3,0.0,0.5,0.0,0.8,0.0,0.2,0.0,0.3},
		{1.0,0.0,0.0,0.2,0.0,0.3,0.0,0.5,0.0,0.0,0.8,0.0},
		{1.0,0.2,0.0,0.3,0.0,0.5,0.0,0.8,0.0,0.2,0.0,0.3},
		{1.0,0.2,0.0,0.3,0.0,0.5,0.0,0.8,0.2,0.0,0.0,0.3},
		{1.0,0.0,0.2,0.3,0.0,0.5,0.8,0.0,0.2,0.3,0.0,0.2},
		{1.0,0.2,0.0,0.3,0.5,0.0,0.8,0.2,0.0,0.3,0.2,0.0},
		{1.0,0.2,0.0,0.0,0.3,0.5,0.8,0.0,0.2,0.0,0.0,0.3},
		{1.0,0.2,0.0,0.0,0.3,0.5,0.0,0.8,0.2,0.0,0.3,0.0},
		{1.0,0.0,0.2,0.0,0.3,0.0,0.5,0.0,0.0,0.8,0.2,0.0},
		{1.0,0.0,0.0,0.2,0.3,0.5,0.0,0.8,0.0,0.2,0.0,0.0},
		{1.0,0.2,0.0,0.0,0.3,0.0,0.5,0.8,0.0,0.0,0.2,0.0},
		{1.0,0.2,0.3,0.0,0.0,0.0,0.5,0.8,0.2,0.0,0.0,0.0},
		{1.0,0.0,0.2,0.3,0.0,0.0,0.5,0.8,0.0,0.2,0.3,0.0}
	}; 
	bool defaultScaleNoteStatus[MAX_SCALES][MAX_NOTES] = {
		{1,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,1,0,1,0,1,0,1,0,1,0},
		{1,0,1,0,1,1,0,1,0,1,0,1},
		{1,0,1,1,0,1,0,1,0,1,1,0},
		{1,1,0,1,0,1,0,1,1,0,1,0},
		{1,0,1,0,1,0,1,1,0,1,0,1},
		{1,0,1,0,1,1,0,1,0,1,1,0},
		{1,0,1,1,0,1,0,1,1,0,1,0},
		{1,1,0,1,0,1,1,0,1,0,1,0},		
		{1,0,1,0,1,0,1,1,0,1,1,0},//new
		{1,1,0,1,1,0,1,0,1,0,1,0},
		{1,0,0,1,1,0,0,1,1,0,0,1},
		{1,0,1,0,1,1,0,1,0,1,1,1},
		{1,0,0,1,0,1,1,1,0,0,1,0},
		{1,1,0,0,1,0,1,0,1,0,1,1},
		{1,1,0,0,1,1,0,1,1,0,0,1},
		{1,0,1,1,0,0,1,1,1,0,1,0},
		{1,0,1,1,0,1,1,0,1,0,1,0},
		{1,0,1,0,1,1,0,1,1,0,0,1},
		{1,0,1,1,0,1,0,1,1,0,0,1},
		{1,0,0,0,1,0,1,1,0,0,0,1},
		{1,0,1,1,0,0,1,1,1,0,0,1},
		{1,1,0,0,0,1,0,1,1,0,0,0},
		{1,1,0,0,0,1,0,1,0,0,0,1},
		{1,1,0,0,0,1,1,0,0,0,1,0},
		{1,0,1,0,1,0,1,0,1,1,0,1},
		{1,0,1,0,1,1,0,1,1,0,1,1},
		{1,0,1,0,1,1,1,0,1,0,1,0},
		{1,0,1,0,1,0,0,1,0,1,0,0},
		{1,0,1,1,0,1,0,1,0,1,0,1},
		{1,0,0,1,0,1,0,1,0,0,1,0},
		{1,1,0,1,0,1,0,1,0,1,0,1},
		{1,1,0,1,0,1,0,1,1,0,0,1},
		{1,0,1,1,0,1,1,0,1,1,0,1},
		{1,1,0,1,1,0,1,1,0,1,1,0},
		{1,1,0,0,1,1,1,0,1,0,0,1},
		{1,1,0,0,1,1,0,1,1,0,1,0},
		{1,0,1,0,1,0,1,0,0,1,1,0},
		{1,0,0,1,1,1,0,1,0,1,0,0},
		{1,1,0,0,1,0,1,1,0,0,1,0},
		{1,1,1,0,0,0,1,1,1,0,0,0},
		{1,0,1,1,0,0,1,1,0,1,1,0}
	}; 
	
	dsp::SchmittTrigger clockTrigger,resetScaleTrigger,algorithmTrigger,scaleMappingTrigger,useScaleWeightingTrigger,octaveWrapAroundTrigger,shiftScalingTrigger,keyScalingTrigger,pitchRandomnessGaussianTrigger; 
	dsp::PulseGenerator noteChangePulse[POLYPHONY];
    GaussianNoiseGenerator _gauss;
 
    bool octaveWrapAround = false;
    //bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteProbability[POLYPHONY][MAX_PITCHES] = {{0.0f}};
    float currentScaleNoteWeighting[MAX_NOTES] = {0.0f};
	bool currentScaleNoteStatus[MAX_NOTES] = {false};
	float actualProbability[POLYPHONY][MAX_NOTES] = {{0.0f}};
	int controlIndex[MAX_NOTES] = {0};

	uint8_t factors[MAX_FACTORS] = {0};
    uint8_t lastFactors[MAX_FACTORS] = {0};
    uint8_t stepsN[MAX_FACTORS] = {0};
    uint8_t lastNSteps[MAX_FACTORS] = {0};
    uint8_t actualNSteps[MAX_FACTORS] = {0};
    uint8_t stepsD[MAX_FACTORS] = {0};
    uint8_t lastDSteps[MAX_FACTORS] = {0};
    uint8_t actualDSteps[MAX_FACTORS] = {0};

	bool reloadPitches = false;

	bool triggerDelayEnabled = false;
	float triggerDelay[TRIGGER_DELAY_SAMPLES] = {0};
	int triggerDelayIndex = 0;


    std::vector<EFPitch> efPitches;
	std::vector<float> numeratorList;
	std::vector<float> denominatorList;
    std::vector<EFPitch> reducedEfPitches;
	std::vector<uint8_t> pitchIncluded;
	
	uint64_t nbrPitches = 0;
	uint64_t lastNbrPitches = 0;
	uint64_t scaleSize = 0;
	uint64_t lastScaleSize = -1;
	uint64_t actualScaleSize;
	int noteReductionAlgorithm = 0;
	int lastNoteReductionAlgorithm = 0;
	std::string noteReductionPatternName;

	int scaleMappingMode = 0;
	int lastScaleMappingMode = -1;
	bool useScaleWeighting = false;
	bool lastUseScaleWeighting;

	bool mapPitches = false;

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


	
	ProbablyNoteMN() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteMN::SPREAD_PARAM, 0.0, 1.0, 0.0,"Spread");
        configParam(ProbablyNoteMN::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteMN::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant");
        configParam(ProbablyNoteMN::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteMN::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNoteMN::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNoteMN::SHIFT_PARAM, -12.0, 12.0, 0.0,"Weight Shift");
        configParam(ProbablyNoteMN::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteMN::KEY_PARAM, 0.0, 10.0, 0.0,"Key");
        configParam(ProbablyNoteMN::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNoteMN::NUMBER_OF_NOTES_PARAM, 1.0, 115.0, 1.0,"Max # of Notes in Scale");
        configParam(ProbablyNoteMN::NUMBER_OF_NOTES_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"# of Notes CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::SCALE_PARAM, 0.0, MAX_SCALES-1.0, 0.0,"Mapped Scale");
        configParam(ProbablyNoteMN::SCALE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Mapped Scale CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
		configParam(ProbablyNoteMN::DISSONANCE_PARAM, -1.0, 1.0, 0.0,"Consonance/Dissonance Balance","%",0,100);
        configParam(ProbablyNoteMN::DISSONANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Dissonance CV Attenuation","%",0,100); 
        configParam(ProbablyNoteMN::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::OCTAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Octave Wraparound");
		configParam(ProbablyNoteMN::WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Weight Scaling","%",0,100);
		configParam(ProbablyNoteMN::PITCH_RANDOMNESS_PARAM, 0.0, 10.0, 0.0,"Randomize Pitch Amount"," Cents");
        configParam(ProbablyNoteMN::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Randomize Pitch Amount CV Attenuation","%",0,100);


        srand(time(NULL));

        for(int i=0;i<MAX_FACTORS;i++) {
            configParam(ProbablyNoteMN::FACTOR_1_PARAM + i, i, MAX_PRIME_NUMBERS-MAX_FACTORS+i, i,"Factor");		
            configParam(ProbablyNoteMN::FACTOR_1_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Factor CV Attenuverter","%",0,100);		
            configParam(ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_PARAM + i, 0.0, 10.0, 0.0,"Numerator Step Count");		
            configParam(ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Numerator Step Count CV Attenuverter","%",0,100);		
            configParam(ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_PARAM + i, 0.0, 5.0, 0.0,"Denominator Step Count");		
            configParam(ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Denominator Step Count CV Attenuverter","%",0,100);		
        }

		onReset();
	}

	void reConfigParam (int paramId, float minValue, float maxValue, float defaultValue) {
		ParamQuantity *pq = paramQuantities[paramId];
		pq->minValue = minValue;
		pq->maxValue = maxValue;
		pq->defaultValue = defaultValue;		
	}

    double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,float scaling,float randomIn) {
        double weightTotal = 0.0f;
		double linearWeight, logWeight, weight;
            
        for(int i = 0;i <actualScaleSize; i++) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

        int chosenWeight = -1;        
        double rnd = randomIn * weightTotal;
        for(int i = 0;i <actualScaleSize;i++ ) {
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


    void BuildDerivedScale()
    {
        efPitches.clear();
		numeratorList.clear();
		denominatorList.clear();
        efPitches.resize(0);
		numeratorList.resize(0);
		denominatorList.resize(0);

        //EFPitch efPitch = new EFPitch();
        EFPitch efPitch;
        efPitch.numerator = 1;
        efPitch.denominator = 1;
		efPitch.ratio = 1;
		efPitch.pitch = 0.0;
		efPitch.dissonance = 0.0;
        efPitches.push_back(efPitch);

		numeratorList.push_back(1);
		denominatorList.push_back(1);

        //Check that we aren't getting too crazy - Simplifying out for now 
        for(uint8_t f = 0; f< MAX_FACTORS;f++) {
            actualDSteps[f] = stepsD[f]; 
            actualNSteps[f] = stepsN[f];
        }
 
        //Calculate Numerators (basically all factors multiplied by each other)
		for (uint8_t fN = 0; fN < MAX_FACTORS; fN++) {
			uint16_t n = numeratorList.size();
			//fprintf(stderr, "f: %i n: %i  \n", f, n);
			for (int s = 1; s <= actualNSteps[fN]; s++) {
				for (int i = 0; i < n; i++) {
					float numerator = (numeratorList[i] * std::pow(factors[fN], s));
					//fprintf(stderr, "n: %f \n", numerator);
					if(numerator > 0.0)
						numeratorList.push_back(numerator);
				}
			}
		}

        //Calculate Denominators (basically all factors multiplied by each other)
		for (uint8_t fD = 0; fD < MAX_FACTORS; fD++) {
			uint16_t n = denominatorList.size();
			//fprintf(stderr, "f: %i n: %i  \n", f, n);
			for (int s = 1; s <= actualDSteps[fD]; s++) {
				for (int i = 0; i < n; i++) {
					float denominator = (denominatorList[i] * std::pow(factors[fD], s));
					//fprintf(stderr, "d: %f \n", denominator);
					if(denominator > 0.0)
						denominatorList.push_back(denominator);
				}
			}
		}

        for (uint16_t d = 1; d < denominatorList.size(); d++) {
			for (uint16_t n = 1; n < numeratorList.size(); n++) {
				EFPitch efPitch;
				float numerator = numeratorList[n];
				float denominator = ScaleDenominator(numerator,denominatorList[d]);        
				numerator = ScaleNumerator(numerator,denominator);
				efPitch.numerator = (uint64_t) numerator;
				efPitch.denominator = (uint64_t) denominator;        
				float ratio = numerator / denominator;
				efPitch.ratio = ratio;
				if(numerator > 0 && IsUniqueRatio(ratio)) {
					uint64_t gcd = GCD(efPitch.numerator,efPitch.denominator);
					efPitch.numerator = efPitch.numerator / gcd;
					efPitch.denominator = efPitch.denominator / gcd;
					float pitchInCents = 1200 * std::log2f(ratio);
					efPitch.pitch = pitchInCents;
					float dissonance = CalculateDissonance(efPitch.numerator,efPitch.denominator);
					efPitch.dissonance = dissonance;
					//fprintf(stderr, "n: %llu d: %llu  p: %f \n", efPitch.numerator, efPitch.denominator, pitchInCents);
	
					efPitches.push_back(efPitch);
				}
			}
		}
		
        nbrPitches = efPitches.size(); //Actual Number of pitches
        std::sort(efPitches.begin(), efPitches.end());
        //fprintf(stderr, "Number of Pitches: %llu \n",nbrPitches);
    }

	bool IsUniqueRatio(float ratio) {
		bool isUnique = true;
		for(int i = 0;i<efPitches.size();i++) {
			if(efPitches[i].ratio == ratio) {
				isUnique = false;
				break;
			}
		}
		return isUnique;
	}
    

    float ScaleDenominator(float numerator,float denominator)
    {
		float newDenominator = denominator;
        while (numerator/newDenominator > 2.0)
        {
            newDenominator = newDenominator * 2.0;
        }

        return newDenominator;
    }

	float ScaleNumerator(float numerator,float denominator)
    {
		float newNumerator = numerator;
        while (newNumerator/denominator < 1.0)
        {
            newNumerator = newNumerator * 2.0;
			//fprintf(stderr, "scaling: %f \n", newNumerator);
        }

        return newNumerator;
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

	void ChooseNotes(uint64_t totalSize, uint64_t scaleSize) {
		pitchIncluded.clear();
		pitchIncluded.resize(totalSize);
		if(totalSize == 0 || scaleSize == 0) 
			return;
		for(int i=0;i<totalSize;i++) {
			pitchIncluded[i] = 0;
		}
		
		switch(noteReductionAlgorithm) {
			case EUCLIDEAN_ALGO :
				EuclideanAlgo(totalSize,scaleSize);
				break;
			case GOLUMB_RULER_ALGO :
				GolumbRulerAlgo(totalSize,scaleSize);
				break;
			case PERFECT_BALANCE_ALGO :
				PerfectBalanceAlgo(totalSize,scaleSize);
				break;
		}
	}

	void EuclideanAlgo(uint64_t totalSize, uint64_t scaleSize) {
		uint64_t bucket = totalSize - 1;                    
		for(uint64_t euclideanStepIndex = 0; euclideanStepIndex < totalSize; euclideanStepIndex++)
		{
			bucket += scaleSize;
			if(bucket >= totalSize) {
				bucket -= totalSize;
				pitchIncluded[euclideanStepIndex] = 1;	
			}                        
		}
		noteReductionPatternName = std::to_string(scaleSize) + " of " + std::to_string(totalSize);
	}

	void GolumbRulerAlgo(uint64_t totalSize, uint64_t scaleSize) {
		int rulerToUse = clamp(scaleSize-1,0,NUM_RULERS-1);
		while(rulerLengths[rulerToUse] + 1 > totalSize && rulerToUse >= 0) {
			rulerToUse -=1;
		}

		noteReductionPatternName = rulerNames[rulerToUse] + " of " + std::to_string(totalSize); 
		
		//Multiply beats so that low division beats fill out entire pattern
		float spaceMultiplier = (totalSize / (rulerLengths[rulerToUse] + 1));

		for (int rulerIndex = 0; rulerIndex < rulerOrders[rulerToUse];rulerIndex++)
		{
			uint64_t noteLocation = (rulers[rulerToUse][rulerIndex] * spaceMultiplier);
			pitchIncluded[noteLocation] = 1;
		}
	}

	void PerfectBalanceAlgo(uint64_t totalSize, uint64_t scaleSize) {
		int pbPatternToUse = -1;
		int pbMatchPatternCount = 0;
		int pbLastMatchedPattern = 0;
		bool patternFound = false;
		while(!patternFound) {
			pbPatternToUse +=1;
			if(pbPatternToUse >= NUM_PB_PATTERNS)
				break;
			if(pbPatternLengths[pbPatternToUse] <= totalSize && totalSize % pbPatternLengths[pbPatternToUse] == 0) {
				pbLastMatchedPattern = pbPatternToUse;
				if(pbMatchPatternCount >= scaleSize-1) {
					patternFound = true;
				} else {
					pbMatchPatternCount +=1;							
				}
			}
		}
		if(!patternFound)
			pbPatternToUse = pbLastMatchedPattern;
		
		//Multiply beats so that low division beats fill out entire pattern
		float spaceMultiplier = (totalSize / (pbPatternLengths[pbPatternToUse]));
		noteReductionPatternName =pbPatternNames[pbPatternToUse] + " of " + std::to_string(totalSize); 

		for (int pbIndex = 0; pbIndex < pbPatternOrders[pbPatternToUse];pbIndex++)
		{
			uint64_t noteLocation = (pbPatterns[pbPatternToUse][pbIndex] * spaceMultiplier);
			pitchIncluded[noteLocation] = 1;
		}
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "triggerDelayEnabled", json_integer((bool) triggerDelayEnabled));
		json_object_set_new(rootJ, "noteReductionAlgorithm", json_integer((int) noteReductionAlgorithm));
		json_object_set_new(rootJ, "scaleMappingMode", json_integer((int) scaleMappingMode));
		json_object_set_new(rootJ, "useScaleWeighting", json_integer((bool) useScaleWeighting));
		json_object_set_new(rootJ, "octaveWrapAround", json_integer((int) octaveWrapAround));
		json_object_set_new(rootJ, "shiftMode", json_integer((int) shiftMode));
		json_object_set_new(rootJ, "keyLogarithmic", json_integer((int) keyLogarithmic));
		json_object_set_new(rootJ, "pitchRandomGaussian", json_integer((int) pitchRandomGaussian));

		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *ctTd = json_object_get(rootJ, "triggerDelayEnabled");
		if (ctTd)
			triggerDelayEnabled = json_integer_value(ctTd);

		json_t *ctNra = json_object_get(rootJ, "noteReductionAlgorithm");
		if (ctNra)
			noteReductionAlgorithm = json_integer_value(ctNra);

		json_t *ctSmm = json_object_get(rootJ, "scaleMappingMode");
		if (ctSmm)
			scaleMappingMode = json_integer_value(ctSmm);

		json_t *ctUsw = json_object_get(rootJ, "useScaleWeighting");
		if (ctUsw)
			useScaleWeighting = json_integer_value(ctUsw);

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
	
	}
	

	void process(const ProcessArgs &args) override {
	
        if (resetScaleTrigger.process(params[RESET_SCALE_PARAM].getValue())) {
			resetTriggered = true;
			lastWeightShift = 0;			
		}		

        if (algorithmTrigger.process(params[NOTE_REDUCTION_ALGORITHM_PARAM].getValue() + inputs[NOTE_REDUCTION_ALGORITHM_INPUT].getVoltage())) {
			noteReductionAlgorithm = (noteReductionAlgorithm + 1) % NBR_ALGORITHMS;
		}		
		switch (noteReductionAlgorithm) {
			case EUCLIDEAN_ALGO :
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT].value = 1;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 1].value = 1;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 2].value = 0;
				break;
			case GOLUMB_RULER_ALGO :
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT].value = 0;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 1].value = 0;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 2].value = 1;
				break;
			case PERFECT_BALANCE_ALGO :
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT].value = 1;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 1].value = .5;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 2].value = 0.0;
				break;
		}

		if (scaleMappingTrigger.process(params[SCALE_MAPPING_PARAM].getValue() + inputs[SCALE_MAPPING_INPUT].getVoltage())) {
			scaleMappingMode = (scaleMappingMode + 1) % NBR_SCALE_MAPPING;
		}		
		switch (scaleMappingMode) {
			case NO_SCALE_MAPPING :
				lights[SCALE_MAPPING_LIGHT].value = 0;
				lights[SCALE_MAPPING_LIGHT + 1].value = 0;
				lights[SCALE_MAPPING_LIGHT + 2].value = 0;
				break;
			case SPREAD_SCALE_MAPPING :
				lights[SCALE_MAPPING_LIGHT].value = 0;
				lights[SCALE_MAPPING_LIGHT + 1].value = 1;
				lights[SCALE_MAPPING_LIGHT + 2].value = 0;
				break;
			case REPEAT_SCALE_MAPPING :
				lights[SCALE_MAPPING_LIGHT].value = 0;
				lights[SCALE_MAPPING_LIGHT + 1].value = 0;
				lights[SCALE_MAPPING_LIGHT + 2].value = 1.0;
				break;
		}

        if (useScaleWeightingTrigger.process(params[USE_SCALE_WEIGHTING_PARAM].getValue() + inputs[USE_SCALE_WEIGHTING_INPUT].getVoltage())) {
			useScaleWeighting = !useScaleWeighting;
		}		
		lights[SCALE_WEIGHTING_LIGHT+2].value = useScaleWeighting;
		
		
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
        
		scaleSize = clamp(params[NUMBER_OF_NOTES_PARAM].getValue() + (inputs[NUMBER_OF_NOTES_INPUT].getVoltage() * 11.5f * params[NUMBER_OF_NOTES_CV_ATTENUVERTER_PARAM].getValue()),1.0f,115.0);

        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,MAX_SCALES-1.0f);

        bool scaleChange = false;
        for(int i=0;i<MAX_FACTORS;i++) {
            factors[i] = primeNumbers[clamp((int) (params[FACTOR_1_PARAM+ i].getValue() + (inputs[FACTOR_1_INPUT + i].getVoltage() * MAX_PRIME_NUMBERS / 10.0 * params[FACTOR_1_CV_ATTENUVERTER_PARAM+i].getValue())),0,MAX_PRIME_NUMBERS-1)];
            stepsN[i] = clamp(params[FACTOR_NUMERATOR_1_STEP_PARAM+ i].getValue() + (inputs[FACTOR_NUMERATOR_STEP_1_INPUT + i].getVoltage() * 1.0f * params[FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM+i].getValue()),0.0f,10.0f);
            stepsD[i] = clamp(params[FACTOR_DENOMINATOR_1_STEP_PARAM+ i].getValue() + (inputs[FACTOR_DENOMINATOR_STEP_1_INPUT + i].getVoltage() * 0.5f * params[FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM+i].getValue()),0.0f,5.0f);
            scaleChange = scaleChange || (factors[i] != lastFactors[i]) || (stepsN[i] != lastNSteps[i]) || (stepsD[i] != lastDSteps[i]);
            lastFactors[i] = factors[i];
            lastNSteps[i] = stepsN[i];
            lastDSteps[i] = stepsD[i];
        }
		//fprintf(stderr,"denom value: %f",(inputs[FACTOR_DENOMINATOR_STEP_1_INPUT + 1].getVoltage() * 1.0f * params[FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM+1].getValue()))
        if(scaleChange) {
			reloadPitches = true;
            //fprintf(stderr, "New Scale Needed\n");
            BuildDerivedScale();
			//reConfigParam(NUMBER_OF_NOTES_PARAM,0,nbrPitches,0.0f); //not going to do this for now
        }

		if(nbrPitches != lastNbrPitches || scaleSize != lastScaleSize || noteReductionAlgorithm != lastNoteReductionAlgorithm ) {
			reloadPitches = true;
			ChooseNotes(nbrPitches,scaleSize);

			lastNbrPitches = nbrPitches;
			lastNoteReductionAlgorithm = noteReductionAlgorithm;
			lastScaleSize = scaleSize;
		}

		if(reloadPitches) {
			reducedEfPitches.clear();
			for(int i=0;i<nbrPitches;i++) {
				if(pitchIncluded[i] > 0) {
					efPitches[i].inUse = false;
					reducedEfPitches.push_back(efPitches[i]);
				}
			}
			reloadPitches = false;
			mapPitches = true;
		}

		if(scaleMappingMode != lastScaleMappingMode || scale != lastScale || useScaleWeighting != lastUseScaleWeighting ) {
			mapPitches = true;
		}

		if(mapPitches) {
			actualScaleSize = reducedEfPitches.size();
			float scaleSpreadFactor = actualScaleSize / float(MAX_NOTES);
			switch(scaleMappingMode) {
				case NO_SCALE_MAPPING :
					for(int i=0;i<actualScaleSize;i++) {
						reducedEfPitches[i].inUse = true;
						reducedEfPitches[i].weighting = 0.8;
					}
					break;
				case SPREAD_SCALE_MAPPING :
					for(int i=0;i<reducedEfPitches.size();i++) {
						reducedEfPitches[i].inUse = false;
                        reducedEfPitches[i].weighting = 0.8;
					}
					for(int i=0;i<MAX_NOTES;i++) {
						if(defaultScaleNoteStatus[scale][i]) {
							int noteIndex = i * scaleSpreadFactor;
							//fprintf(stderr, "note index set %i %i \n", noteIndex,reducedEfPitches[noteIndex].inUse);
							reducedEfPitches[noteIndex].inUse = true;
							reducedEfPitches[noteIndex].weighting = useScaleWeighting ? defaultScaleNoteWeighting[scale][i] : 1.0;
						}
					}
					break;
				case REPEAT_SCALE_MAPPING:
					int scaleIndex = 0;
					for(int i=0;i<actualScaleSize;i++) {
						if(defaultScaleNoteStatus[scale][scaleIndex]) {
							reducedEfPitches[i].inUse = true;
							reducedEfPitches[i].weighting = useScaleWeighting ? defaultScaleNoteWeighting[scale][scaleIndex] : 1.0;
						} else {
							reducedEfPitches[i].inUse = false;
                            reducedEfPitches[i].weighting = 0.8;
						}
						scaleIndex = (scaleIndex+1) % MAX_NOTES;
					}
					break;
			}
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
			double noteIn = inputs[NOTE_INPUT].getVoltage(channel) - (float(key)/12.0);
			octaveIn[channel] = std::floor(noteIn);
			double fractionalValue = noteIn - octaveIn[channel];
			if(fractionalValue < 0.0)
				fractionalValue += 1.0;

											              //fprintf(stderr, "%i %f \n", key, fractionalValue);
			double lastDif = 1.0f;    
			for(int i = 0;i<actualScaleSize;i++) {            
				if(reducedEfPitches[i].inUse) {
					double currentDif = std::abs((reducedEfPitches[i].pitch / 1200.0) - fractionalValue);
					if(currentDif < lastDif) {
						lastDif = currentDif;
						currentNote[channel] = i;
					}            
				}
			}
			if(currentNote[channel] != lastNote[channel]) {
				noteChange = true;
				lastNote[channel] = currentNote[channel];
			}
		}

		if(noteChange || lastSpread != spread || lastSlant != slant || lastFocus != focus || dissonanceProbability != lastDissonanceProbability 
					  || mapPitches ) {
			float actualSpread = spread * float(actualScaleSize) / 2.0;
			upperSpread = std::ceil(actualSpread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil(actualSpread * std::min(1.0-slant,1.0));

			float whatIsDissonant = 7.0; // lower than this is consonant
			for(int channel = 0;channel<currentPolyphony;channel++) {
				for(uint64_t i = 0; i<actualScaleSize;i++) {
					noteProbability[channel][i] = 0.0;
				}

				EFPitch currentPitch = reducedEfPitches[currentNote[channel]];
				noteProbability[channel][currentNote[channel]] = 1.0;

			
				for(int i=1;i<=actualSpread;i++) {
					int noteAbove = (currentNote[channel] + i) % actualScaleSize;
					int noteBelow = (currentNote[channel] - i);
					if(noteBelow < 0)
						noteBelow +=actualScaleSize;

					EFPitch upperPitch = reducedEfPitches[noteAbove];
					EFPitch lowerPitch = reducedEfPitches[noteBelow];
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
					

					float upperInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/actualSpread) * upperNoteDissonanceProbabilityAdjustment * upperPitch.weighting; 
					float lowerInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/actualSpread) * lowerNoteDissonanceProbabilityAdjustment * lowerPitch.weighting; 

					noteProbability[channel][noteAbove] = i <= upperSpread ? upperInitialProbability : 0.0f;				
					noteProbability[channel][noteBelow] = i <= lowerSpread ? lowerInitialProbability : 0.0f;				
				}
			}
			lastSpread = spread;
			lastSlant = slant;
			lastFocus = focus;
			lastDissonanceProbability = dissonanceProbability; 
			lastScaleMappingMode = scaleMappingMode;
			lastScale = scale;
			lastUseScaleWeighting = useScaleWeighting;
			mapPitches = false;
		}


		//Process scales, keys and weights
		if(key != lastKey || weightShift != lastWeightShift || resetTriggered) {		
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
			
					int randomNote = weightedProbability(noteProbability[channel],params[WEIGHT_SCALING_PARAM].getValue(), rnd);
					if(randomNote == -1) { //Couldn't find a note, so find first active
						bool noteOk = false;
						int notesSearched = 0;
						randomNote = currentNote[channel]; 
						do {
							randomNote = (randomNote + 1) % actualScaleSize;
							notesSearched +=1;
							noteOk = notesSearched >= actualScaleSize;
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

					double quantitizedNoteCV = (reducedEfPitches[notePosition].pitch / 1200.0) + (key / 12.0); 
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

void ProbablyNoteMN::onReset() {
	clockTrigger.reset();

	for(int i=0;i<TRIGGER_DELAY_SAMPLES;i++) {
		triggerDelay[i] = 0.0f;
	}
	triggerDelayEnabled = false;
	
}




struct ProbablyNoteMNDisplay : TransparentWidget {
	ProbablyNoteMN *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteMNDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawKey(const DrawArgs &args, Vec pos, int key) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);

		if(key < 0)
			return;

		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "%s", module->noteNames[key]);
		              //fprintf(stderr, "%s\n", module->noteNames[key]);

		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawFactors(const DrawArgs &args, Vec pos, bool shifted) {
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
        nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
        

		if(shifted) 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xff));
		else
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
        for(int i=0;i<MAX_FACTORS;i++) {
    		nvgFontSize(args.vg, 11);
	        snprintf(text, sizeof(text), "%i", module->factors[i]);
            nvgText(args.vg, pos.x, pos.y+i*34.5, text, NULL);
			nvgFontSize(args.vg, 9);
            snprintf(text, sizeof(text), "%i", module->actualNSteps[i]);
            nvgText(args.vg, pos.x+82, pos.y+i*34.5, text, NULL);
            snprintf(text, sizeof(text), "%i", module->actualDSteps[i]);
            nvgText(args.vg, pos.x+164, pos.y+i*34.5, text, NULL);
        }
	}

    void drawPitchInfo(const DrawArgs &args, Vec pos) {

        for(uint64_t i=0;i<module->reducedEfPitches.size();i++) {
            float pitch = module->reducedEfPitches[i].pitch;
            float dissonance = module->reducedEfPitches[i].dissonance;
			bool inUse = module->reducedEfPitches[i].inUse;
			uint8_t opacity =  std::max(255.0f * module->noteProbability[0][i],70.0f);
			if(i == module->probabilityNote[0]) 
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			else
				nvgFillColor(args.vg, inUse ? nvgRGBA(0xff, 0xff, 0x00, opacity) : nvgRGBA(0xff, 0x00, 0x00, opacity));
            nvgBeginPath(args.vg);
              //fprintf(stderr, "i:%llu p:%f d:%f\n", i, pitch,dissonance);
            nvgRect(args.vg,(pitch/8.0)+pos.x,215.5,1,-74.0+std::min((dissonance*2.0),70.0));
            nvgFill(args.vg);
        }
	}


	void drawAlgorithm(const DrawArgs &args, Vec pos, int algorithn) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);


		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "%s", module->algorithms[algorithn]);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawNoteReduction(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "%s", module->noteReductionPatternName.c_str());
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawScale(const DrawArgs &args, Vec pos) {
		if(module->scaleMappingMode > 0) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			char text[128];
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			snprintf(text, sizeof(text), "%s", module->scaleNames[module->scale]);
			nvgText(args.vg, pos.x, pos.y, text, NULL);
		}
	}

	

	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawFactors(args, Vec(35,30), module->weightShift != 0);
        drawPitchInfo(args,Vec(270,156));
		drawKey(args, Vec(264,82), module->lastKey);
		drawAlgorithm(args, Vec(274,240), module->noteReductionAlgorithm);
		drawNoteReduction(args, Vec(274,284));
		drawScale(args, Vec(359,240));
	}
};

struct ProbablyNoteMNWidget : ModuleWidget {
	ProbablyNoteMNWidget(ProbablyNoteMN *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteMN.svg")));


		{
			ProbablyNoteMNDisplay *display = new ProbablyNoteMNDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(263, 345), module, ProbablyNoteMN::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(298, 345), module, ProbablyNoteMN::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(335, 345), module, ProbablyNoteMN::EXTERNAL_RANDOM_INPUT));

        addParam(createParam<RoundSmallFWKnob>(Vec(278,25), module, ProbablyNoteMN::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(304,51), module, ProbablyNoteMN::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(306, 29), module, ProbablyNoteMN::SPREAD_INPUT));

        addParam(createParam<RoundSmallFWKnob>(Vec(353,25), module, ProbablyNoteMN::SLANT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(379,51), module, ProbablyNoteMN::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(381, 29), module, ProbablyNoteMN::SLANT_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(423, 25), module, ProbablyNoteMN::DISTRIBUTION_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(449,51), module, ProbablyNoteMN::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(451, 29), module, ProbablyNoteMN::DISTRIBUTION_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(263,86), module, ProbablyNoteMN::KEY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(289,112), module, ProbablyNoteMN::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(291, 90), module, ProbablyNoteMN::KEY_INPUT));

		addParam(createParam<LEDButton>(Vec(270, 113), module, ProbablyNoteMN::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(271.5, 114.5), module, ProbablyNoteMN::KEY_LOGARITHMIC_SCALE_LIGHT));

        addParam(createParam<RoundSmallFWKnob>(Vec(323,86), module, ProbablyNoteMN::DISSONANCE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(349,112), module, ProbablyNoteMN::DISSONANCE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(351, 90), module, ProbablyNoteMN::DISSONANCE_INPUT));


		addParam(createParam<LEDButton>(Vec(272, 245), module, ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(273.5, 246.5), module, ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(298, 245), module, ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_INPUT));


        addParam(createParam<RoundSmallFWSnapKnob>(Vec(272,289), module, ProbablyNoteMN::NUMBER_OF_NOTES_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(298,312), module, ProbablyNoteMN::NUMBER_OF_NOTES_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(300, 292), module, ProbablyNoteMN::NUMBER_OF_NOTES_INPUT));


        addParam(createParam<RoundSmallFWSnapKnob>(Vec(363,245), module, ProbablyNoteMN::SCALE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(389,272), module, ProbablyNoteMN::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(391, 250), module, ProbablyNoteMN::SCALE_INPUT));

		addParam(createParam<LEDButton>(Vec(363, 295), module, ProbablyNoteMN::SCALE_MAPPING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(364.5, 296.5), module, ProbablyNoteMN::SCALE_MAPPING_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(389, 295), module, ProbablyNoteMN::SCALE_MAPPING_INPUT));

		addParam(createParam<LEDButton>(Vec(363, 315), module, ProbablyNoteMN::USE_SCALE_WEIGHTING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(364.5, 316.5), module, ProbablyNoteMN::SCALE_WEIGHTING_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(389, 315), module, ProbablyNoteMN::USE_SCALE_WEIGHTING_INPUT));


        addParam(createParam<RoundSmallFWSnapKnob>(Vec(443,86), module, ProbablyNoteMN::OCTAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(469,112), module, ProbablyNoteMN::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(471, 90), module, ProbablyNoteMN::OCTAVE_INPUT));

		addParam(createParam<LEDButton>(Vec(469, 143), module, ProbablyNoteMN::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(470.5, 144.5), module, ProbablyNoteMN::OCTAVE_WRAPAROUND_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(447, 144), module, ProbablyNoteMN::OCTAVE_WRAP_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(443,216), module, ProbablyNoteMN::PITCH_RANDOMNESS_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(469,242), module, ProbablyNoteMN::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(471, 220), module, ProbablyNoteMN::PITCH_RANDOMNESS_INPUT));
		addParam(createParam<LEDButton>(Vec(446, 250), module, ProbablyNoteMN::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(447.5, 251.5), module, ProbablyNoteMN::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));


		addParam(createParam<RoundReallySmallFWKnob>(Vec(457,292), module, ProbablyNoteMN::WEIGHT_SCALING_PARAM));	


        for(int i=0;i<MAX_FACTORS;i++) {	
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(10,i*34.5+33), module, ProbablyNoteMN::FACTOR_1_PARAM+i));
            addParam(createParam<RoundReallySmallFWKnob>(Vec(36,i*34.5+35), module, ProbablyNoteMN::FACTOR_1_CV_ATTENUVERTER_PARAM+i));
    		addInput(createInput<FWPortInSmall>(Vec(62, i*34.5+35), module, ProbablyNoteMN::FACTOR_1_INPUT+i));

            addParam(createParam<RoundSmallFWSnapKnob>(Vec(92,i*34.5+33), module, ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_PARAM+i));
            addParam(createParam<RoundReallySmallFWKnob>(Vec(118,i*34.5+35), module, ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM+i));
    		addInput(createInput<FWPortInSmall>(Vec(144, i*34.5+35), module, ProbablyNoteMN::FACTOR_NUMERATOR_STEP_1_INPUT+i));

            addParam(createParam<RoundSmallFWSnapKnob>(Vec(174,i*34.5+33), module, ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_PARAM+i));
            addParam(createParam<RoundReallySmallFWKnob>(Vec(200,i*34.5+35), module, ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM+i));
    		addInput(createInput<FWPortInSmall>(Vec(226, i*34.5+35), module, ProbablyNoteMN::FACTOR_DENOMINATOR_STEP_1_INPUT+i));
        }   


      

		addOutput(createOutput<FWPortInSmall>(Vec(460, 345),  module, ProbablyNoteMN::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(427, 345),  module, ProbablyNoteMN::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(389, 345),  module, ProbablyNoteMN::NOTE_CHANGE_OUTPUT));

	}

	struct TriggerDelayItem : MenuItem {
		ProbablyNoteMN *module;
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

		ProbablyNoteMN *module = dynamic_cast<ProbablyNoteMN*>(this->module);
		assert(module);

		TriggerDelayItem *triggerDelayItem = new TriggerDelayItem();
		triggerDelayItem->module = module;
		menu->addChild(triggerDelayItem);

	}
	
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteMN = createModel<ProbablyNoteMN, ProbablyNoteMNWidget>("ProbablyNoteMN");
    