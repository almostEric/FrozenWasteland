#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FrozenWasteland.hpp"
#include "model/ChristoffelWords.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "dsp-noise/noise.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 16
#define NUM_ALGORITHMS 3
#define EXPANDER_MAX_STEPS 18
#define NUM_RULERS 10
#define MAX_DIVISIONS 6

#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define PASSTHROUGH_OFFSET EXPANDER_MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT

using namespace frozenwasteland::dsp;

struct PWAlgorithmicExpander : Module {
	enum ParamIds {
		STEPS_1_PARAM,
		DIVISIONS_1_PARAM,
		OFFSET_1_PARAM,
		PAD_1_PARAM,
		ALGORITHM_1_PARAM,
        RATIO_1_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		STEPS_1_INPUT,
		DIVISIONS_1_INPUT,
		OFFSET_1_INPUT,
		PAD_1_INPUT,
        ALGORITHM_1_INPUT,
        RATIO_1_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
        CLOCK_LIGHT,
        ALGORITHM_1_LIGHT,
		NUM_LIGHTS = ALGORITHM_1_LIGHT+3
	};
	enum Algorithms {
		EUCLIDEAN_ALGO,
		GOLUMB_RULER_ALGO,
        WELL_FORMED_ALGO
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
    float leftMessages[2][MAX_STEPS * 15] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	
		
    int algorithmMatrix;
	bool beatMatrix[MAX_STEPS];

	float probabilityMatrix[MAX_STEPS];
	float swingMatrix[MAX_STEPS];
	float beatWarpMatrix[MAX_STEPS];
	float irrationalRhythmMatrix[MAX_STEPS];
	double totalIrrationalAdjustment;
	float probabilityGroupModeMatrix[MAX_STEPS];
	int probabilityGroupTriggered;
	int probabilityGroupFirstStep;
	float workingProbabilityMatrix[MAX_STEPS];
	float workingSwingMatrix[MAX_STEPS];
	float workingBeatWarpMatrix[MAX_STEPS];
    bool probabilityResultMatrix[MAX_STEPS] = {1};
    float workingIrrationalRhythmMatrix[MAX_STEPS] = {1};
	float workingConditionalMatrix[MAX_STEPS];
	int conditionalMatrix[MAX_STEPS];
	int conditionalCounterMatrix[MAX_STEPS] = {0};


    std::string trackPatternName = "";

    //Stuff for Advanced Rhythms
    int lastAlgorithmSetting = 0;
    int lastStepsSetting = 0;
    int lastDivisionsSetting = 0;
    int lastOffsetSetting = 0;
    int lastPadSetting = 0;
    float lastExtraParameterValue = 0.0;

    float extraParameterValue= 0; //r for Well Formed, evenness for Perfect Balanced
    double wellFormedStepDurations[MAX_STEPS] = {0};
    double wellFormedTrackDuration= 0;
    double lastWellFormedTrackDuration= 0;

    bool dirty = true;



    float expanderDelayTime[MAX_STEPS] = {0};

	int beatIndex;
	int beatLocation[MAX_STEPS] = {0};
	int beatCount = 0;
	int stepsCount;
	int lastStepsCount;
	double stepDuration;
	double lastStepTime;	
    double lastSwingDuration;

	float swingRandomness;
	bool useGaussianDistribution = {false};
	double calculatedSwingRandomness = {0.0f};
	bool trackSwingUsingDivs = {false};
	int subBeatLength;
	int subBeatIndex;

	float beatWarping;
	int beatWarpingPosition;
    int beatWarpingLength;




	float expanderOutputValue;
	float expanderAccentValue;
	float expanderEocValue;
	float lastExpanderEocValue;
	
	float expanderClockValue = 0;
    float expanderClockDivision = 1;

	double maxStepCount;
	double masterStepCount;
	bool slaveQARsPresent;
	bool masterQARPresent;

    const int rulerOrders[NUM_RULERS] = {1,2,3,4,5,5,6,6,6,6};
	const int rulerLengths[NUM_RULERS] = {0,1,3,6,11,11,17,17,17,17};
	const std::string rulerNames[NUM_RULERS] = {"1","2","3","4","5a","5b","6a","6b","6c","6d"};
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

	bool running;
	bool initialized = false;
	bool QARExpanderDisconnectReset = true;
    bool expanderDataChanged = false;

	double timeElapsed = 0.0;
	double duration = 0.0;
	bool firstClockReceived = false;

	dsp::SchmittTrigger clockTrigger,resetTrigger,algorithmButtonTrigger,algorithmInputTrigger,startTrigger;
	dsp::PulseGenerator beatPulse,eocPulse;

	GaussianNoiseGenerator _gauss;

    ChristoffelWords christoffelWords;
    std::string currentChristoffelword = {"unknown"};

    //percentages
    float stepsPercentage = 0;
    float divisionsPercentage = 0;
    float offsetPercentage = 0;
    float padPercentage = 0;
    float ratioPercentage = 0;



	PWAlgorithmicExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

	
		configParam(STEPS_1_PARAM, 1.0, 16, 16.0,"# Steps");
		configParam(DIVISIONS_1_PARAM, 0.0, 16, 2.0,"# Beats");
		configParam(OFFSET_1_PARAM, 0.0, 15, 0.0,"Step Offset");
		configParam(PAD_1_PARAM, 0.0, 15, 0.0,"Step Padding");
        configParam(ALGORITHM_1_PARAM, 0.0, 1.0, 0.0);
        configParam(RATIO_1_PARAM, 0.0, 1.0, 0.5,"Well Formed Ratio");

