
struct WindowFunction {


	enum WindowFunctionTypes {
		HANNING_WINDOW_FUNCTION,
		BLACKMAN_WINDOW_FUNCTION,
	};


    float process(int windowType, float phase) {
        float returnValue = 0.0;
        switch(windowType) {
            case HANNING_WINDOW_FUNCTION :
                returnValue = HanningWindow(phase);
                break;
            case BLACKMAN_WINDOW_FUNCTION :
                returnValue = BlackmanWindow(phase);
                break;
            default:
                break;
        }


        return returnValue;
    }

	float HanningWindow(float phase) {
		return 0.5f * (1 - cosf(2 * M_PI * phase));
	}

	float BlackmanWindow(float phase) {
		float a0 = 0.42;
		float a1 = 0.5;
		float a2 = 0.08;
		return a0 - (a1 * cosf(2 * M_PI * phase)) + (a2 * cosf(4 * M_PI * phase)) ;
	}
}