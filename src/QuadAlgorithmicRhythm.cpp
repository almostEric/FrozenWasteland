#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 16
#define EXPANDER_MAX_STEPS 18
#define NUM_RULERS 10
#define MAX_DIVISIONS 6


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
		RESET_MODE_PARAM,
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
		NUM_LIGHTS
	};
	enum ChainModes {
		CHAIN_MODE_NONE,
		CHAIN_MODE_BOSS,
		CHAIN_MODE_EMPLOYEE
	};

	
    bool algorithnMatrix[TRACK_COUNT];
	bool beatMatrix[TRACK_COUNT][MAX_STEPS];
	bool accentMatrix[TRACK_COUNT][MAX_STEPS];
	float probabilityMatrix[TRACK_COUNT][MAX_STEPS];
	float swingMatrix[TRACK_COUNT][MAX_STEPS];
	int beatIndex[TRACK_COUNT];
	int stepsCount[TRACK_COUNT];
	float stepDuration[TRACK_COUNT];
	float lastStepTime[TRACK_COUNT];
    float lastSwingDuration[TRACK_COUNT];
	float maxStepCount;
	float masterStepCount;

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
	int chainMode;
	bool initialized = false;
	bool muted = false;
	bool constantTime = false;
	int masterTrack = 0;

	float time = 0.0;
	float duration = 0.0;
	bool secondClockReceived = false;

	dsp::SchmittTrigger clockTrigger,resetTrigger,chainModeTrigger,constantTimeTrigger,muteTrigger,algorithmButtonTrigger[TRACK_COUNT],algorithmInputTrigger[TRACK_COUNT],startTrigger[TRACK_COUNT];
	dsp::PulseGenerator beatPulse[TRACK_COUNT],accentPulse[TRACK_COUNT],eocPulse[TRACK_COUNT];



	QuadAlgorithmicRhythm() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

	
		configParam(STEPS_1_PARAM, 0.0, 18, 16.0);
		configParam(DIVISIONS_1_PARAM, 1.0, 18, 2.0);
		configParam(OFFSET_1_PARAM, 0.0, 17, 0.0);
		configParam(PAD_1_PARAM, 0.0, 17, 0.0);
		configParam(ACCENTS_1_PARAM, 0.0, 17, 0.0);
		configParam(ACCENT_ROTATE_1_PARAM, 0.0, 17, 0.0);
        configParam(ALGORITHM_1_PARAM, 0.0, 1.0, 1.0);

		configParam(STEPS_2_PARAM, 0.0, 18.0, 16.0);
		configParam(DIVISIONS_2_PARAM, 1.0, 18, 2.0);
		configParam(OFFSET_2_PARAM, 0.0, 17, 0.0);
		configParam(PAD_2_PARAM, 0.0, 17, 0.0);
		configParam(ACCENTS_2_PARAM, 0.0, 17, 0.0);
		configParam(ACCENT_ROTATE_2_PARAM, 0.0, 17, 0.0);
        configParam(ALGORITHM_2_PARAM, 0.0, 1.0, 1.0);

		configParam(STEPS_3_PARAM, 0.0, 18, 16.0);
		configParam(DIVISIONS_3_PARAM, 1.0, 18, 2.0);
		configParam(OFFSET_3_PARAM, 0.0, 17, 0.0);
		configParam(PAD_3_PARAM, 0.0, 17, 0.0);
		configParam(ACCENTS_3_PARAM, 0.0, 17, 0.0);
		configParam(ACCENT_ROTATE_3_PARAM, 0.0, 17, 0.0);
        configParam(ALGORITHM_3_PARAM, 0.0, 1.0, 1.0);

		configParam(STEPS_4_PARAM, 0.0, 18, 16.0);
		configParam(DIVISIONS_4_PARAM, 1.0, 18, 2.0);
		configParam(OFFSET_4_PARAM, 0.0, 17, 0.0);
		configParam(PAD_4_PARAM, 0.0, 17, 0.0);
		configParam(ACCENTS_4_PARAM, 0.0, 17, 0.0);
		configParam(ACCENT_ROTATE_4_PARAM, 0.0, 17, 0.0);
        configParam(ALGORITHM_4_PARAM, 0.0, 1.0, 1.0);

		configParam(CHAIN_MODE_PARAM, 0.0, 1.0, 0.0);
		configParam(CONSTANT_TIME_MODE_PARAM, 0.0, 1.0, 0.0);
		
		//srand((unsigned)time(NULL));

		for(int i = 0; i < TRACK_COUNT; i++) {
            algorithnMatrix[i] = true;
			beatIndex[i] = 0;
			stepsCount[i] = MAX_STEPS;
			lastStepTime[i] = 0.0;
			stepDuration[i] = 0.0;
            lastSwingDuration[i] = 0.0;
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
				beatIndex[trackNumber] = 0;
                lastStepTime[trackNumber] = 0;
                lastSwingDuration[trackNumber] = 0; // Not sure about this
			}
			setRunningState();
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
                algorithnMatrix[trackNumber] = !algorithnMatrix[trackNumber];
            }
            if(algorithmInputTrigger[trackNumber].process(inputs[(ALGORITHM_1_INPUT + trackNumber * 8)].getVoltage())) {
                algorithnMatrix[trackNumber] = !algorithnMatrix[trackNumber];
            }
            lights[ALGORITHM_1_LIGHT + trackNumber*3].value = algorithnMatrix[trackNumber];
            lights[ALGORITHM_1_LIGHT + trackNumber*3 + 1].value = algorithnMatrix[trackNumber];
            lights[ALGORITHM_1_LIGHT + trackNumber*3 + 2].value = !algorithnMatrix[trackNumber];
            
            
			//clear out the matrix and levels
			for(int j=0;j<MAX_STEPS;j++)
			{
				beatLocation[j] = 0;
			}

			float stepsCountf = params[(trackNumber * 7) + STEPS_1_PARAM].getValue();
			if(inputs[trackNumber * 8].isConnected()) {
				stepsCountf += inputs[trackNumber * 8 + STEPS_1_INPUT].getVoltage() * 1.8;
			}
			stepsCountf = clamp(stepsCountf,0.0f,18.0f);	

			float divisionf = params[(trackNumber * 7) + DIVISIONS_1_PARAM].getValue();
			if(inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].isConnected()) {
				divisionf += inputs[(trackNumber * 8) + DIVISIONS_1_INPUT].getVoltage() * 1.7;
			}		
			divisionf = clamp(divisionf,1.0f,stepsCountf);

			float offsetf = params[(trackNumber * 7) + OFFSET_1_PARAM].getValue();
			if(inputs[(trackNumber * 8) + OFFSET_1_INPUT].isConnected()) {
				offsetf += inputs[(trackNumber * 8) + OFFSET_1_INPUT].getVoltage() * 1.7;
			}	
			offsetf = clamp(offsetf,0.0f,17.0f);

			float padf = params[trackNumber * 7 + PAD_1_PARAM].getValue();
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

			float accentDivisionf = params[(trackNumber * 7) + ACCENTS_1_PARAM].getValue() * divisionScale;
			if(inputs[(trackNumber * 8) + ACCENTS_1_INPUT].isConnected()) {
				accentDivisionf += inputs[(trackNumber * 8) + ACCENTS_1_INPUT].getVoltage() * divisionScale;
			}
			accentDivisionf = clamp(accentDivisionf,0.0f,divisionf);

			float accentRotationf = params[(trackNumber * 7) + ACCENT_ROTATE_1_PARAM].getValue() * divisionScale;
			if(inputs[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].isConnected()) {
				accentRotationf += inputs[(trackNumber * 8) + ACCENT_ROTATE_1_INPUT].getVoltage() * divisionScale;
			}
			if(divisionf > 0) {
				accentRotationf = clamp(accentRotationf,0.0f,divisionf-1);			
			} else {
				accentRotationf = 0;
			}

			if(stepsCountf > maxStepCount)
				maxStepCount = stepsCountf;
			if(trackNumber == masterTrack - 1)
				masterStepCount = stepsCountf;
			else
				masterStepCount = maxStepCount;
			


			stepsCount[trackNumber] = int(stepsCountf);

			int division = int(divisionf);
			int offset = int(offsetf);		
			int pad = int(padf);
			int accentDivision = int(accentDivisionf);
			int accentRotation = int(accentRotationf);


			if(stepsCount[trackNumber] > 0) {
				
                int bucket = stepsCount[trackNumber] - pad - 1;                    
                if(algorithnMatrix[trackNumber]) { //Euclidean Algorithn
                    int beatIndex = 0;
                    for(int stepIndex = 0; stepIndex < stepsCount[trackNumber]-pad; stepIndex++)
                    {
                        bucket += division;
                        if(bucket >= stepsCount[trackNumber]-pad) {
                            bucket -= (stepsCount[trackNumber] - pad);
                            beatMatrix[trackNumber][((stepIndex + offset + pad) % (stepsCount[trackNumber]))] = true;	
                            beatLocation[beatIndex] = (stepIndex + offset + pad) % stepsCount[trackNumber];	
                            beatIndex++;	
                        } else
                        {
                            beatMatrix[trackNumber][((stepIndex + offset + pad) % (stepsCount[trackNumber]))] = false;	
                        }
                        
                    }
                } else { //Golomb Ruler Algorithm
                    int rulerToUse = division - 1;
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

		if(inputs[RESET_INPUT].isConnected()) {
			if(resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
				int resetBeatIndex = 0;
				if(params[RESET_MODE_PARAM].getValue() == 1) {
					resetBeatIndex = -1;
				}
				for(int trackNumber=0;trackNumber<4;trackNumber++)
				{
					beatIndex[trackNumber] = resetBeatIndex;
				}
				setRunningState();
			}
		}

		if(inputs[MUTE_INPUT].isConnected()) {
			if(muteTrigger.process(inputs[MUTE_INPUT].getVoltage())) {
				muted = !muted;
			}
		}

		//See if need to start up
		for(int trackNumber=0;trackNumber < 4;trackNumber++) {
			if(chainMode != CHAIN_MODE_NONE && inputs[(trackNumber * 8) + 7].isConnected() && !running[trackNumber]) {
				if(startTrigger[trackNumber].process(inputs[START_1_INPUT + (trackNumber * 8)].getVoltage())) {
					running[trackNumber] = true;
				}
			}
		}

		//Get Expander Info
		bool expanderPresent = (rightExpander.module && rightExpander.module->model == modelQuadRhythmExpander);
		if(expanderPresent)
		{			
			float *message = (float*) rightExpander.module->leftExpander.consumerMessage;								
			bool useDivs = message[0] == 0; //0 is divs
			for(int i = 0; i < TRACK_COUNT; i++) {
				for(int j = 0; j < MAX_STEPS; j++) { //reset all probabilities
					probabilityMatrix[i][j] = 1;
					swingMatrix[i][j] = 0;
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
						float probability = message[1 + i * EXPANDER_MAX_STEPS + j];
						float swing = message[1 + (i + TRACK_COUNT) * EXPANDER_MAX_STEPS  + j];
						probabilityMatrix[i][stepIndex] = probability;
						swingMatrix[i][stepIndex] = swing;
					} 
				}
			}
		}

		//Calculate clock duration
		float timeAdvance =1.0 / args.sampleRate;
		time += timeAdvance;
		if(inputs[CLOCK_INPUT].isConnected()) {
			if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				if(secondClockReceived) {
					duration = time;
				}
				time = 0;
				secondClockReceived = true;							
			}

			bool resyncAll = false;
			
			for(int trackNumber=0;trackNumber < TRACK_COUNT;trackNumber++) {
				if(stepsCount[trackNumber] > 0 && constantTime)
					stepDuration[trackNumber] = duration * masterStepCount / (float)stepsCount[trackNumber]; //Constant Time scales duration based on a master track
				else
					stepDuration[trackNumber] = duration; //Otherwise Clock based

                //swing is affected by next beat
                int nextBeat = beatIndex[trackNumber] + 1;
                if(nextBeat >= stepsCount[trackNumber])
                    nextBeat = 0;
                float swingDuration = swingMatrix[trackNumber][nextBeat] * stepDuration[trackNumber];
        
            
				if(running[trackNumber]) {
					lastStepTime[trackNumber] +=timeAdvance;
					if(stepDuration[trackNumber] > 0.0 && lastStepTime[trackNumber] >= stepDuration[trackNumber] + swingDuration - lastSwingDuration[trackNumber]) {
//					if(stepDuration[trackNumber] > 0.0 && lastStepTime[trackNumber] >= stepDuration[trackNumber]) {
                        lastSwingDuration[trackNumber] = swingDuration;
						advanceBeat(trackNumber);
						if(constantTime && stepsCount[trackNumber] >= (int)maxStepCount && beatIndex[trackNumber] == 0) {
							resyncAll = true;
						}
					}					
				}

			}
			if(resyncAll) {
				for(int trackNumber=0;trackNumber < TRACK_COUNT;trackNumber++) {
                    if(beatIndex[trackNumber] > 0) {
                        //lastStepTime[trackNumber] = 0;
                        //lastSwingDuration[trackNumber] = 0;
                        beatIndex[trackNumber] = 0;
                    }
				}
			}
		}


		// Set output to current state
		for(int trackNumber=0;trackNumber<TRACK_COUNT;trackNumber++) {
            //Send Out Beat
            outputs[(trackNumber * 3) + OUTPUT_1].setVoltage(beatPulse[trackNumber].process(1.0 / args.sampleRate) ? 10.0 : 0);	

            //Send out Accent
            outputs[(trackNumber * 3) + ACCENT_OUTPUT_1].setVoltage(accentPulse[trackNumber].process(1.0 / args.sampleRate) ? 10.0 : 0);	

			//Send out End of Cycle
			outputs[(trackNumber * 3) + EOC_OUTPUT_1].setVoltage(eocPulse[trackNumber].process(1.0 / args.sampleRate) ? 10.0 : 0);	
		}
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
        for(int i=0;i<TRACK_COUNT;i++) {
            json_object_set_new(rootJ, "algorithm" + char(64+i), json_integer((bool) algorithnMatrix[i]));
        }
        
        json_object_set_new(rootJ, "constantTime", json_integer((bool) constantTime));
		json_object_set_new(rootJ, "masterTrack", json_integer((int) masterTrack));
		json_object_set_new(rootJ, "chainMode", json_integer((int) chainMode));
		json_object_set_new(rootJ, "muted", json_integer((bool) muted));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

        for(int i=0;i<TRACK_COUNT;i++) {
            json_t *ctAl = json_object_get(rootJ, "algorithm" + char(64+i));
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
			if(chainMode == CHAIN_MODE_EMPLOYEE && inputs[(trackNumber * 7) + 6].isConnected()) { //START Input needs to be active
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
			//If in a chain mode, stop running until start trigger received
			if(chainMode != CHAIN_MODE_NONE && inputs[(trackNumber * 8) + START_1_INPUT].isConnected()) { //START Input needs to be active
				running[trackNumber] = false;
			}
		}

        bool probabilityResult = (float) rand()/RAND_MAX < probabilityMatrix[trackNumber][beatIndex[trackNumber]];	
        

        //Create Beat Trigger    
        if(beatMatrix[trackNumber][beatIndex[trackNumber]] == true && probabilityResult && running[trackNumber] && !muted) {
            beatPulse[trackNumber].trigger(1e-3);
        }

        //Create Accent Trigger
        if(accentMatrix[trackNumber][beatIndex[trackNumber]] == true && probabilityResult && running[trackNumber] && !muted) {
            accentPulse[trackNumber].trigger(1e-3);
        }
	

	}
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    void onReset() override {
		for(int i = 0; i < TRACK_COUNT; i++) {
            algorithnMatrix[i] = true;
			beatIndex[i] = 0;
			stepsCount[i] = MAX_STEPS;
			lastStepTime[i] = 0.0;
			stepDuration[i] = 0.0;
            lastSwingDuration[i] = 0.0;
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

	void drawBox(const DrawArgs &args, float stepNumber, float trackNumber,bool isEuclidean, bool isBeat,bool isAccent,bool isCurrent, float probability, float swing) {
		
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


		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor fillColor = nvgRGBA(0xef,0xe0,0,opacity);
        if(!isEuclidean) {
            strokeColor = nvgRGBA(0, 0xe0, 0xef, 0xff);
			fillColor = nvgRGBA(0,0xe0,0xef,opacity);
        }
		if(isCurrent) {
			strokeColor = nvgRGBA(0x2f, 0xf0, 0, 0xff);
			fillColor = nvgRGBA(0x2f,0xf0,0,opacity);			
		}


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
		}



		//nvgStroke(args.vg);
		
	}

	void drawMasterTrack(const DrawArgs &args, Vec pos, int track) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", track);
		nvgText(args.vg, pos.x + 8, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		for(int trackNumber = 0;trackNumber < 4;trackNumber++) {
            bool isEuclidean = module->algorithnMatrix[trackNumber];
            for(int stepNumber = 0;stepNumber < module->stepsCount[trackNumber];stepNumber++) {				
                bool isBeat = module->beatMatrix[trackNumber][stepNumber];
				bool isAccent = module->accentMatrix[trackNumber][stepNumber];
				bool isCurrent = module->beatIndex[trackNumber] == stepNumber && module->running[trackNumber];		
				float probability = module->probabilityMatrix[trackNumber][stepNumber];
				float swing = module->swingMatrix[trackNumber][stepNumber];				
				drawBox(args, float(stepNumber), float(trackNumber),isEuclidean,isBeat,isAccent,isCurrent,probability,swing);
			}
		}

		if(module->constantTime)
			drawMasterTrack(args, Vec(box.size.x - 43, box.size.y - 80), module->masterTrack);
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
			display->box.size = Vec(box.size.x-15, 351);
			addChild(display);
		}


		addParam(createParam<LEDButton>(Vec(27, 140), module, QuadAlgorithmicRhythm::ALGORITHM_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 138), module, QuadAlgorithmicRhythm::STEPS_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(100, 138), module, QuadAlgorithmicRhythm::DIVISIONS_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(139, 138), module, QuadAlgorithmicRhythm::OFFSET_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(178, 138), module, QuadAlgorithmicRhythm::PAD_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(217, 138), module, QuadAlgorithmicRhythm::ACCENTS_1_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(256, 138), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_1_PARAM));

		addParam(createParam<LEDButton>(Vec(27, 197), module, QuadAlgorithmicRhythm::ALGORITHM_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 195), module, QuadAlgorithmicRhythm::STEPS_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(100, 195), module, QuadAlgorithmicRhythm::DIVISIONS_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(139, 195), module, QuadAlgorithmicRhythm::OFFSET_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(178, 195), module, QuadAlgorithmicRhythm::PAD_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(217, 195), module, QuadAlgorithmicRhythm::ACCENTS_2_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(256, 195), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_2_PARAM));

		addParam(createParam<LEDButton>(Vec(27, 254), module, QuadAlgorithmicRhythm::ALGORITHM_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 252), module, QuadAlgorithmicRhythm::STEPS_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(100, 252), module, QuadAlgorithmicRhythm::DIVISIONS_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(139, 252), module, QuadAlgorithmicRhythm::OFFSET_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(178, 252), module, QuadAlgorithmicRhythm::PAD_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(217, 252), module, QuadAlgorithmicRhythm::ACCENTS_3_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(256, 252), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_3_PARAM));

		addParam(createParam<LEDButton>(Vec(27, 311), module, QuadAlgorithmicRhythm::ALGORITHM_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(61, 309), module, QuadAlgorithmicRhythm::STEPS_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(100, 309), module, QuadAlgorithmicRhythm::DIVISIONS_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(139, 309), module, QuadAlgorithmicRhythm::OFFSET_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(178, 309), module, QuadAlgorithmicRhythm::PAD_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(217, 309), module, QuadAlgorithmicRhythm::ACCENTS_4_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(256, 309), module, QuadAlgorithmicRhythm::ACCENT_ROTATE_4_PARAM));

		addParam(createParam<CKD6>(Vec(291, 283), module, QuadAlgorithmicRhythm::CHAIN_MODE_PARAM));
		addParam(createParam<CKD6>(Vec(375, 283), module, QuadAlgorithmicRhythm::CONSTANT_TIME_MODE_PARAM));
		

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

		addInput(createInput<PJ301MPort>(Vec(302, 343), module, QuadAlgorithmicRhythm::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(347, 343), module, QuadAlgorithmicRhythm::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(385, 343), module, QuadAlgorithmicRhythm::MUTE_INPUT));

		addInput(createInput<PJ301MPort>(Vec(367, 145), module, QuadAlgorithmicRhythm::START_1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(367, 175), module, QuadAlgorithmicRhythm::START_2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(367, 205), module, QuadAlgorithmicRhythm::START_3_INPUT));
		addInput(createInput<PJ301MPort>(Vec(367, 235), module, QuadAlgorithmicRhythm::START_4_INPUT));


		addOutput(createOutput<PJ301MPort>(Vec(303, 145), module, QuadAlgorithmicRhythm::OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(335, 145), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(399, 145), module, QuadAlgorithmicRhythm::EOC_OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(303, 175), module, QuadAlgorithmicRhythm::OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(335, 175), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(399, 175), module, QuadAlgorithmicRhythm::EOC_OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(303, 205), module, QuadAlgorithmicRhythm::OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(335, 205), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(399, 205), module, QuadAlgorithmicRhythm::EOC_OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(303, 235), module, QuadAlgorithmicRhythm::OUTPUT_4));
		addOutput(createOutput<PJ301MPort>(Vec(335, 235), module, QuadAlgorithmicRhythm::ACCENT_OUTPUT_4));
		addOutput(createOutput<PJ301MPort>(Vec(399, 235), module, QuadAlgorithmicRhythm::EOC_OUTPUT_4));

        addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(31, 144), module, QuadAlgorithmicRhythm::ALGORITHM_1_LIGHT));
        addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(31, 201), module, QuadAlgorithmicRhythm::ALGORITHM_2_LIGHT));
        addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(31, 258), module, QuadAlgorithmicRhythm::ALGORITHM_3_LIGHT));
        addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(31, 315), module, QuadAlgorithmicRhythm::ALGORITHM_4_LIGHT));
		
		addChild(createLight<SmallLight<BlueLight>>(Vec(322, 285), module, QuadAlgorithmicRhythm::CHAIN_MODE_NONE_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(Vec(322, 300), module, QuadAlgorithmicRhythm::CHAIN_MODE_BOSS_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(322, 315), module, QuadAlgorithmicRhythm::CHAIN_MODE_EMPLOYEE_LIGHT));

		addChild(createLight<LargeLight<RedLight>>(Vec(413, 347), module, QuadAlgorithmicRhythm::MUTED_LIGHT));
		
	}
};

Model *modelQuadAlgorithmicRhythm = createModel<QuadAlgorithmicRhythm, QuadAlgorithmicRhythmWidget>("QuadAlgorithmicRhythm");
