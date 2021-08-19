#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"
#include "dsp-noise/noise.hpp"
#include "model/ChristoffelWords.hpp"

#include <sstream>
#include <iomanip>
#include <time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "osdialog.h"


#define POLYPHONY 16
#define MAX_PRIME_NUMBERS 49 //Up to 200 and some transcenttals
#define MAX_FACTORS 10
#define MAX_GENERATED_PITCHES 100000 //hard limit but to prevent getting silly
#define MAX_PITCHES 100000 //Not really a hard limit but to prevent getting silly
#define MAX_SCALES 42
#define MAX_NOTES 12
#define NUM_SHIFT_MODES 3
#define TRIGGER_DELAY_SAMPLES 5

#define NBR_ALGORITHMS 4
#define NBR_SCALE_MAPPING 4
#define NBR_TEMPERING_MODES 3
#define NBR_EDO_MODES 3
#define NBR_MOS_MODES 2

//GOLOMB RULER
#define NUM_RULERS 36
#define MAX_DIVISIONS 27
//PERFECT BALANCE
#define NUM_PB_PATTERNS 115
#define MAX_PB_SIZE 73

#define PARAM_CHANGE_SCALING 50 //only read every this many events



using namespace frozenwasteland::dsp;

struct EFPitch {
  int   pitchType;
  double numerator;
  double denominator;
  double tempering;
  double ratio;
  double pitch;
  std::string mondo;
  double dissonance;
  float weighting;
  float xPos;
  float yPos;
  bool inUse;
  bool operator<(const EFPitch& a) const {
    return pitch < a.pitch;
  }
};

struct EFactor {
	double factor;
	double tempering;
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
		FACTOR_TEMPER_1_STEP_PARAM = FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM + MAX_FACTORS,
		FACTOR_TEMPER_1_STEP_CV_ATTENUVERTER_PARAM = FACTOR_TEMPER_1_STEP_PARAM + MAX_FACTORS,
		OCTAVE_SIZE_PARAM = FACTOR_TEMPER_1_STEP_CV_ATTENUVERTER_PARAM + MAX_FACTORS,
		OCTAVE_SIZE_CV_ATTENUVERTER_PARAM,
		OCTAVE_SCALE_SIZE_MAPPING_PARAM,
		WEIGHT_SCALING_PARAM ,
		PITCH_RANDOMNESS_PARAM,
		PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM,
		PITCH_RANDOMNESS_GAUSSIAN_PARAM,
        NOTE_ACTIVE_PARAM,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_NOTES,
		NON_REPEATABILITY_PARAM = NOTE_WEIGHT_PARAM + MAX_NOTES,
		NON_REPEATABILITY_CV_ATTENUVERTER_PARAM,
		EQUAL_DIVISION_MODE_PARAM,
		EQUAL_DIVISION_PARAM,
		EQUAL_STEP_SIZE_PARAM,
		EQUAL_DIVISION_CV_ATTENUVERTER_PARAM,
		EDO_STEPS_PARAM,
		EDO_STEPS_CV_ATTENUVERTER_PARAM,
		EDO_WRAPS_PARAM,
		EDO_WRAPS_CV_ATTENUVERTER_PARAM,
		MOS_MODE_PARAM,
		MOS_LARGE_STEPS_PARAM,
		MOS_LARGE_STEPS_CV_ATTENUVERTER_PARAM,
		MOS_SMALL_STEPS_PARAM,
		MOS_SMALL_STEPS_CV_ATTENUVERTER_PARAM,
		MOS_RATIO_PARAM,
		MOS_RATIO_CV_ATTENUVERTER_PARAM,
		MOS_LEVELS_PARAM,
		MOS_LEVELS_CV_ATTENUVERTER_PARAM,
		EDO_TEMPERING_PARAM,
		EDO_TEMPERING_THRESHOLD_PARAM,
		EDO_TEMPERING_THRESHOLD_CV_ATTENUVERTER_PARAM,
		EDO_TEMPERING_STRENGTH_PARAM,
		EDO_TEMPERING_STRENGTH_CV_ATTENUVERTER_PARAM,
		SPREAD_MODE_PARAM,
		QUANTIZE_OCTAVE_PARAM,
		QUANTIZE_MOS_RATIO_PARAM,
		SET_ROOT_NOTE_PARAM,
		TRIGGER_MODE_PARAM,
		NUM_PARAMS
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
        FACTOR_TEMPER_STEP_1_INPUT = FACTOR_DENOMINATOR_STEP_1_INPUT + MAX_FACTORS,
		OCTAVE_SIZE_INPUT = FACTOR_TEMPER_STEP_1_INPUT + MAX_FACTORS,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
		OCTAVE_WRAP_INPUT,
		PITCH_RANDOMNESS_INPUT,
        NOTE_WEIGHT_INPUT,
		NON_REPEATABILITY_INPUT = NOTE_WEIGHT_INPUT + MAX_NOTES,
		EQUAL_DIVISION_MODE_INPUT,
		EQUAL_DIVISION_INPUT,
		EDO_STEPS_INPUT,
		EDO_WRAPS_INPUT,
		MOS_LARGE_STEPS_INPUT,
		MOS_SMALL_STEPS_INPUT,
		MOS_RATIO_INPUT,
		MOS_LEVELS_INPUT,
		EDO_TEMPERING_INPUT,
		EDO_TEMPERING_THRESHOLD_INPUT,
		EDO_TEMPERING_STRENGTH_INPUT,
		SET_ROOT_NOTE_INPUT,
		NUM_INPUTS
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
        OCTAVE_WRAPAROUND_LIGHT = DISTRIBUTION_GAUSSIAN_LIGHT + 3,
		OCTAVE_SCALE_SIZE_MAPPING_LIGHT = OCTAVE_WRAPAROUND_LIGHT + 3,
		SHIFT_MODE_LIGHT = OCTAVE_SCALE_SIZE_MAPPING_LIGHT + 3,
        KEY_LOGARITHMIC_SCALE_LIGHT = SHIFT_MODE_LIGHT + 3,
		PITCH_RANDOMNESS_GAUSSIAN_LIGHT = KEY_LOGARITHMIC_SCALE_LIGHT + 3,
		SPREAD_MODE_LIGHT = PITCH_RANDOMNESS_GAUSSIAN_LIGHT + 3,
		EDO_MODE_LIGHT = SPREAD_MODE_LIGHT + 3,
		MOS_MODE_LIGHT = EDO_MODE_LIGHT + 3,
		EDO_TEMPERING_LIGHT = MOS_MODE_LIGHT + 3,
		QUANTIZE_OCTAVE_SIZE_LIGHT = EDO_TEMPERING_LIGHT + 3,
		QUANTIZE_MOS_RATIO_LIGHT = QUANTIZE_OCTAVE_SIZE_LIGHT + 3,
		TRIGGER_POLYPHONIC_LIGHT = QUANTIZE_MOS_RATIO_LIGHT + 3,
		NUM_LIGHTS = TRIGGER_POLYPHONIC_LIGHT + 3
	};
	enum QuantizeModes {
		QUANTIZE_CLOSEST,
		QUANTIZE_LOWER,
		QUANTIZE_UPPER,
	};
	enum EqualDivisionModes {
		EQUAL_DIVISION_EDES_MODE,
		EQUAL_STEP_WITH_COMMA_EDES_MODE,
		EQUAL_STEP_SET_OCTAVE_SIZE_EDES_MODE
	};
	enum PitchTypes {
		RATIO_PITCH_TYPE,
		EQUAL_DIVISION_PITCH_TYPE,
		EQUAL_STEP_SIZE_PITCH_TYPE,
		MOS_PITCH_TYPE,
	};
	enum Algorithms {
		NO_REDUCTION_ALGO,
		EUCLIDEAN_ALGO,
		GOLUMB_RULER_ALGO,
		PERFECT_BALANCE_ALGO,
	};
	enum ScaleMapping {
		NO_SCALE_MAPPING,
		SPREAD_SCALE_MAPPING,
		REPEAT_SCALE_MAPPING,
		NEAREST_NEIGHBOR_SCALE_MAPPING
	};
	enum TemperingMpdes {
		NO_TEMPERING,
		KEEP_BEST_MATCH_TEMPERING,
		TEMPER_ALL_PITCHES_TEMPERING,
		TEMPER_BEST_MATCH_TEMPERING
	};

	const char* edoTemperingModes[NBR_TEMPERING_MODES] = {"Best Fit","Temper All"};
	const char* algorithms[NBR_ALGORITHMS] = {"","Euclidean","Golumb Ruler","Perfect Balance"};
	const char* scaleMappings[NBR_SCALE_MAPPING] = {"Spread","Repeat","Nearest Neighbor"};