		configButton(ALGORITHM_1_PARAM,"Algorithm");

		configInput(ALGORITHM_1_INPUT, "Algorithm");
		configInput(STEPS_1_INPUT, "# Steps");
		configInput(DIVISIONS_1_INPUT, "# Beats");
		configInput(OFFSET_1_INPUT, "Step Offset");
		configInput(PAD_1_INPUT, "Step Padding");
		configInput(RATIO_1_INPUT, "Well Formed Ratio");



		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
        rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		srand(time(NULL));
		

        beatIndex = -1;
        stepsCount = MAX_STEPS;
        lastStepsCount = -1;
        lastStepTime = 0.0;
        stepDuration = 0.0;
        lastSwingDuration = 0.0;
        subBeatIndex = -1;
        swingRandomness = 0.0f;
        useGaussianDistribution = false;	
        probabilityGroupTriggered = PENDING_PGTS;
        beatWarping = 1.0;
        beatWarpingPosition = 8;
    
        expanderOutputValue = 0.0;
        expanderAccentValue = 0.0;
        expanderEocValue = 0.0;
        lastExpanderEocValue = 0.0;

        running = true;
        for(int j = 0; j < MAX_STEPS; j++) {
            probabilityMatrix[j] = 1.0;
            conditionalMatrix[j] = 1.0;
            irrationalRhythmMatrix[j] = 1.0;
            swingMatrix[j] = 0.0;
            beatWarpMatrix[j] = 1.0;
            beatMatrix[j] = false;
        }

        onReset();	
	}


