#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "dsp-noise/noise.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_ALGORITHMS 3
#define EXPANDER_MAX_STEPS 18
#define NUM_RULERS 10
#define MAX_DIVISIONS 6
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 6
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * 3 + TRACK_LEVEL_PARAM_COUNT

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
		FIRES_IF_FIRST_TRIGGERED_PGTM,
		MAY_FIRE_IF_FIRST_TRIGGERED_PGTM
	};
	enum ProbabilityGroupTriggeredStatus {
		PENDING_PGTS,
		TRIGGERED_PGTS,
		NOT_TRIGGERED_PGTS
	};

	// Expander
	float consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here
	
    int algorithnMatrix[TRACK_COUNT];
	bool beatMatrix[TRACK_COUNT][MAX_STEPS];
	bool accentMatrix[TRACK_COUNT][MAX_STEPS];

	float probabilityMatrix[TRACK_COUNT][MAX_STEPS];
	float swingMatrix[TRACK_COUNT][MAX_STEPS];
	float probabilityGroupModeMatrix[TRACK_COUNT][MAX_STEPS];
	int probabilityGroupTriggered[TRACK_COUNT];
	int probabilityGroupFirstStep[TRACK_COUNT];
	float workingProbabilityMatrix[TRACK_COUNT][MAX_STEPS];
	float workingSwingMatrix[TRACK_COUNT][MAX_STEPS];

	int beatIndex[TRACK_COUNT];
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

    const int rulerOrders[NUM_RULERS] = {1,2,3,4,5,5,6,6,6,6};
	const int rulerLengths[NUM_RULERS] = {0,1,3,6,11,11,17,17,17,17};
	const int rulers[NUM_RULERS][MAX_DIVISIONS] = {{0},
												   {0,1},
												   {0,1,3},
												   {0,1,4,6},
												   {0,1,4,9,11},
												   {0,2,7,8,11},
												   {0,1,4,10,12,17},
												   {0,1,4,10,15,17},
												   {0,1,8,11,13,17},
												   {0,1,8,12,14,17}};

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

	
		configParam(STEPS_1_PARAM, 0.0, 18, 16.0,"Track 1 Steps");
		configParam(DIVISIONS_1_PARAM, 1.0, 18, 2.0,"Track 1 Divisions");
		configParam(OFFSET_1_PARAM, 0.0, 17, 0.0,"Track 1 Step Offset");
		configParam(PAD_1_PARAM, 0.0, 17, 0.0,"Track 1 Step Padding");
		configParam(ACCENTS_1_PARAM, 0.0, 18, 0.0, "Track 1 Accents");
		configParam(ACCENT_ROTATE_1_PARAM, 0.0, 17, 0.0,"Track 1 Acccent Rotation");
        configParam(ALGORITHM_1_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_2_PARAM, 0.0, 18.0, 16.0,"Track 2 Steps");
		configParam(DIVISIONS_2_PARAM, 1.0, 18, 2.0,"Track 2 Divisions");
		configParam(OFFSET_2_PARAM, 0.0, 17, 0.0,"Track 2 Step Offset");
		configParam(PAD_2_PARAM, 0.0, 17, 0.0,"Track 2 Step Padding");
		configParam(ACCENTS_2_PARAM, 0.0, 18, 0.0, "Track 2 Accents");
		configParam(ACCENT_ROTATE_2_PARAM, 0.0, 17, 0.0,"Track 2 Acccent Rotation");
        configParam(ALGORITHM_2_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_3_PARAM, 0.0, 18, 16.0,"Track 3 Steps");
		configParam(DIVISIONS_3_PARAM, 1.0, 18, 2.0,"Track 3 Divisions");
		configParam(OFFSET_3_PARAM, 0.0, 17, 0.0,"Track 3 Step Offset");
		configParam(PAD_3_PARAM, 0.0, 17, 0.0,"Track 3 Step Padding");
		configParam(ACCENTS_3_PARAM, 0.0, 18, 0.0, "Track 3 Accents");
		configParam(ACCENT_ROTATE_3_PARAM, 0.0, 17, 0.0,"Track 3 Acccent Rotation");
        configParam(ALGORITHM_3_PARAM, 0.0, 1.0, 0.0);

		configParam(STEPS_4_PARAM, 0.0, 18, 16.0,"Track 4 Steps");
		configParam(DIVISIONS_4_PARAM, 1.0, 18, 2.0,"Track 4 Divisions");
		configParam(OFFSET_4_PARAM, 0.0, 17, 0.0,"Track 4 Step Offset");
		configParam(PAD_4_PARAM, 0.0, 17, 0.0,"Track 4 Step Padding");
		configParam(ACCENTS_4_PARAM, 0.0, 18, 0.0, "Track 4 Accents");
		configParam(ACCENT_ROTATE_4_PARAM, 0.0, 17, 0.0,"Track 4 Acccent Rotation");
        configParam(ALGORITHM_4_PARAM, 0.0, 1.0, 0.0);

		configParam(CHAIN_MODE_PARAM, 0.0, 1.0, 0.0);
		configParam(CONSTANT_TIME_MODE_PARAM, 0.0, 1.0, 0.0);

		configParam(RESET_PARAM, 0.0, 1.0, 0.0);
		configParam(MUTE_PARAM, 0.0, 1.0, 0.0);

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
		
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




			expanderOutputValue[i] = 0.0;
			expanderAccentValue[i] = 0.0;
			expanderEocValue[i] = 0.0;
			lastExpanderEocValue[i] = 0.0;

			running[i] = true;
			for(int j = 0; j < MAX_STEPS; j++) {
				probabilityMatrix[i][j] = 1.0;
				swingMatrix[i][j] = 0.0;
				beatMatrix[i][j] = false;
				accentMatrix[i][j] = false;				
			}
		}	

        onReset();	
	}

	void process(const ProcessArgs &args) override  {

		int beatLocation[MAX_STEPS];

		//Initialize
		for(int i = 0; i < TRACK_COUNT; i++) {
			expanderOutputValue[i] = 0; 
			expanderAccentValue[i] = 0; 
			lastExpanderEocValue[i] = 0;
		}
		//See if a slave is passing through an expander
		bool slavedQARPresent = false;
		bool rightExpanderPresent = (rightExpander.module 
		&& (rightExpander.module->model == modelQuadAlgorithmicRhythm || rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander));
		if(rightExpanderPresent)
		{			
			float *message = (float*) rightExpander.module->leftExpander.consumerMessage;
			slavedQARPresent = message[PASSTHROUGH_OFFSET];  // Slave QAR Exists flag

			if(slavedQARPresent) {			
				for(int i = 0; i < TRACK_COUNT; i++) {
					expanderOutputValue[i] = message[PASSTHROUGH_OFFSET + 1 + i * 3] ; 
					expanderAccentValue[i] = message[PASSTHROUGH_OFFSET + 1 + i * 3 + 1] ; 
					lastExpanderEocValue[i] = message[PASSTHROUGH_OFFSET + 1 + i * 3 + 2] ; 					
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
		 || leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander));
		if(leftExpanderPresent)
		{			
			float *consumerMessage = (float*)leftExpander.consumerMessage;
			masterQARPresent = consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT]; 

			if(masterQARPresent) {
			
				expanderClockValue = consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 1] ; 
				expanderResetValue = consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 2] ; 
				expanderMuteValue = consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 3] ; 

				for(int i = 0; i < TRACK_COUNT; i++) {				
					expanderEocValue[i] = consumerMessage[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 4 + i] ; 
				}
			}
		}

		//if(slavedQARPresent)
		//lights[IS_MASTER_LIGHT].value = slavedQARPresent;

		//if(masterQARPresent)
		//lights[IS_SLAVE_LIGHT].value = masterQARPresent;



		

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
				beatLocation[j] = 0;
			}

			float stepsCountf = std::floor(params[(trackNumber * 7) + STEPS_1_PARAM].getValue());			
			if(inputs[trackNumber * 8].isConnected()) {
				stepsCountf += inputs[trackNumber * 8 + STEPS_1_INPUT].getVoltage() * 1.8;
			}
			stepsCountf = clamp(stepsCountf,0.0f,18.0f);
			if(algorithnMatrix[trackNumber] == BOOLEAN_LOGIC_ALGO) { // Boolean Tracks can't exceed length of the tracks they are based (-1 and -2)
				stepsCountf = std::min(stepsCountf,(float)std::min(stepsCount[trackNumber-1],stepsCount[trackNumber-2]));
			}

			float divisionf = std::floor(params[(trackNumber * 7) + DIVISIONS_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].isConnected()) {
				divisionf += inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].getVoltage() * 1.7;
			}		
			divisionf = clamp(divisionf,1.0f,stepsCountf);

			float offsetf = std::floor(params[(trackNumber * 7) + OFFSET_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + OFFSET_1_INPUT].isConnected()) {
				offsetf += inputs[(trackNumber * 8) + OFFSET_1_INPUT].getVoltage() * 1.7;
			}	
			offsetf = clamp(offsetf,0.0f,17.0f);

			float padf = std::floor(params[trackNumber * 7 + PAD_1_PARAM].getValue());
			if(inputs[(trackNumber * 8) + PAD_1_INPUT].isConnected()) {
				padf += inputs[trackNumber * 8 + PAD_1_INPUT].getVoltage() * 1.7;
			}
			padf = clamp(padf,0.0f,stepsCountf -1);
			// Reclamp
			divisionf = clamp(divisionf,1.0f,stepsCountf-padf);



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


			if(stepsCount[trackNumber] > 0) {
				
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
                            beatLocation[euclideanBeatIndex] = (euclideanStepIndex + offset + pad) % stepsCount[trackNumber];	
                            euclideanBeatIndex++;	
                        } else
                        {
                            beatMatrix[trackNumber][((euclideanStepIndex + offset + pad) % (stepsCount[trackNumber]))] = false;	
                        }
                        
                    }
                } else if(algorithnMatrix[trackNumber] == GOLUMB_RULER_ALGO) { //Golomb Ruler Algorithm
				
                    int rulerToUse = clamp(division - 1,0,MAX_DIVISIONS);
                    int actualStepCount = stepsCount[trackNumber] - pad;
                    while(rulerLengths[rulerToUse] + 1 > actualStepCount && rulerToUse >= 	0) {
                        rulerToUse -=1;
                    } 
                    
                    //Multiply beats so that low division beats fill out entire pattern
                    int spaceMultiplier = (actualStepCount / (rulerLengths[rulerToUse] + 1)) + 1;
                    if(actualStepCount % (rulerLengths[rulerToUse] + 1) == 0) {
                        spaceMultiplier -=1;
                    }	

                    //Set all beats to false
                    for(int j=0;j<actualStepCount;j++)
                    {
                        beatMatrix[trackNumber][j] = false; 			
                    }

                    for (int rulerIndex = 0; rulerIndex < rulerOrders[rulerToUse];rulerIndex++)
                    {
                        int divisionLocation = rulers[rulerToUse][rulerIndex] * spaceMultiplier;
                        divisionLocation +=pad;
                        if(rulerIndex > 0) {
                            divisionLocation -=1;
                        }
                        beatMatrix[trackNumber][(divisionLocation + offset) % stepsCount[trackNumber]] = true;
                        beatLocation[rulerIndex] = (divisionLocation + offset) % stepsCount[trackNumber];	            
                    }
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
                        	beatLocation[(logicBeatIndex + offset) % stepsCount[trackNumber]] = logicBeatIndex;	
							logicBeatCount ++;
						}
					}

				}

				bucket = division - 1;
				for(int accentIndex = 0; accentIndex < division; accentIndex++)
				{
					bucket += accentDivision;
					if(bucket >= division) {
						bucket -= division;
						accentMatrix[trackNumber][beatLocation[(accentIndex + accentRotation) % division]] = true;				
					} else
					{
						accentMatrix[trackNumber][beatLocation[(accentIndex + accentRotation) % division]] = false;
					}
					
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
			}
			timeElapsed = 0;
			firstClockReceived = false;
			setRunningState();
		}
		

		

		//Get Expander Info
		if(rightExpander.module && (rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander))
		{			
			QARExpanderDisconnectReset = true;
			float *message = (float*) rightExpander.module->leftExpander.consumerMessage;

			//Process Probability Expander Stuff						
			for(int i = 0; i < TRACK_COUNT; i++) {
				probabilityGroupFirstStep[i] = -1;
				for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities, find first group step
					workingProbabilityMatrix[i][j] = 1;					
				}

				if(message[i] > 0) { // 0 is track not selected
					bool useDivs = message[i] == 2; //2 is divs
					for(int j = 0; j < MAX_STEPS; j++) { // Assign probabilites and swing
						int stepIndex = j;
						bool stepFound = true;
						if(useDivs) { //Use j as a count to the div # we are looking for
							int divIndex = -1;
							stepFound = false;
							for(int k = 0; k< MAX_STEPS; k++) {
								if (beatMatrix[i][k]) {
									divIndex ++;
									if(divIndex == j) {
										stepIndex = k;
										stepFound = true;	
										break;								
									}
								}
							}
						}
						
						if(stepFound) {
							float probability = message[TRACK_LEVEL_PARAM_COUNT + (i * EXPANDER_MAX_STEPS) + j];
							float probabilityMode = message[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT) + (i * EXPANDER_MAX_STEPS) + j];
							
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
				for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities
					workingSwingMatrix[i][j] = 0.0;
				}

				if(message[TRACK_COUNT + i] > 0) { // 0 is track not selected
					bool useDivs = message[TRACK_COUNT + i] == 2; //2 is divs
					trackSwingUsingDivs[i] = useDivs;

					int grooveLength = (int)(message[TRACK_COUNT * 2 + i]);
					bool useTrackLength = message[TRACK_COUNT * 3 + i];

					swingRandomness[i] = message[TRACK_COUNT * 4 + i];
					useGaussianDistribution[i] = message[TRACK_COUNT * 5 + i];

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
						int divCount = -1;
						for(int k = 0; k<= beatIndex[i]; k++) {
							if (beatMatrix[i][k]) {
								divCount++;
							}
						}

						workingBeatIndex = (subBeatIndex[i] - divCount) % grooveLength; 
						if(workingBeatIndex <0) {
							workingBeatIndex +=grooveLength;
						}
					}

					for(int j = 0; j < MAX_STEPS; j++) { // Assign probabilites and swing
						int stepIndex = j;
						bool stepFound = true;
						if(useDivs) { //Use j as a count to the div # we are looking for
							int divIndex = -1;
							stepFound = false;
							for(int k = 0; k< MAX_STEPS; k++) {
								if (beatMatrix[i][k]) {
									divIndex ++;
									if(divIndex == j) {
										stepIndex = k;
										stepFound = true;	
										break;								
									}
								}
							}
						}
						
						if(stepFound) {
							float swing = message[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 2) + (i * EXPANDER_MAX_STEPS) + workingBeatIndex];
							workingSwingMatrix[i][stepIndex] = swing;						
						} 
						workingBeatIndex +=1;
						if(workingBeatIndex >= grooveLength) {
							workingBeatIndex = 0;
						}
					}
				}
			}
			
			
		} else {
			if(QARExpanderDisconnectReset) { //If QRE gets disconnected, reset probability and swing
				for(int i = 0; i < TRACK_COUNT; i++) {
					subBeatIndex[i] = 0;
					for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities
						workingProbabilityMatrix[i][j] = 1;
						workingSwingMatrix[i][j] = 0;
					}
				}
				QARExpanderDisconnectReset = false;
			}
		}


		//set calculated probability and swing
		for(int i = 0; i < TRACK_COUNT; i++) {
			for(int j = 0; j < MAX_STEPS; j++) { 
				probabilityMatrix[i][j] = workingProbabilityMatrix[i][j];
				swingMatrix[i][j] =workingSwingMatrix[i][j];
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
					stepDuration[trackNumber] = duration * masterStepCount / (double)stepsCount[trackNumber] * stepsChangeAdjustemnt; //Constant Time scales duration based on a master track
				}
				else
					stepDuration[trackNumber] = duration; //Otherwise Clock based
				

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
				producerMessage[PASSTHROUGH_OFFSET + 1 + trackNumber * 3] = beatOutputValue; 
				producerMessage[PASSTHROUGH_OFFSET + 1 + trackNumber * 3 + 1] = accentOutputValue;
				producerMessage[PASSTHROUGH_OFFSET + 1 + trackNumber * 3 + 2] = rightExpanderPresent ? lastExpanderEocValue[trackNumber] : eocOutputValue; // If last QAR send Eoc Back, otherwise pass through
			} 
			if(rightExpanderPresent) {
				float *messageToSlave = (float*)(rightExpander.module->leftExpander.producerMessage);	
				messageToSlave[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 4 + trackNumber] = eocOutputValue; 				
			}

		}

		//Send outputs to slaves if present		
		if(rightExpanderPresent) {
			float *messageToSlave = (float*)(rightExpander.module->leftExpander.producerMessage);
			messageToSlave[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT] = true; // tell slave Master is present
			messageToSlave[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 1] = clockInput; 
			messageToSlave[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 2] = resetInput; 
			messageToSlave[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + 3] = muteInput; 				
		}
		
		if(leftExpanderPresent) {
			producerMessage[PASSTHROUGH_OFFSET] = true; //Tell Master that slave is present
			leftExpander.messageFlipRequested = true;	
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
		//bool groupTriggered = false;
		if(probabilityGroupModeMatrix[trackNumber][beatIndex[trackNumber]] != NONE_PGTM) {
			if(probabilityGroupFirstStep[trackNumber] == beatIndex[trackNumber]) {
				probabilityGroupTriggered[trackNumber] = probabilityResult ? TRIGGERED_PGTS : NOT_TRIGGERED_PGTS;
			} else if(probabilityGroupTriggered[trackNumber] == TRIGGERED_PGTS && probabilityGroupModeMatrix[trackNumber][beatIndex[trackNumber]] == FIRES_IF_FIRST_TRIGGERED_PGTM) {
				probabilityResult = true;
			} else if(probabilityGroupTriggered[trackNumber] == NOT_TRIGGERED_PGTS) {
				probabilityResult = false;
			}
		}

        //Create Beat Trigger    
        if(beatMatrix[trackNumber][beatIndex[trackNumber]] == true && probabilityResult && running[trackNumber] && !muted) {
            beatPulse[trackNumber].trigger(1e-3);		
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
			calculatedSwingRandomness[trackNumber] = 1.0 - gaussian / 2 * swingRandomness[trackNumber];
		} else {
			calculatedSwingRandomness[trackNumber] = 1.0 - (((double) rand()/RAND_MAX - 0.5f) * swingRandomness[trackNumber]);
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
			}
		}	
	}
};