// “”
	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
    const double primeNumbers[MAX_PRIME_NUMBERS] = {(1 + std::sqrt(5))/2.0,M_E,M_PI,2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199};
    const std::string primeNumberNames[MAX_PRIME_NUMBERS] = {"ɸ","e","π","2","3","5","7","11","13","17","19","23","29","31","37","41","43","47","53","59","61","67","71","73","79","83","89","97","101","103","107","109","113","127","131","137","139","149","151","157","163","167","173","179","181","191","193","197","199"};


	const float majorRatios[28] = {203.91,386.61,498.04,701.955,884.36,1088.27,1200.0,1403.91,1586.61,1698.04,1901.955,2084.36,2288.27,2400.0,2603.91,2786.61,2898.04,3001.955,3284.36,3488.27,3600,3803.91,3986.61,4098.04,4301.955,4484.36,4688.27,4800};
	const float minorRatios[28] = {203.91,315.64,498.04,701.955,813.69,1017.596,1200.0,1403.91,1515.64,1698.04,1901.955,2013.69,2217.596,2400.0,2603.91,2715.64,2898.04,3001.955,3213.69,3417.596,3600,3803.91,3915.64,4098.04,4301.955,4413.69,4617.596,4800};

	//GOLOMB RULER PATTERNS
    const int rulerOrders[NUM_RULERS] = {1,2,3,4,5,5,6,6,6,6,7,7,7,7,7,8,9,10,11,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
	const uint64_t rulerLengths[NUM_RULERS] = {0,1,3,6,11,11,17,17,17,17,25,25,25,25,25,34,44,55,72,72,85,106,127,151,177,199,216,246,283,333,356,372,425,480,492,553};
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
	const uint64_t pbPatternLengths[NUM_PB_PATTERNS] = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,29,30,30,30,30,30,30,31,37,41,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,47,53,59,61,67,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,70,71,73};
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
	
	dsp::SchmittTrigger clockTrigger[POLYPHONY],clockModeTrigger,algorithmTrigger,edoModeTrigger,mosModeTrigger,edoTemperingTrigger,scaleMappingTrigger,useScaleWeightingTrigger,octaveScaleMappingTrigger,
						octaveWrapAroundTrigger,shiftScalingTrigger,keyScalingTrigger,pitchRandomnessGaussianTrigger,spreadModeTrigger, 
						quantizeOctaveSizeTrigger,quantizeMosRatioTrigger,setRootNoteTrigger; 
	dsp::PulseGenerator noteChangePulse[POLYPHONY];
    GaussianNoiseGenerator _gauss;
 
    bool octaveWrapAround = false;
	bool triggerPolyphonic = false;
    //bool noteActive[MAX_NOTES] = {false};
    float noteScaleProbability[MAX_NOTES] = {0.0f};
    float noteProbability[POLYPHONY][MAX_PITCHES] = {{0.0f}};
    float currentScaleNoteWeighting[MAX_NOTES] = {0.0f};
	bool currentScaleNoteStatus[MAX_NOTES] = {false};
	float actualProbability[POLYPHONY][MAX_PITCHES] = {{0.0f}};
	int controlIndex[MAX_NOTES] = {0};

	int equalDivisionMode = 0; //0 = equal divisions, 1=equal steps within octave, 2 = equal steps with octave adjust 
	int equalDivisions = 0;
	int lastEqualDivisions = 0;
	int equalDivisionSteps = 1;
	int lastEqualDivisionSteps = 1;
	int equalDivisionWraps = 1;
	int lastEqualDivisionWraps = 1;
	int lastEqualDivisionMode = -1;
	float equalStepSize = 0;
	float lastEqualStepSize = 0;
	
	int mosMode = 0; //0 = MOS by christoff words, 1=MOS by prime factors 
	int mosLargeSteps = 0;
	int lastMosLargeSteps = 0;
	int mosSmallSteps = 0;
	int lastMosSmallSteps = 0;
	float mosRatio = 1;
	float lastMosRatio = 1;
	int mosLevels = 1;
	int lastMosLevels = 1;
	bool quantizeMosRatio = true;
	bool invalidMos = false;

	ChristoffelWords christoffelWords;
	std::string currentChristoffelword = {"unknown"};


	float factors[MAX_FACTORS] = {0};
    float lastFactors[MAX_FACTORS] = {0};
	float factorsTempering[MAX_FACTORS] = {0};
    float lastfactorsTempering[MAX_FACTORS] = {0};
	std::string factorNames[MAX_FACTORS] = {""};
    uint8_t stepsN[MAX_FACTORS] = {0};
    uint8_t lastNSteps[MAX_FACTORS] = {0};
    uint8_t actualNSteps[MAX_FACTORS] = {0};
    uint8_t stepsD[MAX_FACTORS] = {0};
    uint8_t lastDSteps[MAX_FACTORS] = {0};
    uint8_t actualDSteps[MAX_FACTORS] = {0};

	int edoTempering = 0;
	int lastEdoTempering = -1;
	float edoTemperingThreshold = 0;
	float lastEdoTemperingThreshold = 0;
	float edoTemperingStrength = 0;
	float lastEdoTemperingStrength = 0;

	bool reloadPitches = false;

	bool triggerDelayEnabled = false;
	float triggerDelay[POLYPHONY][TRIGGER_DELAY_SAMPLES] = {{0}};
	int triggerDelayIndex[POLYPHONY] = {0};

	int pitchGridDisplayMode = 1;

    std::vector<EFPitch> efPitches;
    std::vector<EFPitch> temperingPitches;
    std::vector<EFPitch> resultingPitches;
	std::vector<EFactor> numeratorList;
	std::vector<EFactor> denominatorList;
	std::vector<EFPitch> reducedEfPitches;
	std::vector<uint8_t> pitchIncluded;
	
	uint64_t nbrGeneratedPitches = 0;
	uint64_t lastNbrGeneratedPitches = 0;
	uint64_t nbrReducedPitches = 0;
	uint64_t nbrResultPitches = 0;
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

	bool checkTemperingNeeded = false;
	bool mapPitches = false;

	float octaveSize = 2;;
	float lastOctaveSize = -1;
	float octaveScaleConstant = 1.0;
	bool quantizeOctaveSize = true;
	bool octaveScaleMapping;
	bool spreadMode = false;

    int scale = 0;
    int lastScale = -1;
    int key = 0;
	int lastKey = 0;
	int octave = 0;
    float spread = 0;
	float upperSpread = 0.0;
	float lowerSpread = 0.0;
	float slant = 0;
	float focus = 0; 
	float dissonanceProbability = 0; 
	float randomRange = 0;
	float nonRepeat = 0;
	int lastRandomNote[POLYPHONY] = {-1}; 
	int currentNote[POLYPHONY] = {0};
	uint64_t probabilityNote[POLYPHONY] = {0};
	double lastFractionalValue[POLYPHONY] = {0.0};
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

	int quantizeMode = QUANTIZE_CLOSEST;

	float modulationRoot = 0;
	int microtonalKey = 0;
	bool getModulationRoot = false;

	int currentPolyphony = 1;

	int paramChangeCounter = 0;

	//percentages
	float spreadPercentage = 0;
	float slantPercentage = 0;
	float distributionPercentage = 0;
	float nonRepeatPercentage = 0;
	float keyPercentage = 0;
	float dissonancePercentage = 0;
	float shiftPercentage = 0;
	float octavePercentage = 0;
	float pitchRandomnessPercentage = 0;

	float octaveSizePercentage = 0;

	float mosLargeStepsPercentage = 0;
	float mosSmallStepsPercentage = 0;
	float mosRatioPercentage = 0;
	float mosLevelsPercentage = 0;

	float edoPercentage = 0;
	float edoStepsPercentage = 0;
	float edoWrapsPercentage = 0;

	float edoTemperingThresholdPercentage = 0;
	float edoTemperingStrengthPercentage = 0;

	float factorsPercentage[MAX_FACTORS] = {0};
	float factorsNumeratorPercentage[MAX_FACTORS] = {0};
	float factorsDivisorPercentage[MAX_FACTORS] = {0};
	float factorsTemperingPercentage[MAX_FACTORS] = {0};
	

	float numberOfNotesPercentage = 0;
	float scalePercentage = 0;

	
	ProbablyNoteMN() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteMN::SPREAD_PARAM, 0.0, 10.0, 0.0,"Spread");
        configParam(ProbablyNoteMN::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteMN::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant");
        configParam(ProbablyNoteMN::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteMN::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		configParam(ProbablyNoteMN::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Distribution CV Attenuation");
		configParam(ProbablyNoteMN::NON_REPEATABILITY_PARAM, 0.0, 1.0, 0.0,"Non Repeat Probability"," %",0,100);
        configParam(ProbablyNoteMN::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Non Repeat Probability CV Attenuation","%",0,100);

		configParam(ProbablyNoteMN::SHIFT_PARAM, -12.0, 12.0, 0.0,"Weight Shift");
        configParam(ProbablyNoteMN::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteMN::KEY_PARAM, 0.0, 10.0, 0.0,"Key");
        configParam(ProbablyNoteMN::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNoteMN::NUMBER_OF_NOTES_PARAM, 1.0, 115.0, 12.0,"Max # of Notes in Scale");
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


		configParam(ProbablyNoteMN::EQUAL_DIVISION_PARAM, 0.0, 300.0, 0.0,"# of Equal Divisions");
		configParam(ProbablyNoteMN::EQUAL_STEP_SIZE_PARAM, 1.0, 300.0, 50.0,"Equal Step Size"," cents");
        configParam(ProbablyNoteMN::EQUAL_DIVISION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Equal Divisions CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::EDO_STEPS_PARAM, 1.0, 100.0, 1.0,"# of Equal Division Steps");
        configParam(ProbablyNoteMN::EDO_STEPS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Equal Division Steps CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::EDO_WRAPS_PARAM, 1.0, 100.0, 1.0,"# of Equal Division Octave Wraps");
        configParam(ProbablyNoteMN::EDO_WRAPS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Equal Division Octave Wraps CV Attenuation","%",0,100);

		configParam(ProbablyNoteMN::MOS_LARGE_STEPS_PARAM, 0.0, 20.0, 0.0,"# of MOS Large Steps");
        configParam(ProbablyNoteMN::MOS_LARGE_STEPS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"MOS Large Steps CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::MOS_SMALL_STEPS_PARAM, 0.0, 20.0, 0.0,"# of MOS Small Steps");
        configParam(ProbablyNoteMN::MOS_SMALL_STEPS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"MOS Small Steps CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::MOS_RATIO_PARAM, 1.0, 5.0, 2.0,"MOS L/s Ratio");
        configParam(ProbablyNoteMN::MOS_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"MOS L/s Ratio CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::MOS_LEVELS_PARAM, 1.0, 3.0, 1.0,"MOS Levels Steps");
        configParam(ProbablyNoteMN::MOS_LEVELS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"MOS Levels CV Attenuation","%",0,100);


		configParam(ProbablyNoteMN::EDO_TEMPERING_PARAM, 0.0, 1.0, 0.0,"EOD Tempering");
		configParam(ProbablyNoteMN::EDO_TEMPERING_THRESHOLD_PARAM, 0.0, 1.0, 1.0,"Temperming Threshold","%",0,100);
        configParam(ProbablyNoteMN::EDO_TEMPERING_THRESHOLD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Tempering Threshold CV Attenuation","%",0,100);
		configParam(ProbablyNoteMN::EDO_TEMPERING_STRENGTH_PARAM, 0.0, 1.0, 1.0,"Tempering Strength","%",0,100);
        configParam(ProbablyNoteMN::EDO_TEMPERING_STRENGTH_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Tempering Strength CV Attenuation","%",0,100);


        srand(time(NULL));

        for(int i=0;i<MAX_FACTORS;i++) {
            configParam(ProbablyNoteMN::FACTOR_1_PARAM + i, i, MAX_PRIME_NUMBERS-MAX_FACTORS+i, i+3,"Factor");		
            configParam(ProbablyNoteMN::FACTOR_1_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Factor CV Attenuverter","%",0,100);		
            configParam(ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_PARAM + i, 0.0, 10.0, 0.0,"Numerator Step Count");		
            configParam(ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Numerator Step Count CV Attenuverter","%",0,100);		
            configParam(ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_PARAM + i, 0.0, 5.0, 0.0,"Denominator Step Count");		
            configParam(ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Denominator Step Count CV Attenuverter","%",0,100);		
            configParam(ProbablyNoteMN::FACTOR_TEMPER_1_STEP_PARAM + i, -50.0, 50.0, 0.0,"Factor Tempering"," Cents");		
            configParam(ProbablyNoteMN::FACTOR_TEMPER_1_STEP_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Factor Tempering CV Attenuverter","%",0,100);		
        }

        configParam(ProbablyNoteMN::OCTAVE_SIZE_PARAM, 2.0, 5.0, 2.0,"Octave Size");
		configParam(ProbablyNoteMN::OCTAVE_SIZE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave Size CV Attenuation");


		configParam(ProbablyNoteMN::SPREAD_MODE_PARAM, 0.0, 1.0, 0.0,"Spread by %");
		configParam(ProbablyNoteMN::OCTAVE_WRAPAROUND_PARAM, 0.0, 1.0, 0.0,"Wrap generated notes beyond octave");
		configParam(ProbablyNoteMN::OCTAVE_SCALE_SIZE_MAPPING_PARAM, 0.0, 1.0, 0.0,"Map v/O to Scale's Octave Size");
		configParam(ProbablyNoteMN::QUANTIZE_OCTAVE_PARAM, 0.0, 1.0, 0.0,"Quantitize Octave Size");
		configParam(ProbablyNoteMN::QUANTIZE_MOS_RATIO_PARAM, 0.0, 1.0, 0.0,"Quantitize MOS Ratio");
		configParam(ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_PARAM, 0.0, 1.0, 0.0,"Note Reduction Algorithm");
		configParam(ProbablyNoteMN::SCALE_MAPPING_PARAM, 0.0, 1.0, 0.0,"Map Generated Scale to Traditional Scale");
		configParam(ProbablyNoteMN::USE_SCALE_WEIGHTING_PARAM, 0.0, 1.0, 0.0,"Use Mapped Scale's Probability");

		onReset();
	}

	void reConfigParam (int paramId, float minValue, float maxValue, float defaultValue, std::string unit,float displayBase, float displayMultiplier) {
		ParamQuantity *pq = paramQuantities[paramId];
		pq->minValue = minValue;
		pq->maxValue = maxValue;
		pq->defaultValue = defaultValue;
		pq->unit = unit;		
		pq->displayBase = displayBase;		
		pq->displayMultiplier = displayMultiplier;		
	}

    double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,float scaling,float randomIn) {
        double weightTotal = 0.0f;
		double linearWeight, logWeight, weight;
            
        for(uint64_t i = 0;i <actualScaleSize; i++) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

        int chosenWeight = -1;        
        double rnd = randomIn * weightTotal;
        for(uint64_t i = 0;i <actualScaleSize;i++ ) {
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
		temperingPitches.clear();
		numeratorList.clear();
		denominatorList.clear();
        efPitches.resize(0);
		numeratorList.resize(0);
		denominatorList.resize(0);

        EFPitch efPitch;
		efPitch.pitchType = EQUAL_DIVISION_PITCH_TYPE;
        efPitch.numerator = 1;
        efPitch.denominator = 1;
		efPitch.ratio = 1;
		efPitch.pitch = 0.0;
        efPitches.push_back(efPitch);
		temperingPitches.push_back(efPitch);

        EFPitch octaveEfPitch;
		octaveEfPitch.pitchType = EQUAL_DIVISION_PITCH_TYPE;
        octaveEfPitch.numerator = 2;
        octaveEfPitch.denominator = 1;
		octaveEfPitch.ratio = 2;
		octaveEfPitch.pitch = 1200.0;
		temperingPitches.push_back(octaveEfPitch);

		EFactor baseFactor;
		baseFactor.factor = 1;
		baseFactor.tempering = 0;

		numeratorList.push_back(baseFactor);
		denominatorList.push_back(baseFactor);

		//Do Equal Divisions First

		int eodCount = clamp(equalDivisionWraps * equalDivisions / equalDivisionSteps,0,equalDivisions);
		float stepCount = 1200.0 / equalStepSize; // this should become a float
		uint16_t divisionCount = 0;
		switch(equalDivisionMode) {
			case EQUAL_DIVISION_EDES_MODE :
			for(uint16_t divisionIndex = 0;divisionIndex<eodCount;divisionIndex++) {
				if(divisionIndex > 0) {
					EFPitch efPitch;
					efPitch.pitchType = EQUAL_DIVISION_PITCH_TYPE;
					double numerator = divisionCount;
					double denominator = equalDivisions;        
					double ratio = numerator / denominator;
					efPitch.ratio = pow(2,ratio);
					if(IsUniqueRatio(efPitch.ratio)) {
						double gcd = GCD(numerator,denominator);
						efPitch.numerator = numerator / gcd;
						efPitch.denominator = denominator / gcd;
						double pitchInCents = 1200 * ratio; 
						efPitch.pitch = pitchInCents;
						if(edoTempering > 0)
							temperingPitches.push_back(efPitch);
						else
							efPitches.push_back(efPitch);
					}
				}
				divisionCount = (divisionCount + equalDivisionSteps) % equalDivisions;
			}
			break;
			case EQUAL_STEP_WITH_COMMA_EDES_MODE :
				for(uint16_t stepIndex = 0;stepIndex<stepCount;stepIndex++) {
					if(stepIndex > 0) {
						EFPitch efPitch;
						efPitch.pitchType = EQUAL_STEP_SIZE_PITCH_TYPE;
						double numerator = stepIndex*equalStepSize;
						double denominator = 1200;        
						efPitch.ratio = 1200/equalStepSize*float(stepIndex);
						if(IsUniqueRatio(efPitch.ratio)) {
							double gcd = GCD(numerator,denominator);
							efPitch.numerator = numerator / gcd;
							efPitch.denominator = denominator / gcd;
							efPitch.pitch = equalStepSize*float(stepIndex);
							if(edoTempering > 0)
								temperingPitches.push_back(efPitch);
							else
								efPitches.push_back(efPitch);
						}
					}
				}
			break;
			case EQUAL_STEP_SET_OCTAVE_SIZE_EDES_MODE :
				for(uint16_t stepIndex = 0;stepIndex<stepCount;stepIndex++) {
					if(stepIndex > 0) {
						EFPitch efPitch;
						efPitch.pitchType = EQUAL_STEP_SIZE_PITCH_TYPE;
						double numerator = stepIndex*equalStepSize;
						double denominator = 1200;        
						efPitch.ratio = 1200/equalStepSize*float(stepIndex);
						if(IsUniqueRatio(efPitch.ratio)) {
							double gcd = GCD(numerator,denominator);
							efPitch.numerator = numerator / gcd;
							efPitch.denominator = denominator / gcd;
							efPitch.pitch = equalStepSize*float(stepIndex);
							if(edoTempering > 0)
								temperingPitches.push_back(efPitch);
							else
								efPitches.push_back(efPitch);
						}
					}
				}
			break;
		}
		std::sort(temperingPitches.begin(), temperingPitches.end());

		//Now do MoS
		std::string word = "unknown";
		int numberLargeSteps = mosLargeSteps;
		int numberSmallSteps = mosSmallSteps;
		int totalSteps = 0;
		double currentRatio = mosRatio * mosLevels; //Ensure that our ratios don't go to infinity
		for(int l = 0;l<mosLevels;l++) {
			totalSteps = numberLargeSteps + numberSmallSteps;
			word = christoffelWords.Generate(totalSteps,numberSmallSteps);
			if(word == "unknown") {
				break;
			}
					// fprintf(stderr, "word:%s l:%i  s:%i  r:%f\n", word.c_str(),numberLargeSteps,numberSmallSteps,currentRatio);

			if(l < mosLevels - 1.0) { // don't calculate this for last level
				if(currentRatio <= 2) {
					numberSmallSteps = numberLargeSteps;
					numberLargeSteps = totalSteps;
					currentRatio = 1.0 / (currentRatio - 1.0);
				} else {
					numberSmallSteps = totalSteps;
					currentRatio = currentRatio - 1.0;
				}
			}
		}

		if(word != "unknown") {
			invalidMos = false;
			currentChristoffelword = word;
			float totalSize = numberSmallSteps + (numberLargeSteps * currentRatio);
			float currentPosition = 0;
			for(int wfPitchIndex=0;wfPitchIndex<totalSteps;wfPitchIndex++) { 
				char beatType = currentChristoffelword[wfPitchIndex];
				currentPosition +=  beatType == 's' ? 1.0 : currentRatio;

				EFPitch efPitch;
				efPitch.pitchType = MOS_PITCH_TYPE;
				double numerator = currentPosition;
				double denominator = totalSize;        
				double ratio = numerator / denominator;
				efPitch.ratio = pow(2,ratio);
				if(IsUniqueRatio(efPitch.ratio)) {
					double gcd = GCD(numerator,denominator); 
					efPitch.numerator = numerator / gcd;
					efPitch.denominator = denominator / gcd;
					double pitchInCents = 1200 * ratio; 
					efPitch.pitch = pitchInCents;
					efPitches.push_back(efPitch);
				}
			}
		} else {
			invalidMos = true;
		}



        //Check that we aren't getting too crazy - Simplifying out for now 
		uint64_t numeratorCount = 1;
		uint64_t denominatorCount = 1;

        for(uint8_t f = 0; f< MAX_FACTORS;f++) {
			numeratorCount *= (stepsN[f]+1);
			denominatorCount *= (stepsD[f]+1);

			if(numeratorCount * denominatorCount < MAX_GENERATED_PITCHES) {
				actualNSteps[f] = stepsN[f];
				actualDSteps[f] = stepsD[f]; 
			} else {
				actualNSteps[f] = 0;
				actualDSteps[f] = 0; 
			}
        }
 
        //Calculate Numerators (basically all factors multiplied by each other)
		for (uint8_t fN = 0; fN < MAX_FACTORS; fN++) {
			uint16_t n = numeratorList.size();
			//fprintf(stderr, "f: %i n: %i  \n", f, n);
			for (int s = 1; s <= actualNSteps[fN]; s++) {
				for (int i = 0; i < n; i++) {
					float numerator = (numeratorList[i].factor * std::pow(factors[fN], s));
					//fprintf(stderr, "n: %f \n", numerator);
					if(numerator > 0.0) {
						EFactor numeratorFactor;
						numeratorFactor.factor = numerator;
						numeratorFactor.tempering = numeratorList[i].tempering + factorsTempering[fN] * s; 
						numeratorList.push_back(numeratorFactor);
					}
				}
			}
		}

        //Calculate Denominators (basically all factors multiplied by each other)
		for (uint8_t fD = 0; fD < MAX_FACTORS; fD++) {
			uint16_t n = denominatorList.size();
			//fprintf(stderr, "f: %i n: %i  \n", f, n);
			for (int s = 1; s <= actualDSteps[fD]; s++) {
				for (int i = 0; i < n; i++) {
					float denominator = (denominatorList[i].factor * std::pow(factors[fD], s));
					//fprintf(stderr, "d: %f \n", denominator);
					if(denominator > 0.0) {
						EFactor denominatorFactor;
						denominatorFactor.factor = denominator;
						denominatorFactor.tempering = denominatorList[i].tempering + factorsTempering[fD] * s; 
						denominatorList.push_back(denominatorFactor);
					}
				}
			}
		}

        for (uint16_t d = 1; d < denominatorList.size(); d++) {
			for (uint16_t n = 1; n < numeratorList.size(); n++) {
				EFPitch efPitch;
				EFactor numerator = numeratorList[n];
				EFactor denominator = ScaleDenominator(numerator,denominatorList[d]);        
				numerator = ScaleNumerator(numerator,denominator);
				if(numerator.factor <= float(ULLONG_MAX) && denominator.factor <= float(ULLONG_MAX)) {		
					efPitch.pitchType = RATIO_PITCH_TYPE;
					efPitch.numerator = numerator.factor;
					efPitch.denominator = denominator.factor;        
					double ratio = (numerator.factor / denominator.factor) ;
					efPitch.ratio = ratio;
					if(numerator.factor > 0 && IsUniqueRatio(ratio)) {
					// fprintf(stderr, "n:%f   d:%f   nt:%f    dt:%f \n",numerator.factor,denominator.factor, numerator.tempering, denominator.tempering);
						double gcd = GCD(efPitch.numerator,efPitch.denominator);
						efPitch.numerator = efPitch.numerator / gcd;
						efPitch.denominator = efPitch.denominator / gcd;
						efPitch.tempering = numerator.tempering + denominator.tempering;
						double pitchInCents = 1200 * std::log2f(ratio) + (numerator.tempering + denominator.tempering);
						efPitch.pitch = pitchInCents;
						efPitches.push_back(efPitch);
					}
				}
			}
		}
		
        nbrGeneratedPitches = efPitches.size(); //Actual Number of pitches
        std::sort(efPitches.begin(), efPitches.end());
        //fprintf(stderr, "Number of Pitches: %llu \n",nbrPitches);
    }

	bool IsUniqueRatio(double ratio, bool useTemperedPitches = false) {
		bool isUnique = true;
		const double epsilon = 1e-06;
		size_t pc = useTemperedPitches ? resultingPitches.size() : efPitches.size();
		for(uint64_t i = 0;i<pc;i++) {
			if((!useTemperedPitches && std::abs(efPitches[i].ratio - ratio) < epsilon) || ((useTemperedPitches && std::abs(resultingPitches[i].ratio - ratio) < epsilon))) {
				isUnique = false;
				break;
			}
		}
		return isUnique;
	}
    

    EFactor ScaleDenominator(EFactor numerator,EFactor denominator)
    {
		float newDenominator = denominator.factor;
		float newTempering = denominator.tempering;
        while (numerator.factor/newDenominator > 2.0)
        {
            newDenominator = newDenominator * 2.0;
			// newTempering = newTempering * 2.0;
        }

		EFactor newEFactor;
		newEFactor.factor = newDenominator;
		newEFactor.tempering = newTempering;

        return newEFactor;

    }

	EFactor ScaleNumerator(EFactor numerator,EFactor denominator)
    {
		float newNumerator = numerator.factor;
		float newTempering = numerator.tempering;
        while (newNumerator/denominator.factor < 1.0)
        {
            newNumerator = newNumerator * 2.0;
			// newTempering = newTempering * 2.0;
			//fprintf(stderr, "scaling: %f \n", newNumerator);
        }

		EFactor newEFactor;
		newEFactor.factor = newNumerator;
		newEFactor.tempering = newTempering;

        return newEFactor;
    }

    // float CalculateDissonance(float numerator, float denominator, float gcd)
    // {
    //     float lcm = LCM(numerator, denominator, gcd);
    //     return std::log2f(lcm);
    // }

	double CalculateDissonance(double ratio) {
		//Roughness Calculation Model  (from Vassilakis, 2001 & 2005)
		// will use middle A for calc

		int nbrPartials = 6;

		double dstar= 0.24;  // this is the point of maximum dissonance - the value is derived from a model
							// of the Plomp-Levelt dissonance  curves for all frequencies.
		double s1 = 0.0207;// s1 and s2 are used to allow a single functional form to interpolate beween
		double s2 = 18.96;  //  the various P&L curves of different frequencies by sliding, stretching/compressing
		       				//  the curve so that its max dissonance occurse at dstar. A least-square-fit was made
							// to determine the values.
		
		double c1 = 5;       // these parameters have values to fit the experimental data of Plomp and Levelt
		double c2 = -5;
		double b1 = -3.51; // theses values determine the rates at which the function rises and falls and 
		double b2 = -5.75; // and are based on a gradient minimisation of the squared error between 
						  //  Plomp and Levelt's averaged data and the curve
		
		double rootFrequency = 440;
		double d = 0;
		for(int minPartial = 1;minPartial<=nbrPartials;minPartial++) {
			double fRootPartial = rootFrequency * minPartial; // Middle A
			for(int freqPartial = 1;freqPartial<=nbrPartials;freqPartial++) {
				double interval = ratio * rootFrequency * freqPartial;

				double fMin = std::min(fRootPartial,interval);

				double s=dstar/(s1*fMin+s2); // define s with interpolating values s1 and s2.
											
				double fdif=fabs(interval-fMin); // (fabs gives absolute value)
												//	establishes the frequency difference					
									
				double arg1=b1*s*fdif; // gives arg1/arg2 the powers for e [i.e., as(f2-f1)] 
				double arg2=b2*s*fdif ; // in the equation for dissonance given above.																										
				double exp1=std::exp(arg1); // EXP returns a value containing e
										//(the base of natural logarithms) raised to the specified power.											
				double exp2=std::exp(arg2);
											
				double dNew = (c1*exp1+c2*exp2);//  The lesser of amp(i) and amp(j) is used
				d+=dNew;
			}	
		}					


// fprintf(stderr, "DISSONANCE ratio:%f   d:%f \n",ratio,  d);

		return d / nbrPartials;
	}


    //least common multiple
    inline double LCM(double a, double b,double gcd)
    {
        return (a * b) / gcd;
    }

    //Greatest common divisor
    double GCD(double a, double b)
    {
        if (b == 0)
            return a;
        return GCD(b, std::fmod(a,b));
    }

	void ReduceNotes(uint64_t originalSize, uint64_t reducedSize) {
		pitchIncluded.clear();
		pitchIncluded.resize(originalSize);
		if(originalSize == 0 || reducedSize == 0) 
			return;
		for(uint64_t i=0;i<originalSize;i++) {
			pitchIncluded[i] = 0;
		}
		
		switch(noteReductionAlgorithm) {
			case NO_REDUCTION_ALGO :
				HardLimitAlgo(originalSize,reducedSize);
				break;
			case EUCLIDEAN_ALGO :
				EuclideanAlgo(originalSize,reducedSize);
				break;
			case GOLUMB_RULER_ALGO :
				GolumbRulerAlgo(originalSize,reducedSize);
				break;
			case PERFECT_BALANCE_ALGO :
				PerfectBalanceAlgo(originalSize,reducedSize);
				break;
		}
	}

	void HardLimitAlgo(uint64_t originalSize, uint64_t reducedSize) {
		for(uint64_t noLimitStepIndex = 0; noLimitStepIndex < originalSize; noLimitStepIndex++)
		{
			pitchIncluded[noLimitStepIndex] = 1;	
		}
		noteReductionPatternName = std::to_string(originalSize) + " No Reduce";
	}

	void EuclideanAlgo(uint64_t originalSize, uint64_t reducedSize) {
		uint64_t bucket = originalSize - 1;                    
		for(uint64_t euclideanStepIndex = 0; euclideanStepIndex < originalSize; euclideanStepIndex++)
		{
			bucket += reducedSize;
			if(bucket >= originalSize) {
				bucket -= originalSize;
				pitchIncluded[euclideanStepIndex] = 1;	
			}                        
		}
		noteReductionPatternName = std::to_string(originalSize) + " to " + std::to_string(reducedSize);
	}

	void GolumbRulerAlgo(uint64_t originalSize, uint64_t reducedSize) {
		int rulerToUse = clamp(reducedSize-1,0,NUM_RULERS-1);
		while(rulerLengths[rulerToUse] + 1 > originalSize && rulerToUse >= 0) {
			rulerToUse -=1;
		}

		noteReductionPatternName = std::to_string(originalSize) + " to " + rulerNames[rulerToUse]; 
		
		//Multiply beats so that low division beats fill out entire pattern
		float spaceMultiplier = (originalSize / (rulerLengths[rulerToUse] + 1));

		for (int rulerIndex = 0; rulerIndex < rulerOrders[rulerToUse];rulerIndex++)
		{
			uint64_t noteLocation = (rulers[rulerToUse][rulerIndex] * spaceMultiplier);
			pitchIncluded[noteLocation] = 1;
		}
	}

	void PerfectBalanceAlgo(uint64_t originalSize, uint64_t reducedSize) {
		int pbPatternToUse = -1;
		uint64_t pbMatchPatternCount = 0;
		int pbLastMatchedPattern = 0;
		bool patternFound = false;
		while(!patternFound) {
			pbPatternToUse +=1;
			if(pbPatternToUse >= NUM_PB_PATTERNS)
				break;
			if(pbPatternLengths[pbPatternToUse] <= originalSize && originalSize % pbPatternLengths[pbPatternToUse] == 0) {
				pbLastMatchedPattern = pbPatternToUse;
				if(pbMatchPatternCount >= reducedSize-1) {
					patternFound = true;
				} else {
					pbMatchPatternCount +=1;							
				}
			}
		}
		if(!patternFound)
			pbPatternToUse = pbLastMatchedPattern;
		
		//Multiply beats so that low division beats fill out entire pattern
		float spaceMultiplier = (originalSize / (pbPatternLengths[pbPatternToUse]));
		noteReductionPatternName =std::to_string(originalSize) + " to " + pbPatternNames[pbPatternToUse]; 

		for (int pbIndex = 0; pbIndex < pbPatternOrders[pbPatternToUse];pbIndex++)
		{
			uint64_t noteLocation = (pbPatterns[pbPatternToUse][pbIndex] * spaceMultiplier);
			pitchIncluded[noteLocation] = 1;
		}
	}

	void TemperScale(int temperMode, double threshold, double strength) {
		
		double pitchRange = (1200.0 / equalDivisions) * threshold / 2.0;
		resultingPitches.clear();

		std::vector<std::vector<EFPitch>> temporaryPitches;
		temporaryPitches.clear();
		temporaryPitches.resize(temperingPitches.size());

		for(size_t i=0;i<reducedEfPitches.size();i++) {
			bool tempered = false;
			for(size_t j=0;j<temperingPitches.size();j++) {
				if(std::abs(reducedEfPitches[i].pitch - temperingPitches[j].pitch) <= pitchRange) {
					EFPitch rfPitch;
					rfPitch.pitchType = reducedEfPitches[i].pitchType;
					double numerator = reducedEfPitches[i].numerator;
					double denominator = reducedEfPitches[i].denominator;
					// double efRatio = numerator / denominator;
					// double tRatio = temperingPitches[j].numerator / temperingPitches[j].denominator	;
					double ratio;
					ratio = lerp(reducedEfPitches[i].ratio,temperingPitches[j].ratio,edoTemperingStrength);
					if(ratio == 2) {
						tempered = true;
						break;						
					}
					// fprintf(stderr,"pitch #:%lli pitch:%f  temperingPitch:%f  efRatio:%f  tRatio:%f calcRatio:%f \n",i,efPitches[i].pitch, temperingPitches[j].pitch, efPitches[i].ratio,temperingPitches[j].ratio,ratio);
					rfPitch.ratio = ratio;
					if(IsUniqueRatio(rfPitch.ratio, true)) {
						double gcd = GCD(numerator,denominator); // this aint gonna work
						rfPitch.numerator = numerator / gcd;
						rfPitch.denominator = denominator / gcd;
						double pitchInCents = 1200 * std::log2f(ratio);; 
						rfPitch.pitch = pitchInCents;
						rfPitch.tempering = reducedEfPitches[i].tempering + (std::log2f(ratio/reducedEfPitches[i].ratio) * 1200.0);
						double dissonance = CalculateDissonance(rfPitch.ratio * octaveScaleConstant);
						rfPitch.dissonance = dissonance;
						if(temperMode == TEMPER_ALL_PITCHES_TEMPERING)
							resultingPitches.push_back(rfPitch);
						else 
							temporaryPitches[j].push_back(rfPitch);
					}
					
					tempered = true;
					break;
				}
			}
			if(!tempered) {
				resultingPitches.push_back(reducedEfPitches[i]);
			}
		}
		if(temperMode == KEEP_BEST_MATCH_TEMPERING) {
			for(size_t j=0;j<temperingPitches.size();j++) {
				if(temporaryPitches[j].size() > 0) {
					double bestFit = 50000; // something arbitrarily large
					EFPitch bestFitPitch;
					for(size_t k=0;k<temporaryPitches[j].size();k++) {
						double fit = std::abs(temporaryPitches[j][k].pitch - temperingPitches[j].pitch);
						if(fit < bestFit) {
							bestFit = fit;
							bestFitPitch = temporaryPitches[j][k];
						}
					}
					resultingPitches.push_back(bestFitPitch);
				}
			}
		}
		// nbrPitches = resultingPitches.size(); //Actual Number of pitches
	}

	void CopyReducedNotes() {
		resultingPitches.clear();
		for(size_t i=0;i<reducedEfPitches.size();i++) {
			EFPitch resultPitch = reducedEfPitches[i];
			resultPitch.dissonance = CalculateDissonance(resultPitch.ratio * octaveScaleConstant);
			resultingPitches.push_back(resultPitch);
		}
	}

	int QuantizeNote(double inValue) {
		int selectedNote = 0;
		double lastDif = 99.0f;    
		for(uint64_t i = 0;i<actualScaleSize;i++) {            
			if(resultingPitches[i].inUse) {
				double lowNote = (resultingPitches[i].pitch / 1200.0) * (octaveScaleMapping ? 1.0 : octaveScaleConstant); 					
				double highNote = (resultingPitches[(i+1) % actualScaleSize].pitch / 1200.0) * (octaveScaleMapping ? 1.0 : octaveScaleConstant);
				if(i==actualScaleSize-1) {
					highNote += 1200 * (octaveScaleMapping ? 1.0 : octaveScaleConstant);;
				}
				double median = (lowNote + highNote) / 2.0;

				double lowNoteDif = std::abs(lowNote - inValue);
				double highNoteDif = std::abs(highNote - inValue);
				double medianDif = std::abs(median - inValue);
			
				double currentDif;
				bool direction = lowNoteDif < highNoteDif;
				int note;
				switch(quantizeMode) {
					case QUANTIZE_CLOSEST :
					default :
						currentDif = medianDif;
						note = direction ? i : (i + 1) % actualScaleSize;
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
					selectedNote = note;
				}  								
			}
		}
		return selectedNote;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "pitchGridDisplayMode", json_integer(pitchGridDisplayMode));
		json_object_set_new(rootJ, "triggerDelayEnabled", json_integer((bool) triggerDelayEnabled));
		json_object_set_new(rootJ, "noteReductionAlgorithm", json_integer((int) noteReductionAlgorithm));
		json_object_set_new(rootJ, "scaleMappingMode", json_integer((int) scaleMappingMode));
		json_object_set_new(rootJ, "useScaleWeighting", json_integer((bool) useScaleWeighting));
		json_object_set_new(rootJ, "edoTempering", json_integer(edoTempering));
		json_object_set_new(rootJ, "equalDivisionMode", json_integer(equalDivisionMode));
		json_object_set_new(rootJ, "quantizeMosRatio", json_boolean(quantizeMosRatio));
		json_object_set_new(rootJ, "quantizeOctaveSize", json_boolean(quantizeOctaveSize));
		json_object_set_new(rootJ, "octaveScaleMapping", json_integer((bool) octaveScaleMapping));
		json_object_set_new(rootJ, "spreadMode", json_boolean(spreadMode));
		json_object_set_new(rootJ, "octaveWrapAround", json_integer((int) octaveWrapAround));
		json_object_set_new(rootJ, "shiftMode", json_integer((int) shiftMode));
		json_object_set_new(rootJ, "keyLogarithmic", json_integer((int) keyLogarithmic));
		json_object_set_new(rootJ, "pitchRandomGaussian", json_integer((int) pitchRandomGaussian));
		json_object_set_new(rootJ, "quantizeMode", json_integer((int) quantizeMode)); 
		json_object_set_new(rootJ, "triggerPolyphonic", json_integer((int) triggerPolyphonic)); 
		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *cpGd = json_object_get(rootJ, "pitchGridDisplayMode");
		if (cpGd)
			pitchGridDisplayMode = json_integer_value(cpGd);

		json_t *ctTd = json_object_get(rootJ, "triggerDelayEnabled");
		if (ctTd)
			triggerDelayEnabled = json_integer_value(ctTd);

		json_t *sumEDM = json_object_get(rootJ, "equalDivisionMode");
		if (sumEDM) {
			equalDivisionMode = json_integer_value(sumEDM);			
		}

		json_t *sumMosR = json_object_get(rootJ, "quantizeMosRatio");
		if (sumMosR) {
			quantizeMosRatio = json_boolean_value(sumMosR);			
		}

		json_t *sumEdoT = json_object_get(rootJ, "edoTempering");
		if (sumEdoT) {
			edoTempering = json_integer_value(sumEdoT);			
		}

		json_t *sumQos = json_object_get(rootJ, "quantizeOctaveSize");
		if (sumQos) {
			quantizeOctaveSize = json_boolean_value(sumQos);			
		}

		json_t *sumOS = json_object_get(rootJ, "octaveScaleMapping");
		if (sumOS) {
			octaveScaleMapping = json_integer_value(sumOS);			
		}

		json_t *ctNra = json_object_get(rootJ, "noteReductionAlgorithm");
		if (ctNra)
			noteReductionAlgorithm = json_integer_value(ctNra);

		json_t *ctSmm = json_object_get(rootJ, "scaleMappingMode");
		if (ctSmm)
			scaleMappingMode = json_integer_value(ctSmm);

		json_t *ctUsw = json_object_get(rootJ, "useScaleWeighting");
		if (ctUsw)
			useScaleWeighting = json_integer_value(ctUsw);

		json_t *sumSpM = json_object_get(rootJ, "spreadMode");
		if (sumSpM) {
			spreadMode = json_boolean_value(sumSpM);			
		}

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

		json_t *sumQm = json_object_get(rootJ, "quantizeMode");
		if (sumQm) {
			quantizeMode = json_integer_value(sumQm);			
		}

		if(spreadMode) {
			reConfigParam(SPREAD_PARAM,0,1,0.0f,"%",0,100); 
		}	 else {
			reConfigParam(SPREAD_PARAM,0,10.0,0.0f," Pitches",0,1);
		}

		json_t *sumTp = json_object_get(rootJ, "triggerPolyphonic");
		if (sumTp) {
			triggerPolyphonic = json_integer_value(sumTp);			
		}

	}
	
	void CreateScalaFile(std::string fileName) {
		
		int noteCount = 1;
		for(uint64_t i=1;i<actualScaleSize;i++) {
			if(reducedEfPitches[i].inUse) {
				noteCount++;
			}
		}

		std::ofstream scalefile;
		scalefile.open (fileName);
		scalefile << "Math Nerd Generated Scale File.\n";
		scalefile << "! Numerator Factors:";
		for(int i=0;i<MAX_FACTORS;i++) {
			for(int j=0;j<actualNSteps[i];j++) {
				scalefile << " ";
				scalefile << factorNames[i];
				scalefile << " Tempering: ";
				scalefile << std::to_string(factorsTempering[i]);
			}
		}
		scalefile << "\n";

		scalefile << "! Denominator Factors:";
		for(int i=0;i<MAX_FACTORS;i++) {
			for(int j=0;j<actualDSteps[i];j++) {
				scalefile << " ";
				scalefile << factorNames[i];
				scalefile << " Tempering: ";
				scalefile << std::to_string(factorsTempering[i]);
			}
		}
		scalefile << "\n";

		if(nbrReducedPitches < nbrGeneratedPitches) {
			scalefile << "! Pitches Reduced to " + noteReductionPatternName + " by ";

			switch (noteReductionAlgorithm) {
				case EUCLIDEAN_ALGO :
					scalefile << "Euclidean Algorithm .\n";
					break;
				case GOLUMB_RULER_ALGO :
					scalefile << "Golumb Ruler Algorithm .\n";
					break;
				case PERFECT_BALANCE_ALGO :
					scalefile << "Perfect Balance Algorithm .\n";
					break;
			}
		}

		switch (scaleMappingMode) {
			case NO_SCALE_MAPPING :
				break;
			case SPREAD_SCALE_MAPPING :
				scalefile << "! Spread Mapped to ";
				scalefile << scaleNames[scale];
				scalefile << " scale.\n";
				break;
			case REPEAT_SCALE_MAPPING :
				scalefile << "! Repeat Mapped to ";
				scalefile << scaleNames[scale];
				scalefile << " scale.\n";
				break;
			case NEAREST_NEIGHBOR_SCALE_MAPPING :
				scalefile << "! Nearest Neighbor Mapped to ";
				scalefile << scaleNames[scale];
				scalefile << " scale.\n";
				break;
		}

		scalefile << noteCount;
		scalefile << "\n";
		for(uint64_t i=1;i<actualScaleSize;i++) {
			if(resultingPitches[i].inUse) {
				scalefile <<  std::to_string((resultingPitches[i].pitch * octaveScaleConstant));
				scalefile << "\n";
				scalefile << "! Based on ratio of ";
				scalefile << std::to_string(resultingPitches[i].numerator);
				if(octaveSize > 2.0) {
					scalefile << " x ";
					scalefile << std::to_string(octaveScaleConstant);
				}
				scalefile << " / ";
				scalefile << std::to_string(resultingPitches[i].denominator);
				scalefile << "\n";
			}
		}
		scalefile << std::to_string(octaveScaleConstant*1200.0);
		scalefile << "\n";
		scalefile.close();
	}

	void process(const ProcessArgs &args) override {

        if (edoModeTrigger.process(params[EQUAL_DIVISION_MODE_PARAM].getValue())) {
			equalDivisionMode = (equalDivisionMode + 1) % NBR_EDO_MODES;
		}		
		lights[EDO_MODE_LIGHT].value = equalDivisionMode > 1;
		lights[EDO_MODE_LIGHT + 1].value = equalDivisionMode > 0;
		lights[EDO_MODE_LIGHT + 2].value = 0;


        if (edoTemperingTrigger.process(params[EDO_TEMPERING_PARAM].getValue() + inputs[EDO_TEMPERING_INPUT].getVoltage())) {
			edoTempering = (edoTempering + 1) % NBR_TEMPERING_MODES;
		}		
		switch (edoTempering) {
			case NO_TEMPERING :
				lights[EDO_TEMPERING_LIGHT].value = 0;
				lights[EDO_TEMPERING_LIGHT + 1].value = 0;
				lights[EDO_TEMPERING_LIGHT + 2].value = 0;
				break;
			case KEEP_BEST_MATCH_TEMPERING :
				lights[EDO_TEMPERING_LIGHT].value = 0;
				lights[EDO_TEMPERING_LIGHT + 1].value = 1.0;
				lights[EDO_TEMPERING_LIGHT + 2].value = 0.0;
				break;
			case TEMPER_ALL_PITCHES_TEMPERING :
				lights[EDO_TEMPERING_LIGHT].value = 0;
				lights[EDO_TEMPERING_LIGHT + 1].value = 0.0;
				lights[EDO_TEMPERING_LIGHT + 2].value = 1.0;
				break;
		}

		if (mosModeTrigger.process(params[MOS_MODE_PARAM].getValue())) {
			mosMode = (mosMode + 1) % NBR_MOS_MODES;
		}		
		lights[MOS_MODE_LIGHT].value = 0;
		lights[MOS_MODE_LIGHT + 1].value = mosMode > 0;
		lights[MOS_MODE_LIGHT + 2].value = mosMode > 0;


        if (algorithmTrigger.process(params[NOTE_REDUCTION_ALGORITHM_PARAM].getValue() + inputs[NOTE_REDUCTION_ALGORITHM_INPUT].getVoltage())) {
			noteReductionAlgorithm = (noteReductionAlgorithm + 1) % NBR_ALGORITHMS;
		}		
		switch (noteReductionAlgorithm) {
			case NO_REDUCTION_ALGO :
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT].value = 0;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 1].value = 0;
				lights[NOTE_REDUCTION_ALGORITHM_LIGHT + 2].value = 0;
				break;
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
			case NEAREST_NEIGHBOR_SCALE_MAPPING:
				lights[SCALE_MAPPING_LIGHT].value = 1.0;
				lights[SCALE_MAPPING_LIGHT + 1].value = 0;
				lights[SCALE_MAPPING_LIGHT + 2].value = 1.0;
				break;
		}

        if (useScaleWeightingTrigger.process(params[USE_SCALE_WEIGHTING_PARAM].getValue() + inputs[USE_SCALE_WEIGHTING_INPUT].getVoltage())) {
			useScaleWeighting = !useScaleWeighting;
		}		
		lights[SCALE_WEIGHTING_LIGHT+2].value = useScaleWeighting;
		
		if (quantizeMosRatioTrigger.process(params[QUANTIZE_MOS_RATIO_PARAM].getValue())) {
			quantizeMosRatio = !quantizeMosRatio;
		}		
		lights[QUANTIZE_MOS_RATIO_LIGHT].value = quantizeMosRatio;


        if (octaveScaleMappingTrigger.process(params[OCTAVE_SCALE_SIZE_MAPPING_PARAM].getValue())) {
			octaveScaleMapping = !octaveScaleMapping;
		}		
		lights[OCTAVE_SCALE_SIZE_MAPPING_LIGHT].value = octaveScaleMapping;

		if (quantizeOctaveSizeTrigger.process(params[QUANTIZE_OCTAVE_PARAM].getValue())) {
			quantizeOctaveSize = !quantizeOctaveSize;
		}		
		lights[QUANTIZE_OCTAVE_SIZE_LIGHT].value = quantizeOctaveSize;



        if (spreadModeTrigger.process(params[SPREAD_MODE_PARAM].getValue())) {
			spreadMode = !spreadMode;
			if(spreadMode) {
				reConfigParam(SPREAD_PARAM,0,1,0.0f,"%",0,100); 
				spread = spread / actualScaleSize;
			} else {
				reConfigParam(SPREAD_PARAM,0,10.0,0.0f,"",0,1);
				spread = spread * actualScaleSize; 
			}
			params[SPREAD_PARAM].setValue(spread);
		}		
		lights[SPREAD_MODE_LIGHT].value = spreadMode;


        if (octaveWrapAroundTrigger.process(params[OCTAVE_WRAPAROUND_PARAM].getValue() + inputs[OCTAVE_WRAP_INPUT].getVoltage())) {
			octaveWrapAround = !octaveWrapAround;
		}		
		lights[OCTAVE_WRAPAROUND_LIGHT].value = octaveWrapAround;


		if (keyScalingTrigger.process(params[KEY_SCALING_PARAM].getValue())) {
			keyLogarithmic = !keyLogarithmic;
		}		
		lights[KEY_LOGARITHMIC_SCALE_LIGHT].value = keyLogarithmic;


		if (pitchRandomnessGaussianTrigger.process(params[PITCH_RANDOMNESS_GAUSSIAN_PARAM].getValue())) {
			pitchRandomGaussian = !pitchRandomGaussian;
		}		
		lights[PITCH_RANDOMNESS_GAUSSIAN_LIGHT].value = pitchRandomGaussian;

		if (keyScalingTrigger.process(params[KEY_SCALING_PARAM].getValue())) {
			keyLogarithmic = !keyLogarithmic;
		}		
		lights[KEY_LOGARITHMIC_SCALE_LIGHT].value = keyLogarithmic;

		if (clockModeTrigger.process(params[TRIGGER_MODE_PARAM].getValue())) {
			triggerPolyphonic = !triggerPolyphonic;
		}		
		lights[TRIGGER_POLYPHONIC_LIGHT].value = triggerPolyphonic;



        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() / 10.0f * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		spreadPercentage = spread;

		slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
		slantPercentage = slant;

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		distributionPercentage = focus;

		nonRepeat = clamp(params[NON_REPEATABILITY_PARAM].getValue() + (inputs[NON_REPEATABILITY_INPUT].getVoltage() / 10.0f * params[NON_REPEATABILITY_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		nonRepeatPercentage = nonRepeat;

        dissonanceProbability = clamp(params[DISSONANCE_PARAM].getValue() + (inputs[DISSONANCE_INPUT].getVoltage() / 10.0f * params[DISSONANCE_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
		dissonancePercentage = dissonanceProbability;

		randomRange = clamp(params[PITCH_RANDOMNESS_PARAM].getValue() + (inputs[PITCH_RANDOMNESS_INPUT].getVoltage() * params[PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM].getValue()),0.0,10.0);
		pitchRandomnessPercentage = randomRange / 10.0;
        
		scaleSize = clamp(params[NUMBER_OF_NOTES_PARAM].getValue() + (inputs[NUMBER_OF_NOTES_INPUT].getVoltage() * 11.5f * params[NUMBER_OF_NOTES_CV_ATTENUVERTER_PARAM].getValue()),1.0f,115.0);
		numberOfNotesPercentage = (scaleSize - 1.0) / 114.0;

        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,MAX_SCALES-1.0f);
		scalePercentage = scale/(MAX_SCALES-1.0);

        octaveSize = clamp(params[OCTAVE_SIZE_PARAM].getValue() + (inputs[OCTAVE_SIZE_INPUT].getVoltage() * 2.0 / 10.0 * params[OCTAVE_SIZE_CV_ATTENUVERTER_PARAM].getValue()),2.0f,5.0f);
		octaveSizePercentage = (octaveSize - 2.0) / 3.0;

		if(quantizeOctaveSize) {
			octaveSize = std::round(octaveSize * 4.0) / 4.0;
		}

		//Scale Expensive event handling
		bool scaleChange = false;
		if(paramChangeCounter == 0) {
			mosLargeSteps = clamp(params[MOS_LARGE_STEPS_PARAM].getValue() + (inputs[MOS_LARGE_STEPS_INPUT].getVoltage() * 2.0 * params[MOS_LARGE_STEPS_CV_ATTENUVERTER_PARAM].getValue()),0.0f,20.0f);
			mosLargeStepsPercentage = mosLargeSteps / 20.0;

			mosSmallSteps = clamp(params[MOS_SMALL_STEPS_PARAM].getValue() + (inputs[MOS_SMALL_STEPS_INPUT].getVoltage() * 2.0 * params[MOS_SMALL_STEPS_CV_ATTENUVERTER_PARAM].getValue()),0.0f,20.0f);
			mosSmallStepsPercentage = mosSmallSteps / 20.0;

			mosRatio = clamp(params[MOS_RATIO_PARAM].getValue() + (inputs[MOS_RATIO_INPUT].getVoltage() * 0.5 * params[MOS_RATIO_CV_ATTENUVERTER_PARAM].getValue()),1.0f,5.0f);
			mosRatioPercentage = (mosRatio - 1.0) / 4.0;

			if(quantizeMosRatio) {
				mosRatio = std::round(mosRatio * 4.0) / 4.0;
			}
			mosLevels = clamp(params[MOS_LEVELS_PARAM].getValue() + (inputs[MOS_LEVELS_INPUT].getVoltage() * 0.3 * params[MOS_LEVELS_CV_ATTENUVERTER_PARAM].getValue()),1.0f,3.0f);
			mosLevelsPercentage = (mosLevels - 1.0) / 2.0;

			equalDivisions = clamp(params[EQUAL_DIVISION_PARAM].getValue() + (inputs[EQUAL_DIVISION_INPUT].getVoltage() * 20.0 * params[EQUAL_DIVISION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,300.0f);
			equalDivisionSteps = clamp(params[EDO_STEPS_PARAM].getValue() + (inputs[EDO_STEPS_INPUT].getVoltage() * 20.0 * params[EDO_STEPS_CV_ATTENUVERTER_PARAM].getValue()),1.0,equalDivisions);
			edoStepsPercentage = equalDivisionSteps / 100.0;
			equalDivisionWraps = clamp(params[EDO_WRAPS_PARAM].getValue() + (inputs[EDO_WRAPS_INPUT].getVoltage() * 20.0 * params[EDO_WRAPS_CV_ATTENUVERTER_PARAM].getValue()),1.0,equalDivisionSteps);
			edoWrapsPercentage = equalDivisionWraps / 100.0;
			equalStepSize = clamp(params[EQUAL_STEP_SIZE_PARAM].getValue() + (inputs[EQUAL_DIVISION_INPUT].getVoltage() * 20.0 * params[EQUAL_DIVISION_CV_ATTENUVERTER_PARAM].getValue()),1.0f,300.0f);			
			edoPercentage = equalDivisionMode == 0 ? equalDivisions / 300.0 : equalStepSize / 300.0;

			edoTemperingThreshold = clamp(params[EDO_TEMPERING_THRESHOLD_PARAM].getValue() + (inputs[EDO_TEMPERING_THRESHOLD_INPUT].getVoltage() / 10.0 * params[EDO_TEMPERING_THRESHOLD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
			edoTemperingThresholdPercentage = edoTemperingThreshold;
			edoTemperingStrength = clamp(params[EDO_TEMPERING_STRENGTH_PARAM].getValue() + (inputs[EDO_TEMPERING_STRENGTH_INPUT].getVoltage() / 10.0 * params[EDO_TEMPERING_STRENGTH_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
			edoTemperingStrengthPercentage = edoTemperingStrength;


			for(int i=0;i<MAX_FACTORS;i++) {
				int factorIndex = clamp((int) (params[FACTOR_1_PARAM+ i].getValue() + (inputs[FACTOR_1_INPUT + i].getVoltage() * MAX_PRIME_NUMBERS / 10.0 * params[FACTOR_1_CV_ATTENUVERTER_PARAM+i].getValue())),0,MAX_PRIME_NUMBERS-1);
				factorsPercentage[i] = (factorIndex-i) / (MAX_PRIME_NUMBERS-MAX_FACTORS-1.0);
				factors[i] = primeNumbers[factorIndex];
				factorNames[i] = primeNumberNames[factorIndex];
				stepsN[i] = clamp(params[FACTOR_NUMERATOR_1_STEP_PARAM+ i].getValue() + (inputs[FACTOR_NUMERATOR_STEP_1_INPUT + i].getVoltage() * 1.0f * params[FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM+i].getValue()),0.0f,10.0f);
				factorsNumeratorPercentage[i] = stepsN[i] / 10.0;
				stepsD[i] = clamp(params[FACTOR_DENOMINATOR_1_STEP_PARAM+ i].getValue() + (inputs[FACTOR_DENOMINATOR_STEP_1_INPUT + i].getVoltage() * 0.5f * params[FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM+i].getValue()),0.0f,5.0f);
				factorsDivisorPercentage[i] = stepsD[i] / 5.0;
				factorsTempering[i] = clamp(params[FACTOR_TEMPER_1_STEP_PARAM+ i].getValue() + (inputs[FACTOR_TEMPER_STEP_1_INPUT + i].getVoltage() * 10.0f * params[FACTOR_TEMPER_1_STEP_CV_ATTENUVERTER_PARAM+i].getValue()),-50.0f,50.0f);
				factorsTemperingPercentage[i] = factorsTempering[i] / 50.0;
				scaleChange = scaleChange || (factors[i] != lastFactors[i]) || (factorsTempering[i] != lastfactorsTempering[i]) || (stepsN[i] != lastNSteps[i]) || (stepsD[i] != lastDSteps[i]);
				lastFactors[i] = factors[i];
				lastfactorsTempering[i] = factorsTempering[i];
				lastNSteps[i] = stepsN[i];
				lastDSteps[i] = stepsD[i];
			}
		}
		paramChangeCounter = (paramChangeCounter + 1) % PARAM_CHANGE_SCALING; 

		if(octaveSize != lastOctaveSize) {
			lastOctaveSize = octaveSize;
			octaveScaleConstant = octaveSize - 1.0;
		}

		if(equalDivisionMode != lastEqualDivisionMode || equalDivisions != lastEqualDivisions || equalDivisionSteps != lastEqualDivisionSteps || 
			equalDivisionWraps != lastEqualDivisionWraps || equalStepSize != lastEqualStepSize) {
			scaleChange = true;
			lastEqualDivisionMode = equalDivisionMode;
			lastEqualDivisions = equalDivisions;
			lastEqualDivisionSteps = equalDivisionSteps;
			lastEqualDivisionWraps = equalDivisionWraps;
			lastEqualStepSize = equalStepSize;
		}

		if(mosLargeSteps != lastMosLargeSteps || mosSmallSteps != lastMosSmallSteps || mosRatio != lastMosRatio || mosLevels != lastMosLevels) {
			scaleChange = true;
			lastMosLargeSteps = mosLargeSteps;
			lastMosSmallSteps = mosSmallSteps;
			lastMosRatio = mosRatio;
			lastMosLevels = mosLevels;
		}

		if(edoTempering != lastEdoTempering) {
			scaleChange = true;
		}


		//fprintf(stderr,"denom value: %f",(inputs[FACTOR_DENOMINATOR_STEP_1_INPUT + 1].getVoltage() * 1.0f * params[FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM+1].getValue()))
        if(scaleChange) {
			reloadPitches = true;
            BuildDerivedScale();
        }

		if(scaleChange || scaleSize != lastScaleSize || noteReductionAlgorithm != lastNoteReductionAlgorithm ) {
			reloadPitches = true;
			ReduceNotes(nbrGeneratedPitches,scaleSize);

			reducedEfPitches.clear();
			for(uint64_t i=0;i<nbrGeneratedPitches;i++) {
				if(pitchIncluded[i] > 0) {
					efPitches[i].inUse = false;
					reducedEfPitches.push_back(efPitches[i]);
				}
			}

			nbrReducedPitches = reducedEfPitches.size();
			reloadPitches = false;
			checkTemperingNeeded = true;
			lastNbrGeneratedPitches =nbrGeneratedPitches;
			lastNoteReductionAlgorithm = noteReductionAlgorithm;
			lastScaleSize = scaleSize;				
		}



		if(edoTempering != lastEdoTempering || edoTemperingThreshold != lastEdoTemperingThreshold || edoTemperingStrength != lastEdoTemperingStrength || checkTemperingNeeded) {
			
			if(edoTempering) {
				TemperScale(edoTempering, edoTemperingThreshold,edoTemperingStrength);
			} else {
				CopyReducedNotes();
			}
			nbrResultPitches = resultingPitches.size();
			lastEdoTempering = edoTempering;
			lastEdoTemperingThreshold = edoTemperingThreshold;
			lastEdoTemperingStrength = edoTemperingStrength;
			checkTemperingNeeded = false;
			mapPitches = true;				
		}



		if(scaleMappingMode != lastScaleMappingMode || scale != lastScale || useScaleWeighting != lastUseScaleWeighting ) {
			mapPitches = true;
		}

		if(mapPitches) {
							// fprintf(stderr, "Mapping Scale. %llu pitches \n", resultingPitches.size());

			actualScaleSize = resultingPitches.size();
			float scaleSpreadFactor = actualScaleSize / float(MAX_NOTES);
			uint64_t scaleIndex;
			switch(scaleMappingMode) {
				case NO_SCALE_MAPPING :
					for(uint64_t i=0;i<actualScaleSize;i++) {
						resultingPitches[i].inUse = true;
						resultingPitches[i].weighting = 0.8;
					}
					break;
				case SPREAD_SCALE_MAPPING :
					for(uint64_t i=0;i<resultingPitches.size();i++) {
						resultingPitches[i].inUse = false;
                        resultingPitches[i].weighting = 0.8;
					}
					for(uint64_t i=0;i<MAX_NOTES;i++) {
						if(defaultScaleNoteStatus[scale][i]) {
							uint64_t noteIndex = i * scaleSpreadFactor;
							//fprintf(stderr, "note index set %i %i \n", noteIndex,reducedEfPitches[noteIndex].inUse);
							resultingPitches[noteIndex].inUse = true;
							resultingPitches[noteIndex].weighting = useScaleWeighting ? defaultScaleNoteWeighting[scale][i] : 1.0;
						}
					}
					break;
				case REPEAT_SCALE_MAPPING:
					scaleIndex = 0;
					for(uint64_t i=0;i<actualScaleSize;i++) {
						if(defaultScaleNoteStatus[scale][scaleIndex]) {
							resultingPitches[i].inUse = true;
							resultingPitches[i].weighting = useScaleWeighting ? defaultScaleNoteWeighting[scale][scaleIndex] : 1.0;
						} else {
							resultingPitches[i].inUse = false;
                            resultingPitches[i].weighting = 0.8;
						}
						scaleIndex = (scaleIndex+1) % MAX_NOTES;
					}
					break;
				case NEAREST_NEIGHBOR_SCALE_MAPPING:
					for(uint64_t i=0;i<resultingPitches.size();i++) {
						resultingPitches[i].inUse = false;
                        resultingPitches[i].weighting = 0.8;
					}
					for(uint64_t mapScaleIndex=0;mapScaleIndex<MAX_NOTES;mapScaleIndex++) {
						if(defaultScaleNoteStatus[scale][mapScaleIndex]) {
							float targetPitch = mapScaleIndex * (octaveScaleConstant) * 100.0;
							int64_t selectedPitchIndex = -1;
							float lastDifference = 10000.0;
							for(uint64_t i=0;i<actualScaleSize;i++) {
								float difference = std::abs(resultingPitches[i].pitch - targetPitch);
								if(difference < lastDifference) {
									lastDifference = difference;
									selectedPitchIndex = i;
								} 
							}
							if(selectedPitchIndex >= 0) {
								resultingPitches[selectedPitchIndex].inUse = true;
								resultingPitches[selectedPitchIndex].weighting = useScaleWeighting ? defaultScaleNoteWeighting[scale][mapScaleIndex] : 1.0;
							}
						}
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
        key = clamp(key,0,11);
		keyPercentage = key / 11.0;
		if(key != lastKey) {
			modulationRoot = 0.0;
			microtonalKey = 0;
			lastKey = key;
		}
       
        octave = clamp(params[OCTAVE_PARAM].getValue() + (inputs[OCTAVE_INPUT].getVoltage() * 0.4 * params[OCTAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);
		octavePercentage = (octave + 4.0f) / 8.0f;

		currentPolyphony = inputs[NOTE_INPUT].getChannels();

		outputs[NOTE_CHANGE_OUTPUT].setChannels(currentPolyphony);
		outputs[WEIGHT_OUTPUT].setChannels(currentPolyphony);
		outputs[QUANT_OUTPUT].setChannels(currentPolyphony);

		noteChange = false;
		double octaveIn[POLYPHONY];

//NOTE: Monophonic for testing - will need to be moved below loop
		if (setRootNoteTrigger.process(params[SET_ROOT_NOTE_PARAM].getValue() + inputs[SET_ROOT_NOTE_INPUT].getVoltage())) {
			getModulationRoot = true;
		}		

		for(int channel = 0;channel<currentPolyphony;channel++) {
			double noteIn;
			double originalNoteIn;
			double fractionalValue;
			double originalFractionalValue;	
			bool recalcNoteInNeeded;
			bool recalcKeyNeeded = scaleChange && modulationRoot != 0;  //make a scale change force a recalc as well
			do {							
				recalcNoteInNeeded = false;

				originalNoteIn= inputs[NOTE_INPUT].getVoltage(channel) - (float(key)/12.0);
				noteIn = originalNoteIn - modulationRoot;

				if(!octaveScaleMapping) {
					originalNoteIn /= (octaveScaleConstant);
					noteIn /= (octaveScaleConstant);
				}

				float originalOctaveIn = std::floor(originalNoteIn);
				octaveIn[channel] = std::floor(noteIn);
				fractionalValue = noteIn - octaveIn[channel];
				if(fractionalValue < 0.0)
					fractionalValue += 1.0;

				if(!octaveScaleMapping) {
					fractionalValue *= octaveScaleConstant;
				}

				originalFractionalValue = originalNoteIn - originalOctaveIn;
				if(originalFractionalValue < 0.0)
					originalFractionalValue += 1.0;

				if(!octaveScaleMapping) {
					originalFractionalValue *= octaveScaleConstant;
				}

				if(getModulationRoot) {
					modulationRoot = originalFractionalValue;
					recalcNoteInNeeded = true;
					getModulationRoot = false;
					recalcKeyNeeded = true;
				}
			} while (recalcNoteInNeeded);

			//Calcuate microtonal key note
			if(recalcKeyNeeded) {
				microtonalKey = QuantizeNote(originalFractionalValue);
				recalcKeyNeeded = false;				
			}

											              //fprintf(stderr, "%i %f \n", key, fractionalValue);
			//Calcuate root note
			if(fractionalValue != lastFractionalValue[channel]) {
				currentNote[channel] = QuantizeNote(fractionalValue);
				lastFractionalValue[channel] = fractionalValue;
				if(currentNote[channel] != lastNote[channel]) {
					noteChange = true;
					lastNote[channel] = currentNote[channel];
				}
			}
		}
		


		if(noteChange || lastSpread != spread || lastSlant != slant || lastFocus != focus || dissonanceProbability != lastDissonanceProbability 
					  || mapPitches ) {
			float actualSpread = spreadMode ? spread * float(actualScaleSize) / 2.0 : clamp(spread,0.0,actualScaleSize / 2.0);
			upperSpread = std::ceil(actualSpread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil(actualSpread * std::min(1.0-slant,1.0));


							            //   fprintf(stderr, "Math Nerd - Note Processing \n");


			float whatIsDissonant = 0.2; // lower than this is consonant
			for(int channel = 0;channel<currentPolyphony;channel++) {
				for(uint64_t i = 0; i<actualScaleSize;i++) {
					noteProbability[channel][i] = 0.0;
				}

				//EFPitch currentPitch = reducedEfPitches[currentNote[channel]];
				noteProbability[channel][currentNote[channel]] = 1.0;

			
				for(int i=1;i<=actualSpread;i++) {
					int64_t noteAbove = (currentNote[channel] + i) % actualScaleSize;
					int64_t noteBelow = (currentNote[channel] - i);
					if(noteBelow < 0)
						noteBelow +=actualScaleSize;

					EFPitch upperPitch = resultingPitches[noteAbove];
					EFPitch lowerPitch = resultingPitches[noteBelow];
					float upperDissonance = upperPitch.dissonance;
					float lowerDissonance = lowerPitch.dissonance;
					float upperNoteDissonanceProbabilityAdjustment = 1.0;
					float lowerNoteDissonanceProbabilityAdjustment = 1.0;
					if(dissonanceProbability < 0) {
						if(upperDissonance > whatIsDissonant) {
							upperNoteDissonanceProbabilityAdjustment = (1.0f-upperDissonance) * (1.0 + dissonanceProbability);
						}
						if(lowerDissonance > whatIsDissonant) {
							lowerNoteDissonanceProbabilityAdjustment = (1.0f-lowerDissonance) * (1.0 + dissonanceProbability);
						}
					}					
					if(dissonanceProbability > 0) {
						if(upperDissonance < whatIsDissonant) {
							upperNoteDissonanceProbabilityAdjustment = std::min((upperDissonance / whatIsDissonant),1.0f)  * (1.0 - dissonanceProbability);
						}
						if(lowerDissonance < whatIsDissonant) {
							lowerNoteDissonanceProbabilityAdjustment = std::min((lowerDissonance / whatIsDissonant),1.0f) * (1.0 - dissonanceProbability);
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

					for(size_t i=0;i<actualScaleSize;i++) {
						actualProbability[channel][i] = noteProbability[channel][i];
					}	

					float repeatProbability = ((double) rand()/RAND_MAX);
					if (spread > 0 && nonRepeat > 0.0 && repeatProbability < nonRepeat && lastRandomNote[channel] >= 0) {
						actualProbability[channel][lastRandomNote[channel]] = 0; //Last note has no chance of repeating 						
					}
			
					int randomNote = weightedProbability(actualProbability[channel],params[WEIGHT_SCALING_PARAM].getValue(), rnd);
					if(randomNote == -1) { //Couldn't find a note, so find first active
						bool noteOk = false;
						uint64_t notesSearched = 0;
						randomNote = currentNote[channel]; 
						do {
							randomNote = (randomNote + 1) % actualScaleSize;
							notesSearched +=1;
							noteOk = notesSearched >= actualScaleSize;
						} while(!noteOk);
					}

					lastRandomNote[channel] = randomNote; // for repeatability					
					probabilityNote[channel] = randomNote;

					float octaveAdjust = 0.0;
					if(!octaveWrapAround) {
						if(randomNote > currentNote[channel] && randomNote - currentNote[channel] > upperSpread)
							octaveAdjust = -1.0;
						if(randomNote < currentNote[channel] && currentNote[channel] - randomNote > lowerSpread)
							octaveAdjust = 1.0;
					}

					int notePosition = randomNote;				

					double quantitizedNoteCV = (resultingPitches[notePosition].pitch * octaveScaleConstant / 1200.0) + (key / 12.0); 
            //   fprintf(stderr, "%f  %f\n", resultingPitches[notePosition].pitch,octaveScaleConstant);

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

					quantitizedNoteCV += (octaveIn[channel] + octave + octaveAdjust); 
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
	for(int i=0;i<POLYPHONY;i++) {
		clockTrigger[i].reset();
		for(int j=0;j<TRIGGER_DELAY_SAMPLES;j++) {
			triggerDelay[i][j] = 0.0f;
		}
	}
	resetTriggered = true;
	triggerDelayEnabled = false;

	modulationRoot = 0.0;
	microtonalKey = 0;
}



struct ProbablyNoteMNDisplay : TransparentWidget {
	ProbablyNoteMN *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	bool noteInspectorVisible = false;
	int64_t displayedNoteIndex;
	

	ProbablyNoteMNDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


	//this needs to be mouse hover
	// void onDragStart(const event::DragStart &e) override {
	// 	float hoverX = APP->scene->rack->mousePos.x;
	// 	float hoverY = APP->scene->rack->mousePos.y;
	// }

	void onHover(const event::Hover& e) override {
		//hard coded center for now;
		float hoverX = e.pos.x;
		float hoverY = e.pos.y;

		findNoteByPos(Vec(585.5,241.5),hoverX, hoverY);
		
	}

	void onLeave(const event::Leave& e) override {
		noteInspectorVisible = false;
	}


	void findNoteByPos(Vec center,float hoverX, float hoverY) {

		float posX = hoverX - center.x;
		float posY = hoverY - center.y;

		float distance = posX * posX + posY * posY;
		if(distance < 5625) //75^2
		{
			float theta = atan2(posY,posX);
			float x = cos(theta)*75;
			float y = sin(theta)*75;
			float distanceThreshold = 1;
			int64_t noteIndex = -1;
			for(uint64_t i=0;i<module->resultingPitches.size();i++) {
				float xDist = module->resultingPitches[i].xPos-x;
				float yDist = module->resultingPitches[i].yPos-y;
				float distance = xDist * xDist + yDist * yDist;
				if(distance < distanceThreshold) {
					noteIndex = i;
					break;
				}
			}

			displayedNoteIndex = noteIndex;
			if(noteIndex >= 0) {
				noteInspectorVisible = true;
				// fprintf(stderr, "Show Note! %lli\n", noteIndex);
			} else {
				noteInspectorVisible = false;
			}
			// displayNoteInfo = true;
		} else {
			noteInspectorVisible = false;
		}
	}


    void drawKey(const DrawArgs &args, Vec pos, int key, float modulationRoot) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);

		if(key < 0)
			return;

		char text[128];

		//Green if no other root set
		if(modulationRoot != 0) 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0x8f));
		else
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));

		snprintf(text, sizeof(text), "%s", module->noteNames[key]);
		              //fprintf(stderr, "%s\n", module->noteNames[key]);

		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	 void drawModulationRoot(const DrawArgs &args, Vec pos, int microtonalKey) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);

		if(microtonalKey == 0)
			return;

		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "+%i", microtonalKey);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawFactors(const DrawArgs &args, Vec pos) {
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
        nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);



		char text[128];
        for(int i=0;i<MAX_FACTORS;i++) {
			if(module->actualNSteps[i] == 0 && module->actualDSteps[i] == 0) 
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xcf));
			else
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
    		nvgFontSize(args.vg, 11);
	        snprintf(text, sizeof(text), "%s", module->factorNames[i].c_str());
            nvgText(args.vg, pos.x+5.0, pos.y+i*34.5 + 0.5, text, NULL);

			nvgFontSize(args.vg, 9);
			if(module->actualNSteps[i] == 0) 
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0x8f));
			else
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
            snprintf(text, sizeof(text), "%i", module->actualNSteps[i]);
            nvgText(args.vg, pos.x+74, pos.y+i*34.5, text, NULL);

			if(module->actualDSteps[i] == 0) 
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0x8f));
			else
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
            snprintf(text, sizeof(text), "%i", module->actualDSteps[i]);
            nvgText(args.vg, pos.x+149, pos.y+i*34.5, text, NULL);

			if(module->factorsTempering[i] == 0) 
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			else if(module->factorsTempering[i] > 0)
				nvgFillColor(args.vg, nvgRGBA(0x37, 0x6a, 0xf3, 0xff));
			else
				nvgFillColor(args.vg, nvgRGBA(0xc3, 0x27, 0x27, 0xff));
            snprintf(text, sizeof(text), "%2.2f", module->factorsTempering[i]);
            nvgText(args.vg, pos.x+227, pos.y+i*34.5, text, NULL);

        }
	}

    void drawPitchInfo(const DrawArgs &args, Vec pos) {

		nvgStrokeWidth(args.vg, 1);

		if(module->edoTempering > 0) {
			nvgStrokeColor(args.vg, nvgRGBA(0x4a, 0x23, 0xc7, 0x4f));
			for(uint64_t i=0;i<module->temperingPitches.size();i++) {
				float pitch = module->temperingPitches[i].pitch;
				nvgBeginPath(args.vg);
				float theta = pitch / 1200.0 *  M_PI * 2.0 - (M_PI/2.0);
				float x = cos(theta)*75+pos.x;
				float y = sin(theta)*75+pos.y;
				nvgMoveTo(args.vg, x, y);
				nvgLineTo(args.vg, pos.x, pos.y);
				nvgStroke(args.vg);
			}
		}

        for(uint64_t i=0;i<module->resultingPitches.size();i++) {
            float pitch = module->resultingPitches[i].pitch;
            float dissonance = module->resultingPitches[i].dissonance;
			bool inUse = module->resultingPitches[i].inUse;
			uint8_t opacity =  std::max(255.0f * module->noteProbability[0][i],70.0f);
			
			if(i == module->probabilityNote[0]) 
				nvgStrokeColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			else
				nvgStrokeColor(args.vg, inUse ? nvgRGBA(0xff, 0xff, 0x00, opacity) : nvgRGBA(0xff, 0x00, 0x00, opacity));

            //   fprintf(stderr, "i:%llu p:%f d:%f  iu:%i opacity:%u\n ", i, pitch,dissonance, inUse, opacity);

            nvgBeginPath(args.vg);            
			float dissonanceDistance = dissonance * 75.0f;
			//  dissonanceDistance = 10;
			float theta = pitch / 1200.0 *  M_PI * 2.0 - (M_PI/2.0);
			float x = cos(theta);
			float y = sin(theta);
			nvgMoveTo(args.vg, x*75.0+pos.x, y*75.0+pos.y);
			nvgLineTo(args.vg, x*dissonanceDistance+pos.x, y*dissonanceDistance + pos.y);
			module->resultingPitches[i].xPos = x*75.0;
			module->resultingPitches[i].yPos = y*75.0;
            nvgStroke(args.vg);
        }
	}


    void drawOctaveSize(const DrawArgs &args, Vec pos, float octaveSize) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
			//fprintf(stderr, "a: %llu s: %llu  \n", module->actualScaleSize, module->nbrPitches);
		char text[128];
		// if(module->equalDivisions > 0) 
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		// else
		// 	nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0x00));
		snprintf(text, sizeof(text), "%1.3f", octaveSize);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawMomentsOfSymmetry(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
			//fprintf(stderr, "a: %llu s: %llu  \n", module->actualScaleSize, module->nbrPitches);
		char text[128];
		 if(module->invalidMos)
			nvgFillColor(args.vg, nvgRGB(0xff, 0x00, 0x00));
		else if(module->mosLargeSteps > 0 || module->mosSmallSteps > 0) 
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		else
			nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0x00));
		snprintf(text, sizeof(text), "%i", module->mosLargeSteps);
		nvgText(args.vg, pos.x, pos.y, text, NULL);

		snprintf(text, sizeof(text), "%i", module->mosSmallSteps);
		nvgText(args.vg, pos.x+46, pos.y, text, NULL);

		snprintf(text, sizeof(text), "%1.3f", module->mosRatio);
		nvgText(args.vg, pos.x+92, pos.y, text, NULL);

		snprintf(text, sizeof(text), "%i", module->mosLevels);
		nvgText(args.vg, pos.x+138, pos.y, text, NULL);

	}

    void drawEqualDivisions(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
			//fprintf(stderr, "a: %llu s: %llu  \n", module->actualScaleSize, module->nbrPitches);
		char text[128];
		if(module->equalDivisionMode == 0) {
			if(module->equalDivisions > 0) 
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			else
				nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0x00));
			snprintf(text, sizeof(text), "%i", module->equalDivisions);
			nvgText(args.vg, pos.x, pos.y, text, NULL);

			snprintf(text, sizeof(text), "%i", module->equalDivisionSteps);
			nvgText(args.vg, pos.x+59, pos.y, text, NULL);

			snprintf(text, sizeof(text), "%i", module->equalDivisionWraps);
			nvgText(args.vg, pos.x+116, pos.y, text, NULL);
		} else {
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			snprintf(text, sizeof(text), "%.3f", module->equalStepSize);
			nvgText(args.vg, pos.x, pos.y, text, NULL);

		}
	}

	void drawTemperingMode(const DrawArgs &args, Vec pos, int edoTemperingMode) {
		if(module->edoTempering > 0) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
			char text[128];
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			snprintf(text, sizeof(text), "%s", module->edoTemperingModes[edoTemperingMode-1]);
			nvgText(args.vg, pos.x, pos.y, text, NULL);
		}
	}


    void drawTempering(const DrawArgs &args, Vec pos) {
		if(module->edoTempering > 0) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
				//fprintf(stderr, "a: %llu s: %llu  \n", module->actualScaleSize, module->nbrPitches);
			char text[128];
			if(module->equalDivisions > 0) 
				nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			else
				nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0x00));
			snprintf(text, sizeof(text), "%3.2f %%", module->edoTemperingThreshold*100.0);
			nvgText(args.vg, pos.x, pos.y, text, NULL);

			snprintf(text, sizeof(text), "%3.2f %%", module->edoTemperingStrength*100.0);
			nvgText(args.vg, pos.x+57, pos.y, text, NULL);
		}
	}

	void drawAlgorithm(const DrawArgs &args, Vec pos, int algorithn) {
		if(algorithn != module->NO_REDUCTION_ALGO) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgTextAlign(args.vg,NVG_ALIGN_LEFT);


			char text[128];
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			snprintf(text, sizeof(text), "%s", module->algorithms[algorithn]);
			nvgText(args.vg, pos.x, pos.y, text, NULL);
		}
	}


    void drawNoteReduction(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
			// fprintf(stderr, "a: %llu s: %llu  \n", module->scaleSize, module->nbrPitches);
		char text[128];
		if(module->scaleSize <= module->nbrGeneratedPitches) 
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		else
			nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0x00));
		snprintf(text, sizeof(text), "%s", module->noteReductionPatternName.c_str());
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawScaleMapping(const DrawArgs &args, Vec pos, int scaleMappingMode) {
		if(module->scaleMappingMode > 0) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
			char text[128];
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			snprintf(text, sizeof(text), "%s", module->scaleMappings[scaleMappingMode-1]);
			nvgText(args.vg, pos.x, pos.y, text, NULL);
		}
	}

    void drawScale(const DrawArgs &args, Vec pos, int scaleMappingMode) {
		if(scaleMappingMode > 0) {
			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
			char text[128];
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			snprintf(text, sizeof(text), "%s", module->scaleNames[module->scale]);
			nvgText(args.vg, pos.x, pos.y, text, NULL);
		}
	}

	void drawPitchGrid(const DrawArgs &args, Vec pos, int pitchGridMode) {
		if(pitchGridMode == 0) {
			return;
		} else if (pitchGridMode == 1) {
			nvgStrokeWidth(args.vg, 1);
			float octaveSize = module->octaveSize;
			float nbrCentLines = (octaveSize - 1.0f) * 12;
			for(int i = 0; i < nbrCentLines;i++) {
				if(i % 12 == 0)
					nvgStrokeColor(args.vg, nvgRGB(0x80, 0x80, 0x80));
				else
					nvgStrokeColor(args.vg, nvgRGB(0x40, 0x40, 0x40));

				nvgBeginPath(args.vg);
				float theta = float(i)/nbrCentLines * M_PI * 2.0 - (M_PI/2.0);
				float x = cos(theta);
				float y = sin(theta);
				nvgMoveTo(args.vg, x*75.0+pos.x, y*75.0+pos.y);
				nvgLineTo(args.vg, pos.x, pos.y);
				nvgStroke(args.vg);
			}
		} else if (pitchGridMode == 2) {
			nvgStrokeWidth(args.vg, 1);
			float octaveSize = module->octaveSize;
			for(int i = 0; i < 28;i++) {
				float majorRatio = module->majorRatios[i];
				float octaveCents = ((octaveSize-1.0)*1200);				
				if(majorRatio <= octaveCents) {
					if(i % 7 == 6)
						nvgStrokeColor(args.vg, nvgRGB(0x80, 0x80, 0x80));
					else
						nvgStrokeColor(args.vg, nvgRGB(0x40, 0x40, 0x40));

					nvgBeginPath(args.vg);
					float theta = majorRatio/octaveCents * M_PI * 2.0 - (M_PI/2.0);
					float x = cos(theta);
					float y = sin(theta);
					nvgMoveTo(args.vg, x*75.0+pos.x, y*75.0+pos.y);
					nvgLineTo(args.vg, pos.x, pos.y);
					nvgStroke(args.vg);
				} else {
					break;
				}
			}
		} else if (pitchGridMode == 3) {
			nvgStrokeWidth(args.vg, 1);
			float octaveSize = module->octaveSize;
			for(int i = 0; i < 28;i++) {
				float minorRatio = module->minorRatios[i];
				float octaveCents = ((octaveSize-1.0)*1200);				
				if(minorRatio <= octaveCents) {
					if(i % 7 == 6)
						nvgStrokeColor(args.vg, nvgRGB(0x80, 0x80, 0x80));
					else
						nvgStrokeColor(args.vg, nvgRGB(0x40, 0x40, 0x40));

					nvgBeginPath(args.vg);
					float theta = minorRatio/octaveCents * M_PI * 2.0 - (M_PI/2.0);
					float x = cos(theta);
					float y = sin(theta);
					nvgMoveTo(args.vg, x*75.0+pos.x, y*75.0+pos.y);
					nvgLineTo(args.vg, pos.x, pos.y);
					nvgStroke(args.vg);
				} else {
					break;
				}
			}
		}
	}

	void drawNoteCount(const DrawArgs &args, Vec pos, size_t noteCount) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "%llu", noteCount);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawNoteInspector(const DrawArgs &args, Vec pos, int64_t currentNote) {
			nvgStrokeWidth(args.vg, 1.0);
			nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
			nvgFillColor(args.vg, nvgRGBA(0x10, 0x10, 0x15, 0xff));

			nvgBeginPath(args.vg);
			nvgRect(args.vg,pos.x,pos.y,100,60);
			nvgClosePath(args.vg);		
			nvgStroke(args.vg);
			nvgFill(args.vg);

			EFPitch inspectedPitch = module->resultingPitches[currentNote];

			nvgFontSize(args.vg, 9);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1);
			nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
			char text[128];
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
			std::string pitchTypeName = "";
			switch(inspectedPitch.pitchType) {
				case module->RATIO_PITCH_TYPE:
					pitchTypeName = "JI";
					break;
				case module->EQUAL_DIVISION_PITCH_TYPE:
					pitchTypeName = "EDO";
					break;
				case module->EQUAL_STEP_SIZE_PITCH_TYPE:
					pitchTypeName = "Equal Step";
					break;
				case module->MOS_PITCH_TYPE:
					pitchTypeName = "MOS";
					break;
			}

			snprintf(text, sizeof(text), "Type: %s", pitchTypeName.c_str());
			nvgText(args.vg, pos.x+2, pos.y+10, text, NULL);
			snprintf(text, sizeof(text), "Ratio: %.0f/%.0f", inspectedPitch.numerator,inspectedPitch.denominator);
			nvgText(args.vg, pos.x+2, pos.y+20, text, NULL);
			snprintf(text, sizeof(text), "Tempering: %.3f", inspectedPitch.tempering);
			nvgText(args.vg, pos.x+2, pos.y+30, text, NULL);
			snprintf(text, sizeof(text), "Pitch: %.3f", inspectedPitch.pitch * module->octaveScaleConstant );
			nvgText(args.vg, pos.x+2, pos.y+40, text, NULL);

	}
	

	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawPitchGrid(args, Vec(585.5,241.5),module->pitchGridDisplayMode);
        drawPitchInfo(args,Vec(585.5,241.5));
		drawKey(args, Vec(564,82), module->key, module->modulationRoot);
		drawModulationRoot(args, Vec(578,332), module->microtonalKey);
		drawOctaveSize(args, Vec(532,109),module->octaveSize);
		drawEqualDivisions(args, Vec(344,49)); 
		drawMomentsOfSymmetry(args, Vec(331,131)); 
		drawFactors(args, Vec(35,30));
		drawAlgorithm(args, Vec(404,200), module->noteReductionAlgorithm);
		drawNoteReduction(args, Vec(334,200));
		drawTemperingMode(args, Vec(307,259),module->edoTempering);
		drawTempering(args, Vec(411,259));
		drawScaleMapping(args, Vec(324,318.5),module->scaleMappingMode);
		drawScale(args, Vec(414,308.5),module->scaleMappingMode);
		drawNoteCount(args, Vec(608.5,159),module->actualScaleSize);
		if(noteInspectorVisible)
			drawNoteInspector(args, Vec(420,159),displayedNoteIndex);
	}
};

struct ProbablyNoteMNWidget : ModuleWidget {

	ParamWidget* edoParam;
	ParamWidget* esoParam;

	ProbablyNoteMNWidget(ProbablyNoteMN *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteMN.svg")));


		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(502, 345), module, ProbablyNoteMN::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(532, 345), module, ProbablyNoteMN::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(562, 345), module, ProbablyNoteMN::SET_ROOT_NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(592, 345), module, ProbablyNoteMN::EXTERNAL_RANDOM_INPUT));


		addParam(createParam<LEDButton>(Vec(545, 359), module, ProbablyNoteMN::TRIGGER_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(546.5, 360.5), module, ProbablyNoteMN::TRIGGER_POLYPHONIC_LIGHT));

		addParam(createParam<TL1105>(Vec(576, 360), module, ProbablyNoteMN::SET_ROOT_NOTE_PARAM));

		ParamWidget* spreadParam = createParam<RoundSmallFWKnob>(Vec(508,25), module, ProbablyNoteMN::SPREAD_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(spreadParam)->percentage = &module->spreadPercentage;
		}
		addParam(spreadParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(534,51), module, ProbablyNoteMN::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(536, 29), module, ProbablyNoteMN::SPREAD_INPUT));

		addParam(createParam<LEDButton>(Vec(513, 52), module, ProbablyNoteMN::SPREAD_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(514.5, 53.5), module, ProbablyNoteMN::SPREAD_MODE_LIGHT));


		ParamWidget* slantParam = createParam<RoundSmallFWKnob>(Vec(565,25), module, ProbablyNoteMN::SLANT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->percentage = &module->slantPercentage;
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->biDirectional = true;
		}
		addParam(slantParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(591,51), module, ProbablyNoteMN::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(593, 29), module, ProbablyNoteMN::SLANT_INPUT));

		ParamWidget* distributionParam = createParam<RoundSmallFWKnob>(Vec(622, 25), module, ProbablyNoteMN::DISTRIBUTION_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(distributionParam)->percentage = &module->distributionPercentage;
		}
		addParam(distributionParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(648,51), module, ProbablyNoteMN::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(650, 29), module, ProbablyNoteMN::DISTRIBUTION_INPUT));

		ParamWidget* nonRepeatParam = createParam<RoundSmallFWKnob>(Vec(679, 25), module, ProbablyNoteMN::NON_REPEATABILITY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(nonRepeatParam)->percentage = &module->nonRepeatPercentage;
		}
		addParam(nonRepeatParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(705,51), module, ProbablyNoteMN::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(707, 29), module, ProbablyNoteMN::NON_REPEATABILITY_INPUT));


		ParamWidget* keyParam = createParam<RoundSmallFWSnapKnob>(Vec(563,86), module, ProbablyNoteMN::KEY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(keyParam)->percentage = &module->keyPercentage;
		}
		addParam(keyParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(589,112), module, ProbablyNoteMN::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(591, 90), module, ProbablyNoteMN::KEY_INPUT));

		addParam(createParam<LEDButton>(Vec(568, 113), module, ProbablyNoteMN::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(569.5, 114.5), module, ProbablyNoteMN::KEY_LOGARITHMIC_SCALE_LIGHT));

		ParamWidget* dissonanceParam = createParam<RoundSmallFWKnob>(Vec(623,86), module, ProbablyNoteMN::DISSONANCE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(dissonanceParam)->percentage = &module->dissonancePercentage;
			dynamic_cast<RoundSmallFWKnob*>(dissonanceParam)->biDirectional = true;
		}
		addParam(dissonanceParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(649,112), module, ProbablyNoteMN::DISSONANCE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(651, 90), module, ProbablyNoteMN::DISSONANCE_INPUT));


		ParamWidget* octaveParam = createParam<RoundSmallFWSnapKnob>(Vec(683,86), module, ProbablyNoteMN::OCTAVE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(octaveParam)->percentage = &module->octavePercentage;
		}
		addParam(octaveParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(709,112), module, ProbablyNoteMN::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(711, 90), module, ProbablyNoteMN::OCTAVE_INPUT));

		addParam(createParam<LEDButton>(Vec(709, 143), module, ProbablyNoteMN::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(710.5, 144.5), module, ProbablyNoteMN::OCTAVE_WRAPAROUND_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(687, 144), module, ProbablyNoteMN::OCTAVE_WRAP_INPUT));

		ParamWidget* pitchRandomnessParam = createParam<RoundSmallFWKnob>(Vec(683,216), module, ProbablyNoteMN::PITCH_RANDOMNESS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(pitchRandomnessParam)->percentage = &module->pitchRandomnessPercentage;
		}
		addParam(pitchRandomnessParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(709,242), module, ProbablyNoteMN::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(711, 220), module, ProbablyNoteMN::PITCH_RANDOMNESS_INPUT));
		addParam(createParam<LEDButton>(Vec(686, 250), module, ProbablyNoteMN::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(687.5, 251.5), module, ProbablyNoteMN::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));


		addParam(createParam<RoundReallySmallFWKnob>(Vec(697,292), module, ProbablyNoteMN::WEIGHT_SCALING_PARAM));	




		ParamWidget* octaveSizeParam = createParam<RoundSmallFWKnob>(Vec(503,113), module, ProbablyNoteMN::OCTAVE_SIZE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(octaveSizeParam)->percentage = &module->octaveSizePercentage;
		}
		addParam(octaveSizeParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(529,139), module, ProbablyNoteMN::OCTAVE_SIZE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(531, 117), module, ProbablyNoteMN::OCTAVE_SIZE_INPUT));

		addParam(createParam<LEDButton>(Vec(484, 116), module, ProbablyNoteMN::OCTAVE_SCALE_SIZE_MAPPING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(485.5, 117.5), module, ProbablyNoteMN::OCTAVE_SCALE_SIZE_MAPPING_LIGHT));

		addParam(createParam<LEDButton>(Vec(508, 140), module, ProbablyNoteMN::QUANTIZE_OCTAVE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(509.5, 141.5), module, ProbablyNoteMN::QUANTIZE_OCTAVE_SIZE_LIGHT));


