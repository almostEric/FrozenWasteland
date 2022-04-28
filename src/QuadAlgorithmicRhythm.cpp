#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FrozenWasteland.hpp"
#include "model/ChristoffelWords.hpp"
#include "ui/menu.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/buttons.hpp"
#include "dsp-noise/noise.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 73
#define NUM_ALGORITHMS 7
#define EXPANDER_MAX_STEPS 18
//GOLOMB RULER
#define NUM_RULERS 20 
#define MAX_DIVISIONS 11
//PERFECT BALANCE
#define NUM_PB_PATTERNS 115
#define MAX_PB_SIZE 73

#define NBR_SCENES 8

#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define PASSTHROUGH_OFFSET EXPANDER_MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT

using namespace frozenwasteland::dsp;

struct QuadAlgorithmicRhythm : Module {
	enum ParamIds {
		STEPS_1_PARAM,
		DIVISIONS_1_PARAM,
		OFFSET_1_PARAM,
		PAD_1_PARAM,
		ACCENTS_1_PARAM,
		ACCENT_ROTATE_1_PARAM,
		ALGORITHM_1_PARAM,
		TRACK_1_INDEPENDENT_PARAM,
		STEPS_2_PARAM,
		DIVISIONS_2_PARAM,
		OFFSET_2_PARAM,
		PAD_2_PARAM,
		ACCENTS_2_PARAM,
		ACCENT_ROTATE_2_PARAM,
		ALGORITHM_2_PARAM,
		TRACK_2_INDEPENDENT_PARAM,
		STEPS_3_PARAM,
		DIVISIONS_3_PARAM,
		OFFSET_3_PARAM,
		PAD_3_PARAM,
		ACCENTS_3_PARAM,
		ACCENT_ROTATE_3_PARAM,
		ALGORITHM_3_PARAM,
		TRACK_3_INDEPENDENT_PARAM,
		STEPS_4_PARAM,
		DIVISIONS_4_PARAM,
		OFFSET_4_PARAM,
		PAD_4_PARAM,
		ACCENTS_4_PARAM,
		ACCENT_ROTATE_4_PARAM,
		ALGORITHM_4_PARAM,
		TRACK_4_INDEPENDENT_PARAM,
		ACCENT_ALGORITHM_1_PARAM,
		META_STEP_PARAM = ACCENT_ALGORITHM_1_PARAM + TRACK_COUNT,
		CHAIN_MODE_PARAM,	
		CONSTANT_TIME_MODE_PARAM = CHAIN_MODE_PARAM + 3,	
		RESET_PARAM,
		MUTE_PARAM,
		CHOOSE_SCENE_PARAM,
		SAVE_SCENE_PARAM = CHOOSE_SCENE_PARAM + NBR_SCENES,
		RUN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		STEPS_1_INPUT,
		DIVISIONS_1_INPUT,
		OFFSET_1_INPUT,
		PAD_1_INPUT,
		ACCENTS_1_INPUT,
		ACCENT_ROTATE_1_INPUT,
        ALGORITHM_1_INPUT,
		START_1_INPUT,
		STEPS_2_INPUT,
		DIVISIONS_2_INPUT,
		OFFSET_2_INPUT,
		PAD_2_INPUT,
		ACCENTS_2_INPUT,
		ACCENT_ROTATE_2_INPUT,
        ALGORITHM_2_INPUT,
		START_2_INPUT,
		STEPS_3_INPUT,
		DIVISIONS_3_INPUT,
		OFFSET_3_INPUT,
		PAD_3_INPUT,
		ACCENTS_3_INPUT,
		ACCENT_ROTATE_3_INPUT,
        ALGORITHM_3_INPUT,
		START_3_INPUT,
		STEPS_4_INPUT,
		DIVISIONS_4_INPUT,
		OFFSET_4_INPUT,
		PAD_4_INPUT,
		ACCENTS_4_INPUT,
		ACCENT_ROTATE_4_INPUT,
        ALGORITHM_4_INPUT,
		START_4_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		MUTE_INPUT,
		META_STEP_INPUT,
		CHOOSE_SCENE_INPUT,
		BPM_INPUT,
		RUN_INPUT,
		ACCENT_ALGORITHM_1_INPUT,
		NUM_INPUTS = ACCENT_ALGORITHM_1_INPUT + TRACK_COUNT
	};
	enum OutputIds {
		OUTPUT_1,
		ACCENT_OUTPUT_1,
		EOC_OUTPUT_1,
		OUTPUT_2,
		ACCENT_OUTPUT_2,
		EOC_OUTPUT_2,
		OUTPUT_3,
		ACCENT_OUTPUT_3,
		EOC_OUTPUT_3,
		OUTPUT_4,
		ACCENT_OUTPUT_4,
		EOC_OUTPUT_4,
		GROOVE_EOC_OUTPUT_1,
		TEST_OUTPUT = GROOVE_EOC_OUTPUT_1 + TRACK_COUNT,
		NUM_OUTPUTS
	};
	enum LightIds {
        ALGORITHM_1_LIGHT,
        ALGORITHM_2_LIGHT = ALGORITHM_1_LIGHT+3,
        ALGORITHM_3_LIGHT = ALGORITHM_2_LIGHT+3,
        ALGORITHM_4_LIGHT = ALGORITHM_3_LIGHT+3,
        ACCENT_ALGORITHM_1_LIGHT = ALGORITHM_4_LIGHT+3,
        ACCENT_ALGORITHM_2_LIGHT = ACCENT_ALGORITHM_1_LIGHT+3,
        ACCENT_ALGORITHM_3_LIGHT = ACCENT_ALGORITHM_2_LIGHT+3,
        ACCENT_ALGORITHM_4_LIGHT = ACCENT_ALGORITHM_3_LIGHT+3,
        TRACK_INDEPENDENT_1_LIGHT = ACCENT_ALGORITHM_4_LIGHT+3,
        TRACK_INDEPENDENT_2_LIGHT = TRACK_INDEPENDENT_1_LIGHT+3,
        TRACK_INDEPENDENT_3_LIGHT = TRACK_INDEPENDENT_2_LIGHT+3,
        TRACK_INDEPENDENT_4_LIGHT = TRACK_INDEPENDENT_3_LIGHT+3,
		CHAIN_MODE_NONE_LIGHT = TRACK_INDEPENDENT_4_LIGHT+3,
		CHAIN_MODE_BOSS_LIGHT,
		CHAIN_MODE_EMPLOYEE_LIGHT,
		MUTED_LIGHT,
		CONSTANT_TIME_LIGHT,
		WAITING_1_LIGHT,
		WAITING_2_LIGHT,
		WAITING_3_LIGHT,
		WAITING_4_LIGHT,
		SCENE_ACTIVE_LIGHT,
		SAVE_MODE_LIGHT = SCENE_ACTIVE_LIGHT+NBR_SCENES,
		MANUAL_RUN_LIGHT,
		NUM_LIGHTS = MANUAL_RUN_LIGHT + 3
	};
	enum ChainModes {
		CHAIN_MODE_NONE,
		CHAIN_MODE_BOSS,
		CHAIN_MODE_EMPLOYEE
	};
	enum Algorithms {
		EUCLIDEAN_ALGO,
		GOLUMB_RULER_ALGO,
		WELL_FORMED_ALGO,
		PERFECT_BALANCE_ALGO,
		MANUAL_MODE_ALGO,
		FIBONACCI_MODE_ALGO,
		BOOLEAN_LOGIC_ALGO
	};
	enum ProbabilityGroupTriggerModes {
		NONE_PGTM,
		GROUP_MODE_PGTM,
	};
	enum ProbabilityGroupTriggeredStatus {
		PENDING_PGTS,
		TRIGGERED_PGTS,
		NOT_TRIGGERED_PGTS
	};

	double bpm = 0;
	bool bpmX4 = false;
	bool usingBpm = false;
	bool manualRun = false;

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	
    int algorithmMatrix[TRACK_COUNT];
    int accentAlgorithmMatrix[TRACK_COUNT];
	bool trackIndependent[TRACK_COUNT] = {false};
	bool beatMatrix[TRACK_COUNT][MAX_STEPS];
	bool accentMatrix[TRACK_COUNT][MAX_STEPS];

	std::string trackPatternName[TRACK_COUNT] = {""};
	std::string accentPatternName[TRACK_COUNT] = {""};

	float probabilityMatrix[TRACK_COUNT][MAX_STEPS];
	double swingMatrix[TRACK_COUNT][MAX_STEPS];
	double beatWarpMatrix[TRACK_COUNT][MAX_STEPS];
	float beatRotateMatrix[TRACK_COUNT][MAX_STEPS];
	float irrationalRhythmMatrix[TRACK_COUNT][MAX_STEPS];
	double totalIrrationalAdjustment[TRACK_COUNT];
	float probabilityGroupModeMatrix[TRACK_COUNT][MAX_STEPS];
	int probabilityGroupTriggered[TRACK_COUNT];
	int probabilityGroupFirstStep[TRACK_COUNT];
	float workingProbabilityMatrix[TRACK_COUNT][MAX_STEPS];
	double workingSwingMatrix[TRACK_COUNT][MAX_STEPS];
	double workingBeatWarpMatrix[TRACK_COUNT][MAX_STEPS];
	float workingBeatRotateMatrix[TRACK_COUNT][MAX_STEPS];
	double workingIrrationalRhythmMatrix[TRACK_COUNT][MAX_STEPS];
	float workingConditionalMatrix[TRACK_COUNT][MAX_STEPS];
	int conditionalMatrix[TRACK_COUNT][MAX_STEPS];
	bool workingConditionaModeMatrix[TRACK_COUNT][MAX_STEPS];
	bool conditionalModeMatrix[TRACK_COUNT][MAX_STEPS] = {{0}};
	int conditionalCounterMatrix[TRACK_COUNT][MAX_STEPS] = {{0}};

	float lastProbabilityGroupModeMatrix[TRACK_COUNT][MAX_STEPS];
	float lastProbabilityMatrix[TRACK_COUNT][MAX_STEPS];
	float lastConditionalMatrix[TRACK_COUNT][MAX_STEPS];
	bool lastConditionalModeMatrix[TRACK_COUNT][MAX_STEPS];


	int beatIndex[TRACK_COUNT];
	int beatLocation[TRACK_COUNT][MAX_STEPS] = {{0}};
	int beatCount[TRACK_COUNT] = {0};
	int beatCountAtIndex[TRACK_COUNT] = {-1};
	int stepsCount[TRACK_COUNT];
	int lastStepsCount[TRACK_COUNT];
	double stepDuration[TRACK_COUNT];
	double lastStepTime[TRACK_COUNT];	
    double lastSwingDuration[TRACK_COUNT];

	bool expanderDataChanged[TRACK_COUNT] = {0};
	bool expanderRecalcNeeded[TRACK_COUNT] = {0};

//Stuff for Advanced Rhythms
	int lastAlgorithmSetting[TRACK_COUNT] = {0};
	int lastAccentAlgorithmSetting[TRACK_COUNT] = {0};
	int lastStepsSetting[TRACK_COUNT] = {0};
	int lastDivisionsSetting[TRACK_COUNT] = {0};
	int lastOffsetSetting[TRACK_COUNT] = {0};
	int lastPadSetting[TRACK_COUNT] = {0};
	int lastAccentSetting[TRACK_COUNT] = {0};
	int lastAccentRotationSetting[TRACK_COUNT] = {0};
	float lastExtraParameterValue[TRACK_COUNT] = {0};
	bool lastwfHierarchicalValue[TRACK_COUNT] = {0};
	int lastwfComplementValue[TRACK_COUNT] = {0};

	int currentDivisionsSetting[TRACK_COUNT] = {0};
	float extraParameterValue[TRACK_COUNT] = {0}; //r for Well Formed, evenness for Perfect Balanced
	int wellFormedParentTrack[TRACK_COUNT] = {0};
	double wellFormedStepDurations[TRACK_COUNT][MAX_STEPS] = {{0}};
	double wellFormedTrackDuration[TRACK_COUNT] = {0};
	bool wellFormedHierchical[TRACK_COUNT] = {0};
	int wellFormedComplement[TRACK_COUNT] = {0};


	uint16_t manualBeatMatrix[TRACK_COUNT][5] = {{0}}; //5 should hold 84 beats, so good for our max of 73
	uint16_t manualAccentMatrix[TRACK_COUNT][5] = {{0}}; //5 should hold 84 beats, so good for our max of 73

	bool dirty[TRACK_COUNT] = {true};
	
	ChristoffelWords christoffelWords;
	std::string currentChristoffelword[TRACK_COUNT] = {"unknown"};


	float swingRandomness[TRACK_COUNT];
	bool useGaussianDistribution[TRACK_COUNT] = {false};
	double calculatedSwingRandomness[TRACK_COUNT] = {0.0f};
	bool trackSwingUsingDivs[TRACK_COUNT] = {false};
	int subBeatLength[TRACK_COUNT];
	int subBeatIndex[TRACK_COUNT];


	float beatWarping[TRACK_COUNT];
	int beatWarpingPosition[TRACK_COUNT];
	int beatWarpingLength[TRACK_COUNT];
	

	float beatRotatingAmount[TRACK_COUNT];

	float expanderOutputValue[TRACK_COUNT];
	float expanderAccentValue[TRACK_COUNT];
	float expanderEocValue[TRACK_COUNT];
	float lastExpanderEocValue[TRACK_COUNT];
	
	float expanderClockValue = 0;
	float expanderResetValue = 0;
	float expanderMuteValue = 0;

	double maxStepCount;
	double masterStepCount;
	double metaStepCount;
	bool slaveQARsPresent;
	bool masterQARPresent;

	int currentScene = 0;
	int currentCVScene = 0;
	int lastScene = 0;
	int lastCVScene = 0;
	bool saveMode = false;
	float sceneData[NBR_SCENES][79] = {{0}};
	int sceneChangeMessage = 0;

	bool hardReset = false;
	bool resetTriggerisHardReset = false;
	int modsKeys = 0;

	//percentages
	float stepsPercentage[TRACK_COUNT] = {0};
	float divisionsPercentage[TRACK_COUNT] = {0};
	float offsetPercentage[TRACK_COUNT] = {0};
	float padPercentage[TRACK_COUNT] = {0};
	float accentDivisionsPercentage[TRACK_COUNT] = {0};
	float accentRotatePercentage[TRACK_COUNT] = {0};
	float metaStepCountPercentage = 0;


