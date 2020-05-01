#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "dsp-noise/noise.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 73
#define NUM_ALGORITHMS 3
#define EXPANDER_MAX_STEPS 18
#define NUM_RULERS 20
#define MAX_DIVISIONS 11
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 9
#define PASSTHROUGH_OFFSET EXPANDER_MAX_STEPS * TRACK_COUNT * 3 + TRACK_LEVEL_PARAM_COUNT

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
		STEPS_2_PARAM,
		DIVISIONS_2_PARAM,
		OFFSET_2_PARAM,
		PAD_2_PARAM,
		ACCENTS_2_PARAM,
		ACCENT_ROTATE_2_PARAM,
		ALGORITHM_2_PARAM,
		STEPS_3_PARAM,
		DIVISIONS_3_PARAM,
		OFFSET_3_PARAM,
		PAD_3_PARAM,
		ACCENTS_3_PARAM,
		ACCENT_ROTATE_3_PARAM,
		ALGORITHM_3_PARAM,
		STEPS_4_PARAM,
		DIVISIONS_4_PARAM,
		OFFSET_4_PARAM,
		PAD_4_PARAM,
		ACCENTS_4_PARAM,
		ACCENT_ROTATE_4_PARAM,
		ALGORITHM_4_PARAM,
		CHAIN_MODE_PARAM,	
		CONSTANT_TIME_MODE_PARAM,	
		RESET_PARAM,
		MUTE_PARAM,
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
		NUM_INPUTS
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
		TEST_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
        ALGORITHM_1_LIGHT,
        ALGORITHM_2_LIGHT = ALGORITHM_1_LIGHT+3,
        ALGORITHM_3_LIGHT = ALGORITHM_2_LIGHT+3,
        ALGORITHM_4_LIGHT = ALGORITHM_3_LIGHT+3,
		CHAIN_MODE_NONE_LIGHT = ALGORITHM_4_LIGHT+3,
		CHAIN_MODE_BOSS_LIGHT,
		CHAIN_MODE_EMPLOYEE_LIGHT,
		MUTED_LIGHT,
		CONSTANT_TIME_LIGHT,
		WAITING_1_LIGHT,
		WAITING_2_LIGHT,
		WAITING_3_LIGHT,
		WAITING_4_LIGHT,
		// IS_MASTER_LIGHT,
		// IS_SLAVE_LIGHT,
		NUM_LIGHTS
	};
	enum ChainModes {
		CHAIN_MODE_NONE,
		CHAIN_MODE_BOSS,
		CHAIN_MODE_EMPLOYEE
	};
	enum Algorithms {
		EUCLIDEAN_ALGO,
		GOLUMB_RULER_ALGO,
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

	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	
    int algorithnMatrix[TRACK_COUNT];
	bool beatMatrix[TRACK_COUNT][MAX_STEPS];
	bool accentMatrix[TRACK_COUNT][MAX_STEPS];

	float probabilityMatrix[TRACK_COUNT][MAX_STEPS];
	float swingMatrix[TRACK_COUNT][MAX_STEPS];
	float beatWarpMatrix[TRACK_COUNT][MAX_STEPS];
	float probabilityGroupModeMatrix[TRACK_COUNT][MAX_STEPS];
	int probabilityGroupTriggered[TRACK_COUNT];
	int probabilityGroupFirstStep[TRACK_COUNT];
	float workingProbabilityMatrix[TRACK_COUNT][MAX_STEPS];
	float workingSwingMatrix[TRACK_COUNT][MAX_STEPS];
	float workingBeatWarpMatrix[TRACK_COUNT][MAX_STEPS];

	int beatIndex[TRACK_COUNT];
	int beatLocation[TRACK_COUNT][MAX_STEPS] = {{0}};
	int beatCount[TRACK_COUNT] = {0};
	int beatCountAtIndex[TRACK_COUNT] = {-1};
	int stepsCount[TRACK_COUNT];
	int lastStepsCount[TRACK_COUNT];
	double stepDuration[TRACK_COUNT];
	double lastStepTime[TRACK_COUNT];	
    double lastSwingDuration[TRACK_COUNT];

	float swingRandomness[TRACK_COUNT];
	bool useGaussianDistribution[TRACK_COUNT] = {false};
	double calculatedSwingRandomness[TRACK_COUNT] = {0.0f};
	bool trackSwingUsingDivs[TRACK_COUNT] = {false};
	int subBeatLength[TRACK_COUNT];
	int subBeatIndex[TRACK_COUNT];

	float beatWarping[TRACK_COUNT];
	int beatWarpingPosition[TRACK_COUNT];




	float expanderOutputValue[TRACK_COUNT];
	float expanderAccentValue[TRACK_COUNT];
	float expanderEocValue[TRACK_COUNT];
	float lastExpanderEocValue[TRACK_COUNT];
	
	float expanderClockValue = 0;
	float expanderResetValue = 0;
	float expanderMuteValue = 0;

	double maxStepCount;
	double masterStepCount;
	bool slaveQARsPresent;
	bool masterQARPresent;

	const char* trackNames[TRACK_COUNT] {"1","2","3","4"};

    const int rulerOrders[NUM_RULERS] = {1,2,3,4,5,5,6,6,6,6,7,7,7,7,7,8,9,10,11,11};
	const int rulerLengths[NUM_RULERS] = {0,1,3,6,11,11,17,17,17,17,25,25,25,25,25,34,44,55,72,72};
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

	dsp::SchmittTrigger clockTrigger,resetTrigger,chainModeTrigger,constantTimeTrigger,muteTrigger,algorithmButtonTrigger[TRACK_COUNT],algorithmInputTrigger[TRACK_COUNT],startTrigger[TRACK_COUNT];
	dsp::PulseGenerator beatPulse[TRACK_COUNT],accentPulse[TRACK_COUNT],eocPulse[TRACK_COUNT];

	GaussianNoiseGenerator _gauss;



	QuadAlgorithmicRhythm() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

	
		configParam(STEPS_1_PARAM, 1.0, MAX_STEPS, 16.0,"Track 1 Steps");
		configParam(DIVISIONS_1_PARAM, 0.0, MAX_STEPS, 2.0,"Track 1 Divisions");
		configParam(OFFSET_1_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 1 Step Offset");
		configParam(PAD_1_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 1 Step Padding");
		configParam(ACCENTS_1_PARAM, 0.0, MAX_STEPS, 0.0, "Track 1 Accents");
		configParam(ACCENT_ROTATE_1_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 1 Acccent Rotation");
        configParam(ALGORITHM_1_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_2_PARAM, 1.0, MAX_STEPS, 16.0,"Track 2 Steps");
		configParam(DIVISIONS_2_PARAM, 0.0, MAX_STEPS, 2.0,"Track 2 Divisions");
		configParam(OFFSET_2_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 2 Step Offset");
		configParam(PAD_2_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 2 Step Padding");
		configParam(ACCENTS_2_PARAM, 0.0, MAX_STEPS, 0.0, "Track 2 Accents");
		configParam(ACCENT_ROTATE_2_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 2 Acccent Rotation");
        configParam(ALGORITHM_2_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_3_PARAM, 1.0, MAX_STEPS, 16.0,"Track 3 Steps");
		configParam(DIVISIONS_3_PARAM, 0.0, MAX_STEPS, 2.0,"Track 3 Divisions");
		configParam(OFFSET_3_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 3 Step Offset");
		configParam(PAD_3_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 3 Step Padding");
		configParam(ACCENTS_3_PARAM, 0.0, MAX_STEPS, 0.0, "Track 3 Accents");
		configParam(ACCENT_ROTATE_3_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 3 Acccent Rotation");
        configParam(ALGORITHM_3_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_4_PARAM, 1.0, MAX_STEPS, 16.0,"Track 4 Steps");
		configParam(DIVISIONS_4_PARAM, 0.0, MAX_STEPS, 2.0,"Track 4 Divisions");
		configParam(OFFSET_4_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 4 Step Offset");
		configParam(PAD_4_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 4 Step Padding");
		configParam(ACCENTS_4_PARAM, 0.0, MAX_STEPS, 0.0, "Track 4 Accents");
		configParam(ACCENT_ROTATE_4_PARAM, 0.0, MAX_STEPS-1.0, 0.0,"Track 4 Acccent Rotation");
        configParam(ALGORITHM_4_PARAM, 0.0, 1.0, 0.0);

		configParam(CHAIN_MODE_PARAM, 0.0, 1.0, 0.0);
		configParam(CONSTANT_TIME_MODE_PARAM, 0.0, 1.0, 0.0);

		configParam(RESET_PARAM, 0.0, 1.0, 0.0);
		configParam(MUTE_PARAM, 0.0, 1.0, 0.0);

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
		
		srand(time(NULL));
		

		for(int i = 0; i < TRACK_COUNT; i++) {
            algorithnMatrix[i] = 0;
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

			expanderOutputValue[i] = 0.0;
			expanderAccentValue[i] = 0.0;
			expanderEocValue[i] = 0.0;
			lastExpanderEocValue[i] = 0.0;

			running[i] = true;
			for(int j = 0; j < MAX_STEPS; j++) {
				probabilityMatrix[i][j] = 1.0;
				swingMatrix[i][j] = 0.0;
				beatWarpMatrix[i][j] = 1.0;
				beatMatrix[i][j] = false;
				accentMatrix[i][j] = false;				
			}
		}	

        onReset();	
	}

	void process(const ProcessArgs &args) override  {

		//Initialize
		for(int i = 0; i < TRACK_COUNT; i++) {
			expanderOutputValue[i] = 0; 
			expanderAccentValue[i] = 0; 
			lastExpanderEocValue[i] = 0;
		}
		//See if a slave is passing through an expander
		bool slavedQARPresent = false;
		bool rightExpanderPresent = (rightExpander.module 
		&& (rightExpander.module->model == modelQuadAlgorithmicRhythm || rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARWarpedSpaceExpander));
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
		bool leftExpanderPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm
		 || leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || leftExpander.module->model == modelQARWarpedSpaceExpander));
		if(leftExpanderPresent)
		{			
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			masterQARPresent = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT]; 

			if(masterQARPresent) {			
				expanderClockValue = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 1] ; 
				expanderResetValue = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 2] ; 
				expanderMuteValue = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 3] ; 

				for(int i = 0; i < TRACK_COUNT; i++) {				
					expanderEocValue[i] = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 4 + i] ; 
				}
			}
		}


		//Set startup state	
		if(!initialized) {
			setRunningState();
			initialized = true;
		}

		// Modes
		if (constantTimeTrigger.process(params[CONSTANT_TIME_MODE_PARAM].getValue())) {
			masterTrack = (masterTrack + 1) % 5;
			constantTime = masterTrack > 0;
			for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
				beatIndex[trackNumber] = -1;
                lastStepTime[trackNumber] = 0;
                lastSwingDuration[trackNumber] = 0; // Not sure about this
				expanderEocValue[trackNumber] = 0; 
				lastExpanderEocValue[trackNumber] = 0;		
			}
			
		}
		

		

		if (chainModeTrigger.process(params[CHAIN_MODE_PARAM].getValue())) {
			chainMode = (chainMode + 1) % 3;
			setRunningState();
		}		
		lights[CHAIN_MODE_NONE_LIGHT].value = chainMode == CHAIN_MODE_NONE ? 1.0 : 0.0;
		lights[CHAIN_MODE_BOSS_LIGHT].value = chainMode == CHAIN_MODE_BOSS ? 1.0 : 0.0;
		lights[CHAIN_MODE_EMPLOYEE_LIGHT].value = chainMode == CHAIN_MODE_EMPLOYEE ? 1.0 : 0.0;

		lights[MUTED_LIGHT].value = muted ? 1.0 : 0.0;

		maxStepCount = 0;
		masterStepCount = 0;

		for(int trackNumber=0;trackNumber<4;trackNumber++) {
            if(algorithmButtonTrigger[trackNumber].process(params[(ALGORITHM_1_PARAM + trackNumber * 7)].getValue())) {
                algorithnMatrix[trackNumber] = (algorithnMatrix[trackNumber] + 1) % (trackNumber < 2 ? NUM_ALGORITHMS -1 : NUM_ALGORITHMS); //Only tracks 3 and 4 get logic
            }
            if(algorithmInputTrigger[trackNumber].process(inputs[(ALGORITHM_1_INPUT + trackNumber * 8)].getVoltage())) {
                algorithnMatrix[trackNumber] = (algorithnMatrix[trackNumber] + 1) % (trackNumber < 2 ? NUM_ALGORITHMS -1 : NUM_ALGORITHMS); //Only tracks 3 and 4 get logic
            }

			switch (algorithnMatrix[trackNumber]) {
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

				case BOOLEAN_LOGIC_ALGO :
					lights[ALGORITHM_1_LIGHT + trackNumber*3].value = .875;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = 0;
					lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = .9;
					break;

			}		
            
            
			//clear out the matrix and levels
			for(int j=0;j<MAX_STEPS;j++)
			{
				beatLocation[trackNumber][j] = 0;
			}

			float stepsCountf = std::floor(params[(trackNumber * 7) + STEPS_1_PARAM].getValue());			
			if(inputs[trackNumber * 8].isConnected()) {
				stepsCountf += inputs[trackNumber * 8 + STEPS_1_INPUT].getVoltage() * 1.8;
			}
			stepsCountf = clamp(stepsCountf,1.0f,float(MAX_STEPS));
			if(algorithnMatrix[trackNumber] == BOOLEAN_LOGIC_ALGO) { // Boolean Tracks can't exceed length of the tracks they are based (-1 and -2)
				stepsCountf = std::min(stepsCountf,(float)std::min(stepsCount[trackNumber-1],stepsCount[trackNumber-2]));
			}

			float divisionf = std::floor(params[(trackNumber * 7) + DIVISIONS_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].isConnected()) {
				divisionf += inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].getVoltage() * 1.7;
			}		
			divisionf = clamp(divisionf,0.0f,stepsCountf);

			float offsetf = std::floor(params[(trackNumber * 7) + OFFSET_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + OFFSET_1_INPUT].isConnected()) {
				offsetf += inputs[(trackNumber * 8) + OFFSET_1_INPUT].getVoltage() * 1.7;
			}	
			offsetf = clamp(offsetf,0.0f,MAX_STEPS-1.0f);

			float padf = std::floor(params[trackNumber * 7 + PAD_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + PAD_1_INPUT].isConnected()) {
				padf += inputs[trackNumber * 8 + PAD_1_INPUT].getVoltage() * 1.7;
			}
			padf = clamp(padf,0.0f,stepsCountf -1);
			// Reclamp
			divisionf = clamp(divisionf,0.0f,stepsCountf-padf);



			//Use this to reduce range of accent params/inputs so the range of motion of knob/modulation is more useful.
			float divisionScale = 1;
			if(stepsCountf > 0) {
				divisionScale = divisionf / stepsCountf;
			}		

			float accentDivisionf = std::floor(params[(trackNumber * 7) + ACCENTS_1_PARAM].getValue() * divisionScale);
			if(inputs[(trackNumber * 8) + ACCENTS_1_INPUT].isConnected()) {
				accentDivisionf += inputs[(trackNumber * 8) + ACCENTS_1_INPUT].getVoltage() * divisionScale;
			}
			accentDivisionf = clamp(accentDivisionf,0.0f,divisionf);

			float accentRotationf = std::floor(params[(trackNumber * 7) + ACCENT_ROTATE_1_PARAM].getValue() * divisionScale);
			if(inputs[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].isConnected()) {
				accentRotationf += inputs[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].getVoltage() * divisionScale;
			}
			if(divisionf > 0) {
				accentRotationf = clamp(accentRotationf,0.0f,divisionf-1);			
			} else {
				accentRotationf = 0;
			}	

			if(stepsCountf > maxStepCount)
				maxStepCount = std::floor(stepsCountf);
			if(trackNumber == masterTrack - 1)
				masterStepCount = std::floor(stepsCountf);		

			stepsCount[trackNumber] = int(stepsCountf);
			if(lastStepsCount[trackNumber] == -1) //first time
				lastStepsCount[trackNumber] = stepsCount[trackNumber];

			int division = int(divisionf);
			int offset = int(offsetf);		
			int pad = int(padf);
			int accentDivision = int(accentDivisionf);
			int accentRotation = int(accentRotationf);


			beatCount[trackNumber] = 0;
			if(division > 0) {				
                int bucket = stepsCount[trackNumber] - pad - 1;                    
                if(algorithnMatrix[trackNumber] == EUCLIDEAN_ALGO ) { //Euclidean Algorithn
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
                            euclideanBeatIndex++;	
                        } else
                        {
                            beatMatrix[trackNumber][((euclideanStepIndex + offset + pad) % (stepsCount[trackNumber]))] = false;	
                        }                        
                    }
					beatCount[trackNumber] = euclideanBeatIndex;
                } else if(algorithnMatrix[trackNumber] == GOLUMB_RULER_ALGO) { //Golomb Ruler Algorithm
				
                    int rulerToUse = clamp(division-1,0,NUM_RULERS-1);
                    int actualStepCount = stepsCount[trackNumber] - pad;
                    while(rulerLengths[rulerToUse] + 1 > actualStepCount && rulerToUse >= 0) {
                        rulerToUse -=1;
                    } 
                    
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
                } else { //Boolean Logic only for tracs 3 and 4
					int logicBeatCount = 0;
					int logicMode = (division-1) % 6; 

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
								isBeat = !(beatMatrix[trackNumber-1][logicBeatIndex] || beatMatrix[trackNumber-2][logicBeatIndex]); //NOR
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

				bucket = division - 1;
				for(int accentIndex = 0; accentIndex < division; accentIndex++)
				{
					bucket += accentDivision;
					if(bucket >= division) {
						bucket -= division;
						accentMatrix[trackNumber][beatLocation[trackNumber][(accentIndex + accentRotation) % division]] = true;				
					} else
					{
						accentMatrix[trackNumber][beatLocation[trackNumber][(accentIndex + accentRotation) % division]] = false;
					}
					
				}	        	
			} else {
				//Set all beats to false
				for(int j=0;j<MAX_STEPS;j++)
				{
					beatMatrix[trackNumber][j] = false; 			
				}
			}	
		}

		float resetInput = inputs[RESET_INPUT].getVoltage();
		if(!inputs[RESET_INPUT].isConnected() && masterQARPresent) {
			resetInput = expanderResetValue;
		}
		resetInput += params[RESET_PARAM].getValue(); //RESET BUTTON ALWAYS WORKS		
		if(resetTrigger.process(resetInput)) {
			for(int trackNumber=0;trackNumber<4;trackNumber++)
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
			}
			timeElapsed = 0;
			firstClockReceived = false;
			duration = 0;
			setRunningState();
		}
		

		

		//Get Expander Info
		if(rightExpander.module && (rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || rightExpander.module->model == modelQARWarpedSpaceExpander))
		{			
			QARExpanderDisconnectReset = true;
			float *messagesFromExpanders = (float*)rightExpander.consumerMessage;

			//Process Probability Expander Stuff						
			for(int i = 0; i < TRACK_COUNT; i++) {
				probabilityGroupFirstStep[i] = -1;
				for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities, find first group step
					workingProbabilityMatrix[i][j] = 1;					
				}

				if(messagesFromExpanders[i] > 0) { // 0 is track not selected
					bool useDivs = messagesFromExpanders[i] == 2; //2 is divs
					for(int j = 0; j < stepsCount[i]; j++) { // Assign probabilites and swing
						int stepIndex = j;
						bool stepFound = true;
						if(useDivs) { //Use j as a count to the div # we are looking for
							stepIndex = beatLocation[i][j];
						}
						
						int messageIndex = j % EXPANDER_MAX_STEPS;

						if(stepFound) {
							float probability = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (i * EXPANDER_MAX_STEPS) + messageIndex];
							float probabilityMode = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT) + (i * EXPANDER_MAX_STEPS) + messageIndex];
							
							workingProbabilityMatrix[i][stepIndex] = probability;
							probabilityGroupModeMatrix[i][stepIndex] = probabilityMode;
							for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities, find first group step
								if(probabilityGroupFirstStep[i] < 0 && probabilityGroupModeMatrix[i][j] != NONE_PGTM ) {
									probabilityGroupFirstStep[i] = j;
									break;
								}
							}
						} 
					}
				}
			}

			//Process Groove Expander Stuff									
			for(int i = 0; i < TRACK_COUNT; i++) {
				for(int j = 0; j < MAX_STEPS; j++) { //reset all swing
					workingSwingMatrix[i][j] = 0.0;
				}

				if(messagesFromExpanders[TRACK_COUNT + i] > 0) { // 0 is track not selected
					bool useDivs = messagesFromExpanders[TRACK_COUNT + i] == 2; //2 is divs
					trackSwingUsingDivs[i] = useDivs;

					int grooveLength = (int)(messagesFromExpanders[TRACK_COUNT * 2 + i]);
					bool useTrackLength = messagesFromExpanders[TRACK_COUNT * 3 + i];

					swingRandomness[i] = messagesFromExpanders[TRACK_COUNT * 4 + i];
					useGaussianDistribution[i] = messagesFromExpanders[TRACK_COUNT * 5 + i];

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

							// if(i==0) {
							// 	fprintf(stderr, "%i %i %i %i \n", beatCount[i],subBeatIndex[i],beatCountAtIndex[i],beatIndex[i]);
							// }

					for(int j = 0; j < MAX_STEPS; j++) { // Assign probabilites and swing	
						int stepIndex = j;
						bool stepFound = true;
						if(useDivs) { //Use j as a count to the div # we are looking for
							if(j < beatCount[i]) { //hard coding an 8 for test
								stepIndex = beatLocation[i][j];
							} else {
								stepFound = false;
							}
						}
						
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

			//Process Warped Space Stuff									
			for(int i = 0; i < TRACK_COUNT; i++) {
				for(int j = 0; j < MAX_STEPS; j++) { //reset all warping
					workingBeatWarpMatrix[i][j] = 1.0;
				}

				if(messagesFromExpanders[TRACK_COUNT * 6 + i] > 0) { // 0 is track not selected
					beatWarping[i] = messagesFromExpanders[TRACK_COUNT * 7 + i];
					beatWarpingPosition[i] = (int)messagesFromExpanders[TRACK_COUNT * 8 + i];
					float trackStepCount = (float)stepsCount[i];
					float stepsToSpread = (trackStepCount / 2.0)-1;
					float fraction = 1.0/beatWarping[i];
					for(int j = 0; j < stepsCount[i]; j++) {	
						int actualBeat = (j + beatWarpingPosition[i]) % stepsCount[i]; 
						float fj = (float)j;					 
						if(j <= stepsToSpread)
							workingBeatWarpMatrix[i][actualBeat] = (2-fraction)*(stepsToSpread-fj)/stepsToSpread + (fraction*fj/stepsToSpread); 
						else							
							workingBeatWarpMatrix[i][actualBeat] = (2-fraction)*(fj-stepsToSpread-1.0)/stepsToSpread + (fraction*(trackStepCount-fj-1.0)/stepsToSpread); 						
					}
				}
			}											
		} else {
			if(QARExpanderDisconnectReset) { //If QRE gets disconnected, reset warping, probability and swing
				for(int i = 0; i < TRACK_COUNT; i++) {
					subBeatIndex[i] = 0;
					beatWarping[i] = 1.0;
					for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities
						workingProbabilityMatrix[i][j] = 1;
						workingSwingMatrix[i][j] = 0;
						workingBeatWarpMatrix[i][j] = 1.0;
						swingRandomness[i] = 0.0f;
					}
				}
				QARExpanderDisconnectReset = false;
			}
		}


		//set calculated probability and swing
		for(int i = 0; i < TRACK_COUNT; i++) {
			for(int j = 0; j < MAX_STEPS; j++) { 
				probabilityMatrix[i][j] = workingProbabilityMatrix[i][j];
				swingMatrix[i][j] = workingSwingMatrix[i][j];
				beatWarpMatrix[i][j] = workingBeatWarpMatrix[i][j];
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
					lastStepTime[trackNumber] = 200000; //Trying some arbitrary large value
				}
			}
		}

		//Calculate clock duration
		double timeAdvance =1.0 / args.sampleRate;
		timeElapsed += timeAdvance;

		float clockInput = inputs[CLOCK_INPUT].getVoltage();
		if(!inputs[CLOCK_INPUT].isConnected() && masterQARPresent) {
			clockInput = expanderClockValue;
		}

	

		if(inputs[CLOCK_INPUT].isConnected() || masterQARPresent) {
			if(clockTrigger.process(clockInput)) {
				if(firstClockReceived) {
					duration = timeElapsed;
				}
				timeElapsed = 0;
				firstClockReceived = true;							
			} else if(firstClockReceived && timeElapsed > duration) {  //allow absense of second clock to affect duration
				duration = timeElapsed;
			}			
			
			for(int trackNumber=0;trackNumber < TRACK_COUNT;trackNumber++) {
				if(stepsCount[trackNumber] > 0 && constantTime && beatIndex[trackNumber] >= 0 ) {
					double stepsChangeAdjustemnt = (double)(lastStepsCount[trackNumber] / (double)stepsCount[trackNumber]); 
					stepDuration[trackNumber] = duration * beatWarpMatrix[trackNumber][beatIndex[trackNumber]] * masterStepCount / (double)stepsCount[trackNumber] * stepsChangeAdjustemnt; //Constant Time scales duration based on a master track
				}
				else
					stepDuration[trackNumber] = duration * (beatIndex[trackNumber] >= 0 ? beatWarpMatrix[trackNumber][beatIndex[trackNumber]] : 1.0); //Otherwise Clock based
				

                //swing is affected by next beat
                int nextBeat = beatIndex[trackNumber] + 1;
                if(nextBeat >= stepsCount[trackNumber])
                    nextBeat = 0;
                double swingDuration = (calculatedSwingRandomness[trackNumber] + swingMatrix[trackNumber][nextBeat]) * stepDuration[trackNumber];
                
				if(running[trackNumber]) {
					lastStepTime[trackNumber] +=timeAdvance;
					if(stepDuration[trackNumber] > 0.0 && lastStepTime[trackNumber] >= stepDuration[trackNumber] + swingDuration - lastSwingDuration[trackNumber]) {
						lastSwingDuration[trackNumber] = swingDuration;
						lastStepsCount[trackNumber] = stepsCount[trackNumber];
						advanceBeat(trackNumber);					
					}					
				}	
			}			
		}

		// Set output to current state
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
			lights[WAITING_1_LIGHT+trackNumber].value = !running[trackNumber] ? 1.0 : 0.0;


            //Send Out Beat
			float beatOutputValue = beatPulse[trackNumber].process(1.0 / args.sampleRate) ? 10.0 : 0;
			if(slavedQARPresent)
				beatOutputValue =  clamp(beatOutputValue + expanderOutputValue[trackNumber],0.0f,10.0f);
            outputs[(trackNumber * 3) + OUTPUT_1].setVoltage(beatOutputValue);	

            //Send out Accent
			float accentOutputValue = accentPulse[trackNumber].process(1.0 / args.sampleRate) ? 10.0 : 0;
			if(slavedQARPresent)
				accentOutputValue = clamp(accentOutputValue + expanderAccentValue[trackNumber] ,0.0f,10.0f);
            outputs[(trackNumber * 3) + ACCENT_OUTPUT_1].setVoltage(accentOutputValue);	

			//Send out End of Cycle
			float eocOutputValue = eocPulse[trackNumber].process(1.0 / args.sampleRate) ? 10.0 : 0;
			outputs[(trackNumber * 3) + EOC_OUTPUT_1].setVoltage(eocOutputValue);				
			
			
			if(leftExpanderPresent) {
				float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
				messagesToMother[PASSTHROUGH_OFFSET + 1 + trackNumber * 3] = beatOutputValue; 
				messagesToMother[PASSTHROUGH_OFFSET + 1 + trackNumber * 3 + 1] = accentOutputValue;
				messagesToMother[PASSTHROUGH_OFFSET + 1 + trackNumber * 3 + 2] = rightExpanderPresent ? lastExpanderEocValue[trackNumber] : eocOutputValue; // If last QAR send Eoc Back, otherwise pass through
			} 
			if(rightExpanderPresent) {
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 4 + trackNumber] = eocOutputValue; 				
			}

		}

		//Send outputs to slaves if present		
		if(rightExpanderPresent) {
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT] = true; // tell slave Master is present
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 1] = clockInput; 
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 2] = resetInput; 
			messageToExpander[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 3] = muteInput; 	
			
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
		
        for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "algorithm");
			strcat(buf, trackNames[i]);
            json_object_set_new(rootJ, buf, json_integer((int) algorithnMatrix[i]));
        }
        
        json_object_set_new(rootJ, "constantTime", json_integer((bool) constantTime));
		json_object_set_new(rootJ, "masterTrack", json_integer((int) masterTrack));
		json_object_set_new(rootJ, "chainMode", json_integer((int) chainMode));
		json_object_set_new(rootJ, "muted", json_integer((bool) muted));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

        for(int i=0;i<TRACK_COUNT;i++) {
			char buf[100];
			strcpy(buf, "algorithm");
			strcat(buf, trackNames[i]);
            json_t *ctAl = json_object_get(rootJ, buf);
            if (ctAl)
                algorithnMatrix[i] = json_integer_value(ctAl);
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

	void advanceBeat(int trackNumber) {
       
		beatIndex[trackNumber]++;
		lastStepTime[trackNumber] = 0.0;
    
		//End of Cycle
		if(beatIndex[trackNumber] >= stepsCount[trackNumber]) {
			beatIndex[trackNumber] = 0;
			beatCountAtIndex[trackNumber] = -1;
			eocPulse[trackNumber].trigger(1e-3);
			probabilityGroupTriggered[trackNumber] = PENDING_PGTS;
			if(chainMode != CHAIN_MODE_NONE) {
				running[trackNumber] = false;
			}
		}

		if(!trackSwingUsingDivs[trackNumber]) {
			subBeatIndex[trackNumber]++;
			if(subBeatIndex[trackNumber] >= subBeatLength[trackNumber]) { 
				subBeatIndex[trackNumber] = 0;
			}
		} else if(trackSwingUsingDivs[trackNumber] && beatMatrix[trackNumber][beatIndex[trackNumber]] == true) {
			subBeatIndex[trackNumber]++;
			if(subBeatIndex[trackNumber] >= subBeatLength[trackNumber]) { 
				subBeatIndex[trackNumber] = 0;
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

        //Create Beat Trigger    
        if(beatMatrix[trackNumber][beatIndex[trackNumber]] == true && probabilityResult && running[trackNumber] && !muted) {
            beatPulse[trackNumber].trigger(1e-3);
			beatCountAtIndex[trackNumber]++;
        } 

        //Create Accent Trigger
        if(accentMatrix[trackNumber][beatIndex[trackNumber]] == true && probabilityResult && running[trackNumber] && !muted) {
            accentPulse[trackNumber].trigger(1e-3);
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
            algorithnMatrix[i] = EUCLIDEAN_ALGO;
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
			running[i] = true;
			for(int j = 0; j < MAX_STEPS; j++) {
				probabilityMatrix[i][j] = 1.0;
				swingMatrix[i][j] = 0.0;
				beatMatrix[i][j] = false;
				accentMatrix[i][j] = false;
				beatWarpMatrix[i][j] = 1.0;		
			}
		}	
	}
};


struct QARBeatDisplay : FramebufferWidget {
	QuadAlgorithmicRhythm *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	QARBeatDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}

	void drawBox(const DrawArgs &args, float stepNumber, float trackNumber,float runningTrackWidth, int algorithm, bool isBeat,bool isAccent,bool isCurrent, float beatWarp, float probability, int triggerState, int probabilityGroupMode, float swing, float swingRandomness) {
		
		//nvgSave(args.vg);
		//Rect b = Rect(Vec(0, 0), box.size);
		//nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		
		//float boxX = stepNumber * 22.5;
		float boxX = runningTrackWidth * 22.5;
		float boxY = trackNumber * 22.5;

		float baseWidth = 21.0 * beatWarp;

		float opacity = 0x80; // Testing using opacity for accents

		if(isAccent) {
			opacity = 0xff;
		}

		//TODO: Replace with switch statement
		//Default Euclidean Colors
		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor fillColor = nvgRGBA(0xef,0xe0,0,opacity);
        if(algorithm == 1) { //Golumb Ruler
            strokeColor = nvgRGBA(0, 0xe0, 0xef, 0xff);
			fillColor = nvgRGBA(0,0xe0,0xef,opacity);
        } else if(algorithm == 2) { //Logic
            strokeColor = nvgRGBA(0xe0, 0, 0xef, 0xff);
			fillColor = nvgRGBA(0xe0,0,0xef,opacity);        
		}
		if(isCurrent) {
			strokeColor = nvgRGBA(0x2f, 0xf0, 0, 0xff);
			fillColor = nvgRGBA(0x2f,0xf0,0,opacity);			
		}

		NVGcolor randomFillColor = nvgRGBA(0xff,0x00,0,0x40);			


		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.0);
		nvgRect(args.vg,boxX,boxY,baseWidth,21.0);		
		nvgStroke(args.vg);
		if(isBeat) {
			nvgBeginPath(args.vg);
			nvgStrokeWidth(args.vg, 0.0);
            nvgRect(args.vg,boxX + (baseWidth/2),boxY+(21*(1-probability)),(baseWidth/2)+(baseWidth*std::min(swing,0.0f)),21*probability);
            nvgRect(args.vg,boxX + (baseWidth*std::max(swing,0.0f)),boxY+(21*(1-probability)),(baseWidth/2)-(baseWidth*std::max(swing,0.0f)),21*probability);
            nvgFillColor(args.vg, fillColor);
			nvgFill(args.vg);
			nvgStroke(args.vg);

			//Draw swing randomness
			if(swingRandomness > 0.0f) {
				nvgBeginPath(args.vg);
				nvgStrokeWidth(args.vg, 0.0);
				nvgRect(args.vg,boxX + (baseWidth/2),boxY+(21*(1-probability)),((baseWidth/4)*swingRandomness),21*probability);
				nvgRect(args.vg,boxX + (baseWidth/2)-((baseWidth/4)*swingRandomness),boxY+(21*(1-probability)),((baseWidth/4)*swingRandomness),21*probability);
				nvgFillColor(args.vg, randomFillColor);
				nvgFill(args.vg);
				nvgStroke(args.vg);
			}

		}

		if (triggerState == module->NOT_TRIGGERED_PGTS && probabilityGroupMode != module->NONE_PGTM) {
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, nvgRGBA(0xff, 0x1f, 0, 0xaf));
			nvgStrokeWidth(args.vg, 1.0);
			nvgMoveTo(args.vg,boxX+1,boxY+1);
			nvgLineTo(args.vg,boxX+baseWidth-1,boxY+20);
			//nvgStroke(args.vg);
			nvgMoveTo(args.vg,boxX+baseWidth-1,boxY+1);
			nvgLineTo(args.vg,boxX+1,boxY+20);
			nvgStroke(args.vg);
			
		}		
	}

	void drawArc(const DrawArgs &args, float trackNumber, float stepsCount, float stepNumber, float runningTrackWidth, int algorithm, bool isBeat, bool isAccent, bool isCurrent, float beatWarp, float probability, int triggerState, int probabilityGroupMode, float swing,float nextSwing, float swingRandomness) 
	{		
        const float rotate90 = (M_PI) / 2.0;

        float opacity = 0x80; // Testing using opacity for accents

		if(isAccent) {
			opacity = 0xff;
		}

		//TODO: Replace with switch statement
		//Default Euclidean Colors
		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor fillColor = nvgRGBA(0xef,0xe0,0,opacity);
        if(algorithm == 1) { //Golumb Ruler
            strokeColor = nvgRGBA(0, 0xe0, 0xef, 0xff);
			fillColor = nvgRGBA(0,0xe0,0xef,opacity);
        } else if(algorithm == 2) { //Boolean Logic
            strokeColor = nvgRGBA(0xe0, 0, 0xef, 0xff);
			fillColor = nvgRGBA(0xe0,0,0xef,opacity);        
		}


		if(isCurrent) {
			strokeColor = nvgRGBA(0x2f, 0xf0, 0, 0xff);
			fillColor = nvgRGBA(0x2f,0xf0,0,opacity);			
		}

        
        NVGcolor randomFillColor = nvgRGBA(0xff,0x00,0,0x40);			
        nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));

        double baseStartDegree = (M_PI * 2.0 / stepsCount * (runningTrackWidth + swing)) - rotate90;
        double baseEndDegree = baseStartDegree + (M_PI * 2.0 / stepsCount * (beatWarp - swing + nextSwing));
		if(baseEndDegree < baseStartDegree) {
			baseEndDegree = baseStartDegree;
        }

		float baseRadius = 100.0 - (trackNumber*20);			 
        
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.0);
        nvgBeginPath(args.vg);
        nvgArc(args.vg,113,106,baseRadius+20.0,baseStartDegree,baseEndDegree,NVG_CW);
        double x= cos(baseEndDegree) * baseRadius + 113.0;
        double y= sin(baseEndDegree) * baseRadius + 106;
        nvgLineTo(args.vg,x,y);
        nvgArc(args.vg,113,106,baseRadius,baseEndDegree,baseStartDegree,NVG_CCW);
        nvgClosePath(args.vg);		
        nvgStroke(args.vg);

        float startDegree = baseStartDegree;
        float endDegree = baseEndDegree;
        if(isBeat) {
            nvgBeginPath(args.vg);
            nvgStrokeWidth(args.vg, 0.0);
            nvgArc(args.vg,113,106,baseRadius + (20.0 * probability) ,startDegree,endDegree,NVG_CW);
            double x= cos(endDegree) * baseRadius + 113.0;
            double y= sin(endDegree) * baseRadius + 106;
            nvgLineTo(args.vg,x,y);
            nvgArc(args.vg,113,106,baseRadius,endDegree,startDegree,NVG_CCW);
            nvgClosePath(args.vg);		
            nvgFillColor(args.vg, fillColor);
            nvgFill(args.vg);
            nvgStroke(args.vg);

            //Draw swing randomness
            if(swingRandomness > 0.0f) {
                float width = (baseEndDegree - baseStartDegree) * swingRandomness / 2.0; 
                nvgBeginPath(args.vg);
                nvgStrokeWidth(args.vg, 0.0);
                nvgArc(args.vg,113,106,baseRadius + (20.0 * probability),startDegree,startDegree+width,NVG_CW);
                double srx= cos(startDegree+width) * baseRadius + 113.0;
                double sry= sin(startDegree+width) * baseRadius + 106;
                nvgLineTo(args.vg,srx,sry);
                nvgArc(args.vg,113,106,baseRadius,startDegree+width,startDegree,NVG_CCW);
                nvgFillColor(args.vg, randomFillColor);
                nvgFill(args.vg);
                nvgStroke(args.vg);
            }

			if (triggerState == module->NOT_TRIGGERED_PGTS && probabilityGroupMode != module->NONE_PGTM) {
				nvgBeginPath(args.vg);
				nvgStrokeColor(args.vg, nvgRGBA(0xff, 0x1f, 0, 0xaf));
				nvgStrokeWidth(args.vg, 1.0);
				double sx= cos(startDegree) * (baseRadius+20) + 113.0;
            	double sy= sin(startDegree) * (baseRadius+20) + 106;
				nvgMoveTo(args.vg,sx,sy);
				nvgLineTo(args.vg,x,y);
				// //nvgStroke(args.vg);
				// nvgMoveTo(args.vg,boxX+baseWidth-1,boxY+1);
				// nvgLineTo(args.vg,boxX+1,boxY+20);
				nvgStroke(args.vg);		
			}
        }
	}

	void drawMasterTrack(const DrawArgs &args, Vec pos, int track) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", track);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		
		for(int trackNumber = 0;trackNumber < TRACK_COUNT;trackNumber++) {
            int algorithn = module->algorithnMatrix[trackNumber];
			float runningTrackWidth = 0.0;
			int stepsCount = module->stepsCount[trackNumber];
            for(int stepNumber = 0;stepNumber < stepsCount;stepNumber++) {	
				int nextStepNumber = (stepNumber + 1) % stepsCount;			
                bool isBeat = module->beatMatrix[trackNumber][stepNumber];
				bool isAccent = module->accentMatrix[trackNumber][stepNumber];
				bool isCurrent = module->beatIndex[trackNumber] == stepNumber && module->running[trackNumber];		
				float probability = module->probabilityMatrix[trackNumber][stepNumber];
				float swing = module->swingMatrix[trackNumber][stepNumber];	
				float nextSwing = module->swingMatrix[trackNumber][nextStepNumber];	
				float swingRandomness = module->swingRandomness[trackNumber];
				float beatWarp = module->beatWarpMatrix[trackNumber][stepNumber];
				int triggerState = module->probabilityGroupTriggered[trackNumber];
				int probabilityGroupMode = module->probabilityGroupModeMatrix[trackNumber][stepNumber];
				drawArc(args, float(trackNumber), float(stepsCount), float(stepNumber),runningTrackWidth,algorithn,isBeat,isAccent,isCurrent,beatWarp,probability,triggerState,probabilityGroupMode,swing,nextSwing,swingRandomness);
				runningTrackWidth += beatWarp;
			}
		}	

		if(module->constantTime)
			drawMasterTrack(args, Vec(122, 271), module->masterTrack);
	}
};