//Scale Generation
//Prime Factors
        for(int i=0;i<MAX_FACTORS;i++) {	
			ParamWidget* factorParam = createParam<RoundReallySmallFWSnapKnob>(Vec(10,i*34.5+33), module, ProbablyNoteMN::FACTOR_1_PARAM+i);
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(factorParam)->percentage = &module->factorsPercentage[i];
			}
			addParam(factorParam);							
    		addInput(createInput<FWPortInReallySmall>(Vec(34, i*34.44 + 36), module, ProbablyNoteMN::FACTOR_1_INPUT+i));
            addParam(createParam<RoundExtremelySmallFWKnob>(Vec(50,i*34.5+35), module, ProbablyNoteMN::FACTOR_1_CV_ATTENUVERTER_PARAM+i));

			ParamWidget* factorNumeratorParam = createParam<RoundReallySmallFWSnapKnob>(Vec(82,i*34.5+33), module, ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_PARAM+i);
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(factorNumeratorParam)->percentage = &module->factorsNumeratorPercentage[i];
			}
			addParam(factorNumeratorParam);							
    		addInput(createInput<FWPortInReallySmall>(Vec(106, i*34.5 + 36), module, ProbablyNoteMN::FACTOR_NUMERATOR_STEP_1_INPUT+i));
            addParam(createParam<RoundExtremelySmallFWKnob>(Vec(122,i*34.5+35), module, ProbablyNoteMN::FACTOR_NUMERATOR_1_STEP_CV_ATTENUVERTER_PARAM+i));

            ParamWidget* factorDivisorParam = createParam<RoundReallySmallFWSnapKnob>(Vec(154,i*34.5+33), module, ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_PARAM+i);
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(factorDivisorParam)->percentage = &module->factorsDivisorPercentage[i];
			}
			addParam(factorDivisorParam);							
    		addInput(createInput<FWPortInReallySmall>(Vec(178, i*34.44 + 36), module, ProbablyNoteMN::FACTOR_DENOMINATOR_STEP_1_INPUT+i));
            addParam(createParam<RoundExtremelySmallFWKnob>(Vec(194,i*34.5+35), module, ProbablyNoteMN::FACTOR_DENOMINATOR_1_STEP_CV_ATTENUVERTER_PARAM+i));

			ParamWidget* factorTempermentParam = createParam<RoundReallySmallFWKnob>(Vec(224,i*34.5+33), module, ProbablyNoteMN::FACTOR_TEMPER_1_STEP_PARAM+i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(factorTempermentParam)->percentage = &module->factorsTemperingPercentage[i];
				dynamic_cast<RoundReallySmallFWKnob*>(factorTempermentParam)->biDirectional = true;
			}
			addParam(factorTempermentParam);							
    		addInput(createInput<FWPortInReallySmall>(Vec(248, i*34.44 + 36), module, ProbablyNoteMN::FACTOR_TEMPER_STEP_1_INPUT+i));
            addParam(createParam<RoundExtremelySmallFWKnob>(Vec(264,i*34.5+35), module, ProbablyNoteMN::FACTOR_TEMPER_1_STEP_CV_ATTENUVERTER_PARAM+i));

        }   