	void process(const ProcessArgs &args) override  {


        expanderOutputValue = 0; 
        lastExpanderEocValue = 0;

		// Initialize
		expanderClockValue = 0; 

		//See if a master is passing through an expander
		bool leftExpanderPresent = (leftExpander.module && (leftExpander.module->model == modelPortlandWeather || leftExpander.module->model == modelPWTapBreakoutExpander || 
                                    leftExpander.module->model == modelPWGridControlExpander));
		if(leftExpanderPresent)
		{
            float *messagesFromMother = (float*)leftExpander.consumerMessage;
			
			expanderClockValue = messagesFromMother[0]; 
            expanderClockDivision = messagesFromMother[1];
		}
        lights[CLOCK_LIGHT].value = expanderClockValue;

		//Set startup state	
		if(!initialized) {
			setRunningState();
			initialized = true;
		}

			
		maxStepCount = 0;

        if(algorithmButtonTrigger.process(params[(ALGORITHM_1_PARAM)].getValue())) {
            algorithmMatrix = (algorithmMatrix + 1) % NUM_ALGORITHMS; 
        }
        if(algorithmInputTrigger.process(inputs[(ALGORITHM_1_INPUT)].getVoltage())) {
            algorithmMatrix = (algorithmMatrix + 1) % NUM_ALGORITHMS; 
        }

        switch (algorithmMatrix) {
            case EUCLIDEAN_ALGO :
                lights[ALGORITHM_1_LIGHT].value = 1;
                lights[ALGORITHM_1_LIGHT+ 1].value = 1;
                lights[ALGORITHM_1_LIGHT+ 2].value = 0;
                break;
            case GOLUMB_RULER_ALGO :
                lights[ALGORITHM_1_LIGHT].value = 0;
                lights[ALGORITHM_1_LIGHT + 1].value = 0;
                lights[ALGORITHM_1_LIGHT + 2].value = 1;
                break;
            case WELL_FORMED_ALGO :
                lights[ALGORITHM_1_LIGHT].value = 0;
                lights[ALGORITHM_1_LIGHT + 1].value = .75;
                lights[ALGORITHM_1_LIGHT + 2].value = 0;
                break;
        }		
            
        
        float stepsCountf = std::floor(params[STEPS_1_PARAM].getValue());			
        if(inputs[STEPS_1_INPUT].isConnected()) {
            stepsCountf += inputs[STEPS_1_INPUT].getVoltage() * 1.6;
        }
        stepsCountf = clamp(stepsCountf,1.0f,16.0f);
        stepsPercentage = (stepsCountf - 1.0) / 15.0;

        float divisionf = std::floor(params[DIVISIONS_1_PARAM].getValue());
        if(inputs[DIVISIONS_1_INPUT].isConnected()) {
            divisionf += inputs[DIVISIONS_1_INPUT].getVoltage() * 1.5;
        }		
        divisionf = clamp(divisionf,0.0f,stepsCountf);
        divisionsPercentage = (divisionf-1.0) / 15.0;

        float offsetf = std::floor(params[OFFSET_1_PARAM].getValue());
        if(inputs[OFFSET_1_INPUT].isConnected()) {
            offsetf += inputs[OFFSET_1_INPUT].getVoltage() * 1.5;
        }	
        offsetf = clamp(offsetf,0.0f,15.0f);
        offsetPercentage = offsetf / 15.0;

        float padf = std::floor(params[PAD_1_PARAM].getValue());
        if(inputs[PAD_1_INPUT].isConnected()) {
            padf += inputs[PAD_1_INPUT].getVoltage() * 1.5;
        }
        padf = clamp(padf,0.0f,stepsCountf -1);
        padPercentage = padf / 15.0;
        // Reclamp
        divisionf = clamp(divisionf,0.0f,stepsCountf-padf);

        float t = params[RATIO_1_PARAM].getValue();
        if(inputs[RATIO_1_INPUT].isConnected()) {
            t += inputs[RATIO_1_INPUT].getVoltage() / 10.0;
        }	
        t = clamp(t,0.0f,1.0f);
        ratioPercentage = t;
        extraParameterValue = 1/(1-t);


        if(stepsCountf > maxStepCount)
            maxStepCount = std::floor(stepsCountf);

        stepsCount = int(stepsCountf);
        if(lastStepsCount == -1) //first time
            lastStepsCount = stepsCount;

        int division = int(divisionf);
        int offset = int(offsetf);		
        int pad = int(padf);

        
        //Parameters that cause a word to be created
        if(stepsCount != lastStepsSetting || division != lastDivisionsSetting 
            || algorithmMatrix != lastAlgorithmSetting || pad != lastPadSetting) {
                
            lastAlgorithmSetting = algorithmMatrix;
            lastStepsSetting = stepsCount;
            lastDivisionsSetting = division;
            lastPadSetting = pad;

            dirty = true;
            if(algorithmMatrix == WELL_FORMED_ALGO) {
                std::string word = christoffelWords.Generate(stepsCount,division);
                if(word != "unknown") {
                    currentChristoffelword = word;
                }
                //fprintf(stderr, "%s \n", word.c_str());
            }
        }

        //Everything else that forces beat recalc
        if(offset != lastOffsetSetting || extraParameterValue != lastExtraParameterValue) {
                
            lastOffsetSetting = offset;
            lastExtraParameterValue = extraParameterValue; 

            dirty = true;
        }

        if(dirty) {
            //clear out the matrix and levels
            for(int j=0;j<MAX_STEPS;j++)
            {
                beatLocation[j] = 0;
            }

            beatCount = 0;
            if(division > 0) {					
                int bucket = stepsCount - pad - 1;                    
                if(algorithmMatrix == EUCLIDEAN_ALGO ) { //Euclidean Algorithn
                    int euclideanBeatIndex = 0;
                    //Set padded steps to false
                    for(int euclideanStepIndex = 0; euclideanStepIndex < pad; euclideanStepIndex++) {
                        beatMatrix[((euclideanStepIndex + offset) % (stepsCount))] = false;	
                    }
                    for(int euclideanStepIndex = 0; euclideanStepIndex < stepsCount-pad; euclideanStepIndex++)
                    {
                        bucket += division;
                        if(bucket >= stepsCount-pad) {
                            bucket -= (stepsCount - pad);
                            beatMatrix[((euclideanStepIndex + offset + pad) % (stepsCount))] = true;	
                            beatLocation[euclideanBeatIndex] = (euclideanStepIndex + offset + pad) % stepsCount;	
                            euclideanBeatIndex++;	
                        } else
                        {
                            beatMatrix[((euclideanStepIndex + offset + pad) % (stepsCount))] = false;	
                        }                    
                    }
                    trackPatternName = std::to_string(stepsCount)+"-"+std::to_string(euclideanBeatIndex);
                    beatCount = euclideanBeatIndex;
                } else if(algorithmMatrix == GOLUMB_RULER_ALGO) { //Golomb Ruler Algorithm
                
                    int rulerToUse = clamp(division-1,0,NUM_RULERS-1);
                    int actualStepCount = stepsCount - pad;
                    while(rulerLengths[rulerToUse] + 1 > actualStepCount && rulerToUse >= 0) {
                        rulerToUse -=1;
                    } 
                    trackPatternName =std::to_string(stepsCount)+"-"+rulerNames[rulerToUse]; 
                    //Multiply beats so that low division beats fill out entire pattern
                    float spaceMultiplier = (actualStepCount / (rulerLengths[rulerToUse] + 1));

                    //Set all beats to false
                    for(int j=0;j<stepsCount;j++)
                    {
                        beatMatrix[j] = false; 			
                    }

                    for (int rulerIndex = 0; rulerIndex < rulerOrders[rulerToUse];rulerIndex++)
                    {
                        int divisionLocation = (rulers[rulerToUse][rulerIndex] * spaceMultiplier) + pad;
                        beatMatrix[(divisionLocation + offset) % stepsCount] = true;
                        beatLocation[rulerIndex] = (divisionLocation + offset) % stepsCount;	            
                    }
                    beatCount = rulerOrders[rulerToUse];
                } else if(algorithmMatrix == WELL_FORMED_ALGO) { 
                    int wfBeatCount = 0;
                    wellFormedTrackDuration = division + (stepsCount - division) * extraParameterValue; 
                    if(lastWellFormedTrackDuration == -1) //first time
                        lastWellFormedTrackDuration = wellFormedTrackDuration; 

                    for(int wfBeatIndex=0;wfBeatIndex<stepsCount;wfBeatIndex++) { //NEED TO HANDLE UNKNOWNS
                        int adjustedWfBeatIndex = (wfBeatIndex + offset + pad) % stepsCount;
                        char beatType = currentChristoffelword[wfBeatIndex];
                        if(beatType == 's') {
                            wellFormedStepDurations[adjustedWfBeatIndex] = 1.0;
                            beatLocation[wfBeatCount] = adjustedWfBeatIndex;	     
                            wfBeatCount++;       						
                        } else {
                            wellFormedStepDurations[adjustedWfBeatIndex] = extraParameterValue;
                        }
                        beatMatrix[adjustedWfBeatIndex] = true; 
                    }
                    trackPatternName = std::to_string(stepsCount-wfBeatCount)+"l "+std::to_string(wfBeatCount)+"s"; 
                    beatCount = division;
                }

                bucket = division - 1;
            } else { // No Divisions
                trackPatternName = "";
                for(int j=0;j<stepsCount;j++) {
                    beatMatrix[j] = false; 	
                }
            }	
        }
        dirty = false;

		//Get Expander Info
        //bool rightExpanderPresent = false;
		if(rightExpander.module && (rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARGrooveExpander || 
                                    rightExpander.module->model == modelQARWarpedSpaceExpander || rightExpander.module->model == modelQARIrrationalityExpander ||
                                    rightExpander.module->model == modelQARConditionalExpander))
		{			
			float *messagesFromExpanders = (float*)rightExpander.consumerMessage;

            expanderDataChanged = messagesFromExpanders[0] || (!QARExpanderDisconnectReset); //If an expander first gets hooked up get data changes
			
			QARExpanderDisconnectReset = true;

			//Process Probability Expander Stuff						
            if(expanderDataChanged) {
                probabilityGroupFirstStep = -1;
                for(int j = 0; j < stepsCount; j++) { //reset all probabilities, find first group step
                    workingProbabilityMatrix[j] = 1;					
                }

                if(messagesFromExpanders[TRACK_COUNT * 4 + 0] > 0) { // 0 is track not selected
                    bool useDivs = messagesFromExpanders[TRACK_COUNT * 4 + 0] == 2; //2 is divs
                    for(int j = 0; j < stepsCount; j++) { // Assign probabilites and swing
                        int stepIndex = j;
                        bool stepFound = true;
                        if(useDivs) { //Use j as a count to the div # we are looking for
                            if(j < beatCount) {
                                stepIndex = beatLocation[j];
                                // fprintf(stderr, "Probability Track:%i step:%i BL:%i \n",i,j,stepIndex);
                            } else {
                                stepFound = false;
                            }               
                        }
                        
                        if(stepFound) {
                            float probability = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (0 * EXPANDER_MAX_STEPS) + j];
                            float probabilityMode = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT) + (0 * EXPANDER_MAX_STEPS) + j];
                            
                            workingProbabilityMatrix[stepIndex] = probability;
                            probabilityGroupModeMatrix[stepIndex] = probabilityMode;
                            if(probabilityGroupFirstStep < 0 && probabilityGroupModeMatrix[stepIndex] != NONE_PGTM ) {
                                probabilityGroupFirstStep = stepIndex;
                            }
                        } 
                    }
                }