	//GOLOMB RULER PATTERNS
    const int rulerOrders[NUM_RULERS] = {1,2,3,4,5,5,6,6,6,6,7,7,7,7,7,8,9,10,11,11};
	const int rulerLengths[NUM_RULERS] = {0,1,3,6,11,11,17,17,17,17,25,25,25,25,25,34,44,55,72,72};
	const std::string rulerNames[NUM_RULERS] = {"1","2","3","4","5a","5b","6a","6b","6c","6d","7a","7b","7c","7d","7e","8","9","10","11a","11b"};
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
												   {0,1,9,19,24,31,52,56,58,69,72}};



 
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


	const std::string booleanOperationNames[6] = {"AND","OR","XOR","NAND","NOR","IMP"};


	bool running[TRACK_COUNT];
	int chainMode = 0;
	bool initialized = false;
	bool muted = false;
	bool constantTime = false;
	int masterTrack = 0;
	bool QARExpanderDisconnectReset = true;

	double timeElapsed = 0.0;
	double duration = 0.0;
	bool firstClockReceived = false;
	bool secondClockReceived = false;

	dsp::SchmittTrigger clockTrigger,resetKeyTrigger,resetInputTrigger,chainModeTrigger[3],constantTimeTrigger,muteTrigger,
						algorithmTrigger[TRACK_COUNT],accentAlgorithmTrigger[TRACK_COUNT],trackIndependentTrigger[TRACK_COUNT],startTrigger[TRACK_COUNT],
						chooseSceneTrigger[NBR_SCENES],saveSceneTrigger,manualRunTrigger;
	dsp::PulseGenerator beatPulse[TRACK_COUNT],accentPulse[TRACK_COUNT],eocPulse[TRACK_COUNT],grooveEocPulse[TRACK_COUNT];

	GaussianNoiseGenerator _gauss;

	float saveBlinkDurationCounter = 0;
	bool saveBlinker = false;


	QuadAlgorithmicRhythm() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

	
		configParam(STEPS_1_PARAM, 1.0, MAX_STEPS, 16.0,"Track 1 Steps");
		configParam(DIVISIONS_1_PARAM, 0.0, MAX_STEPS, 2.0,"Track 1 Beats");
		configParam(OFFSET_1_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 1 Step Offset");
		configParam(PAD_1_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 1 Step Padding");
		configParam(ACCENTS_1_PARAM, 0.0, MAX_STEPS, 0.0, "Track 1 Accents");
		configParam(ACCENT_ROTATE_1_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 1 Acccent Rotation");
        configParam(ALGORITHM_1_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_2_PARAM, 1.0, MAX_STEPS, 16.0,"Track 2 Steps");
		configParam(DIVISIONS_2_PARAM, 0.0, MAX_STEPS, 2.0,"Track 2 Beats");
		configParam(OFFSET_2_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 2 Step Offset");
		configParam(PAD_2_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 2 Step Padding");
		configParam(ACCENTS_2_PARAM, 0.0, MAX_STEPS, 0.0, "Track 2 Accents");
		configParam(ACCENT_ROTATE_2_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 2 Acccent Rotation");
        configParam(ALGORITHM_2_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_3_PARAM, 1.0, MAX_STEPS, 16.0,"Track 3 Steps");
		configParam(DIVISIONS_3_PARAM, 0.0, MAX_STEPS, 2.0,"Track 3 Beats");
		configParam(OFFSET_3_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 3 Step Offset");
		configParam(PAD_3_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 3 Step Padding");
		configParam(ACCENTS_3_PARAM, 0.0, MAX_STEPS, 0.0, "Track 3 Accents");
		configParam(ACCENT_ROTATE_3_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 3 Acccent Rotation");
        configParam(ALGORITHM_3_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_4_PARAM, 1.0, MAX_STEPS, 16.0,"Track 4 Steps");
		configParam(DIVISIONS_4_PARAM, 0.0, MAX_STEPS, 2.0,"Track 4 Beats");
		configParam(OFFSET_4_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 4 Step Offset");
		configParam(PAD_4_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 4 Step Padding");
		configParam(ACCENTS_4_PARAM, 0.0, MAX_STEPS, 0.0, "Track 4 Accents");
		configParam(ACCENT_ROTATE_4_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 4 Acccent Rotation");
        configParam(ALGORITHM_4_PARAM, 0.0, 1.0, 0.0);


        configParam(META_STEP_PARAM, 1.0, 360.0, 16.0,"Meta Step Count");

		configParam(CHAIN_MODE_PARAM, 0.0, 1.0, 0.0);
		configParam(CHAIN_MODE_PARAM+1, 0.0, 1.0, 0.0);
		configParam(CHAIN_MODE_PARAM+2, 0.0, 1.0, 0.0);
		configParam(CONSTANT_TIME_MODE_PARAM, 0.0, 1.0, 0.0);

		configParam(RESET_PARAM, 0.0, 1.0, 0.0);
		configParam(MUTE_PARAM, 0.0, 1.0, 0.0);


		configButton(META_STEP_PARAM,"Meta Track Step Count");
		configButton(CHAIN_MODE_PARAM,"Chain Mode - None");
		configButton(CHAIN_MODE_PARAM+1,"Chain Mode - Boss");
		configButton(CHAIN_MODE_PARAM+2,"Chain Mode - Employee");
		configButton(CONSTANT_TIME_MODE_PARAM,"Time Sync Tracks");
		configButton(RESET_PARAM,"Reset");
		configButton(MUTE_PARAM,"Mute");
		for(int i=0;i<NBR_SCENES;i++) {
			configButton(CHOOSE_SCENE_PARAM+i,"Select Scene " + std::to_string(i+1));
		}
		for(int i=0;i<TRACK_COUNT;i++) {
			configButton(ALGORITHM_1_PARAM+(i*8),"Track " + std::to_string(i+1) + " Algorithm");
			configButton(ACCENT_ALGORITHM_1_PARAM+i,"Track " + std::to_string(i+1) + " Accent Algorithm");
			configButton(TRACK_1_INDEPENDENT_PARAM+(i*8),"Track " + std::to_string(i+1) + " Free Running");
		}

		configButton(SAVE_SCENE_PARAM,"Save Scene");
		configButton(RUN_PARAM,"Run");



		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(MUTE_INPUT, "Mute");
		configInput(META_STEP_INPUT, "Meta Track # of Steps");
		configInput(CHOOSE_SCENE_INPUT, "Scene #");
		configInput(BPM_INPUT, "BPM");
		configInput(RUN_INPUT, "Run");

		configInput(STEPS_1_INPUT, "Track 1 # of Steps");
		configInput(DIVISIONS_1_INPUT, "Track 1 # of Beats");
		configInput(OFFSET_1_INPUT, "Track 1 Offset");
		configInput(PAD_1_INPUT, "Track 1 Padding");
		configInput(ACCENTS_1_INPUT, "Track 1 Accents");
		configInput(ACCENT_ROTATE_1_INPUT, "Track 1 Accent Rotate");
		configInput(ALGORITHM_1_INPUT, "Track 1 Algorithm");
		configInput(ACCENT_ALGORITHM_1_INPUT, "Track 1 Accent Algorithm");

		configOutput(OUTPUT_1, "Track 1 Beat");
		configOutput(ACCENT_OUTPUT_1, "Track 1 Accent");
		configOutput(EOC_OUTPUT_1, "Track 1 EOC");
		configOutput(GROOVE_EOC_OUTPUT_1, "Track 1 Groove EOC");

		configInput(STEPS_2_INPUT, "Track 2 # of Steps");
		configInput(DIVISIONS_2_INPUT, "Track 2 # of Beats");
		configInput(OFFSET_2_INPUT, "Track 2 Offset");
		configInput(PAD_2_INPUT, "Track 2 Padding");
		configInput(ACCENTS_2_INPUT, "Track 2 Accents");
		configInput(ACCENT_ROTATE_2_INPUT, "Track 2 Accent Rotate");
		configInput(ALGORITHM_2_INPUT, "Track 2 Algorithm");
		configInput(ACCENT_ALGORITHM_1_INPUT+1, "Track 2 Accent Algorithm");

		configOutput(OUTPUT_2, "Track 2 Beat");
		configOutput(ACCENT_OUTPUT_2, "Track 2 Accent");
		configOutput(EOC_OUTPUT_2, "Track 2 EOC");
		configOutput(GROOVE_EOC_OUTPUT_1+1, "Track 2 Groove EOC");

		configInput(STEPS_3_INPUT, "Track 3 # of Steps");
		configInput(DIVISIONS_3_INPUT, "Track 3 # of Beats");
		configInput(OFFSET_3_INPUT, "Track 3 Offset");
		configInput(PAD_3_INPUT, "Track 3 Padding");
		configInput(ACCENTS_3_INPUT, "Track 3 Accents");
		configInput(ACCENT_ROTATE_3_INPUT, "Track 3 Accent Rotate");
		configInput(ALGORITHM_3_INPUT, "Track 3 Algorithm");
		configInput(ACCENT_ALGORITHM_1_INPUT+2, "Track 3 Accent Algorithm");

		configOutput(OUTPUT_3, "Track 3 Beat");
		configOutput(ACCENT_OUTPUT_3, "Track 3 Accent");
		configOutput(EOC_OUTPUT_3, "Track 3 EOC");
		configOutput(GROOVE_EOC_OUTPUT_1+2, "Track 3 Groove EOC");

		configInput(STEPS_4_INPUT, "Track 4 # of Steps");
		configInput(DIVISIONS_4_INPUT, "Track 4 # of Beats");
		configInput(OFFSET_4_INPUT, "Track 4 Offset");
		configInput(PAD_4_INPUT, "Track 4 Padding");
		configInput(ACCENTS_4_INPUT, "Track 4 Accents");
		configInput(ACCENT_ROTATE_4_INPUT, "Track 4 Accent Rotate");
		configInput(ALGORITHM_4_INPUT, "Track 4 Algorithm");
		configInput(ACCENT_ALGORITHM_1_INPUT+3, "Track 4 Accent Algorithm");

		configOutput(OUTPUT_4, "Track 4 Beat");
		configOutput(ACCENT_OUTPUT_4, "Track 4 Accent");
		configOutput(EOC_OUTPUT_4, "Track 4 EOC");
		configOutput(GROOVE_EOC_OUTPUT_1+3, "Track 4 Groove EOC");



		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
		
		srand(time(NULL));
		

		for(int i = 0; i < TRACK_COUNT; i++) {
            algorithmMatrix[i] = 0;
			accentAlgorithmMatrix[i] = 0;
			beatIndex[i] = -1;
			stepsCount[i] = MAX_STEPS;
			lastStepsCount[i] = -1;
			lastStepTime[i] = 0.0;
			stepDuration[i] = 0.0;
            lastSwingDuration[i] = 0.0;
			subBeatIndex[i] = -1;
			swingRandomness[i] = 0.0f;
			useGaussianDistribution[i] = false;	
			probabilityGroupTriggered[i] = PENDING_PGTS;
			beatWarping[i] = 1.0;
			beatWarpingPosition[i] = 8;
			beatWarpingLength[i] = 72;
			beatRotatingAmount[i] = 0.0;
			extraParameterValue[i] = 1.0;

			expanderOutputValue[i] = 0.0;
			expanderAccentValue[i] = 0.0;
			expanderEocValue[i] = 0.0;
			lastExpanderEocValue[i] = 0.0;
			
			totalIrrationalAdjustment[i] = 1.0;
			running[i] = true;
			for(int j = 0; j < MAX_STEPS; j++) {
				probabilityMatrix[i][j] = 1.0;
				swingMatrix[i][j] = 0.0;
				beatWarpMatrix[i][j] = 1.0;
				irrationalRhythmMatrix[i][j] = 1.0;
				beatMatrix[i][j] = false;
				accentMatrix[i][j] = false;				
			}
		}	

        onReset();	
	}


	void saveScene(int scene) {
		sceneData[scene][0] = true;
		sceneData[scene][1] = masterTrack;
		sceneData[scene][2] = params[META_STEP_PARAM].getValue();
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			sceneData[scene][trackNumber*19+3] = algorithmMatrix[trackNumber];
			sceneData[scene][trackNumber*19+4] = params[(trackNumber * 8) + STEPS_1_PARAM].getValue();
			sceneData[scene][trackNumber*19+5] = params[(trackNumber * 8) + DIVISIONS_1_PARAM].getValue();
			sceneData[scene][trackNumber*19+6] = params[(trackNumber * 8) + OFFSET_1_PARAM].getValue();
			sceneData[scene][trackNumber*19+7] = params[(trackNumber * 8) + PAD_1_PARAM].getValue();
			sceneData[scene][trackNumber*19+8] = params[(trackNumber * 8) + ACCENTS_1_PARAM].getValue();
			sceneData[scene][trackNumber*19+9] = params[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].getValue();
			sceneData[scene][trackNumber*19+10] = trackIndependent[trackNumber];
			sceneData[scene][trackNumber*19+11] = accentAlgorithmMatrix[trackNumber];
			for(int index=0;index<5;index++) {
				sceneData[scene][trackNumber*19+12+index] = manualBeatMatrix[trackNumber][index];
				sceneData[scene][trackNumber*19+17+index] = manualAccentMatrix[trackNumber][index];
			}
		}
	}

	bool loadScene(int scene) {
		if(sceneData[scene][0] != 0 ) {  //Don't load empty scenes
			masterTrack = sceneData[scene][1];
			constantTime = masterTrack > 0;
			params[META_STEP_PARAM].setValue(sceneData[scene][2]);
			for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
				algorithmMatrix[trackNumber] = sceneData[scene][trackNumber*19+3];
				params[(trackNumber * 8) + STEPS_1_PARAM].setValue(sceneData[scene][trackNumber*19+4]);
				params[(trackNumber * 8) + DIVISIONS_1_PARAM].setValue(sceneData[scene][trackNumber*19+5]);
				params[(trackNumber * 8) + OFFSET_1_PARAM].setValue(sceneData[scene][trackNumber*19+6]);
				params[(trackNumber * 8) + PAD_1_PARAM].setValue(sceneData[scene][trackNumber*19+7]);
				params[(trackNumber * 8) + ACCENTS_1_PARAM].setValue(sceneData[scene][trackNumber*19+8]);
				params[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].setValue(sceneData[scene][trackNumber*19+9]);
				trackIndependent[trackNumber] = sceneData[scene][trackNumber*19+10];
				accentAlgorithmMatrix[trackNumber] = sceneData[scene][trackNumber*19+11];
				for(int index=0;index<5;index++) {
					manualBeatMatrix[trackNumber][index] = sceneData[scene][trackNumber*19+12+index];
					manualAccentMatrix[trackNumber][index] = sceneData[scene][trackNumber*19+17+index];
				}
				dirty[trackNumber] = true;
			}
			return true;
		} else {
			return false;
		}
	}

	void toggleManualBeat(int trackNumber, int8_t stepNumber,bool accent) {
		// for(uint8_t i=0;i<stepsCount[i];i++) {
		// 	fprintf(stderr, "------- track:%i step:%i  beat:%u  accent:%u\n", trackNumber, i, getManualBeat(trackNumber,i),getManualAccent(trackNumber,i));
		// }

		if(accent) {
			// stepNumber -= lastOffsetSetting[trackNumber];
			// if(stepNumber < 0)
			// 	stepNumber += stepsCount[trackNumber];
			

			if(beatMatrix[trackNumber][stepNumber]) { // Only toggle if there is actualy a beat
				int bl = -1;
				for(int s=0;s<beatCount[trackNumber];s++) {
					if (beatLocation[trackNumber][s] == stepNumber) {
						bl = s;
						break;
					}
				}
				if(bl >= 0) {
					// fprintf(stderr, "changing accent on track:%i step:%u beat:%i rotation:%i \n", trackNumber, stepNumber,bl,lastAccentRotationSetting[trackNumber]);
					int accentNumber = (bl - lastAccentRotationSetting[trackNumber]);
					if(accentNumber < 0)
						accentNumber += beatCount[trackNumber];

					uint8_t index = accentNumber >> 2;
					uint8_t bitPosition = accentNumber & 0x0F;
					manualAccentMatrix[trackNumber][index]  ^= 1UL << bitPosition;
				}
			}
		} else {
			stepNumber -= lastOffsetSetting[trackNumber];
			if(stepNumber < 0)
				stepNumber += stepsCount[trackNumber];

			uint8_t index = stepNumber >> 2;
			uint8_t bitPosition = stepNumber & 0x0F;
			manualBeatMatrix[trackNumber][index]  ^= 1UL << bitPosition;
		// fprintf(stderr, "track:%i step:%u index:%u bit:%u  value:%u \n", trackNumber, stepNumber, index,bitPosition,manualBeatMatrix[trackNumber][index]);
		}

		dirty[trackNumber] = true;
	}

	bool getManualBeat(int trackNumber, uint8_t stepNumber) {
		uint8_t index = stepNumber >> 2;
		uint8_t bitPosition = stepNumber & 0x0F;

		return (manualBeatMatrix[trackNumber][index] >> bitPosition) & 1U;		
	}

	bool getManualAccent(int trackNumber, uint8_t stepNumber) {
		uint8_t index = stepNumber >> 2;
		uint8_t bitPosition = stepNumber & 0x0F;

		return (manualAccentMatrix[trackNumber][index] >> bitPosition) & 1U;		
	}

	void process(const ProcessArgs &args) override  {

		if(inputs[BPM_INPUT].isConnected()) {
			bpm = powf(2.0,clamp(inputs[BPM_INPUT].getVoltage(),-10.0f,10.0f)) * 120.0 * (bpmX4 ? 4 : 1);
			// fprintf(stderr, "%f %i\n", bpm, bpmX4);
			usingBpm = true;
		} else {
			usingBpm = false;	
		}
		if (manualRunTrigger.process(params[RUN_PARAM].getValue() +  inputs[RUN_INPUT].getVoltage())) {
            manualRun = !manualRun && usingBpm;
        }

		//Initialize
		for(int i = 0; i < TRACK_COUNT; i++) {
			expanderOutputValue[i] = 0; 
			expanderAccentValue[i] = 0; 
			lastExpanderEocValue[i] = 0;
		}
		//See if a slave is passing through an expander
		bool slavedQARPresent = false;
		bool rightExpanderPresent = (rightExpander.module 
		&& (rightExpander.module->model == modelQuadAlgorithmicRhythm || rightExpander.module->model == modelQARWellFormedRhythmExpander || 
			rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || 
			rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQARIrrationalityExpander ||
			rightExpander.module->model == modelQARConditionalExpander));
		if(rightExpanderPresent)
		{			
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;
			slavedQARPresent = messagesFromExpander[PASSTHROUGH_OFFSET];  // Slave QAR Exists flag

			if(slavedQARPresent) {			
				for(int i = 0; i < TRACK_COUNT; i++) {
					expanderOutputValue[i] = messagesFromExpander[PASSTHROUGH_OFFSET + 1 + i * 3] ; 
					expanderAccentValue[i] = messagesFromExpander[PASSTHROUGH_OFFSET + 1 + i * 3 + 1] ; 
					lastExpanderEocValue[i] = messagesFromExpander[PASSTHROUGH_OFFSET + 1 + i * 3 + 2] ; 					
				}
			}

			//Process Advanced Rhythms Stuff
			for(int i = 0; i < TRACK_COUNT; i++) {
				wellFormedHierchical[i] = messagesFromExpander[TRACK_COUNT * 2 + i];
				wellFormedComplement[i] = messagesFromExpander[TRACK_COUNT * 3 + i];
				if(!wellFormedHierchical[i]) {
					extraParameterValue[i] = messagesFromExpander[TRACK_COUNT + i];
				}
			}
		}
	
		
		// Initialize
		for(int i = 0; i < TRACK_COUNT; i++) {				
			expanderEocValue[i] = 0; 
		}
		expanderClockValue = 0; 
		expanderResetValue = 0; 
		expanderMuteValue = 0; 

		//See if a master is passing through an expander
		bool masterQARPresent = false;
		bool leftExpanderPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWarpedSpaceExpander || 
									leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander ||
									leftExpander.module->model == modelQARIrrationalityExpander || leftExpander.module->model == modelQARConditionalExpander));
		if(leftExpanderPresent)
		{			
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			sceneChangeMessage = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT+0]; 
			masterQARPresent = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT+1]; 
			if(masterQARPresent) {			
				expanderClockValue = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 2] ; 
				expanderResetValue = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 3] ; 
				expanderMuteValue = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 4] ; 

				for(int i = 0; i < TRACK_COUNT; i++) {				
					expanderEocValue[i] = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 5 + i] ; 
				}
			}
		}


		//Set startup state	
		if(!initialized) {
			setRunningState();
			initialized = true;
		}

		float resetInputValue = inputs[RESET_INPUT].getVoltage();
		if(!inputs[RESET_INPUT].isConnected() && masterQARPresent) {
			resetInputValue = expanderResetValue;
		}
		bool resetInput =  resetInputTrigger.process(resetInputValue);
		bool resetKey = resetKeyTrigger.process(params[RESET_PARAM].getValue());
		if( resetInput || resetKey) {

			
			hardReset = (resetKey &&  (modsKeys & GLFW_MOD_SHIFT)) || (resetInput && resetTriggerisHardReset);

			if(hardReset) { // With shift key down, do a full reset
				for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++)
				{
					beatIndex[trackNumber] = -1;
					lastStepTime[trackNumber] = 0;
					lastSwingDuration[trackNumber] = 0; // Not sure about this
					expanderEocValue[trackNumber] = 0; 
					lastExpanderEocValue[trackNumber] = 0;		
					subBeatIndex[trackNumber] = -1;
					swingRandomness[trackNumber] = 0.0f;
					useGaussianDistribution[trackNumber] = false;
					beatWarping[trackNumber] = 1.0;
					beatWarpingPosition[trackNumber] = 8;
					beatWarpingLength[trackNumber] = 72;
					extraParameterValue[trackNumber] = 1.0;
					for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
						conditionalCounterMatrix[trackNumber][stepNumber] = 0;
					}
				}
				timeElapsed = 0;
				firstClockReceived = false;
				secondClockReceived = false;
				duration = 0.0;				
				setRunningState();
			} else { //otherwise just reset beat for ratcheting
				for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++)
				{
					beatIndex[trackNumber] = -1;
					lastStepTime[trackNumber] = PTRDIFF_MAX; // this should force beat to fire immediately
				}
				timeElapsed = 0;
				duration = 0; 
				secondClockReceived = false;
			}
		}
		
		//Do scene stuff early so we can pass message along
		if(saveSceneTrigger.process(params[SAVE_SCENE_PARAM].getValue())) {
			saveMode = !saveMode;
		}
		//Look for scenes
		sceneChangeMessage = 0; //Reset message 
		for(int scene=0;scene<NBR_SCENES;scene++) {
			if(chooseSceneTrigger[scene].process(params[CHOOSE_SCENE_PARAM+scene].getValue())) {
				if(lastScene != scene || saveMode) {
					if(saveMode) {
						currentScene = scene;
						lastScene = currentScene;
						sceneChangeMessage = currentScene + 20; // +20 means save
						saveScene(currentScene);
					} else {
						if(loadScene(scene)) {
							currentScene = scene;
							sceneChangeMessage = currentScene + 10; // +10 means load
							lastScene = currentScene;
						};
					}
					saveMode = false;
				}
				break;
			}
		}

		if(inputs[CHOOSE_SCENE_INPUT].isConnected()) {
			currentCVScene = clamp(std::floor(inputs[CHOOSE_SCENE_INPUT].getVoltage() * 0.8),0.0,NBR_SCENES);
			if(currentCVScene != lastCVScene)  { ////A manual change will override CV (until cv changes)
				if(loadScene(currentCVScene)) {
					currentScene = currentCVScene;
					lastCVScene = currentCVScene;
					sceneChangeMessage = currentScene + 10; // +10 means load
				}
			}
		}

		saveBlinkDurationCounter +=1;
		if(saveBlinkDurationCounter > args.sampleRate) {
			saveBlinkDurationCounter = 0;
			saveBlinker = !saveBlinker;
		}
		for(int scene=0;scene<NBR_SCENES;scene++) {
			lights[SCENE_ACTIVE_LIGHT+scene].value = currentScene == scene ? 1.0 : 0.0;
		}
		lights[SAVE_MODE_LIGHT].value = saveMode && saveBlinker ? 1.0 : 0.0;

	
		// Modes
		if (constantTimeTrigger.process(params[CONSTANT_TIME_MODE_PARAM].getValue())) {
			masterTrack = (masterTrack + 1) % 6;
			constantTime = masterTrack > 0;
			for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
				beatIndex[trackNumber] = -1;
                lastStepTime[trackNumber] = 0;
                lastSwingDuration[trackNumber] = 0; // Not sure about this
				expanderEocValue[trackNumber] = 0; 
				lastExpanderEocValue[trackNumber] = 0;		
				extraParameterValue[trackNumber] = 1.0;
			}
			
		}
		
		for(int cm=0;cm<3;cm++) {
			if (chainModeTrigger[cm].process(params[CHAIN_MODE_PARAM+cm].getValue())) {
				chainMode = cm;
				setRunningState();
			}		
		}
		lights[CHAIN_MODE_NONE_LIGHT].value = chainMode == CHAIN_MODE_NONE ? 1.0 : 0.0;
		lights[CHAIN_MODE_BOSS_LIGHT].value = chainMode == CHAIN_MODE_BOSS ? 1.0 : 0.0;
		lights[CHAIN_MODE_EMPLOYEE_LIGHT].value = chainMode == CHAIN_MODE_EMPLOYEE ? 1.0 : 0.0;

		lights[MUTED_LIGHT].value = muted ? 1.0 : 0.0;
		lights[MANUAL_RUN_LIGHT + 1].value = manualRun ? 1.0 : 0.0;
		

		maxStepCount = 0;
		masterStepCount = 0;

		metaStepCount = params[META_STEP_PARAM].getValue();
		if(inputs[META_STEP_INPUT].isConnected()) {
			metaStepCount += inputs[META_STEP_INPUT].getVoltage() * 36.0;
		}		
		metaStepCount = clamp(metaStepCount,1.0f,360.0f);
		metaStepCountPercentage = metaStepCount / 360.0;



		for(int trackNumber=0;trackNumber<4;trackNumber++) {
            if(algorithmTrigger[trackNumber].process(params[(ALGORITHM_1_PARAM + trackNumber * 8)].getValue()+inputs[(ALGORITHM_1_INPUT + trackNumber * 8)].getVoltage())) {
                algorithmMatrix[trackNumber] = (algorithmMatrix[trackNumber] + 1) % (trackNumber < 2 ? NUM_ALGORITHMS -1 : NUM_ALGORITHMS); //Only tracks 3 and 4 get logic
            } 
			switch (algorithmMatrix[trackNumber]) {
				case EUCLIDEAN_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = 1;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 1;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 0;
					break;
				case GOLUMB_RULER_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = 0;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 0;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 1;
					break;
				case WELL_FORMED_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = 0;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .75;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 0;
					break;
				case PERFECT_BALANCE_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = 1;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .5;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 0.0;
					break;
				case MANUAL_MODE_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = .875;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .875;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .875;
					break;
				case BOOLEAN_LOGIC_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = .875;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 0;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .9;
					break;
				case FIBONACCI_MODE_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = .04;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .04;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .58;
					break;
			}
			            
            if(accentAlgorithmTrigger[trackNumber].process(params[(ACCENT_ALGORITHM_1_PARAM + trackNumber)].getValue()+inputs[(ACCENT_ALGORITHM_1_INPUT + trackNumber)].getVoltage())) {
                accentAlgorithmMatrix[trackNumber] = (accentAlgorithmMatrix[trackNumber] + 1) % (trackNumber < 2 ? NUM_ALGORITHMS -1 : NUM_ALGORITHMS); //Only tracks 3 and 4 get logic
				if(accentAlgorithmMatrix[trackNumber] == WELL_FORMED_ALGO) //Skip me
					accentAlgorithmMatrix[trackNumber]++; //No Well Formed Algo for Accents
            } 
			switch (accentAlgorithmMatrix[trackNumber]) {
				case EUCLIDEAN_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = 1;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 1;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 0;
					break;
				case GOLUMB_RULER_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = 0;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 0;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 1;
					break;
				case WELL_FORMED_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = 0;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .75;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 0;
					break;
				case PERFECT_BALANCE_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = 1;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .5;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = 0.0;
					break;
				case MANUAL_MODE_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = .875;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .875;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .875;
					break;
				case BOOLEAN_LOGIC_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = .875;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 0;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .9;
					break;
				case FIBONACCI_MODE_ALGO :
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3].value = .04;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = .04;
					lights[ACCENT_ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .58;
					break;
			}

			if(trackIndependentTrigger[trackNumber].process(params[(TRACK_1_INDEPENDENT_PARAM + trackNumber * 8)].getValue())) {
                trackIndependent[trackNumber] = !trackIndependent[trackNumber]; 
            }
			lights[TRACK_INDEPENDENT_1_LIGHT + trackNumber*3 + 1].value = trackIndependent[trackNumber] ? 0.875 : 0.0;

            
        
			float stepsCountf = std::floor(params[(trackNumber * 8) + STEPS_1_PARAM].getValue());			
			if(inputs[trackNumber * 8].isConnected()) {
				stepsCountf += inputs[trackNumber * 8 + STEPS_1_INPUT].getVoltage() * 7.3;
			}
			stepsCountf = clamp(stepsCountf,1.0f,float(MAX_STEPS));
			if(algorithmMatrix[trackNumber] == BOOLEAN_LOGIC_ALGO) { // Boolean Tracks can't exceed length of the tracks they are based (-1 and -2)
				stepsCountf = std::min(stepsCountf,(float)std::min(stepsCount[trackNumber-1],stepsCount[trackNumber-2]));
			}
			stepsPercentage[trackNumber] = stepsCountf / MAX_STEPS;

			float divisionf = std::floor(params[(trackNumber * 8) + DIVISIONS_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].isConnected()) {
				divisionf += inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].getVoltage() * 7.3;
			}		
			divisionf = clamp(divisionf,0.0f,stepsCountf);
			divisionsPercentage[trackNumber] = divisionf / MAX_STEPS;

			float offsetf = std::floor(params[(trackNumber * 8) + OFFSET_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + OFFSET_1_INPUT].isConnected()) {
				offsetf += inputs[(trackNumber * 8) + OFFSET_1_INPUT].getVoltage() * 7.2;
			}	
			offsetf = clamp(offsetf,0.0f,MAX_STEPS-1.0f);
			offsetPercentage[trackNumber] = offsetf / (MAX_STEPS-1.0);

			float padf = std::floor(params[trackNumber * 8 + PAD_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + PAD_1_INPUT].isConnected()) {
				padf += inputs[trackNumber * 8 + PAD_1_INPUT].getVoltage() * 7.2;
			}
			padf = clamp(padf,0.0f,stepsCountf -1);
			padPercentage[trackNumber] = padf / (MAX_STEPS-1.0);

			// Reclamp
			divisionf = clamp(divisionf,0.0f,stepsCountf-padf);



			float accentDivisionf = std::floor(params[(trackNumber * 8) + ACCENTS_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + ACCENTS_1_INPUT].isConnected()) {
				accentDivisionf += inputs[(trackNumber * 8) + ACCENTS_1_INPUT].getVoltage() * MAX_STEPS / 10.0;
			}
			accentDivisionsPercentage[trackNumber] = accentDivisionf / MAX_STEPS;

			float accentRotationf = std::floor(params[(trackNumber * 8) + ACCENT_ROTATE_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].isConnected()) {
				accentRotationf += inputs[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].getVoltage() * MAX_STEPS / 10.0;
			}
			accentRotatePercentage[trackNumber] = accentRotationf / MAX_STEPS;

			// if(divisionf > 0) {
			// 	accentRotationf = clamp(accentRotationf,0.0f,divisionf-1);			
			// } else {
			// 	accentRotationf = 0;
			// }	

			if(stepsCountf > maxStepCount)
				maxStepCount = std::floor(stepsCountf);
			if(trackNumber == masterTrack - 1)
				masterStepCount = std::floor(stepsCountf);		

			stepsCount[trackNumber] = int(stepsCountf);
			if(lastStepsCount[trackNumber] == -1) //first time
				lastStepsCount[trackNumber] = stepsCount[trackNumber];

			int division = int(divisionf);
			currentDivisionsSetting[trackNumber] = division;
			int offset = int(offsetf);		
			int pad = int(padf);
			int accentDivision = int(accentDivisionf);
			int accentRotation = int(accentRotationf);


			if(trackNumber > 0 && algorithmMatrix[trackNumber] == WELL_FORMED_ALGO && wellFormedHierchical[trackNumber]) {				
				int parentTrackNumber = trackNumber-1;
				while (parentTrackNumber >=0 && algorithmMatrix[parentTrackNumber] != WELL_FORMED_ALGO) {
					parentTrackNumber -=1;
				}
				if(parentTrackNumber >=0) {
					wellFormedParentTrack[trackNumber] = parentTrackNumber;
					int numberLargeSteps = stepsCount[parentTrackNumber] - currentDivisionsSetting[parentTrackNumber];
					int newLargeValue = stepsCount[parentTrackNumber] + numberLargeSteps;
					if(extraParameterValue[parentTrackNumber] < 2) {
						currentDivisionsSetting[trackNumber] = numberLargeSteps;
						division = numberLargeSteps;
						extraParameterValue[trackNumber] = 1.0 / (extraParameterValue[parentTrackNumber] - 1.0);
					} else {
						currentDivisionsSetting[trackNumber] = stepsCount[parentTrackNumber];
						division = stepsCount[parentTrackNumber];
						extraParameterValue[trackNumber] = extraParameterValue[parentTrackNumber] - 1.0;
					}
					//fprintf(stderr, "%f %f\n", extraParameterValue[parentTrackNumber], extraParameterValue[trackNumber]);
					stepsCount[trackNumber] = newLargeValue;
					params[STEPS_1_PARAM + (trackNumber * 8)].setValue(newLargeValue);
					params[DIVISIONS_1_PARAM + (trackNumber * 8)].setValue(currentDivisionsSetting[trackNumber]);
				}				
			}


			//Parameters that cause a word to be created
			if(stepsCount[trackNumber] != lastStepsSetting[trackNumber] || currentDivisionsSetting[trackNumber] != lastDivisionsSetting[trackNumber] 
				|| algorithmMatrix[trackNumber] != lastAlgorithmSetting[trackNumber] || pad != lastPadSetting[trackNumber]) {
					
				lastAlgorithmSetting[trackNumber] = algorithmMatrix[trackNumber];
				lastStepsSetting[trackNumber] = stepsCount[trackNumber];
				lastDivisionsSetting[trackNumber] = currentDivisionsSetting[trackNumber];
				lastPadSetting[trackNumber] = pad;

				dirty[trackNumber] = true;
				if(algorithmMatrix[trackNumber] == WELL_FORMED_ALGO) {
					std::string word = christoffelWords.Generate(stepsCount[trackNumber]-pad,division);
					if(word != "unknown") {
						currentChristoffelword[trackNumber] = word;
					}
					for(int i=trackNumber+1;i<TRACK_COUNT;i++) {
						dirty[i] = true;
					}
					//fprintf(stderr, "%s \n", word.c_str());
				}
			}

			//These force other tracks to recalc as well 
			if(offset != lastOffsetSetting[trackNumber] || extraParameterValue[trackNumber] != lastExtraParameterValue[trackNumber] 
				|| lastwfHierarchicalValue[trackNumber] != wellFormedHierchical[trackNumber] || lastwfComplementValue[trackNumber] != wellFormedComplement[trackNumber]) {

				lastOffsetSetting[trackNumber] = offset;
				lastExtraParameterValue[trackNumber] = extraParameterValue[trackNumber]; 
				lastwfHierarchicalValue[trackNumber] = wellFormedHierchical[trackNumber];
				lastwfComplementValue[trackNumber] = wellFormedComplement[trackNumber];
				
				for(int i=trackNumber;i<TRACK_COUNT;i++) {
					dirty[i] = true;
				}
			}


			//Everything else that forces beat recalc
			if(accentDivision != lastAccentSetting[trackNumber] || accentRotation != lastAccentRotationSetting[trackNumber] || 
				accentAlgorithmMatrix[trackNumber] != lastAccentAlgorithmSetting[trackNumber]) {
				lastAccentAlgorithmSetting[trackNumber] = accentAlgorithmMatrix[trackNumber]; 
				lastAccentSetting[trackNumber] = accentDivision;
				lastAccentRotationSetting[trackNumber] = accentRotation;

				dirty[trackNumber] = true;
			}

			if(dirty[trackNumber]) {
				beatCount[trackNumber] = 0;

				//clear out the matrix and levels
				for(int j=0;j<MAX_STEPS;j++)
				{
					beatLocation[trackNumber][j] = 0;
				}


				if(division > 0) {				
					int bucket = stepsCount[trackNumber] - pad - 1;                    
					if(algorithmMatrix[trackNumber] == EUCLIDEAN_ALGO ) { //Euclidean Algorithn
						int euclideanBeatIndex = 0;
						//Set padded steps to false
						for(int euclideanStepIndex = 0; euclideanStepIndex < pad; euclideanStepIndex++) {
							beatMatrix[trackNumber][((euclideanStepIndex + offset) % (stepsCount[trackNumber]))] = false;	
						}
						for(int euclideanStepIndex = 0; euclideanStepIndex < stepsCount[trackNumber]-pad; euclideanStepIndex++)
						{
							bucket += division;
							if(bucket >= stepsCount[trackNumber]-pad) {
								bucket -= (stepsCount[trackNumber] - pad);
								beatMatrix[trackNumber][((euclideanStepIndex + offset + pad) % (stepsCount[trackNumber]))] = true;	
								beatLocation[trackNumber][euclideanBeatIndex] = (euclideanStepIndex + offset + pad) % stepsCount[trackNumber];									
								// fprintf(stderr, "Euclidean Track:%i BI:%i  BL:%i \n",trackNumber,euclideanBeatIndex,beatLocation[trackNumber][euclideanBeatIndex]);
								euclideanBeatIndex++;	
							} else
							{
								beatMatrix[trackNumber][((euclideanStepIndex + offset + pad) % (stepsCount[trackNumber]))] = false;	
							}                        
						}
						trackPatternName[trackNumber] = std::to_string(stepsCount[trackNumber])+"-"+std::to_string(euclideanBeatIndex);
						beatCount[trackNumber] = euclideanBeatIndex;
					} else if(algorithmMatrix[trackNumber] == GOLUMB_RULER_ALGO) { //Golomb Ruler Algorithm
					
						int rulerToUse = clamp(division-1,0,NUM_RULERS-1);
						int actualStepCount = stepsCount[trackNumber] - pad;
						while(rulerLengths[rulerToUse] + 1 > actualStepCount && rulerToUse >= 0) {
							rulerToUse -=1;
						}

						trackPatternName[trackNumber] =std::to_string(stepsCount[trackNumber])+"-"+rulerNames[rulerToUse]; 
						
						//Multiply beats so that low division beats fill out entire pattern
						float spaceMultiplier = (actualStepCount / (rulerLengths[rulerToUse] + 1));

						//Set all beats to false
						for(int j=0;j<stepsCount[trackNumber];j++)
						{
							beatMatrix[trackNumber][j] = false; 			
						}

						for (int rulerIndex = 0; rulerIndex < rulerOrders[rulerToUse];rulerIndex++)
						{
							int divisionLocation = (rulers[rulerToUse][rulerIndex] * spaceMultiplier) + pad;
							beatMatrix[trackNumber][(divisionLocation + offset) % stepsCount[trackNumber]] = true;
							beatLocation[trackNumber][rulerIndex] = (divisionLocation + offset) % stepsCount[trackNumber];	            
						}
						beatCount[trackNumber] = rulerOrders[rulerToUse];
					} else if(algorithmMatrix[trackNumber] == WELL_FORMED_ALGO) { 
						int wfBeatCount = 0;
						int wfParentBeatIndex = 0;
						double parentTrackPosition = 0.0;
						double trackPosition = 0.0;
						wellFormedTrackDuration[trackNumber] = currentDivisionsSetting[trackNumber] + (stepsCount[trackNumber] - currentDivisionsSetting[trackNumber]) * extraParameterValue[trackNumber]; 
						double stepScaling = (masterTrack <= TRACK_COUNT ? wellFormedTrackDuration[masterTrack-1] : metaStepCount) / wellFormedTrackDuration[trackNumber];
						double parentStepScaling = (masterTrack <= TRACK_COUNT ? 1.0 : metaStepCount / wellFormedTrackDuration[wellFormedParentTrack[trackNumber]]);

						//Set all beats to false
						for(int j=0;j<stepsCount[trackNumber];j++)
						{
							beatMatrix[trackNumber][j] = false; 			
						}

						for(int wfBeatIndex=0;wfBeatIndex<stepsCount[trackNumber]-pad;wfBeatIndex++) { //NEED TO HANDLE UNKNOWNS
							char beatType = currentChristoffelword[trackNumber][wfBeatIndex];
							int adjustedWfBeatIndex = (wfBeatIndex + offset + pad) % stepsCount[trackNumber];
							if(beatType == 's') {
								wellFormedStepDurations[trackNumber][adjustedWfBeatIndex] = 1.0;
								beatLocation[trackNumber][wfBeatCount] = adjustedWfBeatIndex;	     
								wfBeatCount++;       						
							} else {
								wellFormedStepDurations[trackNumber][(wfBeatIndex + offset + pad) % stepsCount[trackNumber]] = extraParameterValue[trackNumber];
							}
							if(wellFormedHierchical[trackNumber] && wellFormedComplement[trackNumber]) {		
								beatMatrix[trackNumber][adjustedWfBeatIndex] = abs(trackPosition * stepScaling - parentTrackPosition) >= .01 || (wellFormedComplement[trackNumber] == 1 && !beatMatrix[wellFormedParentTrack[trackNumber]][wfParentBeatIndex]); 
								trackPosition += wellFormedStepDurations[trackNumber][adjustedWfBeatIndex];
								if(trackPosition * stepScaling > parentTrackPosition + .01) { 
									parentTrackPosition += wellFormedStepDurations[wellFormedParentTrack[trackNumber]][wfParentBeatIndex] * parentStepScaling;
									wfParentBeatIndex++;
								}
							} else {
								beatMatrix[trackNumber][adjustedWfBeatIndex] = true; //every step is a beat if not complementiing
							}
						}


						trackPatternName[trackNumber] = std::to_string(stepsCount[trackNumber]-pad-wfBeatCount)+"l "+std::to_string(wfBeatCount)+"s"; 
						beatCount[trackNumber] = currentDivisionsSetting[trackNumber];
					} else if(algorithmMatrix[trackNumber] == PERFECT_BALANCE_ALGO) { 
						int pbPatternToUse = -1;
						int pbMatchPatternCount = 0;
						int pbLastMatchedPattern = 0;
						int actualStepCount = stepsCount[trackNumber] - pad;
						bool patternFound = false;
						while(!patternFound) {
							pbPatternToUse +=1;
							if(pbPatternToUse >= NUM_PB_PATTERNS)
								break;
							if(pbPatternLengths[pbPatternToUse] <= actualStepCount && actualStepCount % pbPatternLengths[pbPatternToUse] == 0) {
								pbLastMatchedPattern = pbPatternToUse;
								if(pbMatchPatternCount >= division-1) {
									patternFound = true;
								} else {
									pbMatchPatternCount +=1;							
								}
							}
						}
						if(!patternFound)
							pbPatternToUse = pbLastMatchedPattern;
						
						//Set all beats to false
						for(int j=0;j<stepsCount[trackNumber];j++)
						{
							beatMatrix[trackNumber][j] = false; 			
						}

						//Multiply beats so that low division beats fill out entire pattern
						float spaceMultiplier = (actualStepCount / (pbPatternLengths[pbPatternToUse]));
						trackPatternName[trackNumber] =std::to_string(stepsCount[trackNumber])+"-"+pbPatternNames[pbPatternToUse]; 

						for (int pbIndex = 0; pbIndex < pbPatternOrders[pbPatternToUse];pbIndex++)
						{
							int divisionLocation = (pbPatterns[pbPatternToUse][pbIndex] * spaceMultiplier) + pad;
							beatMatrix[trackNumber][(divisionLocation + offset) % stepsCount[trackNumber]] = true;
							beatLocation[trackNumber][pbIndex] = (divisionLocation + offset) % stepsCount[trackNumber];	            
						}
						beatCount[trackNumber] = pbPatternOrders[pbPatternToUse];
					} else if(algorithmMatrix[trackNumber] == MANUAL_MODE_ALGO) {
						int manualBeatCount = 0;

						for (int manualBeatIndex = 0; manualBeatIndex < stepsCount[trackNumber];manualBeatIndex++) {
							bool isBeat = getManualBeat(trackNumber,manualBeatIndex) ;  
							beatMatrix[trackNumber][(manualBeatIndex + offset) % stepsCount[trackNumber]] = isBeat;
							if(isBeat) {
								//beatLocation[trackNumber][(manualBeatIndex + offset) % stepsCount[trackNumber]] = manualBeatIndex;	
								beatLocation[trackNumber][manualBeatCount] = (manualBeatIndex + offset) % stepsCount[trackNumber];	
								manualBeatCount ++;
							}
						}
						beatCount[trackNumber] = manualBeatCount;
						trackPatternName[trackNumber] = std::to_string(stepsCount[trackNumber]) + "-" + std::to_string(manualBeatCount);
					} else if(algorithmMatrix[trackNumber] == FIBONACCI_MODE_ALGO) { //FIBONACCi
					
						int actualStepCount = stepsCount[trackNumber] - pad;						
						//Set all beats to false
						for(int j=0;j<stepsCount[trackNumber];j++)
						{
							beatMatrix[trackNumber][j] = false; 			
						}

						int startPos = 1;
						int lastStartPos = 0;
						int fbBeatCount = 0;
						for (int fibIndex = 0; fibIndex < division;fibIndex++)
						{
							int divisionLocation = startPos + lastStartPos;
							lastStartPos = startPos;
							startPos = divisionLocation;

							//Subtract 1 since position is 0 based
							divisionLocation = (divisionLocation - 1 + pad);
							if(divisionLocation < actualStepCount) {
								fbBeatCount++;
								beatMatrix[trackNumber][(divisionLocation + offset) % actualStepCount] = true;
								beatLocation[trackNumber][fibIndex] = (divisionLocation + offset) % actualStepCount;	
							} 							          
						}
						beatCount[trackNumber] = fbBeatCount;
						trackPatternName[trackNumber] =std::to_string(stepsCount[trackNumber])+"-"+std::to_string(fbBeatCount); 
					} else if(algorithmMatrix[trackNumber] == BOOLEAN_LOGIC_ALGO) { //Boolean Logic only for tracs 3 and 4
						int logicBeatCount = 0;
						int logicMode = (division-1) % 6; 

						trackPatternName[trackNumber] = booleanOperationNames[logicMode];

						for (int logicBeatIndex = 0; logicBeatIndex < stepsCount[trackNumber];logicBeatIndex++) {
							bool isBeat = false;
							switch (logicMode) {
								case 0 :
									isBeat = beatMatrix[trackNumber-1][logicBeatIndex] && beatMatrix[trackNumber-2][logicBeatIndex];  //AND
									break;
								case 1 :
									isBeat = beatMatrix[trackNumber-1][logicBeatIndex] || beatMatrix[trackNumber-2][logicBeatIndex];  //OR
									break;
								case 2 :
									isBeat = beatMatrix[trackNumber-1][logicBeatIndex] != beatMatrix[trackNumber-2][logicBeatIndex]; //XOR
									break;
								case 3 :
									isBeat = !(beatMatrix[trackNumber-1][logicBeatIndex] && beatMatrix[trackNumber-2][logicBeatIndex]);  //NAND
									break;
								case 4 :
									isBeat = !(beatMatrix[trackNumber-1][logicBeatIndex] || beatMatrix[trackNumber-2][logicBeatIndex]);  //NOR
									break;
								case 5 :
									isBeat = beatMatrix[trackNumber-1][logicBeatIndex] == beatMatrix[trackNumber-2][logicBeatIndex]; //IMP
									break;
							}
							beatMatrix[trackNumber][(logicBeatIndex + offset) % stepsCount[trackNumber]] = isBeat;
							if(isBeat) {
								beatLocation[trackNumber][(logicBeatIndex + offset) % stepsCount[trackNumber]] = logicBeatIndex;	
								logicBeatCount ++;
							}
						}
						beatCount[trackNumber] = logicBeatCount;
					}

					//Set all accents to false
					for(int j=0;j<stepsCount[trackNumber];j++)
					{
						accentMatrix[trackNumber][j] = false; 			
					}

					//Calculate Accents
					
					int unclampedAccentDivision = accentDivision;			
					accentDivision = clamp(accentDivision,0,beatCount[trackNumber]);
					if(accentDivision > 0 && stepsCount[trackNumber] > 0) {
						accentRotation = accentRotation	% beatCount[trackNumber];						
						if(accentAlgorithmMatrix[trackNumber] == EUCLIDEAN_ALGO) {
							bucket = beatCount[trackNumber] - 1;
							int euclideanBeatIndex = 0;
							for(int accentIndex = 0; accentIndex < beatCount[trackNumber]; accentIndex++)
							{
								bucket += accentDivision;
								if(bucket >= beatCount[trackNumber]) {
									bucket -= beatCount[trackNumber];
									accentMatrix[trackNumber][beatLocation[trackNumber][(accentIndex + accentRotation) % beatCount[trackNumber]]] = true;
									euclideanBeatIndex++;			
								} else
								{
									accentMatrix[trackNumber][beatLocation[trackNumber][(accentIndex + accentRotation) % beatCount[trackNumber]]] = false;
								}						
							}
							accentPatternName[trackNumber] = std::to_string(beatCount[trackNumber])+"-"+std::to_string(euclideanBeatIndex);
						} else if(accentAlgorithmMatrix[trackNumber] == GOLUMB_RULER_ALGO) {	 
							int rulerToUse = clamp(accentDivision-1,0,NUM_RULERS-1);
							while(rulerLengths[rulerToUse] + 1 > accentDivision && rulerToUse >= 0) {
								rulerToUse -=1;
							}
							
							//Multiply beats so that low division beats fill out entire pattern
							float spaceMultiplier = (beatCount[trackNumber] / (rulerLengths[rulerToUse] + 1));

							for (int rulerIndex = 0; rulerIndex < rulerOrders[rulerToUse];rulerIndex++)
							{
								int accentLocation = (rulers[rulerToUse][rulerIndex] * spaceMultiplier);
								accentMatrix[trackNumber][beatLocation[trackNumber][(accentLocation + accentRotation) % beatCount[trackNumber]]] = true;
							}

							accentPatternName[trackNumber] =std::to_string(beatCount[trackNumber])+"-"+rulerNames[rulerToUse]; 
						} else if(accentAlgorithmMatrix[trackNumber] == PERFECT_BALANCE_ALGO) {	 
							int pbPatternToUse = -1;
							int pbMatchPatternCount = 0;
							int pbLastMatchedPattern = 0;
							bool patternFound = false;
							while(!patternFound) {
								pbPatternToUse +=1;
								if(pbPatternToUse >= NUM_PB_PATTERNS)
									break;
								if(pbPatternLengths[pbPatternToUse] <= beatCount[trackNumber] && beatCount[trackNumber] % pbPatternLengths[pbPatternToUse] == 0) {
									pbLastMatchedPattern = pbPatternToUse;
									if(pbMatchPatternCount >= division-1) {
										patternFound = true;
									} else {
										pbMatchPatternCount +=1;							
									}
								}
							}
							if(!patternFound)
								pbPatternToUse = pbLastMatchedPattern;
							
							//Multiply beats so that low division beats fill out entire pattern
							float spaceMultiplier = (beatCount[trackNumber] / (pbPatternLengths[pbPatternToUse]));
							accentPatternName[trackNumber] =std::to_string(beatCount[trackNumber])+"-"+pbPatternNames[pbPatternToUse]; 

							for (int pbIndex = 0; pbIndex < pbPatternOrders[pbPatternToUse];pbIndex++)
							{
								int divisionLocation = (pbPatterns[pbPatternToUse][pbIndex] * spaceMultiplier);
								accentMatrix[trackNumber][beatLocation[trackNumber][(divisionLocation + accentRotation) % division]] = true;
							}

						} else if(accentAlgorithmMatrix[trackNumber] == FIBONACCI_MODE_ALGO) {	 
							int startPos = 1;
							int lastStartPos = 0;
							int fbBeatCount = 0;
							for (int fibIndex = 0; fibIndex < beatCount[trackNumber];fibIndex++)
							{
								int divisionLocation = startPos + lastStartPos;
								lastStartPos = startPos;
								startPos = divisionLocation;

								//Subtract 1 since position is 0 based
								divisionLocation = (divisionLocation - 1);
								if(divisionLocation < beatCount[trackNumber]) {
									fbBeatCount++;
									accentMatrix[trackNumber][beatLocation[trackNumber][(divisionLocation + accentRotation) % beatCount[trackNumber]]] = true;
								} 							          
							}
							accentPatternName[trackNumber] =std::to_string(beatCount[trackNumber])+"-"+std::to_string(fbBeatCount); 

						} else if(accentAlgorithmMatrix[trackNumber] == BOOLEAN_LOGIC_ALGO) {	 
							int logicMode = (unclampedAccentDivision-1) % 6; 

							accentPatternName[trackNumber] = booleanOperationNames[logicMode];

							for (int logicAccentIndex = 0; logicAccentIndex < stepsCount[trackNumber];logicAccentIndex++) {
								bool isBeat = false;
								switch (logicMode) {
									case 0 :
										isBeat = accentMatrix[trackNumber-1][logicAccentIndex] && accentMatrix[trackNumber-2][logicAccentIndex];  //AND
										break;
									case 1 :
										isBeat = accentMatrix[trackNumber-1][logicAccentIndex] || accentMatrix[trackNumber-2][logicAccentIndex];  //OR
										break;
									case 2 :
										isBeat = accentMatrix[trackNumber-1][logicAccentIndex] != accentMatrix[trackNumber-2][logicAccentIndex]; //XOR
										break;
									case 3 :
										isBeat = !(accentMatrix[trackNumber-1][logicAccentIndex] && accentMatrix[trackNumber-2][logicAccentIndex]);  //NAND
										break;
									case 4 :
										isBeat = !(accentMatrix[trackNumber-1][logicAccentIndex] || accentMatrix[trackNumber-2][logicAccentIndex]);  //NOR
										break;
									case 5 :
										isBeat = accentMatrix[trackNumber-1][logicAccentIndex] == accentMatrix[trackNumber-2][logicAccentIndex]; //IMP
										break;
								}
								accentMatrix[trackNumber][(logicAccentIndex + accentRotation) % stepsCount[trackNumber]] = isBeat;			
							}
						} else if(accentAlgorithmMatrix[trackNumber] == MANUAL_MODE_ALGO) {
							int accentCount = 0;
							for (int manualAccentIndex = 0; manualAccentIndex < beatCount[trackNumber];manualAccentIndex++) {
								bool isAccent = getManualAccent(trackNumber,manualAccentIndex);
								if(isAccent) {
									accentMatrix[trackNumber][beatLocation[trackNumber][(manualAccentIndex + accentRotation) % beatCount[trackNumber]]] = true; 
									accentCount++;
								}
							}
							accentPatternName[trackNumber] =std::to_string(beatCount[trackNumber])+"-"+std::to_string(accentCount); 
						}       	
					} else {
						accentPatternName[trackNumber] ="Off"; 
					}
				} else {
					trackPatternName[trackNumber] = "";
					//Set all beats to false
					for(int j=0;j<stepsCount[trackNumber];j++)
					{
						beatMatrix[trackNumber][j] = false; 			
					}
				}	
			}
			dirty[trackNumber] = false;
		}
		

		//Get Expander Info
		if(rightExpander.module && (rightExpander.module->model == modelQARWellFormedRhythmExpander || rightExpander.module->model == modelQARProbabilityExpander || 
		   rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARWarpedSpaceExpander ||
		   rightExpander.module->model == modelQARIrrationalityExpander || rightExpander.module->model == modelQARConditionalExpander))
		{			
			float *messagesFromExpanders = (float*)rightExpander.consumerMessage;

			for(int i = 0;i<TRACK_COUNT;i++) {
				expanderDataChanged[i] = messagesFromExpanders[i] || expanderRecalcNeeded[i] || (!QARExpanderDisconnectReset); //If an expander first gets hooked up get data changes
			}

			QARExpanderDisconnectReset = true;

			//Process Probability Expander Stuff						
			for(int i = 0; i < TRACK_COUNT; i++) {
				if(expanderDataChanged[i]) {
					probabilityGroupFirstStep[i] = -1;
					for(int j = 0; j < stepsCount[i]; j++) { //reset all probabilities, find first group step
						workingProbabilityMatrix[i][j] = 1;					
					}

					if(messagesFromExpanders[TRACK_COUNT * 4 + i] > 0) { // 0 is track not selected
						bool useDivs = messagesFromExpanders[TRACK_COUNT * 4 + i] == 2; //2 is divs
						for(int j = 0; j < stepsCount[i]; j++) { // Assign probabilites and swing
							int stepIndex = j;
							bool stepFound = true;
							if(useDivs) { //Use j as a count to the div # we are looking for
								if(j < beatCount[i]) {
									stepIndex = beatLocation[i][j];
									// fprintf(stderr, "Probability Track:%i step:%i BL:%i \n",i,j,stepIndex);
								} else {
									stepFound = false;
								}
							}
							
							int messageIndex = j % EXPANDER_MAX_STEPS;

							if(stepFound) {
								float probability = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (i * EXPANDER_MAX_STEPS) + messageIndex];
								float probabilityMode = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT) + (i * EXPANDER_MAX_STEPS) + messageIndex];
								
								workingProbabilityMatrix[i][stepIndex] = probability;
								probabilityGroupModeMatrix[i][stepIndex] = probabilityMode;
								if(probabilityGroupFirstStep[i] < 0 && probabilityGroupModeMatrix[i][stepIndex] != NONE_PGTM ) {
									probabilityGroupFirstStep[i] = stepIndex;
								}
							} 
						}
					}
				}
			}

			//Process Conditional Expander Stuff						
			for(int i = 0; i < TRACK_COUNT; i++) {
				if(expanderDataChanged[i]) {
					for(int j = 0; j < stepsCount[i]; j++) { //reset all conditiona, find first group step
						workingConditionalMatrix[i][j] = 1;
						workingConditionaModeMatrix[i][j] = 0;											
					}

					if(messagesFromExpanders[TRACK_COUNT * 16 + i] > 0) { // 0 is track not selected
						bool useDivs = messagesFromExpanders[TRACK_COUNT * 16 + i] == 2; //2 is divs
						for(int j = 0; j < stepsCount[i]; j++) { // Assign probabilites and swing
							int stepIndex = j;
							bool stepFound = true;
							if(useDivs) { //Use j as a count to the div # we are looking for
								if(j < beatCount[i]) {
									stepIndex = beatLocation[i][j];
									// fprintf(stderr, "Probability Track:%i step:%i BL:%i \n",i,j,stepIndex);
								} else {
									stepFound = false;
								}
							}
							
							int messageIndex = j % EXPANDER_MAX_STEPS;

							if(stepFound) {
								int divideCount = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 4) + (i * EXPANDER_MAX_STEPS) + messageIndex];
								bool conditionalMode = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 5) + (i * EXPANDER_MAX_STEPS) + messageIndex];
								// fprintf(stderr, "Conditional Track:%i step:%i DC:%i \n",i,stepIndex,divideCount);
								workingConditionalMatrix[i][stepIndex] = divideCount;
								workingConditionaModeMatrix[i][stepIndex] = conditionalMode;
							} 
						}
					}
				}
			}

			//Process Groove Expander Stuff									
			for(int i = 0; i < TRACK_COUNT; i++) {
				if(expanderDataChanged[i] || expanderRecalcNeeded[i]) {				
					for(int j = 0; j < stepsCount[i]; j++) { //reset all swing
						workingSwingMatrix[i][j] = 0.0;
					}

					if(messagesFromExpanders[TRACK_COUNT * 5 + i] > 0) { // 0 is track not selected
						bool useDivs = messagesFromExpanders[TRACK_COUNT * 5 + i] == 2; //2 is divs
						trackSwingUsingDivs[i] = useDivs;

						int grooveLength = (int)(messagesFromExpanders[TRACK_COUNT * 6 + i]);
						bool useTrackLength = messagesFromExpanders[TRACK_COUNT * 7 + i];

						swingRandomness[i] = messagesFromExpanders[TRACK_COUNT * 8 + i];
						useGaussianDistribution[i] = messagesFromExpanders[TRACK_COUNT * 9 + i];

						if(useTrackLength) {
							grooveLength = stepsCount[i];
						}
						subBeatLength[i] = grooveLength;
						if(subBeatIndex[i] >= grooveLength) { //Reset if necessary
							subBeatIndex[i] = 0;
						}
						

						int workingBeatIndex;
						if(!useDivs) {
							workingBeatIndex = (subBeatIndex[i] - beatIndex[i]) % grooveLength; 
							if(workingBeatIndex <0) {
								workingBeatIndex +=grooveLength;
							}
						} else {
							workingBeatIndex = (subBeatIndex[i] - beatCountAtIndex[i]) % grooveLength; 
							if(workingBeatIndex <0) {
								workingBeatIndex +=grooveLength;
							}
						}

								// if(i==1) {
								// 	fprintf(stderr, "%i %i %i %i \n", beatCount[i],subBeatIndex[i],beatCountAtIndex[i],beatIndex[i]);
								// }

						for(int j = 0; j < stepsCount[i]; j++) { 
							int stepIndex = j;
							bool stepFound = true;
							if(useDivs) { //Use j as a count to the div # we are looking for
								if(j < beatCount[i]) { 
									stepIndex = beatLocation[i][j];
								} else {
									stepFound = false;
								}
							}

								// if(i==0) {
								// 	fprintf(stderr, "%i %i %i \n", stepIndex,stepFound,beatCount[i]);
								// }


							int messageIndex = workingBeatIndex % EXPANDER_MAX_STEPS;

							if(stepFound) {
								float swing = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 2) + (i * EXPANDER_MAX_STEPS) + messageIndex];
								workingSwingMatrix[i][stepIndex] = swing;						
							} 
							workingBeatIndex +=1;
							if(workingBeatIndex >= grooveLength) {
								workingBeatIndex = 0;
							}
						}
					}
				}
			}

			//Process Warped Space Stuff									
			for(int i = 0; i < TRACK_COUNT; i++) {
				if(expanderDataChanged[i]) {
					for(int j = 0; j < stepsCount[i]; j++) { //reset all warping
						workingBeatWarpMatrix[i][j] = 1.0;
					}

					if(messagesFromExpanders[TRACK_COUNT * 10 + i] > 0) { // 0 is track not selected
						beatWarping[i] = messagesFromExpanders[TRACK_COUNT * 11 + i];
						beatWarpingPosition[i] = (int)messagesFromExpanders[TRACK_COUNT * 12 + i];
						beatWarpingLength[i] = (int)messagesFromExpanders[TRACK_COUNT * 13 + i];
						float trackWarpedStepCount = (float)(std::min(beatWarpingLength[i],stepsCount[i]));
						
						float stepsToSpread = (trackWarpedStepCount / 2.0)-1;
						float fraction = 1.0/beatWarping[i];
						for(int j = 0; j < trackWarpedStepCount; j++) {	
							int actualBeat = (j + beatWarpingPosition[i]) % stepsCount[i]; 
							float fj = (float)j;					 
							if(j <= stepsToSpread)
								workingBeatWarpMatrix[i][actualBeat] = (2-fraction)*(stepsToSpread-fj)/stepsToSpread + (fraction*fj/stepsToSpread); 
							else							
								workingBeatWarpMatrix[i][actualBeat] = (2-fraction)*(fj-stepsToSpread-1.0)/stepsToSpread + (fraction*(trackWarpedStepCount-fj-1.0)/stepsToSpread); 						
						}
					}
				}
			}

			//Process Irrational Rhythm Stuff									
			for(int i = 0; i < TRACK_COUNT; i++) {
				// fprintf(stderr, "Irrational Track:%i dirty:%i \n",i,expanderDataChanged[i]);
				if(expanderDataChanged[i]) {
					for(int j = 0; j < stepsCount[i]; j++) { //reset all irrationals
						workingIrrationalRhythmMatrix[i][j] = 1.0;
					}

					for(int j = 0; j < EXPANDER_MAX_STEPS; j+=3) { // find any irrationals
						int irPos = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 3) + (i * EXPANDER_MAX_STEPS) + j];
						if(irPos != 0) {
							bool useDivs = irPos < 0;
							irPos = std::abs(irPos-1); //Get back to 0 based
							int stepIndex = irPos;
							bool stepFound = true;
							if(useDivs) { //Use j as a count to the div # we are looking for
								if(irPos < beatCount[i]) {
									stepIndex = beatLocation[i][irPos];
									// fprintf(stderr, "Probability Track:%i step:%i BL:%i \n",i,j,stepIndex);
								} else {
									stepFound = false;
								}
							}
											
							if(stepFound) {
								float irNbrSteps = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 3) + (i * EXPANDER_MAX_STEPS) + j + 1];
								float ratio = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 3) + (i * EXPANDER_MAX_STEPS) + j + 2];
								float actualRatio = std::min(ratio/irNbrSteps,1.0f);
								for(int k=0;k<irNbrSteps;k++) {
									workingIrrationalRhythmMatrix[i][stepIndex+k] *= actualRatio; // allow rhythms to nest
								}
							} 
						} else {
							break; // no more irrational rhythms
						}
					}			
				}	
			}

			//set calculated probability and swing 
			//switch to std::copy
			for(int i = 0; i < TRACK_COUNT; i++) {
				if(expanderDataChanged[i] || expanderRecalcNeeded[i]) {
					totalIrrationalAdjustment[i] = 1.0;
					double runningTotal = 0.0;
					for(int j = 0; j < stepsCount[i]; j++) { 
						probabilityMatrix[i][j] = workingProbabilityMatrix[i][j];
						conditionalMatrix[i][j] = workingConditionalMatrix[i][j];
						conditionalModeMatrix[i][j] = workingConditionaModeMatrix[i][j];
						swingMatrix[i][j] = workingSwingMatrix[i][j];
						beatWarpMatrix[i][j] = workingBeatWarpMatrix[i][j];
						irrationalRhythmMatrix[i][j] = workingIrrationalRhythmMatrix[i][j];
						runningTotal += irrationalRhythmMatrix[i][j];
					}
					totalIrrationalAdjustment[i] = runningTotal / stepsCount[i];
				}
				expanderRecalcNeeded[i] = false;
			}
											
		} else {
			if(QARExpanderDisconnectReset) { //If QRE gets disconnected, reset warping, probability and swing
				for(int i = 0; i < TRACK_COUNT; i++) {
					subBeatIndex[i] = 0;
					beatWarping[i] = 1.0;
					beatRotatingAmount[i] = 0.0;
					extraParameterValue[i] = 1.0;
					totalIrrationalAdjustment[i] = 1.0;
					for(int j = 0; j < stepsCount[i]; j++) { //reset all probabilities
						probabilityMatrix[i][j] = 1;
						conditionalMatrix[i][j] = 1;
						conditionalModeMatrix[i][j] = 0;
						swingMatrix[i][j] = 0;
						beatWarpMatrix[i][j] = 1.0;
						irrationalRhythmMatrix[i][j] = 1.0;
						swingRandomness[i] = 0.0f;
					}
				}
				QARExpanderDisconnectReset = false;
			}
		}




		
		float muteInput = inputs[MUTE_INPUT].getVoltage();
		if(!inputs[MUTE_INPUT].isConnected() && masterQARPresent) {
			muteInput = expanderMuteValue;
		}
		muteInput += params[MUTE_PARAM].getValue(); //MUTE BUTTON ALWAYS WORKS		
		if(muteTrigger.process(muteInput)) {
			muted = !muted;
		}
		
			

		//See if need to start up
		for(int trackNumber=0;trackNumber < TRACK_COUNT;trackNumber++) {
			float startInput = 0;
			if(inputs[START_1_INPUT + (trackNumber * 8)].isConnected()) {
				startInput = inputs[START_1_INPUT + (trackNumber * 8)].getVoltage();
			} else if(masterQARPresent) {
				startInput = expanderEocValue[trackNumber];
			} else if(rightExpanderPresent) {
				startInput = lastExpanderEocValue[trackNumber];
			}
			
			if(chainMode != CHAIN_MODE_NONE && (inputs[(trackNumber * 8) + START_1_INPUT].isConnected() || masterQARPresent || slavedQARPresent) && !running[trackNumber]) {
				if(startTrigger[trackNumber].process(startInput)) {
					running[trackNumber] = true;
					beatIndex[trackNumber] = -1;
					lastStepTime[trackNumber] = PTRDIFF_MAX; //Trying some arbitrary large value
				}
			}
		}


		float clockInput = inputs[CLOCK_INPUT].getVoltage();
		if(!inputs[CLOCK_INPUT].isConnected() && masterQARPresent) {
			clockInput = expanderClockValue;
		}

		if((inputs[CLOCK_INPUT].isConnected() || masterQARPresent || manualRun)) {
			//Calculate clock duration
			double timeAdvance = 1.0;

			if(manualRun) {
				duration = 60.0 / bpm * args.sampleRate;
			} else {
				timeElapsed += timeAdvance;
				if(clockTrigger.process(clockInput)) {
					if(firstClockReceived) {
						duration = timeElapsed;
						secondClockReceived = true;
					}
					timeElapsed = 0;
					firstClockReceived = true;							
				} else if(secondClockReceived && timeElapsed > duration) {  //allow absense of second clock to affect duration
					duration = timeElapsed;
				}			
			}
			
			for(int trackNumber=0;trackNumber < TRACK_COUNT;trackNumber++) {
				double beatSizeAdjustment = beatIndex[trackNumber] >= 0 ? 
				       (algorithmMatrix[trackNumber] == WELL_FORMED_ALGO ? wellFormedStepDurations[trackNumber][beatIndex[trackNumber]] : 1.0) * 
					   beatWarpMatrix[trackNumber][beatIndex[trackNumber]] * irrationalRhythmMatrix[trackNumber][beatIndex[trackNumber]] : 1.0;

				if(stepsCount[trackNumber] > 0 && constantTime && !trackIndependent[trackNumber]  ) {
					double constantNumerator = masterTrack <= TRACK_COUNT ? (algorithmMatrix[masterTrack-1] != WELL_FORMED_ALGO ? 
																							masterStepCount : 
																							wellFormedTrackDuration[masterTrack-1]) * totalIrrationalAdjustment[masterTrack-1] : 
																			metaStepCount;
					double constantDenominator;
					if(algorithmMatrix[trackNumber] != WELL_FORMED_ALGO) {
						double stepsChangeAdjustemnt = (double)(lastStepsCount[trackNumber] / (double)stepsCount[trackNumber]); 
						constantDenominator = (double)stepsCount[trackNumber] * totalIrrationalAdjustment[trackNumber] * stepsChangeAdjustemnt;
					} else {
						constantDenominator = wellFormedTrackDuration[trackNumber] * totalIrrationalAdjustment[trackNumber];
					}
					double constantTimeAdjustment = constantNumerator / constantDenominator;

					stepDuration[trackNumber] = duration * beatSizeAdjustment * constantTimeAdjustment; //Constant Time scales duration based on a master track
					// if(masterTrack >= 3) {
					// 	fprintf(stderr, "%i %5.10f  %f\n", trackNumber,masterStepCount,stepDuration[trackNumber]);
					// }
				}
				else {
					stepDuration[trackNumber] = duration * beatSizeAdjustment; //Otherwise Clock based
				}
				
                //swing is affected by next beat
                int nextBeat = beatIndex[trackNumber] + 1;
                if(nextBeat >= stepsCount[trackNumber])
                    nextBeat = 0;
                double swingDuration = (calculatedSwingRandomness[trackNumber] + swingMatrix[trackNumber][nextBeat]) * stepDuration[trackNumber];
                
				if(running[trackNumber]) {
					lastStepTime[trackNumber] +=timeAdvance; //Just chaged below to > instead of >=
					double totalStepDuration = stepDuration[trackNumber] + swingDuration - lastSwingDuration[trackNumber];
					if(stepDuration[trackNumber] > 0.0 && lastStepTime[trackNumber] >= totalStepDuration) {
						lastSwingDuration[trackNumber] = swingDuration;
						lastStepsCount[trackNumber] = stepsCount[trackNumber];
						lastStepTime[trackNumber] -= totalStepDuration;
						lastStepTime[trackNumber] -= trunc(lastStepTime[trackNumber]);
						// lastStepTime[trackNumber] = 0;
						//  fprintf(stderr, "%i %5.10f %5.10f\n", trackNumber,totalStepDuration,lastStepTime[trackNumber]);
						advanceBeat(trackNumber, args.sampleRate);					
					}					
				}	
			}			
		}

		// Set output to current state
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			lights[WAITING_1_LIGHT+trackNumber].value = !running[trackNumber] ? 1.0 : 0.0;


            //Send Out Beat	
			float beatOutputValue = beatPulse[trackNumber].process(args.sampleTime) ? 10.0 : 0;
			if(slavedQARPresent)
				beatOutputValue =  clamp(beatOutputValue + expanderOutputValue[trackNumber],0.0f,10.0f);
            outputs[(trackNumber * 3) + OUTPUT_1].setVoltage(beatOutputValue);	

            //Send out Accent
			float accentOutputValue = accentPulse[trackNumber].process(args.sampleTime) ? 10.0 : 0;
			if(slavedQARPresent)
				accentOutputValue = clamp(accentOutputValue + expanderAccentValue[trackNumber] ,0.0f,10.0f);
            outputs[(trackNumber * 3) + ACCENT_OUTPUT_1].setVoltage(accentOutputValue);	

			//Send out End of Cycle
			float eocOutputValue = eocPulse[trackNumber].process(args.sampleTime) ? 10.0 : 0;
			outputs[(trackNumber * 3) + EOC_OUTPUT_1].setVoltage(eocOutputValue);				
			
			float grooveEocOutputValue = grooveEocPulse[trackNumber].process(args.sampleTime) ? 10.0 : 0;
			outputs[GROOVE_EOC_OUTPUT_1+trackNumber].setVoltage(grooveEocOutputValue);				
			
			if(leftExpanderPresent) {
				float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
				messagesToMother[PASSTHROUGH_OFFSET + 1 + trackNumber * 3] = beatOutputValue; 
				messagesToMother[PASSTHROUGH_OFFSET + 1 + trackNumber * 3 + 1] = accentOutputValue;
				messagesToMother[PASSTHROUGH_OFFSET + 1 + trackNumber * 3 + 2] = rightExpanderPresent ? lastExpanderEocValue[trackNumber] : eocOutputValue; // If last QAR send Eoc Back, otherwise pass through
			} 
			if(rightExpanderPresent) {
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 5 + trackNumber] = eocOutputValue; 				
			}

		}

		//Send outputs to slaves if present		
		if(rightExpanderPresent) {
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			if(sceneChangeMessage > 0) {
				// fprintf(stderr, "Scene Message Sent %i\n", sceneChangeMessage );
			}
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT+ 0] = sceneChangeMessage;
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT+ 1] = true; // tell slave Master is present
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 2] = clockInput; 
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 3] = resetInput; 
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 4] = muteInput; 	
			
			rightExpander.module->leftExpander.messageFlipRequested = true;			
		}
		
		if(leftExpanderPresent) {
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
			messagesToMother[PASSTHROUGH_OFFSET] = true; //Tell Master that slave is present
			
			leftExpander.module->rightExpander.messageFlipRequested = true;
		} 			
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		json_object_set_new(rootJ, "manualRun", json_integer((bool) manualRun));

        for(int i=0;i<TRACK_COUNT;i++) {
			std::string buf = "algorithm-" + std::to_string(i) ;
            json_object_set_new(rootJ, buf.c_str(), json_integer((int) algorithmMatrix[i]));

			buf = "accentAlgorithm-" + std::to_string(i) ;
            json_object_set_new(rootJ, buf.c_str(), json_integer((int) accentAlgorithmMatrix[i]));

			buf = "independent-" + std::to_string(i) ;
            json_object_set_new(rootJ, buf.c_str(), json_integer((int) trackIndependent[i]));

			for(int j=0;j<5;j++) {
				buf = "manualBeat-" + std::to_string(i) + "-" + std::to_string(j) ;
				json_object_set_new(rootJ, buf.c_str(), json_integer((int) manualBeatMatrix[i][j]));				
			}

			for(int j=0;j<5;j++) {
				buf = "manualAccent-" + std::to_string(i) + "-" + std::to_string(j) ;
				json_object_set_new(rootJ, buf.c_str(), json_integer((int) manualAccentMatrix[i][j]));				
			}

        }
        
        json_object_set_new(rootJ, "currentScene", json_integer((int) currentScene));
        json_object_set_new(rootJ, "constantTime", json_integer((bool) constantTime));
		json_object_set_new(rootJ, "masterTrack", json_integer((int) masterTrack));
		json_object_set_new(rootJ, "chainMode", json_integer((int) chainMode));
		json_object_set_new(rootJ, "muted", json_integer((bool) muted));
		json_object_set_new(rootJ, "bpmX4", json_integer((bool) bpmX4));
		json_object_set_new(rootJ, "resetTriggerisHardReset", json_integer((bool) resetTriggerisHardReset));



		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<79;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_object_set_new(rootJ, buf.c_str(), json_real(sceneData[scene][i]));
			}
		}

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *mrJ = json_object_get(rootJ, "manualRun");
		if (mrJ)
			manualRun = json_integer_value(mrJ);

        for(int i=0;i<TRACK_COUNT;i++) {
			std::string buf = "algorithm-" + std::to_string(i) ;
            json_t *ctAl = json_object_get(rootJ, buf.c_str());
            if (ctAl)
                algorithmMatrix[i] = json_integer_value(ctAl);

			buf = "accentAlgorithm-" + std::to_string(i) ;
            json_t *ctAal = json_object_get(rootJ, buf.c_str());
            if (ctAal)
                accentAlgorithmMatrix[i] = json_integer_value(ctAal);

			buf = "independent-" + std::to_string(i) ;
            json_t *ctTi = json_object_get(rootJ, buf.c_str());
            if (ctTi)
                trackIndependent[i] = json_integer_value(ctTi);

			for(int j=0;j<5;j++) {
				buf = "manualBeat-" + std::to_string(i) + "-" + std::to_string(j) ;
				json_t *ctTmm = json_object_get(rootJ, buf.c_str());
				if (ctTmm)
					manualBeatMatrix[i][j] = json_integer_value(ctTmm);

				buf = "manualAccent-" + std::to_string(i) + "-" + std::to_string(j) ;
				json_t *ctTmma = json_object_get(rootJ, buf.c_str());
				if (ctTmma)
					manualAccentMatrix[i][j] = json_integer_value(ctTmma);
			}

        }

		json_t *csJ = json_object_get(rootJ, "currentScene");
		if (csJ) {
			currentScene = json_integer_value(csJ);
			lastScene = currentScene;
		}

		json_t *ctJ = json_object_get(rootJ, "constantTime");
		if (ctJ)
			constantTime = json_integer_value(ctJ);

		json_t *mtJ = json_object_get(rootJ, "masterTrack");
		if (mtJ)
			masterTrack = json_integer_value(mtJ);

		json_t *cmJ = json_object_get(rootJ, "chainMode");
		if (cmJ)
			chainMode = json_integer_value(cmJ);

		json_t *mutedJ = json_object_get(rootJ, "muted");
		if (mutedJ)
			muted = json_integer_value(mutedJ);

		json_t *bpmx4J = json_object_get(rootJ, "bpmX4");
		if (bpmx4J)
			bpmX4 = json_integer_value(bpmx4J);

		json_t *rtmJ = json_object_get(rootJ, "bpmX4");
		if (rtmJ)
			resetTriggerisHardReset = json_integer_value(rtmJ);



		for(int scene=0;scene<NBR_SCENES;scene++) {
			for(int i=0;i<79;i++) {
				std::string buf = "sceneData-" + std::to_string(scene) + "-" + std::to_string(i) ;
				json_t *sdJ = json_object_get(rootJ, buf.c_str());
				if (json_real_value(sdJ)) {
					sceneData[scene][i] = json_real_value(sdJ);
				}
			}
		}
	}

	void setRunningState() {
		for(int trackNumber=0;trackNumber<4;trackNumber++)
		{
			if(chainMode == CHAIN_MODE_EMPLOYEE) { 
				running[trackNumber] = false;
			}
			else {
				running[trackNumber] = true;
			}
		}
	}

	void advanceBeat(int trackNumber,float sampleRate) {
       
		beatIndex[trackNumber]++;
		float pulseLength = std::floor(sampleRate/1000) / sampleRate;
    
		//End of Cycle
		if(beatIndex[trackNumber] >= stepsCount[trackNumber]) {
			beatIndex[trackNumber] = 0;
			beatCountAtIndex[trackNumber] = -1;
			eocPulse[trackNumber].trigger(pulseLength);
			probabilityGroupTriggered[trackNumber] = PENDING_PGTS;
			if(chainMode != CHAIN_MODE_NONE) {
				running[trackNumber] = false;
			}
		}

		if(!trackSwingUsingDivs[trackNumber]) {
			subBeatIndex[trackNumber]++;
			if(subBeatIndex[trackNumber] >= subBeatLength[trackNumber]) { 
				subBeatIndex[trackNumber] = 0;
				grooveEocPulse[trackNumber].trigger(pulseLength);
				expanderRecalcNeeded[trackNumber] = true;
			}
		} else if(trackSwingUsingDivs[trackNumber] && beatMatrix[trackNumber][beatIndex[trackNumber]] == true) {
			subBeatIndex[trackNumber]++;
			if(subBeatIndex[trackNumber] >= subBeatLength[trackNumber]) { 
				subBeatIndex[trackNumber] = 0;
				grooveEocPulse[trackNumber].trigger(pulseLength);
				expanderRecalcNeeded[trackNumber] = true;
			}
		}


        bool probabilityResult = (float) rand()/RAND_MAX < probabilityMatrix[trackNumber][beatIndex[trackNumber]];	
		if(probabilityGroupModeMatrix[trackNumber][beatIndex[trackNumber]] != NONE_PGTM) {
			if(probabilityGroupFirstStep[trackNumber] == beatIndex[trackNumber]) {
				probabilityGroupTriggered[trackNumber] = probabilityResult ? TRIGGERED_PGTS : NOT_TRIGGERED_PGTS;
			} else if(probabilityGroupTriggered[trackNumber] == NOT_TRIGGERED_PGTS) {
				probabilityResult = false;
			}
		}

		conditionalCounterMatrix[trackNumber][beatIndex[trackNumber]] -=1;
		bool conditionalResult = conditionalCounterMatrix[trackNumber][beatIndex[trackNumber]] <= 0;
		if(conditionalResult) {
			conditionalCounterMatrix[trackNumber][beatIndex[trackNumber]] = conditionalMatrix[trackNumber][beatIndex[trackNumber]];
		}

        //Create Beat Trigger    
        if(beatMatrix[trackNumber][beatIndex[trackNumber]] == true && probabilityResult && 
									(conditionalResult != conditionalModeMatrix[trackNumber][beatIndex[trackNumber]]) && 
									running[trackNumber] && !muted) {
            beatPulse[trackNumber].trigger(pulseLength);
			beatCountAtIndex[trackNumber]++;

			//Create Accent Trigger
			if(accentMatrix[trackNumber][beatIndex[trackNumber]] == true) {
				accentPulse[trackNumber].trigger(pulseLength);
			}

        } 


		if(useGaussianDistribution[trackNumber]) {
			bool gaussOk = false; // don't want values that are beyond our mean
			float gaussian;
			do {
				gaussian= _gauss.next();
				gaussOk = gaussian >= -1 && gaussian <= 1;
			} while (!gaussOk);
			//calculatedSwingRandomness[trackNumber] = 1.0 - gaussian / 2 * swingRandomness[trackNumber];
			calculatedSwingRandomness[trackNumber] =  gaussian / 2 * swingRandomness[trackNumber];
		} else {
			//calculatedSwingRandomness[trackNumber] = 1.0 - (((double) rand()/RAND_MAX - 0.5f) * swingRandomness[trackNumber]);
			calculatedSwingRandomness[trackNumber] =  (((double) rand()/RAND_MAX - 0.5f) * swingRandomness[trackNumber]);
		}
	}
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
		for(int i = 0; i < TRACK_COUNT; i++) {
            algorithmMatrix[i] = EUCLIDEAN_ALGO;
			accentAlgorithmMatrix[i] = EUCLIDEAN_ALGO;
			beatIndex[i] = -1;
			stepsCount[i] = MAX_STEPS;
			beatCountAtIndex[i] = -1;
			lastStepTime[i] = 0.0;
			stepDuration[i] = 0.0;
            lastSwingDuration[i] = 0.0;
			expanderAccentValue[i] = 0.0;
			expanderOutputValue[i] = 0.0;
			expanderEocValue[i] = 0; 
			lastExpanderEocValue[i] = 0;
			probabilityGroupTriggered[i] = PENDING_PGTS;
			swingRandomness[i] = 0.0f;
			useGaussianDistribution[i] = false;	
			subBeatIndex[i] = -1;
			extraParameterValue[i] = 1.0;
			running[i] = true;
			for(int j = 0; j < MAX_STEPS; j++) {
				probabilityMatrix[i][j] = 1.0;
				conditionalMatrix[i][j] = 1.0;
				conditionalModeMatrix[i][j] = 0.0;
				conditionalCounterMatrix[i][j] = 0.0;
				swingMatrix[i][j] = 0.0;
				beatMatrix[i][j] = false;
				accentMatrix[i][j] = false;
				beatWarpMatrix[i][j] = 1.0;
				irrationalRhythmMatrix[i][j] = 1.0;
			}
			for(int j = 0; j < 5; j++) {
				manualBeatMatrix[i][j] = 0;
				manualAccentMatrix[i][j] = 0;
			}
			for(int scene=0;scene<NBR_SCENES;scene++) {
				std::fill(sceneData[scene], sceneData[scene]+79, 0.0);
			}
			
		}	
	}
};