struct QARBeatDisplay : TransparentWidget {
	QuadAlgorithmicRhythm *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	QARBeatDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}

	void drawBox(const DrawArgs &args, float stepNumber, float trackNumber,int algorithm, bool isBeat,bool isAccent,bool isCurrent, float probability, int triggerState, int probabilityGroupMode, float swing, float swingRandomness) {
		
		//nvgSave(args.vg);
		//Rect b = Rect(Vec(0, 0), box.size);
		//nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		
		float boxX = stepNumber * 22.5;
		float boxY = trackNumber * 22.5;

		float opacity = 0x80; // Testing using opacity for accents

		if(isAccent) {
			opacity = 0xff;
		}

		if(triggerState == module->TRIGGERED_PGTS && probabilityGroupMode == module->FIRES_IF_FIRST_TRIGGERED_PGTM) {
			probability = 1.0f;
		} else if (triggerState == module->NOT_TRIGGERED_PGTS && probabilityGroupMode != module->NONE_PGTM) {
			probability = 0.0f;
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
		nvgRect(args.vg,boxX,boxY,21,21.0);		
		nvgStroke(args.vg);
		if(isBeat) {
			nvgBeginPath(args.vg);
			nvgStrokeWidth(args.vg, 0.0);
            nvgRect(args.vg,boxX + 10.5,boxY+(21*(1-probability)),10.5+(21*std::min(swing,0.0f)),21*probability);
            nvgRect(args.vg,boxX + (21*std::max(swing,0.0f)),boxY+(21*(1-probability)),10.5-(21*std::max(swing,0.0f)),21*probability);
            nvgFillColor(args.vg, fillColor);
			nvgFill(args.vg);
			nvgStroke(args.vg);

			//Draw swing randomness
			if(swingRandomness > 0.0f) {
				nvgBeginPath(args.vg);
				nvgStrokeWidth(args.vg, 0.0);
				nvgRect(args.vg,boxX + 10.5f,boxY+(21*(1-probability)),(5.5*swingRandomness),21*probability);
				nvgRect(args.vg,boxX + 10.5f-(5.5*swingRandomness),boxY+(21*(1-probability)),(5.5*swingRandomness),21*probability);
				nvgFillColor(args.vg, randomFillColor);
				nvgFill(args.vg);
				nvgStroke(args.vg);
			}

		}



		//nvgStroke(args.vg);
		
	}

	void drawMasterTrack(const DrawArgs &args, Vec pos, int track) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", track);
		nvgText(args.vg, pos.x + 8, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		
		for(int trackNumber = 0;trackNumber < TRACK_COUNT;trackNumber++) {
            int algorithn = module->algorithnMatrix[trackNumber];
            for(int stepNumber = 0;stepNumber < module->stepsCount[trackNumber];stepNumber++) {				
                bool isBeat = module->beatMatrix[trackNumber][stepNumber];
				bool isAccent = module->accentMatrix[trackNumber][stepNumber];
				bool isCurrent = module->beatIndex[trackNumber] == stepNumber && module->running[trackNumber];		
				float probability = module->probabilityMatrix[trackNumber][stepNumber];
				float swing = module->swingMatrix[trackNumber][stepNumber];				
				float swingRandomness = module->swingRandomness[trackNumber];
				int triggerState = module->probabilityGroupTriggered[trackNumber];
				int probabilityGroupMode = module->probabilityGroupModeMatrix[trackNumber][stepNumber];
				drawBox(args, float(stepNumber), float(trackNumber),algorithn,isBeat,isAccent,isCurrent,probability,triggerState,probabilityGroupMode,swing,swingRandomness);
			}
		}

		if(module->constantTime)
			drawMasterTrack(args, Vec(box.size.x - 21, box.size.y - 80), module->masterTrack);
			//drawMasterTrack(args, Vec(box.size.x - 21, box.size.y - 80), module->probabilityGroupFirstStep[1]);
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


		addParam(createParam<LEDButton>(Vec(26, 140), module, QuadAlgorithmicRhythm::ALGORITHM_1_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(61, 138), module, QuadAlgorithmicRhythm::STEPS_1_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(100, 138), module, QuadAlgorithmicRhythm::DIVISIONS_1_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(139, 138), module, QuadAlgorithmicRhythm::OFFSET_1_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(178, 138), module, QuadAlgorithmicRhythm::PAD_1_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(217, 138), module, QuadAlgorithmicRhythm::ACCENTS_1_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(256, 138), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_PARAM));

		addParam(createParam<LEDButton>(Vec(26, 197), module, QuadAlgorithmicRhythm::ALGORITHM_2_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(61, 195), module, QuadAlgorithmicRhythm::STEPS_2_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(100, 195), module, QuadAlgorithmicRhythm::DIVISIONS_2_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(139, 195), module, QuadAlgorithmicRhythm::OFFSET_2_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(178, 195), module, QuadAlgorithmicRhythm::PAD_2_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(217, 195), module, QuadAlgorithmicRhythm::ACCENTS_2_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(256, 195), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_2_PARAM));

		addParam(createParam<LEDButton>(Vec(26, 254), module, QuadAlgorithmicRhythm::ALGORITHM_3_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(61, 252), module, QuadAlgorithmicRhythm::STEPS_3_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(100, 252), module, QuadAlgorithmicRhythm::DIVISIONS_3_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(139, 252), module, QuadAlgorithmicRhythm::OFFSET_3_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(178, 252), module, QuadAlgorithmicRhythm::PAD_3_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(217, 252), module, QuadAlgorithmicRhythm::ACCENTS_3_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(256, 252), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_3_PARAM));

		addParam(createParam<LEDButton>(Vec(26, 311), module, QuadAlgorithmicRhythm::ALGORITHM_4_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(61, 309), module, QuadAlgorithmicRhythm::STEPS_4_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(100, 309), module, QuadAlgorithmicRhythm::DIVISIONS_4_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(139, 309), module, QuadAlgorithmicRhythm::OFFSET_4_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(178, 309), module, QuadAlgorithmicRhythm::PAD_4_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(217, 309), module, QuadAlgorithmicRhythm::ACCENTS_4_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(256, 309), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_4_PARAM));

		addParam(createParam<CKD6>(Vec(299, 283), module, QuadAlgorithmicRhythm::CHAIN_MODE_PARAM));
		addParam(createParam<CKD6>(Vec(381, 283), module, QuadAlgorithmicRhythm::CONSTANT_TIME_MODE_PARAM));

		addParam(createParam<TL1105>(Vec(364, 347), module, QuadAlgorithmicRhythm::RESET_PARAM));
		addParam(createParam<LEDButton>(Vec(414, 346), module, QuadAlgorithmicRhythm::MUTE_PARAM));
		

        addInput(createInput<PJ301MPort>(Vec(23, 164), module, QuadAlgorithmicRhythm::ALGORITHM_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(61, 164), module, QuadAlgorithmicRhythm::STEPS_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(100, 164), module, QuadAlgorithmicRhythm::DIVISIONS_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(139, 164), module, QuadAlgorithmicRhythm::OFFSET_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(178, 164), module, QuadAlgorithmicRhythm::PAD_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(217, 164), module, QuadAlgorithmicRhythm::ACCENTS_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(256, 164), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_INPUT));

        addInput(createInput<PJ301MPort>(Vec(23, 221), module, QuadAlgorithmicRhythm::ALGORITHM_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(61, 221), module, QuadAlgorithmicRhythm::STEPS_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(100, 221), module, QuadAlgorithmicRhythm::DIVISIONS_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(139, 221), module, QuadAlgorithmicRhythm::OFFSET_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(178, 221), module, QuadAlgorithmicRhythm::PAD_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(217, 221), module, QuadAlgorithmicRhythm::ACCENTS_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(256, 221), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_2_INPUT));

        addInput(createInput<PJ301MPort>(Vec(23, 278), module, QuadAlgorithmicRhythm::ALGORITHM_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(61, 278), module, QuadAlgorithmicRhythm::STEPS_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(100, 278), module, QuadAlgorithmicRhythm::DIVISIONS_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(139, 278), module, QuadAlgorithmicRhythm::OFFSET_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(178, 278), module, QuadAlgorithmicRhythm::PAD_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(217, 278), module, QuadAlgorithmicRhythm::ACCENTS_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(256, 278), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_3_INPUT));

        addInput(createInput<PJ301MPort>(Vec(23, 335), module, QuadAlgorithmicRhythm::ALGORITHM_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(61, 335), module, QuadAlgorithmicRhythm::STEPS_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(100, 335), module, QuadAlgorithmicRhythm::DIVISIONS_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(139, 335), module, QuadAlgorithmicRhythm::OFFSET_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(178, 335), module, QuadAlgorithmicRhythm::PAD_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(217, 335), module, QuadAlgorithmicRhythm::ACCENTS_4_INPUT));
		addInput(createInput<PJ301MPort>(Vec(256, 335), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_4_INPUT));

		addInput(createInput<PJ301MPort>(Vec(298, 343), module, QuadAlgorithmicRhythm::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(338, 343), module, QuadAlgorithmicRhythm::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(386, 343), module, QuadAlgorithmicRhythm::MUTE_INPUT));

		addInput(createInput<PJ301MPort>(Vec(365, 148), module, QuadAlgorithmicRhythm::START_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(365, 178), module, QuadAlgorithmicRhythm::START_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(365, 208), module, QuadAlgorithmicRhythm::START_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(365, 238), module, QuadAlgorithmicRhythm::START_4_INPUT));


		addOutput(createOutput<PJ301MPort>(Vec(301, 148), module, QuadAlgorithmicRhythm::OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(333, 148), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(397, 148), module, QuadAlgorithmicRhythm::EOC_OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(301, 178), module, QuadAlgorithmicRhythm::OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(333, 178), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(397, 178), module, QuadAlgorithmicRhythm::EOC_OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(301, 208), module, QuadAlgorithmicRhythm::OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(333, 208), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(397, 208), module, QuadAlgorithmicRhythm::EOC_OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(301, 238), module, QuadAlgorithmicRhythm::OUTPUT_4));
		addOutput(createOutput<PJ301MPort>(Vec(333, 238), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_4));
		addOutput(createOutput<PJ301MPort>(Vec(397, 238), module, QuadAlgorithmicRhythm::EOC_OUTPUT_4));

        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(27.5, 141.5), module, QuadAlgorithmicRhythm::ALGORITHM_1_LIGHT));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(27.5, 198.5), module, QuadAlgorithmicRhythm::ALGORITHM_2_LIGHT));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(27.5, 255.5), module, QuadAlgorithmicRhythm::ALGORITHM_3_LIGHT));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(27.5, 312.5), module, QuadAlgorithmicRhythm::ALGORITHM_4_LIGHT));
		
		addChild(createLight<SmallLight<BlueLight>>(Vec(332, 285), module, QuadAlgorithmicRhythm::CHAIN_MODE_NONE_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(Vec(332, 300), module, QuadAlgorithmicRhythm::CHAIN_MODE_BOSS_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(332, 315), module, QuadAlgorithmicRhythm::CHAIN_MODE_EMPLOYEE_LIGHT));

		// addChild(createLight<LargeLight<GreenLight>>(Vec(50, 362), module, QuadAlgorithmicRhythm::IS_MASTER_LIGHT));
		// addChild(createLight<LargeLight<RedLight>>(Vec(80, 362), module, QuadAlgorithmicRhythm::IS_SLAVE_LIGHT));

		addChild(createLight<LargeLight<RedLight>>(Vec(415, 347), module, QuadAlgorithmicRhythm::MUTED_LIGHT));
		
	}
};

Model *modelQuadAlgorithmicRhythm = createModel<QuadAlgorithmicRhythm, QuadAlgorithmicRhythmWidget>("QuadAlgorithmicRhythm");