//ED
		edoParam = createParam<RoundReallySmallFWSnapKnob>(Vec(304,53), module, ProbablyNoteMN::EQUAL_DIVISION_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(edoParam)->percentage = &module->edoPercentage;
		}
		addParam(edoParam);							
        esoParam = createParam<RoundReallySmallFWKnob>(Vec(304,53), module, ProbablyNoteMN::EQUAL_STEP_SIZE_PARAM);
		addParam(esoParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(326,70), module, ProbablyNoteMN::EQUAL_DIVISION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(328, 55), module, ProbablyNoteMN::EQUAL_DIVISION_INPUT));

		addParam(createParam<LEDButton>(Vec(306, 80), module, ProbablyNoteMN::EQUAL_DIVISION_MODE_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(307.5, 81.5), module, ProbablyNoteMN::EDO_MODE_LIGHT));


		ParamWidget* edoStepsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(361,53), module, ProbablyNoteMN::EDO_STEPS_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(edoStepsParam)->percentage = &module->edoStepsPercentage;
		}
		addParam(edoStepsParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(383,70), module, ProbablyNoteMN::EDO_STEPS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(385, 55), module, ProbablyNoteMN::EDO_STEPS_INPUT));

		ParamWidget* edoWrapsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(418,53), module, ProbablyNoteMN::EDO_WRAPS_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(edoWrapsParam)->percentage = &module->edoWrapsPercentage;
		}
		addParam(edoWrapsParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(440,70), module, ProbablyNoteMN::EDO_WRAPS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(442, 55), module, ProbablyNoteMN::EDO_WRAPS_INPUT));

