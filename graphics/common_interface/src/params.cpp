#include "defines.h"
#include "params.h"
#include "InputEvent.h"
#include <iomanip>
#include <sstream>

int Param::inputType() const {
    uint32_t t = tsl::graphics::KeyboardType::TYPE_CLASS_NUMBER;
    if (min < 0 || max < 0)
        t |= tsl::graphics::KeyboardType::TYPE_NUMBER_FLAG_SIGNED;

    if (digits > 0)
        t |= tsl::graphics::KeyboardType::TYPE_NUMBER_FLAG_DECIMAL;
    return t;
}

MYFLOAT  Param::toDisplay(int sr, MYFLOAT  value) const {
    if (type == ParameterType_enum) {
            for(int i = 0; i<values.size();i++)
                if(values[i]==value)return (MYFLOAT)i;
        return  value;
    }

    switch (paramCurve) {
        case ParamCurve::Linear:
            break;
        case ParamCurve::Log10:
            value = pow(10, value * .05);
            value =  std::max((MYFLOAT)0, value - offset);
            if (flags & ConvertMs) {
                const MYFLOAT samplesperms = sr * 0.001;
                value = value / samplesperms;
            }

            return value;
        case ParamCurve::Octave:
            value = std::pow(2.0, value * 8.0 - 4.0);
            break;
	}

    if (flags & ConvertMs) {
        const MYFLOAT samplesperms = sr * 0.001;
        value = value / samplesperms;
    }
    return value - offset;
}

MYFLOAT  Param::fromDisplay(int sr, MYFLOAT  val) const {
    if (type == ParameterType_enum) {
        if (val<values.size()) {
            return values[(int)val];
        }
        return val;
    }

    if (flags & ConvertMs) val = val * sr / 1000.;

    switch (paramCurve) {
    case ParamCurve::Linear:
        val += offset;
        break;
    case ParamCurve::Log10:
        val += offset;
        val = LOG10D20(val);
        break;
    case ParamCurve::Octave:
        val += offset;
        val = (std::log2(val) + 4.0) / 8.0;
        break;
    }
    if (flags & CastInt) val = std::round(val);
    return val;
}



MYFLOAT Param::toNormalized(MYFLOAT val) {
    // ENUM
    if (type == ParameterType_enum) {
        if (!values.empty()) {
            for (int i = 0; i < values.size(); i++)
                if (values[i] == val)
                    return (MYFLOAT)i / (MYFLOAT)(values.size() - 1);
        }
        if (!names.empty())
            return val / (MYFLOAT)(names.size() - 1);
        return 0;
    }

    // BOOL
    if (type == ParameterType_bool)
        return val > 0 ? 1.f : 0.f;

    return (val - min) / (max - min);
}

MYFLOAT Param::fromNormalized(MYFLOAT normalized) {
    // ENUM
    if (type == ParameterType_enum) {
        if (!values.empty()) {
            int idx = (int)std::round(normalized * (MYFLOAT)(values.size() - 1));
            idx = std::clamp(idx, 0, (int)values.size() - 1);
            return values[idx];
        }
        return std::round(normalized * (MYFLOAT)(names.size() - 1));
    }

    // BOOL
    if (type == ParameterType_bool)
        return normalized >= 0.5f ? 1.f : 0.f;

    return min + normalized * (max - min);
}



MYFLOAT  Param::toDSP(int sr, MYFLOAT  value) {
    if (paramCurve == ParamCurve::Log10) {
        value = pow(10, value * .05);
    }
    else if (paramCurve == ParamCurve::Octave) {
        value = std::pow(2.0, value * 8.0 - 4.0);
    }
    if (flags & CastInt) value = std::round(value);

    if (flags & ConvertMs) {
        const MYFLOAT samplesperms = sr * 0.001;
        value = value * samplesperms;
    }

    return value - offset;

};


MYFLOAT  Param::getMin(int sr) const {
    if (type == ParameterType_enum) {
        return 0;
    }
    return toDisplay(sr, min);
}
MYFLOAT  Param::getMax(int sr) const {
    if (type == ParameterType_enum) {
        if (names.size())return names.size();
        else return values.size();
    }
    return toDisplay(sr, max);
}

std::string Param::toString(float value) const {
    std::stringstream str;
    str << std::fixed << std::setprecision(digits) << value;
    return str.str();
}


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

tsl::parameters::Event tsl::parameters::Event::createEvent(uint8_t trackIndex, uint8_t eventType, uint16_t paramIndex, double value, uint8_t subType, uint16_t groudId, uint8_t flags) {
    Event e;
    e.eventType = eventType;
    e.trackIndex = trackIndex;
    e.paramIndex = paramIndex;
    e.value = value;
    e.subType = subType;
    e.groupId = groudId;
    e.flags = flags;
    return e;
}

tsl::parameters::Event tsl::parameters::Event::createRerenderEvent(uint8_t trackIndex, uint16_t paramIndex, uint16_t groudId) {
    Event e;
    e.eventType = Eventtype::Rerender;
    e.trackIndex = trackIndex;
    e.paramIndex = paramIndex;
    e.value = 1.0;
    e.subType = 0;
    e.groupId = groudId;
    e.flags = Event::Redraw | Event::Info | Event::History;
    return e;
}

tsl::parameters::Event tsl::parameters::Event::createTextEvent(uint8_t trackIndex, const char* text, uint16_t paramIndex, uint8_t subType, uint16_t groudId) {
    Event e;
    e.eventType = Eventtype::TextEvent;
    e.trackIndex = trackIndex;
    e.paramIndex = paramIndex;
    e.subType = subType;
    e.groupId = groudId;
    e.flags = Event::Redraw | Event::Info | Event::History;
    e.text = text;
    return e;
}
