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

#define MAX_NOTES 12
#define MAX_JINS 24
#define MAX_FAMILIES 16
#define MAX_UPPER_AJNAS 10
#define MAX_OCTAVE_AJNAS 4
#define MAX_MAQAM 8
#define MAX_JINS_NOTES 9
#define MAX_COMPLETE_SCALE_SIZE 16


using namespace frozenwasteland::dsp;

struct Jins {
	const char* Name;
	int NumberMainTones;
	int NumberLowerTones;
	int NumberExtendedTones;
	int GhammazPosition;
	int NumberTonics;
	int AvailableTonics[MAX_NOTES];
	float Intonation[MAX_JINS_NOTES];
	float DefaultWeighting[MAX_JINS_NOTES];	
	int NumberMaqams;
};


struct Maqam {
	const char* Name;
	int NumberUpperAnjas;
	int UpperAjnas[MAX_UPPER_AJNAS];
	int OverlapPoint[MAX_UPPER_AJNAS];
	float AjnasWeighting[MAX_UPPER_AJNAS+1][MAX_UPPER_AJNAS+1];
	bool AjnasUsed[MAX_UPPER_AJNAS+1][MAX_UPPER_AJNAS+1];

	int NumberOctaveAnjas;
	int OctaveAjnas[MAX_OCTAVE_AJNAS];
	float OctaveAjnasWeighting[MAX_UPPER_AJNAS+1][MAX_OCTAVE_AJNAS];
	bool OctaveAjnasUsed[MAX_UPPER_AJNAS+1][MAX_OCTAVE_AJNAS];

};

struct ProbablyNoteArabic : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		SLANT_PARAM,
		SLANT_CV_ATTENUVERTER_PARAM,
		FOCUS_PARAM,
		FOCUS_CV_ATTENUVERTER_PARAM,
        MAQAM_PARAM,
		MAQAM_CV_ATTENUVERTER_PARAM,
        FAMILY_PARAM,
		FAMILY_CV_ATTENUVERTER_PARAM,
		MODULATE_PARAM,
        MODULATION_CHANCE_PARAM,
		MODULATION_CHANCE_CV_ATTENUVERTER_PARAM,
        TONIC_PARAM,
		TONIC_CV_ATTENUVERTER_PARAM,
        USE_TRADITIONAL_TONIC_PARAM,
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,	
		PITCH_RANDOMNESS_PARAM,
		PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM,
		PITCH_RANDOMNESS_GAUSSIAN_PARAM,
        RESET_SCALE_PARAM,
		WEIGHT_SCALING_PARAM,
		LOWER_JINS_ACTIVE_PARAM,
		LOWER_JINS_WEIGHT_PARAM,
		UPPER_AJNAS_ACTIVE_PARAM,
		UPPER_AJNAS_WEIGHT_PARAM = UPPER_AJNAS_ACTIVE_PARAM + MAX_UPPER_AJNAS,
		OCTAVE_AJNAS_ACTIVE_PARAM = UPPER_AJNAS_WEIGHT_PARAM + MAX_UPPER_AJNAS,
		OCTAVE_AJNAS_WEIGHT_PARAM = OCTAVE_AJNAS_ACTIVE_PARAM + MAX_OCTAVE_AJNAS,
        NOTE_ACTIVE_PARAM = OCTAVE_AJNAS_WEIGHT_PARAM + MAX_OCTAVE_AJNAS,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_COMPLETE_SCALE_SIZE,
		NUM_PARAMS = NOTE_WEIGHT_PARAM + MAX_COMPLETE_SCALE_SIZE
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		SLANT_INPUT,
		FOCUS_INPUT,
		MAQAM_INPUT,
        FAMILY_INPUT,
		MODULATION_CHANCE_INPUT,
        TONIC_INPUT,
        OCTAVE_INPUT,
		PITCH_RANDOMNESS_INPUT,
		RESET_SCALE_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
		MODULATION_TRIGGER_INPUT,
		JINS_SELECT_INPUT,
		LOWER_JINS_WEIGHT_INPUT,		
        UPPER_AJNAS_WEIGHT_INPUT,
        OCTAVE_AJNAS_WEIGHT_INPUT = UPPER_AJNAS_WEIGHT_INPUT + MAX_UPPER_AJNAS,
        NOTE_WEIGHT_INPUT = OCTAVE_AJNAS_WEIGHT_INPUT + MAX_OCTAVE_AJNAS,
		NUM_INPUTS = NOTE_WEIGHT_INPUT + MAX_COMPLETE_SCALE_SIZE
	};
	enum OutputIds {
		QUANT_OUTPUT,
		WEIGHT_OUTPUT,
		NOTE_CHANGE_OUTPUT,
		CURRENT_JINS_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		USE_TRADITIONAL_TONIC_LIGHT,
		PITCH_RANDOMNESS_GAUSSIAN_LIGHT,
		LOWER_JINS_ACTIVE_LIGHT,
		UPPER_AJNAS_ACTIVE_LIGHT = LOWER_JINS_ACTIVE_LIGHT + 2,
		OCTAVE_AJNAS_ACTIVE_LIGHT = UPPER_AJNAS_ACTIVE_LIGHT + MAX_UPPER_AJNAS * 2,
        NOTE_ACTIVE_LIGHT = OCTAVE_AJNAS_ACTIVE_LIGHT + MAX_OCTAVE_AJNAS * 2,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_COMPLETE_SCALE_SIZE * 2
	};


	const char* noteNames[MAX_NOTES] = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};