//MOS

//This needs more research!
		// addParam(createParam<LEDButton>(Vec(297, 162), module, ProbablyNoteMN::MOS_MODE_PARAM));
		// addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(298.5, 163.5), module, ProbablyNoteMN::MOS_MODE_LIGHT));

		ParamWidget* mosLargeStepsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(295,135), module, ProbablyNoteMN::MOS_LARGE_STEPS_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(mosLargeStepsParam)->percentage = &module->mosLargeStepsPercentage;
		}
		addParam(mosLargeStepsParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(317,152), module, ProbablyNoteMN::MOS_LARGE_STEPS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(319, 138), module, ProbablyNoteMN::MOS_LARGE_STEPS_INPUT));

		ParamWidget* mosSmallStepsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(341,135), module, ProbablyNoteMN::MOS_SMALL_STEPS_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(mosSmallStepsParam)->percentage = &module->mosSmallStepsPercentage;
		}
		addParam(mosSmallStepsParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(363,152), module, ProbablyNoteMN::MOS_SMALL_STEPS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(365, 138), module, ProbablyNoteMN::MOS_SMALL_STEPS_INPUT));

		ParamWidget* mosRatioParam = createParam<RoundReallySmallFWKnob>(Vec(387,135), module, ProbablyNoteMN::MOS_RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(mosRatioParam)->percentage = &module->mosRatioPercentage;
		}
		addParam(mosRatioParam);								
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(409,152), module, ProbablyNoteMN::MOS_RATIO_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(411, 138), module, ProbablyNoteMN::MOS_RATIO_INPUT));

		addParam(createParam<LEDButton>(Vec(390, 159), module, ProbablyNoteMN::QUANTIZE_MOS_RATIO_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(391.5, 160.5), module, ProbablyNoteMN::QUANTIZE_MOS_RATIO_LIGHT));

		ParamWidget* mosLevelsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(433,135), module, ProbablyNoteMN::MOS_LEVELS_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(mosLevelsParam)->percentage = &module->mosLevelsPercentage;
		}
		addParam(mosLevelsParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(455,152), module, ProbablyNoteMN::MOS_LEVELS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(457, 138), module, ProbablyNoteMN::MOS_LEVELS_INPUT));