                // Process Conditional  Expander Stuff	
                for(int j = 0; j < stepsCount; j++) { //reset all conditiona, find first group step
                    workingConditionalMatrix[j] = 1;					
                }

                if(messagesFromExpanders[TRACK_COUNT * 16 + 0] > 0) { // 0 is track not selected
                    bool useDivs = messagesFromExpanders[TRACK_COUNT * 16 + 0] == 2; //2 is divs
                    for(int j = 0; j < stepsCount; j++) { // Assign probabilites and swing
                        int stepIndex = j;
                        bool stepFound = true;
                        if(useDivs) { //Use j as a count to the div # we are looking for
                            if(j < beatCount) {
                                stepIndex = beatLocation[j];
                                // fprintf(stderr, "Conditional step:%i BL:%i \n",j,stepIndex);
                            } else {
                                stepFound = false;
                            }
                        }
                        
                        if(stepFound) {
                            int divideCount = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 4) + (0 * EXPANDER_MAX_STEPS) + j];
                            // fprintf(stderr, "Conditional Track:%i step:%i DC:%i \n",i,stepIndex,divideCount);
                            workingConditionalMatrix[stepIndex] = divideCount;
                        } 
                    }
                }

            
                //Process Groove Expander Stuff									
                for(int j = 0; j < stepsCount; j++) { //reset all probabilities
                    workingSwingMatrix[j] = 0.0;
                }

                if(messagesFromExpanders[TRACK_COUNT * 5 + 0] > 0) { // 0 is track not selected
                    bool useDivs = messagesFromExpanders[TRACK_COUNT * 5 + 0] == 2; //2 is divs
                    trackSwingUsingDivs = useDivs;

                    int grooveLength = (int)(messagesFromExpanders[TRACK_COUNT * 6 + 0]);
                    bool useTrackLength = messagesFromExpanders[TRACK_COUNT * 7 + 0];

                    swingRandomness = messagesFromExpanders[TRACK_COUNT * 8 + 0];
                    useGaussianDistribution = messagesFromExpanders[TRACK_COUNT * 9 + 0];

                    if(useTrackLength) {
                        grooveLength = stepsCount;
                    }
                    subBeatLength = grooveLength;
                    // if(subBeatIndex >= grooveLength) { //Reset if necessary
                    subBeatIndex = 0;
                    // }
                    

                    int workingBeatIndex = 0;

                    for(int j = 0; j < stepsCount; j++) { // Assign probabilites and swing
                        int stepIndex = j;
                        bool stepFound = true;
                        if(useDivs) { //Use j as a count to the div # we are looking for
                            if(j < beatCount) { //hard coding an 8 for test
                                stepIndex = beatLocation[j];
                            } else {
                                stepFound = false;
                            }
                        }
                        
                        if(stepFound) {
                            float swing = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 2) + (0 * EXPANDER_MAX_STEPS) + workingBeatIndex];
                            workingSwingMatrix[stepIndex] = swing;						
                        } 
                        workingBeatIndex +=1;
                        if(workingBeatIndex >= grooveLength) {
                            workingBeatIndex = 0;
                        }
                    }
                }
            

                // Process Warped Space Stuff									
                for(int j = 0; j < stepsCount; j++) { //reset all warping
                    workingBeatWarpMatrix[j] = 1.0;
                }

                if(messagesFromExpanders[TRACK_COUNT * 10 + 0] > 0) { // 0 is track not selected
                    beatWarping = messagesFromExpanders[TRACK_COUNT * 11 + 0];
                    beatWarpingPosition = (int)messagesFromExpanders[TRACK_COUNT * 12 + 0];
                    beatWarpingLength = (int)messagesFromExpanders[TRACK_COUNT * 13 + 0];
                    
                    float trackWarpedStepCount = (float)(std::min(beatWarpingLength,stepsCount));					
                    float stepsToSpread = (trackWarpedStepCount / 2.0)-1;
                    float fraction = 1.0/beatWarping;
                    for(int j = 0; j < trackWarpedStepCount; j++) {	
                        int actualBeat = (j + beatWarpingPosition) % stepsCount; 
                        float fj = (float)j;					 
                        if(j <= stepsToSpread)
                            workingBeatWarpMatrix[actualBeat] = (2-fraction)*(stepsToSpread-fj)/stepsToSpread + (fraction*fj/stepsToSpread); 
                        else							
                            workingBeatWarpMatrix[actualBeat] = (2-fraction)*(fj-stepsToSpread-1.0)/stepsToSpread + (fraction*(trackWarpedStepCount-fj-1.0)/stepsToSpread); 						
                    }
                }

                //Process Irrational Rhythms
                for(int j = 0; j < stepsCount; j++) { //reset all irrationals
                    workingIrrationalRhythmMatrix[j] = 1.0;
                }

                for(int j = 0; j < EXPANDER_MAX_STEPS; j+=3) { // find any irrationals
                    int irPos = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 3) + (0 * EXPANDER_MAX_STEPS) + j];
                    if(irPos != 0) {
                        bool useDivs = irPos < 0;
                        irPos = std::abs(irPos-1); //Get back to 0 based
                        int stepIndex = irPos;
                        bool stepFound = true;
                        if(useDivs) { //Use j as a count to the div # we are looking for
                            if(irPos < beatCount) {
                                stepIndex = beatLocation[irPos];
                                // fprintf(stderr, "Probability Track:%i step:%i BL:%i \n",i,j,stepIndex);
                            } else {
                                stepFound = false;
                            }
                        }
                                        
                        if(stepFound) {
                            float irNbrSteps = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 3) + (0 * EXPANDER_MAX_STEPS) + j + 1];
                            float ratio = messagesFromExpanders[TRACK_LEVEL_PARAM_COUNT + (EXPANDER_MAX_STEPS * TRACK_COUNT * 3) + (0 * EXPANDER_MAX_STEPS) + j + 2];
                            float actualRatio = std::min(ratio/irNbrSteps,1.0f);
                            for(int k=0;k<irNbrSteps;k++) {
                                if(stepIndex+k < 16) {
                                    workingIrrationalRhythmMatrix[stepIndex+k] *= actualRatio; // allow rhythms to nest
                                } else {
                                    break;
                                }
                            }
                        } 
                    } else {
                        break; // no more irrational rhythms
                    }
                }			

        		//set calculated probability and swing
                for(int j = 0; j < MAX_STEPS; j++) { 
                    probabilityMatrix[j] = workingProbabilityMatrix[j];
                    conditionalMatrix[j] = workingConditionalMatrix[j];
                    swingMatrix[j] = workingSwingMatrix[j];
                    beatWarpMatrix[j] = workingBeatWarpMatrix[j];
                    irrationalRhythmMatrix[j] = workingIrrationalRhythmMatrix[j];                    
                }
            }
            rightExpander.module->leftExpander.messageFlipRequested = true;			
		} else {
			if(QARExpanderDisconnectReset) { //If QRE gets disconnected, reset warping, probability and swing
                subBeatIndex = 0;
                beatWarping = 1.0;
                for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities
                    probabilityMatrix[j] = 1.0;
                    conditionalMatrix[j] = 1;
                    irrationalRhythmMatrix[j] = 1.0;
                    swingMatrix[j] = 0;
                    beatWarpMatrix[j] = 1.0;
                    swingRandomness = 0.0f;
                }
            }
            QARExpanderDisconnectReset = false;
		}


		//Calculate clock duration
		double timeAdvance =1.0 / args.sampleRate;
		timeElapsed += timeAdvance;

		float clockInput = expanderClockValue;	

        bool clockReceived = false; // This will be used to trigger recalculations of probabilities and randomness
        if(clockTrigger.process(clockInput)) {
            if(firstClockReceived) {
                duration = timeElapsed;
            }
            timeElapsed = 0;
            firstClockReceived = true;		
            clockReceived = true;					
        } else if(firstClockReceived && timeElapsed > duration) {  //allow absense of second clock to affect duration
            duration = timeElapsed;
        }			
        
            
        beatIndex = -1;
        float delayTime = 0;
        if(clockReceived) {
            probabilityGroupTriggered = PENDING_PGTS;
        }
        for(int step=0;step<stepsCount;step++) {   
            double beatSizeAdjustment = beatIndex >= 0 ? (algorithmMatrix == WELL_FORMED_ALGO ? wellFormedStepDurations[step] : 1.0) * beatWarpMatrix[step] : 1.0;
            stepDuration = duration / (expanderClockDivision * MAX_STEPS) * beatSizeAdjustment; 
            //swing is affected by next beat
            int nextBeat = beatIndex + 1;
            if(nextBeat >= stepsCount)
                nextBeat = 0;
            double swingDuration = (calculatedSwingRandomness + swingMatrix[nextBeat]) * stepDuration;

            delayTime += stepDuration + swingDuration - lastSwingDuration;
            lastSwingDuration = swingDuration;
            lastStepsCount = stepsCount;
            bool probabilityResult = advanceBeat(clockReceived);
            if(clockReceived) {
                probabilityResultMatrix[beatIndex] = probabilityResult;
            }
            expanderDelayTime[beatIndex] = beatMatrix[beatIndex] && probabilityResultMatrix[beatIndex] ? delayTime : -1;
        }
        for(int step=stepsCount;step<MAX_STEPS;step++) {   //Mute the rest
            expanderDelayTime[step] = -1;
        }	

		if(leftExpanderPresent) {
            float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
            messagesToMother[3] = true; // Algorithm Expander present 
            for(int i=0;i<MAX_STEPS;i++) {                    
                messagesToMother[MAX_STEPS * 7 + i ] = expanderDelayTime[i]; 
            }
			leftExpander.module->rightExpander.messageFlipRequested = true;
		} 			
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
        json_object_set_new(rootJ, "algorithm", json_integer((int) algorithmMatrix));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		json_t *aJ = json_object_get(rootJ, "algorithm");
		if (aJ)
			algorithmMatrix = json_integer_value(aJ);
	}

	void setRunningState() {
        running = true;
	}

	bool advanceBeat(bool calculateProbabilities) {
       
		beatIndex++;
		lastStepTime = 0.0;
    
		//End of Cycle
		// if(beatIndex >= stepsCount) {
		// 	// beatIndex = 0;
		// 	// eocPulse.trigger(1e-3);
		// 	probabilityGroupTriggered = PENDING_PGTS;
		// }

		if(!trackSwingUsingDivs) {
			subBeatIndex++;
			if(subBeatIndex >= subBeatLength) { 
				subBeatIndex = 0;
			}
		} else if(trackSwingUsingDivs && beatMatrix[beatIndex] == true) {
			subBeatIndex++;
			if(subBeatIndex >= subBeatLength) { 
				subBeatIndex = 0;
			}
		}

        bool probabilityResult = true;
        bool conditionalResult = true;
        if(calculateProbabilities) {
            probabilityResult = (float) rand()/RAND_MAX < probabilityMatrix[beatIndex];	
            if(probabilityGroupModeMatrix[beatIndex] != NONE_PGTM) {
                if(probabilityGroupFirstStep == beatIndex) {
                    probabilityGroupTriggered = probabilityResult ? TRIGGERED_PGTS : NOT_TRIGGERED_PGTS;
                } else if(probabilityGroupTriggered == NOT_TRIGGERED_PGTS) {
                    probabilityResult = false;
                }
            }

            if(useGaussianDistribution) {
                bool gaussOk = false; // don't want values that are beyond our mean
                float gaussian;
                do {
                    gaussian= _gauss.next();
                    gaussOk = gaussian >= -1 && gaussian <= 1;
                } while (!gaussOk);
                calculatedSwingRandomness =  gaussian / 2 * swingRandomness;
            } else {
                calculatedSwingRandomness =  (((double) rand()/RAND_MAX - 0.5f) * swingRandomness);
            }

            conditionalCounterMatrix[beatIndex] -=1;
            conditionalResult = conditionalCounterMatrix[beatIndex] <= 0;
            if(conditionalResult) {
                conditionalCounterMatrix[beatIndex] = conditionalMatrix[beatIndex];
            }
        }

        

        return probabilityResult && conditionalResult;
	}
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
        algorithmMatrix = EUCLIDEAN_ALGO;
        beatIndex = -1;
        stepsCount = MAX_STEPS;
        lastStepTime = 0.0;
        stepDuration = 0.0;
        lastSwingDuration = 0.0;
        expanderAccentValue = 0.0;
        expanderOutputValue = 0.0;
        expanderEocValue = 0; 
        lastExpanderEocValue = 0;
        probabilityGroupTriggered = PENDING_PGTS;
        swingRandomness = 0.0f;
        useGaussianDistribution = false;	
        subBeatIndex = -1;
        running = true;
        for(int j = 0; j < MAX_STEPS; j++) {
            probabilityMatrix[j] = 1.0;
            swingMatrix[j] = 0.0;
            beatMatrix[j] = false;
            beatWarpMatrix[j] = 1.0;		
        }
    }	
};


