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
#define MAX_OCTAVES 11
#define MAX_NOTES 12
#define MAX_SCALES 42
#define MAX_TEMPERMENTS 3
#define NUM_SHIFT_MODES 3
#define TRIGGER_DELAY_SAMPLES 5

#define EXPANDER_MOTHER_SEND_MESSAGE_COUNT 3 + 2 //3 chords plus curent note and octave
#define EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT 7 + 11*12


using namespace frozenwasteland::dsp;

struct ProbablyNote : Module {
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
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,
        RESET_SCALE_PARAM,
        OCTAVE_WRAPAROUND_PARAM,
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
        OCTAVE_INPUT,
		TEMPERMENT_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
		OCTAVE_WRAP_INPUT,		
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
        OCTAVE_WRAPAROUND_LIGHT,
		INTONATION_LIGHT,
		SHIFT_MODE_LIGHT = INTONATION_LIGHT+3,
        KEY_LOGARITHMIC_SCALE_LIGHT = SHIFT_MODE_LIGHT + 3,
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

	

	// Expander
	//float leftMessages[2][EXPANDER_MOTHER_SEND_MESSAGE_COUNT + EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT] = {};
	float rightMessages[2][EXPANDER_MOTHER_SEND_MESSAGE_COUNT + EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT] = {};



	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
	const char* scaleNames[MAX_SCALES] = {"Chromatic","Whole Tone","Ionian M","Dorian","Phrygian","Lydian","Mixolydian","Aeolian m","Locrian","Acoustic", "Altered", "Augmented", "Bebop Dominant", "Blues",  
											"Enigmatic","Flamenco","Gypsy","Half diminished","Harmonic Major","Harmonic Minor","Hirajoshi",
											"Hungarian","Miyako-bushi", "Insen", "Iwato", "Lydian Augmented",  "Bebob Major", "Locrian Major",
											"Pentatonic Major","Melodic Minor","Pentatonic Minor - Yo","Neapoliltan Major", "Neapolitan Minor", "Octatonic 1",  "Octatonic 2",
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

	float scaleNoteWeighting[MAX_SCALES][MAX_NOTES]; 
	bool scaleNoteStatus[MAX_SCALES][MAX_NOTES];

	const char* tempermentNames[MAX_TEMPERMENTS] = {"Equal","Just","Pythagorean"};
    double noteTemperment[MAX_TEMPERMENTS][MAX_NOTES] = {
        {0,100,200,300,400,500,600,700,800,900,1000,1100},
        {0,111.73,203.91,315.64,386.61,498.04,582.51,701.955,813.69,884.36,1017.596,1088.27},
        {0,90.22,203.91,294.13,407.82,498.04,611.73,701.955,792.18,905.87,996.09,1109.78},
    };


    // const char *scaleKeys[SCALE_KEYS]  = { 
    //     "2221212",      "1212222",      "313131" ,      "22122111",     "321132",       
    //     "1322211",      "1312131",      "2131122",      "2121222",      "2212131",      "2122131",      "42141",
    //     "2131131",      "14214",        "14242",        "14142",        "2222121",      "22121211",     "2211222",
    //     "22323",        "2122221",      "32232",        "1222221",      "1222131",      "21212121",     "12121212",
    //     "1311231",      "1312122",      "222312",       "311223",       "132132",       "114114",       "2131212",
         
    // };
	// const char *scaleNames[SCALE_KEYS] = {
    //     "Acoustic",     "Altered",      "Augmented",    "Bebop dom.",   "Blues",        
    //     "Enigmatic",    "Flamenco",     "Gypsy",        "Half diminished",    "harmonic Major",  "harmonic Minor",  "Hirajoshi",
    //     "Hungarian",    "Miyako-bushi", "Insen",        "Iwato",        "Lydian augmented",  "Bebob Major",   "Locrian Major",
    //     "Pentatonic Major",  "melodic Minor",   "Pentatonic Minor -  Yo","Neapoliltan Major", "Neapolitan Minor", "Octatonic 1",  "Octatonic 2",
    //     "Persian",      "Phrygian dominant",  "Prometheus",   "Harmonics",    "Tritone",      "Tritone 2S",   "Ukrainian Dorian",
        
    // };


	
	dsp::SchmittTrigger clockTrigger[POLYPHONY],clockModeTrigger,resetScaleTrigger,octaveWrapAroundTrigger,
						tempermentActiveTrigger,tempermentTrigger,shiftScalingTrigger,keyScalingTrigger,
						pitchRandomnessGaussianTrigger,noteActiveTrigger[MAX_NOTES]; 
	dsp::PulseGenerator noteChangePulse[POLYPHONY];
    GaussianNoiseGenerator _gauss;
 
    bool octaveWrapAround = false;
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
    int octave = 0;
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
	bool resetTriggered = false;
	int lastNote[POLYPHONY] = {-1}; 
	bool noteChange = false; // True if any of the input notes has changed
	int lastSpread = -1;
	float lastSlant = -1;
	float lastFocus = -1;
	bool alternateIntonationActive = false;
	int currentIntonation = 0;
	int shiftMode = 0;
	bool keyLogarithmic = false;
	bool pitchRandomGaussian = false;

	bool useCircleLayout = false;
	int quantizeMode = QUANTIZE_CLOSEST;

	bool generateChords = false;
	float dissonance5Prbability = 0.0;
	float dissonance7Prbability = 0.0;
	float suspensionProbability = 0.0;
	float inversionProbability = 0.0;
	float externalDissonance5Random = 0.0;
	float externalDissonance7Random = 0.0;
	float externalSuspensionRandom = 0.0;

	int fifthOffset,seventhOffset,thirdOffset;
	int currentPolyphony = 1;
    
	float octaveProbability[MAX_NOTES][MAX_OCTAVES] = {{0.0}};
	float totalAverage[MAX_NOTES] = {0};
	int currentRandomOctave = 0;


	//percentages
	float spreadPercentage = 0;
	float slantPercentage = 0;
	float distributionPercentage = 0;
	float nonRepeatPercentage = 0;
	float scalePercentage = 0;
	float keyPercentage = 0;
	float shiftPercentage = 0;
	float octavePercentage = 0;
	float pitchRandomnessPercentage = 0;
	float weightPercentage[MAX_NOTES] = {0};

	ProbablyNote() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNote::SPREAD_PARAM, 0.0, 6.0, 0.0,"Spread"," Notes");
        configParam(ProbablyNote::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNote::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant","%",0,100);
        configParam(ProbablyNote::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNote::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Focus","%",0,100);
		configParam(ProbablyNote::DISTRIBUTION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Focus CV Attenuation");
		configParam(ProbablyNote::SHIFT_PARAM, -11.0, 11.0, 0.0,"Weight Shift");
        configParam(ProbablyNote::SHIFT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Weight Shift CV Attenuation" ,"%",0,100);
		configParam(ProbablyNote::SCALE_PARAM, 0.0, MAX_SCALES-1, 0.0,"Scale");
        configParam(ProbablyNote::SCALE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Scale CV Attenuation","%",0,100);
		configParam(ProbablyNote::KEY_PARAM, 0.0, 11.0, 0.0,"Key");
        configParam(ProbablyNote::KEY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Key CV Attenuation","%",0,100); 
		configParam(ProbablyNote::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
        configParam(ProbablyNote::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNote::WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Weight Scaling","%",0,100);
		configParam(ProbablyNote::PITCH_RANDOMNESS_PARAM, 0.0, 10.0, 0.0,"Randomize Pitch Amount"," Cents");
        configParam(ProbablyNote::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Randomize Pitch Amount CV Attenuation","%",0,100);
		configParam(ProbablyNote::NON_REPEATABILITY_PARAM, 0.0, 1.0, 0.0,"Note Non-Repeat Probability"," %",0,100);
        configParam(ProbablyNote::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Note Non-Repeat Probability CV Attenuation","%",0,100);

		configButton(OCTAVE_WRAPAROUND_PARAM,"Octave Wraparound");
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
		configInput(OCTAVE_INPUT, "Octave");
		configInput(TEMPERMENT_INPUT, "Tempermanet");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(EXTERNAL_RANDOM_INPUT, "External Random");
		configInput(OCTAVE_WRAP_INPUT, "Octave Wrap");
		configInput(PITCH_RANDOMNESS_INPUT, "Pitch Randomness");
		configInput(NOTE_WEIGHT_INPUT, "Left");
		configInput(NON_REPEATABILITY_INPUT, "Note Non-Repeat Probability");

		configOutput(QUANT_OUTPUT, "Quantized CV");
		configOutput(WEIGHT_OUTPUT, "Note Weight");
		configOutput(NOTE_CHANGE_OUTPUT, "Note Changed");


		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		onReset();
	}

    double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,int count, float scaling,float randomIn) {
        double weightTotal = 0.0f;
		double linearWeight, logWeight, weight;
            
        for(int i = 0;i <count; i++) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

        int chosenWeight = -1;        
        double rnd = randomIn * weightTotal;
        for(int i = 0;i <count;i++ ) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);

            if(rnd < weight) {
                chosenWeight = i;
                break;
            }
            rnd -= weight;
        }
        return chosenWeight;
    }

	double quantizedCVValue(int note, int key, int intonationType) {
		if(intonationType == 0) {
			return (note / 12.0); 
		} else {
			int octaveAdjust = 0;
			int notePosition = note - key;
			if(notePosition < 0) {
				notePosition += MAX_NOTES;
				octaveAdjust -=1;
			}
			return (noteTemperment[intonationType][notePosition] / 1200.0) + (key /12.0) + octaveAdjust; 
		}
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

		json_object_set_new(rootJ, "triggerDelayEnabled", json_boolean(triggerDelayEnabled));
		json_object_set_new(rootJ, "octaveWrapAround", json_boolean(octaveWrapAround));
		json_object_set_new(rootJ, "alternateIntonationActive", json_boolean(alternateIntonationActive));
		json_object_set_new(rootJ, "currentIntonation", json_integer((int) currentIntonation));
		json_object_set_new(rootJ, "shiftMode", json_integer((int) shiftMode));
		json_object_set_new(rootJ, "keyLogarithmic", json_boolean(keyLogarithmic));
		json_object_set_new(rootJ, "useCircleLayout", json_boolean(useCircleLayout));
		json_object_set_new(rootJ, "pitchRandomGaussian", json_boolean(pitchRandomGaussian)); 
		json_object_set_new(rootJ, "triggerPolyphonic", json_boolean(triggerPolyphonic)); 
		json_object_set_new(rootJ, "quantizeMode", json_integer((int) quantizeMode)); 

		
		

		for(int i=0;i<MAX_SCALES;i++) {
			for(int j=0;j<MAX_NOTES;j++) {
				char buf[140];
				char notebuf[100];
				strcpy(buf, "scaleWeight-");
				strcat(buf, scaleNames[i]);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				json_object_set_new(rootJ, buf, json_real((float) scaleNoteWeighting[i][j]));

				char buf2[140];
				char notebuf2[100];
				strcpy(buf2, "scaleStatus-");
				strcat(buf2, scaleNames[i]);
				strcat(buf2, ".");
				sprintf(notebuf2, "%i", j);
				strcat(buf2, notebuf2);
				json_object_set_new(rootJ, buf2, json_boolean(scaleNoteStatus[i][j]));
			}
		}
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *ctTd = json_object_get(rootJ, "triggerDelayEnabled");
		if (ctTd)
			triggerDelayEnabled = json_boolean_value(ctTd);


		json_t *sumO = json_object_get(rootJ, "octaveWrapAround");
		if (sumO) {
			octaveWrapAround = json_boolean_value(sumO);			
		}

		json_t *sumTa = json_object_get(rootJ, "alternateIntonationActive");
		if (sumTa) {
			alternateIntonationActive = json_boolean_value(sumTa);			
		}

		json_t *sumT = json_object_get(rootJ, "currentIntonation");
		if (sumT) {
			currentIntonation = json_integer_value(sumT);			
		}

		json_t *sumL = json_object_get(rootJ, "shiftMode");
		if (sumL) {
			shiftMode = json_integer_value(sumL);			
		}


		json_t *sumK = json_object_get(rootJ, "keyLogarithmic");
		if (sumK) {
			keyLogarithmic = json_boolean_value(sumK);			
		}

		json_t *sumPg = json_object_get(rootJ, "pitchRandomGaussian");
		if (sumPg) {
			pitchRandomGaussian = json_boolean_value(sumPg);			
		}

		json_t *sumCl = json_object_get(rootJ, "useCircleLayout");
		if (sumCl) {
			useCircleLayout = json_boolean_value(sumCl);			
		}

		json_t *sumTp = json_object_get(rootJ, "triggerPolyphonic");
		if (sumTp) {
			triggerPolyphonic = json_boolean_value(sumTp);			
		}

		json_t *sumQm = json_object_get(rootJ, "quantizeMode");
		if (sumQm) {
			quantizeMode = json_integer_value(sumQm);			
		}


		

		for(int i=0;i<MAX_SCALES;i++) {
			for(int j=0;j<MAX_NOTES;j++) {
				char buf[140];
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

				char buf2[140];
				char notebuf2[100];
				strcpy(buf2, "scaleStatus-");
				strcat(buf2, scaleNames[i]);
				strcat(buf2, ".");
				sprintf(notebuf2, "%i", j);
				strcat(buf2, notebuf2);
				json_t *sumJ2 = json_object_get(rootJ, buf2);
				if (sumJ2) {
					scaleNoteStatus[i][j] = json_boolean_value(sumJ2);
				}
			}
		}		

		resetTriggered = true;
	}
	

	void process(const ProcessArgs &args) override {

		//Get Expander Info
		if(rightExpander.module && (rightExpander.module->model == modelPNChordExpander || rightExpander.module->model == modelPNOctaveProbabilityExpander)) {	
			
			
			generateChords = true;		
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;
			
			dissonance5Prbability = messagesFromExpander[0];
			dissonance7Prbability = messagesFromExpander[1];
			suspensionProbability = messagesFromExpander[2];
			externalDissonance5Random = messagesFromExpander[4];
			externalDissonance7Random = messagesFromExpander[5];
			externalSuspensionRandom = messagesFromExpander[6];

			//Send outputs to slaves if present		
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			
			messageToExpander[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 0] = thirdOffset; 
			messageToExpander[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 1] = fifthOffset; 
			messageToExpander[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 2] = seventhOffset;


			for(int i = 0;i < MAX_NOTES;i++) {
				totalAverage[i] = 0.0;
				for (int j = 0;j<MAX_OCTAVES;j++) {
					octaveProbability[i][j] = messagesFromExpander[7 + i*MAX_OCTAVES + j];
					totalAverage[i] += octaveProbability[i][j];
				}
			}
			
			//Add current note and octave
			messageToExpander[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 3] = probabilityNote[0]; 
			messageToExpander[EXPANDER_MOTHER_RECEIVE_MESSAGE_COUNT + 4] = currentRandomOctave; 

			rightExpander.module->leftExpander.messageFlipRequested = true;			
		} else {
			generateChords = false;
		}
	

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

		if (tempermentActiveTrigger.process(inputs[TEMPERMENT_INPUT].getVoltage())) {
			alternateIntonationActive = !alternateIntonationActive;
		}		
		if (tempermentTrigger.process(params[TEMPERMENT_PARAM].getValue())) {
			currentIntonation = (currentIntonation + 1) % MAX_TEMPERMENTS;
			alternateIntonationActive = currentIntonation > 0;
		}		
		if(alternateIntonationActive) {
			lights[INTONATION_LIGHT + 1].value = currentIntonation == 1;
			lights[INTONATION_LIGHT + 2].value = currentIntonation == 2;
		} else {
			lights[INTONATION_LIGHT].value = 0;
			lights[INTONATION_LIGHT+1].value = 0;
			lights[INTONATION_LIGHT+2].value = 0;
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


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,6.0f);
		spreadPercentage = spread / 6.0f;

        slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
		slantPercentage = slant;

        focus = clamp(params[DISTRIBUTION_PARAM].getValue() + (inputs[DISTRIBUTION_INPUT].getVoltage() / 10.0f * params[DISTRIBUTION_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		distributionPercentage = focus;

        nonRepeat = clamp(params[NON_REPEATABILITY_PARAM].getValue() + (inputs[NON_REPEATABILITY_INPUT].getVoltage() / 10.0f * params[NON_REPEATABILITY_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		nonRepeatPercentage = nonRepeat;

        scale = clamp(params[SCALE_PARAM].getValue() + (inputs[SCALE_INPUT].getVoltage() * MAX_SCALES / 10.0 * params[SCALE_CV_ATTENUVERTER_PARAM].getValue()),0.0,MAX_SCALES-1.0f);
		scalePercentage = scale / (MAX_SCALES - 1.0f);
        
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
		
        octave = clamp(params[OCTAVE_PARAM].getValue() + (inputs[OCTAVE_INPUT].getVoltage() * 0.4 * params[OCTAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);
		octavePercentage = (octave + 4.0f) / 8.0f;

		currentPolyphony = inputs[NOTE_INPUT].getChannels();

		outputs[NOTE_CHANGE_OUTPUT].setChannels(currentPolyphony);
		outputs[WEIGHT_OUTPUT].setChannels(currentPolyphony);

		noteChange = false;
		double octaveIn[POLYPHONY];
		for(int channel = 0;channel<currentPolyphony;channel++) {
			double noteIn = inputs[NOTE_INPUT].getVoltage(channel);
			octaveIn[channel] = std::floor(noteIn);
			double fractionalValue = std::abs(noteIn - octaveIn[channel]);
			double lastDif = 99.0f;    
			for(int i = 0;i<MAX_NOTES;i++) {
				double lowNote = (i / 12.0);
				double highNote = ((i+1) / 12.0);
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
				noteCount +=1;		
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
		weightShift = clamp(weightShift,-11,11);
		shiftPercentage = weightShift / 11.0f;

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
				params[NOTE_WEIGHT_PARAM + (useCircleLayout ? i : noteValue)].setValue(shiftWeights[noteValue]);
			}	

			lastKey = key;
			lastScale = scale;
			lastWeightShift = weightShift;
			resetTriggered = false;
		}


		for(int i=0;i<MAX_NOTES;i++) {
			int controlOffset = (i + key) % MAX_NOTES;
			int actualTarget = useCircleLayout ?  controlOffset : i;
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
				weightPercentage[i] = 0.0;
				lights[NOTE_ACTIVE_LIGHT+i*2].value = 0;    
				lights[NOTE_ACTIVE_LIGHT+i*2+1].value = 1;    	
			}

			for(int channel = 0; channel < currentPolyphony; channel++) {
				actualProbability[channel][actualTarget] = noteInitialProbability[channel][actualTarget] * userProbability; 
			}

			if(useCircleLayout) {
				int scalePosition = controlIndex[controlOffset] - key;
				if (scalePosition < 0)
					scalePosition += MAX_NOTES;
				scaleNoteWeighting[scale][i] = params[NOTE_WEIGHT_PARAM+scalePosition].getValue(); 
				scaleNoteStatus[scale][i] =noteActive[controlOffset];
			} else {
				scaleNoteWeighting[scale][i] = params[NOTE_WEIGHT_PARAM+controlIndex[controlOffset]].getValue(); 
				scaleNoteStatus[scale][i] = noteActive[controlOffset];
			}									
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
					if (spread > 0 && nonRepeat > 0.0 && repeatProbability < nonRepeat && lastRandomNote[channel] >= 0 ) {
						actualProbability[channel][lastRandomNote[channel]] = 0; //Last note has no chance of repeating 						
					}
			
					int randomNote = weightedProbability(actualProbability[channel],MAX_NOTES,params[WEIGHT_SCALING_PARAM].getValue(), rnd);
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
						// fprintf(stderr, "newnote %i probabillty:%f value:%f lastNote:%i rNote:%i\n",newNote,nonRepeat,repeatProbability,lastRandomNote[channel],randomNote);
					lastRandomNote[channel] = randomNote; // for repeatability
					probabilityNote[channel] = randomNote; // for triggers

					float octaveAdjust = 0.0;
					if(!octaveWrapAround) {
						if(randomNote > currentNote[channel] && randomNote - currentNote[channel] > upperSpread)
							octaveAdjust = -1.0;
						if(randomNote < currentNote[channel] && currentNote[channel] - randomNote > lowerSpread)
							octaveAdjust = 1.0;
					}

					if(totalAverage[probabilityNote[channel]] > 0 ) {
						float rndO = ((float) rand()/RAND_MAX);
						int randomOctave = weightedProbability(octaveProbability[probabilityNote[channel]],MAX_OCTAVES, params[WEIGHT_SCALING_PARAM].getValue(), rndO);
						octaveAdjust += (5-randomOctave);
						// fprintf(stderr, "random Octave:%i  total Average:%f\n",randomOctave,totalAverage[probabilityNote[channel]]);
						if(channel == 0) {
							currentRandomOctave = randomOctave; 
						}
					}

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


					double quantitizedNoteCV = quantizedCVValue(randomNote,key,alternateIntonationActive ? currentIntonation : 0);
					quantitizedNoteCV += octaveIn[channel] + octave + octaveAdjust;
					
					
					//Chord Stuff
					if(!generateChords || currentPolyphony > 1) { 
						outputs[QUANT_OUTPUT].setChannels(currentPolyphony);
						outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV + pitchRandomness,channel);
					} else {
						outputs[QUANT_OUTPUT].setChannels(4);
						outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV + pitchRandomness,0);

						float rndDissonance5 = ((float) rand()/RAND_MAX);
						if(externalDissonance5Random != -1)
							rndDissonance5 = externalDissonance5Random;

						float rndDissonance7 = ((float) rand()/RAND_MAX);
						if(externalDissonance7Random != -1)
							rndDissonance7 = externalDissonance7Random;

						float rndSuspension = ((float) rand()/RAND_MAX);
						if(externalSuspensionRandom != -1)
							rndSuspension = externalSuspensionRandom;
						//float rndInversion = ((float) rand()/RAND_MAX);

						int secondNote = nextActiveNote(randomNote,2);					
						if(rndSuspension < suspensionProbability) {
							float secondOrFourth = ((float) rand()/RAND_MAX);
							if(secondOrFourth > 0.5) {
								thirdOffset = 1;
								secondNote = nextActiveNote(randomNote,3);
							} else {
								thirdOffset =-1;
								secondNote = nextActiveNote(randomNote,1);
							}					
						} else {
							thirdOffset = 0;
						}
						int secondNoteOctave = 0;
						if(secondNote < randomNote) {
							secondNoteOctave +=1;
						}

						int thirdNote = nextActiveNote(randomNote,4);
						if(rndDissonance5 < dissonance5Prbability) {
							float flatOrSharp = ((float) rand()/RAND_MAX);
							fifthOffset = -1;
							if(flatOrSharp > 0.5) {
								fifthOffset = 1;
							}
							thirdNote = (thirdNote + fifthOffset) % MAX_NOTES;
							if(thirdNote < 0)
								thirdNote += MAX_NOTES;
						} else {
							fifthOffset = 0;
						}
						int thirdNoteOctave = 0;
						if(thirdNote < randomNote) {
							thirdNoteOctave +=1;
						}

						int fourthNote = nextActiveNote(randomNote,6);
						if(rndDissonance7 < dissonance7Prbability) {
							float flatOrSharp = ((float) rand()/RAND_MAX);
							seventhOffset = -1;
							if(flatOrSharp > 0.5) {
								seventhOffset = 1;
							}
							fourthNote = (fourthNote + seventhOffset) % MAX_NOTES;
							if(fourthNote < 0)
								fourthNote += MAX_NOTES;
						} else {
							seventhOffset = 0;
						}
						int fourthNoteOctave = 0;
						if(fourthNote < randomNote) {
							fourthNoteOctave +=1;
						}
				
						outputs[QUANT_OUTPUT].setVoltage((double)secondNote/12.0 + octaveIn[0] + octave + octaveAdjust + secondNoteOctave,1);
						outputs[QUANT_OUTPUT].setVoltage((double)thirdNote/12.0 + octaveIn[0] + octave + octaveAdjust + thirdNoteOctave,2);
						outputs[QUANT_OUTPUT].setVoltage((double)fourthNote/12.0 + octaveIn[0] + octave + octaveAdjust + fourthNoteOctave,3);
					}

					outputs[WEIGHT_OUTPUT].setVoltage(clamp((params[NOTE_WEIGHT_PARAM+randomNote].getValue() + (inputs[NOTE_WEIGHT_INPUT+randomNote].getVoltage() / 10.0f) * 10.0f),0.0f,10.0f),channel);
					if(lastQuantizedCV[channel] != quantitizedNoteCV) {
						noteChangePulse[channel].trigger();	
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

void ProbablyNote::onReset() {
	for(int i=0;i<POLYPHONY;i++) {
		clockTrigger[i].reset();
		for(int j=0;j<TRIGGER_DELAY_SAMPLES;j++) {
			triggerDelay[i][j] = 0.0f;
		}
	}
	resetTriggered = true;
	triggerDelayEnabled = false;

			// fprintf(stderr, "Resetting \n");

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





struct ProbablyNoteWidget : ModuleWidget {
	SvgPanel* circlePanel;

	PortWidget* inputs[MAX_NOTES];
	ParamWidget* weightParams[MAX_NOTES];
	ParamWidget* noteOnParams[MAX_NOTES];
	LightWidget* lights[MAX_NOTES];


	struct ProbablyNoteDisplay : TransparentWidget {
	ProbablyNote *module;
	int frame = 0;
	std::shared_ptr<Font> font;
	std::string fontPath;

	ProbablyNoteDisplay() {
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
		nvgTextLetterSpacing(args.vg, -1.5);

		if(shifted) 
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x00, 0xff));
		else
			nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->scaleNames[scale]);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawOctave(const DrawArgs &args, Vec pos, int octave) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", octave);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	

	void drawNoteRangeNormal(const DrawArgs &args, float *noteInitialProbability) 
	{		
		// Draw indicator
		for(int i = 0; i<MAX_NOTES;i++) {
			const float rotate90 = (M_PI) / 2.0;

			float opacity = noteInitialProbability[i] * 255;
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			if(i == module->probabilityNote[0]) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			}
			switch(i) {
				case 0:
				//C
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,330);
				nvgLineTo(args.vg,167, 330);
				nvgLineTo(args.vg,167, 302);
				nvgLineTo(args.vg,87, 302);
				nvgArc(args.vg,75.5,302,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,75.5, 314);		
				nvgLineTo(args.vg,25, 314);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 1:
				//C#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,314.5);
				nvgLineTo(args.vg,75.5,314.5);
				nvgArc(args.vg,75.5,302,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,25,289.5);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 2:
				//D
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,87,302);
				nvgLineTo(args.vg,167,302);
				nvgLineTo(args.vg,167,274);		
				nvgLineTo(args.vg,87,274);
				nvgArc(args.vg,75.5,274,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,25,286.5);
				nvgLineTo(args.vg,25,289.5);
				nvgArc(args.vg,75.5,302,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 3:
				//D#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,286.5);
				nvgLineTo(args.vg,75.5,286.5);
				nvgArc(args.vg,75.5,274,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,25,261.5);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 4:
				//E
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,87,274);
				nvgLineTo(args.vg,167,274);
				nvgLineTo(args.vg,167,246);		
				nvgLineTo(args.vg,25,246);
				nvgLineTo(args.vg,25,261.5);		
				nvgLineTo(args.vg,75.5, 261.5);
				nvgArc(args.vg,75.5,274,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 5:
				//F 
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,246);
				nvgLineTo(args.vg,167, 246);
				nvgLineTo(args.vg,167, 218);
				nvgLineTo(args.vg,87, 218);
				nvgArc(args.vg,75.5,218,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,75.5, 230.5);		
				nvgLineTo(args.vg,25, 230.5);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 6:
				//F#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,230.5);
				nvgLineTo(args.vg,75.5,230.5);
				nvgArc(args.vg,75.5,218,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				//nvgLineTo(args.vg,62,206);		
				nvgLineTo(args.vg,25,205.5);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 7:
				//G
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,87,218);
				nvgLineTo(args.vg,167,218);
				nvgLineTo(args.vg,167,190);		
				nvgLineTo(args.vg,87,190);
				nvgArc(args.vg,75.5,190,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,25,202.5);
				nvgLineTo(args.vg,25,205.5);
				nvgArc(args.vg,75.5,218,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 8:
				//G#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,202.5);
				nvgLineTo(args.vg,75.5,202.5);
				nvgArc(args.vg,75.5,190,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,25,178);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 9:
				//A
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,87,190);
				nvgLineTo(args.vg,167,190);
				nvgLineTo(args.vg,167,162);		
				nvgLineTo(args.vg,87,162);
				nvgArc(args.vg,75.5,162,12.5,0,M_PI-rotate90,NVG_CW);
				nvgLineTo(args.vg,25,174.5);
				nvgLineTo(args.vg,25,177.5);
				nvgArc(args.vg,75.5,190,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 10:
				//A#
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,25,174.5);
				nvgLineTo(args.vg,75.5,174.5);
				nvgArc(args.vg,75.5,162,12.5,M_PI-rotate90,0-rotate90,NVG_CCW);
				nvgLineTo(args.vg,25,150);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

				case 11:
				//B
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,87,162);
				nvgLineTo(args.vg,167,162);
				nvgLineTo(args.vg,167,134);		
				nvgLineTo(args.vg,25,134);
				nvgLineTo(args.vg,25,149.5);		
				nvgLineTo(args.vg,75.5,149.5);
				nvgArc(args.vg,75.5,162,12.5,0-rotate90,0,NVG_CW);
				nvgClosePath(args.vg);		
				nvgFill(args.vg);
				break;

			}
		}
	}

	void drawNoteRangeCircular(const DrawArgs &args, float *noteInitialProbability, int key) 
	{		
		float notePosition[MAX_NOTES][3] = {
			{103,163,NVG_ALIGN_LEFT},
			{138,185,NVG_ALIGN_LEFT},
			{168,212,NVG_ALIGN_CENTER},
			{175,250,NVG_ALIGN_CENTER},
			{161,288,NVG_ALIGN_CENTER},
			{134,310,NVG_ALIGN_RIGHT},
			{97,317,NVG_ALIGN_RIGHT},
			{60,302,NVG_ALIGN_RIGHT},
			{32,272,NVG_ALIGN_CENTER}, 
			{25,236,NVG_ALIGN_CENTER},
			{30,198,NVG_ALIGN_LEFT},
			{66,172,NVG_ALIGN_LEFT}
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
			// nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
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

		drawScale(args, Vec(10,82), module->lastScale, module->lastWeightShift != 0);
		drawKey(args, Vec(74,82), module->lastKey, module->transposedKey);
		//drawOctave(args, Vec(66, 280), module->octave);
		if(module->useCircleLayout) {
			drawNoteRangeCircular(args, module->noteInitialProbability[0], module->key);
		} else {
			drawNoteRangeNormal(args, module->noteInitialProbability[0]);
		}
	}
};

	
	ProbablyNoteWidget(ProbablyNote *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNote.svg")));
        if (module) {
			circlePanel = new SvgPanel();
			circlePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNote-alt.svg")));
			circlePanel->visible = false;
			addChild(circlePanel);
		}

		{
			ProbablyNoteDisplay *display = new ProbablyNoteDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		addInput(createInput<FWPortInSmall>(Vec(8, 345), module, ProbablyNote::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(43, 345), module, ProbablyNote::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(90, 345), module, ProbablyNote::EXTERNAL_RANDOM_INPUT));

		ParamWidget* spreadParam = createParam<RoundSmallFWSnapKnob>(Vec(10,25), module, ProbablyNote::SPREAD_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(spreadParam)->percentage = &module->spreadPercentage;
		}
		addParam(spreadParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(36,51), module, ProbablyNote::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(38, 29), module, ProbablyNote::SPREAD_INPUT));

		ParamWidget* slantParam = createParam<RoundSmallFWKnob>(Vec(67,25), module, ProbablyNote::SLANT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->percentage = &module->slantPercentage;
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->biDirectional = true;
		}
		addParam(slantParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(93,51), module, ProbablyNote::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(95, 29), module, ProbablyNote::SLANT_INPUT));

		ParamWidget* distributionParam = createParam<RoundSmallFWKnob>(Vec(124, 25), module, ProbablyNote::DISTRIBUTION_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(distributionParam)->percentage = &module->distributionPercentage;
		}
		addParam(distributionParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(150,51), module, ProbablyNote::DISTRIBUTION_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(152, 29), module, ProbablyNote::DISTRIBUTION_INPUT));

		ParamWidget* nonRepeatParam = createParam<RoundSmallFWKnob>(Vec(181, 25), module, ProbablyNote::NON_REPEATABILITY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(nonRepeatParam)->percentage = &module->nonRepeatPercentage;
		}
		addParam(nonRepeatParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(207,51), module, ProbablyNote::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(209, 29), module, ProbablyNote::NON_REPEATABILITY_INPUT));

		ParamWidget* scaleParam = createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNote::SCALE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(scaleParam)->percentage = &module->scalePercentage;
		}
		addParam(scaleParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNote::SCALE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNote::SCALE_INPUT));

		addParam(createParam<TL1105>(Vec(15, 115), module, ProbablyNote::RESET_SCALE_PARAM));

		ParamWidget* keyParam = createParam<RoundSmallFWSnapKnob>(Vec(68,86), module, ProbablyNote::KEY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(keyParam)->percentage = &module->keyPercentage;
		}
		addParam(keyParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94,112), module, ProbablyNote::KEY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(96, 90), module, ProbablyNote::KEY_INPUT));

		addParam(createParam<LEDButton>(Vec(70, 113), module, ProbablyNote::KEY_SCALING_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(71.5, 114.5), module, ProbablyNote::KEY_LOGARITHMIC_SCALE_LIGHT));

        ParamWidget* shiftParam = createParam<RoundSmallFWSnapKnob>(Vec(133,85), module, ProbablyNote::SHIFT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(shiftParam)->percentage = &module->shiftPercentage;
			dynamic_cast<RoundSmallFWSnapKnob*>(shiftParam)->biDirectional = true;
		}
		addParam(shiftParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(159,112), module, ProbablyNote::SHIFT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(161, 90), module, ProbablyNote::SHIFT_INPUT));

		addParam(createParam<LEDButton>(Vec(135, 113), module, ProbablyNote::SHIFT_SCALING_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(136.5, 114.5), module, ProbablyNote::SHIFT_MODE_LIGHT));

		ParamWidget* octaveParam = createParam<RoundSmallFWSnapKnob>(Vec(188,86), module, ProbablyNote::OCTAVE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(octaveParam)->percentage = &module->octavePercentage;
		}
		addParam(octaveParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,112), module, ProbablyNote::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 90), module, ProbablyNote::OCTAVE_INPUT));


		addParam(createParam<LEDButton>(Vec(214, 143), module, ProbablyNote::OCTAVE_WRAPAROUND_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(215.5, 144.5), module, ProbablyNote::OCTAVE_WRAPAROUND_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 144), module, ProbablyNote::OCTAVE_WRAP_INPUT));
		