//Note reduction
		ParamWidget* numberOfNotesParam = createParam<RoundReallySmallFWSnapKnob>(Vec(342,205), module, ProbablyNoteMN::NUMBER_OF_NOTES_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(numberOfNotesParam)->percentage = &module->numberOfNotesPercentage;
		}
		addParam(numberOfNotesParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(366,222), module, ProbablyNoteMN::NUMBER_OF_NOTES_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(368, 208), module, ProbablyNoteMN::NUMBER_OF_NOTES_INPUT));

		addParam(createParam<LEDButton>(Vec(412, 205), module, ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(413.5, 206.5), module, ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(434, 208), module, ProbablyNoteMN::NOTE_REDUCTION_ALGORITHM_INPUT));

//Tempering
		addParam(createParam<LEDButton>(Vec(314, 265), module, ProbablyNoteMN::EDO_TEMPERING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(315.5, 266.5), module, ProbablyNoteMN::EDO_TEMPERING_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(336, 268), module, ProbablyNoteMN::EDO_TEMPERING_INPUT));

        ParamWidget* edoTemperingThresholdParam = createParam<RoundReallySmallFWKnob>(Vec(375,265), module, ProbablyNoteMN::EDO_TEMPERING_THRESHOLD_PARAM);
		if (module) {
			 dynamic_cast<RoundReallySmallFWKnob*>(edoTemperingThresholdParam)->percentage = &module->edoTemperingThresholdPercentage;
		}
        addParam(edoTemperingThresholdParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(397,281), module, ProbablyNoteMN::EDO_TEMPERING_THRESHOLD_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(399, 267), module, ProbablyNoteMN::EDO_TEMPERING_THRESHOLD_INPUT));

		ParamWidget* edoTemperingStrengthParam = createParam<RoundReallySmallFWKnob>(Vec(434,265), module, ProbablyNoteMN::EDO_TEMPERING_STRENGTH_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(edoTemperingStrengthParam)->percentage = &module->edoTemperingStrengthPercentage;
		}
		addParam(edoTemperingStrengthParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(456,281), module, ProbablyNoteMN::EDO_TEMPERING_STRENGTH_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(458, 267), module, ProbablyNoteMN::EDO_TEMPERING_STRENGTH_INPUT));