struct PWAEBeatDisplay : FramebufferWidget {
	PWAlgorithmicExpander *module;
	int frame = 0;
	std::shared_ptr<Font> digitalFont;
	std::shared_ptr<Font> textFont;

	std::string digitalFontPath;
	std::string textFontPath;


	PWAEBeatDisplay() {
        digitalFontPath =asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf");
        textFontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
	}


    void drawArc(const DrawArgs &args, float stepsCount, float stepNumber, float runningTrackWidth, int algorithm, bool isBeat, float beatWarp, float probability, 
                int countsLeft, int divideCount, int triggerState, int probabilityGroupMode, float swing, float nextSwing, float swingRandomness, 
                float wfTrackDurationAdjustment, float wfStepDuration) 
	{		
        const float rotate90 = (M_PI) / 2.0;

        float opacity = 0x80; // Testing using opacity for accents

		//Default Euclidean Colors
		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor randomFillColor = nvgRGBA(0xff,0x00,0,0x40);			
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
		}		
        
        nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));

    
        double baseStartDegree = (M_PI * 2.0 / stepsCount * (runningTrackWidth + swing)) - rotate90;
		double wfModifier = algorithm == module->WELL_FORMED_ALGO ? (wfStepDuration*wfTrackDurationAdjustment) : 1.0;
        double baseEndDegree = baseStartDegree + (M_PI * 2.0 / stepsCount * (wfModifier * beatWarp - swing + nextSwing));
		if(baseEndDegree < baseStartDegree) {
			baseEndDegree = baseStartDegree;
        }

        
        
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.0);
        nvgBeginPath(args.vg);
        nvgArc(args.vg,89,80.5,85.0,baseStartDegree,baseEndDegree,NVG_CW);
        double x= cos(baseEndDegree) * 65.0 + 89.0;
        double y= sin(baseEndDegree) * 65.0 + 80.5;
        nvgLineTo(args.vg,x,y);
        nvgArc(args.vg,89,80.5,65.0,baseEndDegree,baseStartDegree,NVG_CCW);
        nvgClosePath(args.vg);		
        nvgStroke(args.vg);

        float startDegree = baseStartDegree;
        float endDegree = baseEndDegree;
        if(isBeat) {
            nvgBeginPath(args.vg);
            nvgStrokeWidth(args.vg, 0.0);
            nvgArc(args.vg,89,80.5,65.0 + (20.0 * probability) ,startDegree,endDegree,NVG_CW);
            double x= cos(endDegree) * 65.0 + 89.0;
            double y= sin(endDegree) * 65.0 + 80.5;
            nvgLineTo(args.vg,x,y);
            nvgArc(args.vg,89,80.5,65.0,endDegree,startDegree,NVG_CCW);
            nvgClosePath(args.vg);		
			NVGpaint paint = nvgRadialGradient(args.vg,89.0, 80.0, 65.0, 65.0 + (20*probability),
					outerFillColor, innerFillColor);
			nvgFillPaint(args.vg, paint);
            // nvgFillColor(args.vg, fillColor);
            nvgFill(args.vg);
            nvgStroke(args.vg);

            //Draw swing randomness
            if(swingRandomness > 0.0f) {
                float width = (baseEndDegree - baseStartDegree) * swingRandomness / 2.0; 
                nvgBeginPath(args.vg);
                nvgStrokeWidth(args.vg, 0.0);
                nvgArc(args.vg,89,80.5,65.0 + (20.0 * probability),startDegree,startDegree+width,NVG_CW);
                double srx= cos(startDegree+width) * 65.0 + 89.0;
                double sry= sin(startDegree+width) * 65.0 + 80.5;
                nvgLineTo(args.vg,srx,sry);
                nvgArc(args.vg,89,80.5,65.0,startDegree+width,startDegree,NVG_CCW);
                nvgFillColor(args.vg, randomFillColor);
                nvgFill(args.vg);
                nvgStroke(args.vg);
            }

            //draw conditional status
			if(divideCount > 1) {
				nvgBeginPath(args.vg);
				nvgStrokeWidth(args.vg, 0.5);
				nvgStrokeColor(args.vg, countsLeft > 1 ? nvgRGBA(0xff, 0x1f, 0, 0xaf) : nvgRGBA(0x1f, 0xff, 0x1f, 0xaf));				
				nvgFillColor(args.vg, countsLeft > 1 ? nvgRGBA(0xff, 0x1f, 0, 0xaf) : nvgRGBA(0x1f, 0xff, 0x1f, 0xaf));				
					
				double cx= ((cos(startDegree) * 85.0 + 89.0) + x) / 2.0;
            	double cy= ((sin(startDegree) * 85.0 + 80.0) + y) / 2.0;
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
				double sx= cos(startDegree) * 85.0 + 89.0;
            	double sy= sin(startDegree) * 85.0 + 80.5;
				nvgMoveTo(args.vg,sx,sy);
				nvgLineTo(args.vg,x,y);
				// //nvgStroke(args.vg);
				// nvgMoveTo(args.vg,boxX+baseWidth-1,boxY+1);
				// nvgLineTo(args.vg,boxX+1,boxY+20);
				nvgStroke(args.vg);		
			}
        }
	}

    void drawPatternNameTrack(const DrawArgs &args, Vec pos, std::string trackPatternName) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, textFont->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_CENTER);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[32];
		snprintf(text, sizeof(text), "%s", trackPatternName.c_str() );
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}


	void draw(const DrawArgs &args) override {
        digitalFont = APP->window->loadFont(digitalFontPath);
		textFont = APP->window->loadFont(textFontPath);

		if (!module)
			return;
		
        int algorithn = module->algorithmMatrix;
        float runningTrackWidth = 0.0;
        int stepsCount = module->stepsCount;
        float wfTrackDurationAdjustment = stepsCount / (module->wellFormedTrackDuration);
        for(int stepNumber = 0;stepNumber < stepsCount;stepNumber++) {		
            int nextStepNumber = (stepNumber + 1) % stepsCount;					
            bool isBeat = module->beatMatrix[stepNumber];
            float probability = module->probabilityMatrix[stepNumber];
            int countsLeft = module->conditionalCounterMatrix[stepNumber];
            int divideCount = module->conditionalMatrix[stepNumber];
            float swing = module->swingMatrix[stepNumber];	
            float nextSwing = module->swingMatrix[nextStepNumber];	
            float swingRandomness = module->swingRandomness;
            float beatWarp = module->beatWarpMatrix[stepNumber];
            float irrationalRhythm = module->irrationalRhythmMatrix[stepNumber];
            float beatSizeAdjust = beatWarp * irrationalRhythm;
            float wfStepDuration = module->wellFormedStepDurations[stepNumber];
            int triggerState = module->probabilityGroupTriggered;
            int probabilityGroupMode = module->probabilityGroupModeMatrix[stepNumber];
            drawArc(args, float(stepsCount), float(stepNumber),runningTrackWidth,algorithn,isBeat,beatSizeAdjust,probability,
                            countsLeft,divideCount,triggerState,probabilityGroupMode,
                            swing,nextSwing,swingRandomness,wfTrackDurationAdjustment, wfStepDuration);
            runningTrackWidth += beatSizeAdjust * (algorithn == module->WELL_FORMED_ALGO ? wfStepDuration * wfTrackDurationAdjustment : 1);
        }		

        drawPatternNameTrack(args, Vec(90 , 184), module->trackPatternName);
	}
};