struct QuadAlgorithmicRhythmWidget : ModuleWidget {
	QuadAlgorithmicRhythmWidget(QuadAlgorithmicRhythm *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadAlgorithmicRhythm.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		{
			QARBeatDisplay *display = new QARBeatDisplay();
			display->module = module;
			display->box.pos = Vec(16, 34);
			display->box.size = Vec(box.size.x-31, 351);
			addChild(display);
		}

		for(int track=0;track<TRACK_COUNT;track++) {
			addParam(createParam<LEDButton>(Vec(290+(track*62), 42), module, QuadAlgorithmicRhythm::ALGORITHM_1_PARAM+(track*7)));
			addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(291.5+(track*62), 43.5), module, QuadAlgorithmicRhythm::ALGORITHM_1_LIGHT+(track*3)));
			addParam(createParam<RoundSmallFWSnapKnob>(Vec(286+(track*62), 68), module, QuadAlgorithmicRhythm::STEPS_1_PARAM+(track*7)));
			addParam(createParam<RoundSmallFWSnapKnob>(Vec(286+(track*62), 98), module, QuadAlgorithmicRhythm::DIVISIONS_1_PARAM+(track*7)));
			addParam(createParam<RoundSmallFWSnapKnob>(Vec(286+(track*62), 128), module, QuadAlgorithmicRhythm::OFFSET_1_PARAM+(track*7)));
			addParam(createParam<RoundSmallFWSnapKnob>(Vec(286+(track*62), 158), module, QuadAlgorithmicRhythm::PAD_1_PARAM+(track*7)));
			addParam(createParam<RoundSmallFWSnapKnob>(Vec(286+(track*62), 188), module, QuadAlgorithmicRhythm::ACCENTS_1_PARAM+(track*7)));
			addParam(createParam<RoundSmallFWSnapKnob>(Vec(286+(track*62), 218), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_PARAM+(track*7)));

			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 43), module, QuadAlgorithmicRhythm::ALGORITHM_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 72), module, QuadAlgorithmicRhythm::STEPS_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 102), module, QuadAlgorithmicRhythm::DIVISIONS_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 132), module, QuadAlgorithmicRhythm::OFFSET_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 162), module, QuadAlgorithmicRhythm::PAD_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 192), module, QuadAlgorithmicRhythm::ACCENTS_1_INPUT+(track*8)));
			addInput(createInput<FWPortInSmall>(Vec(316+(track*62), 222), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_INPUT+(track*8)));

			addInput(createInput<FWPortInSmall>(Vec(290+(track*62), 256), module, QuadAlgorithmicRhythm::START_1_INPUT+(track*8)));
			addOutput(createOutput<FWPortOutSmall>(Vec(290+(track*62), 282), module, QuadAlgorithmicRhythm::EOC_OUTPUT_1+(track*3)));
			addOutput(createOutput<FWPortOutSmall>(Vec(290+(track*62), 308), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_1+(track*3)));
			addOutput(createOutput<FWPortOutSmall>(Vec(290+(track*62), 334), module, QuadAlgorithmicRhythm::OUTPUT_1+(track*3)));

			addChild(createLight<SmallLight<RedLight>>(Vec(312+(track*62), 344), module, QuadAlgorithmicRhythm::WAITING_1_LIGHT+track));

		}

		addParam(createParam<CKD6>(Vec(10, 283), module, QuadAlgorithmicRhythm::CHAIN_MODE_PARAM));
		addParam(createParam<CKD6>(Vec(92, 283), module, QuadAlgorithmicRhythm::CONSTANT_TIME_MODE_PARAM));

		addParam(createParam<TL1105>(Vec(82, 347), module, QuadAlgorithmicRhythm::RESET_PARAM));
		addParam(createParam<LEDButton>(Vec(135, 346), module, QuadAlgorithmicRhythm::MUTE_PARAM));
		

		addInput(createInput<PJ301MPort>(Vec(14, 343), module, QuadAlgorithmicRhythm::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 343), module, QuadAlgorithmicRhythm::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(107, 343), module, QuadAlgorithmicRhythm::MUTE_INPUT));


		addChild(createLight<SmallLight<BlueLight>>(Vec(44, 286), module, QuadAlgorithmicRhythm::CHAIN_MODE_NONE_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(Vec(44, 301), module, QuadAlgorithmicRhythm::CHAIN_MODE_BOSS_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(44, 316), module, QuadAlgorithmicRhythm::CHAIN_MODE_EMPLOYEE_LIGHT));


		// addChild(createLight<LargeLight<GreenLight>>(Vec(50, 362), module, QuadAlgorithmicRhythm::IS_MASTER_LIGHT));
		// addChild(createLight<LargeLight<RedLight>>(Vec(80, 362), module, QuadAlgorithmicRhythm::IS_SLAVE_LIGHT));

		addChild(createLight<LargeLight<RedLight>>(Vec(136, 347), module, QuadAlgorithmicRhythm::MUTED_LIGHT));

	}
};

Model *modelQuadAlgorithmicRhythm = createModel<QuadAlgorithmicRhythm, QuadAlgorithmicRhythmWidget>("QuadAlgorithmicRhythm");