		addParam(createParam<LEDButton>(Vec(214, 180), module, ProbablyNote::TEMPERMENT_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(215.5, 181.5), module, ProbablyNote::INTONATION_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(192, 181), module, ProbablyNote::TEMPERMENT_INPUT)); 


		ParamWidget* pitchRandomnessParam = createParam<RoundSmallFWKnob>(Vec(188,216), module, ProbablyNote::PITCH_RANDOMNESS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(pitchRandomnessParam)->percentage = &module->pitchRandomnessPercentage;
		}
		addParam(pitchRandomnessParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,242), module, ProbablyNote::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 220), module, ProbablyNote::PITCH_RANDOMNESS_INPUT));

		addParam(createParam<LEDButton>(Vec(190, 250), module, ProbablyNote::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(191.5, 251.5), module, ProbablyNote::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));

		addParam(createParam<RoundReallySmallFWKnob>(Vec(202,292), module, ProbablyNote::WEIGHT_SCALING_PARAM));		


		addParam(createParam<LEDButton>(Vec(62, 345), module, ProbablyNote::TRIGGER_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(63.5, 346.5), module, ProbablyNote::TRIGGER_POLYPHONIC_LIGHT));


		PortWidget* weightInput;
		ParamWidget* weightParam;
		ParamWidget* noteOnParam;
		LightWidget* noteOnLight;

		// addInput(bob);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,306), module, ProbablyNote::NOTE_WEIGHT_PARAM+0);
		weightParams[0] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[0];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(142, 310), module, ProbablyNote::NOTE_WEIGHT_INPUT+0);
		inputs[0] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 307), module, ProbablyNote::NOTE_ACTIVE_PARAM+0);
		noteOnParams[0] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 308.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+0); 
		lights[0] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(47,292), module, ProbablyNote::NOTE_WEIGHT_PARAM+1);
		weightParams[1] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[1];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(32, 296), module, ProbablyNote::NOTE_WEIGHT_INPUT+1);
		inputs[1] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(69, 293), module, ProbablyNote::NOTE_ACTIVE_PARAM+1);
		noteOnParams[1] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(70.5, 294.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+2); 
		lights[1] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,278), module, ProbablyNote::NOTE_WEIGHT_PARAM+2);
		weightParams[2] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[2];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(142, 282), module, ProbablyNote::NOTE_WEIGHT_INPUT+2);
		inputs[2] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 279), module, ProbablyNote::NOTE_ACTIVE_PARAM+2);
		noteOnParams[2] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 280.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+4); 
		lights[2] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(47,264), module, ProbablyNote::NOTE_WEIGHT_PARAM+3);
		weightParams[3] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[3];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(32, 268), module, ProbablyNote::NOTE_WEIGHT_INPUT+3);
		inputs[3] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(69, 265), module, ProbablyNote::NOTE_ACTIVE_PARAM+3);
		noteOnParams[3] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(70.5, 266.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+6); 
		lights[3] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,250), module, ProbablyNote::NOTE_WEIGHT_PARAM+4);
		weightParams[4] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[4];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(142, 254), module, ProbablyNote::NOTE_WEIGHT_INPUT+4);
		inputs[4] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 251), module, ProbablyNote::NOTE_ACTIVE_PARAM+4);
		noteOnParams[4] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 252.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+8); 
		lights[4] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,222), module, ProbablyNote::NOTE_WEIGHT_PARAM+5);
		weightParams[5] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[5];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(142, 226), module, ProbablyNote::NOTE_WEIGHT_INPUT+5);
		inputs[5] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 223), module, ProbablyNote::NOTE_ACTIVE_PARAM+5);
		noteOnParams[5] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 224.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+10); 
		lights[5] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(47,208), module, ProbablyNote::NOTE_WEIGHT_PARAM+6);
		weightParams[6] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[6];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(32, 212), module, ProbablyNote::NOTE_WEIGHT_INPUT+6);
		inputs[6] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(69, 209), module, ProbablyNote::NOTE_ACTIVE_PARAM+6);
		noteOnParams[6] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(70.5, 210.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+12); 
		lights[6] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,194), module, ProbablyNote::NOTE_WEIGHT_PARAM+7);
		weightParams[7] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[7];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(142, 198), module, ProbablyNote::NOTE_WEIGHT_INPUT+7);
		inputs[7] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 195), module, ProbablyNote::NOTE_ACTIVE_PARAM+7);
		noteOnParams[7] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 196.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+14); 
		lights[7] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(47,180), module, ProbablyNote::NOTE_WEIGHT_PARAM+8);
		weightParams[8] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[8];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(32, 184), module, ProbablyNote::NOTE_WEIGHT_INPUT+8);
		inputs[8] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(69, 181), module, ProbablyNote::NOTE_ACTIVE_PARAM+8);
		noteOnParams[8] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(70.5, 182.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+16); 
		lights[8] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,166), module, ProbablyNote::NOTE_WEIGHT_PARAM+9);
		weightParams[9] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[9];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(132, 170), module, ProbablyNote::NOTE_WEIGHT_INPUT+9);
		inputs[9] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 167), module, ProbablyNote::NOTE_ACTIVE_PARAM+9);
		noteOnParams[9] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 168.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+18); 
		lights[9] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(47,152), module, ProbablyNote::NOTE_WEIGHT_PARAM+10);
		weightParams[10] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[10];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(32, 156), module, ProbablyNote::NOTE_WEIGHT_INPUT+10);
		inputs[10] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(69, 153), module, ProbablyNote::NOTE_ACTIVE_PARAM+10);
		noteOnParams[10] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(70.5, 154.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+20); 
		lights[10] = noteOnLight;
		addChild(noteOnLight);

		weightParam = createParam<RoundReallySmallFWKnob>(Vec(119,138), module, ProbablyNote::NOTE_WEIGHT_PARAM+11);
		weightParams[11] = weightParam;
		if (module) {
			dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->weightPercentage[11];
		}
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(142, 142), module, ProbablyNote::NOTE_WEIGHT_INPUT+11);
		inputs[11] = weightInput;	
		addInput(weightInput);
		noteOnParam = createParam<LEDButton>(Vec(97, 139), module, ProbablyNote::NOTE_ACTIVE_PARAM+11);
		noteOnParams[11] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(98.5, 140.5), module, ProbablyNote::NOTE_ACTIVE_LIGHT+22); 
		lights[11] = noteOnLight;
		addChild(noteOnLight);

		

		addOutput(createOutput<FWPortInSmall>(Vec(205, 345),  module, ProbablyNote::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(172, 345),  module, ProbablyNote::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(134, 345),  module, ProbablyNote::NOTE_CHANGE_OUTPUT));

	}

	void step() override {
		if (module) {
			//panel->visible = ((((ProbablyNote*)module)->useCircleLayout) == 0);

			if((((ProbablyNote*)module)->useCircleLayout) == 0) {
				circlePanel->visible  = false;
				getPanel()->visible = true;

				weightParams[0]->box.pos.x = 119;
				weightParams[0]->box.pos.y = 306;
				inputs[0]->box.pos.x = 142;
				inputs[0]->box.pos.y =310;
				noteOnParams[0]->box.pos.x = 97;
				noteOnParams[0]->box.pos.y = 307;
				lights[0]->box.pos.x = 98.5;
				lights[0]->box.pos.y = 308.5;
												
				weightParams[1]->box.pos.x = 47;
				weightParams[1]->box.pos.y = 292;
				inputs[1]->box.pos.x = 32;
				inputs[1]->box.pos.y = 296;
				noteOnParams[1]->box.pos.x = 69;
				noteOnParams[1]->box.pos.y = 294;
				lights[1]->box.pos.x = 70.5;
				lights[1]->box.pos.y = 294.5;

				weightParams[2]->box.pos.x = 119;
				weightParams[2]->box.pos.y = 278;
				inputs[2]->box.pos.x = 142;
				inputs[2]->box.pos.y = 282;
				noteOnParams[2]->box.pos.x = 97;
				noteOnParams[2]->box.pos.y = 279;
				lights[2]->box.pos.x = 98.5;
				lights[2]->box.pos.y = 280.5;

				weightParams[3]->box.pos.x = 47;
				weightParams[3]->box.pos.y = 264;
				inputs[3]->box.pos.x = 32;
				inputs[3]->box.pos.y = 268;
				noteOnParams[3]->box.pos.x = 69;
				noteOnParams[3]->box.pos.y = 265;
				lights[3]->box.pos.x = 70.5;
				lights[3]->box.pos.y = 266.5;


				weightParams[4]->box.pos.x = 119;
				weightParams[4]->box.pos.y = 250;
				inputs[4]->box.pos.x = 142;
				inputs[4]->box.pos.y = 254;
				noteOnParams[4]->box.pos.x = 97;
				noteOnParams[4]->box.pos.y = 251;
				lights[4]->box.pos.x = 98.5;
				lights[4]->box.pos.y = 252;

				weightParams[5]->box.pos.x = 119;
				weightParams[5]->box.pos.y = 222;
				inputs[5]->box.pos.x = 142;
				inputs[5]->box.pos.y = 226;
				noteOnParams[5]->box.pos.x = 97;
				noteOnParams[5]->box.pos.y = 223;
				lights[5]->box.pos.x = 98.5;
				lights[5]->box.pos.y = 224.5;

				weightParams[6]->box.pos.x = 47;
				weightParams[6]->box.pos.y = 208;
				inputs[6]->box.pos.x = 32;
				inputs[6]->box.pos.y = 212;
				noteOnParams[6]->box.pos.x = 69;
				noteOnParams[6]->box.pos.y = 209;
				lights[6]->box.pos.x = 70.5;
				lights[6]->box.pos.y = 210.5;


				weightParams[7]->box.pos.x = 119;
				weightParams[7]->box.pos.y = 194;
				inputs[7]->box.pos.x = 142;
				inputs[7]->box.pos.y = 198;
				noteOnParams[7]->box.pos.x = 97;
				noteOnParams[7]->box.pos.y = 195;
				lights[7]->box.pos.x = 98.5;
				lights[7]->box.pos.y = 196.5;

				weightParams[8]->box.pos.x = 47;
				weightParams[8]->box.pos.y = 180;
				inputs[8]->box.pos.x = 32;
				inputs[8]->box.pos.y = 184;
				noteOnParams[8]->box.pos.x = 69;
				noteOnParams[8]->box.pos.y = 181;
				lights[8]->box.pos.x = 70.5;
				lights[8]->box.pos.y = 182.5;

				weightParams[9]->box.pos.x = 119;
				weightParams[9]->box.pos.y = 166;
				inputs[9]->box.pos.x = 142;
				inputs[9]->box.pos.y = 170;
				noteOnParams[9]->box.pos.x = 97;
				noteOnParams[9]->box.pos.y = 167;
				lights[9]->box.pos.x = 98.5;
				lights[9]->box.pos.y = 168.5;

				weightParams[10]->box.pos.x = 47;
				weightParams[10]->box.pos.y = 152;
				inputs[10]->box.pos.x = 32;
				inputs[10]->box.pos.y = 156;
				noteOnParams[10]->box.pos.x = 69;
				noteOnParams[10]->box.pos.y = 153;
				lights[10]->box.pos.x = 70.5;
				lights[10]->box.pos.y = 154.5;

				weightParams[11]->box.pos.x = 119;
				weightParams[11]->box.pos.y = 138;
				inputs[11]->box.pos.x = 142;
				inputs[11]->box.pos.y = 142;
				noteOnParams[11]->box.pos.x = 97;
				noteOnParams[11]->box.pos.y = 139;
				lights[11]->box.pos.x = 98.5;
				lights[11]->box.pos.y = 140.5;

			} else {
				circlePanel->visible  = true;
				getPanel()->visible = false;

				for(int i=0;i<MAX_NOTES;i++) {
					double position = 2.0 * M_PI / MAX_NOTES * i  - M_PI / 2.0; // Rotate 90 degrees

					double x= cos(position) * 54.0 + 90.0;
					double y= sin(position) * 54.0 + 230.5;

					//Rotate inputs 1 degrees
					weightParams[i]->box.pos.x = x;
					weightParams[i]->box.pos.y = y;
					x= cos(position + (M_PI / 180.0 * 1.0)) * 36.0 + 94.0;
					y= sin(position + (M_PI / 180.0 * 1.0)) * 36.0 + 235.0;
					inputs[i]->box.pos.x = x;
					inputs[i]->box.pos.y = y;

					//Rotate buttons 5 degrees
					x= cos(position - (M_PI / 180.0 * 5.0)) * 75.0 + 91.0;
					y= sin(position - (M_PI / 180.0 * 5.0)) * 75.0 + 231.0;
					noteOnParams[i]->box.pos.x = x;
					noteOnParams[i]->box.pos.y = y;
					lights[i]->box.pos.x = x+1.5;
					lights[i]->box.pos.y = y+1.5;
				}
			}
		}
		Widget::step();
	}



	struct PNLayoutItem : MenuItem {
		ProbablyNote *module;
		bool layout;
		void onAction(const event::Action &e) override {
			module->useCircleLayout = layout;
		}
		void step() override {
			rightText = (module->useCircleLayout == layout) ? "✔" : "";
		}
	};

	struct TriggerDelayItem : MenuItem {
		ProbablyNote *module;
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

		ProbablyNote *module = dynamic_cast<ProbablyNote*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Layout";
		menu->addChild(themeLabel);

		PNLayoutItem *pnLayout1Item = new PNLayoutItem();
		pnLayout1Item->text = "Keyboard";// 
		pnLayout1Item->module = module;
		pnLayout1Item->layout= false;
		menu->addChild(pnLayout1Item);

		PNLayoutItem *pnLayout2Item = new PNLayoutItem();
		pnLayout2Item->text = "Circle";// 
		pnLayout2Item->module = module;
		pnLayout2Item->layout= true;
		menu->addChild(pnLayout2Item);

		MenuLabel *spacerLabel2 = new MenuLabel();
		menu->addChild(spacerLabel2);

		menu->addChild(new MenuLabel());
		{
      		OptionsMenuItem* mi = new OptionsMenuItem("Quantize Mode");
			mi->addItem(OptionMenuItem("Closest", [module]() { return module->quantizeMode == 0; }, [module]() { module->quantizeMode = 0; }));
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
Model *modelProbablyNote = createModel<ProbablyNote, ProbablyNoteWidget>("ProbablyNote");
    