// c c#     d       d#     e     f       f#      g       g#      a     a#     b
//{0,111.73,203.91,315.64,386.61,498.04,582.51,701.955,813.69,884.36,996.09,1088.27},
//pythagorean minor 3d 294.13, maj 407.82
	Jins jins[MAX_JINS] = {
		{"Rast", 5,2,2,4,3, {0,5,7}, {-315.64,-161.73,0,203.91,344.13,498.04,701.955,813.69,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1,0.05}, 7}, 
		{"Nahawand", 5,3,1,4,4, {0,2,5,7}, {-386.61,-315.64,-111.78,0,203.91,294.13,498.04,701.955,813.69}, {0.03,0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 5}, 
		{"Nikriz", 5,2,2,4,4, {0,2,5,7}, {-386.61,-111.73,0,203.91,315.64,582.51,701.955,813.69,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1,0.05}, 4}, 
		{"Ajam", 5,2,1,4,3, {0,5,7}, {-315.64,-111.73,0,203.91,407.82,498.04,701.955,884.36}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 4}, 
		{"Bayati", 4,2,2,3,4, {1,4,9,0}, {-336.61,-203.91,0,161.73,386.61,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 6}, 
		{"Hijaz", 4,2,2,3,4, {1,7,9,0}, {-336.61,-203.91,0,111.73,294.13,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 7}, 
		{"Kurd", 4,1,2,4,4, {1,7,9,0}, {-203.91,0,121.73,315.64,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 3}, 
		{"Saba", 3,2,3,2,3, {1,7,9}, {-336.61,-203.91,0,161.73,294.13,386.61,498.04,582.51}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 2}, 
		{"Sikah", 3,3,1,2,3, {4,9,11}, {-386.61,-203.91,-111.73,50,203.91,386.61,498.04}, {0.05,0.05,0.1,1,0.5,0.8,0.1}, 7}, 
		{"Jiharkah", 5,2,1,4,3, {5,7,11}, {-315.64,-161.73,0,203.91,382.33,473.04,701.955,884.36}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 1}, //Extended Jins
		{"Sazkar", 5,2,2,4,4, {0,2,5,7}, {-315.64,-161.73,0,244.91,344.13,498.04,701.955,813.69,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1,0.05}, 5}, 
		{"Musta'ar", 3,3,1,2,3, {4,9,11}, {-386.61,-203.91,-111.73,50,203.91,386.61,498.04}, {0.03,0.05,0.1,1,0.5,0.8,0.1}, 1}, 
		{"Murassa", 5,2,1,4,4, {0,2,5,7}, {-315.64,-111.78,0,203.91,294.13,498.04,582.51,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 1}, 
		{"Athar Kurd", 5,2,1,4,5, {0,2,5,7,9}, {-315.64,-111.78,0,111.73,315.64,582.51,701.955,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 1}, 
		{"Saba Zamzam", 3,1,3,2,3, {1,7,9}, {-203.91,0,111.73,294.13,386.61,498.04,582.51}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 1}, 
		{"Lami", 4,0,2,4,1, {1}, {0,111.73,315.64,498.04,582.51,813.69}, {1,0.5,0.5,0.8,0.1,0.05}, 1}, 
		{"Upper Rast", 4,1,1,1,1, {0}, {-203.91,0,203.91,344.13,498.04,701.955}, {0.1,0.8,0.5,0.5,1.0,0.1}, 0}, //Auexillary Jins for modulations #16
		{"Upper Ajam", 4,1,1,1,1, {0}, {-203.91,0,203.91,407.82,498.04,701.955}, {0.1,0.8,0.5,0.5,1.0,0.1}, 0}, 
		{"Saba Dalanshin", 3,1,2,1,1, {0}, {-203.91,0,161.73,294.13,498.04,701.955,813.69}, {0.1,0.8,0.5,1,0.1,0.1,0.05}, 0}, 
		{"Hijazkar", 6,0,0,2,1, {0}, {-336.61,-203.91,0,111.73,294.13,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 0}, //Not done
		{"Sikah Baladi", 6,0,0,2,1, {0}, {-386.61,-203.91,-111.73,50,203.91,386.61,498.04}, {0.05,0.05,0.1,1,0.5,0.8,0.1}, 0}, //Not done
		{"Mukhalif Sharqi", 3,0,0,2,1, {0}, {0,203.91,386.61}, {1,0.5,0.8}, 0}, //Not done
		{"Hijaz Murassa", 4,2,1,1,1, {0}, {-336.61,-203.91,0,111.73,294.13,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 0}, //Not done
		{"Ajam Murassa", 5,1,1,4,1, {0}, {-111.73,0,203.91,407.82,498.04,701.955,884.36}, {0.1,1,0.5,0.5,0.8,0.1,0.05}, 0}, //Not Done
	};

	Maqam maqams[MAX_FAMILIES][MAX_MAQAM] = {
		{ 
			{"Rast", 6, {0,1,2,3,4,5}, {5,5,5,5,5,5}, 
				{{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3}}, 
				{{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1}} }, //Not done -place holder jins
			{"Kirdan", 4, {16,1,5,8}, {5,5,5,3}, 
				{{1.0f,1.0f,0.5,0.4,0.3},{1.0f,1.0f,0.5,0.4,0.0},{1.0f,1.0f,0.5,0.4,0.3},{1.0f,1.0f,0.5,0.4,0.3},{1.0f,0.0f,0.5,0.4,0.3}}, 
				{{1,1,1,1,1},{1,1,1,1,0},{1,1,1,1,1},{1,1,1,1,1},{1,0,1,1,1}} }, 
			{"Suznak", 5, {5,4,7,1,8}, {5,5,5,5,3}, 
				{{1.0f,1.0f,0.0,0.0,0.5,0.5},{1.0f,1.0f,0.5,0.4,0.3,0.3},{1.0f,1.0f,0.5,0.4,0.3,0.3},{0.0f,1.0f,0.5,0.4,0.3,0.3},{1.0f,0.0f,0.5,0.4,0.3,0.3},{1.0f,0.0f,0.5,0.4,0.3,0.3}}, 
				{{1,1,1,0,0,1},{1,1,1,1,1,1},{1,1,1,1,1,1},{0,1,1,1,1,1},{1,1,1,1,1,1},{1,1,1,1,1,1}} },
			{"Nairuz", 4, {4,7,5,1}, {5,5,5,5}, 
				{{1.0f,1.0f,0.5,0.4,0.3},{1.0f,1.0f,0.5,0.4,0.3},{1.0f,1.0f,0.5,0.4,0.3},{1.0f,1.0f,0.5,0.4,0.3},{1.0f,1.0f,0.5,0.4,0.3}}, 
				{{1,1,1,1,1},{1,1,1,1,1},{1,1,1,1,1},{1,1,1,1,1},{1,1,1,1,1}} },
			{"Dalanshin", 3, {18,16,1}, {6,5,5}, 
				{{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4}}, 
				{{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}} }, 
			{"Suzdalara", 3, {5,8,11}, {5,3,3}, 
				{{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4}}, 
				{{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}} }, 
			{"Mahur", 3, {17,1,5}, {5,5,5}, 
				{{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4},{1.0f,1.0f,0.5,0.4}}, 
				{{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}} } 
		},
		{

		},
		{

		},
		{ 
			{"Ajam", 6, {0,1,2,3,4,5}, {5,5,5,5,5,5}, 
				{{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3},{1.0f,1.0f,0.3,0.3,0.3,0.1,0.3}}, 
				{{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1}} }, 
			{"Jiharkah", 5, {4,5,6,7,8}, {5,5,5,6,5}, {1.0f,1.0f,0.3,0.3,0.3,0.3}, {{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1},{1,1,1,1,1,1,1}} } 
		},
		{
			{"Bayati Shuri", 5, {5,4,1,0,7}, {4,8,7,7,4}, 
				{{1.0f,1.0f,0.5,0.0,0.5,0.5},{1.0f,1.0f,0.5,0.4,0.3,0.3},{1.0f,1.0f,0.5,0.4,0.3,0.3},{0.0f,1.0f,0.5,0.4,0.3,0.3},{1.0f,0.0f,0.5,0.4,0.3,0.3},{1.0f,0.0f,0.5,0.4,0.3,0.3}}, 
				{{1,1,1,0,0,0},{1,1,1,1,1,1},{1,1,1,0,0,0},{0,1,0,1,1,0},{0,1,0,1,1,0},{0,1,0,0,0,1}} }
		}
	};
   

	const char* maqamNames[MAX_FAMILIES][MAX_MAQAM] = {
		{"Mahur" , "Nairuz" , "Rast" , "Suznak" , "Yakah"},
		{"Farahfaza", "Nahawand", "Nahawand Murassah", "‘Ushaq Masri" , "Sultani Yakah"},
		{"Athar Kurd", "Nawa Athar", "Nikriz" , "Hisar"},
		{"Ajam","Jiharkah","Shawq Afza","Ajam Ushayran"},
		{"Bayatayn" , "Bayati" , "Bayati Shuri" , "Husayni" , "Nahfat" , "Huseini Ushayran"},
		{"Hijaz" , "Hijaz Kar" , "Shad ‘Araban" , "Shahnaz" , "Suzidil" , "Zanjaran" , "Hijazain"},
		{"Kurd" , "Hijaz Kar Kurd", "Lami"},
		{"Saba" , "Saba Zamzam" },
		{"Bastah Nikar", "Huzam", "‘Iraq" , "Musta‘ar", "Rahat al-Arwah" , "Sikah" , "Sikah Baladi"}
	};

	
	
	//const char* arabicScaleNames[MAX_JINS] = {"عجم","بياتي","حجاز","كرد","نهاوند","نكريز","راست","صبا","سيكاه"};
	
	dsp::SchmittTrigger clockTrigger,modulateTrigger,resetScaleTrigger,traditionalTonicsTrigger,pitchRandomnessGaussianTrigger,noteActiveTrigger[MAX_JINS_NOTES],lowerJinsActiveTrigger, upperAjnasActiveTrigger[MAX_UPPER_AJNAS]; 
	dsp::PulseGenerator noteChangePulse,jinsChangedPulse;
	GaussianNoiseGenerator _gauss;
    
 
    bool noteActive[MAX_COMPLETE_SCALE_SIZE] = {false};
    float noteInitialProbability[MAX_COMPLETE_SCALE_SIZE] = {0.0f};
	float actualNoteProbability[MAX_COMPLETE_SCALE_SIZE] = {0.0};
	

	int currentUpperAjnas[MAX_UPPER_AJNAS] = {0};
	int currentUpperAjnasOverlap[MAX_UPPER_AJNAS] = {0};
    bool currentAjnasStatus[MAX_UPPER_AJNAS+1] = {0};
    bool currentAjnasActive[MAX_UPPER_AJNAS+1] = {1,1,1,1,1,1,1};
    float currentAjnasWeighting[MAX_UPPER_AJNAS+1] = {0};
	float actualAjnasProbability[MAX_UPPER_AJNAS+1] = {0};
	
    int octave = 0;
    int spread = 0;
	float upperSpread = 0.0;
	float lowerSpread = 0.0;
	float slant = 0;
	float focus = 0; 
	int currentNote = 0;
	int probabilityNote = 0;
	float lastQuantizedCV = 0;
	bool resetTriggered = false;
	int lastNote = -1;
	int lastSpread = -1;
	int lastSlant = -1;
	float lastFocus = -1;
	bool pitchRandomGaussian = false;


	int family = 0;
	int lastFamily = -1;

    int tonic = 0;
    int lastTonic = -1;

	int maqamIndex = 0;
	int lastMaqamIndex = -1;

	int jinsIndex = 0;
	int lastJinsIndex = -1;

	Jins currentJins;
	Jins rootJins;
	Maqam currentMaqam;


	bool lowerJinsActive;
	bool upperAjnasActive[MAX_UPPER_AJNAS];
	int numberActiveAjnas = 0;

	int numberMainTones;
	int numberLowerTones;
	int numberExtendedTones;
	int ghammazPosition;
	float currentIntonation[MAX_COMPLETE_SCALE_SIZE];
	float currentDefaultWeighting[MAX_COMPLETE_SCALE_SIZE];	
	int numberMaqams;
	int numberTonics;
	int availableTonics[MAX_NOTES];



	std::string lastPath;
    

	ProbablyNoteArabic() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteArabic::SPREAD_PARAM, 0.0, 7.0, 0.0,"Spread");
        configParam(ProbablyNoteArabic::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteArabic::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant");
        configParam(ProbablyNoteArabic::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteArabic::FOCUS_PARAM, 0.0, 1.0, 0.0,"Focus");
		configParam(ProbablyNoteArabic::FOCUS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Focus CV Attenuation");
		configParam(ProbablyNoteArabic::FAMILY_PARAM, 0.0, (float)MAX_FAMILIES-1, 0.0,"Family (Root Jins)");
        configParam(ProbablyNoteArabic::FAMILY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Family CV Attenuation","%",0,100);
		configParam(ProbablyNoteArabic::MAQAM_PARAM, 0.0, 8.0, 0.0,"Maqam");
        configParam(ProbablyNoteArabic::MAQAM_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Maqam CV Attenuation","%",0,100);
		
        
		configParam(ProbablyNoteArabic::TONIC_PARAM, 0.0, 2.0, 0.0,"Tonic");
        configParam(ProbablyNoteArabic::TONIC_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Tonic CV Attenuation","%",0,100); 
		configParam(ProbablyNoteArabic::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
        configParam(ProbablyNoteArabic::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNoteArabic::PITCH_RANDOMNESS_PARAM, 0.0, 10.0, 0.0,"Randomize Pitch Amount"," Cents");
        configParam(ProbablyNoteArabic::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Randomize Pitch Amount CV Attenuation","%",0,100);
		
		

        srand(time(NULL));

        for(int i=0;i<MAX_UPPER_AJNAS;i++) {
			configParam(ProbablyNoteArabic::UPPER_AJNAS_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Upper Jins Active");
			configParam(ProbablyNoteArabic::UPPER_AJNAS_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Upper Jins Weight");
		}

        for(int i=0;i<MAX_COMPLETE_SCALE_SIZE;i++) {
            configParam(ProbablyNoteArabic::NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
            configParam(ProbablyNoteArabic::NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight");		
        }

		onReset();
	}

	void reConfigParam (int paramId, float minValue, float maxValue, float defaultValue) {
		ParamQuantity *pq = paramQuantities[paramId];
		pq->minValue = minValue;
		pq->maxValue = maxValue;
		pq->defaultValue = defaultValue;		
	}

    float lerp(float v0, float v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,int weightCount, float scaling,float randomIn) {
        float weightTotal = 0.0f;
		float linearWeight, logWeight, weight;
            
        for(int i = 0;i < weightCount; i++) {
			linearWeight = weights[i];
			logWeight = std::log10(weights[i]*10 + 1);
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

		//Bail out if no weights
			if(weightTotal == 0.0)
			return 0;

        int chosenWeight = -1;        
        float rnd = randomIn * weightTotal;
        for(int i = 0;i < weightCount;i++ ) {
			linearWeight = weights[i];
			logWeight = std::log10(weights[i]*10 + 1);
			weight = lerp(linearWeight,logWeight,scaling);

            if(rnd < weight) {
                chosenWeight = i;
                break;
            }
            rnd -= weight;
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
			offsetNote = (offsetNote + 1) % MAX_JINS_NOTES;
			notesSearched +=1;
			if(noteActive[offsetNote]) {
				noteCount +=1;
			}
			noteOk = noteCount == offset;
		} while(!noteOk && notesSearched < MAX_JINS_NOTES);
		return offsetNote;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		

		for(int i=0;i<MAX_JINS;i++) {
			for(int j=0;j<MAX_JINS_NOTES;j++) {
				char buf[100];
				char notebuf[100];
				strcpy(buf, "jinsNoteWeight-");
				strcat(buf, jins[i].Name);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				//json_object_set_new(rootJ, buf, json_real((float) scaleNoteWeighting[i][j]));
			}
		}
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {
		for(int i=0;i<MAX_JINS;i++) {
			for(int j=0;j<MAX_JINS_NOTES;j++) {
				char buf[100];
				char notebuf[100];
				strcpy(buf, "jinsNoteWeight-");
				strcat(buf, jins[i].Name);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				json_t *sumJ = json_object_get(rootJ, buf);
				if (sumJ) {
					//scaleNoteWeighting[i][j] = json_real_value(sumJ);
				}
			}
		}		
	}
	

	void process(const ProcessArgs &args) override {
	
		if (resetScaleTrigger.process(params[RESET_SCALE_PARAM].getValue())) {
			resetTriggered = true;			
			//for(int j=0;j<MAX_NOTES;j++) {
				// scaleNoteWeighting[scale][j] = defaultScaleNoteWeighting[scale][j];
				// scaleNoteStatus[scale][j] = defaultScaleNoteStatus[scale][j];
				// currentScaleNoteWeighting[j] = defaultScaleNoteWeighting[scale][j];
				// currentScaleNoteStatus[j] =	defaultScaleNoteStatus[scale][j];		
			//}					
		}	

		if (pitchRandomnessGaussianTrigger.process(params[PITCH_RANDOMNESS_GAUSSIAN_PARAM].getValue())) {
			pitchRandomGaussian = !pitchRandomGaussian;
		}		
		lights[PITCH_RANDOMNESS_GAUSSIAN_LIGHT].value = pitchRandomGaussian;



        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,15.0f);

		slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);

        focus = clamp(params[FOCUS_PARAM].getValue() + (inputs[FOCUS_INPUT].getVoltage() / 10.0f * params[FOCUS_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);

		family = clamp(params[FAMILY_PARAM].getValue() + (inputs[FAMILY_PARAM].getVoltage() * MAX_FAMILIES / 10.0 * params[FAMILY_CV_ATTENUVERTER_PARAM].getValue()),0.0,MAX_FAMILIES);
		if(family != lastFamily) {

			rootJins = jins[family]; 
			ghammazPosition = rootJins.GhammazPosition;			
			numberTonics = rootJins.NumberTonics;
			numberMaqams = rootJins.NumberMaqams;

			std::copy(rootJins.AvailableTonics,rootJins.AvailableTonics+numberTonics,availableTonics);


			reConfigParam(MAQAM_PARAM,0,(float)(numberMaqams-1),0);
			maqamIndex = 0;
			lastMaqamIndex = -1; //Force update;

			reConfigParam(TONIC_PARAM,0,(float)(numberTonics-1),0);
			tonic = 0;
			lastTonic = -1; //Force Update

			//Reset
			if(lastFamily != -1) {
				params[MAQAM_PARAM].setValue(0);
				params[TONIC_PARAM].setValue(0);
			}


			lastFamily = family;
		}

		maqamIndex = clamp(params[MAQAM_PARAM].getValue() + (inputs[MAQAM_INPUT].getVoltage() * MAX_MAQAM / 10.0 * params[MAQAM_CV_ATTENUVERTER_PARAM].getValue()),0.0,11.0f);
		if(maqamIndex != lastMaqamIndex) {
			currentMaqam = maqams[family][maqamIndex];

			numberActiveAjnas = currentMaqam.NumberUpperAnjas;
			std::copy(currentMaqam.UpperAjnas,currentMaqam.UpperAjnas+MAX_UPPER_AJNAS,currentUpperAjnas);
			std::copy(currentMaqam.OverlapPoint,currentMaqam.OverlapPoint+MAX_UPPER_AJNAS,currentUpperAjnasOverlap);
			
			jinsIndex = 0;
			lastJinsIndex = -1; // Force reload of jins status and weighting

			lastMaqamIndex = maqamIndex;
		}

		//Process Modulations
		if(modulateTrigger.process(params[MODULATE_PARAM].getValue() + inputs[MODULATION_TRIGGER_INPUT].getVoltage())) {
			if(lowerJinsActive) {
				actualAjnasProbability[0] = params[LOWER_JINS_WEIGHT_PARAM].getValue(); // need to include cv input
			} else {
				actualAjnasProbability[0] = 0;
			}

			for(int i=0;i<numberActiveAjnas;i++) {
				if(upperAjnasActive[i]) {	
					actualAjnasProbability[i+1] = params[UPPER_AJNAS_WEIGHT_PARAM+i].getValue(); // need to include cv input
				} else {
					actualAjnasProbability[i+1] = 0;
				}
			}

			// if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
			// 	rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f;
			// }	

			bool jinsChanged = false;

			do {
				float rnd = ((float) rand()/RAND_MAX);
				jinsIndex = weightedProbability(actualAjnasProbability, numberActiveAjnas+1, params[WEIGHT_SCALING_PARAM].getValue(),rnd);				
				jinsChanged = jinsIndex != lastJinsIndex;
				jinsChanged = true;
			} while(!jinsChanged);										
		}
		

		if(inputs[JINS_SELECT_INPUT].isConnected()) {
			jinsIndex = clamp((int)inputs[JINS_SELECT_INPUT].getVoltage(),0,numberActiveAjnas); //Might need to add 1
		}

		if(jinsIndex != lastJinsIndex) {
			currentJins = jins[jinsIndex == 0 ? family : currentUpperAjnas[jinsIndex-1]];

			std::copy(currentMaqam.AjnasWeighting[jinsIndex],currentMaqam.AjnasWeighting[jinsIndex]+MAX_UPPER_AJNAS+1,currentAjnasWeighting);
			std::copy(currentMaqam.AjnasUsed[jinsIndex],currentMaqam.AjnasUsed[jinsIndex]+MAX_UPPER_AJNAS+1,currentAjnasStatus);
			

			for(int i=0;i<numberActiveAjnas;i++) {
				upperAjnasActive[i] = currentAjnasStatus[i+1] && currentAjnasActive[i+1];	
				params[UPPER_AJNAS_WEIGHT_PARAM+i].setValue(currentAjnasWeighting[i+1]);
			}
			for(int i=numberActiveAjnas;i<MAX_UPPER_AJNAS;i++) {
				upperAjnasActive[i] = false;	
				params[UPPER_AJNAS_WEIGHT_PARAM+i].setValue(0.0);
			}

			lowerJinsActive =currentAjnasStatus[0];
			params[LOWER_JINS_WEIGHT_PARAM].setValue(currentAjnasWeighting[0]);


			numberMainTones = currentJins.NumberMainTones;
			numberLowerTones = currentJins.NumberLowerTones;
			numberExtendedTones = currentJins.NumberExtendedTones;
			// std::copy(currentJins.Intonation,currentJins.Intonation+MAX_JINS_NOTES,currentIntonation);
			// std::copy(currentJins.DefaultWeighting,currentJins.DefaultWeighting+MAX_JINS_NOTES,currentDefaultWeighting);

			for(int i=0;i<MAX_COMPLETE_SCALE_SIZE;i++) {
				if(3-rootJins.NumberLowerTones > i) {
					noteActive[i] = 0;
					currentDefaultWeighting[i] = 0.0f;
				} else if(jinsIndex > 0 && i >= 2 + currentMaqam.OverlapPoint[jinsIndex-1] && i < 2 + currentMaqam.OverlapPoint[jinsIndex-1] + currentJins.NumberMainTones ) {			
					noteActive[i] = 1;	
					currentDefaultWeighting[i] = currentJins.DefaultWeighting[i - (2 + currentMaqam.OverlapPoint[jinsIndex-1]) + currentJins.NumberLowerTones];	
					currentIntonation[i] = rootJins.Intonation[i - (2 + currentMaqam.OverlapPoint[jinsIndex-1]) + currentJins.NumberLowerTones];			
				} else if(i < 3 + rootJins.NumberMainTones) {
					noteActive[i+3-rootJins.NumberLowerTones] = 1;
					currentDefaultWeighting[i+3-rootJins.NumberLowerTones] = rootJins.DefaultWeighting[i];	
					currentIntonation[i+3-rootJins.NumberLowerTones] = rootJins.Intonation[i];
				} else if(jinsIndex == 0 && i < 3 + rootJins.NumberLowerTones + rootJins.NumberLowerTones + rootJins.NumberExtendedTones) {
					noteActive[i+3-rootJins.NumberLowerTones] = 1;
					currentDefaultWeighting[i+3-rootJins.NumberLowerTones] = rootJins.DefaultWeighting[i];	
					currentIntonation[i+3-rootJins.NumberLowerTones] = rootJins.Intonation[i];			
				} else {
					noteActive[i] = 0;
					currentDefaultWeighting[i] = 0.0f;
				} 

			}


		}

        int tonicIndex = clamp(params[TONIC_PARAM].getValue() + (inputs[TONIC_INPUT].getVoltage() * (float)numberTonics / 10.0 * params[TONIC_CV_ATTENUVERTER_PARAM].getValue()),0.0f,(float)numberTonics);
		tonic = availableTonics[tonicIndex];
		
        octave = clamp(params[OCTAVE_PARAM].getValue() + (inputs[OCTAVE_INPUT].getVoltage() * 0.4 * params[OCTAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);

	
        double noteIn = inputs[NOTE_INPUT].getVoltage();
        double octaveIn = std::floor(noteIn);
        double fractionalValue = noteIn - octaveIn;
        double lastDif = 1.0f;    
		float octaveAdjust = 0.0;
        for(int i = 0;i<MAX_COMPLETE_SCALE_SIZE;i++) {            
			double currentDif = std::abs((i / (float)MAX_NOTES) - fractionalValue);
			if(currentDif < lastDif) {
				lastDif = currentDif;
				currentNote = (i+numberLowerTones-tonic) % (numberLowerTones + numberMainTones + numberExtendedTones);
				if(currentNote < 0)
					currentNote += numberLowerTones + numberMainTones + numberExtendedTones;
			}            
        }
		if(currentNote < numberLowerTones)
			octaveAdjust = 1;


		if(lastNote != currentNote || lastSpread != spread || lastSlant != slant || lastFocus != focus) {
			for(int i = 0; i<MAX_COMPLETE_SCALE_SIZE;i++) {
				noteInitialProbability[i] = 0.0;
			}
			noteInitialProbability[currentNote] = 1.0;
			upperSpread = std::ceil((float)spread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil((float)spread * std::min(1.0-slant,1.0));

			for(int i=1;i<=spread;i++) {
				int noteAbove = (currentNote + i);
				int noteBelow = (currentNote - i);

				if(noteAbove < MAX_COMPLETE_SCALE_SIZE) {
					float upperInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
					noteInitialProbability[noteAbove] = i <= upperSpread ? upperInitialProbability : 0.0f;	
				}			
				if(noteBelow >= 0) {
					float lowerInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
					noteInitialProbability[noteBelow] = i <= lowerSpread ? lowerInitialProbability : 0.0f;		
				}		
			}
			lastNote = currentNote;
			lastSpread = spread;
			lastSlant = slant;
			lastFocus = focus;
		}

		
	
	
		//Process weights
		if(jinsIndex != lastJinsIndex || resetTriggered) {

			for(int i=0;i<MAX_JINS_NOTES;i++) {
				// currentScaleNoteWeighting[i] = scaleNoteWeighting[scale][i];
				// currentScaleNoteStatus[i] = scaleNoteStatus[scale][i];
				//noteActive[i] = currentScaleNoteStatus[i];
				//noteScaleProbability[i] = currentScaleNoteWeighting[i];
			}	

			
			for(int i=0;i<MAX_COMPLETE_SCALE_SIZE;i++) {				
				params[NOTE_WEIGHT_PARAM + i].setValue(currentDefaultWeighting[i]);
			}	

			lastTonic = tonic;
			lastJinsIndex = jinsIndex;
			resetTriggered = false;
		}

		if (lowerJinsActiveTrigger.process(params[LOWER_JINS_ACTIVE_PARAM].getValue())) {
			lowerJinsActive = !lowerJinsActive;
		}

		if(lowerJinsActive) {
			lights[LOWER_JINS_ACTIVE_LIGHT].value = clamp(params[LOWER_JINS_WEIGHT_PARAM].getValue() + (inputs[LOWER_JINS_WEIGHT_INPUT].getVoltage() / 10.0f),0.0f,1.0f);;    
			lights[LOWER_JINS_ACTIVE_LIGHT+1].value = 0;    
		}
		else { 
			lights[LOWER_JINS_ACTIVE_LIGHT].value = 0;    
			lights[LOWER_JINS_ACTIVE_LIGHT+1].value = 1;    
		}

        for(int i=0;i<MAX_UPPER_AJNAS;i++) {
			if(i < numberActiveAjnas) {
				if (upperAjnasActiveTrigger[i].process(params[UPPER_AJNAS_ACTIVE_PARAM+i].getValue())) {
					upperAjnasActive[i] = !upperAjnasActive[i];
				}
			}

			if(upperAjnasActive[i]) {
	            lights[UPPER_AJNAS_ACTIVE_LIGHT+i*2].value = clamp(params[UPPER_AJNAS_WEIGHT_PARAM+i].getValue() + (inputs[UPPER_AJNAS_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);;    
				lights[UPPER_AJNAS_ACTIVE_LIGHT+(i*2)+1].value = 0;    
			}
			else { 
				lights[UPPER_AJNAS_ACTIVE_LIGHT+(i*2)].value = 0;    
				lights[UPPER_AJNAS_ACTIVE_LIGHT+(i*2)+1].value = 1;    
			}
		}

        for(int i=0;i<MAX_COMPLETE_SCALE_SIZE;i++) {
            if (noteActiveTrigger[i].process( params[NOTE_ACTIVE_PARAM+i].getValue())) {
                noteActive[i] = !noteActive[i];
            }

			float userProbability;
			if(noteActive[i]) {
	            userProbability = clamp(params[NOTE_WEIGHT_PARAM+i].getValue() + (inputs[NOTE_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);    
				lights[NOTE_ACTIVE_LIGHT+(i*2)].value = userProbability;    
				lights[NOTE_ACTIVE_LIGHT+(i*2)+1].value = 0;    
			}
			else { 
				userProbability = 0.0;
				lights[NOTE_ACTIVE_LIGHT+(i*2)].value = 0;    
				lights[NOTE_ACTIVE_LIGHT+(i*2)+1].value = 1;    
			}

			actualNoteProbability[i] = noteInitialProbability[i] * userProbability; 
        }

		if( inputs[TRIGGER_INPUT].active ) {
			if (clockTrigger.process(inputs[TRIGGER_INPUT].value) ) {		
				float rnd = ((float) rand()/RAND_MAX);
				if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
					rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f;
				}	
			
				int randomNote = weightedProbability(actualNoteProbability,MAX_COMPLETE_SCALE_SIZE,params[WEIGHT_SCALING_PARAM].getValue(),rnd);
				if(randomNote == -1) { //Couldn't find a note, so find first active
					bool noteOk = false;
					int notesSearched = 0;
					randomNote = currentNote; 
					do {
						randomNote = (randomNote + 1) % MAX_COMPLETE_SCALE_SIZE;
						notesSearched +=1;
						noteOk = noteActive[randomNote] || notesSearched >= MAX_COMPLETE_SCALE_SIZE;
					} while(!noteOk);
				}


				probabilityNote = randomNote;

				double quantitizedNoteCV;
				int notePosition = randomNote;
				if(notePosition < 0) {
					notePosition += MAX_COMPLETE_SCALE_SIZE;
					octaveAdjust -=1;
				}


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


				quantitizedNoteCV =(currentJins.Intonation[notePosition] / 1200.0) + (tonic /12.0); 
				//Add offset if an upper Jins
				if(jinsIndex > 0) {
					 int upperJinsOverlapPosition = currentUpperAjnasOverlap[jinsIndex-1];
					 int positionInRootJins = rootJins.NumberLowerTones + upperJinsOverlapPosition - 1; //Subtract 1 since intonations are 0 indexed

					 float noteCVOffset = (rootJins.Intonation[positionInRootJins] / 1200.0);
					 quantitizedNoteCV += noteCVOffset;
				}
				quantitizedNoteCV += octaveIn + octave + octaveAdjust; 
				outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV + pitchRandomness);
				outputs[WEIGHT_OUTPUT].setVoltage(clamp((params[NOTE_WEIGHT_PARAM+randomNote].getValue() + (inputs[NOTE_WEIGHT_INPUT+randomNote].getVoltage() / 10.0f) * 10.0f),0.0f,10.0f));
				outputs[CURRENT_JINS_OUTPUT].setVoltage(jinsIndex);
				if(lastQuantizedCV != quantitizedNoteCV) {
					noteChangePulse.trigger(1e-3);	
					lastQuantizedCV = quantitizedNoteCV;
				}        
			}
			outputs[NOTE_CHANGE_OUTPUT].setVoltage(noteChangePulse.process(1.0 / args.sampleRate) ? 10.0 : 0);
		}

	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void ProbablyNoteArabic::onReset() {
	clockTrigger.reset();
	// for(int i = 0;i<MAX_SCALES;i++) {
	// 	for(int j=0;j<MAX_NOTES;j++) {
	// 		//scaleNoteWeighting[i][j] = defaultScaleNoteWeighting[i][j];
	// 	}
	// }
}




struct ProbablyNoteArabicDisplay : TransparentWidget {
	ProbablyNoteArabic *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	ProbablyNoteArabicDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}


    void drawTonic(const DrawArgs &args, Vec pos, int tonic) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		snprintf(text, sizeof(text), "%s", module->noteNames[tonic]);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawMaqam(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->currentMaqam.Name);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawFamily(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->rootJins.Name);
	
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawUpperAjnas(const DrawArgs &args, Vec pos, int numberUpperAnjas, int *upperAjnas) {

		float jinsPosition[MAX_UPPER_AJNAS][3] = {
			{150,135,NVG_ALIGN_LEFT},
			{186,150,NVG_ALIGN_CENTER},
			{202,220,NVG_ALIGN_LEFT},
			{152,248,NVG_ALIGN_LEFT},
			{81,220,NVG_ALIGN_RIGHT},
			{92,151,NVG_ALIGN_CENTER}
		};


		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		for(int i=0;i<numberUpperAnjas;i++) {
			if(upperAjnas[i] >= 0) {
				snprintf(text, sizeof(text), "%s", module->jins[upperAjnas[i]].Name);
				float x= jinsPosition[i][0];
				float y= jinsPosition[i][1];
				int align = (int)jinsPosition[i][2];

				nvgTextAlign(args.vg,align);
				nvgText(args.vg, x, y, text, NULL);
			}
		}
	}


	void drawNoteRange(const DrawArgs &args, float *noteInitialProbability, int numberMainTones, int numberLowerTones) 
	{		
		
		// Draw indicator
		for(int i = 0; i<MAX_COMPLETE_SCALE_SIZE;i++) {
			

			int actualTarget = (i ) % MAX_COMPLETE_SCALE_SIZE;

			float opacity = noteInitialProbability[actualTarget] * 255;
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			if(actualTarget == module->probabilityNote) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			}

			float y = 263;
			float x = (i*23)+3;
			float width = 23;

			if(i < 3) {
				y = 273;
				if(i == 0) {
					x = 5;
					width = 22;
				}
			} else if (i >= 3) {
				y = 268;
				if (i == 15) {
					//width = 27;
				}
			}

			nvgBeginPath(args.vg);
			nvgRect(args.vg,x,y,width,21);
			nvgFill(args.vg);

		}
	}

	void drawCurrentJins(const DrawArgs &args, int jinsIndex) 
	{		

		// Draw indicator
		for(int i = 0; i<MAX_UPPER_AJNAS;i++) {
			const float rotate90 = (M_PI) / 2.0;		
			//float opacity = noteInitialProbability[actualTarget] * 255;
			bool jinsUsed = module->currentAjnasStatus[i+1]; //should be +1	

			if(jinsUsed) {
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xbF));
				if(i == jinsIndex - 1) {
					nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, 0xcf));
				}
			} else 
			{
				nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0x00));
			}
			double startDegree = (M_PI * 2.0 * ((double)i - 0.5) / MAX_UPPER_AJNAS) - rotate90;
			double endDegree = (M_PI * 2.0 * ((double)i + 0.5) / MAX_UPPER_AJNAS) - rotate90;
			

			nvgBeginPath(args.vg);
			nvgArc(args.vg,140,190.5,65.0,startDegree,endDegree,NVG_CW);
			double x= cos(endDegree) * 65.0 + 140.0;
			double y= sin(endDegree) * 65.0 + 190.5;
			nvgLineTo(args.vg,x,y);
			nvgArc(args.vg,140,190.5,47.0,endDegree,startDegree,NVG_CCW);
			nvgClosePath(args.vg);		
			nvgFill(args.vg);
		
		}

		if(jinsIndex == 0) {
			nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, 0x4f));
			nvgBeginPath(args.vg);
			nvgRect(args.vg,6,240,58,22);
			nvgClosePath(args.vg);		
			nvgFill(args.vg);
		}


		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,148,127,40,10);
		nvgClosePath(args.vg);		
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,149,128,38,8);
		nvgClosePath(args.vg);		
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,168,142,40,10);
		nvgClosePath(args.vg);		
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,169,143,38,8);
		nvgClosePath(args.vg);		
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,199,213,40,10);
		nvgClosePath(args.vg);		
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,200,214,38,8);
		nvgClosePath(args.vg);		
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,150,241,40,10);
		nvgClosePath(args.vg);		
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,151,242,38,8);
		nvgClosePath(args.vg);		
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,43,213,40,10);
		nvgClosePath(args.vg);		
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,44,214,38,8);
		nvgClosePath(args.vg);		
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, 1.0);
		nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,73,144,40,10);
		nvgClosePath(args.vg);		
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgBeginPath(args.vg);
		nvgRect(args.vg,74,145,38,8);
		nvgClosePath(args.vg);		
		nvgFill(args.vg);


	}


	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		if(module->lastFamily == -1  || module->lastMaqamIndex == -1)
			return;

		drawFamily(args, Vec(10,82));
		drawMaqam(args, Vec(10,158));
		drawTonic(args, Vec(135,72), module->tonic);
		drawCurrentJins(args,module->jinsIndex);
		drawNoteRange(args, module->noteInitialProbability, module->currentJins.NumberMainTones, module->currentJins.NumberLowerTones);
		drawUpperAjnas(args, Vec(105,138), module->numberActiveAjnas, module->currentUpperAjnas);
	}
};

struct ProbablyNoteArabicWidget : ModuleWidget {
	PortWidget* inputs[MAX_COMPLETE_SCALE_SIZE];
	ParamWidget* weightParams[MAX_COMPLETE_SCALE_SIZE];
	ParamWidget* noteOnParams[MAX_COMPLETE_SCALE_SIZE];
	LightWidget* lights[MAX_COMPLETE_SCALE_SIZE];  


	ProbablyNoteArabicWidget(ProbablyNoteArabic *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteArabic.svg")));


		{
			ProbablyNoteArabicDisplay *display = new ProbablyNoteArabicDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));



		addInput(createInput<FWPortInSmall>(Vec(5, 345), module, ProbablyNoteArabic::NOTE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(30, 345), module, ProbablyNoteArabic::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(55, 345), module, ProbablyNoteArabic::EXTERNAL_RANDOM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(82, 345), module, ProbablyNoteArabic::MODULATION_TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(108, 345), module, ProbablyNoteArabic::JINS_SELECT_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(18,25), module, ProbablyNoteArabic::SPREAD_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(44,51), module, ProbablyNoteArabic::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(46, 29), module, ProbablyNoteArabic::SPREAD_INPUT));

        addParam(createParam<RoundSmallFWKnob>(Vec(78,25), module, ProbablyNoteArabic::SLANT_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(104,51), module, ProbablyNoteArabic::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(106, 29), module, ProbablyNoteArabic::SLANT_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(168, 25), module, ProbablyNoteArabic::FOCUS_PARAM));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(194,51), module, ProbablyNoteArabic::FOCUS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(196, 29), module, ProbablyNoteArabic::FOCUS_INPUT));


        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNoteArabic::FAMILY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,112), module, ProbablyNoteArabic::FAMILY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNoteArabic::FAMILY_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(8,161), module, ProbablyNoteArabic::MAQAM_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,187), module, ProbablyNoteArabic::MAQAM_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 165), module, ProbablyNoteArabic::MAQAM_INPUT));



        addParam(createParam<RoundSmallFWSnapKnob>(Vec(128,76), module, ProbablyNoteArabic::TONIC_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(154,102), module, ProbablyNoteArabic::TONIC_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(156, 80), module, ProbablyNoteArabic::TONIC_INPUT));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(188,86), module, ProbablyNoteArabic::OCTAVE_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(214,112), module, ProbablyNoteArabic::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(216, 90), module, ProbablyNoteArabic::OCTAVE_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(248,86), module, ProbablyNoteArabic::PITCH_RANDOMNESS_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(274,112), module, ProbablyNoteArabic::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(276, 90), module, ProbablyNoteArabic::PITCH_RANDOMNESS_INPUT));
		addParam(createParam<LEDButton>(Vec(250, 120), module, ProbablyNoteArabic::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(251.5, 121.5), module, ProbablyNoteArabic::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));



		addParam(createParam<TL1105>(Vec(15, 117), module, ProbablyNoteArabic::RESET_SCALE_PARAM));
		addParam(createParam<CKD6>(Vec(76, 85), module, ProbablyNoteArabic::MODULATE_PARAM));


        addParam(createParam<RoundReallySmallFWKnob>(Vec(28,241), module, ProbablyNoteArabic::LOWER_JINS_WEIGHT_PARAM));				
		addInput(createInput<FWPortInReallySmall>(Vec(50,244), module, ProbablyNoteArabic::LOWER_JINS_WEIGHT_INPUT));
		addParam(createParam<LEDButton>(Vec(8, 241), module, ProbablyNoteArabic::LOWER_JINS_ACTIVE_PARAM));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(9.5, 242.5), module, ProbablyNoteArabic::LOWER_JINS_ACTIVE_LIGHT));

		for(int i=0;i<MAX_UPPER_AJNAS;i++) {
			double position = 2.0 * M_PI / MAX_UPPER_AJNAS * i  - M_PI / 2.0; // Rotate 90 degrees

			double x= cos(position) * 34.0 + 130.0;
			double y= sin(position) * 34.0 + 180.5;

			//Rotate inputs 1 degrees
			addParam(createParam<RoundReallySmallFWKnob>(Vec(x,y), module, ProbablyNoteArabic::UPPER_AJNAS_WEIGHT_PARAM+i));			
			x= cos(position + (M_PI / 180.0 * 1.0)) * 16.0 + 134.0;
			y= sin(position + (M_PI / 180.0 * 1.0)) * 16.0 + 185.0;
			addInput(createInput<FWPortInReallySmall>(Vec(x, y), module, ProbablyNoteArabic::UPPER_AJNAS_WEIGHT_INPUT+i));

			//Rotate buttons 1 degrees
			x= cos(position - (M_PI / 180.0 * 1.0)) * 56.0 + 131.0;
			y= sin(position - (M_PI / 180.0 * 1.0)) * 56.0 + 181.0;
			addParam(createParam<LEDButton>(Vec(x, y), module, ProbablyNoteArabic::UPPER_AJNAS_ACTIVE_PARAM+i));
			addChild(createLight<LargeLight<GreenRedLight>>(Vec(x+1.5, y+1.5), module, ProbablyNoteArabic::UPPER_AJNAS_ACTIVE_LIGHT+i*2));

		}