struct QARBeatDisplay : FramebufferWidget {
	QuadAlgorithmicRhythm *module;
	int frame = 0;
	float stepThetas[TRACK_COUNT][MAX_STEPS] = {{0}};
	std::shared_ptr<Font> digitalFont;
	std::shared_ptr<Font> textFont;
	std::string digitalFontPath;
	std::string textFontPath;


	const float rotate90 = (M_PI) / 2.0;

	QARBeatDisplay() {
		digitalFontPath = asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf");
		textFontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");

	}

	
	void drawArc(const DrawArgs &args, float trackNumber, float stepsCount, float stepNumber, float runningTrackWidth, int algorithm, bool isBeat, bool isAccent,
					 bool isCurrent, float beatWarp, float probability,int countsLeft, int divideCount, bool conditionalMode, int triggerState, int probabilityGroupMode, 
					 float swing,float nextSwing, float swingRandomness, float wfTrackDurationAdjustment, float wfStepDuration) 
	{		

        float opacity = 0x80; // Testing using opacity for accents

		if(isAccent) {
			opacity = 0xff;
		}

		//TODO: Replace with switch statement
		//Default Euclidean Colors
		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor innerFillColor = nvgRGBA(0xef,0xe0,0,opacity);
		NVGcolor outerFillColor = nvgRGBA(0xef,0xe0,0,0x30);
        if(algorithm == module->GOLUMB_RULER_ALGO) { 
            strokeColor = nvgRGBA(0, 0xe0, 0xef, 0xff);
			innerFillColor = nvgRGBA(0,0xe0,0xef,opacity);
			outerFillColor = nvgRGBA(0,0xe0,0xef,0x30);
        } else if(algorithm == module->WELL_FORMED_ALGO ) { 
            strokeColor = nvgRGBA(0x10, 0xcf, 0x20, 0xff);
			innerFillColor = nvgRGBA(0x10,0x7f,0x20,opacity);        
			outerFillColor = nvgRGBA(0x10,0x7f,0x20,0x30);        
		}  else if(algorithm == module->PERFECT_BALANCE_ALGO ) { 
            strokeColor = nvgRGBA(0xe0, 0x70, 0, 0xff);
			innerFillColor = nvgRGBA(0xe0,0x70, 0,opacity);        
			outerFillColor = nvgRGBA(0xe0,0x70, 0,0x30);        
		}  else if(algorithm == module->BOOLEAN_LOGIC_ALGO ) { 
            strokeColor = nvgRGBA(0xe0, 0, 0xef, 0xff);
			innerFillColor = nvgRGBA(0xe0,0,0xef,opacity);        
			outerFillColor = nvgRGBA(0xe0,0,0xef,0x30);        
		} else if(algorithm == module->MANUAL_MODE_ALGO ) { 
            strokeColor = nvgRGBA(0xe0, 0xe0, 0xef, 0xff);
			innerFillColor = nvgRGBA(0xe0,0xe0,0xef,opacity);        
			outerFillColor = nvgRGBA(0xe0,0xe0,0xef,0x30);        
		} else if(algorithm == module->FIBONACCI_MODE_ALGO ) { 
            strokeColor = nvgRGBA(0x1a, 0x13, 0xc7, 0xff);
			innerFillColor = nvgRGBA(0x1a,0x13,0xc7,opacity);        
			outerFillColor = nvgRGBA(0x1a,0x13,0xc7,0x30);        
		}


		if(isCurrent) {
			strokeColor = nvgRGBA(0x2f, 0xf0, 0, 0xff);
			innerFillColor = nvgRGBA(0x2f,0xf0,0,opacity);			
			outerFillColor = nvgRGBA(0x2f,0xf0,0,0x30);			
		}

        
        NVGcolor randomFillColor = nvgRGBA(0xff,0x00,0,0x40);			
        nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));

        double baseStartDegree = (M_PI * 2.0 / stepsCount * (runningTrackWidth + swing)) - rotate90;
		double wfModifier = algorithm == module->WELL_FORMED_ALGO ? (wfStepDuration*wfTrackDurationAdjustment) : 1.0;
        double baseEndDegree = baseStartDegree + (M_PI * 2.0 / stepsCount * (wfModifier * beatWarp - swing + nextSwing));
		if(baseEndDegree < baseStartDegree) {
			baseEndDegree = baseStartDegree;
        }

		stepThetas[int(trackNumber)][int(stepNumber)] = baseStartDegree;

		// fprintf(stderr, "track:%f step:%f  theta:%f \n", trackNumber,stepNumber,baseEndDegree);

		float baseRadius = 100.0 - (trackNumber*20);			 
        
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.0);
        nvgBeginPath(args.vg);
        nvgArc(args.vg,119,120,baseRadius+20.0,baseStartDegree,baseEndDegree,NVG_CW);
        double x= cos(baseEndDegree) * baseRadius + 119.0;
        double y= sin(baseEndDegree) * baseRadius + 120;
        nvgLineTo(args.vg,x,y);
        nvgArc(args.vg,119,120,baseRadius,baseEndDegree,baseStartDegree,NVG_CCW);
        nvgClosePath(args.vg);		
        nvgStroke(args.vg);

        float startDegree = baseStartDegree;
        float endDegree = baseEndDegree;
        if(isBeat) {
            nvgBeginPath(args.vg);
            nvgStrokeWidth(args.vg, 0.0);
            nvgArc(args.vg,119,120,baseRadius + (20.0 * probability) ,startDegree,endDegree,NVG_CW);
            double x= cos(endDegree) * baseRadius + 119.0;
            double y= sin(endDegree) * baseRadius + 120;
            nvgLineTo(args.vg,x,y);
            nvgArc(args.vg,119,120,baseRadius,endDegree,startDegree,NVG_CCW);
            nvgClosePath(args.vg);		
            // nvgFillColor(args.vg, fillColor);
			NVGpaint paint = nvgRadialGradient(args.vg,119, 120, baseRadius, baseRadius + (20*probability),
					outerFillColor, innerFillColor);
			nvgFillPaint(args.vg, paint);

            nvgFill(args.vg);
            nvgStroke(args.vg);

            //Draw swing randomness
            if(swingRandomness > 0.0f) {
                float width = (baseEndDegree - baseStartDegree) * swingRandomness / 2.0; 
                nvgBeginPath(args.vg);
                nvgStrokeWidth(args.vg, 0.0);
                nvgArc(args.vg,119,120,baseRadius + (20.0 * probability),startDegree,startDegree+width,NVG_CW);
                double srx= cos(startDegree+width) * baseRadius + 119.0;
                double sry= sin(startDegree+width) * baseRadius + 120;
                nvgLineTo(args.vg,srx,sry);
                nvgArc(args.vg,119,120,baseRadius,startDegree+width,startDegree,NVG_CCW);
                nvgFillColor(args.vg, randomFillColor);
                nvgFill(args.vg);
                nvgStroke(args.vg);
            }

			//draw conditional status
			if(divideCount > 1) {
				nvgBeginPath(args.vg);
				nvgStrokeWidth(args.vg, 0.5);
				nvgStrokeColor(args.vg, ((countsLeft > 1) != conditionalMode) ? nvgRGBA(0xff, 0x1f, 0, 0xaf) : nvgRGBA(0x1f, 0xff, 0x1f, 0xaf));				
				nvgFillColor(args.vg, ((countsLeft > 1) != conditionalMode) ? nvgRGBA(0xff, 0x1f, 0, 0xaf) : nvgRGBA(0x1f, 0xff, 0x1f, 0xaf));				
					
				double cx= ((cos(startDegree) * (baseRadius+25) + 119.0) + x) / 2.0;
            	double cy= ((sin(startDegree) * (baseRadius+25) + 120.0) + y) / 2.0;
				nvgCircle(args.vg,cx,cy,5.0);
                nvgStroke(args.vg);
				if(countsLeft <= 1 || countsLeft == divideCount) {
					nvgFill(args.vg);	
				} else {
					nvgBeginPath(args.vg);
					float dcDegree = (M_PI * 2.0f * (countsLeft-1.0) / (divideCount-1.0)) - rotate90;
					nvgMoveTo(args.vg,cx,cy);
					nvgLineTo(args.vg,cx,cy+5.0);                
					nvgArc(args.vg,cx,cy,5,-rotate90,dcDegree,NVG_CW);
					nvgLineTo(args.vg,cx,cy);                
					nvgFill(args.vg);
				}
			}


			if (triggerState == module->NOT_TRIGGERED_PGTS && probabilityGroupMode != module->NONE_PGTM) {
				nvgBeginPath(args.vg);
				nvgStrokeColor(args.vg, nvgRGBA(0xff, 0x1f, 0, 0xaf));
				nvgStrokeWidth(args.vg, 1.0);
				double sx= cos(startDegree) * (baseRadius+20) + 119.0;
            	double sy= sin(startDegree) * (baseRadius+20) + 120;
				nvgMoveTo(args.vg,sx,sy);
				nvgLineTo(args.vg,x,y);
				nvgStroke(args.vg);		
			}
        }
	}

	void drawMasterTrack(const DrawArgs &args, Vec pos, int track) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, digitalFont->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[8];
		if(track <= TRACK_COUNT) {
			snprintf(text, sizeof(text), " %i", track);
		} else {
			snprintf(text, sizeof(text), " X");
		}
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawAlgorithm(const DrawArgs &args, Vec pos, int algorithm) {
		nvgFontSize(args.vg, 8);
		nvgFontFaceId(args.vg, textFont->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_CENTER);
		nvgTextLetterSpacing(args.vg, -1.2);

		std::string algorithmName = "";
		switch(algorithm) {
			case QuadAlgorithmicRhythm::EUCLIDEAN_ALGO :
				algorithmName = "Eucldiean";
				nvgFillColor(args.vg, nvgRGBA(0xef, 0xe0, 0, 0xff));
				break;
			case QuadAlgorithmicRhythm::GOLUMB_RULER_ALGO :
				algorithmName = "Golumb Ruler";
				nvgFillColor(args.vg, nvgRGBA(0, 0xe0, 0xef, 0xff));
				break;
			case QuadAlgorithmicRhythm::WELL_FORMED_ALGO :
				algorithmName = "Well Formed";
				nvgFillColor(args.vg, nvgRGBA(0x10, 0xcf, 0x20, 0xff));
				break;
			case QuadAlgorithmicRhythm::PERFECT_BALANCE_ALGO :
				algorithmName = "Perfect Balance";
				nvgFillColor(args.vg, nvgRGBA(0xe0, 0x70, 0, 0xff));
				break;
			case QuadAlgorithmicRhythm::MANUAL_MODE_ALGO :
				algorithmName = "Manual";
				nvgFillColor(args.vg, nvgRGBA(0xe0, 0xe0, 0xef, 0xff));
				break;
			case QuadAlgorithmicRhythm::BOOLEAN_LOGIC_ALGO :
				algorithmName = "Boolean";
				nvgFillColor(args.vg, nvgRGBA(0xe0, 0, 0xef, 0xff));
				break;
			case QuadAlgorithmicRhythm::FIBONACCI_MODE_ALGO :
				algorithmName = "Fibonacci";
				nvgFillColor(args.vg, nvgRGBA(0x7a, 0x73, 0xfe, 0xff));
				break;
		}
		char text[32];
		snprintf(text, sizeof(text), "%s", algorithmName.c_str() );
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawPatternName(const DrawArgs &args, Vec pos, std::string patternName) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, textFont->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_CENTER);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[32];
		snprintf(text, sizeof(text), "%s", patternName.c_str() );
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawActiveScenes(const DrawArgs &args, Vec pos) {
		nvgStrokeColor(args.vg, nvgRGBA(0x10, 0xff, 0x10, 0xff));
		nvgStrokeWidth(args.vg, 1.0);
		for(int scene=0;scene<NBR_SCENES;scene++) {
			if(module->sceneData[scene][0] != 0 ) {
				double position = 2.0 * M_PI / NBR_SCENES * scene  - M_PI / 2.0; // Rotate 90 degrees
				double x= cos(position) * 18.0 + 120.0;
				double y= sin(position) * 18.0 + 281.5;
				nvgBeginPath(args.vg);
				nvgCircle(args.vg,x,y,6.0);
				nvgStroke(args.vg);
			}
		}

	}


	void draw(const DrawArgs &args) override {
		digitalFont = APP->window->loadFont(digitalFontPath);
		textFont = APP->window->loadFont(textFontPath);

		if (!module)
			return;
		
		for(int trackNumber = 0;trackNumber < TRACK_COUNT;trackNumber++) {
            int algorithn = module->algorithmMatrix[trackNumber];
			float runningTrackWidth = 0.0;
			int stepsCount = module->stepsCount[trackNumber];
			float wfTrackDurationAdjustment = stepsCount / (module->wellFormedTrackDuration[trackNumber]);


			
			drawAlgorithm(args, Vec(330 + trackNumber*82, 18), module->algorithmMatrix[trackNumber]);
			drawPatternName(args, Vec(330 + trackNumber*82, 34), module->trackPatternName[trackNumber]);

			drawAlgorithm(args, Vec(330 + trackNumber*82, 161), module->accentAlgorithmMatrix[trackNumber]);
			drawPatternName(args, Vec(330 + trackNumber*82, 176), module->accentPatternName[trackNumber]);
			// if(trackNumber == 0) {
			// 	fprintf(stderr, "%f %f \n", wfTrackDurationAdjustment, module->wellFormedTrackDuration[trackNumber]);
			// }

            for(int stepNumber = 0;stepNumber < stepsCount;stepNumber++) {	
				int nextStepNumber = (stepNumber + 1) % stepsCount;			
                bool isBeat = module->beatMatrix[trackNumber][stepNumber];
				bool isAccent = module->accentMatrix[trackNumber][stepNumber];
				bool isCurrent = module->beatIndex[trackNumber] == stepNumber && module->running[trackNumber];		
				float probability = module->probabilityMatrix[trackNumber][stepNumber];
				int countsLeft = module->conditionalCounterMatrix[trackNumber][stepNumber];
				int divideCount = module->conditionalMatrix[trackNumber][stepNumber];
				bool conditionalMode = module->conditionalModeMatrix[trackNumber][stepNumber];
				float swing = module->swingMatrix[trackNumber][stepNumber];	
				float nextSwing = module->swingMatrix[trackNumber][nextStepNumber];	
				float swingRandomness = module->swingRandomness[trackNumber];
				float beatWarp = module->beatWarpMatrix[trackNumber][stepNumber];
				float irrationalRhythm = module->irrationalRhythmMatrix[trackNumber][stepNumber];
				float beatSizeAdjust = beatWarp * irrationalRhythm;
				float wfStepDuration = module->wellFormedStepDurations[trackNumber][stepNumber];
				int triggerState = module->probabilityGroupTriggered[trackNumber];
				int probabilityGroupMode = module->probabilityGroupModeMatrix[trackNumber][stepNumber];
				drawArc(args, float(trackNumber), float(stepsCount), float(stepNumber),runningTrackWidth,algorithn,isBeat,isAccent,isCurrent,beatSizeAdjust,
						probability,countsLeft,divideCount,conditionalMode, triggerState,probabilityGroupMode,
						swing,nextSwing,swingRandomness,wfTrackDurationAdjustment,wfStepDuration);
				runningTrackWidth += beatSizeAdjust * (algorithn == module->WELL_FORMED_ALGO ? wfStepDuration * wfTrackDurationAdjustment : 1);
			}
		}	

		drawActiveScenes(args, Vec(136, 327));

		if(module->constantTime)
			drawMasterTrack(args, Vec(216, 280), module->masterTrack);
	}

	void onButton(const event::Button &e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
		bool accent = (e.mods & GLFW_MOD_SHIFT);
		float initX = e.pos.x - 119;
		float initY = e.pos.y - 120;
		float distance = sqrt(initX * initX + initY * initY);
		if(distance >= 40) {
			int trackNumber = (distance-40) / 20.0;
			if (trackNumber < 5 ) {
				trackNumber = 3-trackNumber;
				if((module->algorithmMatrix[trackNumber] == module->MANUAL_MODE_ALGO || !accent) 
					|| (module->accentAlgorithmMatrix[trackNumber] == module->MANUAL_MODE_ALGO && accent)) {
					float theta = atan2(initY,initX);
					if(theta <= -M_PI_2) {
						theta += (M_PI * 2);
					}
					int stepNumber = module->stepsCount[trackNumber]-1;
					for(int i=1;i<module->stepsCount[trackNumber];i++) {
						if(theta < stepThetas[trackNumber][i]) {
							stepNumber = i-1;
							break;
						}
					}
					module->toggleManualBeat(trackNumber,stepNumber,accent);
					e.consume(this);
					// fprintf(stderr, "click distance:%f theta:%f track:%i step:%i \n", distance, theta, trackNumber,stepNumber);
				}
			}
		}
    }
  }
};


