#include "params.h"
#include "keyboard.h"
#include <iomanip>
#include <sstream>

#ifdef PLUGIN_MODE
void Param::StartParamChange(tsl::AppState *_appState, int tindex)
{
    // Increment, fetch the old value
    int old = paramChanging[tindex].fetch_add(1, std::memory_order_acq_rel);

    if (old == 0)
    {
        // We transitioned 0 -> 1
        _STATE->BeginEndInformHostOfParamChange(tindex, pluginIndex, false);
    }
}

void Param::SetParamValue(tsl::AppState* _appState, int tindex, double value)
{
    _STATE->InformHostOfParamChange(tindex, pluginIndex, toNormalized(_STATE->sr, value));
}

void Param::EndParamChange(tsl::AppState* _appState, int tindex)
{
    // Decrement, fetch the old value
    int old = paramChanging[tindex].fetch_sub(1, std::memory_order_acq_rel);

    if (old == 1)
    {
        // We transitioned 1 -> 0
        _STATE->BeginEndInformHostOfParamChange(tindex, pluginIndex, true);
    }
}

#endif

int Param::inputType() const {
    int t = tsl::graphics::TextInput::TYPE_CLASS_NUMBER;
    if (min < 0 || max < 0)
        t |= tsl::graphics::TextInput::TYPE_NUMBER_FLAG_SIGNED;

    if (digits > 0)
        t |= tsl::graphics::TextInput::TYPE_NUMBER_FLAG_DECIMAL;
    return t;
}

MYFLOAT  Param::toDisplay(int sr, MYFLOAT  value) const {
    if (type == ParameterType_enum) {
        if(values != nullptr){
            for(int i = 0; i<numValues;i++){
                if(values[i]==value)return (MYFLOAT)i;
            }
		}
		return  value;
    }

    if (logarithmic) {
        value = pow(10, value * .05);
    }

    if (convertms) {
        const MYFLOAT samplesperms = sr * 0.001;
        value = value / samplesperms;
    }
    return value - offset;
}

MYFLOAT  Param::fromDisplay(int sr, MYFLOAT  val) const {
    if (type == ParameterType_enum) {
        if (values != nullptr && val<numValues) {
            return values[(int)val];
        }
        return val;
    }

    val += offset;
    if (convertms) val = val * sr / 1000.f;
    if (logarithmic) val = LOG10D20F(val);
    if (castint) val = std::round(val);
    return val;
}

MYFLOAT  Param::getMin(int sr) const {
    return toDisplay(sr, min);
}

MYFLOAT  Param::getMax(int sr) const {
    return toDisplay(sr, max);
}

std::string Param::toString(float value) const {
    std::stringstream str;
    str << std::fixed << std::setprecision(digits) << value;
    return str.str();
}

MYFLOAT Param::toNormalized(int sr, MYFLOAT val) {
    if(type == ParameterType_enum){
        if(values != nullptr){
            for(int i = 0; i<numValues;i++){
                if(values[i]==val)return (MYFLOAT)i/(MYFLOAT)(numValues-1);
            }
        }
        return val/(MYFLOAT)(numValues-1);
	}
    else if(type == ParameterType_bool){
        return val>0?1.f:0.f;
	}
    if(logarithmic){
#ifdef PLUGIN_MODE
        return std::pow((toDisplay(sr, val) - toDisplay(sr, min)) / (toDisplay(sr, max) - toDisplay(sr, min)), 1.0 / 4.0);
#else
     return (toDisplay(sr, val) - toDisplay(sr, min)) / (toDisplay(sr, max) - toDisplay(sr, min));
#endif        //return pow(DISTANCE(min, val) / DISTANCE(min, max), 1.0f / 4.0f);
    }
    return DISTANCE(min, val) / DISTANCE(min, max);
}
MYFLOAT Param::fromNormalized(MYFLOAT normalized) {
    if (type == ParameterType_enum) {
        if (values != nullptr) {
            int idx = (int)std::round(normalized * (MYFLOAT)(numValues - 1));
            if (idx < 0)idx = 0;
            if (idx >= numValues)idx = numValues - 1;
            return values[idx];
        }
        return std::round(normalized * (MYFLOAT)(numValues - 1));
    }
    else if(type == ParameterType_bool){
        return normalized>=0.5f?1.f:0.f;
	}
    return min + normalized * DISTANCE(min, max);
}

MYFLOAT  Param::toDSP(int sr, MYFLOAT  value){
    if (logarithmic) {
        value = pow(10, value * .05);
    }
    if(castint) value =  std::round(value);

    if (convertms) {
        const MYFLOAT samplesperms = sr * 0.001;
        value = value * samplesperms;
    }

    return value - offset;

};

int numDigits(int number) {
    int digits = 0;
    while (number) {
        number /= 10;
        digits++;
    }
    return digits;
}


float Param::getDisplayNameWidth(float maxCharWidth){
    return (valuename == nullptr || valuename[0] == ' ') ? 1 * maxCharWidth: (strlen(valuename) +1) * maxCharWidth;
}


float Param::getDisplayWidthValue(double sr, float maxCharWidth){
    auto mind = getMin(sr), maxd = getMax(sr);
    int digs = 0;
    if(mind<0||maxd<0)digs++;
    if(digits)digs++;
    digs+=digits;
    int mindigs = numDigits(std::abs(mind)),maxdigs = numDigits(std::abs(maxd));
    digs += (mindigs > maxdigs) ? mindigs : maxdigs;
    return (digs+1) * maxCharWidth;
}