//Move able
		PortWidget* weightInput;
		ParamWidget* weightParam;
		ParamWidget* noteOnParam;
		LightWidget* noteOnLight;


		noteOnParam = createParam<LEDButton>(Vec(6, 275), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+0);
		noteOnParams[0] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(7.5, 276.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+0); 
		lights[0] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(5,295), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+0);
		weightParams[0] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(9, 318), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+0);
		inputs[0] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(29, 275), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+1);
		noteOnParams[1] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(30.5, 276.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+2); 
		lights[1] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(28,295), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+1);
		weightParams[1] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(32, 318), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+1);
		inputs[1] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(52, 275), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+2);
		noteOnParams[2] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(53.5, 276.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+4); 
		lights[2] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(51,295), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+2);
		weightParams[2] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(55, 318), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+2);
		inputs[2] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(75, 270), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+3);
		noteOnParams[3] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(76.5, 271.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+6); 
		lights[3] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(74,290), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+3);
		weightParams[3] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(78, 313), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+3);
		inputs[3] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(98, 270), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+4);
		noteOnParams[4] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(99.5, 271.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+8); 
		lights[4] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(97,290), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+4);
		weightParams[4] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(101, 313), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+4);
		inputs[4] = weightInput;	
		addInput(weightInput);


		noteOnParam = createParam<LEDButton>(Vec(121, 270), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+5);
		noteOnParams[5] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(122.5, 271.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+10); 
		lights[5] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(120,290), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+5);
		weightParams[5] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(124, 313), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+5);
		inputs[5] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(144, 270), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+6);
		noteOnParams[6] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(145.5, 271.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+12); 
		lights[6] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(143,290), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+6);
		weightParams[6] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(147, 313), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+6);
		inputs[6] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(167, 270), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+7);
		noteOnParams[7] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(168.5, 271.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+14); 
		lights[7] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(166,290), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+7);
		weightParams[7] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(170, 313), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+7);
		inputs[7] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(190, 265), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+8);
		noteOnParams[8] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(191.5, 266.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+16); 
		lights[8] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(189,285), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+8);
		weightParams[8] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(193, 308), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+8);
		inputs[8] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(213, 265), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+9);
		noteOnParams[9] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(214.5, 266.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+18); 
		lights[9] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(212,285), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+9);
		weightParams[9] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(216, 308), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+9);
		inputs[9] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(236, 265), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+10);
		noteOnParams[10] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(237.5, 266.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+20); 
		lights[10] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(235,285), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+10);
		weightParams[10] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(239, 308), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+10);
		inputs[10] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(259, 260), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+11);
		noteOnParams[11] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(260.5, 261.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+22); 
		lights[11] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(258,280), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+11);
		weightParams[11] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(262, 303), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+11);
		inputs[11] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(282, 260), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+12);
		noteOnParams[12] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(283.5, 261.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+24); 
		lights[12] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(281,280), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+12);
		weightParams[12] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(285, 303), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+12);
		inputs[12] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(305, 260), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+13);
		noteOnParams[13] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(306.5, 261.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+26); 
		lights[13] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(304,280), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+13);
		weightParams[13] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(308, 303), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+13);
		inputs[13] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(328, 260), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+14);
		noteOnParams[14] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(329.5, 261.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+28); 
		lights[14] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(327,280), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+14);
		weightParams[14] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(331, 303), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+14);
		inputs[14] = weightInput;	
		addInput(weightInput);

		noteOnParam = createParam<LEDButton>(Vec(351, 260), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+15);
		noteOnParams[15] = noteOnParam;
		addParam(noteOnParam);
		noteOnLight = createLight<LargeLight<GreenRedLight>>(Vec(352.5, 261.5), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+30); 
		lights[15] = noteOnLight;
		addChild(noteOnLight);
		weightParam = createParam<RoundReallySmallFWKnob>(Vec(350,280), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+15);
		weightParams[15] = weightParam;
		addParam(weightParam);		
		weightInput = createInput<FWPortInReallySmall>(Vec(354, 303), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+15);
		inputs[15] = weightInput;	
		addInput(weightInput);



		addParam(createParam<RoundReallySmallFWKnob>(Vec(322,97), module, ProbablyNoteArabic::WEIGHT_SCALING_PARAM));		

		addOutput(createOutput<FWPortInSmall>(Vec(355, 345),  module, ProbablyNoteArabic::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(328, 345),  module, ProbablyNoteArabic::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(300, 345),  module, ProbablyNoteArabic::NOTE_CHANGE_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(275, 345),  module, ProbablyNoteArabic::CURRENT_JINS_OUTPUT));

	}


	// int NumberMainTones;
	// int NumberLowerTones;
	// int NumberExtendedTones;

	void step() override {
		if (module) {
			int jinsIndex = (((ProbablyNoteArabic*)module)->jinsIndex);
			for(int i = 0; i < MAX_COMPLETE_SCALE_SIZE; i++) {							
				if(jinsIndex > 0 && i >= 2 + (((ProbablyNoteArabic*)module)->currentMaqam.OverlapPoint[jinsIndex-1]) && i < 2 + (((ProbablyNoteArabic*)module)->currentMaqam.OverlapPoint[jinsIndex-1]) + (((ProbablyNoteArabic*)module)->currentJins.NumberMainTones)) {
					noteOnParams[i]->box.pos.y = 265;
					lights[i]->box.pos.y = 266.5;
					weightParams[i]->box.pos.y = 285;
					inputs[i]->box.pos.y = 308;
				} else if(i > 2) {
					noteOnParams[i]->box.pos.y = 270;
					lights[i]->box.pos.y = 271.5;
					weightParams[i]->box.pos.y = 290;
					inputs[i]->box.pos.y = 313;
				} else {
					noteOnParams[i]->box.pos.y = 275;
					lights[i]->box.pos.y = 276.5;
					weightParams[i]->box.pos.y = 295;
					inputs[i]->box.pos.y = 318;
				}
			}
		}
		Widget::step();
	}
};



// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteArabic = createModel<ProbablyNoteArabic, ProbablyNoteArabicWidget>("ProbablyNoteArabic");
    