struct PWAlgorithmicExpanderWidget : ModuleWidget {
	PWAlgorithmicExpanderWidget(PWAlgorithmicExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PWAlgorithmicExpander.svg")));

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		{
			PWAEBeatDisplay *display = new PWAEBeatDisplay();
			display->module = module;
			display->box.pos = Vec(16, 34);
			display->box.size = Vec(box.size.x-31, 351);
			addChild(display);
		}


		addParam(createParam<LEDButton>(Vec(10, 250), module, PWAlgorithmicExpander::ALGORITHM_1_PARAM));
		ParamWidget* stepsParam = createParam<RoundSmallFWSnapKnob>(Vec(40, 248), module, PWAlgorithmicExpander::STEPS_1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(stepsParam)->percentage = &module->stepsPercentage;
		}
		addParam(stepsParam);							
		ParamWidget* divisionsParam = createParam<RoundSmallFWSnapKnob>(Vec(74, 248), module, PWAlgorithmicExpander::DIVISIONS_1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(divisionsParam)->percentage = &module->divisionsPercentage;
		}
		addParam(divisionsParam);							
		ParamWidget* offsetParam = createParam<RoundSmallFWSnapKnob>(Vec(108, 248), module, PWAlgorithmicExpander::OFFSET_1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(offsetParam)->percentage = &module->offsetPercentage;
		}
		addParam(offsetParam);							
		ParamWidget* padParam = createParam<RoundSmallFWSnapKnob>(Vec(142, 248), module, PWAlgorithmicExpander::PAD_1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(padParam)->percentage = &module->padPercentage;
		}
		addParam(padParam);							
		ParamWidget* ratioParam = createParam<RoundSmallFWKnob>(Vec(176, 248), module, PWAlgorithmicExpander::RATIO_1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(ratioParam)->percentage = &module->ratioPercentage;
		}
		addParam(ratioParam);							

        addInput(createInput<FWPortInSmall>(Vec(11, 276), module, PWAlgorithmicExpander::ALGORITHM_1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(42, 276), module, PWAlgorithmicExpander::STEPS_1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(76, 276), module, PWAlgorithmicExpander::DIVISIONS_1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(110, 276), module, PWAlgorithmicExpander::OFFSET_1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(144, 276), module, PWAlgorithmicExpander::PAD_1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(178, 276), module, PWAlgorithmicExpander::RATIO_1_INPUT));


        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 251.5), module, PWAlgorithmicExpander::ALGORITHM_1_LIGHT));


        //addChild(createLight<LargeLight<BlueLight>>(Vec(27.5, 341.5), module, PWAlgorithmicExpander::CLOCK_LIGHT));

	}
};

Model *modelPWAlgorithmicExpander = createModel<PWAlgorithmicExpander, PWAlgorithmicExpanderWidget>("PWAlgorithmicExpander");
