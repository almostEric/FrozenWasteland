#pragma once

#define MAX_STEPS 73


struct ChristoffelWords {

    uint8_t maxStringLength = 73;;

    std::string Generate(uint8_t length, uint8_t smallSteps) {

        maxStringLength = length;
        //fprintf(stderr, "Finding c word %hu %hu \n", length,smallSteps);
        std::string output = "unknown";
        std::string initial = "ls";
        if(length <= 2) 
            return initial;

        std::string lastList[300];
        std::string newList[300];
        uint8_t lastListCount;
        uint8_t newListCount;

        lastList[0] = initial;
        lastListCount = 1;
        for(int i=0;i<MAX_STEPS;i++) {
            newListCount = 0;
            for(int j=0;j<lastListCount;j++) {
                std::string s1 = Morph1(lastList[j]); 
                if(s1.length() == length) {
                    if(CountS(s1) == smallSteps) {
                        return s1;
                    }
                } else if (s1.length() < length) {
                    newList[newListCount] = s1;
                    newListCount++;
                }

                std::string s2 = Morph2(lastList[j]); 
                if(s2.length() == length) {
                    if(CountS(s2) == smallSteps) {
                        return s2;
                    }
                } else if (s2.length() < length) {
                    newList[newListCount] = s2;
                    newListCount++;
                }
            }
            std::swap(newList,lastList);
            lastListCount = newListCount;
        }

        return output;
    }

    int CountS(std::string input) {
        int sCount = 0;
        for(uint16_t i=0;i<input.length();i++) {
            if(input[i] == 's')
                sCount++;
        }
        return sCount;
    }


    std::string Morph1(std::string input) {

        std::string output = "";
        output.reserve(maxStringLength);

        for(uint16_t i=0;i<input.length();i++) {
            if(input[i] == 'l') {
                //output += "ls";
                output.append("ls");
            } else {
                //output += "l";
                output.append("l");
            }
        }

        return output;
    }

    std::string Morph2(std::string input) {
        std::string output = "";
        output.reserve(maxStringLength);

        for(uint16_t i=0;i<input.length();i++) {
            if(input[i] == 'l') {
                output.append("ls");
            } else {
                output.append("s");
            }
        }
        return output;

    }
};