// Map to normal scale
		addParam(createParam<LEDButton>(Vec(340, 323), module, ProbablyNoteMN::SCALE_MAPPING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(341.5, 324.5), module, ProbablyNoteMN::SCALE_MAPPING_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(364, 326), module, ProbablyNoteMN::SCALE_MAPPING_INPUT));


        ParamWidget* scaleParam = createParam<RoundReallySmallFWSnapKnob>(Vec(414,324), module, ProbablyNoteMN::SCALE_PARAM);
		if (module) {
			dynamic_cast<RoundReallySmallFWSnapKnob*>(scaleParam)->percentage = &module->scalePercentage;
		}
		addParam(scaleParam);							
        addParam(createParam<RoundExtremelySmallFWKnob>(Vec(438,341), module, ProbablyNoteMN::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInReallySmall>(Vec(440, 327), module, ProbablyNoteMN::SCALE_INPUT));

		addParam(createParam<LEDButton>(Vec(340, 343), module, ProbablyNoteMN::USE_SCALE_WEIGHTING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(341.5, 344.5), module, ProbablyNoteMN::SCALE_WEIGHTING_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(364, 346), module, ProbablyNoteMN::USE_SCALE_WEIGHTING_INPUT));

      

		addOutput(createOutput<FWPortInSmall>(Vec(700, 345),  module, ProbablyNoteMN::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(667, 345),  module, ProbablyNoteMN::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(629, 345),  module, ProbablyNoteMN::NOTE_CHANGE_OUTPUT));


		{
			ProbablyNoteMNDisplay *display = new ProbablyNoteMNDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

	}

	void step() override {
		if (module) {
			if((((ProbablyNoteMN*)module)->equalDivisionMode) == 0) {
				edoParam->visible = true;
				esoParam->visible = false;
			} else {
				edoParam->visible = false;
				esoParam->visible = true;
			}
		}
		Widget::step();
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

	struct PNMNSaveScaletem : MenuItem {
		ProbablyNoteMN *module ;
		void onAction(const event::Action &e) override {
			
			osdialog_filters* filters = osdialog_filters_parse("Scale:scl");
			char *filename  = osdialog_file(OSDIALOG_SAVE, NULL, NULL, filters);        //////////dir.c_str(),
			if (filename) {

				char *dot = strrchr(filename,'.');
				if(dot == 0 || strcmp(dot,".scl") != 0)
				strcat(filename,".scl");

				module->CreateScalaFile(filename);
				free(filename);
			}
			osdialog_filters_free(filters);
		}
	};

	
	
	void appendContextMenu(Menu *menu) override {
		// MenuLabel *spacerLabel = new MenuLabel();
		// menu->addChild(spacerLabel);

		ProbablyNoteMN *module = dynamic_cast<ProbablyNoteMN*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());
		{
      		OptionsMenuItem* qmi = new OptionsMenuItem("Quantize Mode");
			qmi->addItem(OptionMenuItem("Closet", [module]() { return module->quantizeMode == 0; }, [module]() { module->quantizeMode = 0; }));
			qmi->addItem(OptionMenuItem("Round Lower", [module]() { return module->quantizeMode == 1; }, [module]() { module->quantizeMode = 1; }));
			qmi->addItem(OptionMenuItem("Round Upper", [module]() { return module->quantizeMode == 2; }, [module]() { module->quantizeMode = 2; }));
			menu->addChild(qmi);			
		}

		TriggerDelayItem *triggerDelayItem = new TriggerDelayItem();
		triggerDelayItem->module = module;
		menu->addChild(triggerDelayItem);

		menu->addChild(new MenuLabel());
		{
      		OptionsMenuItem* mi = new OptionsMenuItem("Pitch Grid");
			mi->addItem(OptionMenuItem("Off", [module]() { return module->pitchGridDisplayMode == 0; }, [module]() { module->pitchGridDisplayMode = 0; }));
			mi->addItem(OptionMenuItem("100¢", [module]() { return module->pitchGridDisplayMode == 1; }, [module]() { module->pitchGridDisplayMode = 1; }));
			mi->addItem(OptionMenuItem("Major Ratios", [module]() { return module->pitchGridDisplayMode == 2; }, [module]() { module->pitchGridDisplayMode = 2; }));
			mi->addItem(OptionMenuItem("Minor Ratios", [module]() { return module->pitchGridDisplayMode == 3; }, [module]() { module->pitchGridDisplayMode = 3; }));
			menu->addChild(mi);
		}



		PNMNSaveScaletem *saveScaleItem = new PNMNSaveScaletem();
		saveScaleItem->module = module;
		saveScaleItem->text = "Save as Scale File";
		menu->addChild(saveScaleItem);

	}
	
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteMN = createModel<ProbablyNoteMN, ProbablyNoteMNWidget>("ProbablyNoteMN");