struct QuadAlgorithmicRhythmWidget : ModuleWidget {
	QuadAlgorithmicRhythm *module_;

	QuadAlgorithmicRhythmWidget(QuadAlgorithmicRhythm *module) {
		setModule(module);
		module_ = module;

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadAlgorithmicRhythm.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		{
			QARBeatDisplay *display = new QARBeatDisplay();
			display->module = module;
			display->box.pos = Vec(10, 20); // was 16,34
			display->box.size = Vec(box.size.x-10, 365);
			addChild(display);
		}

		for(int track=0;track<TRACK_COUNT;track++) {
			addParam(createParam<LEDButton>(Vec(296+(track*82), 27), module, QuadAlgorithmicRhythm::ALGORITHM_1_PARAM+(track*8)));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(297.5+(track*82), 28.5), module, QuadAlgorithmicRhythm::ALGORITHM_1_LIGHT+(track*3)));
			ParamWidget* stepsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(316+(track*82), 63), module, QuadAlgorithmicRhythm::STEPS_1_PARAM+(track*8));		
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(stepsParam)->percentage = &module->stepsPercentage[track];
			}
			addParam(stepsParam);							
			ParamWidget* divisionsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(316+(track*82), 88), module, QuadAlgorithmicRhythm::DIVISIONS_1_PARAM+(track*8));
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(divisionsParam)->percentage = &module->divisionsPercentage[track];
			}
			addParam(divisionsParam);							
			ParamWidget* offsetParam = createParam<RoundReallySmallFWSnapKnob>(Vec(316+(track*82), 113), module, QuadAlgorithmicRhythm::OFFSET_1_PARAM+(track*8));		
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(offsetParam)->percentage = &module->offsetPercentage[track];
			}
			addParam(offsetParam);							
			ParamWidget* padParam = createParam<RoundReallySmallFWSnapKnob>(Vec(316+(track*82), 138), module, QuadAlgorithmicRhythm::PAD_1_PARAM+(track*8));		
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(padParam)->percentage = &module->padPercentage[track];
			}
			addParam(padParam);							

			addParam(createParam<LEDButton>(Vec(296+(track*82), 169), module, QuadAlgorithmicRhythm::ACCENT_ALGORITHM_1_PARAM+track));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(297.5+(track*82), 170.5), module, QuadAlgorithmicRhythm::ACCENT_ALGORITHM_1_LIGHT+track*3));

			ParamWidget* accentDivisionsParam = createParam<RoundReallySmallFWSnapKnob>(Vec(316+(track*82), 203), module, QuadAlgorithmicRhythm::ACCENTS_1_PARAM+(track*8));
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(accentDivisionsParam)->percentage = &module->accentDivisionsPercentage[track];
			}
			addParam(accentDivisionsParam);							
			ParamWidget* accentRotateParam = createParam<RoundReallySmallFWSnapKnob>(Vec(316+(track*82), 228), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_PARAM+(track*8));
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(accentRotateParam)->percentage = &module->accentRotatePercentage[track];
			}
			addParam(accentRotateParam);							

			addParam(createParam<LEDButton>(Vec(317+(track*82), 253), module, QuadAlgorithmicRhythm::TRACK_1_INDEPENDENT_PARAM+(track*8)));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(318.5+(track*82), 254.5), module, QuadAlgorithmicRhythm::TRACK_INDEPENDENT_1_LIGHT+(track*3)));

			addInput(createInput<FWPortInSmall>(Vec(296+(track*82), 46), module, QuadAlgorithmicRhythm::ALGORITHM_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(341+(track*82), 65), module, QuadAlgorithmicRhythm::STEPS_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(341+(track*82), 90), module, QuadAlgorithmicRhythm::DIVISIONS_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(341+(track*82), 115), module, QuadAlgorithmicRhythm::OFFSET_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(341+(track*82), 140), module, QuadAlgorithmicRhythm::PAD_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(296+(track*82), 188), module, QuadAlgorithmicRhythm::ACCENT_ALGORITHM_1_INPUT+track));
			addInput(createInput<FWPortInSmall>(Vec(341+(track*82), 205), module, QuadAlgorithmicRhythm::ACCENTS_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(341+(track*82), 230), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_INPUT+(track*8)));

			addInput(createInput<FWPortInSmall>(Vec(317+(track*82), 277), module, QuadAlgorithmicRhythm::START_1_INPUT+(track*8)));
			addOutput(createOutput<FWPortOutSmall>(Vec(317+(track*82), 299), module, QuadAlgorithmicRhythm::EOC_OUTPUT_1+(track*3)));
			addOutput(createOutput<FWPortOutSmall>(Vec(341+(track*82), 299), module, QuadAlgorithmicRhythm::GROOVE_EOC_OUTPUT_1+track));
			addOutput(createOutput<FWPortOutSmall>(Vec(317+(track*82), 321), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_1+(track*3)));
			addOutput(createOutput<FWPortOutSmall>(Vec(317+(track*82), 343), module, QuadAlgorithmicRhythm::OUTPUT_1+(track*3)));

			addChild(createLight<SmallLight<RedLight>>(Vec(338+(track*82), 353), module, QuadAlgorithmicRhythm::WAITING_1_LIGHT+track));

		}

		addParam(createParam<FWLEDButton>(Vec(12, 279.5), module, QuadAlgorithmicRhythm::CHAIN_MODE_PARAM));
		addChild(createLight<MediumLight<BlueLight>>(Vec(13.5, 281), module, QuadAlgorithmicRhythm::CHAIN_MODE_NONE_LIGHT));
		addParam(createParam<FWLEDButton>(Vec(12, 294.5), module, QuadAlgorithmicRhythm::CHAIN_MODE_PARAM+1));
		addChild(createLight<MediumLight<GreenLight>>(Vec(13.5, 296), module, QuadAlgorithmicRhythm::CHAIN_MODE_BOSS_LIGHT));
		addParam(createParam<FWLEDButton>(Vec(12, 309.5), module, QuadAlgorithmicRhythm::CHAIN_MODE_PARAM+2));
		addChild(createLight<MediumLight<RedLight>>(Vec(13.5, 311), module, QuadAlgorithmicRhythm::CHAIN_MODE_EMPLOYEE_LIGHT));

		addParam(createParam<CKD6>(Vec(176, 278), module, QuadAlgorithmicRhythm::CONSTANT_TIME_MODE_PARAM));

		addParam(createParam<LEDButton>(Vec(81, 359), module, QuadAlgorithmicRhythm::RUN_PARAM));
		addParam(createParam<TL1105>(Vec(118.5, 360), module, QuadAlgorithmicRhythm::RESET_PARAM));
		addParam(createParam<LEDButton>(Vec(157, 359), module, QuadAlgorithmicRhythm::MUTE_PARAM));
		

		addInput(createInput<FWPortInSmall>(Vec(13, 340), module, QuadAlgorithmicRhythm::CLOCK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(48, 340), module, QuadAlgorithmicRhythm::BPM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(81, 340), module, QuadAlgorithmicRhythm::RUN_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(118, 340), module, QuadAlgorithmicRhythm::RESET_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(157, 340), module, QuadAlgorithmicRhythm::MUTE_INPUT));


		ParamWidget* metaStepParam = createParam<RoundSmallFWSnapKnob>(Vec(190, 334), module, QuadAlgorithmicRhythm::META_STEP_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(metaStepParam)->percentage = &module->metaStepCountPercentage;
		}
		addParam(metaStepParam);							
		addInput(createInput<FWPortInSmall>(Vec(220, 338), module, QuadAlgorithmicRhythm::META_STEP_INPUT));


		// addChild(createLight<LargeLight<GreenLight>>(Vec(50, 362), module, QuadAlgorithmicRhythm::IS_MASTER_LIGHT));
		// addChild(createLight<LargeLight<RedLight>>(Vec(80, 362), module, QuadAlgorithmicRhythm::IS_SLAVE_LIGHT));

		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(82, 360.5), module, QuadAlgorithmicRhythm::MANUAL_RUN_LIGHT));
		addChild(createLight<LargeLight<RedLight>>(Vec(158, 360.5), module, QuadAlgorithmicRhythm::MUTED_LIGHT));

		for(int scene=0;scene<NBR_SCENES;scene++) {
			double position = 2.0 * M_PI / NBR_SCENES * scene  - M_PI / 2.0; // Rotate 90 degrees

			double x= cos(position) * 18.0 + 124.0;
			double y= sin(position) * 18.0 + 295.5;

			addParam(createParam<FWLEDButton>(Vec(x, y), module, QuadAlgorithmicRhythm::CHOOSE_SCENE_PARAM+scene));
			addChild(createLight<MediumLight<GreenLight>>(Vec(x+1.5, y+1.5), module, QuadAlgorithmicRhythm::SCENE_ACTIVE_LIGHT+scene));
		}
		addInput(createInput<FWPortInSmall>(Vec(152, 306), module, QuadAlgorithmicRhythm::CHOOSE_SCENE_INPUT));
		
		addParam(createParam<LEDButton>(Vec(121, 292.5), module, QuadAlgorithmicRhythm::SAVE_SCENE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(122.5, 294), module, QuadAlgorithmicRhythm::SAVE_MODE_LIGHT));
	}	

	void step() override {
		if(module_) {
			module_->modsKeys = APP->window->getMods();
		}
	}

	struct BPMX4Item : MenuItem {
		QuadAlgorithmicRhythm *module;
		void onAction(const event::Action &e) override {
			module->bpmX4 = !module->bpmX4;
		}
		void step() override {
			text = "BPM x4";
			rightText = (module->bpmX4) ? "✔" : "";
		}
	};

	
	struct ResetTriggerModeItem : MenuItem {
		QuadAlgorithmicRhythm *module;
		void onAction(const event::Action &e) override {
			module->resetTriggerisHardReset = !module->resetTriggerisHardReset;
		}
		void step() override {
			text = "Reset to Step 0";
			rightText = (module->resetTriggerisHardReset) ? "✔" : "";
		}
	};


	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		QuadAlgorithmicRhythm *module = dynamic_cast<QuadAlgorithmicRhythm*>(this->module);
		assert(module);

		ResetTriggerModeItem *resetTriggerModeItem = new ResetTriggerModeItem();
		resetTriggerModeItem->module = module;
		menu->addChild(resetTriggerModeItem);

		BPMX4Item *bpmX4Item = new BPMX4Item();
		bpmX4Item->module = module;
		menu->addChild(bpmX4Item);

	}
};

Model *modelQuadAlgorithmicRhythm = createModel<QuadAlgorithmicRhythm, QuadAlgorithmicRhythmWidget>("QuadAlgorithmicRhythm");
