#include "windowFunction.cpp"

struct WindowSet {

    int numberWindows;
    int windowSize;
    int sampleSize;
    int windowOverlap;
    int windowFunction;

    int sampleIndex;

    WindowFunction windowFunction = new WindowFunction();

    WindowSet(int numberWindows_, int sampleSize_, int windowSize_, int windowOverlap_, int windowFunction_) {
        numberWindows = numberWindows_;
        sampleSize = sampleSize_;
        windowSize = windowSize_;
        windowOverlap = windowOverlap_;
        windowFunction = windowFunction_;
    }

    void push(float in) {
        
        for(int windowIndex = 0; windowIndex < numberWindows;windowIndex++) {
            int phase = sampleIndex + (windowIndex * windowOverlap);
        }


        sampleIndex = (sampleIndex++) % sampleSize;

    }
    
}