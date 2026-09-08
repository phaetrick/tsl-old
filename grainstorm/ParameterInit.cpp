#include "ParameterInit.h"
#include "defines.h"
#include "types.h"
#include <gs_common.h>
#include "Convolver.h"
#include "pv.h"
#include "chorus.h"
#include "vco.h"
#include "pitchmap.h"
#include "grainstorm.h"
#include <app.h>

static void initControl(tsl::AppState* _appState, int32_t id, const char* name, const char* valuename,
	MYFLOAT min, MYFLOAT max,
	MYFLOAT initvalue, MYFLOAT progress, int32_t digits, Param::ParamCurve paramCurve = Param::ParamCurve::Linear) {
	_STATE->parameters[id].min = paramCurve == Param::ParamCurve::Log10 ? LOG10D20F(min) : min;
	_STATE->parameters[id].max = paramCurve == Param::ParamCurve::Log10 ? LOG10D20F(max) : max;
	_STATE->parameters[id].name = name;
	_STATE->parameters[id].valuename = valuename;
	_STATE->parameters[id].paramCurve = paramCurve;
	_STATE->parameters[id].type = ParameterType_double;
	_STATE->parameters[id].digits = digits;
	_STATE->parameters[id].initvalue = paramCurve == Param::ParamCurve::Log10 ? LOG10D20F(initvalue) : initvalue;
	_STATE->parameters[id].progress = progress;
	_STATE->parameters[id].flags |= Param::MidiParam;
}
static constexpr std::string_view stoppedNames[] = { "ON", "OFF" };


void tsl::initParams(tsl::AppState* _appState) {
	const char* bounda = "BOUNDA", * boundb = "BOUND B";
	const char* nameSyncing = "SYNCING";
	const char* nameSync = "SYNC";
	const char* nameSyncFact = "SYNC FACTOR";
	const char* nameVelup = "VEL UP";
	const char* nameVelDown = "VEL DOWN";
	const char* namePower = "POWER";
	const char* nameDelay = "DELAY";
	const char* mDelayCat = "MULTI DELAY";
	const char* mDelaySubCat[] = { "DELAY1", "DELAY2", "DELAY3", "DELAY4", "DELAY5", "DELAY6", "DELAY7", "DELAY8" };
	const int mdelayNumParams = MDELAY2DEL - MDELAY1DEL;

	for (int i = 0; i < 8; i++) {
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].min = LOG10D20F(5.);
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].max = LOG10D20F(1000.0f);
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].name = nameDelay;
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].valuename = "ms";
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].type = ParameterType_double;
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].digits = 1;
		// Octave series: 1000, 500, 250 ... 7.8125 ms. This MUST stay identical to
		// the values TRACK::init() seeds into params[][], or the two disagree and
		// the snapshot silently drops these params -- it only records an event
		// when a value differs from the declared default, so a save/restore then
		// moved every tap onto the declared default instead of what was playing.
		// It used to be the harmonic series 1000/(2*(i+1)), which coincided with
		// the seeded value only at tap 4.
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].initvalue = LOG10D20F(1000. / static_cast<double>(1 << i));
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].setFlag(Param::OwnInitValue, true);
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].progress = .125;
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].category = mDelayCat;
		_STATE->parameters[MDELAY1DEL + i * mdelayNumParams].subcategory = mDelaySubCat[i];


		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].min = -1.;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].max = 1.;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].name = "FB";
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].valuename = " ";
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].type = ParameterType_double;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].digits = 2;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].initvalue = .75;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].progress = .01;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].category = mDelayCat;
		_STATE->parameters[MDELAY1FB + i * mdelayNumParams].subcategory = mDelaySubCat[i];


		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].min = LOG10D20F(18.);
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].max = LOG10D20F(20000.);
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].name = "HP CUT";
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].valuename = "Hz";
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].type = ParameterType_double;
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].initvalue = LOG10D20F(18.);
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].progress = .25;
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].category = mDelayCat;
		_STATE->parameters[MDELAY1HP + i * mdelayNumParams].subcategory = mDelaySubCat[i];

		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].min = LOG10D20F(18.);
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].max = LOG10D20F(20000.);
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].name = "LP CUT";
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].valuename = "Hz";
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].type = ParameterType_double;
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].initvalue = LOG10D20F(20000.);
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].progress = .25;
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].category = mDelayCat;
		_STATE->parameters[MDELAY1LP + i * mdelayNumParams].subcategory = mDelaySubCat[i];

		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].min = -60;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].max = 60.;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].name = "GAIN";
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].valuename = "dB";
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].type = ParameterType_double;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].initvalue = -6;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].progress = 1;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].category = mDelayCat;
		_STATE->parameters[MDELAY1GAIN + i * mdelayNumParams].subcategory = mDelaySubCat[i];

		_STATE->parameters[MDELAY1HOLD + i * mdelayNumParams].name = "FREEZE";
		_STATE->parameters[MDELAY1HOLD + i * mdelayNumParams].type = ParameterType_bool;
		_STATE->parameters[MDELAY1HOLD + i * mdelayNumParams].flags |= Param::NoAssignment;
		_STATE->parameters[MDELAY1HOLD + i * mdelayNumParams].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1HOLD + i * mdelayNumParams].category = mDelayCat;
		_STATE->parameters[MDELAY1HOLD + i * mdelayNumParams].subcategory = mDelaySubCat[i];

		_STATE->parameters[MDELAY1BACKW + i].name = "BACKW";
		_STATE->parameters[MDELAY1BACKW + i].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1BACKW + i].type = ParameterType_bool;
		_STATE->parameters[MDELAY1BACKW + i].category = mDelayCat;
		_STATE->parameters[MDELAY1BACKW + i].subcategory = mDelaySubCat[i];

		_STATE->parameters[MDELAY1_POW + i].name = namePower;
		_STATE->parameters[MDELAY1_POW + i].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1_POW + i].type = ParameterType_bool;
		_STATE->parameters[MDELAY1_POW + i].category = mDelayCat;
		_STATE->parameters[MDELAY1_POW + i].subcategory = mDelaySubCat[i];

		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].name = nameSyncing;
		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].category = mDelayCat;
		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].subcategory = mDelaySubCat[i];
		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].flags |= Param::MidiParam;
		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].type = ParameterType_bool;
		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].flags |= Param::NoAssignment;
		_STATE->parameters[MDELAY1_CONTROLS_ACTIVE + i].setFlag(Param::NoAssignment, true);

		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].category = mDelayCat;
		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].subcategory = mDelaySubCat[i];
		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].name = nameSyncFact;
		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].initvalue = 2.0;
		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].flags |= Param::NoAssignment;
		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].setFlag(Param::NoAssignment, true);
		_STATE->parameters[MDELAY1_SYNC_FACTOR + i].type = ParameterType_bool;

		_STATE->parameters[MDELAY1_SYNC + i].category = mDelayCat;
		_STATE->parameters[MDELAY1_SYNC + i].subcategory = mDelaySubCat[i];
		_STATE->parameters[MDELAY1_SYNC + i].name = nameSync;
		_STATE->parameters[MDELAY1_SYNC + i].type = ParameterType_bool;
		_STATE->parameters[MDELAY1_SYNC + i].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

		_STATE->parameters[MDELAY1_FAST + i].category = mDelayCat;
		_STATE->parameters[MDELAY1_FAST + i].subcategory = mDelaySubCat[i];
		_STATE->parameters[MDELAY1_FAST + i].name = nameVelup;
		_STATE->parameters[MDELAY1_FAST + i].type = ParameterType_bool;
		_STATE->parameters[MDELAY1_FAST + i].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

		_STATE->parameters[MDELAY1_SLOW + i].category = mDelayCat;
		_STATE->parameters[MDELAY1_SLOW + i].subcategory = mDelaySubCat[i];
		_STATE->parameters[MDELAY1_SLOW + i].name = nameVelDown;
		_STATE->parameters[MDELAY1_SLOW + i].type = ParameterType_bool;
		_STATE->parameters[MDELAY1_SLOW + i].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	}

	const uint16_t mdelayMultisNumMdelay[] = { MDELAY1DEL, MDELAY1FB, MDELAY1HP, MDELAY1LP, MDELAY1GAIN, MDELAY1HOLD };
	const uint16_t mdelayMultis1[] = {
		MDELAY1_CONTROLS_ACTIVE,
		MDELAY1BACKW,
		MDELAY1_POW,
		MDELAY1_CONTROLS_ACTIVE,
		MDELAY1_SYNC_FACTOR,
		MDELAY1_SYNC,
		MDELAY1_FAST,
		MDELAY1_SLOW,
	};

	for (auto m : mdelayMultisNumMdelay) {
		_STATE->parameters[m].paramOffset = ASMDEL;
		_STATE->parameters[m].offsetFact = mdelayNumParams;
	};
	for (auto m : mdelayMultis1) {
		_STATE->parameters[m].paramOffset = ASMDEL;
	}



	_STATE->parameters[MDELAY1_POW].initvalue = 1.0;

	_STATE->parameters[MDELAY_SYNC].category = mDelayCat;
	_STATE->parameters[MDELAY_SYNC].name = nameSync;
	_STATE->parameters[MDELAY_SYNC].type = ParameterType_bool;
	_STATE->parameters[MDELAY_SYNC].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[MDELAY_FAST].category = mDelayCat;
	_STATE->parameters[MDELAY_FAST].name = nameVelup;
	_STATE->parameters[MDELAY_FAST].type = ParameterType_bool;
	_STATE->parameters[MDELAY_FAST].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[MDELAY_SLOW].category = mDelayCat;
	_STATE->parameters[MDELAY_SLOW].name = nameVelDown;
	_STATE->parameters[MDELAY_SLOW].type = ParameterType_bool;
	_STATE->parameters[MDELAY_SLOW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[MDELAY_MODE].category = mDelayCat;
	_STATE->parameters[MDELAY_MODE].name = "MODE";
	_STATE->parameters[MDELAY_MODE].type = ParameterType_enum;
	_STATE->parameters[MDELAY_MODE].names = mdelaymodes;
	_STATE->parameters[MDELAY_MODE].flags |= Param::MidiParam;

	_STATE->parameters[MDELAYPOW].category = mDelayCat;
	_STATE->parameters[MDELAYPOW].name = namePower;
	_STATE->parameters[MDELAYPOW].flags |= Param::MidiParam;
	_STATE->parameters[MDELAYPOW].type = ParameterType_bool;

	_STATE->parameters[MDELAYGAIN].min = -60;
	_STATE->parameters[MDELAYGAIN].max = 20.;
	_STATE->parameters[MDELAYGAIN].name = "GAIN";
	_STATE->parameters[MDELAYGAIN].valuename = "dB";
	_STATE->parameters[MDELAYGAIN].type = ParameterType_double;
	_STATE->parameters[MDELAYGAIN].initvalue = 0;
	_STATE->parameters[MDELAYGAIN].progress = 1;
	_STATE->parameters[MDELAYGAIN].flags |= Param::MidiParam;
	_STATE->parameters[MDELAYGAIN].category = mDelayCat;

	_STATE->parameters[MDELAYMIX].min = 0;
	_STATE->parameters[MDELAYMIX].max = 1.;
	_STATE->parameters[MDELAYMIX].name = "MIX";
	_STATE->parameters[MDELAYMIX].valuename = " ";
	_STATE->parameters[MDELAYMIX].type = ParameterType_double;
	_STATE->parameters[MDELAYMIX].digits = 2;
	_STATE->parameters[MDELAYMIX].initvalue = .5;
	_STATE->parameters[MDELAYMIX].progress = .01;
	_STATE->parameters[MDELAYMIX].flags |= Param::MidiParam;
	_STATE->parameters[MDELAYMIX].category = mDelayCat;

	_STATE->parameters[DELAYDEL].category = nameDelay;
	_STATE->parameters[DELAYDEL].min = LOG10D20F(5.);
	_STATE->parameters[DELAYDEL].max = LOG10D20F(1000.0);
	_STATE->parameters[DELAYDEL].name = nameDelay;
	_STATE->parameters[DELAYDEL].valuename = "ms";
	_STATE->parameters[DELAYDEL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[DELAYDEL].type = ParameterType_double;
	_STATE->parameters[DELAYDEL].digits = 1;
	_STATE->parameters[DELAYDEL].initvalue = LOG10D20F(500.);
	_STATE->parameters[DELAYDEL].progress = .125;
	_STATE->parameters[DELAYDEL].flags |= Param::MidiParam;

	_STATE->parameters[DELAYFB].category = nameDelay;
	_STATE->parameters[DELAYFB].min = -1.;
	_STATE->parameters[DELAYFB].max = 1.;
	_STATE->parameters[DELAYFB].name = "FB";
	_STATE->parameters[DELAYFB].valuename = " ";
	_STATE->parameters[DELAYFB].type = ParameterType_double;
	_STATE->parameters[DELAYFB].digits = 2;
	_STATE->parameters[DELAYFB].initvalue = .75;
	_STATE->parameters[DELAYFB].progress = .01;
	_STATE->parameters[DELAYFB].flags |= Param::MidiParam;


	_STATE->parameters[DELAYHP].category = nameDelay;
	_STATE->parameters[DELAYHP].min = LOG10D20F(18.);
	_STATE->parameters[DELAYHP].max = LOG10D20F(20000.);
	_STATE->parameters[DELAYHP].name = "HP CUT";
	_STATE->parameters[DELAYHP].valuename = "Hz";
	_STATE->parameters[DELAYHP].type = ParameterType_double;
	_STATE->parameters[DELAYHP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[DELAYHP].initvalue = LOG10D20F(18.);
	_STATE->parameters[DELAYHP].progress = .25;
	_STATE->parameters[DELAYHP].flags |= Param::MidiParam;

	_STATE->parameters[DELAYLP].category = nameDelay;
	_STATE->parameters[DELAYLP].min = LOG10D20F(18.);
	_STATE->parameters[DELAYLP].max = LOG10D20F(20000.);
	_STATE->parameters[DELAYLP].name = "LP CUT";
	_STATE->parameters[DELAYLP].valuename = "Hz";
	_STATE->parameters[DELAYLP].type = ParameterType_double;
	_STATE->parameters[DELAYLP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[DELAYLP].initvalue = LOG10D20F(20000);
	_STATE->parameters[DELAYLP].progress = .25;
	_STATE->parameters[DELAYLP].flags |= Param::MidiParam;

	_STATE->parameters[DELAYSHIFT2].category = nameDelay;
	_STATE->parameters[DELAYSHIFT2].min = -12.;
	_STATE->parameters[DELAYSHIFT2].max = 12.;
	_STATE->parameters[DELAYSHIFT2].name = "SHIFT";
	_STATE->parameters[DELAYSHIFT2].valuename = "Semitones";
	_STATE->parameters[DELAYSHIFT2].type = ParameterType_double;
	_STATE->parameters[DELAYSHIFT2].digits = 1;
	_STATE->parameters[DELAYSHIFT2].initvalue = 1;
	_STATE->parameters[DELAYSHIFT2].progress = 1;
	_STATE->parameters[DELAYSHIFT2].flags |= Param::MidiParam;

	_STATE->parameters[DELAYSHIFTMIX].category = nameDelay;
	_STATE->parameters[DELAYSHIFTMIX].min = 0.;
	_STATE->parameters[DELAYSHIFTMIX].max = 1.;
	_STATE->parameters[DELAYSHIFTMIX].name = "SHMIX";
	_STATE->parameters[DELAYSHIFTMIX].valuename = " ";
	_STATE->parameters[DELAYSHIFTMIX].type = ParameterType_double;
	_STATE->parameters[DELAYSHIFTMIX].digits = 2;
	_STATE->parameters[DELAYSHIFTMIX].initvalue = 0;
	_STATE->parameters[DELAYSHIFTMIX].progress = .05;
	_STATE->parameters[DELAYSHIFTMIX].flags |= Param::MidiParam;


	_STATE->parameters[DELAYDRY].category = nameDelay;
	_STATE->parameters[DELAYDRY].min = -60;
	_STATE->parameters[DELAYDRY].max = 60.;
	_STATE->parameters[DELAYDRY].name = "DRY";
	_STATE->parameters[DELAYDRY].valuename = "dB";
	_STATE->parameters[DELAYDRY].type = ParameterType_double;
	_STATE->parameters[DELAYDRY].initvalue = -6;
	_STATE->parameters[DELAYDRY].progress = 1;
	_STATE->parameters[DELAYDRY].flags |= Param::MidiParam;

	_STATE->parameters[DELAYWET].category = nameDelay;
	_STATE->parameters[DELAYWET].min = -60;
	_STATE->parameters[DELAYWET].max = 60.;
	_STATE->parameters[DELAYWET].name = "WET";
	_STATE->parameters[DELAYWET].valuename = "dB";
	_STATE->parameters[DELAYWET].type = ParameterType_double;
	_STATE->parameters[DELAYWET].initvalue = -6;
	_STATE->parameters[DELAYWET].progress = 1;
	_STATE->parameters[DELAYWET].flags |= Param::MidiParam;

	_STATE->parameters[DELAYHOLD].category = nameDelay;
	_STATE->parameters[DELAYHOLD].name = "FREEZE";
	_STATE->parameters[DELAYHOLD].type = ParameterType_bool;
	_STATE->parameters[DELAYHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	_STATE->parameters[DELAYBACKW].category = nameDelay;
	_STATE->parameters[DELAYBACKW].name = "BACKW";
	_STATE->parameters[DELAYBACKW].type = ParameterType_bool;
	_STATE->parameters[DELAYBACKW].flags |= Param::MidiParam;

	_STATE->parameters[DELAY_CONTROLS_ACTIVE].category = nameDelay;
	_STATE->parameters[DELAY_CONTROLS_ACTIVE].name = nameSyncing;
	_STATE->parameters[DELAY_CONTROLS_ACTIVE].flags |= Param::MidiParam;
	_STATE->parameters[DELAY_CONTROLS_ACTIVE].type = ParameterType_bool;
	_STATE->parameters[DELAY_CONTROLS_ACTIVE].flags |= Param::NoAssignment;


	_STATE->parameters[DELAY_SYNC].category = nameDelay;
	_STATE->parameters[DELAY_SYNC].name = nameSync;
	_STATE->parameters[DELAY_SYNC].type = ParameterType_bool;
	_STATE->parameters[DELAY_SYNC].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[DELAY_FAST].category = nameDelay;
	_STATE->parameters[DELAY_FAST].name = nameVelup;
	_STATE->parameters[DELAY_FAST].type = ParameterType_bool;
	_STATE->parameters[DELAY_FAST].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[DELAY_SLOW].category = nameDelay;
	_STATE->parameters[DELAY_SLOW].name = nameVelDown;
	_STATE->parameters[DELAY_SLOW].type = ParameterType_bool;
	_STATE->parameters[DELAY_SLOW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[DELAY_SYNC_FACTOR].category = nameDelay;
	_STATE->parameters[DELAY_SYNC_FACTOR].name = nameSyncFact;
	_STATE->parameters[DELAY_SYNC_FACTOR].initvalue = 2.0;
	_STATE->parameters[DELAY_SYNC_FACTOR].flags |= Param::NoAssignment;
	_STATE->parameters[DELAY_SYNC_FACTOR].setFlag(Param::NoAssignment, true);
	_STATE->parameters[DELAY_SYNC_FACTOR].type = ParameterType_bool;

	const char* lphpName = "LP/HP";
	_STATE->parameters[LPHPLPCUT].category = lphpName;
	_STATE->parameters[LPHPLPCUT].min = LOG10D20F(18.);
	_STATE->parameters[LPHPLPCUT].max = LOG10D20F(20000.);
	_STATE->parameters[LPHPLPCUT].name = "LP CUT";
	_STATE->parameters[LPHPLPCUT].valuename = "Hz";
	_STATE->parameters[LPHPLPCUT].type = ParameterType_double;
	_STATE->parameters[LPHPLPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[LPHPLPCUT].initvalue = LOG10D20F(20000.);
	_STATE->parameters[LPHPLPCUT].progress = .25;
	_STATE->parameters[LPHPLPCUT].flags |= Param::MidiParam;

	_STATE->parameters[LPHPHPCUT].category = lphpName;
	_STATE->parameters[LPHPHPCUT].min = LOG10D20F(18.);
	_STATE->parameters[LPHPHPCUT].max = LOG10D20F(20000.);
	_STATE->parameters[LPHPHPCUT].name = "HP CUT";
	_STATE->parameters[LPHPHPCUT].valuename = "Hz";
	_STATE->parameters[LPHPHPCUT].type = ParameterType_double;
	_STATE->parameters[LPHPHPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[LPHPHPCUT].initvalue = LOG10D20F(18.);
	_STATE->parameters[LPHPHPCUT].progress = .25;
	_STATE->parameters[LPHPHPCUT].flags |= Param::MidiParam;


	_STATE->parameters[LPHPMIX].category = lphpName;
	_STATE->parameters[LPHPMIX].min = 0.;
	_STATE->parameters[LPHPMIX].max = 1.;
	_STATE->parameters[LPHPMIX].name = "MIX";
	_STATE->parameters[LPHPMIX].valuename = " ";
	_STATE->parameters[LPHPMIX].type = ParameterType_double;
	_STATE->parameters[LPHPMIX].digits = 2;
	_STATE->parameters[LPHPMIX].initvalue = 1;
	_STATE->parameters[LPHPMIX].progress = .01;
	_STATE->parameters[LPHPMIX].flags |= Param::MidiParam;


	_STATE->parameters[LPHPGAIN].category = lphpName;
	_STATE->parameters[LPHPGAIN].min = -60;
	_STATE->parameters[LPHPGAIN].max = 60.;
	_STATE->parameters[LPHPGAIN].name = "GAIN";
	_STATE->parameters[LPHPGAIN].valuename = "dB";
	_STATE->parameters[LPHPGAIN].type = ParameterType_double;
	_STATE->parameters[LPHPGAIN].initvalue = 0;
	_STATE->parameters[LPHPGAIN].progress = 1;
	_STATE->parameters[LPHPGAIN].flags |= Param::MidiParam;

	const char* brName = "BANDREJECT";
	_STATE->parameters[BRCENTER].category = brName;
	_STATE->parameters[BRCENTER].min = LOG10D20F(18.);
	_STATE->parameters[BRCENTER].max = LOG10D20F(20000.);
	_STATE->parameters[BRCENTER].name = "CENTER";
	_STATE->parameters[BRCENTER].valuename = "Hz";
	_STATE->parameters[BRCENTER].type = ParameterType_double;
	_STATE->parameters[BRCENTER].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[BRCENTER].initvalue = LOG10D20F(1000.);
	_STATE->parameters[BRCENTER].progress = .25;
	_STATE->parameters[BRCENTER].flags |= Param::MidiParam;

	_STATE->parameters[BRBW].category = brName;
	_STATE->parameters[BRBW].min = 0.0;
	_STATE->parameters[BRBW].max = 1.0;
	_STATE->parameters[BRBW].name = "Q";
	_STATE->parameters[BRBW].valuename = " ";
	_STATE->parameters[BRBW].type = ParameterType_double;
	//_STATE->parameters[BRBW].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[BRBW].initvalue = .5f;
	_STATE->parameters[BRBW].progress = .05;
	_STATE->parameters[BRBW].digits = 2;
	_STATE->parameters[BRBW].flags |= Param::MidiParam;


	_STATE->parameters[BRMIX].category = brName;
	_STATE->parameters[BRMIX].min = 0.;
	_STATE->parameters[BRMIX].max = 1.;
	_STATE->parameters[BRMIX].name = "MIX";
	_STATE->parameters[BRMIX].valuename = " ";
	_STATE->parameters[BRMIX].type = ParameterType_double;
	_STATE->parameters[BRMIX].digits = 2;
	_STATE->parameters[BRMIX].initvalue = 1.;
	_STATE->parameters[BRMIX].progress = .01;
	_STATE->parameters[BRMIX].flags |= Param::MidiParam;


	_STATE->parameters[BRGAIN].category = brName;
	_STATE->parameters[BRGAIN].min = -60;
	_STATE->parameters[BRGAIN].max = 60.;
	_STATE->parameters[BRGAIN].name = "GAIN";
	_STATE->parameters[BRGAIN].valuename = "dB";
	_STATE->parameters[BRGAIN].type = ParameterType_double;
	_STATE->parameters[BRGAIN].initvalue = 0;
	_STATE->parameters[BRGAIN].progress = 1;
	_STATE->parameters[BRGAIN].flags |= Param::MidiParam;

	const char* resonName = "RESON";
	_STATE->parameters[BPCENTER].category = resonName;
	_STATE->parameters[BPCENTER].min = LOG10D20F(18.);
	_STATE->parameters[BPCENTER].max = LOG10D20F(20000.);
	_STATE->parameters[BPCENTER].name = "CENTER";
	_STATE->parameters[BPCENTER].valuename = "Hz";
	_STATE->parameters[BPCENTER].type = ParameterType_double;
	_STATE->parameters[BPCENTER].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[BPCENTER].initvalue = LOG10D20F(1000.);
	_STATE->parameters[BPCENTER].progress = .25;
	_STATE->parameters[BPCENTER].flags |= Param::MidiParam;


	_STATE->parameters[BPBW].category = resonName;
	_STATE->parameters[BPBW].min = LOG10D20F(0.001);
	_STATE->parameters[BPBW].max = LOG10D20F(1.);
	_STATE->parameters[BPBW].name = "Q";
	_STATE->parameters[BPBW].valuename = " ";
	_STATE->parameters[BPBW].type = ParameterType_double;
	_STATE->parameters[BPBW].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[BPBW].digits = 3;
	_STATE->parameters[BPBW].initvalue = LOG10D20F(.1);
	_STATE->parameters[BPBW].progress = .5;
	_STATE->parameters[BPBW].flags |= Param::MidiParam;


	_STATE->parameters[BPMIX].category = resonName;
	_STATE->parameters[BPMIX].min = 0.;
	_STATE->parameters[BPMIX].max = 1.;
	_STATE->parameters[BPMIX].name = "MIX";
	_STATE->parameters[BPMIX].valuename = " ";
	_STATE->parameters[BPMIX].type = ParameterType_double;
	_STATE->parameters[BPMIX].digits = 2;
	_STATE->parameters[BPMIX].initvalue = 1;
	_STATE->parameters[BPMIX].progress = .01;
	_STATE->parameters[BPMIX].flags |= Param::MidiParam;


	_STATE->parameters[BPGAIN].category = resonName;
	_STATE->parameters[BPGAIN].min = -60.;
	_STATE->parameters[BPGAIN].max = 60.;
	_STATE->parameters[BPGAIN].name = "GAIN";
	_STATE->parameters[BPGAIN].valuename = "dB";
	_STATE->parameters[BPGAIN].type = ParameterType_double;
	_STATE->parameters[BPGAIN].initvalue = 0;
	_STATE->parameters[BPGAIN].progress = 1;
	_STATE->parameters[BPGAIN].flags |= Param::MidiParam;


	const char* compName = "COMPRESSOR";
	_STATE->parameters[COMPTHR].category = compName;
	_STATE->parameters[COMPTHR].min = -60;
	_STATE->parameters[COMPTHR].max = LOG10D20F(1.);
	_STATE->parameters[COMPTHR].name = "THRESHOLD";
	_STATE->parameters[COMPTHR].valuename = "dB";
	_STATE->parameters[COMPTHR].type = ParameterType_double;
	_STATE->parameters[COMPTHR].flags |= Param::MidiParam;
	_STATE->parameters[COMPTHR].initvalue = -20;
	_STATE->parameters[COMPTHR].progress = 1.;

	_STATE->parameters[COMPMAKE].category = compName;
	_STATE->parameters[COMPMAKE].min = 0;
	_STATE->parameters[COMPMAKE].max = 60;
	_STATE->parameters[COMPMAKE].name = "MAKEUP";
	_STATE->parameters[COMPMAKE].valuename = "dB";
	_STATE->parameters[COMPMAKE].type = ParameterType_double;
	_STATE->parameters[COMPMAKE].flags |= Param::MidiParam;
	_STATE->parameters[COMPMAKE].initvalue = 0;
	_STATE->parameters[COMPMAKE].progress = 1;


	_STATE->parameters[COMPKNEE].category = compName;
	_STATE->parameters[COMPKNEE].min = 0;
	_STATE->parameters[COMPKNEE].max = 60.;
	_STATE->parameters[COMPKNEE].name = "KNEE";
	_STATE->parameters[COMPKNEE].valuename = "dB";
	_STATE->parameters[COMPKNEE].type = ParameterType_double;
	_STATE->parameters[COMPKNEE].flags |= Param::MidiParam;
	_STATE->parameters[COMPKNEE].initvalue = 10;
	_STATE->parameters[COMPKNEE].progress = 1;


	_STATE->parameters[COMPRMS].category = compName;
	_STATE->parameters[COMPRMS].min = 0.;
	_STATE->parameters[COMPRMS].max = 1000.;
	_STATE->parameters[COMPRMS].name = "RMS SIZE";
	_STATE->parameters[COMPRMS].valuename = "ms";
	_STATE->parameters[COMPRMS].type = ParameterType_double;
	_STATE->parameters[COMPRMS].digits = 0;
	_STATE->parameters[COMPRMS].flags |= Param::MidiParam;
	_STATE->parameters[COMPRMS].initvalue = 50.;
	_STATE->parameters[COMPRMS].progress = 10;

	_STATE->parameters[COMPATT].category = compName;
	_STATE->parameters[COMPATT].min = LOG10D20F(0.1);
	_STATE->parameters[COMPATT].max = LOG10D20F(70.);
	_STATE->parameters[COMPATT].name = "ATTACK";
	_STATE->parameters[COMPATT].valuename = "ms";
	_STATE->parameters[COMPATT].type = ParameterType_double;
	_STATE->parameters[COMPATT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[COMPATT].digits = 1;
	_STATE->parameters[COMPATT].initvalue = LOG10D20F(3.);
	_STATE->parameters[COMPATT].progress = 1;
	_STATE->parameters[COMPATT].flags |= Param::MidiParam;

	_STATE->parameters[COMPREL].category = compName;
	_STATE->parameters[COMPREL].min = LOG10D20F(1.);
	_STATE->parameters[COMPREL].max = LOG10D20F(200.);
	_STATE->parameters[COMPREL].name = "RELEASE";
	_STATE->parameters[COMPREL].valuename = "ms";
	_STATE->parameters[COMPREL].type = ParameterType_double;
	_STATE->parameters[COMPREL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[COMPREL].digits = 1;
	_STATE->parameters[COMPREL].initvalue = LOG10D20F(30.);
	_STATE->parameters[COMPREL].progress = 1;
	_STATE->parameters[COMPREL].flags |= Param::MidiParam;


	_STATE->parameters[COMPRATIO].category = compName;
	_STATE->parameters[COMPRATIO].min = -3;
	_STATE->parameters[COMPRATIO].max = 1.;
	_STATE->parameters[COMPRATIO].name = "RATIO";
	_STATE->parameters[COMPRATIO].valuename = " ";
	_STATE->parameters[COMPRATIO].type = ParameterType_double;
	_STATE->parameters[COMPRATIO].flags |= Param::MidiParam;
	_STATE->parameters[COMPRATIO].initvalue = .5;
	_STATE->parameters[COMPRATIO].progress = .01;

	_STATE->parameters[COMPLOOKA].category = compName;
	_STATE->parameters[COMPLOOKA].min = 0.;
	_STATE->parameters[COMPLOOKA].max = 1000.;
	_STATE->parameters[COMPLOOKA].name = "LOOKAHEAD";
	_STATE->parameters[COMPLOOKA].valuename = "ms";
	_STATE->parameters[COMPLOOKA].type = ParameterType_double;
	_STATE->parameters[COMPLOOKA].digits = 0;
	_STATE->parameters[COMPLOOKA].initvalue = 0;
	_STATE->parameters[COMPLOOKA].progress = 1;
	_STATE->parameters[COMPLOOKA].flags |= Param::MidiParam;

	const char* pshiftName = "PITCH SHIFTER";

	_STATE->parameters[PITCHSHIFTMODE].category = pshiftName;
	_STATE->parameters[PITCHSHIFTMODE].name = "SHIFTMODE";
	_STATE->parameters[PITCHSHIFTMODE].type = ParameterType_enum;
	_STATE->parameters[PITCHSHIFTMODE].names = makeStringSpan(shimmermodesnames).subspan(1, 2);
	_STATE->parameters[PITCHSHIFTMODE].flags |= Param::MidiParam;

	_STATE->parameters[PITCHSHIFTSHIFT].category = pshiftName;
	_STATE->parameters[PITCHSHIFTSHIFT].min = -12;
	_STATE->parameters[PITCHSHIFTSHIFT].max = 12;
	_STATE->parameters[PITCHSHIFTSHIFT].name = "SHIFT";
	_STATE->parameters[PITCHSHIFTSHIFT].valuename = "SEMITONES";
	_STATE->parameters[PITCHSHIFTSHIFT].type = ParameterType_double;
	_STATE->parameters[PITCHSHIFTSHIFT].digits = 2;
	_STATE->parameters[PITCHSHIFTSHIFT].initvalue = 0;
	_STATE->parameters[PITCHSHIFTSHIFT].progress = 1;
	_STATE->parameters[PITCHSHIFTSHIFT].flags |= Param::MidiParam;

	_STATE->parameters[PITCHSHIFERMIX].category = pshiftName;
	_STATE->parameters[PITCHSHIFERMIX].min = 0.;
	_STATE->parameters[PITCHSHIFERMIX].max = 1.;
	_STATE->parameters[PITCHSHIFERMIX].name = "MIX";
	_STATE->parameters[PITCHSHIFERMIX].valuename = " ";
	_STATE->parameters[PITCHSHIFERMIX].type = ParameterType_double;
	_STATE->parameters[PITCHSHIFERMIX].digits = 2;
	_STATE->parameters[PITCHSHIFERMIX].initvalue = .5;
	_STATE->parameters[PITCHSHIFERMIX].progress = .01;
	_STATE->parameters[PITCHSHIFERMIX].flags |= Param::MidiParam;


	const char* grainParams = "GRANULATION";
	//_STATE->parameters[PREGAIN].category = grainParams;
	_STATE->parameters[PREGAIN].min = -60.;
	_STATE->parameters[PREGAIN].max = 60.;
	_STATE->parameters[PREGAIN].name = "PREGAIN";
	_STATE->parameters[PREGAIN].valuename = "dB";
	_STATE->parameters[PREGAIN].type = ParameterType_double;
	_STATE->parameters[PREGAIN].initvalue = -14;
	_STATE->parameters[PREGAIN].progress = 1;
	_STATE->parameters[PREGAIN].flags |= Param::MidiParam;


	_STATE->parameters[POSTGAIN].min = -60.;
	_STATE->parameters[POSTGAIN].max = 60.;
	_STATE->parameters[POSTGAIN].name = "POSTGAIN";
	_STATE->parameters[POSTGAIN].valuename = "dB";
	_STATE->parameters[POSTGAIN].type = ParameterType_double;
	_STATE->parameters[POSTGAIN].initvalue = 0;
	_STATE->parameters[POSTGAIN].progress = 1;
	_STATE->parameters[POSTGAIN].flags |= Param::MidiParam;


	_STATE->parameters[GRAINSIZE].category = grainParams;
	_STATE->parameters[GRAINSIZE].min = _STATE->sr / 1000. * 1.;
	_STATE->parameters[GRAINSIZE].max = _DATA->maxgrainsize;
	_STATE->parameters[GRAINSIZE].name = "GRAINSIZE";
	_STATE->parameters[GRAINSIZE].valuename = "ms";
	_STATE->parameters[GRAINSIZE].type = ParameterType_double;
	_STATE->parameters[GRAINSIZE].flags |= Param::ConvertMs;
	_STATE->parameters[GRAINSIZE].initvalue = 1024;
	_STATE->parameters[GRAINSIZE].progress = _STATE->sr / 1000.;
	_STATE->parameters[GRAINSIZE].flags |= Param::MidiParam;

	_STATE->parameters[DENSITY].category = grainParams;
	_STATE->parameters[DENSITY].min = DENSITYMIN;
	_STATE->parameters[DENSITY].max = DENSITYMAX;
	_STATE->parameters[DENSITY].name = "DENSITY";
	_STATE->parameters[DENSITY].valuename = "Hz";
	_STATE->parameters[DENSITY].type = ParameterType_double;
	_STATE->parameters[DENSITY].initvalue = 172;
	_STATE->parameters[DENSITY].progress = 1;
	_STATE->parameters[DENSITY].flags |= Param::MidiParam;
	_STATE->parameters[DENSITY].digits = 2;


	//_STATE->parameters[SPEED].category = grainParams;
	_STATE->parameters[SPEED].min = LOG10D20F(SPEED_OFFSET);
	_STATE->parameters[SPEED].max = LOG10D20F(SPEED_MAX + SPEED_OFFSET);
	_STATE->parameters[SPEED].name = "SPEED";
	_STATE->parameters[SPEED].valuename = "x";
	_STATE->parameters[SPEED].type = ParameterType_double;
	_STATE->parameters[SPEED].digits = 3;
	_STATE->parameters[SPEED].initvalue = LOG10D20F(1. + SPEED_OFFSET);
	_STATE->parameters[SPEED].progress = 1;
	_STATE->parameters[SPEED].offset = SPEED_OFFSET;
	_STATE->parameters[SPEED].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPEED].flags |= Param::MidiParam;


	_STATE->parameters[RNDREAD].category = grainParams;
	_STATE->parameters[RNDREAD].min = 0;
	_STATE->parameters[RNDREAD].max = 2 * _STATE->sr;
	_STATE->parameters[RNDREAD].name = "RNDREAD";
	_STATE->parameters[RNDREAD].valuename = "ms";
	_STATE->parameters[RNDREAD].type = ParameterType_double;
	_STATE->parameters[RNDREAD].flags |= Param::ConvertMs;
	_STATE->parameters[RNDREAD].digits = 1;
	_STATE->parameters[RNDREAD].initvalue = 0;
	_STATE->parameters[RNDREAD].progress = _STATE->sr / 1000. * 5;
	_STATE->parameters[RNDREAD].flags |= Param::MidiParam;

	_STATE->parameters[RNDWRITE].category = grainParams;
	_STATE->parameters[RNDWRITE].min = 0;
	_STATE->parameters[RNDWRITE].max = _STATE->sr / 1000. * 20.;
	_STATE->parameters[RNDWRITE].name = "RNDWRITE";
	_STATE->parameters[RNDWRITE].valuename = "ms";
	_STATE->parameters[RNDWRITE].type = ParameterType_double;
	_STATE->parameters[RNDWRITE].flags |= Param::ConvertMs;
	_STATE->parameters[RNDWRITE].digits = 1;
	_STATE->parameters[RNDWRITE].initvalue = 0;
	_STATE->parameters[RNDWRITE].progress = _STATE->sr / 1000. * .5f;
	//_STATE->parameters[MDELAY1DEL].flags |= Param::MidiParam;


	_STATE->parameters[AWINCYLCES].category = grainParams;
	_STATE->parameters[AWINCYLCES].min = 1;
	_STATE->parameters[AWINCYLCES].max = 120;
	_STATE->parameters[AWINCYLCES].name = "ENV CYCLES";
	_STATE->parameters[AWINCYLCES].flags |= Param::CastInt;
	_STATE->parameters[AWINCYLCES].valuename = " / GRAIN";
	_STATE->parameters[AWINCYLCES].type = ParameterType_double;
	_STATE->parameters[AWINCYLCES].initvalue = 1;
	_STATE->parameters[AWINCYLCES].progress = 1;
	_STATE->parameters[AWINCYLCES].flags |= Param::MidiParam;

	const char* seqName = "SEQUENCER";
	_STATE->parameters[GRAINS].category = seqName;
	_STATE->parameters[GRAINS].min = 1;
	_STATE->parameters[GRAINS].max = 1024;
	_STATE->parameters[GRAINS].name = "GRAINS";
	_STATE->parameters[GRAINS].valuename = " ";
	_STATE->parameters[GRAINS].flags |= Param::CastInt;
	_STATE->parameters[GRAINS].type = ParameterType_double;
	_STATE->parameters[GRAINS].initvalue = 1;
	_STATE->parameters[GRAINS].progress = 1;
	_STATE->parameters[GRAINS].flags |= Param::MidiParam;

	_STATE->parameters[SILENCE].category = seqName;
	_STATE->parameters[SILENCE].min = 0;
	_STATE->parameters[SILENCE].max = 1024;
	_STATE->parameters[SILENCE].name = "SILENCE";
	_STATE->parameters[SILENCE].valuename = " ";
	_STATE->parameters[SILENCE].flags |= Param::CastInt;
	_STATE->parameters[SILENCE].type = ParameterType_double;
	_STATE->parameters[SILENCE].initvalue = 0;
	_STATE->parameters[SILENCE].progress = 1;
	_STATE->parameters[SILENCE].flags |= Param::MidiParam;


	_STATE->parameters[PITCH].category = grainParams;
	_STATE->parameters[PITCH].min = -3.;
	_STATE->parameters[PITCH].max = 3.;
	_STATE->parameters[PITCH].name = "PITCH";
	_STATE->parameters[PITCH].valuename = "Octaves";
	_STATE->parameters[PITCH].digits = 2;
	_STATE->parameters[PITCH].type = ParameterType_double;
	_STATE->parameters[PITCH].initvalue = 0;
	_STATE->parameters[PITCH].progress = .01;
	_STATE->parameters[PITCH].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGLISS].category = grainParams;
	_STATE->parameters[GRAINGLISS].min = -3.;
	_STATE->parameters[GRAINGLISS].max = 3.;
	_STATE->parameters[GRAINGLISS].name = "GLISS";
	_STATE->parameters[GRAINGLISS].valuename = "Octaves";
	_STATE->parameters[GRAINGLISS].digits = 2;
	_STATE->parameters[GRAINGLISS].type = ParameterType_double;
	_STATE->parameters[GRAINGLISS].initvalue = 0;
	_STATE->parameters[GRAINGLISS].progress = .01;
	_STATE->parameters[GRAINGLISS].flags |= Param::MidiParam;

	const char* rpitch = "RANDOM PITCH";
	_STATE->parameters[SEMITONES].category = rpitch;
	_STATE->parameters[SEMITONES].min = 0;
	_STATE->parameters[SEMITONES].max = 12.;
	_STATE->parameters[SEMITONES].name = "SEMITONES";
	_STATE->parameters[SEMITONES].valuename = "Semitones";
	_STATE->parameters[SEMITONES].flags |= Param::CastInt;
	_STATE->parameters[SEMITONES].type = ParameterType_double;
	_STATE->parameters[SEMITONES].initvalue = 0;
	_STATE->parameters[SEMITONES].progress = 1;
	_STATE->parameters[SEMITONES].flags |= Param::MidiParam;


	_STATE->parameters[SEMITONES_MIN].category = rpitch;
	_STATE->parameters[SEMITONES_MIN].min = -12.;
	_STATE->parameters[SEMITONES_MIN].max = 12.;
	_STATE->parameters[SEMITONES_MIN].name = "MIN";
	_STATE->parameters[SEMITONES_MIN].valuename = "Semitones";
	_STATE->parameters[SEMITONES_MIN].flags |= Param::CastInt;
	_STATE->parameters[SEMITONES_MIN].type = ParameterType_double;
	_STATE->parameters[SEMITONES_MIN].initvalue = 0;
	_STATE->parameters[SEMITONES_MIN].progress = 1;
	_STATE->parameters[SEMITONES_MIN].flags |= Param::MidiParam;

	_STATE->parameters[SEMITONES_MAX].category = rpitch;
	_STATE->parameters[SEMITONES_MAX].min = -12.;
	_STATE->parameters[SEMITONES_MAX].max = 12.;
	_STATE->parameters[SEMITONES_MAX].name = "MAX";
	_STATE->parameters[SEMITONES_MAX].valuename = "Semitones";
	_STATE->parameters[SEMITONES_MAX].flags |= Param::CastInt;
	_STATE->parameters[SEMITONES_MAX].type = ParameterType_double;
	_STATE->parameters[SEMITONES_MAX].initvalue = 0;
	_STATE->parameters[SEMITONES_MAX].progress = 1;
	_STATE->parameters[SEMITONES_MAX].flags |= Param::MidiParam;

	_STATE->parameters[PITCHMIN].category = rpitch;
	_STATE->parameters[PITCHMIN].min = -3.;
	_STATE->parameters[PITCHMIN].max = 3.;
	_STATE->parameters[PITCHMIN].name = "PITCH MIN";
	_STATE->parameters[PITCHMIN].valuename = "Octaves";
	_STATE->parameters[PITCHMIN].digits = 2;
	_STATE->parameters[PITCHMIN].type = ParameterType_double;
	_STATE->parameters[PITCHMIN].initvalue = 0;
	_STATE->parameters[PITCHMIN].progress = .01;
	_STATE->parameters[PITCHMIN].flags |= Param::MidiParam;

	_STATE->parameters[PITCHMAX].category = rpitch;
	_STATE->parameters[PITCHMAX].min = -3.;
	_STATE->parameters[PITCHMAX].max = 3.;
	_STATE->parameters[PITCHMAX].name = "PITCH MAX";
	_STATE->parameters[PITCHMAX].valuename = "Octaves";
	_STATE->parameters[PITCHMAX].digits = 2;
	_STATE->parameters[PITCHMAX].type = ParameterType_double;
	_STATE->parameters[PITCHMAX].initvalue = 0;
	_STATE->parameters[PITCHMAX].progress = .01;
	_STATE->parameters[PITCHMAX].flags |= Param::MidiParam;

	const char* grainbp = "GRAIN RESON";
	_STATE->parameters[GRAINBPBWMIN].category = grainbp;
	_STATE->parameters[GRAINBPBWMIN].min = LOG10D20F(0.001);
	_STATE->parameters[GRAINBPBWMIN].max = LOG10D20F(1.);
	_STATE->parameters[GRAINBPBWMIN].name = "Q MIN";
	_STATE->parameters[GRAINBPBWMIN].valuename = " ";
	_STATE->parameters[GRAINBPBWMIN].digits = 3;
	_STATE->parameters[GRAINBPBWMIN].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINBPBWMIN].type = ParameterType_double;
	_STATE->parameters[GRAINBPBWMIN].initvalue = LOG10D20F(.1);
	_STATE->parameters[GRAINBPBWMIN].progress = .25;
	_STATE->parameters[GRAINBPBWMIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINBPBWMAX].category = grainbp;
	_STATE->parameters[GRAINBPBWMAX].min = LOG10D20F(0.001);
	_STATE->parameters[GRAINBPBWMAX].max = LOG10D20F(1.);
	_STATE->parameters[GRAINBPBWMAX].name = "Q MAX";
	_STATE->parameters[GRAINBPBWMAX].valuename = " ";
	_STATE->parameters[GRAINBPBWMAX].digits = 3;
	_STATE->parameters[GRAINBPBWMAX].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINBPBWMAX].type = ParameterType_double;
	_STATE->parameters[GRAINBPBWMAX].initvalue = 0;
	_STATE->parameters[GRAINBPBWMAX].progress = .25;
	_STATE->parameters[GRAINBPBWMAX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINBPCENTERMIN].category = grainbp;
	_STATE->parameters[GRAINBPCENTERMIN].min = LOG10D20F(18.);
	_STATE->parameters[GRAINBPCENTERMIN].max = LOG10D20F(5000.);
	_STATE->parameters[GRAINBPCENTERMIN].name = "CENTER MIN";
	_STATE->parameters[GRAINBPCENTERMIN].valuename = "Hz";
	_STATE->parameters[GRAINBPCENTERMIN].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINBPCENTERMIN].type = ParameterType_double;
	_STATE->parameters[GRAINBPCENTERMIN].initvalue = LOG10D20F(400.);
	_STATE->parameters[GRAINBPCENTERMIN].progress = .25;
	_STATE->parameters[GRAINBPCENTERMIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINBPCENTERMAX].category = grainbp;
	_STATE->parameters[GRAINBPCENTERMAX].min = LOG10D20F(18.);
	_STATE->parameters[GRAINBPCENTERMAX].max = LOG10D20F(5000.);
	_STATE->parameters[GRAINBPCENTERMAX].name = "CENTER MAX";
	_STATE->parameters[GRAINBPCENTERMAX].valuename = "Hz";
	_STATE->parameters[GRAINBPCENTERMAX].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINBPCENTERMAX].type = ParameterType_double;
	_STATE->parameters[GRAINBPCENTERMAX].initvalue = LOG10D20F(2000.);
	_STATE->parameters[GRAINBPCENTERMAX].progress = .25;
	_STATE->parameters[GRAINBPCENTERMAX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINBPMIX].category = grainbp;
	_STATE->parameters[GRAINBPMIX].min = 0;
	_STATE->parameters[GRAINBPMIX].max = 1;
	_STATE->parameters[GRAINBPMIX].name = "MIX";
	_STATE->parameters[GRAINBPMIX].valuename = " ";
	_STATE->parameters[GRAINBPMIX].type = ParameterType_double;
	_STATE->parameters[GRAINBPMIX].digits = 2;
	_STATE->parameters[GRAINBPMIX].initvalue = 1;
	_STATE->parameters[GRAINBPMIX].progress = .01;
	_STATE->parameters[GRAINBPMIX].flags |= Param::MidiParam;


	_STATE->parameters[GRAINBPGAIN].category = grainbp;
	_STATE->parameters[GRAINBPGAIN].min = -60.;
	_STATE->parameters[GRAINBPGAIN].max = 60.;
	_STATE->parameters[GRAINBPGAIN].name = "GAIN";
	_STATE->parameters[GRAINBPGAIN].valuename = "dB";
	_STATE->parameters[GRAINBPGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINBPGAIN].initvalue = 0;
	_STATE->parameters[GRAINBPGAIN].progress = 1;
	_STATE->parameters[GRAINBPGAIN].flags |= Param::MidiParam;

	const char* grainrm = "GRAIN RINGMOD";
	_STATE->parameters[GRAINRMMIN].category = grainrm;
	_STATE->parameters[GRAINRMMIN].min = 100.;
	_STATE->parameters[GRAINRMMIN].max = 10000.;
	_STATE->parameters[GRAINRMMIN].name = "RATE MIN";
	_STATE->parameters[GRAINRMMIN].valuename = "Hz";
	_STATE->parameters[GRAINRMMIN].type = ParameterType_double;
	_STATE->parameters[GRAINRMMIN].initvalue = 1000;
	_STATE->parameters[GRAINRMMIN].progress = 100.;
	_STATE->parameters[GRAINRMMIN].flags |= Param::MidiParam;


	_STATE->parameters[GRAINRMMAX].category = grainrm;
	_STATE->parameters[GRAINRMMAX].min = 100.;
	_STATE->parameters[GRAINRMMAX].max = 10000.;
	_STATE->parameters[GRAINRMMAX].name = "RATE MAX";
	_STATE->parameters[GRAINRMMAX].valuename = "Hz";
	_STATE->parameters[GRAINRMMAX].type = ParameterType_double;
	_STATE->parameters[GRAINRMMAX].initvalue = 5000.;
	_STATE->parameters[GRAINRMMAX].progress = 100.;
	_STATE->parameters[GRAINRMMAX].flags |= Param::MidiParam;


	_STATE->parameters[GRAINRMMIX].category = grainrm;
	_STATE->parameters[GRAINRMMIX].min = 0.;
	_STATE->parameters[GRAINRMMIX].max = 1.;
	_STATE->parameters[GRAINRMMIX].name = "MIX";
	_STATE->parameters[GRAINRMMIX].valuename = " ";
	_STATE->parameters[GRAINRMMIX].type = ParameterType_double;
	_STATE->parameters[GRAINRMMIX].digits = 2;
	_STATE->parameters[GRAINRMMIX].initvalue = .5;
	_STATE->parameters[GRAINRMMIX].progress = .01;
	_STATE->parameters[GRAINRMMIX].flags |= Param::MidiParam;

	const char* sat = "SATURATOR";
	_STATE->parameters[SATDRIVE].category = sat;
	_STATE->parameters[SATDRIVE].min = -120;
	_STATE->parameters[SATDRIVE].max = 120.;
	_STATE->parameters[SATDRIVE].name = "DRIVE";
	_STATE->parameters[SATDRIVE].valuename = "dB";
	_STATE->parameters[SATDRIVE].type = ParameterType_double;
	_STATE->parameters[SATDRIVE].initvalue = 0;
	_STATE->parameters[SATDRIVE].progress = 1;
	_STATE->parameters[SATDRIVE].flags |= Param::MidiParam;

	_STATE->parameters[SATWET].category = sat;
	_STATE->parameters[SATWET].min = -120.;
	_STATE->parameters[SATWET].max = 120;
	_STATE->parameters[SATWET].name = "WET";
	_STATE->parameters[SATWET].valuename = "dB";
	_STATE->parameters[SATWET].type = ParameterType_double;
	_STATE->parameters[SATWET].initvalue = -6;
	_STATE->parameters[SATWET].progress = 1;
	_STATE->parameters[SATWET].flags |= Param::MidiParam;

	_STATE->parameters[SATDRY].category = sat;
	_STATE->parameters[SATDRY].min = -120.;
	_STATE->parameters[SATDRY].max = 120;
	_STATE->parameters[SATDRY].name = "DRY";
	_STATE->parameters[SATDRY].valuename = "dB";
	_STATE->parameters[SATDRY].type = ParameterType_double;
	_STATE->parameters[SATDRY].initvalue = -6;
	_STATE->parameters[SATDRY].progress = 1;
	_STATE->parameters[SATDRY].flags |= Param::MidiParam;

	// Inert while the VOX params are parked after NUM_PARAMS.
	if constexpr (VOXFREQ < NUM_PARAMS) {
	const char* vox = "VOX";
	_STATE->parameters[VOXFREQ].category = vox;
	_STATE->parameters[VOXFREQ].min = LOG10D20F(20.);
	_STATE->parameters[VOXFREQ].max = LOG10D20F(2000.);
	_STATE->parameters[VOXFREQ].name = "CPS";
	_STATE->parameters[VOXFREQ].valuename = "Hz";
	_STATE->parameters[VOXFREQ].type = ParameterType_double;
	_STATE->parameters[VOXFREQ].initvalue = LOG10D20F(110.);
	_STATE->parameters[VOXFREQ].progress = 1.;
	_STATE->parameters[VOXFREQ].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOXFREQ].flags |= Param::MidiParam;

	_STATE->parameters[VOXVOWEL].category = vox;
	_STATE->parameters[VOXVOWEL].min = 0.;
	_STATE->parameters[VOXVOWEL].max = 4.;
	_STATE->parameters[VOXVOWEL].name = "VOWEL";
	_STATE->parameters[VOXVOWEL].valuename = " ";
	_STATE->parameters[VOXVOWEL].type = ParameterType_double;
	_STATE->parameters[VOXVOWEL].initvalue = 0;
	_STATE->parameters[VOXVOWEL].digits = 2;
	_STATE->parameters[VOXVOWEL].progress = 1;
	_STATE->parameters[VOXVOWEL].flags |= Param::MidiParam;

	_STATE->parameters[VOXVOICE].category = vox;
	_STATE->parameters[VOXVOICE].name = "VOICE";
	_STATE->parameters[VOXVOICE].type = ParameterType_enum;
	_STATE->parameters[VOXVOICE].names = voxvoicenames;
	_STATE->parameters[VOXVOICE].initvalue = 1;
	_STATE->parameters[VOXVOICE].flags |= Param::MidiParam;

	_STATE->parameters[VOXATT].category = vox;
	_STATE->parameters[VOXATT].min = 0.;
	_STATE->parameters[VOXATT].max = 1.;
	_STATE->parameters[VOXATT].name = "ATTACK";
	_STATE->parameters[VOXATT].valuename = " ";
	_STATE->parameters[VOXATT].type = ParameterType_double;
	_STATE->parameters[VOXATT].initvalue = .15;
	_STATE->parameters[VOXATT].digits = 2;
	_STATE->parameters[VOXATT].progress = 1;
	_STATE->parameters[VOXATT].flags |= Param::MidiParam;

	_STATE->parameters[VOXGLISS].category = vox;
	_STATE->parameters[VOXGLISS].min = -24.;
	_STATE->parameters[VOXGLISS].max = 24.;
	_STATE->parameters[VOXGLISS].name = "GLISS";
	_STATE->parameters[VOXGLISS].valuename = "st";
	_STATE->parameters[VOXGLISS].type = ParameterType_double;
	_STATE->parameters[VOXGLISS].initvalue = 0;
	_STATE->parameters[VOXGLISS].digits = 1;
	_STATE->parameters[VOXGLISS].progress = 1;
	_STATE->parameters[VOXGLISS].flags |= Param::MidiParam;

	_STATE->parameters[VOXOCT].category = vox;
	_STATE->parameters[VOXOCT].min = 0.;
	_STATE->parameters[VOXOCT].max = 2.;
	_STATE->parameters[VOXOCT].name = "OCT";
	_STATE->parameters[VOXOCT].valuename = " ";
	_STATE->parameters[VOXOCT].type = ParameterType_double;
	_STATE->parameters[VOXOCT].initvalue = 0;
	_STATE->parameters[VOXOCT].digits = 2;
	_STATE->parameters[VOXOCT].progress = 1;
	_STATE->parameters[VOXOCT].flags |= Param::MidiParam;

	_STATE->parameters[VOXWET].category = vox;
	_STATE->parameters[VOXWET].min = -120.;
	_STATE->parameters[VOXWET].max = 120;
	_STATE->parameters[VOXWET].name = "WET";
	_STATE->parameters[VOXWET].valuename = "dB";
	_STATE->parameters[VOXWET].type = ParameterType_double;
	_STATE->parameters[VOXWET].initvalue = 0;
	_STATE->parameters[VOXWET].progress = 1;
	_STATE->parameters[VOXWET].flags |= Param::MidiParam;

	_STATE->parameters[VOXDRY].category = vox;
	_STATE->parameters[VOXDRY].min = -120.;
	_STATE->parameters[VOXDRY].max = 120;
	_STATE->parameters[VOXDRY].name = "DRY";
	_STATE->parameters[VOXDRY].valuename = "dB";
	_STATE->parameters[VOXDRY].type = ParameterType_double;
	_STATE->parameters[VOXDRY].initvalue = -6;
	_STATE->parameters[VOXDRY].progress = 1;
	_STATE->parameters[VOXDRY].flags |= Param::MidiParam;

	_STATE->parameters[VOXFOLLOW].category = vox;
	_STATE->parameters[VOXFOLLOW].name = "FOLLOW";
	_STATE->parameters[VOXFOLLOW].type = ParameterType_bool;
	_STATE->parameters[VOXFOLLOW].flags |= Param::MidiParam;

	_STATE->parameters[VOXHOLD].category = vox;
	_STATE->parameters[VOXHOLD].name = "HOLD";
	_STATE->parameters[VOXHOLD].type = ParameterType_bool;
	_STATE->parameters[VOXHOLD].flags |= Param::MidiParam;
	}

	const char* vox2 = "VOX2";
	_STATE->parameters[VOX2VOWEL].category = vox2;
	_STATE->parameters[VOX2VOWEL].min = 0.;
	_STATE->parameters[VOX2VOWEL].max = 4.;
	_STATE->parameters[VOX2VOWEL].name = "VOWEL";
	_STATE->parameters[VOX2VOWEL].valuename = " ";
	_STATE->parameters[VOX2VOWEL].type = ParameterType_double;
	_STATE->parameters[VOX2VOWEL].initvalue = 0;
	_STATE->parameters[VOX2VOWEL].digits = 2;
	_STATE->parameters[VOX2VOWEL].progress = 1;
	_STATE->parameters[VOX2VOWEL].flags |= Param::MidiParam;

	_STATE->parameters[VOX2VOICE].category = vox2;
	_STATE->parameters[VOX2VOICE].name = "VOICE";
	_STATE->parameters[VOX2VOICE].type = ParameterType_enum;
	_STATE->parameters[VOX2VOICE].names = voxvoicenames;
	_STATE->parameters[VOX2VOICE].initvalue = 1;
	_STATE->parameters[VOX2VOICE].flags |= Param::MidiParam;

	_STATE->parameters[VOX2BW].category = vox2;
	_STATE->parameters[VOX2BW].min = .25;
	_STATE->parameters[VOX2BW].max = 4.;
	_STATE->parameters[VOX2BW].name = "BW";
	_STATE->parameters[VOX2BW].valuename = "x";
	_STATE->parameters[VOX2BW].type = ParameterType_double;
	_STATE->parameters[VOX2BW].initvalue = 1;
	_STATE->parameters[VOX2BW].digits = 2;
	_STATE->parameters[VOX2BW].progress = .01;
	_STATE->parameters[VOX2BW].flags |= Param::MidiParam;

	_STATE->parameters[VOX2WET].category = vox2;
	_STATE->parameters[VOX2WET].min = -120.;
	_STATE->parameters[VOX2WET].max = 120;
	_STATE->parameters[VOX2WET].name = "WET";
	_STATE->parameters[VOX2WET].valuename = "dB";
	_STATE->parameters[VOX2WET].type = ParameterType_double;
	_STATE->parameters[VOX2WET].initvalue = 0;
	_STATE->parameters[VOX2WET].progress = 1;
	_STATE->parameters[VOX2WET].flags |= Param::MidiParam;

	_STATE->parameters[VOX2DRY].category = vox2;
	_STATE->parameters[VOX2DRY].min = -120.;
	_STATE->parameters[VOX2DRY].max = 120;
	_STATE->parameters[VOX2DRY].name = "DRY";
	_STATE->parameters[VOX2DRY].valuename = "dB";
	_STATE->parameters[VOX2DRY].type = ParameterType_double;
	_STATE->parameters[VOX2DRY].initvalue = -6;
	_STATE->parameters[VOX2DRY].progress = 1;
	_STATE->parameters[VOX2DRY].flags |= Param::MidiParam;

	const char* dist = "DISTORTION";
	_STATE->parameters[DISTTONE].category = dist;
	_STATE->parameters[DISTTONE].min = 0.;
	_STATE->parameters[DISTTONE].max = 1.;
	_STATE->parameters[DISTTONE].name = "TONE";
	_STATE->parameters[DISTTONE].valuename = " ";
	_STATE->parameters[DISTTONE].type = ParameterType_double;
	_STATE->parameters[DISTTONE].digits = 2;
	_STATE->parameters[DISTTONE].initvalue = 0.5f;
	_STATE->parameters[DISTTONE].progress = .01;
	_STATE->parameters[DISTTONE].flags |= Param::MidiParam;

	// 0..1, remapped exponentially onto the shaper gain in DISTORT::compute.
	// Was 0..60 dB up to preset version 21, of which only the bottom ~24 dB did
	// anything; kDistDriveInit is the knob position that reproduces the old
	// 0 dB default. Old files are converted by migrateDistDrive() on load.
	_STATE->parameters[DISTDRIVE].category = dist;
	_STATE->parameters[DISTDRIVE].min = 0.;
	_STATE->parameters[DISTDRIVE].max = 1.;
	_STATE->parameters[DISTDRIVE].name = "DRIVE";
	_STATE->parameters[DISTDRIVE].valuename = " ";
	_STATE->parameters[DISTDRIVE].type = ParameterType_double;
	_STATE->parameters[DISTDRIVE].digits = 2;
	_STATE->parameters[DISTDRIVE].initvalue = kDistDriveInit;
	_STATE->parameters[DISTDRIVE].progress = .01;
	_STATE->parameters[DISTDRIVE].flags |= Param::MidiParam;

	_STATE->parameters[DISTMIX].category = dist;
	_STATE->parameters[DISTMIX].min = 0.;
	_STATE->parameters[DISTMIX].max = 1.;
	_STATE->parameters[DISTMIX].name = "MIX";
	_STATE->parameters[DISTMIX].valuename = " ";
	_STATE->parameters[DISTMIX].type = ParameterType_double;
	_STATE->parameters[DISTMIX].digits = 2;
	_STATE->parameters[DISTMIX].initvalue = 1;
	_STATE->parameters[DISTMIX].progress = .01;
	_STATE->parameters[DISTMIX].flags |= Param::MidiParam;

	_STATE->parameters[DISTGAIN].category = dist;
	_STATE->parameters[DISTGAIN].min = -60.;
	_STATE->parameters[DISTGAIN].max = 60.;
	_STATE->parameters[DISTGAIN].name = "GAIN";
	_STATE->parameters[DISTGAIN].valuename = "dB";
	_STATE->parameters[DISTGAIN].type = ParameterType_double;
	_STATE->parameters[DISTGAIN].initvalue = 0;
	_STATE->parameters[DISTGAIN].progress = 1;
	_STATE->parameters[DISTGAIN].flags |= Param::MidiParam;

	const char* flanger = "FLANGER";

	_STATE->parameters[FLANGERDELAY].category = flanger;
	_STATE->parameters[FLANGERDELAY].min = 0;
	_STATE->parameters[FLANGERDELAY].max = 5.;
	_STATE->parameters[FLANGERDELAY].name = "DELAY";
	_STATE->parameters[FLANGERDELAY].valuename = "ms";
	_STATE->parameters[FLANGERDELAY].type = ParameterType_double;
	_STATE->parameters[FLANGERDELAY].digits = 1;
	_STATE->parameters[FLANGERDELAY].initvalue = 5;
	_STATE->parameters[FLANGERDELAY].progress = .1;
	_STATE->parameters[FLANGERDELAY].flags |= Param::MidiParam;

	_STATE->parameters[FLANGERFB].category = flanger;
	_STATE->parameters[FLANGERFB].min = -1.;
	_STATE->parameters[FLANGERFB].max = 1.;
	_STATE->parameters[FLANGERFB].name = "FEEDBACK";
	_STATE->parameters[FLANGERFB].valuename = " ";
	_STATE->parameters[FLANGERFB].type = ParameterType_double;
	_STATE->parameters[FLANGERFB].digits = 2;
	_STATE->parameters[FLANGERFB].initvalue = .5;
	_STATE->parameters[FLANGERFB].progress = .01;
	_STATE->parameters[FLANGERFB].flags |= Param::MidiParam;

	_STATE->parameters[FLANGERMIX].category = flanger;
	_STATE->parameters[FLANGERMIX].min = 0;
	_STATE->parameters[FLANGERMIX].max = 1;
	_STATE->parameters[FLANGERMIX].name = "MIX";
	_STATE->parameters[FLANGERMIX].valuename = " ";
	_STATE->parameters[FLANGERMIX].type = ParameterType_double;
	_STATE->parameters[FLANGERMIX].digits = 2;
	_STATE->parameters[FLANGERMIX].initvalue = .5;
	_STATE->parameters[FLANGERMIX].progress = .01;
	_STATE->parameters[FLANGERMIX].flags |= Param::MidiParam;

	_STATE->parameters[FLANGERGAIN].category = flanger;
	_STATE->parameters[FLANGERGAIN].min = -60;
	_STATE->parameters[FLANGERGAIN].max = 60;
	_STATE->parameters[FLANGERGAIN].name = "GAIN";
	_STATE->parameters[FLANGERGAIN].valuename = "dB";
	_STATE->parameters[FLANGERGAIN].type = ParameterType_double;
	_STATE->parameters[FLANGERGAIN].digits = 0;
	_STATE->parameters[FLANGERGAIN].initvalue = 0;
	_STATE->parameters[FLANGERGAIN].progress = 1;
	_STATE->parameters[FLANGERGAIN].flags |= Param::MidiParam;

	const char* ssb = "SSB MOD";
	_STATE->parameters[SSBMODMIX].category = ssb;
	_STATE->parameters[SSBMODMIX].min = 0;
	_STATE->parameters[SSBMODMIX].max = 1;
	_STATE->parameters[SSBMODMIX].name = "MIX";
	_STATE->parameters[SSBMODMIX].valuename = " ";
	_STATE->parameters[SSBMODMIX].type = ParameterType_double;
	_STATE->parameters[SSBMODMIX].digits = 2;
	_STATE->parameters[SSBMODMIX].initvalue = .5;
	_STATE->parameters[SSBMODMIX].progress = .01;
	_STATE->parameters[SSBMODMIX].flags |= Param::MidiParam;

	_STATE->parameters[SSBMODRATE].category = ssb;
	_STATE->parameters[SSBMODRATE].min = LOG10D20F(0.01);
	_STATE->parameters[SSBMODRATE].max = LOG10D20F(20000.);
	_STATE->parameters[SSBMODRATE].name = "CARRIER RATE";
	_STATE->parameters[SSBMODRATE].valuename = "Hz";
	_STATE->parameters[SSBMODRATE].type = ParameterType_double;
	_STATE->parameters[SSBMODRATE].digits = 2;
	_STATE->parameters[SSBMODRATE].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SSBMODRATE].initvalue = 0;
	_STATE->parameters[SSBMODRATE].progress = .25;
	_STATE->parameters[SSBMODRATE].flags |= Param::MidiParam;

	_STATE->parameters[SSBMODTYPE].category = ssb;
	_STATE->parameters[SSBMODTYPE].min = 0;
	_STATE->parameters[SSBMODTYPE].max = 2;
	_STATE->parameters[SSBMODTYPE].name = "SIDEBAND";
	_STATE->parameters[SSBMODTYPE].valuename = " ";
	_STATE->parameters[SSBMODTYPE].digits = 0;
	_STATE->parameters[SSBMODTYPE].initvalue = 0;
	_STATE->parameters[SSBMODTYPE].progress = 1;
	_STATE->parameters[SSBMODTYPE].flags |= Param::MidiParam;
	_STATE->parameters[SSBMODTYPE].type = ParameterType_enum;
	_STATE->parameters[SSBMODTYPE].names = ssbmodeschars;

	const char* conv = "CONVOLUTION";
	_STATE->parameters[CREVMIX].category = conv;
	_STATE->parameters[CREVMIX].min = 0;
	_STATE->parameters[CREVMIX].max = 1;
	_STATE->parameters[CREVMIX].name = "MIX";
	_STATE->parameters[CREVMIX].valuename = " ";
	_STATE->parameters[CREVMIX].type = ParameterType_double;
	_STATE->parameters[CREVMIX].digits = 2;
	_STATE->parameters[CREVMIX].initvalue = .5;
	_STATE->parameters[CREVMIX].progress = .01;
	_STATE->parameters[CREVMIX].flags |= Param::MidiParam;

	_STATE->parameters[CREVGAIN].category = conv;
	_STATE->parameters[CREVGAIN].min = -60;
	_STATE->parameters[CREVGAIN].max = 60.;
	_STATE->parameters[CREVGAIN].name = "GAIN";
	_STATE->parameters[CREVGAIN].valuename = "dB";
	_STATE->parameters[CREVGAIN].type = ParameterType_double;
	_STATE->parameters[CREVGAIN].digits = 0;
	_STATE->parameters[CREVGAIN].initvalue = 0;
	_STATE->parameters[CREVGAIN].progress = 1;
	_STATE->parameters[CREVGAIN].flags |= Param::MidiParam;

	for (int32_t i = 0; i < 5; i++) {
		_STATE->parameters[CREVENVY0 + i].initvalue = 0;
		_STATE->parameters[CREVENVY0 + i].category = conv;
		_STATE->parameters[CREVENVY0 + i].valuename = "dB";
		_STATE->parameters[CREVENVY0 + i].min = -60;
		_STATE->parameters[CREVENVY0 + i].max = 0;
		_STATE->parameters[CREVENVX0 + i].category = conv;
		_STATE->parameters[CREVENVX0 + i].initvalue = i < 5 ? i * .25 : 1.;
		_STATE->parameters[CREVENVX0 + i].min = 0;
		_STATE->parameters[CREVENVX0 + i].max = 1;
		_STATE->parameters[CREVENVX0 + i].type = _STATE->parameters[CREVENVY0 + i].type = ParameterType_double;

	}

	_STATE->parameters[CREVREVERSE].name = "REVERSE";
	_STATE->parameters[CREVREVERSE].initvalue = 0;
	_STATE->parameters[CREVREVERSE].category = conv;

	auto seconds = 10000;


	initControl(_STATE, CREVMAXSIZE, "MAX", "ms", 50, seconds, seconds, 1, 0, Param::ParamCurve::Log10);
	_STATE->parameters[CREVMAXSIZE].category = conv;

	_STATE->parameters[CREVBOUNCE].category = conv;
	_STATE->parameters[CREVBOUNCE].name = "BOUNCE";
	_STATE->parameters[CREVBOUNCE].type = ParameterType_bool;

	/* CREVERB second space (CREVERB2). RATE/FRONT/REVERSE drive the same IR
	   replacement front GENCREVERB uses, so they are defined exactly like
	   SPECDELMORPHRATE/FRONT/REV - including the log curve with an offset,
	   which is what lets the bottom of RATE reach an exact 0 (= parked). */
	_STATE->parameters[CREVMORPHRATE].category = conv;
	_STATE->parameters[CREVMORPHRATE].min = LOG10D20F(SPECDELRATEOFFS);
	_STATE->parameters[CREVMORPHRATE].max = LOG10D20F(SPECDELRATEMAX + SPECDELRATEOFFS);
	/* short names: they label knobs now, and the CONVOLUTION category already
	   separates them from GENCREVERB's identically named RATE/FRONT */
	_STATE->parameters[CREVMORPHRATE].name = "RATE";
	_STATE->parameters[CREVMORPHRATE].valuename = "x";
	_STATE->parameters[CREVMORPHRATE].type = ParameterType_double;
	_STATE->parameters[CREVMORPHRATE].initvalue = LOG10D20F(1. + SPECDELRATEOFFS);
	_STATE->parameters[CREVMORPHRATE].progress = .5;
	_STATE->parameters[CREVMORPHRATE].digits = 2;
	_STATE->parameters[CREVMORPHRATE].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CREVMORPHRATE].offset = SPECDELRATEOFFS;
	_STATE->parameters[CREVMORPHRATE].flags |= Param::MidiParam;

	_STATE->parameters[CREVMORPHFRONT].category = conv;
	_STATE->parameters[CREVMORPHFRONT].min = 0.;
	_STATE->parameters[CREVMORPHFRONT].max = 1.;
	_STATE->parameters[CREVMORPHFRONT].name = "FRONT";
	_STATE->parameters[CREVMORPHFRONT].valuename = " ";
	_STATE->parameters[CREVMORPHFRONT].type = ParameterType_double;
	_STATE->parameters[CREVMORPHFRONT].initvalue = 1.;
	_STATE->parameters[CREVMORPHFRONT].progress = .01;
	_STATE->parameters[CREVMORPHFRONT].digits = 2;
	_STATE->parameters[CREVMORPHFRONT].flags |= Param::MidiParam;

	_STATE->parameters[CREVMORPHREV].category = conv;
	_STATE->parameters[CREVMORPHREV].name = "MORPH REVERSE";
	_STATE->parameters[CREVMORPHREV].type = ParameterType_bool;
	_STATE->parameters[CREVMORPHREV].flags |= Param::MidiParam;

	/* init 0 = RAW, i.e. the level behaviour every existing session already
	   has - normalisation is opt-in so no saved setting changes loudness */
	_STATE->parameters[CREVNORM].category = conv;
	_STATE->parameters[CREVNORM].name = "NORM";
	_STATE->parameters[CREVNORM].names = makeStringSpan(crevnormmodes);
	_STATE->parameters[CREVNORM].type = ParameterType_enum;
	_STATE->parameters[CREVNORM].initvalue = 0;
	_STATE->parameters[CREVNORM].flags |= Param::MidiParam;

	_STATE->parameters[CREVAUTO].category = conv;
	_STATE->parameters[CREVAUTO].name = "AUTO RELOAD";
	_STATE->parameters[CREVAUTO].type = ParameterType_bool;
	_STATE->parameters[CREVAUTO].initvalue = 1;

	const char* voc = "VOCODER";
	_STATE->parameters[VOCBW].category = voc;
	_STATE->parameters[VOCBW].min = 0;
	_STATE->parameters[VOCBW].max = 1.;
	_STATE->parameters[VOCBW].name = "Q";
	_STATE->parameters[VOCBW].valuename = " ";
	_STATE->parameters[VOCBW].type = ParameterType_double;
	_STATE->parameters[VOCBW].digits = 2;
	_STATE->parameters[VOCBW].initvalue = .5;
	_STATE->parameters[VOCBW].progress = .01;
	// _STATE->parameters[VOCBW].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOCBW].flags |= Param::MidiParam;

	_STATE->parameters[VOCA].category = voc;
	_STATE->parameters[VOCA].min = LOG10D20F(31.25);
	_STATE->parameters[VOCA].max = LOG10D20F(8000);
	_STATE->parameters[VOCA].name = "BOUND A";
	_STATE->parameters[VOCA].valuename = "Hz";
	_STATE->parameters[VOCA].type = ParameterType_double;
	_STATE->parameters[VOCA].digits = 0;
	_STATE->parameters[VOCA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOCA].initvalue = LOG10D20F(31.25);
	_STATE->parameters[VOCA].progress = 1;
	_STATE->parameters[VOCA].flags |= Param::MidiParam;

	_STATE->parameters[VOCB].category = voc;
	_STATE->parameters[VOCB].min = LOG10D20F(80);
	_STATE->parameters[VOCB].max = LOG10D20F(16000.);
	_STATE->parameters[VOCB].name = "BOUND B";
	_STATE->parameters[VOCB].valuename = "Hz";
	_STATE->parameters[VOCB].type = ParameterType_double;
	_STATE->parameters[VOCB].digits = 0;
	_STATE->parameters[VOCB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOCB].initvalue = LOG10D20F(16000);
	_STATE->parameters[VOCB].progress = 1;
	_STATE->parameters[VOCB].flags |= Param::MidiParam;

	_STATE->parameters[VOCCHANS].category = voc;
	_STATE->parameters[VOCCHANS].min = 4.;
	_STATE->parameters[VOCCHANS].max = 28.;
	_STATE->parameters[VOCCHANS].name = "CHANNELS";
	_STATE->parameters[VOCCHANS].valuename = " ";
	_STATE->parameters[VOCCHANS].type = ParameterType_double;
	_STATE->parameters[VOCCHANS].digits = 0;
	_STATE->parameters[VOCCHANS].flags |= Param::CastInt;
	_STATE->parameters[VOCCHANS].initvalue = 10;
	_STATE->parameters[VOCCHANS].progress = 1;
	_STATE->parameters[VOCCHANS].flags |= Param::MidiParam;

	_STATE->parameters[VOCATT].category = voc;
	_STATE->parameters[VOCATT].min = LOG10D20F(1.);
	_STATE->parameters[VOCATT].max = LOG10D20F(200.);
	_STATE->parameters[VOCATT].name = "ATTACK";
	_STATE->parameters[VOCATT].valuename = "ms";
	_STATE->parameters[VOCATT].type = ParameterType_double;
	_STATE->parameters[VOCATT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOCATT].digits = 1;
	_STATE->parameters[VOCATT].initvalue = LOG10D20F(3.);
	_STATE->parameters[VOCATT].progress = 1;
	_STATE->parameters[VOCATT].flags |= Param::MidiParam;

	_STATE->parameters[VOCREL].category = voc;
	_STATE->parameters[VOCREL].min = LOG10D20F(1.);
	_STATE->parameters[VOCREL].max = LOG10D20F(200.);
	_STATE->parameters[VOCREL].name = "RELEASE";
	_STATE->parameters[VOCREL].valuename = "ms";
	_STATE->parameters[VOCREL].type = ParameterType_double;
	_STATE->parameters[VOCREL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOCREL].digits = 1;
	_STATE->parameters[VOCREL].initvalue = LOG10D20F(10.);
	_STATE->parameters[VOCREL].progress = 1;
	_STATE->parameters[VOCREL].flags |= Param::MidiParam;

	_STATE->parameters[VOCMIX].category = voc;
	_STATE->parameters[VOCMIX].min = 0;
	_STATE->parameters[VOCMIX].max = 1.;
	_STATE->parameters[VOCMIX].name = "MIX";
	_STATE->parameters[VOCMIX].valuename = " ";
	_STATE->parameters[VOCMIX].type = ParameterType_double;
	_STATE->parameters[VOCMIX].digits = 2;
	_STATE->parameters[VOCMIX].initvalue = 1;
	_STATE->parameters[VOCMIX].progress = .01;
	_STATE->parameters[VOCMIX].flags |= Param::MidiParam;

	_STATE->parameters[VOCGAIN].category = voc;
	_STATE->parameters[VOCGAIN].min = -60;
	_STATE->parameters[VOCGAIN].max = 60.;
	_STATE->parameters[VOCGAIN].name = "GAIN";
	_STATE->parameters[VOCGAIN].valuename = "dB";
	_STATE->parameters[VOCGAIN].type = ParameterType_double;
	_STATE->parameters[VOCGAIN].digits = 0;
	_STATE->parameters[VOCGAIN].initvalue = 0;
	_STATE->parameters[VOCGAIN].progress = 1;
	_STATE->parameters[VOCGAIN].flags |= Param::MidiParam;

	const char* moog = "MOOG LADDER";

	_STATE->parameters[MOOGRES].category = moog;
	_STATE->parameters[MOOGRES].min = 0;
	_STATE->parameters[MOOGRES].max = 1.;
	_STATE->parameters[MOOGRES].name = "RES";
	_STATE->parameters[MOOGRES].valuename = " ";
	_STATE->parameters[MOOGRES].type = ParameterType_double;
	_STATE->parameters[MOOGRES].digits = 2;
	_STATE->parameters[MOOGRES].initvalue = .5;
	_STATE->parameters[MOOGRES].progress = .01;
	_STATE->parameters[MOOGRES].flags |= Param::MidiParam;

	_STATE->parameters[MOOGCUT].category = moog;
	_STATE->parameters[MOOGCUT].min = LOG10D20F(100.);
	_STATE->parameters[MOOGCUT].max = LOG10D20F(10000.);
	_STATE->parameters[MOOGCUT].name = "CUT";
	_STATE->parameters[MOOGCUT].valuename = "Hz";
	_STATE->parameters[MOOGCUT].type = ParameterType_double;
	_STATE->parameters[MOOGCUT].digits = 0;
	_STATE->parameters[MOOGCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[MOOGCUT].initvalue = LOG10D20F(1000.);
	_STATE->parameters[MOOGCUT].progress = .5;
	_STATE->parameters[MOOGCUT].flags |= Param::MidiParam;

	_STATE->parameters[MOOGMIX].category = moog;
	_STATE->parameters[MOOGMIX].min = 0;
	_STATE->parameters[MOOGMIX].max = 1.;
	_STATE->parameters[MOOGMIX].name = "MIX";
	_STATE->parameters[MOOGMIX].valuename = " ";
	_STATE->parameters[MOOGMIX].type = ParameterType_double;
	_STATE->parameters[MOOGMIX].digits = 2;
	_STATE->parameters[MOOGMIX].initvalue = .5;
	_STATE->parameters[MOOGMIX].progress = .01;
	_STATE->parameters[MOOGMIX].flags |= Param::MidiParam;

	_STATE->parameters[MOOGGAIN].category = moog;
	_STATE->parameters[MOOGGAIN].min = -60;
	_STATE->parameters[MOOGGAIN].max = 60.;
	_STATE->parameters[MOOGGAIN].name = "GAIN";
	_STATE->parameters[MOOGGAIN].valuename = "dB";
	_STATE->parameters[MOOGGAIN].type = ParameterType_double;
	_STATE->parameters[MOOGGAIN].digits = 0;
	_STATE->parameters[MOOGGAIN].initvalue = 0;
	_STATE->parameters[MOOGGAIN].progress = 1;
	_STATE->parameters[MOOGGAIN].flags |= Param::MidiParam;

	const char* ph4 = "PHASER4";

	_STATE->parameters[PHASER4FB].category = ph4;
	_STATE->parameters[PHASER4FB].min = 0;
	_STATE->parameters[PHASER4FB].max = 1.;
	_STATE->parameters[PHASER4FB].name = "FEEDBACK";
	_STATE->parameters[PHASER4FB].valuename = " ";
	_STATE->parameters[PHASER4FB].type = ParameterType_double;
	_STATE->parameters[PHASER4FB].digits = 2;
	_STATE->parameters[PHASER4FB].initvalue = .5;
	_STATE->parameters[PHASER4FB].progress = .01;
	_STATE->parameters[PHASER4FB].flags |= Param::MidiParam;

	_STATE->parameters[PHASER4BAND].category = ph4;
	_STATE->parameters[PHASER4BAND].min = LOG10D20F(20.);
	_STATE->parameters[PHASER4BAND].max = LOG10D20F(5000.);
	_STATE->parameters[PHASER4BAND].name = "BAND";
	_STATE->parameters[PHASER4BAND].valuename = "Hz";
	_STATE->parameters[PHASER4BAND].type = ParameterType_double;
	_STATE->parameters[PHASER4BAND].digits = 0;
	_STATE->parameters[PHASER4BAND].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PHASER4BAND].initvalue = LOG10D20F(500.);
	_STATE->parameters[PHASER4BAND].progress = .25;
	_STATE->parameters[PHASER4BAND].flags |= Param::MidiParam;

	_STATE->parameters[PHASER4MIX].category = ph4;
	_STATE->parameters[PHASER4MIX].min = 0;
	_STATE->parameters[PHASER4MIX].max = 1.;
	_STATE->parameters[PHASER4MIX].name = "MIX";
	_STATE->parameters[PHASER4MIX].valuename = " ";
	_STATE->parameters[PHASER4MIX].type = ParameterType_double;
	_STATE->parameters[PHASER4MIX].digits = 2;
	_STATE->parameters[PHASER4MIX].initvalue = .5f;
	_STATE->parameters[PHASER4MIX].progress = .05;
	_STATE->parameters[PHASER4MIX].flags |= Param::MidiParam;

	_STATE->parameters[PHASER4STAGES].category = ph4;
	_STATE->parameters[PHASER4STAGES].min = 1;
	_STATE->parameters[PHASER4STAGES].max = 32.;
	_STATE->parameters[PHASER4STAGES].name = "STAGES";
	_STATE->parameters[PHASER4STAGES].valuename = " ";
	_STATE->parameters[PHASER4STAGES].type = ParameterType_double;
	_STATE->parameters[PHASER4STAGES].digits = 0;
	_STATE->parameters[PHASER4STAGES].flags |= Param::CastInt;
	_STATE->parameters[PHASER4STAGES].initvalue = 4;
	_STATE->parameters[PHASER4STAGES].progress = 1;
	_STATE->parameters[PHASER4STAGES].flags |= Param::MidiParam;

	_STATE->parameters[PHASER4SPACING].category = ph4;
	_STATE->parameters[PHASER4SPACING].min = 0;
	_STATE->parameters[PHASER4SPACING].max = 2.;
	_STATE->parameters[PHASER4SPACING].name = "DISTANCE";
	_STATE->parameters[PHASER4SPACING].valuename = " ";
	_STATE->parameters[PHASER4SPACING].type = ParameterType_double;
	_STATE->parameters[PHASER4SPACING].digits = 2;
	_STATE->parameters[PHASER4SPACING].initvalue = 1;
	_STATE->parameters[PHASER4SPACING].progress = .02;
	_STATE->parameters[PHASER4SPACING].flags |= Param::MidiParam;

	_STATE->parameters[PHASER4RADIUS].category = ph4;
	_STATE->parameters[PHASER4RADIUS].min = 0;
	_STATE->parameters[PHASER4RADIUS].max = 1.;
	_STATE->parameters[PHASER4RADIUS].name = "RADIUS";
	_STATE->parameters[PHASER4RADIUS].valuename = " ";
	_STATE->parameters[PHASER4RADIUS].type = ParameterType_double;
	_STATE->parameters[PHASER4RADIUS].digits = 2;
	_STATE->parameters[PHASER4RADIUS].initvalue = .5;
	_STATE->parameters[PHASER4RADIUS].progress = .05;
	_STATE->parameters[PHASER4RADIUS].flags |= Param::MidiParam;

	const char* phaser = "PHASER";

	_STATE->parameters[PHASERFB].category = phaser;
	_STATE->parameters[PHASERFB].min = -1.;
	_STATE->parameters[PHASERFB].max = 1.;
	_STATE->parameters[PHASERFB].name = "FEEDBACK";
	_STATE->parameters[PHASERFB].valuename = " ";
	_STATE->parameters[PHASERFB].type = ParameterType_double;
	_STATE->parameters[PHASERFB].digits = 2;
	_STATE->parameters[PHASERFB].initvalue = .5;
	_STATE->parameters[PHASERFB].progress = .01;
	_STATE->parameters[PHASERFB].flags |= Param::MidiParam;

	_STATE->parameters[PHASERBAND].category = phaser;
	_STATE->parameters[PHASERBAND].min = LOG10D20F(20.);
	_STATE->parameters[PHASERBAND].max = LOG10D20F(20000.);
	_STATE->parameters[PHASERBAND].name = "BAND";
	_STATE->parameters[PHASERBAND].valuename = "Hz";
	_STATE->parameters[PHASERBAND].type = ParameterType_double;
	_STATE->parameters[PHASERBAND].digits = 0;
	_STATE->parameters[PHASERBAND].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PHASERBAND].initvalue = LOG10D20F(5000);
	_STATE->parameters[PHASERBAND].progress = 1;
	_STATE->parameters[PHASERBAND].flags |= Param::MidiParam;

	_STATE->parameters[PHASERMIX].category = phaser;
	_STATE->parameters[PHASERMIX].min = 0;
	_STATE->parameters[PHASERMIX].max = 1.;
	_STATE->parameters[PHASERMIX].name = "MIX";
	_STATE->parameters[PHASERMIX].valuename = " ";
	_STATE->parameters[PHASERMIX].type = ParameterType_double;
	_STATE->parameters[PHASERMIX].digits = 2;
	_STATE->parameters[PHASERMIX].initvalue = .5f;
	_STATE->parameters[PHASERMIX].progress = .05;
	_STATE->parameters[PHASERMIX].flags |= Param::MidiParam;

	_STATE->parameters[PHASERNOTCHES].category = phaser;
	_STATE->parameters[PHASERNOTCHES].min = 1;
	_STATE->parameters[PHASERNOTCHES].max = 32.;
	_STATE->parameters[PHASERNOTCHES].name = "NOTCHES";
	_STATE->parameters[PHASERNOTCHES].valuename = " ";
	_STATE->parameters[PHASERNOTCHES].type = ParameterType_double;
	_STATE->parameters[PHASERNOTCHES].digits = 0;
	_STATE->parameters[PHASERNOTCHES].flags |= Param::CastInt;
	_STATE->parameters[PHASERNOTCHES].initvalue = 4;
	_STATE->parameters[PHASERNOTCHES].progress = 1;
	_STATE->parameters[PHASERNOTCHES].flags |= Param::MidiParam;

	const char* eq10 = "EQ10";

	const char* eq10_names[] = { "32 ", "64 ", "125", "250", "500", "1K", "2K", "4K", "8K",
								"16K",
								"GAIN", "Q" };

	for (int32_t stage = 0; stage < 10; stage++) {
		_STATE->parameters[EQ10_0 + stage].category = eq10;
		_STATE->parameters[EQ10_0 + stage].min = -24;
		_STATE->parameters[EQ10_0 + stage].max = 24.;
		_STATE->parameters[EQ10_0 + stage].name = eq10_names[stage];
		_STATE->parameters[EQ10_0 + stage].valuename = "dB";
		_STATE->parameters[EQ10_0 + stage].type = ParameterType_double;
		_STATE->parameters[EQ10_0 + stage].initvalue = 0;
		_STATE->parameters[EQ10_0 + stage].progress = 1;
		_STATE->parameters[EQ10_0 + stage].flags |= Param::MidiParam;

	}

	_STATE->parameters[EQ10_Q].category = eq10;
	_STATE->parameters[EQ10_Q].min = LOG10D20F(.01);
	_STATE->parameters[EQ10_Q].max = LOG10D20F(10.);
	_STATE->parameters[EQ10_Q].name = "Q";
	_STATE->parameters[EQ10_Q].valuename = " ";
	_STATE->parameters[EQ10_Q].type = ParameterType_double;
	_STATE->parameters[EQ10_Q].initvalue = LOG10D20F(1.);
	_STATE->parameters[EQ10_Q].progress = 1;
	_STATE->parameters[EQ10_Q].digits = 2;
	_STATE->parameters[EQ10_Q].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[EQ10_Q].flags |= Param::MidiParam;

	_STATE->parameters[EQ10_GAIN].category = eq10;
	_STATE->parameters[EQ10_GAIN].min = -60.;
	_STATE->parameters[EQ10_GAIN].max = 60.;
	_STATE->parameters[EQ10_GAIN].name = "GAIN";
	_STATE->parameters[EQ10_GAIN].valuename = "dB";
	_STATE->parameters[EQ10_GAIN].type = ParameterType_double;
	_STATE->parameters[EQ10_GAIN].initvalue = 0;
	_STATE->parameters[EQ10_GAIN].progress = 1;
	_STATE->parameters[EQ10_GAIN].flags |= Param::MidiParam;


	const char* stc = "STEREO COMPRESSOR";

	_STATE->parameters[STCOMPTHR].category = stc;
	_STATE->parameters[STCOMPTHR].min = -60;
	_STATE->parameters[STCOMPTHR].max = LOG10D20F(1.);
	_STATE->parameters[STCOMPTHR].name = "THRESHOLD";
	_STATE->parameters[STCOMPTHR].valuename = "dB";
	_STATE->parameters[STCOMPTHR].type = ParameterType_double;
	_STATE->parameters[STCOMPTHR].flags |= Param::MidiParam;
	_STATE->parameters[STCOMPTHR].initvalue = -20;
	_STATE->parameters[STCOMPTHR].progress = 1;

	_STATE->parameters[STCOMPMAKE].category = stc;
	_STATE->parameters[STCOMPMAKE].min = 0;
	_STATE->parameters[STCOMPMAKE].max = 60;
	_STATE->parameters[STCOMPMAKE].name = "MAKEUP";
	_STATE->parameters[STCOMPMAKE].valuename = "dB";
	_STATE->parameters[STCOMPMAKE].type = ParameterType_double;
	_STATE->parameters[STCOMPMAKE].flags |= Param::MidiParam;
	_STATE->parameters[STCOMPMAKE].initvalue = 0;
	_STATE->parameters[STCOMPMAKE].progress = 1;

	_STATE->parameters[STCOMPKNEE].category = stc;
	_STATE->parameters[STCOMPKNEE].min = 0;
	_STATE->parameters[STCOMPKNEE].max = 60.;
	_STATE->parameters[STCOMPKNEE].name = "KNEE";
	_STATE->parameters[STCOMPKNEE].valuename = "dB";
	_STATE->parameters[STCOMPKNEE].type = ParameterType_double;
	_STATE->parameters[STCOMPKNEE].flags |= Param::MidiParam;
	_STATE->parameters[STCOMPKNEE].initvalue = 10;
	_STATE->parameters[STCOMPKNEE].progress = 1;


	_STATE->parameters[STCOMPRMS].category = stc;
	_STATE->parameters[STCOMPRMS].min = 0.;
	_STATE->parameters[STCOMPRMS].max = 1000.;
	_STATE->parameters[STCOMPRMS].name = "RMS SIZE";
	_STATE->parameters[STCOMPRMS].valuename = "ms";
	_STATE->parameters[STCOMPRMS].type = ParameterType_double;
	_STATE->parameters[STCOMPRMS].digits = 0;
	_STATE->parameters[STCOMPRMS].initvalue = 50;
	_STATE->parameters[STCOMPRMS].progress = 10;
	_STATE->parameters[STCOMPRMS].flags |= Param::MidiParam;

	_STATE->parameters[STCOMPATT].category = stc;
	_STATE->parameters[STCOMPATT].min = LOG10D20F(0.1);
	_STATE->parameters[STCOMPATT].max = LOG10D20F(70.);
	_STATE->parameters[STCOMPATT].name = "ATTACK";
	_STATE->parameters[STCOMPATT].valuename = "ms";
	_STATE->parameters[STCOMPATT].type = ParameterType_double;
	_STATE->parameters[STCOMPATT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[STCOMPATT].digits = 1;
	_STATE->parameters[STCOMPATT].initvalue = LOG10D20F(3.);
	_STATE->parameters[STCOMPATT].progress = 1;
	_STATE->parameters[STCOMPATT].flags |= Param::MidiParam;

	_STATE->parameters[STCOMPDEC].category = stc;
	_STATE->parameters[STCOMPDEC].min = LOG10D20F(1.);
	_STATE->parameters[STCOMPDEC].max = LOG10D20F(200.);
	_STATE->parameters[STCOMPDEC].name = "RELEASE";
	_STATE->parameters[STCOMPDEC].valuename = "ms";
	_STATE->parameters[STCOMPDEC].type = ParameterType_double;
	_STATE->parameters[STCOMPDEC].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[STCOMPDEC].digits = 1;
	_STATE->parameters[STCOMPDEC].initvalue = LOG10D20F(10.);
	_STATE->parameters[STCOMPDEC].progress = 1;
	_STATE->parameters[STCOMPDEC].flags |= Param::MidiParam;


	_STATE->parameters[STCOMPRATIO].category = stc;
	_STATE->parameters[STCOMPRATIO].min = 0;
	_STATE->parameters[STCOMPRATIO].max = 1.;
	_STATE->parameters[STCOMPRATIO].name = "RATIO";
	_STATE->parameters[STCOMPRATIO].valuename = " ";
	_STATE->parameters[STCOMPRATIO].type = ParameterType_double;
	_STATE->parameters[STCOMPRATIO].flags |= Param::MidiParam;
	_STATE->parameters[STCOMPRATIO].initvalue = .5;
	_STATE->parameters[STCOMPRATIO].progress = .01;
	_STATE->parameters[STCOMPRATIO].flags |= Param::MidiParam;


	_STATE->parameters[STCOMPLOOKA].category = stc;
	_STATE->parameters[STCOMPLOOKA].min = 0.;
	_STATE->parameters[STCOMPLOOKA].max = 1000.;
	_STATE->parameters[STCOMPLOOKA].name = "LOOKAHEAD";
	_STATE->parameters[STCOMPLOOKA].valuename = "ms";
	_STATE->parameters[STCOMPLOOKA].type = ParameterType_double;
	_STATE->parameters[STCOMPLOOKA].digits = 0;
	_STATE->parameters[STCOMPLOOKA].initvalue = 0;
	_STATE->parameters[STCOMPLOOKA].progress = 1;
	_STATE->parameters[STCOMPLOOKA].flags |= Param::MidiParam;

	const char* r1 = "REVERB1";

	_STATE->parameters[REV1DAMP].category = r1;
	_STATE->parameters[REV1DAMP].min = 0;
	_STATE->parameters[REV1DAMP].max = 1;
	_STATE->parameters[REV1DAMP].name = "DAMPING";
	_STATE->parameters[REV1DAMP].valuename = " ";
	_STATE->parameters[REV1DAMP].type = ParameterType_double;
	_STATE->parameters[REV1DAMP].digits = 2;
	_STATE->parameters[REV1DAMP].initvalue = .5;
	_STATE->parameters[REV1DAMP].progress = .1;
	_STATE->parameters[REV1DAMP].flags |= Param::MidiParam;

	_STATE->parameters[REV1MIX].category = r1;
	_STATE->parameters[REV1MIX].min = 0;
	_STATE->parameters[REV1MIX].max = 1;
	_STATE->parameters[REV1MIX].name = "MIX";
	_STATE->parameters[REV1MIX].valuename = " ";
	_STATE->parameters[REV1MIX].type = ParameterType_double;
	_STATE->parameters[REV1MIX].digits = 2;
	_STATE->parameters[REV1MIX].initvalue = .5f;
	_STATE->parameters[REV1MIX].progress = .01;
	_STATE->parameters[REV1MIX].flags |= Param::MidiParam;

	_STATE->parameters[REV1GAIN].category = r1;
	_STATE->parameters[REV1GAIN].min = -60;
	_STATE->parameters[REV1GAIN].max = 60.;
	_STATE->parameters[REV1GAIN].name = "GAIN";
	_STATE->parameters[REV1GAIN].valuename = "dB";
	_STATE->parameters[REV1GAIN].type = ParameterType_double;
	_STATE->parameters[REV1GAIN].initvalue = 0;
	_STATE->parameters[REV1GAIN].progress = 1;
	_STATE->parameters[REV1GAIN].flags |= Param::MidiParam;

	const char* r2 = "REVERB2";

	_STATE->parameters[REV2MIX].category = r2;
	_STATE->parameters[REV2MIX].min = 0;
	_STATE->parameters[REV2MIX].max = 1;
	_STATE->parameters[REV2MIX].name = "MIX";
	_STATE->parameters[REV2MIX].valuename = " ";
	_STATE->parameters[REV2MIX].type = ParameterType_double;
	_STATE->parameters[REV2MIX].digits = 2;
	_STATE->parameters[REV2MIX].initvalue = .5f;
	_STATE->parameters[REV2MIX].progress = .01;
	_STATE->parameters[REV2MIX].flags |= Param::MidiParam;

	_STATE->parameters[REV2GAIN].category = r2;
	_STATE->parameters[REV2GAIN].min = -60;
	_STATE->parameters[REV2GAIN].max = 60.;
	_STATE->parameters[REV2GAIN].name = "GAIN";
	_STATE->parameters[REV2GAIN].valuename = "dB";
	_STATE->parameters[REV2GAIN].type = ParameterType_double;
	_STATE->parameters[REV2GAIN].initvalue = 0;
	_STATE->parameters[REV2GAIN].progress = 1;
	_STATE->parameters[REV2GAIN].flags |= Param::MidiParam;

	const char* r3 = "REVERB3";

	_STATE->parameters[REV3REF].category = r3;
	_STATE->parameters[REV3REF].min = 0;
	_STATE->parameters[REV3REF].max = 1;
	_STATE->parameters[REV3REF].name = "FEEDBACK";
	_STATE->parameters[REV3REF].valuename = " ";
	_STATE->parameters[REV3REF].type = ParameterType_double;
	_STATE->parameters[REV3REF].digits = 2;
	_STATE->parameters[REV3REF].initvalue = .5;
	_STATE->parameters[REV3REF].progress = .01;
	_STATE->parameters[REV3REF].flags |= Param::MidiParam;

	_STATE->parameters[REV3MIX].category = r3;
	_STATE->parameters[REV3MIX].min = 0;
	_STATE->parameters[REV3MIX].max = 1;
	_STATE->parameters[REV3MIX].name = "MIX";
	_STATE->parameters[REV3MIX].valuename = " ";
	_STATE->parameters[REV3MIX].type = ParameterType_double;
	_STATE->parameters[REV3MIX].digits = 2;
	_STATE->parameters[REV3MIX].initvalue = .5f;
	_STATE->parameters[REV3MIX].progress = .01;
	_STATE->parameters[REV3MIX].flags |= Param::MidiParam;

	_STATE->parameters[REV3GAIN].category = r3;
	_STATE->parameters[REV3GAIN].min = -60;
	_STATE->parameters[REV3GAIN].max = 60.;
	_STATE->parameters[REV3GAIN].name = "GAIN";
	_STATE->parameters[REV3GAIN].valuename = "dB";
	_STATE->parameters[REV3GAIN].type = ParameterType_double;
	_STATE->parameters[REV3GAIN].initvalue = 0;
	_STATE->parameters[REV3GAIN].progress = 1;
	_STATE->parameters[REV3GAIN].flags |= Param::MidiParam;

	const char* r4 = "REVERB4";

	_STATE->parameters[REV4REF].category = r4;
	_STATE->parameters[REV4REF].min = 0;
	_STATE->parameters[REV4REF].max = 1;
	_STATE->parameters[REV4REF].name = "FEEDBACK";
	_STATE->parameters[REV4REF].valuename = " ";
	_STATE->parameters[REV4REF].type = ParameterType_double;
	_STATE->parameters[REV4REF].digits = 2;
	_STATE->parameters[REV4REF].initvalue = .5;
	_STATE->parameters[REV4REF].progress = .01;
	_STATE->parameters[REV4REF].flags |= Param::MidiParam;

	_STATE->parameters[REV4MIX].category = r4;
	_STATE->parameters[REV4MIX].min = 0;
	_STATE->parameters[REV4MIX].max = 1;
	_STATE->parameters[REV4MIX].name = "MIX";
	_STATE->parameters[REV4MIX].valuename = " ";
	_STATE->parameters[REV4MIX].type = ParameterType_double;
	_STATE->parameters[REV4MIX].digits = 2;
	_STATE->parameters[REV4MIX].initvalue = .5f;
	_STATE->parameters[REV4MIX].progress = .01;
	_STATE->parameters[REV4MIX].flags |= Param::MidiParam;

	_STATE->parameters[REV4GAIN].category = r4;
	_STATE->parameters[REV4GAIN].min = -60;
	_STATE->parameters[REV4GAIN].max = 60.;
	_STATE->parameters[REV4GAIN].name = "GAIN";
	_STATE->parameters[REV4GAIN].valuename = "dB";
	_STATE->parameters[REV4GAIN].type = ParameterType_double;
	_STATE->parameters[REV4GAIN].initvalue = 0;
	_STATE->parameters[REV4GAIN].progress = 1;
	_STATE->parameters[REV4GAIN].flags |= Param::MidiParam;

	const char* r5 = "REVERB5";

	_STATE->parameters[REVERB5FB].category = r5;
	_STATE->parameters[REVERB5FB].min = 0;
	_STATE->parameters[REVERB5FB].max = 1;
	_STATE->parameters[REVERB5FB].name = "FEEDBACK";
	_STATE->parameters[REVERB5FB].valuename = " ";
	_STATE->parameters[REVERB5FB].type = ParameterType_double;
	_STATE->parameters[REVERB5FB].digits = 2;
	_STATE->parameters[REVERB5FB].initvalue = .5;
	_STATE->parameters[REVERB5FB].progress = .1;
	_STATE->parameters[REVERB5FB].flags |= Param::MidiParam;

	_STATE->parameters[REVERB5MIX].category = r5;
	_STATE->parameters[REVERB5MIX].min = 0;
	_STATE->parameters[REVERB5MIX].max = 1;
	_STATE->parameters[REVERB5MIX].name = "MIX";
	_STATE->parameters[REVERB5MIX].valuename = " ";
	_STATE->parameters[REVERB5MIX].type = ParameterType_double;
	_STATE->parameters[REVERB5MIX].digits = 2;
	_STATE->parameters[REVERB5MIX].initvalue = .5;
	_STATE->parameters[REVERB5MIX].progress = .01;
	_STATE->parameters[REVERB5MIX].flags |= Param::MidiParam;

	_STATE->parameters[REVERB5GAIN].category = r5;
	_STATE->parameters[REVERB5GAIN].min = -60;
	_STATE->parameters[REVERB5GAIN].max = 60.;
	_STATE->parameters[REVERB5GAIN].name = "GAIN";
	_STATE->parameters[REVERB5GAIN].valuename = "dB";
	_STATE->parameters[REVERB5GAIN].type = ParameterType_double;
	_STATE->parameters[REVERB5GAIN].initvalue = 0;
	_STATE->parameters[REVERB5GAIN].progress = 1;
	_STATE->parameters[REVERB5GAIN].flags |= Param::MidiParam;

	const char* chorus = "CHORUS";

	_STATE->parameters[CHORUSINT].category = chorus;
	_STATE->parameters[CHORUSINT].min = 0;
	_STATE->parameters[CHORUSINT].max = 1;
	_STATE->parameters[CHORUSINT].name = "DEPTH";
	_STATE->parameters[CHORUSINT].valuename = " ";
	_STATE->parameters[CHORUSINT].type = ParameterType_double;
	_STATE->parameters[CHORUSINT].digits = 2;
	_STATE->parameters[CHORUSINT].initvalue = .5;
	_STATE->parameters[CHORUSINT].progress = .05;
	_STATE->parameters[CHORUSINT].flags |= Param::MidiParam;

	_STATE->parameters[CHORUSMIX].category = chorus;
	_STATE->parameters[CHORUSMIX].min = 0;
	_STATE->parameters[CHORUSMIX].max = 1;
	_STATE->parameters[CHORUSMIX].name = "MIX";
	_STATE->parameters[CHORUSMIX].valuename = " ";
	_STATE->parameters[CHORUSMIX].type = ParameterType_double;
	_STATE->parameters[CHORUSMIX].digits = 2;
	_STATE->parameters[CHORUSMIX].initvalue = .5f;
	_STATE->parameters[CHORUSMIX].progress = .01;
	_STATE->parameters[CHORUSMIX].flags |= Param::MidiParam;

	_STATE->parameters[CHORUSGAIN].category = chorus;
	_STATE->parameters[CHORUSGAIN].min = -60;
	_STATE->parameters[CHORUSGAIN].max = 60.;
	_STATE->parameters[CHORUSGAIN].name = "GAIN";
	_STATE->parameters[CHORUSGAIN].valuename = "dB";
	_STATE->parameters[CHORUSGAIN].type = ParameterType_double;
	_STATE->parameters[CHORUSGAIN].initvalue = 0;
	_STATE->parameters[CHORUSGAIN].progress = 1;
	_STATE->parameters[CHORUSGAIN].flags |= Param::MidiParam;

	_STATE->parameters[CHORUSMOD].category = chorus;
	_STATE->parameters[CHORUSMOD].name = "MODULATION";
	_STATE->parameters[CHORUSMOD].type = ParameterType_enum;
	_STATE->parameters[CHORUSMOD].names = std::span<const std::string_view>(vcoWaveforms, 2);
	_STATE->parameters[CHORUSMOD].flags |= Param::MidiParam;

	const char* cross = "CROSS";
	const char* pv = "PV";

	const char* ipol = "INTERPOLATION";

	_STATE->parameters[IPOL].category = cross;
	_STATE->parameters[IPOL].subcategory = ipol;
	_STATE->parameters[IPOL].min = 0;
	_STATE->parameters[IPOL].max = 1;
	_STATE->parameters[IPOL].name = "IPOL";
	_STATE->parameters[IPOL].valuename = " ";
	_STATE->parameters[IPOL].type = ParameterType_double;
	_STATE->parameters[IPOL].digits = 2;
	_STATE->parameters[IPOL].flags |= Param::MidiParam;
	_STATE->parameters[IPOL].initvalue = .5;
	_STATE->parameters[IPOL].progress = .01;

	_STATE->parameters[VOC2DRIVE].category = cross;
	_STATE->parameters[VOC2DRIVE].subcategory = voc;
	_STATE->parameters[VOC2DRIVE].min = -60;
	_STATE->parameters[VOC2DRIVE].max = 60;
	_STATE->parameters[VOC2DRIVE].name = "DRIVE";
	_STATE->parameters[VOC2DRIVE].valuename = "dB";
	_STATE->parameters[VOC2DRIVE].type = ParameterType_double;
	_STATE->parameters[VOC2DRIVE].flags |= Param::MidiParam;
	_STATE->parameters[VOC2DRIVE].initvalue = 0;
	_STATE->parameters[VOC2DRIVE].progress = 1.;

	_STATE->parameters[VOC2HP].category = cross;
	_STATE->parameters[VOC2HP].subcategory = voc;
	_STATE->parameters[VOC2HP].min = LOG10D20F(1000.);
	_STATE->parameters[VOC2HP].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[VOC2HP].name = "HP THROUGH";
	_STATE->parameters[VOC2HP].valuename = "Hz";
	_STATE->parameters[VOC2HP].type = ParameterType_double;
	_STATE->parameters[VOC2HP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[VOC2HP].flags |= Param::MidiParam;
	_STATE->parameters[VOC2HP].initvalue = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[VOC2HP].progress = 1.;
	_STATE->parameters[VOC2HP].flags |= Param::MidiParam;

	_STATE->parameters[VOC2CHANS].category = cross;
	_STATE->parameters[VOC2CHANS].subcategory = voc;
	_STATE->parameters[VOC2CHANS].initvalue = 10.;
	_STATE->parameters[VOC2CHANS].name = "CHANNELS";
	_STATE->parameters[VOC2CHANS].type = ParameterType_enum;

	const char* cep = "CEPSTRUM";
	const char* cepw = "CEPSTRUM W";

	_STATE->parameters[CUTOFFSRC].category = cross;
	_STATE->parameters[CUTOFFSRC].subcategory = cepw;
	_STATE->parameters[CUTOFFSRC].min = .01;
	_STATE->parameters[CUTOFFSRC].max = .85;
	_STATE->parameters[CUTOFFSRC].name = "CUT SRC";
	_STATE->parameters[CUTOFFSRC].valuename = " ";
	_STATE->parameters[CUTOFFSRC].type = ParameterType_double;
	_STATE->parameters[CUTOFFSRC].digits = 2;
	_STATE->parameters[CUTOFFSRC].flags |= Param::MidiParam;
	_STATE->parameters[CUTOFFSRC].initvalue = .04;
	_STATE->parameters[CUTOFFSRC].progress = .01;

	_STATE->parameters[CUTOFFMOD].category = cross;
	_STATE->parameters[CUTOFFMOD].subcategory = cepw;
	_STATE->parameters[CUTOFFMOD].min = .01;
	_STATE->parameters[CUTOFFMOD].max = .85;
	_STATE->parameters[CUTOFFMOD].name = "CUT MOD";
	_STATE->parameters[CUTOFFMOD].valuename = " ";
	_STATE->parameters[CUTOFFMOD].type = ParameterType_double;
	_STATE->parameters[CUTOFFMOD].digits = 2;
	_STATE->parameters[CUTOFFMOD].flags |= Param::MidiParam;
	_STATE->parameters[CUTOFFMOD].initvalue = .04;
	_STATE->parameters[CUTOFFMOD].progress = .01;

	const char* form = "FORMANT SHIFT";

	_STATE->parameters[CUTSTRETCH].category = pv;
	_STATE->parameters[CUTSTRETCH].subcategory = form;
	_STATE->parameters[CUTSTRETCH].min = 0;
	_STATE->parameters[CUTSTRETCH].max = 1;
	_STATE->parameters[CUTSTRETCH].name = "CUT OFF";
	_STATE->parameters[CUTSTRETCH].valuename = " ";
	_STATE->parameters[CUTSTRETCH].type = ParameterType_double;
	_STATE->parameters[CUTSTRETCH].digits = 3;
	_STATE->parameters[CUTSTRETCH].flags |= Param::MidiParam;
	_STATE->parameters[CUTSTRETCH].initvalue = .1;
	_STATE->parameters[CUTSTRETCH].progress = .01;

	_STATE->parameters[STRETCHCOEFF].category = pv;
	_STATE->parameters[STRETCHCOEFF].subcategory = form;
	_STATE->parameters[STRETCHCOEFF].min = LOG10D20F(.1);
	_STATE->parameters[STRETCHCOEFF].max = LOG10D20F(5);
	_STATE->parameters[STRETCHCOEFF].name = "STRETCH";
	_STATE->parameters[STRETCHCOEFF].valuename = "x";
	_STATE->parameters[STRETCHCOEFF].type = ParameterType_double;
	_STATE->parameters[STRETCHCOEFF].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[STRETCHCOEFF].digits = 2;
	_STATE->parameters[STRETCHCOEFF].flags |= Param::MidiParam;
	_STATE->parameters[STRETCHCOEFF].initvalue = 0;
	_STATE->parameters[STRETCHCOEFF].progress = .25;

	// Log10 curve: the STORED value is 20*log10(ms), so the knob (which is linear
	// in the stored domain) gives fine control down at the fast end. 0.25 ms is the
	// floor because Follower::setAttackTime adds 0.25 ms before building the
	// coefficient -- below that the offset, not the knob, sets the time.
	_STATE->parameters[FOLLOWERATT].min = LOG10D20F(0.25);
	_STATE->parameters[FOLLOWERATT].max = LOG10D20F(2000.);
	_STATE->parameters[FOLLOWERATT].name = "ATTACK";
	_STATE->parameters[FOLLOWERATT].valuename = "ms";
	_STATE->parameters[FOLLOWERATT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FOLLOWERATT].type = ParameterType_double;
	_STATE->parameters[FOLLOWERATT].digits = 2;
	_STATE->parameters[FOLLOWERATT].initvalue = LOG10D20F(3.);
	_STATE->parameters[FOLLOWERATT].progress = .25;
	_STATE->parameters[FOLLOWERATT].flags |= Param::MidiParam;

	const char* fol = "ENVELOPE FOLLOWER";

	_STATE->parameters[FOLLOWERREL].category = fol;
	_STATE->parameters[FOLLOWERREL].min = LOG10D20F(0.25);
	_STATE->parameters[FOLLOWERREL].max = LOG10D20F(2000.);
	_STATE->parameters[FOLLOWERREL].name = "RELEASE";
	_STATE->parameters[FOLLOWERREL].valuename = "ms";
	_STATE->parameters[FOLLOWERREL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FOLLOWERREL].type = ParameterType_double;
	_STATE->parameters[FOLLOWERREL].digits = 2;
	_STATE->parameters[FOLLOWERREL].initvalue = LOG10D20F(10.);
	_STATE->parameters[FOLLOWERREL].progress = .25;
	_STATE->parameters[FOLLOWERREL].flags |= Param::Param::MidiParam;


	_STATE->parameters[FOLLOWERBOUNDA].category = fol;
	_STATE->parameters[FOLLOWERBOUNDA].name = "BOUNDA";
	_STATE->parameters[FOLLOWERBOUNDA].type = ParameterType_double;
	_STATE->parameters[FOLLOWERBOUNDA].flags |= Param::Param::MidiParam;
	_STATE->parameters[FOLLOWERBOUNDB].name = "BOUNDB";
	_STATE->parameters[FOLLOWERBOUNDB].flags |= Param::Param::MidiParam;
	_STATE->parameters[FOLLOWERBOUNDB].type = ParameterType_double;
	_STATE->parameters[FOLLOWERSRC].flags |= Param::Param::MidiParam;
	_STATE->parameters[FOLLOWERSRC].name = "SIDECHAIN";
	_STATE->parameters[FOLLOWERSRC].category = fol;
	_STATE->parameters[FOLLOWERSRC].max = 4;

	_STATE->parameters[FOLLOWERGAIN].category = fol;
	_STATE->parameters[FOLLOWERGAIN].min = -60;
	_STATE->parameters[FOLLOWERGAIN].max = 60.;
	_STATE->parameters[FOLLOWERGAIN].name = "GAIN";
	_STATE->parameters[FOLLOWERGAIN].valuename = "dB";
	_STATE->parameters[FOLLOWERGAIN].type = ParameterType_double;
	// _STATE->parameters[FOLLOWER1GAIN].flags |= Param::MidiParam;
	_STATE->parameters[FOLLOWERGAIN].initvalue = 0;
	_STATE->parameters[FOLLOWERGAIN].progress = 1;
	_STATE->parameters[FOLLOWERGAIN].flags |= Param::Param::MidiParam;

	_STATE->parameters[FOLLOWERDEST].flags |= Param::NoAssignment;
	_STATE->parameters[FOLLOWERDEST].initvalue = DISTMIX;
	_STATE->parameters[FOLLOWERDEST].type = ParameterType_enum;
	_STATE->parameters[FOLLOWERDEST].name = "TARGET";
	_STATE->parameters[FOLLOWERDEST].category = fol;

	_STATE->parameters[FOLLOWERDESTPOWER].flags |= Param::MidiParam;
	_STATE->parameters[FOLLOWERDESTPOWER].type = ParameterType_bool;


	const char* grainenv1 = "GRAINENV 1";
	const char* grainenv2 = "GRAINENV 2";
	const char* env1 = "ENV 1";
	const char* env2 = "ENV 2";
	const char* inner = "INNER ENV";
	const char* outer = "OUTER ENV";

	// SPACE_GRAINENV1/2 are _space_windows members, values 0 and 1 -- NOT
	// ParameterNum. parameters[] is indexed by ParameterNum, so these two lines
	// stamped NoAssignment onto parameters[0] (PARAM_NOT_ASSIGNED) and
	// parameters[1] (MDELAY1DEL). The grain-env spaces are windows, not
	// parameters; the real parameter is GRAINENVSPACE, flagged below.
	//
	// The MDELAY1DEL hit was silent data loss. TRACK::reset() deliberately skips
	// NoAssignment params, so a host ClassInfo restore left the tap holding its
	// old value; the snapshot event then applied a value the engine already had,
	// Event::apply only notifies on a change, so Snapshot::addEvent never ran and
	// the event vanished from events_ while the engine stayed correct. The next
	// save wrote the tap out as its 1000 ms default. Caught by the auhost soak.
	_STATE->parameters[ASENV].flags |= Param::NoAssignment;

	_STATE->parameters[GRAINNSEGS].category = grainenv2;
	_STATE->parameters[GRAINNSEGS].initvalue = 4.;
	_STATE->parameters[GRAINNSEGS].name = "SEGMENTS";
	_STATE->parameters[GRAINNSEGS].valuename = "/ GRAIN";
	_STATE->parameters[GRAINNSEGS].min = 1;
	_STATE->parameters[GRAINNSEGS].max = 8;
	_STATE->parameters[GRAINNSEGS].progress = 1;

	_STATE->parameters[GRAINJOIN].category = grainenv2;
	_STATE->parameters[GRAINJOIN].initvalue = 1.0;
	_STATE->parameters[GRAINJOIN].type = ParameterType_bool;
	_STATE->parameters[GRAINJOIN].name = "JOIN ENDS";

	_STATE->parameters[GRAINCURVE].category = grainenv2;
	_STATE->parameters[GRAINCURVE].name = "CURVE";
	_STATE->parameters[GRAINCURVE].type = ParameterType_enum;
	_STATE->parameters[GRAINCURVE].names = std::span<const std::string_view>{ editorcurvenames };

	_STATE->parameters[GRAINQUANT].category = grainenv2;
	_STATE->parameters[GRAINQUANT].initvalue = 1.0;
	_STATE->parameters[GRAINQUANT].name = "QUANT";

	for (int32_t i = 0; i <= 8; i++) {
		_STATE->parameters[GRAINENVY0 + i].category = grainenv2;//0;
		_STATE->parameters[GRAINENVX0 + i].type = _STATE->parameters[GRAINENVY0 + i].type = ParameterType_double;

		_STATE->parameters[GRAINENVX0 + i].category = grainenv2;
		_STATE->parameters[GRAINENVX0 + i].initvalue = i < 5 ? i * .25 : 1.;
		_STATE->parameters[GRAINENVX0 + i].min = 0.;
		_STATE->parameters[GRAINENVX0 + i].max = 1.;
		_STATE->parameters[GRAINENVY0 + i].initvalue = 1;//0;
		_STATE->parameters[GRAINENVY0 + i].min = 0.;
		_STATE->parameters[GRAINENVY0 + i].max = 1.;
	}


	_STATE->parameters[ENVOUTER].initvalue = _STATE->parameters[ENVOUTER2].initvalue = _STATE->parameters[ENVOUTER].initvalue = findIndexChar(envelopesnames,
		ARRAY_LEN(envelopesnames),
		"RECTANGULAR");
	_STATE->parameters[ENVINNER].initvalue = _STATE->parameters[ENVINNER2].initvalue = _STATE->parameters[ENVINNER].initvalue = findIndexChar(envelopesnames,
		ARRAY_LEN(envelopesnames),
		"BLACKMAN");
	_STATE->parameters[ENVOUTER].type = _STATE->parameters[ENVOUTER2].type = _STATE->parameters[ENVINNER].type = _STATE->parameters[ENVINNER2].type = ParameterType_enum;
	_STATE->parameters[ENVOUTER].names = _STATE->parameters[ENVOUTER2].names = _STATE->parameters[ENVINNER].names = _STATE->parameters[ENVINNER2].names = envelopesnames;
	_STATE->parameters[ENVOUTER].flags = _STATE->parameters[ENVOUTER2].flags = _STATE->parameters[ENVINNER].flags = _STATE->parameters[ENVINNER2].flags |= Param::MidiParam;


	_STATE->parameters[ENVOUTER].category = grainenv1;
	_STATE->parameters[ENVOUTER].subcategory = env1;
	_STATE->parameters[ENVOUTER].name = outer;

	_STATE->parameters[ENVINNER].category = grainenv1;
	_STATE->parameters[ENVINNER].name = inner;
	_STATE->parameters[ENVINNER].subcategory = env1;

	_STATE->parameters[ENVOUTER2].category = grainenv1;
	_STATE->parameters[ENVOUTER2].subcategory = env2;
	_STATE->parameters[ENVOUTER2].name = outer;

	_STATE->parameters[ENVINNER2].category = grainenv1;
	_STATE->parameters[ENVINNER2].name = inner;
	_STATE->parameters[ENVINNER2].subcategory = env2;

	_STATE->parameters[AOUTERCYCLES].category = grainenv1;

	_STATE->parameters[AOUTERCYCLES].min = 1;
	_STATE->parameters[AOUTERCYCLES].max = 40.;
	_STATE->parameters[AOUTERCYCLES].name = "CYCLES";
	_STATE->parameters[AOUTERCYCLES].valuename = "/ GRAIN";
	_STATE->parameters[AOUTERCYCLES].type = ParameterType_double;
	_STATE->parameters[AOUTERCYCLES].flags |= Param::MidiParam;
	_STATE->parameters[AOUTERCYCLES].flags |= Param::CastInt;
	_STATE->parameters[AOUTERCYCLES].initvalue = 1;
	_STATE->parameters[AOUTERCYCLES].progress = 1;
	_STATE->parameters[AOUTERCYCLES].subcategory = env1;

	_STATE->parameters[AOUTERCYCLES2] = _STATE->parameters[AOUTERCYCLES];
	_STATE->parameters[AOUTERCYCLES2].subcategory = env2;


	_STATE->parameters[AOUTERDEPTH].category = grainenv1;
	_STATE->parameters[AOUTERDEPTH].min = 0;
	_STATE->parameters[AOUTERDEPTH].max = 1.;
	_STATE->parameters[AOUTERDEPTH].name = "DEPTH";
	_STATE->parameters[AOUTERDEPTH].valuename = " ";
	_STATE->parameters[AOUTERDEPTH].type = ParameterType_double;
	_STATE->parameters[AOUTERDEPTH].flags |= Param::MidiParam;
	_STATE->parameters[AOUTERDEPTH].digits = 2;
	_STATE->parameters[AOUTERDEPTH].initvalue = 0;
	_STATE->parameters[AOUTERDEPTH].progress = .01;

	_STATE->parameters[AOUTERDEPTH2] = _STATE->parameters[AOUTERDEPTH];
	_STATE->parameters[AOUTERDEPTH].subcategory = env1;
	_STATE->parameters[AOUTERDEPTH2].subcategory = env2;

	_STATE->parameters[ENVINNER].paramOffset = _STATE->parameters[ENVOUTER].paramOffset = _STATE->parameters[AOUTERCYCLES].paramOffset = _STATE->parameters[AOUTERDEPTH].paramOffset = GRAINENVSPACE;
	_STATE->parameters[AOUTERDEPTH].offsetFact = AOUTERDEPTH2 - AOUTERDEPTH;
	_STATE->parameters[AOUTERCYCLES].offsetFact = AOUTERCYCLES2 - AOUTERCYCLES;
	_STATE->parameters[ENVINNER].offsetFact = ENVINNER2 - ENVINNER;
	_STATE->parameters[ENVOUTER].offsetFact = ENVOUTER2 - ENVOUTER;

	_STATE->parameters[GRAINENVINTERPOL].category = grainenv1;
	_STATE->parameters[GRAINENVINTERPOL].min = 0;
	_STATE->parameters[GRAINENVINTERPOL].max = 1.;
	_STATE->parameters[GRAINENVINTERPOL].name = "INTERPOL";
	_STATE->parameters[GRAINENVINTERPOL].valuename = " ";
	_STATE->parameters[GRAINENVINTERPOL].type = ParameterType_double;
	_STATE->parameters[GRAINENVINTERPOL].flags |= Param::MidiParam;
	_STATE->parameters[GRAINENVINTERPOL].digits = 2;
	_STATE->parameters[GRAINENVINTERPOL].initvalue = 0;
	_STATE->parameters[GRAINENVINTERPOL].progress = .01;

	_STATE->parameters[GRAINENVSPACE].flags |= Param::NoAssignment;
	_STATE->parameters[GRAINENVSPACE].max = 2;
	_STATE->parameters[GRAINENVSPACE].category = grainenv1;
	//_STATE->parameters[GRAINENVSPACE].afterChange = rendergrainenv1;

	//_STATE->parameters[WRITEOFFSET].reference[index] = &track->wr;
	_STATE->parameters[WRITEOFFSET].category = grainParams;
	_STATE->parameters[WRITEOFFSET].min = 0;
	_STATE->parameters[WRITEOFFSET].max = _DATA->flanger_max_samples;
	_STATE->parameters[WRITEOFFSET].name = "WRITE OFFSET";
	_STATE->parameters[WRITEOFFSET].valuename = "ms";
	_STATE->parameters[WRITEOFFSET].type = ParameterType_double;
	_STATE->parameters[WRITEOFFSET].digits = 2;
	_STATE->parameters[WRITEOFFSET].flags |= Param::ConvertMs;
	_STATE->parameters[WRITEOFFSET].initvalue = 0;
	_STATE->parameters[WRITEOFFSET].progress = _STATE->sr / 5000.;
	//  _STATE->parameters[WRITEOFFSET].flags |= Param::MidiParam_windows;




	const char* grainrev = "GRAIN REVERB";

	_STATE->parameters[GRAINREVERBDECAY].category = grainrev;
	_STATE->parameters[GRAINREVERBDECAY].min = 0.;
	_STATE->parameters[GRAINREVERBDECAY].max = 1.;
	_STATE->parameters[GRAINREVERBDECAY].name = "ROOM";
	_STATE->parameters[GRAINREVERBDECAY].valuename = " ";
	_STATE->parameters[GRAINREVERBDECAY].type = ParameterType_double;
	_STATE->parameters[GRAINREVERBDECAY].digits = 2;
	_STATE->parameters[GRAINREVERBDECAY].initvalue = .5;
	_STATE->parameters[GRAINREVERBDECAY].progress = .05;
	_STATE->parameters[GRAINREVERBDECAY].flags |= Param::MidiParam;

	_STATE->parameters[GRAINREVERBMIX].category = grainrev;
	_STATE->parameters[GRAINREVERBMIX].min = 0.;
	_STATE->parameters[GRAINREVERBMIX].max = 1.;
	_STATE->parameters[GRAINREVERBMIX].name = "MIX";
	_STATE->parameters[GRAINREVERBMIX].valuename = " ";
	_STATE->parameters[GRAINREVERBMIX].type = ParameterType_double;
	_STATE->parameters[GRAINREVERBMIX].digits = 2;
	_STATE->parameters[GRAINREVERBMIX].initvalue = .5;
	_STATE->parameters[GRAINREVERBMIX].progress = .01;
	_STATE->parameters[GRAINREVERBMIX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINREVERBGAIN].category = grainrev;
	_STATE->parameters[GRAINREVERBGAIN].min = -60;
	_STATE->parameters[GRAINREVERBGAIN].max = 60.;
	_STATE->parameters[GRAINREVERBGAIN].name = "GAIN";
	_STATE->parameters[GRAINREVERBGAIN].valuename = "dB";
	_STATE->parameters[GRAINREVERBGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINREVERBGAIN].initvalue = -3;
	_STATE->parameters[GRAINREVERBGAIN].progress = 1;
	_STATE->parameters[GRAINREVERBGAIN].flags |= Param::MidiParam;

	// ── Grain effects: WAVESET / DRIVE / DISPERSE / VOWEL / PLUCK ────────────
	// Every MIN/MAX pair below is a per-grain random range, not a sweep: the
	// value is drawn once per grain (the GRAIN RINGMOD / RESON convention).
	// Set MIN == MAX for a fixed value.

	const char* grainws = "GRAIN WAVESET";

	_STATE->parameters[GRAINWSMODE].category = grainws;
	_STATE->parameters[GRAINWSMODE].name = "MODE";
	_STATE->parameters[GRAINWSMODE].names = grainwavesetmodes;
	_STATE->parameters[GRAINWSMODE].type = ParameterType_enum;
	_STATE->parameters[GRAINWSMODE].initvalue = 0;
	_STATE->parameters[GRAINWSMODE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINWSAMT].category = grainws;
	_STATE->parameters[GRAINWSAMT].min = 0.;
	_STATE->parameters[GRAINWSAMT].max = 1.;
	_STATE->parameters[GRAINWSAMT].name = "AMOUNT";
	_STATE->parameters[GRAINWSAMT].valuename = " ";
	_STATE->parameters[GRAINWSAMT].type = ParameterType_double;
	_STATE->parameters[GRAINWSAMT].digits = 2;
	_STATE->parameters[GRAINWSAMT].initvalue = .5;
	_STATE->parameters[GRAINWSAMT].progress = .01;
	_STATE->parameters[GRAINWSAMT].flags |= Param::MidiParam;

	_STATE->parameters[GRAINWSGROUP].category = grainws;
	_STATE->parameters[GRAINWSGROUP].min = 1.;
	_STATE->parameters[GRAINWSGROUP].max = 8.;
	_STATE->parameters[GRAINWSGROUP].name = "GROUP";
	_STATE->parameters[GRAINWSGROUP].valuename = " ";
	_STATE->parameters[GRAINWSGROUP].type = ParameterType_double;
	_STATE->parameters[GRAINWSGROUP].digits = 0;
	_STATE->parameters[GRAINWSGROUP].initvalue = 1.;
	_STATE->parameters[GRAINWSGROUP].progress = 1.;
	_STATE->parameters[GRAINWSGROUP].flags |= Param::MidiParam;

	_STATE->parameters[GRAINWSMIX].category = grainws;
	_STATE->parameters[GRAINWSMIX].min = 0.;
	_STATE->parameters[GRAINWSMIX].max = 1.;
	_STATE->parameters[GRAINWSMIX].name = "MIX";
	_STATE->parameters[GRAINWSMIX].valuename = " ";
	_STATE->parameters[GRAINWSMIX].type = ParameterType_double;
	_STATE->parameters[GRAINWSMIX].digits = 2;
	_STATE->parameters[GRAINWSMIX].initvalue = 1.;
	_STATE->parameters[GRAINWSMIX].progress = .01;
	_STATE->parameters[GRAINWSMIX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINWSGAIN].category = grainws;
	_STATE->parameters[GRAINWSGAIN].min = -60.;
	_STATE->parameters[GRAINWSGAIN].max = 60.;
	_STATE->parameters[GRAINWSGAIN].name = "GAIN";
	_STATE->parameters[GRAINWSGAIN].valuename = "dB";
	_STATE->parameters[GRAINWSGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINWSGAIN].initvalue = 0.;
	_STATE->parameters[GRAINWSGAIN].progress = 1.;
	_STATE->parameters[GRAINWSGAIN].flags |= Param::MidiParam;

	const char* graindrv = "GRAIN DRIVE";

	_STATE->parameters[GRAINDRVTYPE].category = graindrv;
	_STATE->parameters[GRAINDRVTYPE].name = "TYPE";
	_STATE->parameters[GRAINDRVTYPE].names = graindrivetypes;
	_STATE->parameters[GRAINDRVTYPE].type = ParameterType_enum;
	_STATE->parameters[GRAINDRVTYPE].initvalue = 0;
	_STATE->parameters[GRAINDRVTYPE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDRVMIN].category = graindrv;
	_STATE->parameters[GRAINDRVMIN].min = 0.;
	_STATE->parameters[GRAINDRVMIN].max = 48.;
	_STATE->parameters[GRAINDRVMIN].name = "DRIVE MIN";
	_STATE->parameters[GRAINDRVMIN].valuename = "dB";
	_STATE->parameters[GRAINDRVMIN].type = ParameterType_double;
	_STATE->parameters[GRAINDRVMIN].initvalue = 6.;
	_STATE->parameters[GRAINDRVMIN].progress = 1.;
	_STATE->parameters[GRAINDRVMIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDRVMAX].category = graindrv;
	_STATE->parameters[GRAINDRVMAX].min = 0.;
	_STATE->parameters[GRAINDRVMAX].max = 48.;
	_STATE->parameters[GRAINDRVMAX].name = "DRIVE MAX";
	_STATE->parameters[GRAINDRVMAX].valuename = "dB";
	_STATE->parameters[GRAINDRVMAX].type = ParameterType_double;
	_STATE->parameters[GRAINDRVMAX].initvalue = 18.;
	_STATE->parameters[GRAINDRVMAX].progress = 1.;
	_STATE->parameters[GRAINDRVMAX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDRVMIX].category = graindrv;
	_STATE->parameters[GRAINDRVMIX].min = 0.;
	_STATE->parameters[GRAINDRVMIX].max = 1.;
	_STATE->parameters[GRAINDRVMIX].name = "MIX";
	_STATE->parameters[GRAINDRVMIX].valuename = " ";
	_STATE->parameters[GRAINDRVMIX].type = ParameterType_double;
	_STATE->parameters[GRAINDRVMIX].digits = 2;
	_STATE->parameters[GRAINDRVMIX].initvalue = 1.;
	_STATE->parameters[GRAINDRVMIX].progress = .01;
	_STATE->parameters[GRAINDRVMIX].flags |= Param::MidiParam;

	// starts trimmed: every shaper preserves the peak but raises the RMS
	_STATE->parameters[GRAINDRVGAIN].category = graindrv;
	_STATE->parameters[GRAINDRVGAIN].min = -60.;
	_STATE->parameters[GRAINDRVGAIN].max = 60.;
	_STATE->parameters[GRAINDRVGAIN].name = "GAIN";
	_STATE->parameters[GRAINDRVGAIN].valuename = "dB";
	_STATE->parameters[GRAINDRVGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINDRVGAIN].initvalue = -6.;
	_STATE->parameters[GRAINDRVGAIN].progress = 1.;
	_STATE->parameters[GRAINDRVGAIN].flags |= Param::MidiParam;

	const char* graindisp = "GRAIN DISPERSE";

	_STATE->parameters[GRAINDISPSTAGES].category = graindisp;
	_STATE->parameters[GRAINDISPSTAGES].min = 1.;
	_STATE->parameters[GRAINDISPSTAGES].max = 48.;
	_STATE->parameters[GRAINDISPSTAGES].name = "STAGES";
	_STATE->parameters[GRAINDISPSTAGES].valuename = " ";
	_STATE->parameters[GRAINDISPSTAGES].type = ParameterType_double;
	_STATE->parameters[GRAINDISPSTAGES].digits = 0;
	_STATE->parameters[GRAINDISPSTAGES].initvalue = 12.;
	_STATE->parameters[GRAINDISPSTAGES].progress = 1.;
	_STATE->parameters[GRAINDISPSTAGES].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDISPFMIN].category = graindisp;
	_STATE->parameters[GRAINDISPFMIN].min = LOG10D20F(20.);
	_STATE->parameters[GRAINDISPFMIN].max = LOG10D20F(18000.);
	_STATE->parameters[GRAINDISPFMIN].name = "FREQ MIN";
	_STATE->parameters[GRAINDISPFMIN].valuename = "Hz";
	_STATE->parameters[GRAINDISPFMIN].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINDISPFMIN].type = ParameterType_double;
	_STATE->parameters[GRAINDISPFMIN].initvalue = LOG10D20F(200.);
	_STATE->parameters[GRAINDISPFMIN].progress = .25;
	_STATE->parameters[GRAINDISPFMIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDISPFMAX].category = graindisp;
	_STATE->parameters[GRAINDISPFMAX].min = LOG10D20F(20.);
	_STATE->parameters[GRAINDISPFMAX].max = LOG10D20F(18000.);
	_STATE->parameters[GRAINDISPFMAX].name = "FREQ MAX";
	_STATE->parameters[GRAINDISPFMAX].valuename = "Hz";
	_STATE->parameters[GRAINDISPFMAX].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINDISPFMAX].type = ParameterType_double;
	_STATE->parameters[GRAINDISPFMAX].initvalue = LOG10D20F(4000.);
	_STATE->parameters[GRAINDISPFMAX].progress = .25;
	_STATE->parameters[GRAINDISPFMAX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDISPDEPTH].category = graindisp;
	_STATE->parameters[GRAINDISPDEPTH].min = 0.;
	_STATE->parameters[GRAINDISPDEPTH].max = 1.;
	_STATE->parameters[GRAINDISPDEPTH].name = "DEPTH";
	_STATE->parameters[GRAINDISPDEPTH].valuename = " ";
	_STATE->parameters[GRAINDISPDEPTH].type = ParameterType_double;
	_STATE->parameters[GRAINDISPDEPTH].digits = 2;
	_STATE->parameters[GRAINDISPDEPTH].initvalue = .7;
	_STATE->parameters[GRAINDISPDEPTH].progress = .01;
	_STATE->parameters[GRAINDISPDEPTH].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDISPMIX].category = graindisp;
	_STATE->parameters[GRAINDISPMIX].min = 0.;
	_STATE->parameters[GRAINDISPMIX].max = 1.;
	_STATE->parameters[GRAINDISPMIX].name = "MIX";
	_STATE->parameters[GRAINDISPMIX].valuename = " ";
	_STATE->parameters[GRAINDISPMIX].type = ParameterType_double;
	_STATE->parameters[GRAINDISPMIX].digits = 2;
	_STATE->parameters[GRAINDISPMIX].initvalue = 1.;
	_STATE->parameters[GRAINDISPMIX].progress = .01;
	_STATE->parameters[GRAINDISPMIX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINDISPGAIN].category = graindisp;
	_STATE->parameters[GRAINDISPGAIN].min = -60.;
	_STATE->parameters[GRAINDISPGAIN].max = 60.;
	_STATE->parameters[GRAINDISPGAIN].name = "GAIN";
	_STATE->parameters[GRAINDISPGAIN].valuename = "dB";
	_STATE->parameters[GRAINDISPGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINDISPGAIN].initvalue = 0.;
	_STATE->parameters[GRAINDISPGAIN].progress = 1.;
	_STATE->parameters[GRAINDISPGAIN].flags |= Param::MidiParam;

	const char* grainvow = "GRAIN VOWEL";

	_STATE->parameters[GRAINVOWVOICE].category = grainvow;
	_STATE->parameters[GRAINVOWVOICE].name = "VOICE";
	_STATE->parameters[GRAINVOWVOICE].names = voxvoicenames;
	_STATE->parameters[GRAINVOWVOICE].type = ParameterType_enum;
	_STATE->parameters[GRAINVOWVOICE].initvalue = 1;
	_STATE->parameters[GRAINVOWVOICE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVOWMIN].category = grainvow;
	_STATE->parameters[GRAINVOWMIN].min = 0.;
	_STATE->parameters[GRAINVOWMIN].max = 4.;
	_STATE->parameters[GRAINVOWMIN].name = "VOWEL MIN";
	_STATE->parameters[GRAINVOWMIN].valuename = " ";
	_STATE->parameters[GRAINVOWMIN].type = ParameterType_double;
	_STATE->parameters[GRAINVOWMIN].digits = 2;
	_STATE->parameters[GRAINVOWMIN].initvalue = 0.;
	_STATE->parameters[GRAINVOWMIN].progress = 1.;
	_STATE->parameters[GRAINVOWMIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVOWMAX].category = grainvow;
	_STATE->parameters[GRAINVOWMAX].min = 0.;
	_STATE->parameters[GRAINVOWMAX].max = 4.;
	_STATE->parameters[GRAINVOWMAX].name = "VOWEL MAX";
	_STATE->parameters[GRAINVOWMAX].valuename = " ";
	_STATE->parameters[GRAINVOWMAX].type = ParameterType_double;
	_STATE->parameters[GRAINVOWMAX].digits = 2;
	_STATE->parameters[GRAINVOWMAX].initvalue = 4.;
	_STATE->parameters[GRAINVOWMAX].progress = 1.;
	_STATE->parameters[GRAINVOWMAX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVOWBW].category = grainvow;
	_STATE->parameters[GRAINVOWBW].min = .25;
	_STATE->parameters[GRAINVOWBW].max = 4.;
	_STATE->parameters[GRAINVOWBW].name = "BW";
	_STATE->parameters[GRAINVOWBW].valuename = " ";
	_STATE->parameters[GRAINVOWBW].type = ParameterType_double;
	_STATE->parameters[GRAINVOWBW].digits = 2;
	_STATE->parameters[GRAINVOWBW].initvalue = 1.;
	_STATE->parameters[GRAINVOWBW].progress = .05;
	_STATE->parameters[GRAINVOWBW].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVOWMIX].category = grainvow;
	_STATE->parameters[GRAINVOWMIX].min = 0.;
	_STATE->parameters[GRAINVOWMIX].max = 1.;
	_STATE->parameters[GRAINVOWMIX].name = "MIX";
	_STATE->parameters[GRAINVOWMIX].valuename = " ";
	_STATE->parameters[GRAINVOWMIX].type = ParameterType_double;
	_STATE->parameters[GRAINVOWMIX].digits = 2;
	_STATE->parameters[GRAINVOWMIX].initvalue = 1.;
	_STATE->parameters[GRAINVOWMIX].progress = .01;
	_STATE->parameters[GRAINVOWMIX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVOWGAIN].category = grainvow;
	_STATE->parameters[GRAINVOWGAIN].min = -60.;
	_STATE->parameters[GRAINVOWGAIN].max = 60.;
	_STATE->parameters[GRAINVOWGAIN].name = "GAIN";
	_STATE->parameters[GRAINVOWGAIN].valuename = "dB";
	_STATE->parameters[GRAINVOWGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINVOWGAIN].initvalue = 0.;
	_STATE->parameters[GRAINVOWGAIN].progress = 1.;
	_STATE->parameters[GRAINVOWGAIN].flags |= Param::MidiParam;

	const char* grainplk = "GRAIN PLUCK";

	_STATE->parameters[GRAINPLKMIN].category = grainplk;
	_STATE->parameters[GRAINPLKMIN].min = LOG10D20F(20.);
	_STATE->parameters[GRAINPLKMIN].max = LOG10D20F(4000.);
	_STATE->parameters[GRAINPLKMIN].name = "TUNE MIN";
	_STATE->parameters[GRAINPLKMIN].valuename = "Hz";
	_STATE->parameters[GRAINPLKMIN].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINPLKMIN].type = ParameterType_double;
	_STATE->parameters[GRAINPLKMIN].initvalue = LOG10D20F(110.);
	_STATE->parameters[GRAINPLKMIN].progress = .25;
	_STATE->parameters[GRAINPLKMIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPLKMAX].category = grainplk;
	_STATE->parameters[GRAINPLKMAX].min = LOG10D20F(20.);
	_STATE->parameters[GRAINPLKMAX].max = LOG10D20F(4000.);
	_STATE->parameters[GRAINPLKMAX].name = "TUNE MAX";
	_STATE->parameters[GRAINPLKMAX].valuename = "Hz";
	_STATE->parameters[GRAINPLKMAX].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINPLKMAX].type = ParameterType_double;
	_STATE->parameters[GRAINPLKMAX].initvalue = LOG10D20F(440.);
	_STATE->parameters[GRAINPLKMAX].progress = .25;
	_STATE->parameters[GRAINPLKMAX].flags |= Param::MidiParam;

	// 0 -> 50 ms, 1 -> 10 s (T60)
	_STATE->parameters[GRAINPLKDECAY].category = grainplk;
	_STATE->parameters[GRAINPLKDECAY].min = 0.;
	_STATE->parameters[GRAINPLKDECAY].max = 1.;
	_STATE->parameters[GRAINPLKDECAY].name = "DECAY";
	_STATE->parameters[GRAINPLKDECAY].valuename = " ";
	_STATE->parameters[GRAINPLKDECAY].type = ParameterType_double;
	_STATE->parameters[GRAINPLKDECAY].digits = 2;
	_STATE->parameters[GRAINPLKDECAY].initvalue = .35;
	_STATE->parameters[GRAINPLKDECAY].progress = .01;
	_STATE->parameters[GRAINPLKDECAY].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPLKDAMP].category = grainplk;
	_STATE->parameters[GRAINPLKDAMP].min = 0.;
	_STATE->parameters[GRAINPLKDAMP].max = 1.;
	_STATE->parameters[GRAINPLKDAMP].name = "DAMP";
	_STATE->parameters[GRAINPLKDAMP].valuename = " ";
	_STATE->parameters[GRAINPLKDAMP].type = ParameterType_double;
	_STATE->parameters[GRAINPLKDAMP].digits = 2;
	_STATE->parameters[GRAINPLKDAMP].initvalue = .4;
	_STATE->parameters[GRAINPLKDAMP].progress = .01;
	_STATE->parameters[GRAINPLKDAMP].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPLKMIX].category = grainplk;
	_STATE->parameters[GRAINPLKMIX].min = 0.;
	_STATE->parameters[GRAINPLKMIX].max = 1.;
	_STATE->parameters[GRAINPLKMIX].name = "MIX";
	_STATE->parameters[GRAINPLKMIX].valuename = " ";
	_STATE->parameters[GRAINPLKMIX].type = ParameterType_double;
	_STATE->parameters[GRAINPLKMIX].digits = 2;
	_STATE->parameters[GRAINPLKMIX].initvalue = .5;
	_STATE->parameters[GRAINPLKMIX].progress = .01;
	_STATE->parameters[GRAINPLKMIX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPLKGAIN].category = grainplk;
	_STATE->parameters[GRAINPLKGAIN].min = -60.;
	_STATE->parameters[GRAINPLKGAIN].max = 60.;
	_STATE->parameters[GRAINPLKGAIN].name = "GAIN";
	_STATE->parameters[GRAINPLKGAIN].valuename = "dB";
	_STATE->parameters[GRAINPLKGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINPLKGAIN].initvalue = -3.;
	_STATE->parameters[GRAINPLKGAIN].progress = 1.;
	_STATE->parameters[GRAINPLKGAIN].flags |= Param::MidiParam;

	const char* lpcvoc = "LPC VOCODER";
	const char* white = "WHITE";

	_STATE->parameters[LPCVOCORDER].category = lpcvoc;
	_STATE->parameters[LPCVOCORDER].min = 2.;
	_STATE->parameters[LPCVOCORDER].max = 100.;
	_STATE->parameters[LPCVOCORDER].name = "ORDER";
	_STATE->parameters[LPCVOCORDER].valuename = " ";
	_STATE->parameters[LPCVOCORDER].type = ParameterType_double;
	_STATE->parameters[LPCVOCORDER].digits = 0;
	// 44 rather than the old 20: measured against a synthetic three-formant
	// vowel (700/1220/2600 Hz) at 48 kHz, below ~40 the fit cannot separate F1
	// from F2 and merges them into one peak near 900 Hz -- the "muddy, all
	// vowels sound the same" failure. The effect scales the effective order by
	// sr/48000, so this stays calibrated at other sample rates.
	_STATE->parameters[LPCVOCORDER].initvalue = 44;
	_STATE->parameters[LPCVOCORDER].progress = 1;
	_STATE->parameters[LPCVOCORDER].flags |= Param::MidiParam;

	// The carrier whitening order is hardcoded (CAR_ORDER_AT_48K in vocoder.h):
	// exposing it as a knob made no audible difference anywhere in its range.

	_STATE->parameters[LPCVOCWHITE].name = white;
	_STATE->parameters[LPCVOCWHITE].category = lpcvoc;
	_STATE->parameters[LPCVOCWHITE].type = ParameterType_bool;
	_STATE->parameters[LPCVOCWHITE].flags |= Param::MidiParam;

	// Pre-emphasis applied to the modulator on the analysis path only. Without
	// it the fit spends its poles on the low-frequency tilt of speech and
	// under-models everything above 2 kHz. Plain 0..1 here; the effect maps it
	// onto the alpha range that actually does something (see vocoder.cpp),
	// since everything audible sits between alpha 0.7 and 0.97.
	_STATE->parameters[LPCVOCEMPH].category = lpcvoc;
	_STATE->parameters[LPCVOCEMPH].min = 0.;
	_STATE->parameters[LPCVOCEMPH].max = 1.;
	_STATE->parameters[LPCVOCEMPH].name = "PREEMPH";
	_STATE->parameters[LPCVOCEMPH].valuename = " ";
	_STATE->parameters[LPCVOCEMPH].type = ParameterType_double;
	_STATE->parameters[LPCVOCEMPH].digits = 2;
	_STATE->parameters[LPCVOCEMPH].initvalue = 0.9;
	_STATE->parameters[LPCVOCEMPH].progress = 0.01;
	_STATE->parameters[LPCVOCEMPH].flags |= Param::MidiParam;

	const char* lpc = "LPC";

	_STATE->parameters[CROSSLPCW].category = cross;
	_STATE->parameters[CROSSLPCW].subcategory = lpc;
	_STATE->parameters[CROSSLPCW].name = white;
	_STATE->parameters[CROSSLPCW].type = ParameterType_bool;
	_STATE->parameters[CROSSLPCW].flags |= Param::MidiParam;


	_STATE->parameters[CROSSLPCEMPH].category = cross;
	_STATE->parameters[CROSSLPCEMPH].subcategory = lpc;
	_STATE->parameters[CROSSLPCEMPH].min = 0.;
	_STATE->parameters[CROSSLPCEMPH].max = 1;
	_STATE->parameters[CROSSLPCEMPH].name = "PREEMPH";
	_STATE->parameters[CROSSLPCEMPH].valuename = " ";
	_STATE->parameters[CROSSLPCEMPH].type = ParameterType_double;
	_STATE->parameters[CROSSLPCEMPH].digits = 2;
	_STATE->parameters[CROSSLPCEMPH].initvalue = 0;
	_STATE->parameters[CROSSLPCEMPH].progress = 0.01;
	_STATE->parameters[CROSSLPCEMPH].flags |= Param::MidiParam;
	//_STATE->parameters[CEPSCALE1].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CROSSCEP2EMPH1] = _STATE->parameters[CROSSCEP2EMPH2] = _STATE->parameters[CROSSCEP1EMPH] = _STATE->parameters[CROSSLPCEMPH];
	_STATE->parameters[CROSSCEP1EMPH].subcategory = conv;
	_STATE->parameters[CROSSCEP1EMPH].progress = 0.01;

	_STATE->parameters[CROSSLPCORDER] = _STATE->parameters[LPCVOCORDER];
	_STATE->parameters[CROSSLPCORDER].initvalue = 10.;
	_STATE->parameters[CROSSLPCORDER].max = 100;
	_STATE->parameters[CROSSLPCORDER].category = cross;
	_STATE->parameters[CROSSLPCORDER].subcategory = lpc;
	_STATE->parameters[CROSSLPCORDER].setFlag(Param::OwnInitValue, true);

	_STATE->parameters[LPCVOCMIX].category = lpcvoc;
	_STATE->parameters[LPCVOCMIX].min = 0.;
	_STATE->parameters[LPCVOCMIX].max = 1.;
	_STATE->parameters[LPCVOCMIX].name = "MIX";
	_STATE->parameters[LPCVOCMIX].valuename = " ";
	_STATE->parameters[LPCVOCMIX].type = ParameterType_double;
	_STATE->parameters[LPCVOCMIX].digits = 2;
	_STATE->parameters[LPCVOCMIX].initvalue = 1.0;
	_STATE->parameters[LPCVOCMIX].progress = .01;
	_STATE->parameters[LPCVOCMIX].flags |= Param::MidiParam;

	_STATE->parameters[LPCVOCGAIN].category = lpcvoc;
	_STATE->parameters[LPCVOCGAIN].min = -60;
	_STATE->parameters[LPCVOCGAIN].max = 60.;
	_STATE->parameters[LPCVOCGAIN].name = "GAIN";
	_STATE->parameters[LPCVOCGAIN].valuename = "dB";
	_STATE->parameters[LPCVOCGAIN].type = ParameterType_double;
	_STATE->parameters[LPCVOCGAIN].initvalue = LOG10D20F(1.0);
	_STATE->parameters[LPCVOCGAIN].progress = 1;
	_STATE->parameters[LPCVOCGAIN].flags |= Param::MidiParam;

	const char* specfilt = "SPECTRAL FILTER";

	_STATE->parameters[SPECFILTMAGLPCUT].category = specfilt;
	_STATE->parameters[SPECFILTMAGLPCUT].min = -60.;
	_STATE->parameters[SPECFILTMAGLPCUT].max = 0.;
	_STATE->parameters[SPECFILTMAGLPCUT].name = "MAG LP";
	_STATE->parameters[SPECFILTMAGLPCUT].valuename = " ";
	_STATE->parameters[SPECFILTMAGLPCUT].type = ParameterType_double;
	_STATE->parameters[SPECFILTMAGLPCUT].digits = 3;
	_STATE->parameters[SPECFILTMAGLPCUT].initvalue = 0.;
	_STATE->parameters[SPECFILTMAGLPCUT].progress = 1.;
	_STATE->parameters[SPECFILTMAGLPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECFILTMAGLPCUT].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTFREQLPCUT].category = specfilt;
	_STATE->parameters[SPECFILTFREQLPCUT].min = -60.;
	_STATE->parameters[SPECFILTFREQLPCUT].max = 0.;
	_STATE->parameters[SPECFILTFREQLPCUT].name = "PHASE LP";
	_STATE->parameters[SPECFILTFREQLPCUT].valuename = " ";
	_STATE->parameters[SPECFILTFREQLPCUT].type = ParameterType_double;
	_STATE->parameters[SPECFILTFREQLPCUT].digits = 3;
	_STATE->parameters[SPECFILTFREQLPCUT].initvalue = 0.;
	_STATE->parameters[SPECFILTFREQLPCUT].progress = 1.;
	_STATE->parameters[SPECFILTFREQLPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECFILTFREQLPCUT].flags |= Param::MidiParam;

	// The two SpectralFilter actually reads. See SMOOTH2POLE: 0..1 rising the way
	// the name reads, over the same -60..0 dB span, with 0 exactly the old 0 dB
	// default -- so the default sound is unchanged and 1 is now the most filtering.
	_STATE->parameters[SPECFILTMAGLPCUT2].category = specfilt;
	_STATE->parameters[SPECFILTMAGLPCUT2].min = 0.0;
	_STATE->parameters[SPECFILTMAGLPCUT2].max = 1.0;
	_STATE->parameters[SPECFILTMAGLPCUT2].name = "MAG LP";
	_STATE->parameters[SPECFILTMAGLPCUT2].valuename = " ";
	_STATE->parameters[SPECFILTMAGLPCUT2].type = ParameterType_double;
	_STATE->parameters[SPECFILTMAGLPCUT2].digits = 2;
	_STATE->parameters[SPECFILTMAGLPCUT2].initvalue = 0.;
	_STATE->parameters[SPECFILTMAGLPCUT2].progress = .01;
	_STATE->parameters[SPECFILTMAGLPCUT2].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTFREQLPCUT2].category = specfilt;
	_STATE->parameters[SPECFILTFREQLPCUT2].min = 0.0;
	_STATE->parameters[SPECFILTFREQLPCUT2].max = 1.0;
	_STATE->parameters[SPECFILTFREQLPCUT2].name = "PHASE LP";
	_STATE->parameters[SPECFILTFREQLPCUT2].valuename = " ";
	_STATE->parameters[SPECFILTFREQLPCUT2].type = ParameterType_double;
	_STATE->parameters[SPECFILTFREQLPCUT2].digits = 2;
	_STATE->parameters[SPECFILTFREQLPCUT2].initvalue = 0.;
	_STATE->parameters[SPECFILTFREQLPCUT2].progress = .01;
	_STATE->parameters[SPECFILTFREQLPCUT2].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTRES].category = specfilt;
	_STATE->parameters[SPECFILTRES].min = 0.;
	_STATE->parameters[SPECFILTRES].max = 1.;
	_STATE->parameters[SPECFILTRES].name = "RES";
	_STATE->parameters[SPECFILTRES].valuename = " ";
	_STATE->parameters[SPECFILTRES].type = ParameterType_double;
	_STATE->parameters[SPECFILTRES].digits = 2;
	_STATE->parameters[SPECFILTRES].initvalue = 0;
	_STATE->parameters[SPECFILTRES].progress = .01;
	_STATE->parameters[SPECFILTRES].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTMAGHPCUT].category = specfilt;
	_STATE->parameters[SPECFILTMAGHPCUT].min = 0.;
	_STATE->parameters[SPECFILTMAGHPCUT].max = 1.;
	_STATE->parameters[SPECFILTMAGHPCUT].name = "DELAY";
	_STATE->parameters[SPECFILTMAGHPCUT].valuename = " ";
	_STATE->parameters[SPECFILTMAGHPCUT].type = ParameterType_double;
	_STATE->parameters[SPECFILTMAGHPCUT].digits = 2;
	_STATE->parameters[SPECFILTMAGHPCUT].initvalue = 0;
	_STATE->parameters[SPECFILTMAGHPCUT].progress = .01;
	_STATE->parameters[SPECFILTMAGHPCUT].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTFREQHPCUT].category = specfilt;
	_STATE->parameters[SPECFILTFREQHPCUT].min = 0.;
	_STATE->parameters[SPECFILTFREQHPCUT].max = 1.;
	_STATE->parameters[SPECFILTFREQHPCUT].name = "FEEDBACK";
	_STATE->parameters[SPECFILTFREQHPCUT].valuename = " ";
	_STATE->parameters[SPECFILTFREQHPCUT].type = ParameterType_double;
	_STATE->parameters[SPECFILTFREQHPCUT].digits = 2;
	_STATE->parameters[SPECFILTFREQHPCUT].initvalue = 0;
	_STATE->parameters[SPECFILTFREQHPCUT].progress = .01;
	_STATE->parameters[SPECFILTFREQHPCUT].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTBOUNDA].category = specfilt;
	_STATE->parameters[SPECFILTBOUNDA].min = LOG10D20F(18.);
	_STATE->parameters[SPECFILTBOUNDA].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[SPECFILTBOUNDA].name = "CUT A";
	_STATE->parameters[SPECFILTBOUNDA].valuename = "Hz";
	_STATE->parameters[SPECFILTBOUNDA].type = ParameterType_double;
	_STATE->parameters[SPECFILTBOUNDA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECFILTBOUNDA].initvalue = LOG10D20F(18.);
	_STATE->parameters[SPECFILTBOUNDA].progress = 1.;
	_STATE->parameters[SPECFILTBOUNDA].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTBOUNDB].category = specfilt;
	_STATE->parameters[SPECFILTBOUNDB].min = LOG10D20F(18.);
	_STATE->parameters[SPECFILTBOUNDB].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[SPECFILTBOUNDB].name = "CUT B";
	_STATE->parameters[SPECFILTBOUNDB].valuename = "Hz";
	_STATE->parameters[SPECFILTBOUNDB].type = ParameterType_double;
	_STATE->parameters[SPECFILTBOUNDB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECFILTBOUNDB].initvalue = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[SPECFILTBOUNDB].progress = 1.;
	_STATE->parameters[SPECFILTBOUNDB].flags |= Param::MidiParam;


	_STATE->parameters[SPECFILTDRY].category = specfilt;
	_STATE->parameters[SPECFILTDRY].min = -60;
	_STATE->parameters[SPECFILTDRY].max = 60.;
	_STATE->parameters[SPECFILTDRY].name = "DRY";
	_STATE->parameters[SPECFILTDRY].valuename = "dB";
	_STATE->parameters[SPECFILTDRY].type = ParameterType_double;
	_STATE->parameters[SPECFILTDRY].initvalue = -60;
	_STATE->parameters[SPECFILTDRY].progress = 1;
	_STATE->parameters[SPECFILTDRY].flags |= Param::MidiParam;

	_STATE->parameters[SPECFILTWET].category = specfilt;
	_STATE->parameters[SPECFILTWET].min = -60;
	_STATE->parameters[SPECFILTWET].max = 60.;
	_STATE->parameters[SPECFILTWET].name = "WET";
	_STATE->parameters[SPECFILTWET].valuename = "dB";
	_STATE->parameters[SPECFILTWET].type = ParameterType_double;
	_STATE->parameters[SPECFILTWET].initvalue = LOG10D20F(1.0);
	_STATE->parameters[SPECFILTWET].progress = 1;
	_STATE->parameters[SPECFILTWET].flags |= Param::MidiParam;

	_STATE->parameters[SEMITONES_LFO].min = 0;
	_STATE->parameters[SEMITONES_LFO].max = 12.;
	_STATE->parameters[SEMITONES_LFO].name = "SEMITONES";
	_STATE->parameters[SEMITONES_LFO].valuename = " ";
	_STATE->parameters[SEMITONES_LFO].type = ParameterType_double;
	_STATE->parameters[SEMITONES_LFO].initvalue = 0;
	_STATE->parameters[SEMITONES_LFO].progress = 1;

	_STATE->parameters[READOFFSET_LFO].min = 0;
	_STATE->parameters[READOFFSET_LFO].max = _STATE->sr;
	_STATE->parameters[READOFFSET_LFO].name = "READ OFFSET";
	_STATE->parameters[READOFFSET_LFO].valuename = "ms";
	_STATE->parameters[READOFFSET_LFO].type = ParameterType_double;
	_STATE->parameters[READOFFSET_LFO].initvalue = 0;
	_STATE->parameters[READOFFSET_LFO].progress = _STATE->sr / 100.;
	_STATE->parameters[READOFFSET_LFO].flags |= Param::ConvertMs;
	_STATE->parameters[READOFFSET_LFO].setFlag(Param::ConvertMs, true);

	const char* sidechain = "SIDECHAIN";

	_STATE->parameters[MONOCOMPSRC].category = compName;
	_STATE->parameters[MONOCOMPSRC].name = sidechain;
	_STATE->parameters[MONOCOMPSRC].flags |= (Param::NoAssignment | Param::MidiParam);
	_STATE->parameters[MONOCOMPSRC].setFlag(Param::NoAssignment, true);
	_STATE->parameters[MONOCOMPSRC].type = ParameterType_enum;
	_STATE->parameters[MONOCOMPSRC].names = tracknames;
	
	const char* pvamps = "PVAMPS";

	_STATE->parameters[PVAMPSRANGE].category = pvamps;
	_STATE->parameters[PVAMPSRANGE].min = 1.;
	_STATE->parameters[PVAMPSRANGE].max = PVAmpsChannels;
	_STATE->parameters[PVAMPSRANGE].name = "CHANNELS";
	_STATE->parameters[PVAMPSRANGE].valuename = " ";
	_STATE->parameters[PVAMPSRANGE].type = ParameterType_double;
	_STATE->parameters[PVAMPSRANGE].digits = 0;
	_STATE->parameters[PVAMPSRANGE].initvalue = 1;
	_STATE->parameters[PVAMPSRANGE].progress = 1;
	_STATE->parameters[PVAMPSRANGE].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSBOUNDA].category = pvamps;
	_STATE->parameters[PVAMPSBOUNDA].min = LOG10D20F(18.);
	_STATE->parameters[PVAMPSBOUNDA].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[PVAMPSBOUNDA].name = "CUT A";
	_STATE->parameters[PVAMPSBOUNDA].valuename = "Hz";
	_STATE->parameters[PVAMPSBOUNDA].type = ParameterType_double;
	_STATE->parameters[PVAMPSBOUNDA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PVAMPSBOUNDA].initvalue = LOG10D20F(18.);
	_STATE->parameters[PVAMPSBOUNDA].progress = 1.;
	_STATE->parameters[PVAMPSBOUNDA].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSBOUNDB].category = pvamps;
	_STATE->parameters[PVAMPSBOUNDB].min = LOG10D20F(18.);
	_STATE->parameters[PVAMPSBOUNDB].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[PVAMPSBOUNDB].name = "CUT B";
	_STATE->parameters[PVAMPSBOUNDB].valuename = "Hz";
	_STATE->parameters[PVAMPSBOUNDB].type = ParameterType_double;
	_STATE->parameters[PVAMPSBOUNDB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PVAMPSBOUNDB].initvalue = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[PVAMPSBOUNDB].progress = 1.;
	_STATE->parameters[PVAMPSBOUNDB].flags |= Param::MidiParam;


	_STATE->parameters[PVAMPSDRY].category = pvamps;
	_STATE->parameters[PVAMPSDRY].min = -60;
	_STATE->parameters[PVAMPSDRY].max = 60.;
	_STATE->parameters[PVAMPSDRY].name = "DRY";
	_STATE->parameters[PVAMPSDRY].valuename = "dB";
	_STATE->parameters[PVAMPSDRY].type = ParameterType_double;
	_STATE->parameters[PVAMPSDRY].initvalue = -60;
	_STATE->parameters[PVAMPSDRY].progress = 1;
	_STATE->parameters[PVAMPSDRY].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSWET].category = pvamps;
	_STATE->parameters[PVAMPSWET].min = -60;
	_STATE->parameters[PVAMPSWET].max = 60.;
	_STATE->parameters[PVAMPSWET].name = "WET";
	_STATE->parameters[PVAMPSWET].valuename = "dB";
	_STATE->parameters[PVAMPSWET].type = ParameterType_double;
	_STATE->parameters[PVAMPSWET].initvalue = LOG10D20F(1.0);
	_STATE->parameters[PVAMPSWET].progress = 1;
	_STATE->parameters[PVAMPSWET].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSVCOWAVEFORM].category = pvamps;
	_STATE->parameters[PVAMPSVCOWAVEFORM].initvalue = -1.;
	_STATE->parameters[PVAMPSVCOWAVEFORM].type = ParameterType_enum;
	_STATE->parameters[PVAMPSVCOWAVEFORM].names = vcoWaveforms;
	_STATE->parameters[PVAMPSVCOWAVEFORM].values = vcoModes;
	_STATE->parameters[PVAMPSVCOWAVEFORM].name = "WAVEFORM";
	_STATE->parameters[PVAMPSVCOWAVEFORM].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSVCOWAVEFORM2].category = pvamps;
	_STATE->parameters[PVAMPSVCOWAVEFORM2].type = ParameterType_enum;
	_STATE->parameters[PVAMPSVCOWAVEFORM2].names = vcoWaveforms;
	_STATE->parameters[PVAMPSVCOWAVEFORM2].values = vcoModes;
	_STATE->parameters[PVAMPSVCOWAVEFORM2].name = "WAVEFORM";
	_STATE->parameters[PVAMPSVCOWAVEFORM2].flags |= Param::MidiParam;



	_STATE->parameters[PVAMPSPHASE].category = pvamps;
	_STATE->parameters[PVAMPSPHASE].type = ParameterType_enum;
	_STATE->parameters[PVAMPSPHASE].names = oscnames;
	_STATE->parameters[PVAMPSPHASE].name = "PHASE";
	_STATE->parameters[PVAMPSPHASE].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSSMOOTH].category = pvamps;
	_STATE->parameters[PVAMPSSMOOTH].min = LOG10D20F(.001);
	_STATE->parameters[PVAMPSSMOOTH].max = LOG10D20F(1.0);
	_STATE->parameters[PVAMPSSMOOTH].name = "SMOOTH";
	_STATE->parameters[PVAMPSSMOOTH].valuename = " ";
	_STATE->parameters[PVAMPSSMOOTH].type = ParameterType_double;
	_STATE->parameters[PVAMPSSMOOTH].initvalue = -30;
	_STATE->parameters[PVAMPSSMOOTH].progress = 1.0;
	_STATE->parameters[PVAMPSSMOOTH].digits = 3;
	_STATE->parameters[PVAMPSSMOOTH].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PVAMPSSMOOTH].flags |= Param::MidiParam;

	// The one PVAMPS reads. Linear 0..1 that rises the way the name reads: 0 is
	// no smoothing at all, 1 is the longest glide and the longest hold. It maps
	// onto the old -60..0 dB cutoff reversed (see PVAmps::onBufferReady), so the
	// 0.5 default is bit-for-bit the old -30 dB one.
	_STATE->parameters[PVAMPSSMOOTH2].category = pvamps;
	_STATE->parameters[PVAMPSSMOOTH2].min = 0.0;
	_STATE->parameters[PVAMPSSMOOTH2].max = 1.0;
	_STATE->parameters[PVAMPSSMOOTH2].name = "SMOOTH";
	_STATE->parameters[PVAMPSSMOOTH2].valuename = " ";
	_STATE->parameters[PVAMPSSMOOTH2].type = ParameterType_double;
	_STATE->parameters[PVAMPSSMOOTH2].initvalue = 0.5;
	_STATE->parameters[PVAMPSSMOOTH2].progress = 0.01;
	_STATE->parameters[PVAMPSSMOOTH2].digits = 2;
	_STATE->parameters[PVAMPSSMOOTH2].flags |= Param::MidiParam;

	// Defaults to CLASSIC: the routing the effect always had.
	_STATE->parameters[PVAMPSROUTE].category = pvamps;
	_STATE->parameters[PVAMPSROUTE].type = ParameterType_enum;
	_STATE->parameters[PVAMPSROUTE].names = pvampsroutes;
	_STATE->parameters[PVAMPSROUTE].name = "ROUTING";
	_STATE->parameters[PVAMPSROUTE].initvalue = 0;
	_STATE->parameters[PVAMPSROUTE].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPSPW].category = pvamps;
	_STATE->parameters[PVAMPSPW].min = 0.0;
	_STATE->parameters[PVAMPSPW].max = 1.0;
	_STATE->parameters[PVAMPSPW].name = "PW";
	_STATE->parameters[PVAMPSPW].valuename = " ";
	_STATE->parameters[PVAMPSPW].type = ParameterType_double;
	_STATE->parameters[PVAMPSPW].initvalue = .5;
	_STATE->parameters[PVAMPSPW].progress = .05;
	_STATE->parameters[PVAMPSPW].digits = 2;

	// The effect is RETUNE in the UI and to the host; the internal identifiers
	// (PMAP* params, SPACE_PITCHMAP, PitchMap, pitchmap.h) keep the old name --
	// renaming the space id would shift every later id and break saved spaces.
	const char* pitchmap = "RETUNE";

	// CPS names a pitch, so it is kept in hertz with the dB-log storage and the
	// Log10 curve rather than normalised -- same as PVSNAPROOT and VOC2HP. An LFO
	// on it then sweeps linearly in pitch, because the stored domain is the log
	// domain, and the mapping plays a melody instead of crawling through the
	// bottom octave.
	_STATE->parameters[PMAPCPS].category = pitchmap;
	_STATE->parameters[PMAPCPS].min = LOG10D20F(27.5);
	_STATE->parameters[PMAPCPS].max = LOG10D20F(2000.);
	_STATE->parameters[PMAPCPS].name = "CPS";
	_STATE->parameters[PMAPCPS].valuename = "Hz";
	_STATE->parameters[PMAPCPS].type = ParameterType_double;
	_STATE->parameters[PMAPCPS].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PMAPCPS].initvalue = LOG10D20F(220.);
	_STATE->parameters[PMAPCPS].progress = 1.;
	_STATE->parameters[PMAPCPS].digits = 1;
	_STATE->parameters[PMAPCPS].flags |= Param::MidiParam;

	_STATE->parameters[PMAPSCALE].category = pitchmap;
	_STATE->parameters[PMAPSCALE].name = "SCALE";
	_STATE->parameters[PMAPSCALE].type = ParameterType_enum;
	_STATE->parameters[PMAPSCALE].names = pitchmap_scales;
	_STATE->parameters[PMAPSCALE].initvalue = 7.;	// MIN PENT
	_STATE->parameters[PMAPSCALE].flags |= Param::MidiParam;

	_STATE->parameters[PMAPAMT].category = pitchmap;
	_STATE->parameters[PMAPAMT].min = 0.;
	_STATE->parameters[PMAPAMT].max = 1.;
	_STATE->parameters[PMAPAMT].name = "AMOUNT";
	_STATE->parameters[PMAPAMT].valuename = " ";
	_STATE->parameters[PMAPAMT].type = ParameterType_double;
	_STATE->parameters[PMAPAMT].initvalue = 1.;
	_STATE->parameters[PMAPAMT].progress = .01;
	_STATE->parameters[PMAPAMT].digits = 2;
	_STATE->parameters[PMAPAMT].flags |= Param::MidiParam;

	_STATE->parameters[PMAPSOURCES].category = pitchmap;
	_STATE->parameters[PMAPSOURCES].min = 1.;
	_STATE->parameters[PMAPSOURCES].max = PitchMapMaxSources;
	_STATE->parameters[PMAPSOURCES].name = "NOTES";
	_STATE->parameters[PMAPSOURCES].valuename = " ";
	_STATE->parameters[PMAPSOURCES].type = ParameterType_double;
	_STATE->parameters[PMAPSOURCES].digits = 0;
	_STATE->parameters[PMAPSOURCES].initvalue = 3.;
	_STATE->parameters[PMAPSOURCES].progress = 1.;
	_STATE->parameters[PMAPSOURCES].flags |= Param::MidiParam;

	// Everything the detector could not assign to a note -- drums, breath,
	// consonants, and any peak that is not part of a harmonic series -- passes
	// through unmoved. PURIFY is how much of it to take away.
	_STATE->parameters[PMAPPURIFY].category = pitchmap;
	_STATE->parameters[PMAPPURIFY].min = 0.;
	_STATE->parameters[PMAPPURIFY].max = 1.;
	_STATE->parameters[PMAPPURIFY].name = "PURIFY";
	_STATE->parameters[PMAPPURIFY].valuename = " ";
	_STATE->parameters[PMAPPURIFY].type = ParameterType_double;
	_STATE->parameters[PMAPPURIFY].initvalue = 0.;
	_STATE->parameters[PMAPPURIFY].progress = .01;
	_STATE->parameters[PMAPPURIFY].digits = 2;
	_STATE->parameters[PMAPPURIFY].flags |= Param::MidiParam;

	_STATE->parameters[PMAPFORMANT].category = pitchmap;
	_STATE->parameters[PMAPFORMANT].min = 0.;
	_STATE->parameters[PMAPFORMANT].max = 1.;
	_STATE->parameters[PMAPFORMANT].name = "FORMANT";
	_STATE->parameters[PMAPFORMANT].valuename = " ";
	_STATE->parameters[PMAPFORMANT].type = ParameterType_double;
	_STATE->parameters[PMAPFORMANT].initvalue = 0.;
	_STATE->parameters[PMAPFORMANT].progress = .01;
	_STATE->parameters[PMAPFORMANT].digits = 2;
	_STATE->parameters[PMAPFORMANT].flags |= Param::MidiParam;

	// How much of a note's tracked pitch is history. High values make the
	// mapping decision slow to change, which is what stops a wavering source
	// flicking between two scale steps; it does not slow the shift itself, which
	// is always built from the frequency measured this frame.
	_STATE->parameters[PMAPGLIDE].category = pitchmap;
	_STATE->parameters[PMAPGLIDE].min = 0.;
	_STATE->parameters[PMAPGLIDE].max = 1.;
	_STATE->parameters[PMAPGLIDE].name = "SMOOTH";
	_STATE->parameters[PMAPGLIDE].valuename = " ";
	_STATE->parameters[PMAPGLIDE].type = ParameterType_double;
	_STATE->parameters[PMAPGLIDE].initvalue = .7;
	_STATE->parameters[PMAPGLIDE].progress = .01;
	_STATE->parameters[PMAPGLIDE].digits = 2;
	_STATE->parameters[PMAPGLIDE].flags |= Param::MidiParam;

	// The band the fundamentals are searched in. Narrowing it is both faster and
	// more accurate: a bass part and a lead do not need the same search.
	_STATE->parameters[PMAPLO].category = pitchmap;
	_STATE->parameters[PMAPLO].min = LOG10D20F(20.);
	_STATE->parameters[PMAPLO].max = LOG10D20F(1000.);
	_STATE->parameters[PMAPLO].name = "LOW";
	_STATE->parameters[PMAPLO].valuename = "Hz";
	_STATE->parameters[PMAPLO].type = ParameterType_double;
	_STATE->parameters[PMAPLO].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PMAPLO].initvalue = LOG10D20F(60.);
	_STATE->parameters[PMAPLO].progress = 1.;
	_STATE->parameters[PMAPLO].digits = 1;
	_STATE->parameters[PMAPLO].flags |= Param::MidiParam;

	_STATE->parameters[PMAPHI].category = pitchmap;
	_STATE->parameters[PMAPHI].min = LOG10D20F(100.);
	_STATE->parameters[PMAPHI].max = LOG10D20F(4000.);
	_STATE->parameters[PMAPHI].name = "HIGH";
	_STATE->parameters[PMAPHI].valuename = "Hz";
	_STATE->parameters[PMAPHI].type = ParameterType_double;
	_STATE->parameters[PMAPHI].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PMAPHI].initvalue = LOG10D20F(1000.);
	_STATE->parameters[PMAPHI].progress = 1.;
	_STATE->parameters[PMAPHI].digits = 1;
	_STATE->parameters[PMAPHI].flags |= Param::MidiParam;

	_STATE->parameters[PMAPDRY].category = pitchmap;
	_STATE->parameters[PMAPDRY].min = -60.;
	_STATE->parameters[PMAPDRY].max = 60.;
	_STATE->parameters[PMAPDRY].name = "DRY";
	_STATE->parameters[PMAPDRY].valuename = "dB";
	_STATE->parameters[PMAPDRY].type = ParameterType_double;
	_STATE->parameters[PMAPDRY].initvalue = -60.;
	_STATE->parameters[PMAPDRY].progress = 1.;
	_STATE->parameters[PMAPDRY].flags |= Param::MidiParam;

	_STATE->parameters[PMAPWET].category = pitchmap;
	_STATE->parameters[PMAPWET].min = -60.;
	_STATE->parameters[PMAPWET].max = 60.;
	_STATE->parameters[PMAPWET].name = "WET";
	_STATE->parameters[PMAPWET].valuename = "dB";
	_STATE->parameters[PMAPWET].type = ParameterType_double;
	_STATE->parameters[PMAPWET].initvalue = LOG10D20F(1.0);
	_STATE->parameters[PMAPWET].progress = 1.;
	_STATE->parameters[PMAPWET].flags |= Param::MidiParam;


	const char* grainmodal = "GRAIN MODAL";

	_STATE->parameters[GRAINMODALDRY].category = grainmodal;
	_STATE->parameters[GRAINMODALDRY].min = -120;
	_STATE->parameters[GRAINMODALDRY].max = 60.;
	_STATE->parameters[GRAINMODALDRY].name = "DRY";
	_STATE->parameters[GRAINMODALDRY].valuename = "dB";
	_STATE->parameters[GRAINMODALDRY].type = ParameterType_double;
	_STATE->parameters[GRAINMODALDRY].initvalue = -120;
	_STATE->parameters[GRAINMODALDRY].progress = 1;
	_STATE->parameters[GRAINMODALDRY].flags |= Param::MidiParam;

	_STATE->parameters[GRAINMODALWET].category = grainmodal;
	_STATE->parameters[GRAINMODALWET].min = -120;
	_STATE->parameters[GRAINMODALWET].max = 60.;
	_STATE->parameters[GRAINMODALWET].name = "WET";
	_STATE->parameters[GRAINMODALWET].valuename = "dB";
	_STATE->parameters[GRAINMODALWET].type = ParameterType_double;
	_STATE->parameters[GRAINMODALWET].initvalue = -10;
	_STATE->parameters[GRAINMODALWET].progress = 1;
	_STATE->parameters[GRAINMODALWET].flags |= Param::MidiParam;

	_STATE->parameters[GRAINMODALINGAIN].category = grainmodal;
	_STATE->parameters[GRAINMODALINGAIN].min = -120;
	_STATE->parameters[GRAINMODALINGAIN].max = 60.;
	_STATE->parameters[GRAINMODALINGAIN].name = "IN GAIN";
	_STATE->parameters[GRAINMODALINGAIN].valuename = "dB";
	_STATE->parameters[GRAINMODALINGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINMODALINGAIN].initvalue = -120;
	_STATE->parameters[GRAINMODALINGAIN].progress = 1;
	_STATE->parameters[GRAINMODALINGAIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINMODALPREGAIN].category = grainmodal;
	_STATE->parameters[GRAINMODALPREGAIN].min = -120;
	_STATE->parameters[GRAINMODALPREGAIN].max = 60.;
	_STATE->parameters[GRAINMODALPREGAIN].name = "PRE GAIN";
	_STATE->parameters[GRAINMODALPREGAIN].valuename = "dB";
	_STATE->parameters[GRAINMODALPREGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINMODALPREGAIN].initvalue = LOG10D20F(1.0);
	_STATE->parameters[GRAINMODALPREGAIN].progress = 1;
	_STATE->parameters[GRAINMODALPREGAIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINMODALFREQ].category = grainmodal;
	_STATE->parameters[GRAINMODALFREQ].min = LOG10D20F(20.);
	_STATE->parameters[GRAINMODALFREQ].max = LOG10D20F(14000.);
	_STATE->parameters[GRAINMODALFREQ].name = "CPS";
	_STATE->parameters[GRAINMODALFREQ].valuename = "Hz";
	_STATE->parameters[GRAINMODALFREQ].type = ParameterType_double;
	_STATE->parameters[GRAINMODALFREQ].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINMODALFREQ].initvalue = LOG10D20F(220.);
	_STATE->parameters[GRAINMODALFREQ].progress = 1.;
	_STATE->parameters[GRAINMODALFREQ].flags |= Param::MidiParam;
	_STATE->parameters[GRAINMODALCPSOLD0].initvalue = _STATE->parameters[GRAINMODALCPSOLD1].initvalue = 220.;

	_STATE->parameters[GRAINMODALQ].category = grainmodal;
	_STATE->parameters[GRAINMODALQ].min = 0;
	_STATE->parameters[GRAINMODALQ].max = 1;
	_STATE->parameters[GRAINMODALQ].name = "RES";
	_STATE->parameters[GRAINMODALQ].valuename = " ";
	_STATE->parameters[GRAINMODALQ].type = ParameterType_double;
	_STATE->parameters[GRAINMODALQ].initvalue = .33;
	_STATE->parameters[GRAINMODALQ].progress = .05;
	_STATE->parameters[GRAINMODALQ].digits = 2;
	_STATE->parameters[GRAINMODALQ].flags |= Param::MidiParam;

	_STATE->parameters[GRAINMODALMODE].category = grainmodal;
	_STATE->parameters[GRAINMODALMODE].name = "MODE";
	_STATE->parameters[GRAINMODALMODE].initvalue = 0;
	_STATE->parameters[GRAINMODALMODE].type = ParameterType_enum;
	_STATE->parameters[GRAINMODALMODE].names = modal_names;
	_STATE->parameters[GRAINMODALMODE].flags |= Param::MidiParam;


	_STATE->parameters[GRAINMODALPRELPCUT].category = grainmodal;
	_STATE->parameters[GRAINMODALPRELPCUT].min = LOG10D20F(20);
	_STATE->parameters[GRAINMODALPRELPCUT].max = LOG10D20F(20000.);
	_STATE->parameters[GRAINMODALPRELPCUT].name = "INLPCUT";
	_STATE->parameters[GRAINMODALPRELPCUT].valuename = "Hz";
	_STATE->parameters[GRAINMODALPRELPCUT].type = ParameterType_double;
	_STATE->parameters[GRAINMODALPRELPCUT].digits = 0;
	_STATE->parameters[GRAINMODALPRELPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINMODALPRELPCUT].initvalue = LOG10D20F(20000.);
	_STATE->parameters[GRAINMODALPRELPCUT].progress = 1;

	_STATE->parameters[GRAINMODALPREHPCUT].category = grainmodal;
	_STATE->parameters[GRAINMODALPREHPCUT].min = LOG10D20F(18);
	_STATE->parameters[GRAINMODALPREHPCUT].max = LOG10D20F(8000.);
	_STATE->parameters[GRAINMODALPREHPCUT].name = "INHPCUT";
	_STATE->parameters[GRAINMODALPREHPCUT].valuename = "Hz";
	_STATE->parameters[GRAINMODALPREHPCUT].type = ParameterType_double;
	_STATE->parameters[GRAINMODALPREHPCUT].digits = 0;
	_STATE->parameters[GRAINMODALPREHPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINMODALPREHPCUT].initvalue = LOG10D20F(18.);
	_STATE->parameters[GRAINMODALPREHPCUT].progress = 1;

	_STATE->parameters[GRAINMODALSMOOTH].category = grainmodal;
	_STATE->parameters[GRAINMODALSMOOTH].min = LOG10D20F(0.001);
	_STATE->parameters[GRAINMODALSMOOTH].max = LOG10D20F(1);
	_STATE->parameters[GRAINMODALSMOOTH].name = "SMOOTH";
	_STATE->parameters[GRAINMODALSMOOTH].valuename = " ";
	_STATE->parameters[GRAINMODALSMOOTH].type = ParameterType_double;
	_STATE->parameters[GRAINMODALSMOOTH].digits = 3;
	_STATE->parameters[GRAINMODALSMOOTH].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINMODALSMOOTH].initvalue = LOG10D20F(.5);
	_STATE->parameters[GRAINMODALSMOOTH].progress = 1;

	_STATE->parameters[GRAINMODALFOLLOW].category = grainmodal;
	_STATE->parameters[GRAINMODALFOLLOW].name = "FOLLOW";
	_STATE->parameters[GRAINMODALFOLLOW].type = ParameterType_bool;
	_STATE->parameters[GRAINMODALFOLLOW].flags |= Param::MidiParam;


	_STATE->parameters[GRAINMODALHOLD].category = grainmodal;
	_STATE->parameters[GRAINMODALHOLD].name = "HOLD";
	_STATE->parameters[GRAINMODALHOLD].type = ParameterType_bool;
	_STATE->parameters[GRAINMODALHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	const char* buzz = "BUZZ";

	_STATE->parameters[GRAINPARTFREQ].category = buzz;
	_STATE->parameters[GRAINPARTFREQ].min = LOG10D20F(18.);
	_STATE->parameters[GRAINPARTFREQ].max = LOG10D20F(5000.);
	_STATE->parameters[GRAINPARTFREQ].name = "CPS";
	_STATE->parameters[GRAINPARTFREQ].valuename = "Hz";
	_STATE->parameters[GRAINPARTFREQ].type = ParameterType_double;
	_STATE->parameters[GRAINPARTFREQ].initvalue = LOG10D20F(220.);
	_STATE->parameters[GRAINPARTFREQ].progress = 1.;
	_STATE->parameters[GRAINPARTFREQ].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINPARTFREQ].flags |= Param::MidiParam;
	_STATE->parameters[GRAINBUZZCPSOLD0].initvalue = _STATE->parameters[GRAINBUZZCPSOLD1].initvalue = 220.;


	_STATE->parameters[GRAINPARTNUMPART].category = buzz;
	_STATE->parameters[GRAINPARTNUMPART].min = 1.;
	_STATE->parameters[GRAINPARTNUMPART].max = 20.;
	_STATE->parameters[GRAINPARTNUMPART].name = "PARTIALS";
	_STATE->parameters[GRAINPARTNUMPART].valuename = " ";
	_STATE->parameters[GRAINPARTNUMPART].type = ParameterType_double;
	_STATE->parameters[GRAINPARTNUMPART].initvalue = 1.0;
	_STATE->parameters[GRAINPARTNUMPART].progress = 1;
	_STATE->parameters[GRAINPARTNUMPART].flags |= Param::MidiParam;


	_STATE->parameters[GRAINPARTOFFSET].category = buzz;
	_STATE->parameters[GRAINPARTOFFSET].min = 1.;
	_STATE->parameters[GRAINPARTOFFSET].max = 20.;
	_STATE->parameters[GRAINPARTOFFSET].name = "OFFSET";
	_STATE->parameters[GRAINPARTOFFSET].valuename = " ";
	_STATE->parameters[GRAINPARTOFFSET].type = ParameterType_double;
	_STATE->parameters[GRAINPARTOFFSET].initvalue = 1;
	_STATE->parameters[GRAINPARTOFFSET].progress = 1;
	_STATE->parameters[GRAINPARTOFFSET].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPARTMULTI].category = buzz;
	_STATE->parameters[GRAINPARTMULTI].min = 0.;
	_STATE->parameters[GRAINPARTMULTI].max = 1.;
	_STATE->parameters[GRAINPARTMULTI].name = "BRIGHTNESS";
	_STATE->parameters[GRAINPARTMULTI].valuename = " ";
	_STATE->parameters[GRAINPARTMULTI].type = ParameterType_double;
	_STATE->parameters[GRAINPARTMULTI].initvalue = .5;
	_STATE->parameters[GRAINPARTMULTI].progress = .01;
	_STATE->parameters[GRAINPARTMULTI].digits = 2;
	_STATE->parameters[GRAINPARTMULTI].flags |= Param::MidiParam;


	_STATE->parameters[GRAINPARTDRY].category = buzz;
	_STATE->parameters[GRAINPARTDRY].min = -120;
	_STATE->parameters[GRAINPARTDRY].max = 120.;
	_STATE->parameters[GRAINPARTDRY].name = "DRY";
	_STATE->parameters[GRAINPARTDRY].valuename = "dB";
	_STATE->parameters[GRAINPARTDRY].type = ParameterType_double;
	_STATE->parameters[GRAINPARTDRY].initvalue = -120;
	_STATE->parameters[GRAINPARTDRY].progress = 1;
	_STATE->parameters[GRAINPARTDRY].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPARTWET].category = buzz;
	_STATE->parameters[GRAINPARTWET].min = -120;
	_STATE->parameters[GRAINPARTWET].max = 120.;
	_STATE->parameters[GRAINPARTWET].name = "WET";
	_STATE->parameters[GRAINPARTWET].valuename = "dB";
	_STATE->parameters[GRAINPARTWET].type = ParameterType_double;
	_STATE->parameters[GRAINPARTWET].initvalue = -16;
	_STATE->parameters[GRAINPARTWET].progress = 1;
	_STATE->parameters[GRAINPARTWET].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPARTPRELPCUT].category = buzz;
	_STATE->parameters[GRAINPARTPRELPCUT].min = LOG10D20F(20);
	_STATE->parameters[GRAINPARTPRELPCUT].max = LOG10D20F(20000.);
	_STATE->parameters[GRAINPARTPRELPCUT].name = "INLPCUT";
	_STATE->parameters[GRAINPARTPRELPCUT].valuename = "Hz";
	_STATE->parameters[GRAINPARTPRELPCUT].type = ParameterType_double;
	_STATE->parameters[GRAINPARTPRELPCUT].digits = 0;
	_STATE->parameters[GRAINPARTPRELPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINPARTPRELPCUT].initvalue = LOG10D20F(20000.);
	_STATE->parameters[GRAINPARTPRELPCUT].progress = 1;

	_STATE->parameters[GRAINPARTPREHPCUT].category = buzz;
	_STATE->parameters[GRAINPARTPREHPCUT].min = LOG10D20F(18);
	_STATE->parameters[GRAINPARTPREHPCUT].max = LOG10D20F(8000.);
	_STATE->parameters[GRAINPARTPREHPCUT].name = "INHPCUT";
	_STATE->parameters[GRAINPARTPREHPCUT].valuename = "Hz";
	_STATE->parameters[GRAINPARTPREHPCUT].type = ParameterType_double;
	_STATE->parameters[GRAINPARTPREHPCUT].digits = 0;
	_STATE->parameters[GRAINPARTPREHPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINPARTPREHPCUT].initvalue = LOG10D20F(18.);
	_STATE->parameters[GRAINPARTPREHPCUT].progress = 1;


	_STATE->parameters[GRAINPARTPREFILTER].category = buzz;
	_STATE->parameters[GRAINPARTPREFILTER].name = "FILTERIN";

	_STATE->parameters[GRAINPARTSMOOTH].category = buzz;
	_STATE->parameters[GRAINPARTSMOOTH].min = LOG10D20F(0.001);
	_STATE->parameters[GRAINPARTSMOOTH].max = LOG10D20F(1);
	_STATE->parameters[GRAINPARTSMOOTH].name = "SMOOTH";
	_STATE->parameters[GRAINPARTSMOOTH].valuename = " ";
	_STATE->parameters[GRAINPARTSMOOTH].type = ParameterType_double;
	_STATE->parameters[GRAINPARTSMOOTH].digits = 3;
	_STATE->parameters[GRAINPARTSMOOTH].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINPARTSMOOTH].initvalue = LOG10D20F(.5);
	_STATE->parameters[GRAINPARTSMOOTH].progress = 1;

	_STATE->parameters[GRAINPARTFOLLOW].category = buzz;
	_STATE->parameters[GRAINPARTFOLLOW].name = "FOLLOW";
	_STATE->parameters[GRAINPARTFOLLOW].type = ParameterType_bool;
	_STATE->parameters[GRAINPARTFOLLOW].flags |= Param::MidiParam;


	_STATE->parameters[GRAINPARTHOLD].category = buzz;
	_STATE->parameters[GRAINPARTHOLD].name = "HOLD";
	_STATE->parameters[GRAINPARTHOLD].type = ParameterType_bool;
	_STATE->parameters[GRAINPARTHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	_STATE->parameters[GRAINPARTPORT].initvalue = 1;
	_STATE->parameters[GRAINPARTPORT].name = "PORTAMTO";

	const char* eq5 = "EQ5";

	_STATE->parameters[EQ5LOWCF].category = eq5;
	_STATE->parameters[EQ5LOWCF].min = LOG10D20F(18.);
	_STATE->parameters[EQ5LOWCF].max = LOG10D20F(20000.);
	_STATE->parameters[EQ5LOWCF].name = "LCF";
	_STATE->parameters[EQ5LOWCF].valuename = "Hz";
	_STATE->parameters[EQ5LOWCF].type = ParameterType_double;
	_STATE->parameters[EQ5LOWCF].initvalue = LOG10D20F(100.);
	_STATE->parameters[EQ5LOWCF].progress = 1.;
	_STATE->parameters[EQ5LOWCF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5LOWGAIN].category = eq5;
	_STATE->parameters[EQ5LOWGAIN].min = -24;
	_STATE->parameters[EQ5LOWGAIN].max = 24.;
	_STATE->parameters[EQ5LOWGAIN].name = "LGN";
	_STATE->parameters[EQ5LOWGAIN].valuename = "dB";
	_STATE->parameters[EQ5LOWGAIN].type = ParameterType_double;
	_STATE->parameters[EQ5LOWGAIN].initvalue = 0;
	_STATE->parameters[EQ5LOWGAIN].progress = 1;

	_STATE->parameters[EQ50CF].category = eq5;
	_STATE->parameters[EQ50CF].min = LOG10D20F(100.);
	_STATE->parameters[EQ50CF].max = LOG10D20F(400.);
	_STATE->parameters[EQ50CF].name = "0CF";
	_STATE->parameters[EQ50CF].valuename = "Hz";
	_STATE->parameters[EQ50CF].type = ParameterType_double;
	_STATE->parameters[EQ50CF].initvalue = LOG10D20F(300.);
	_STATE->parameters[EQ50CF].progress = 1.;
	_STATE->parameters[EQ50CF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ50GAIN].category = eq5;
	_STATE->parameters[EQ50GAIN].min = -24;
	_STATE->parameters[EQ50GAIN].max = 24.;
	_STATE->parameters[EQ50GAIN].name = "0GN";
	_STATE->parameters[EQ50GAIN].valuename = "dB";
	_STATE->parameters[EQ50GAIN].type = ParameterType_double;
	_STATE->parameters[EQ50GAIN].initvalue = 0;
	_STATE->parameters[EQ50GAIN].progress = 1;

	_STATE->parameters[EQ51CF].category = eq5;
	_STATE->parameters[EQ51CF].min = LOG10D20F(400.);
	_STATE->parameters[EQ51CF].max = LOG10D20F(1200.);
	_STATE->parameters[EQ51CF].name = "1CF";
	_STATE->parameters[EQ51CF].valuename = "Hz";
	_STATE->parameters[EQ51CF].type = ParameterType_double;
	_STATE->parameters[EQ51CF].initvalue = LOG10D20F(800.);
	_STATE->parameters[EQ51CF].progress = 1.;
	_STATE->parameters[EQ51CF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ51GAIN].category = eq5;
	_STATE->parameters[EQ51GAIN].min = -24;
	_STATE->parameters[EQ51GAIN].max = 24.;
	_STATE->parameters[EQ51GAIN].name = "1GN";
	_STATE->parameters[EQ51GAIN].valuename = "dB";
	_STATE->parameters[EQ51GAIN].type = ParameterType_double;
	_STATE->parameters[EQ51GAIN].initvalue = 0;
	_STATE->parameters[EQ51GAIN].progress = 1;

	_STATE->parameters[EQ52CF].category = eq5;
	_STATE->parameters[EQ52CF].min = LOG10D20F(1200.);
	_STATE->parameters[EQ52CF].max = LOG10D20F(2400.);
	_STATE->parameters[EQ52CF].name = "2CF";
	_STATE->parameters[EQ52CF].valuename = "Hz";
	_STATE->parameters[EQ52CF].type = ParameterType_double;
	_STATE->parameters[EQ52CF].initvalue = LOG10D20F(1240.);
	_STATE->parameters[EQ52CF].progress = 1.;
	_STATE->parameters[EQ52CF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ52GAIN].category = eq5;
	_STATE->parameters[EQ52GAIN].min = -24;
	_STATE->parameters[EQ52GAIN].max = 24.;
	_STATE->parameters[EQ52GAIN].name = "2GN";
	_STATE->parameters[EQ52GAIN].valuename = "dB";
	_STATE->parameters[EQ52GAIN].type = ParameterType_double;
	_STATE->parameters[EQ52GAIN].initvalue = 0;
	_STATE->parameters[EQ52GAIN].progress = 1;

	_STATE->parameters[EQ5HIGHCF].category = eq5;
	_STATE->parameters[EQ5HIGHCF].min = LOG10D20F(2400.);
	_STATE->parameters[EQ5HIGHCF].max = LOG10D20F(20000.);
	_STATE->parameters[EQ5HIGHCF].name = "HIG";
	_STATE->parameters[EQ5HIGHCF].valuename = "Hz";
	_STATE->parameters[EQ5HIGHCF].type = ParameterType_double;
	_STATE->parameters[EQ5HIGHCF].initvalue = LOG10D20F(3200.);
	_STATE->parameters[EQ5HIGHCF].progress = 1.;
	_STATE->parameters[EQ5HIGHCF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5HIGHGAIN].category = eq5;
	_STATE->parameters[EQ5HIGHGAIN].min = -24;
	_STATE->parameters[EQ5HIGHGAIN].max = 24.;
	_STATE->parameters[EQ5HIGHGAIN].name = "HGN";
	_STATE->parameters[EQ5HIGHGAIN].valuename = "dB";
	_STATE->parameters[EQ5HIGHGAIN].type = ParameterType_double;
	_STATE->parameters[EQ5HIGHGAIN].initvalue = 0;
	_STATE->parameters[EQ5HIGHGAIN].progress = 1;


	_STATE->parameters[EQ5QLOW].category = eq5;
	_STATE->parameters[EQ5QLOW].min = LOG10D20F(.01);
	_STATE->parameters[EQ5QLOW].max = LOG10D20F(1.);
	_STATE->parameters[EQ5QLOW].name = "QLOW";
	_STATE->parameters[EQ5QLOW].valuename = " ";
	_STATE->parameters[EQ5QLOW].type = ParameterType_double;
	_STATE->parameters[EQ5QLOW].initvalue = LOG10D20F(1.);
	_STATE->parameters[EQ5QLOW].progress = 1;
	_STATE->parameters[EQ5QLOW].digits = 2;
	_STATE->parameters[EQ5QLOW].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5Q0].category = eq5;
	_STATE->parameters[EQ5Q0].min = LOG10D20F(.01);
	_STATE->parameters[EQ5Q0].max = LOG10D20F(10.);
	_STATE->parameters[EQ5Q0].name = "Q0";
	_STATE->parameters[EQ5Q0].valuename = " ";
	_STATE->parameters[EQ5Q0].type = ParameterType_double;
	_STATE->parameters[EQ5Q0].initvalue = LOG10D20F(1.);
	_STATE->parameters[EQ5Q0].progress = 1;
	_STATE->parameters[EQ5Q0].digits = 2;
	_STATE->parameters[EQ5Q0].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5Q1].category = eq5;
	_STATE->parameters[EQ5Q1].min = LOG10D20F(.01);
	_STATE->parameters[EQ5Q1].max = LOG10D20F(10.);
	_STATE->parameters[EQ5Q1].name = "Q1";
	_STATE->parameters[EQ5Q1].valuename = " ";
	_STATE->parameters[EQ5Q1].type = ParameterType_double;
	_STATE->parameters[EQ5Q1].initvalue = LOG10D20F(1.);
	_STATE->parameters[EQ5Q1].progress = 1;
	_STATE->parameters[EQ5Q1].digits = 2;
	_STATE->parameters[EQ5Q1].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5Q2].category = eq5;
	_STATE->parameters[EQ5Q2].min = LOG10D20F(.01);
	_STATE->parameters[EQ5Q2].max = LOG10D20F(10.);
	_STATE->parameters[EQ5Q2].name = "Q2";
	_STATE->parameters[EQ5Q2].valuename = " ";
	_STATE->parameters[EQ5Q2].type = ParameterType_double;
	_STATE->parameters[EQ5Q2].initvalue = LOG10D20F(1.);
	_STATE->parameters[EQ5Q2].progress = 1;
	_STATE->parameters[EQ5Q2].digits = 2;
	_STATE->parameters[EQ5Q2].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5QHIGH].category = eq5;
	_STATE->parameters[EQ5QHIGH].min = LOG10D20F(.01);
	_STATE->parameters[EQ5QHIGH].max = LOG10D20F(1.);
	_STATE->parameters[EQ5QHIGH].name = "Q";
	_STATE->parameters[EQ5QHIGH].valuename = " ";
	_STATE->parameters[EQ5QHIGH].type = ParameterType_double;
	_STATE->parameters[EQ5QHIGH].initvalue = LOG10D20F(1.);
	_STATE->parameters[EQ5QHIGH].progress = 1;
	_STATE->parameters[EQ5QHIGH].digits = 2;
	_STATE->parameters[EQ5QHIGH].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[EQ5GAIN].category = eq5;
	_STATE->parameters[EQ5GAIN].min = -60;
	_STATE->parameters[EQ5GAIN].max = 60.;
	_STATE->parameters[EQ5GAIN].name = "GAIN";
	_STATE->parameters[EQ5GAIN].valuename = "dB";
	_STATE->parameters[EQ5GAIN].type = ParameterType_double;
	_STATE->parameters[EQ5GAIN].initvalue = 0;
	_STATE->parameters[EQ5GAIN].progress = 1;
	_STATE->parameters[EQ5GAIN].flags |= Param::MidiParam;




	_STATE->parameters[ENVF1_DEST].initvalue = SSBMODRATE;
	_STATE->parameters[ENVF2_DEST].initvalue = BPCENTER;
	_STATE->parameters[ENVF3_DEST].initvalue = DISTMIX;



	const char* gencreverb = "GENCREVERB";

	_STATE->parameters[SPECDELGAIN].category = gencreverb;
	_STATE->parameters[SPECDELGAIN].min = -60.;
	_STATE->parameters[SPECDELGAIN].max = 60.;
	_STATE->parameters[SPECDELGAIN].name = "GAIN";
	_STATE->parameters[SPECDELGAIN].valuename = "dB";
	_STATE->parameters[SPECDELGAIN].type = ParameterType_double;
	_STATE->parameters[SPECDELGAIN].progress = 1;
	_STATE->parameters[SPECDELGAIN].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELMIX].category = gencreverb;
	_STATE->parameters[SPECDELMIX].min = 0.;
	_STATE->parameters[SPECDELMIX].max = 1.;
	_STATE->parameters[SPECDELMIX].name = "MIX";
	_STATE->parameters[SPECDELMIX].valuename = " ";
	_STATE->parameters[SPECDELMIX].type = ParameterType_double;
	_STATE->parameters[SPECDELMIX].digits = 2;
	_STATE->parameters[SPECDELMIX].initvalue = 1.0;
	_STATE->parameters[SPECDELMIX].progress = .01;
	_STATE->parameters[SPECDELMIX].flags |= Param::MidiParam;


	_STATE->parameters[SPECDELDEL].category = gencreverb;
	_STATE->parameters[SPECDELDEL].min = SPECDELMINDELMS;
	_STATE->parameters[SPECDELDEL].max = SPECDELMAXDELMS;
	_STATE->parameters[SPECDELDEL].name = "DELAY";
	_STATE->parameters[SPECDELDEL].valuename = "ms";
	_STATE->parameters[SPECDELDEL].type = ParameterType_double;
	_STATE->parameters[SPECDELDEL].initvalue = 1000.;
	_STATE->parameters[SPECDELDEL].progress = 10.;
	_STATE->parameters[SPECDELDEL].digits = 0;
	// _STATE->parameters[SPECDELDEL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECDELDEL].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELMODE].category = gencreverb;
	_STATE->parameters[SPECDELMODE].name = "MODE";
	_STATE->parameters[SPECDELMODE].names =
		makeStringSpan(specdelmodes).first(specdelmodes_shown);
	_STATE->parameters[SPECDELMODE].type = ParameterType_enum;
	_STATE->parameters[SPECDELMODE].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELBOUNDA].category = gencreverb;
	_STATE->parameters[SPECDELBOUNDA].min = LOG10D20F(18.);
	_STATE->parameters[SPECDELBOUNDA].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[SPECDELBOUNDA].name = bounda;
	_STATE->parameters[SPECDELBOUNDA].valuename = "Hz";
	_STATE->parameters[SPECDELBOUNDA].type = ParameterType_double;
	_STATE->parameters[SPECDELBOUNDA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECDELBOUNDA].initvalue = _STATE->parameters[SPECDELBOUNDA].min;//LOG10D20F(2000.);
	_STATE->parameters[SPECDELBOUNDA].progress = 1.;
	_STATE->parameters[SPECDELBOUNDA].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELBOUNDB].category = gencreverb;
	_STATE->parameters[SPECDELBOUNDB].min = LOG10D20F(18.);
	_STATE->parameters[SPECDELBOUNDB].max = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[SPECDELBOUNDB].name = boundb;
	_STATE->parameters[SPECDELBOUNDB].valuename = "Hz";
	_STATE->parameters[SPECDELBOUNDB].type = ParameterType_double;
	_STATE->parameters[SPECDELBOUNDB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECDELBOUNDB].initvalue = LOG10D20F(_STATE->sr / 2);
	_STATE->parameters[SPECDELBOUNDB].progress = 1.;
	_STATE->parameters[SPECDELBOUNDB].flags |= Param::MidiParam;

	/* IR replacement front. A new IR does not appear at once: it is swapped in
	   one partition per partition-block, which is exactly one tap per sample -
	   the same rate at which a sound travels along the IR. These three shape
	   that handover. */
	/* log-scaled: most of the interesting range is below 1x, where the front
	   falls behind the sound. The offset puts an exact 0 - a parked front - at
	   the bottom of the range. */
	_STATE->parameters[SPECDELMORPHRATE].category = gencreverb;
	_STATE->parameters[SPECDELMORPHRATE].min = LOG10D20F(SPECDELRATEOFFS);
	_STATE->parameters[SPECDELMORPHRATE].max = LOG10D20F(SPECDELRATEMAX + SPECDELRATEOFFS);
	_STATE->parameters[SPECDELMORPHRATE].name = "RATE";
	_STATE->parameters[SPECDELMORPHRATE].valuename = "x";
	_STATE->parameters[SPECDELMORPHRATE].type = ParameterType_double;
	_STATE->parameters[SPECDELMORPHRATE].initvalue = LOG10D20F(1. + SPECDELRATEOFFS);
	_STATE->parameters[SPECDELMORPHRATE].progress = .5;
	_STATE->parameters[SPECDELMORPHRATE].digits = 2;
	_STATE->parameters[SPECDELMORPHRATE].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECDELMORPHRATE].offset = SPECDELRATEOFFS;
	_STATE->parameters[SPECDELMORPHRATE].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELMORPHFRONT].category = gencreverb;
	_STATE->parameters[SPECDELMORPHFRONT].min = 0.;
	_STATE->parameters[SPECDELMORPHFRONT].max = 1.;
	_STATE->parameters[SPECDELMORPHFRONT].name = "FRONT";
	_STATE->parameters[SPECDELMORPHFRONT].valuename = " ";
	_STATE->parameters[SPECDELMORPHFRONT].type = ParameterType_double;
	_STATE->parameters[SPECDELMORPHFRONT].initvalue = 1.;
	_STATE->parameters[SPECDELMORPHFRONT].progress = .01;
	_STATE->parameters[SPECDELMORPHFRONT].digits = 2;
	_STATE->parameters[SPECDELMORPHFRONT].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELMORPHREV].category = gencreverb;
	_STATE->parameters[SPECDELMORPHREV].name = "REVERSE";
	_STATE->parameters[SPECDELMORPHREV].type = ParameterType_bool;
	_STATE->parameters[SPECDELMORPHREV].flags |= Param::MidiParam;

	/* per-algorithm params, shown in the second space's sub-views. All plain
	   0..1 with the map inside the DSP, except STEPS (a count) and ROOT (a
	   pitch, Hz with dB-log storage like the BOUND knobs). */
	auto specdel01 = [&](int id, const char* nm, double init) {
		_STATE->parameters[id].category = gencreverb;
		_STATE->parameters[id].min = 0.;
		_STATE->parameters[id].max = 1.;
		_STATE->parameters[id].name = nm;
		_STATE->parameters[id].valuename = " ";
		_STATE->parameters[id].type = ParameterType_double;
		_STATE->parameters[id].digits = 2;
		_STATE->parameters[id].initvalue = init;
		_STATE->parameters[id].progress = .01;
		_STATE->parameters[id].flags |= Param::MidiParam;
	};
	specdel01(SPECDELSHAPE, "SHAPE", .5);
	specdel01(SPECDELRANDEVO, "EVOLVE", 0.);
	specdel01(SPECDELGAUSSEVO, "EVOLVE", 0.);
	specdel01(SPECDELFB, "FEEDBACK", .5);
	specdel01(SPECDELDAMP, "DAMP", .3);
	specdel01(SPECDELRIPPLES, "RIPPLES", .3);
	specdel01(SPECDELRIPPLEPH, "PHASE", 0.);
	specdel01(SPECDELDRIFTAMT, "AMOUNT", .3);
	specdel01(SPECDELDRIFTRATE, "RATE", .5);
	specdel01(SPECDELBARBN, "RIPPLES", .3);
	specdel01(SPECDELBARBSPEED, "SPEED", .5);
	specdel01(SPECDELTIDESPEED, "SPEED", .5);
	specdel01(SPECDELADEPTH, "DEPTH", .5);
	specdel01(SPECDELARATE, "RATE", .5);
	/* global IR shaping - first space, next to DELAY */
	specdel01(SPECDELDECAY, "DECAY", 0.);
	specdel01(SPECDELDAMPHF, "DAMP", 0.);
	/* LONGVERB: EVOLVE walks the kernel's phases (bounded step per roll,
	   NOT independent re-rolls - those landed as a lurch once per sweep).
	   The kernel is outside the loop, so this no longer decorrelates
	   repeats (the FDN has none) - it makes the colour drift over time
	   instead of sitting as one fixed spectral fingerprint. Left at max;
	   turning it down freezes the colour, which is a character choice. */
	specdel01(SPECDELVERBEVO, "EVOLVE", 1.);
	specdel01(SPECDELVERBDAMP, "DAMP", .2);
	/* the FDN room: SIZE is the line-length base, log 20..80 ms (.5 = 40 ms).
	   XFEED defaults to FULL - anything less leaves the lines partly isolated
	   and their individual mode grids stack up into a metallic ring, which is
	   what the 4-unit version had to be tuned to 1.0 for. */
	specdel01(SPECDELVERBSIZE, "SIZE", .5);
	specdel01(SPECDELVERBXFEED, "XFEED", 1.);

	_STATE->parameters[SPECDELSTEPS].category = gencreverb;
	_STATE->parameters[SPECDELSTEPS].min = 2;
	_STATE->parameters[SPECDELSTEPS].max = 24;
	_STATE->parameters[SPECDELSTEPS].name = "STEPS";
	_STATE->parameters[SPECDELSTEPS].valuename = " ";
	_STATE->parameters[SPECDELSTEPS].type = ParameterType_double;
	_STATE->parameters[SPECDELSTEPS].initvalue = 8;
	_STATE->parameters[SPECDELSTEPS].progress = 1.;
	_STATE->parameters[SPECDELSTEPS].digits = 0;
	_STATE->parameters[SPECDELSTEPS].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELROOT].category = gencreverb;
	_STATE->parameters[SPECDELROOT].min = LOG10D20F(30.);
	_STATE->parameters[SPECDELROOT].max = LOG10D20F(2000.);
	_STATE->parameters[SPECDELROOT].name = "ROOT";
	_STATE->parameters[SPECDELROOT].valuename = "Hz";
	_STATE->parameters[SPECDELROOT].type = ParameterType_double;
	_STATE->parameters[SPECDELROOT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECDELROOT].initvalue = LOG10D20F(110.);
	_STATE->parameters[SPECDELROOT].progress = 1.;
	_STATE->parameters[SPECDELROOT].digits = 0;
	_STATE->parameters[SPECDELROOT].flags |= Param::MidiParam;

	_STATE->parameters[SPECDELAINV].category = gencreverb;
	_STATE->parameters[SPECDELAINV].name = "INVERT";
	_STATE->parameters[SPECDELAINV].type = ParameterType_bool;
	_STATE->parameters[SPECDELAINV].flags |= Param::MidiParam;


	/* LONGVERB decay time - seconds, dB-log storage like ROOT/BOUND */
	_STATE->parameters[SPECDELVERBRT60].category = gencreverb;
	_STATE->parameters[SPECDELVERBRT60].min = LOG10D20F(.5);
	_STATE->parameters[SPECDELVERBRT60].max = LOG10D20F(120.);
	_STATE->parameters[SPECDELVERBRT60].name = "RT60";
	_STATE->parameters[SPECDELVERBRT60].valuename = "s";
	_STATE->parameters[SPECDELVERBRT60].type = ParameterType_double;
	_STATE->parameters[SPECDELVERBRT60].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[SPECDELVERBRT60].initvalue = LOG10D20F(20.);
	_STATE->parameters[SPECDELVERBRT60].progress = 1.;
	_STATE->parameters[SPECDELVERBRT60].digits = 1;
	_STATE->parameters[SPECDELVERBRT60].flags |= Param::MidiParam;

	const char* bit = "LOFI";

	_STATE->parameters[BCGAIN].category = bit;
	_STATE->parameters[BCGAIN].min = -60.;
	_STATE->parameters[BCGAIN].max = 60.;
	_STATE->parameters[BCGAIN].name = "GAIN";
	_STATE->parameters[BCGAIN].valuename = "dB";
	_STATE->parameters[BCGAIN].type = ParameterType_double;
	_STATE->parameters[BCGAIN].progress = 1;
	_STATE->parameters[BCGAIN].flags |= Param::MidiParam;

	_STATE->parameters[BCMIX].category = bit;
	_STATE->parameters[BCMIX].min = 0.;
	_STATE->parameters[BCMIX].max = 1.;
	_STATE->parameters[BCMIX].name = "MIX";
	_STATE->parameters[BCMIX].valuename = " ";
	_STATE->parameters[BCMIX].type = ParameterType_double;
	_STATE->parameters[BCMIX].digits = 2;
	_STATE->parameters[BCMIX].initvalue = .5;
	_STATE->parameters[BCMIX].progress = .05;
	_STATE->parameters[BCMIX].flags |= Param::MidiParam;

	_STATE->parameters[BCSR].category = bit;
	_STATE->parameters[BCSR].min = LOG10D20F(_STATE->sr / 250.);
	_STATE->parameters[BCSR].max = LOG10D20F(_STATE->sr);
	_STATE->parameters[BCSR].name = "SAMPLERATE";
	_STATE->parameters[BCSR].valuename = "Hz";
	_STATE->parameters[BCSR].type = ParameterType_double;
	_STATE->parameters[BCSR].initvalue = LOG10D20F(_STATE->sr);
	_STATE->parameters[BCSR].progress = 1;
	_STATE->parameters[BCSR].digits = 0;
	_STATE->parameters[BCSR].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[BCSR].flags |= Param::MidiParam;

	// Continuous depth: bitsToFact() is exp2-based, so fractional bit counts are
	// real settings, not just points the smoother passes through on its way
	// between two whole depths.
	_STATE->parameters[BCBITS].category = bit;
	_STATE->parameters[BCBITS].name = "BITS";
	_STATE->parameters[BCBITS].min = 1;
	_STATE->parameters[BCBITS].max = 16;
	_STATE->parameters[BCBITS].valuename = " ";
	_STATE->parameters[BCBITS].type = ParameterType_double;
	_STATE->parameters[BCBITS].digits = 2;
	_STATE->parameters[BCBITS].initvalue = 16;
	_STATE->parameters[BCBITS].progress = .01;
	_STATE->parameters[BCBITS].flags |= Param::MidiParam;

	_STATE->parameters[BCTONE].category = bit;
	_STATE->parameters[BCTONE].name = "TONE";
	_STATE->parameters[BCTONE].min = 0.;
	_STATE->parameters[BCTONE].max = 1.;
	_STATE->parameters[BCTONE].valuename = " ";
	_STATE->parameters[BCTONE].type = ParameterType_double;
	_STATE->parameters[BCTONE].initvalue = .5f;
	_STATE->parameters[BCTONE].progress = .05f;
	_STATE->parameters[BCTONE].digits = 2;
	_STATE->parameters[BCTONE].flags |= Param::MidiParam;

	const char* grainfilt = "GRAIN FILTER";

	_STATE->parameters[GRAINFILTERRES].category = grainfilt;
	_STATE->parameters[GRAINFILTERRES].min = 0;
	_STATE->parameters[GRAINFILTERRES].max = 1.;
	_STATE->parameters[GRAINFILTERRES].name = "RES/Q";
	_STATE->parameters[GRAINFILTERRES].valuename = " ";
	_STATE->parameters[GRAINFILTERRES].type = ParameterType_double;
	_STATE->parameters[GRAINFILTERRES].digits = 2;
	_STATE->parameters[GRAINFILTERRES].initvalue = .5;
	_STATE->parameters[GRAINFILTERRES].progress = .01;
	_STATE->parameters[GRAINFILTERRES].flags |= Param::MidiParam;

	_STATE->parameters[GRAINFILTERMIX].category = grainfilt;
	_STATE->parameters[GRAINFILTERMIX].min = 0;
	_STATE->parameters[GRAINFILTERMIX].max = 1.;
	_STATE->parameters[GRAINFILTERMIX].name = "MIX";
	_STATE->parameters[GRAINFILTERMIX].valuename = " ";
	_STATE->parameters[GRAINFILTERMIX].type = ParameterType_double;
	_STATE->parameters[GRAINFILTERMIX].digits = 2;
	_STATE->parameters[GRAINFILTERMIX].initvalue = 1;
	_STATE->parameters[GRAINFILTERMIX].progress = .01;
	_STATE->parameters[GRAINFILTERMIX].flags |= Param::MidiParam;

	_STATE->parameters[GRAINFILTERGAIN].category = grainfilt;
	_STATE->parameters[GRAINFILTERGAIN].min = -60;
	_STATE->parameters[GRAINFILTERGAIN].max = 60.;
	_STATE->parameters[GRAINFILTERGAIN].name = "GAIN";
	_STATE->parameters[GRAINFILTERGAIN].valuename = "dB";
	_STATE->parameters[GRAINFILTERGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINFILTERGAIN].digits = 0;
	_STATE->parameters[GRAINFILTERGAIN].initvalue = 0;
	_STATE->parameters[GRAINFILTERGAIN].progress = 1;
	_STATE->parameters[GRAINFILTERGAIN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINFILTERTYPE].category = grainfilt;
	_STATE->parameters[GRAINFILTERTYPE].initvalue = 0;
	_STATE->parameters[GRAINFILTERTYPE].type = ParameterType_enum;
	_STATE->parameters[GRAINFILTERTYPE].name = "TYPE";
	_STATE->parameters[GRAINFILTERTYPE].names = filtertypes;
	_STATE->parameters[GRAINFILTERTYPE].flags |= Param::MidiParam;


	_STATE->parameters[GRAINFILTERNSEGS].category = grainfilt;
	_STATE->parameters[GRAINFILTERNSEGS].initvalue = 4;
	_STATE->parameters[GRAINFILTERNSEGS].name = "SEGMENTS";

	_STATE->parameters[GRAINFILTERCURVE].category = grainfilt;
	_STATE->parameters[GRAINFILTERCURVE].initvalue = 0;
	_STATE->parameters[GRAINFILTERCURVE].type = ParameterType_enum;
	_STATE->parameters[GRAINFILTERCURVE].names = editorcurvenames;

	_STATE->parameters[GRAINFILTEREDRAW].initvalue = 1.0;

	_STATE->parameters[GRAINFILTERRECOMPUTE1].initvalue = 1.0;

	_STATE->parameters[GRAINFILTERRECOMPUTE2].initvalue = 1.0;
	for (int32_t i = 0; i < 9; i++) {
		_STATE->parameters[GRAINFILTERENVX0 + i].type = _STATE->parameters[GRAINFILTERENVY0 + i].type = ParameterType_double;
		_STATE->parameters[GRAINFILTERENVX0 + i].category = grainfilt;
		_STATE->parameters[GRAINFILTERENVX0 + i].initvalue = i < 5 ? i * .25 : 1;
		_STATE->parameters[GRAINFILTERENVX0 + i].min = 0.;
		_STATE->parameters[GRAINFILTERENVX0 + i].max = 1.;
		_STATE->parameters[GRAINFILTERENVY0 + i].min = 0.;
		_STATE->parameters[GRAINFILTERENVY0 + i].max = 1.;
	}
	_STATE->parameters[GRAINFILTERENVY0].initvalue = 0;
	_STATE->parameters[GRAINFILTERENVY1].initvalue = .5;
	_STATE->parameters[GRAINFILTERENVY2].initvalue = .5;
	_STATE->parameters[GRAINFILTERENVY3].initvalue = .5;
	_STATE->parameters[GRAINFILTERENVY4].initvalue = 0;

	_STATE->parameters[GRAINFILTERJOINENDS].category = grainfilt;
	_STATE->parameters[GRAINFILTERJOINENDS].initvalue = 0;
	_STATE->parameters[GRAINFILTERJOINENDS].name = "JOIN";
	_STATE->parameters[GRAINFILTERJOINENDS].type = ParameterType_bool;

	_STATE->parameters[GRAINFILTERQUANT].category = grainfilt;
	_STATE->parameters[GRAINFILTERQUANT].initvalue = 1;
	_STATE->parameters[GRAINFILTERQUANT].name = "QUANT";

	const char* vco = "VCO";
	const char* vcos[] = { "OSC1", "OSC2", "OSC3" };

	_STATE->parameters[GRAINVCO1PW].category = vco;
	_STATE->parameters[GRAINVCO1PW].subcategory = vcos[0];
	_STATE->parameters[GRAINVCO1PW].min = 0.0;
	_STATE->parameters[GRAINVCO1PW].max = 1;
	_STATE->parameters[GRAINVCO1PW].name = "PW";
	_STATE->parameters[GRAINVCO1PW].valuename = " ";
	_STATE->parameters[GRAINVCO1PW].type = ParameterType_double;
	_STATE->parameters[GRAINVCO1PW].initvalue = 0;
	_STATE->parameters[GRAINVCO1PW].progress = .01;
	_STATE->parameters[GRAINVCO1PW].digits = 2;
	_STATE->parameters[GRAINVCO1PW].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCO1DET].category = vco;
	_STATE->parameters[GRAINVCO1DET].subcategory = vcos[0];
	_STATE->parameters[GRAINVCO1DET].min = -50.;
	_STATE->parameters[GRAINVCO1DET].max = 50.;
	_STATE->parameters[GRAINVCO1DET].name = "DETUNE";
	_STATE->parameters[GRAINVCO1DET].valuename = "CENTS";
	_STATE->parameters[GRAINVCO1DET].type = ParameterType_double;
	_STATE->parameters[GRAINVCO1DET].initvalue = 0;
	_STATE->parameters[GRAINVCO1DET].progress = 1;
	_STATE->parameters[GRAINVCO1DET].digits = 2;
	_STATE->parameters[GRAINVCO1DET].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCO1AMP].category = vco;
	_STATE->parameters[GRAINVCO1AMP].subcategory = vcos[0];
	_STATE->parameters[GRAINVCO1AMP].min = -120.;
	_STATE->parameters[GRAINVCO1AMP].max = 120.;
	_STATE->parameters[GRAINVCO1AMP].name = "GAIN";
	_STATE->parameters[GRAINVCO1AMP].valuename = "dB";
	_STATE->parameters[GRAINVCO1AMP].type = ParameterType_double;
	_STATE->parameters[GRAINVCO1AMP].progress = 1;
	_STATE->parameters[GRAINVCO1AMP].initvalue = -16.;
	_STATE->parameters[GRAINVCO1AMP].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCO0WAVEFORM].category = vco;
	_STATE->parameters[GRAINVCO0WAVEFORM].subcategory = vcos[0];
	_STATE->parameters[GRAINVCO0WAVEFORM].name = "TYPE";
	_STATE->parameters[GRAINVCO0WAVEFORM].type = ParameterType_enum;
	_STATE->parameters[GRAINVCO0WAVEFORM].names = vcoWaveforms;
	_STATE->parameters[GRAINVCO0WAVEFORM].values = vcoModes;
	_STATE->parameters[GRAINVCO0WAVEFORM].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCO1POW].category = vco;
	_STATE->parameters[GRAINVCO1POW].initvalue = 1;
	_STATE->parameters[GRAINVCO1POW].name = "OSC1";
	_STATE->parameters[GRAINVCO1POW].type = ParameterType_bool;
	_STATE->parameters[GRAINVCO1POW].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCO2PW] = _STATE->parameters[GRAINVCO3PW] = _STATE->parameters[GRAINVCO1PW];
	_STATE->parameters[GRAINVCO2PW].subcategory = vcos[1]; _STATE->parameters[GRAINVCO3PW].subcategory = vcos[2];
	_STATE->parameters[GRAINVCO2DET] = _STATE->parameters[GRAINVCO3DET] = _STATE->parameters[GRAINVCO1DET];
	_STATE->parameters[GRAINVCO2DET].subcategory = vcos[1]; _STATE->parameters[GRAINVCO3DET].subcategory = vcos[2];
	_STATE->parameters[GRAINVCO2AMP] = _STATE->parameters[GRAINVCO3AMP] = _STATE->parameters[GRAINVCO1AMP];
	_STATE->parameters[GRAINVCO2AMP].subcategory = vcos[1]; _STATE->parameters[GRAINVCO3AMP].subcategory = vcos[2];
	_STATE->parameters[GRAINVCO2POW] = _STATE->parameters[GRAINVCO3POW] = _STATE->parameters[GRAINVCO1POW];
	_STATE->parameters[GRAINVCO2POW].name = "OSC2"; _STATE->parameters[GRAINVCO3POW].name = "OSC3";
	_STATE->parameters[GRAINVCO2POW].initvalue = _STATE->parameters[GRAINVCO3POW].initvalue = 0.0;
	_STATE->parameters[GRAINVCO1WAVEFORM] = _STATE->parameters[GRAINVCO2WAVEFORM] = _STATE->parameters[GRAINVCO0WAVEFORM];
	_STATE->parameters[GRAINVCO1WAVEFORM].subcategory = vcos[1]; _STATE->parameters[GRAINVCO2WAVEFORM].subcategory = vcos[2];
	
	_STATE->parameters[GRAINVCOCPS].category = vco;
	_STATE->parameters[GRAINVCOCPS].min = LOG10D20F(20);
	_STATE->parameters[GRAINVCOCPS].max = LOG10D20F(8000.);
	_STATE->parameters[GRAINVCOCPS].name = "CPS";
	_STATE->parameters[GRAINVCOCPS].valuename = "Hz";
	_STATE->parameters[GRAINVCOCPS].type = ParameterType_double;
	_STATE->parameters[GRAINVCOCPS].digits = 0;
	_STATE->parameters[GRAINVCOCPS].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINVCOCPS].initvalue = LOG10D20F(440.);
	_STATE->parameters[GRAINVCOCPS].progress = 1;
	_STATE->parameters[GRAINVCOCPS].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCODETLR].category = vco;
	_STATE->parameters[GRAINVCODETLR].min = -50.;
	_STATE->parameters[GRAINVCODETLR].max = 50.;
	_STATE->parameters[GRAINVCODETLR].name = "DETLR";
	_STATE->parameters[GRAINVCODETLR].valuename = "CENTS";
	_STATE->parameters[GRAINVCODETLR].type = ParameterType_double;
	_STATE->parameters[GRAINVCODETLR].initvalue = 0;
	_STATE->parameters[GRAINVCODETLR].progress = 1;
	_STATE->parameters[GRAINVCODETLR].digits = 2;
	_STATE->parameters[GRAINVCODETLR].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCOVCF].category = vco;
	_STATE->parameters[GRAINVCOVCF].name = "FILTER";
	_STATE->parameters[GRAINVCOVCF].type = ParameterType_bool;
	_STATE->parameters[GRAINVCOVCF].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCOPHRESET].category = vco;
	_STATE->parameters[GRAINVCOPHRESET].name = "PHASERESET";
	_STATE->parameters[GRAINVCOPHRESET].type = ParameterType_bool;
	_STATE->parameters[GRAINVCOPHRESET].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCODRY].category = vco;
	_STATE->parameters[GRAINVCODRY].min = -60;
	_STATE->parameters[GRAINVCODRY].max = 60.;
	_STATE->parameters[GRAINVCODRY].name = "DRY";
	_STATE->parameters[GRAINVCODRY].valuename = " ";
	_STATE->parameters[GRAINVCODRY].type = ParameterType_double;
	_STATE->parameters[GRAINVCODRY].digits = 0;
	_STATE->parameters[GRAINVCODRY].initvalue = -60;
	_STATE->parameters[GRAINVCODRY].progress = 1;
	_STATE->parameters[GRAINVCODRY].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCOWET].category = vco;
	_STATE->parameters[GRAINVCOWET].min = -60;
	_STATE->parameters[GRAINVCOWET].max = 60.;
	_STATE->parameters[GRAINVCOWET].name = "WET";
	_STATE->parameters[GRAINVCOWET].valuename = " ";
	_STATE->parameters[GRAINVCOWET].type = ParameterType_double;
	_STATE->parameters[GRAINVCOWET].digits = 0;
	_STATE->parameters[GRAINVCOWET].initvalue = -12;
	_STATE->parameters[GRAINVCOWET].progress = 1;
	_STATE->parameters[GRAINVCOWET].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCOFOLLOW].category = vco;
	_STATE->parameters[GRAINVCOFOLLOW].name = "FOLLOW";
	_STATE->parameters[GRAINVCOFOLLOW].type = ParameterType_bool;
	_STATE->parameters[GRAINVCOFOLLOW].flags |= Param::MidiParam;

	_STATE->parameters[GRAINVCOHOLD].category = vco;
	_STATE->parameters[GRAINVCOHOLD].name = "HOLD";
	_STATE->parameters[GRAINVCOHOLD].type = ParameterType_bool;
	_STATE->parameters[GRAINVCOHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	const char* specdel = "SPECTRAL DELAY";

	_STATE->parameters[SPECDEL2FB].category = specdel;
	_STATE->parameters[SPECDEL2FB].min = 0;
	_STATE->parameters[SPECDEL2FB].max = 1.;
	_STATE->parameters[SPECDEL2FB].name = "FB";
	_STATE->parameters[SPECDEL2FB].valuename = " ";
	_STATE->parameters[SPECDEL2FB].type = ParameterType_double;
	_STATE->parameters[SPECDEL2FB].digits = 2;
	_STATE->parameters[SPECDEL2FB].initvalue = .5;
	_STATE->parameters[SPECDEL2FB].progress = .01;
	_STATE->parameters[SPECDEL2FB].flags |= Param::MidiParam;

	_STATE->parameters[SPECDEL2DEL].category = specdel;
	_STATE->parameters[SPECDEL2DEL].min = 0;
	_STATE->parameters[SPECDEL2DEL].max = 10.;
	_STATE->parameters[SPECDEL2DEL].name = "MAXDEL";
	_STATE->parameters[SPECDEL2DEL].valuename = "s";
	_STATE->parameters[SPECDEL2DEL].type = ParameterType_double;
	_STATE->parameters[SPECDEL2DEL].digits = 2;
	_STATE->parameters[SPECDEL2DEL].initvalue = 1;
	_STATE->parameters[SPECDEL2DEL].progress = .1;
	_STATE->parameters[SPECDEL2DEL].flags |= Param::MidiParam;


	_STATE->parameters[SPECDEL2MIX].category = specdel;
	_STATE->parameters[SPECDEL2MIX].min = 0;
	_STATE->parameters[SPECDEL2MIX].max = 1.;
	_STATE->parameters[SPECDEL2MIX].name = "MIX";
	_STATE->parameters[SPECDEL2MIX].valuename = " ";
	_STATE->parameters[SPECDEL2MIX].type = ParameterType_double;
	_STATE->parameters[SPECDEL2MIX].digits = 2;
	_STATE->parameters[SPECDEL2MIX].initvalue = 1;
	_STATE->parameters[SPECDEL2MIX].progress = .01;
	_STATE->parameters[SPECDEL2MIX].flags |= Param::MidiParam;

	_STATE->parameters[SPECDEL2GAIN].category = specdel;
	_STATE->parameters[SPECDEL2GAIN].min = -60;
	_STATE->parameters[SPECDEL2GAIN].max = 60.;
	_STATE->parameters[SPECDEL2GAIN].name = "GAIN";
	_STATE->parameters[SPECDEL2GAIN].valuename = "dB";
	_STATE->parameters[SPECDEL2GAIN].type = ParameterType_double;
	_STATE->parameters[SPECDEL2GAIN].digits = 0;
	_STATE->parameters[SPECDEL2GAIN].initvalue = 0;
	_STATE->parameters[SPECDEL2GAIN].progress = 1;
	_STATE->parameters[SPECDEL2GAIN].flags |= Param::MidiParam;

	_STATE->parameters[SPECDEL2NSEGS].category = specdel;
	_STATE->parameters[SPECDEL2NSEGS].initvalue = 4;
	_STATE->parameters[SPECDEL2NSEGS].name = "SEGMENTS";

	_STATE->parameters[SPECDEL2JOINENDS].category = specdel;
	_STATE->parameters[SPECDEL2JOINENDS].initvalue = 0;
	_STATE->parameters[SPECDEL2JOINENDS].name = "JOIN";
	_STATE->parameters[SPECDEL2JOINENDS].type = ParameterType_bool;


	_STATE->parameters[SPECDEL2RND].category = specdel;
	_STATE->parameters[SPECDEL2RND].initvalue = 0;
	_STATE->parameters[SPECDEL2RND].name = "FOLLOW";
	_STATE->parameters[SPECDEL2RND].type = ParameterType_bool;
	_STATE->parameters[SPECDEL2RND].flags |= Param::MidiParam;

	_STATE->parameters[SPECDEL2QUANT].category = specdel;
	_STATE->parameters[SPECDEL2QUANT].initvalue = 1;
	_STATE->parameters[SPECDEL2QUANT].name = "QUANT";

	_STATE->parameters[SPECDEL2CURVE].category = specdel;
	_STATE->parameters[SPECDEL2CURVE].initvalue = 0;
	_STATE->parameters[SPECDEL2CURVE].type = ParameterType_enum;
	_STATE->parameters[SPECDEL2CURVE].names = editorcurvenames;

	_STATE->parameters[SPECDEL2REDRAW].initvalue = 1.0;

	_STATE->parameters[SPECDEL2RECOMPUTE1].initvalue = 1;

	_STATE->parameters[SPECDEL2RECOMPUTE2].initvalue = 1;

	for (int32_t i = 0; i < 9; i++) {
		_STATE->parameters[SPECDEL2X0 + i].initvalue = i < 5 ? i * .25 : 1;
		_STATE->parameters[SPECDEL2X0 + i].category = specdel;
		_STATE->parameters[SPECDEL2X0 + i].type = _STATE->parameters[SPECDEL2Y0 + i].type = ParameterType_double;
		_STATE->parameters[SPECDEL2X0 + i].min = _STATE->parameters[SPECDEL2Y0 + i].min = 0;
		_STATE->parameters[SPECDEL2X0 + i].max = _STATE->parameters[SPECDEL2Y0 + i].max = 1.;
	}



	//_STATE->parameters[BANDLIMITEDGRAINENV].category = grainParams;
	_STATE->parameters[BANDLIMITEDGRAINENV].type = ParameterType_bool;
	_STATE->parameters[BANDLIMITEDGRAINENV].initvalue = 0.0;
	_STATE->parameters[BANDLIMITEDGRAINENV].name = "BANDLIMITED ENVELOPE";
	_STATE->parameters[BANDLIMITEDGRAINENV].flags |= Param::MidiParam;

	//_STATE->parameters[INTEGERENVCYCLES].category = grainParams;
	_STATE->parameters[INTEGERENVCYCLES].type = ParameterType_bool;
	_STATE->parameters[INTEGERENVCYCLES].initvalue = 1.0;
	_STATE->parameters[INTEGERENVCYCLES].name = "INTEGER ENVCYCLES";
	_STATE->parameters[INTEGERENVCYCLES].flags |= Param::MidiParam;


	_STATE->parameters[HQ_RESAMPLING].name = "HQ RESAMPLING";
	//_STATE->parameters[HQ_RESAMPLING].category = grainParams;
	_STATE->parameters[HQ_RESAMPLING].type = ParameterType_bool;
	_STATE->parameters[HQ_RESAMPLING].flags |= Param::MidiParam;

	_STATE->parameters[WSOLA].name = "WSOLA";
	_STATE->parameters[WSOLA].type = ParameterType_bool;
	_STATE->parameters[WSOLA].flags |= Param::MidiParam;

	//_STATE->parameters[NOINPUTFROMTRACK].category = grainParams;
	_STATE->parameters[NOINPUTFROMTRACK].type = ParameterType_bool;
	_STATE->parameters[NOINPUTFROMTRACK].initvalue = 0.0;
	_STATE->parameters[NOINPUTFROMTRACK].name = "ENVELOPE ONLY / NO INPUT";
	_STATE->parameters[NOINPUTFROMTRACK].flags |= Param::MidiParam;

	//_STATE->parameters[DOWSOLA].category = grainParams;
	_STATE->parameters[DOWSOLA].type = ParameterType_bool;
	_STATE->parameters[DOWSOLA].initvalue = 0.0;
	_STATE->parameters[DOWSOLA].name = "WSOLA";
	_STATE->parameters[DOWSOLA].flags |= Param::MidiParam;

	const char* oscbank = "OSCBANK";

	_STATE->parameters[PVAMPS2RANGE].category = pv;
	_STATE->parameters[PVAMPS2RANGE].subcategory = oscbank;
	_STATE->parameters[PVAMPS2RANGE].min = 1;
	_STATE->parameters[PVAMPS2RANGE].max = 100.;
	_STATE->parameters[PVAMPS2RANGE].name = "CHANNELS";
	_STATE->parameters[PVAMPS2RANGE].valuename = " ";
	_STATE->parameters[PVAMPS2RANGE].type = ParameterType_double;
	_STATE->parameters[PVAMPS2RANGE].digits = 2;
	_STATE->parameters[PVAMPS2RANGE].initvalue = 25;
	_STATE->parameters[PVAMPS2RANGE].progress = 1;
	_STATE->parameters[PVAMPS2RANGE].digits = 0;
	_STATE->parameters[PVAMPS2RANGE].flags |= Param::MidiParam;


	_STATE->parameters[PVAMPS2DRY].category = pv;
	_STATE->parameters[PVAMPS2DRY].subcategory = oscbank;
	_STATE->parameters[PVAMPS2DRY].min = -60;
	_STATE->parameters[PVAMPS2DRY].max = 60;
	_STATE->parameters[PVAMPS2DRY].name = "DRY";
	_STATE->parameters[PVAMPS2DRY].valuename = "dB";
	_STATE->parameters[PVAMPS2DRY].type = ParameterType_double;
	_STATE->parameters[PVAMPS2DRY].digits = 2;
	_STATE->parameters[PVAMPS2DRY].initvalue = -60;
	_STATE->parameters[PVAMPS2DRY].progress = 1;
	_STATE->parameters[PVAMPS2DRY].digits = 0;
	_STATE->parameters[PVAMPS2DRY].flags |= Param::MidiParam;

	_STATE->parameters[PVAMPS2WET].category = pv;
	_STATE->parameters[PVAMPS2WET].subcategory = oscbank;
	_STATE->parameters[PVAMPS2WET].min = -60.;
	_STATE->parameters[PVAMPS2WET].max = 60.;
	_STATE->parameters[PVAMPS2WET].name = "WET";
	_STATE->parameters[PVAMPS2WET].valuename = "dB";
	_STATE->parameters[PVAMPS2WET].type = ParameterType_double;
	_STATE->parameters[PVAMPS2WET].digits = 2;
	_STATE->parameters[PVAMPS2WET].initvalue = 0;
	_STATE->parameters[PVAMPS2WET].progress = 1;
	_STATE->parameters[PVAMPS2WET].digits = 0;
	_STATE->parameters[PVAMPS2WET].flags |= Param::MidiParam;


	_STATE->parameters[PVAMPS2PHASE].category = pv;
	_STATE->parameters[PVAMPS2PHASE].subcategory = oscbank;
	_STATE->parameters[PVAMPS2PHASE].initvalue = 0.0;
	_STATE->parameters[PVAMPS2PHASE].name = "PHASE";
	_STATE->parameters[PVAMPS2PHASE].type = ParameterType_enum;
	_STATE->parameters[PVAMPS2PHASE].names = oscnames;
	_STATE->parameters[PVAMPS2PHASE].flags |= Param::MidiParam;

	const char* bowed = "BOWED";

	_STATE->parameters[BOWEDCPS].category = bowed;
	_STATE->parameters[BOWEDCPS].min = LOG10D20F(27.5f);
	_STATE->parameters[BOWEDCPS].max = LOG10D20F(5000.);
	_STATE->parameters[BOWEDCPS].name = "CPS";
	_STATE->parameters[BOWEDCPS].valuename = "Hz";
	_STATE->parameters[BOWEDCPS].type = ParameterType_double;
	_STATE->parameters[BOWEDCPS].digits = 0;
	_STATE->parameters[BOWEDCPS].initvalue = LOG10D20F(160.);
	_STATE->parameters[BOWEDCPS].progress = 1;
	_STATE->parameters[BOWEDCPS].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[BOWEDCPS].flags |= Param::MidiParam;


	_STATE->parameters[BOWEDPOS].category = bowed;
	_STATE->parameters[BOWEDPOS].min = 0.;
	_STATE->parameters[BOWEDPOS].max = 1.;
	_STATE->parameters[BOWEDPOS].name = "POS";
	_STATE->parameters[BOWEDPOS].valuename = " ";
	_STATE->parameters[BOWEDPOS].type = ParameterType_double;
	_STATE->parameters[BOWEDPOS].digits = 2;
	_STATE->parameters[BOWEDPOS].initvalue = .5;
	_STATE->parameters[BOWEDPOS].progress = .05;
	_STATE->parameters[BOWEDPOS].flags |= Param::MidiParam;

	_STATE->parameters[BOWEDPRES].category = bowed;
	_STATE->parameters[BOWEDPRES].min = 0.;
	_STATE->parameters[BOWEDPRES].max = 1.;
	_STATE->parameters[BOWEDPRES].name = "PRESSURE";
	_STATE->parameters[BOWEDPRES].valuename = " ";
	_STATE->parameters[BOWEDPRES].type = ParameterType_double;
	_STATE->parameters[BOWEDPRES].digits = 2;
	_STATE->parameters[BOWEDPRES].initvalue = .5;
	_STATE->parameters[BOWEDPRES].progress = .05;
	_STATE->parameters[BOWEDPRES].flags |= Param::MidiParam;

	_STATE->parameters[BOWEDVIB].category = bowed;
	_STATE->parameters[BOWEDVIB].min = 0.;
	_STATE->parameters[BOWEDVIB].max = 12.;
	_STATE->parameters[BOWEDVIB].name = "VIBRATO";
	_STATE->parameters[BOWEDVIB].valuename = "Hz";
	_STATE->parameters[BOWEDVIB].type = ParameterType_double;
	_STATE->parameters[BOWEDVIB].digits = 2;
	_STATE->parameters[BOWEDVIB].initvalue = 6.12723;
	_STATE->parameters[BOWEDVIB].progress = .1;
	_STATE->parameters[BOWEDVIB].flags |= Param::MidiParam;

	_STATE->parameters[BOWEDVIBGAIN].category = bowed;
	_STATE->parameters[BOWEDVIBGAIN].min = 0.;
	_STATE->parameters[BOWEDVIBGAIN].max = 1.;
	_STATE->parameters[BOWEDVIBGAIN].name = "VIBAMP";
	_STATE->parameters[BOWEDVIBGAIN].valuename = " ";
	_STATE->parameters[BOWEDVIBGAIN].type = ParameterType_double;
	_STATE->parameters[BOWEDVIBGAIN].digits = 2;
	_STATE->parameters[BOWEDVIBGAIN].initvalue = 0;
	_STATE->parameters[BOWEDVIBGAIN].progress = .05;
	_STATE->parameters[BOWEDVIBGAIN].flags |= Param::MidiParam;

	_STATE->parameters[BOWEDDRY].category = bowed;
	_STATE->parameters[BOWEDDRY].min = -60;
	_STATE->parameters[BOWEDDRY].max = 60.;
	_STATE->parameters[BOWEDDRY].name = "DRY";
	_STATE->parameters[BOWEDDRY].valuename = "dB";
	_STATE->parameters[BOWEDDRY].type = ParameterType_double;
	_STATE->parameters[BOWEDDRY].initvalue = -3;
	_STATE->parameters[BOWEDDRY].progress = 1;
	_STATE->parameters[BOWEDDRY].flags |= Param::MidiParam;

	_STATE->parameters[BOWEDWET].category = bowed;
	_STATE->parameters[BOWEDWET].min = -60;
	_STATE->parameters[BOWEDWET].max = 60.;
	_STATE->parameters[BOWEDWET].name = "WET";
	_STATE->parameters[BOWEDWET].valuename = "dB";
	_STATE->parameters[BOWEDWET].type = ParameterType_double;
	_STATE->parameters[BOWEDWET].initvalue = -20;
	_STATE->parameters[BOWEDWET].progress = 1;
	_STATE->parameters[BOWEDWET].flags |= Param::MidiParam;

	_STATE->parameters[BOWEDFOLLOW].category = bowed;
	_STATE->parameters[BOWEDFOLLOW].name = "FOLLOW";
	_STATE->parameters[BOWEDFOLLOW].type = ParameterType_bool;
	_STATE->parameters[BOWEDFOLLOW].flags |= Param::MidiParam;


	_STATE->parameters[BOWEDHOLD].category = bowed;
	_STATE->parameters[BOWEDHOLD].name = "HOLD";
	_STATE->parameters[BOWEDHOLD].type = ParameterType_bool;
	_STATE->parameters[BOWEDHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	const char* pdetect = "PITCH DETECT";

	_STATE->parameters[PDETECTSMOOTH].category = pdetect;
	_STATE->parameters[PDETECTSMOOTH].min = LOG10D20F(0.001);
	_STATE->parameters[PDETECTSMOOTH].max = LOG10D20F(1.);
	_STATE->parameters[PDETECTSMOOTH].name = "SMOOTH";
	_STATE->parameters[PDETECTSMOOTH].valuename = " ";
	_STATE->parameters[PDETECTSMOOTH].type = ParameterType_double;
	_STATE->parameters[PDETECTSMOOTH].digits = 3;
	_STATE->parameters[PDETECTSMOOTH].initvalue = -30;
	_STATE->parameters[PDETECTSMOOTH].progress = 1;
	_STATE->parameters[PDETECTSMOOTH].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTSMOOTH].flags |= Param::MidiParam;

	// The one PitchDetect reads. See SMOOTH2POLE: 0..1 rising the way the name
	// reads, over the same span, with 0.5 exactly the old -30 dB default.
	_STATE->parameters[PDETECTSMOOTH2].category = pdetect;
	_STATE->parameters[PDETECTSMOOTH2].min = 0.0;
	_STATE->parameters[PDETECTSMOOTH2].max = 1.0;
	_STATE->parameters[PDETECTSMOOTH2].name = "SMOOTH";
	_STATE->parameters[PDETECTSMOOTH2].valuename = " ";
	_STATE->parameters[PDETECTSMOOTH2].type = ParameterType_double;
	_STATE->parameters[PDETECTSMOOTH2].digits = 2;
	_STATE->parameters[PDETECTSMOOTH2].initvalue = 0.5;
	_STATE->parameters[PDETECTSMOOTH2].progress = 0.01;
	_STATE->parameters[PDETECTSMOOTH2].flags |= Param::MidiParam;

	_STATE->parameters[PDETECTPRELP].category = pdetect;
	_STATE->parameters[PDETECTPRELP].min = LOG10D20F(20.);
	_STATE->parameters[PDETECTPRELP].max = LOG10D20F(20000.);
	_STATE->parameters[PDETECTPRELP].name = "PRELP";
	_STATE->parameters[PDETECTPRELP].valuename = "Hz";
	_STATE->parameters[PDETECTPRELP].type = ParameterType_double;
	_STATE->parameters[PDETECTPRELP].digits = 0;
	_STATE->parameters[PDETECTPRELP].initvalue = LOG10D20F(1000.);
	_STATE->parameters[PDETECTPRELP].progress = 1;
	_STATE->parameters[PDETECTPRELP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTPRELP].flags |= Param::MidiParam;

	_STATE->parameters[PITCHDETECTFXTRACK].category = pdetect;
	_STATE->parameters[PITCHDETECTFXTRACK].name = "SOURCE";
	_STATE->parameters[PITCHDETECTFXTRACK].flags |= (Param::NoAssignment | Param::MidiParam);
	_STATE->parameters[PITCHDETECTFXTRACK].type = ParameterType_enum;
	_STATE->parameters[PITCHDETECTFXTRACK].names = tracknames;


	_STATE->parameters[PITCHDETECTFXTRACKOUT0].name = "LAST PITCH OUT";
	_STATE->parameters[PITCHDETECTFXTRACKOUT0].initvalue = 0;

	_STATE->parameters[PITCHDETECTFXTRACKOUT1].name = "LAST PITCH OUT";
	_STATE->parameters[PITCHDETECTFXTRACKOUT1].initvalue = 0;

	_STATE->parameters[PITCHDETECTFXTRACKLASTPITCH0].name = "LAST PITCH";
	_STATE->parameters[PITCHDETECTFXTRACKLASTPITCH0].initvalue = 27.5;

	_STATE->parameters[PITCHDETECTFXTRACKLASTPITCH1].name = "LAST PITCH";
	_STATE->parameters[PITCHDETECTFXTRACKLASTPITCH1].initvalue = 27.5;

	_STATE->parameters[PDETECTTRANSPOSE].category = pdetect;
	_STATE->parameters[PDETECTTRANSPOSE].min = -36.;
	_STATE->parameters[PDETECTTRANSPOSE].max = 36.;
	_STATE->parameters[PDETECTTRANSPOSE].name = "TRANSPOSE";
	_STATE->parameters[PDETECTTRANSPOSE].valuename = "SEMITONES";
	_STATE->parameters[PDETECTTRANSPOSE].type = ParameterType_double;
	_STATE->parameters[PDETECTTRANSPOSE].initvalue = 0;
	_STATE->parameters[PDETECTTRANSPOSE].progress = 1;
	_STATE->parameters[PDETECTTRANSPOSE].digits = 1;
	_STATE->parameters[PDETECTTRANSPOSE].flags |= Param::MidiParam;

	_STATE->parameters[PDETECTA].category = pdetect;
	_STATE->parameters[PDETECTA].min = LOG10D20F(27.5);
	_STATE->parameters[PDETECTA].max = LOG10D20F(5000.);
	_STATE->parameters[PDETECTA].name = bounda;
	_STATE->parameters[PDETECTA].valuename = "Hz";
	_STATE->parameters[PDETECTA].type = ParameterType_double;
	_STATE->parameters[PDETECTA].digits = 0;
	_STATE->parameters[PDETECTA].initvalue = LOG10D20F(27.5);
	_STATE->parameters[PDETECTA].progress = 1;
	_STATE->parameters[PDETECTA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTA].flags |= Param::MidiParam;

	_STATE->parameters[PDETECTB].category = pdetect;
	_STATE->parameters[PDETECTB].min = LOG10D20F(27.5f);
	_STATE->parameters[PDETECTB].max = LOG10D20F(5000.);
	_STATE->parameters[PDETECTB].name = boundb;
	_STATE->parameters[PDETECTB].valuename = "Hz";
	_STATE->parameters[PDETECTB].type = ParameterType_double;
	_STATE->parameters[PDETECTB].digits = 0;
	_STATE->parameters[PDETECTB].initvalue = LOG10D20F(5000.);
	_STATE->parameters[PDETECTB].progress = 1;
	_STATE->parameters[PDETECTB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTB].flags |= Param::MidiParam;


	_STATE->parameters[PDETECTGRAINSMOOTH].category = grainParams;
	_STATE->parameters[PDETECTGRAINSMOOTH].subcategory = pdetect;
	_STATE->parameters[PDETECTGRAINSMOOTH].min = LOG10D20F(0.001);
	_STATE->parameters[PDETECTGRAINSMOOTH].max = LOG10D20F(1.);
	_STATE->parameters[PDETECTGRAINSMOOTH].name = "SMOOTH";
	_STATE->parameters[PDETECTGRAINSMOOTH].valuename = " ";
	_STATE->parameters[PDETECTGRAINSMOOTH].type = ParameterType_double;
	_STATE->parameters[PDETECTGRAINSMOOTH].digits = 3;
	_STATE->parameters[PDETECTGRAINSMOOTH].initvalue = -30;
	_STATE->parameters[PDETECTGRAINSMOOTH].progress = 1;
	_STATE->parameters[PDETECTGRAINSMOOTH].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTGRAINSMOOTH].flags |= Param::MidiParam;

	// The one PitchDetectGrain reads. See SMOOTH2POLE.
	_STATE->parameters[PDETECTGRAINSMOOTH2].category = grainParams;
	_STATE->parameters[PDETECTGRAINSMOOTH2].subcategory = pdetect;
	_STATE->parameters[PDETECTGRAINSMOOTH2].min = 0.0;
	_STATE->parameters[PDETECTGRAINSMOOTH2].max = 1.0;
	_STATE->parameters[PDETECTGRAINSMOOTH2].name = "SMOOTH";
	_STATE->parameters[PDETECTGRAINSMOOTH2].valuename = " ";
	_STATE->parameters[PDETECTGRAINSMOOTH2].type = ParameterType_double;
	_STATE->parameters[PDETECTGRAINSMOOTH2].digits = 2;
	_STATE->parameters[PDETECTGRAINSMOOTH2].initvalue = 0.5;
	_STATE->parameters[PDETECTGRAINSMOOTH2].progress = 0.01;
	_STATE->parameters[PDETECTGRAINSMOOTH2].flags |= Param::MidiParam;

	_STATE->parameters[PDETECTGRAINPRELP].category = grainParams;
	_STATE->parameters[PDETECTGRAINPRELP].subcategory = pdetect;
	_STATE->parameters[PDETECTGRAINPRELP].min = LOG10D20F(20.);
	_STATE->parameters[PDETECTGRAINPRELP].max = LOG10D20F(20000.);
	_STATE->parameters[PDETECTGRAINPRELP].name = "PRELP";
	_STATE->parameters[PDETECTGRAINPRELP].valuename = "Hz";
	_STATE->parameters[PDETECTGRAINPRELP].type = ParameterType_double;
	_STATE->parameters[PDETECTGRAINPRELP].digits = 0;
	_STATE->parameters[PDETECTGRAINPRELP].initvalue = LOG10D20F(1000.);
	_STATE->parameters[PDETECTGRAINPRELP].progress = 1;
	_STATE->parameters[PDETECTGRAINPRELP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTGRAINPRELP].flags |= Param::MidiParam;

	_STATE->parameters[PITCHDETECTGRAINFXTRACKOUT0].name = "LAST PITCH OUT";
	_STATE->parameters[PITCHDETECTGRAINFXTRACKOUT0].initvalue = 0;

	_STATE->parameters[PITCHDETECTGRAINFXTRACKOUT1].name = "LAST PITCH OUT";
	_STATE->parameters[PITCHDETECTGRAINFXTRACKOUT1].initvalue = 0;

	_STATE->parameters[PITCHDETECTGRAINFXTRACKLASTPITCH0].name = "LAST PITCH";
	_STATE->parameters[PITCHDETECTGRAINFXTRACKLASTPITCH0].initvalue = 27.5;

	_STATE->parameters[PITCHDETECTGRAINFXTRACKLASTPITCH1].name = "LAST PITCH";
	_STATE->parameters[PITCHDETECTGRAINFXTRACKLASTPITCH1].initvalue = 27.5;

	_STATE->parameters[PDETECTGRAINTRANSPOSE].category = grainParams;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].subcategory = pdetect;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].min = -36.;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].max = 36.;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].name = "TRANSPOSE";
	_STATE->parameters[PDETECTGRAINTRANSPOSE].valuename = "SEMITONES";
	_STATE->parameters[PDETECTGRAINTRANSPOSE].type = ParameterType_double;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].initvalue = 0;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].progress = 1;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].digits = 1;
	_STATE->parameters[PDETECTGRAINTRANSPOSE].flags |= Param::MidiParam;

	_STATE->parameters[PDETECTGRAINA].category = grainParams;
	_STATE->parameters[PDETECTGRAINA].subcategory = pdetect;
	_STATE->parameters[PDETECTGRAINA].min = LOG10D20F(27.5);
	_STATE->parameters[PDETECTGRAINA].max = LOG10D20F(5000.);
	_STATE->parameters[PDETECTGRAINA].name = bounda;
	_STATE->parameters[PDETECTGRAINA].valuename = "Hz";
	_STATE->parameters[PDETECTGRAINA].type = ParameterType_double;
	_STATE->parameters[PDETECTGRAINA].digits = 0;
	_STATE->parameters[PDETECTGRAINA].initvalue = LOG10D20F(27.5);
	_STATE->parameters[PDETECTGRAINA].progress = 1;
	_STATE->parameters[PDETECTGRAINA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTGRAINA].flags |= Param::MidiParam;

	_STATE->parameters[PDETECTGRAINB].category = grainParams;
	_STATE->parameters[PDETECTGRAINB].subcategory = pdetect;
	_STATE->parameters[PDETECTGRAINB].min = LOG10D20F(27.5f);
	_STATE->parameters[PDETECTGRAINB].max = LOG10D20F(5000.);
	_STATE->parameters[PDETECTGRAINB].name = boundb;
	_STATE->parameters[PDETECTGRAINB].valuename = "Hz";
	_STATE->parameters[PDETECTGRAINB].type = ParameterType_double;
	_STATE->parameters[PDETECTGRAINB].digits = 0;
	_STATE->parameters[PDETECTGRAINB].initvalue = LOG10D20F(5000.);
	_STATE->parameters[PDETECTGRAINB].progress = 1;
	_STATE->parameters[PDETECTGRAINB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PDETECTGRAINB].flags |= Param::MidiParam;

	const char* fm = "FM";

	_STATE->parameters[FMCPS].category = fm;
	_STATE->parameters[FMCPS].min = LOG10D20F(27.5f);
	_STATE->parameters[FMCPS].max = LOG10D20F(5000.);
	_STATE->parameters[FMCPS].name = "CPS";
	_STATE->parameters[FMCPS].valuename = "Hz";
	_STATE->parameters[FMCPS].type = ParameterType_double;
	_STATE->parameters[FMCPS].digits = 0;
	_STATE->parameters[FMCPS].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FMCPS].initvalue = LOG10D20F(160.);
	_STATE->parameters[FMCPS].progress = 1;
	_STATE->parameters[FMCPS].flags |= Param::MidiParam;

	_STATE->parameters[FMI].category = fm;
	_STATE->parameters[FMI].min = 0.;
	_STATE->parameters[FMI].max = 1.;
	_STATE->parameters[FMI].name = "INDEX";
	_STATE->parameters[FMI].valuename = " ";
	_STATE->parameters[FMI].type = ParameterType_double;
	_STATE->parameters[FMI].initvalue = 0;
	_STATE->parameters[FMI].progress = .01;
	_STATE->parameters[FMI].digits = 2;
	// _STATE->parameters[FMI].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FMI].flags |= Param::MidiParam;

	_STATE->parameters[FMR].category = fm;
	_STATE->parameters[FMR].min = 0;
	_STATE->parameters[FMR].max = 1;
	_STATE->parameters[FMR].name = "FB";
	_STATE->parameters[FMR].valuename = " ";
	_STATE->parameters[FMR].type = ParameterType_double;
	_STATE->parameters[FMR].initvalue = 0;
	_STATE->parameters[FMR].progress = 0.01;
	_STATE->parameters[FMR].digits = 2;
	_STATE->parameters[FMR].flags |= Param::MidiParam;

	//_STATE->parameters[FMR].paramCurve = Param::ParamCurve::Log10;


	_STATE->parameters[FMS].category = fm;
	_STATE->parameters[FMS].min = -1.;
	_STATE->parameters[FMS].max = 1.;
	_STATE->parameters[FMS].name = "S";
	_STATE->parameters[FMS].valuename = " ";
	_STATE->parameters[FMS].type = ParameterType_double;
	_STATE->parameters[FMS].initvalue = 1.0;
	_STATE->parameters[FMS].progress = .05;
	_STATE->parameters[FMS].digits = 2;
	// _STATE->parameters[FMR].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[FMRATIO].category = fm;
	_STATE->parameters[FMRATIO].min = LOG10D20F(0.2);
	_STATE->parameters[FMRATIO].max = LOG10D20F(5.);
	_STATE->parameters[FMRATIO].name = "M:C";
	_STATE->parameters[FMRATIO].valuename = " ";
	_STATE->parameters[FMRATIO].type = ParameterType_double;
	_STATE->parameters[FMRATIO].initvalue = LOG10D20F(1.);
	_STATE->parameters[FMRATIO].progress = 1;
	_STATE->parameters[FMRATIO].digits = 2;
	_STATE->parameters[FMRATIO].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FMRATIO].flags |= Param::MidiParam;

	_STATE->parameters[FMVIBRATEA].category = fm;
	_STATE->parameters[FMVIBRATEA].min = 0.;
	_STATE->parameters[FMVIBRATEA].max = 12.;
	_STATE->parameters[FMVIBRATEA].name = "VIBRT";
	_STATE->parameters[FMVIBRATEA].valuename = "Hz";
	_STATE->parameters[FMVIBRATEA].type = ParameterType_double;
	_STATE->parameters[FMVIBRATEA].initvalue = 7.;
	_STATE->parameters[FMVIBRATEA].progress = .5;
	_STATE->parameters[FMVIBRATEA].digits = 1;
	_STATE->parameters[FMVIBRATEA].flags |= Param::MidiParam;

	_STATE->parameters[FMVIBDEPTHA].category = fm;
	_STATE->parameters[FMVIBDEPTHA].min = 0.;
	_STATE->parameters[FMVIBDEPTHA].max = 1.;
	_STATE->parameters[FMVIBDEPTHA].name = "VIBAMT";
	_STATE->parameters[FMVIBDEPTHA].valuename = " ";
	_STATE->parameters[FMVIBDEPTHA].type = ParameterType_double;
	_STATE->parameters[FMVIBDEPTHA].initvalue = 0.;
	_STATE->parameters[FMVIBDEPTHA].progress = .01;
	_STATE->parameters[FMVIBDEPTHA].digits = 2;
	_STATE->parameters[FMVIBDEPTHA].flags |= Param::MidiParam;

	_STATE->parameters[FMVIBRATEB].category = fm;
	_STATE->parameters[FMVIBRATEB].min = 0.;
	_STATE->parameters[FMVIBRATEB].max = 12.;
	_STATE->parameters[FMVIBRATEB].name = "VIBRTB";
	_STATE->parameters[FMVIBRATEB].valuename = "Hz";
	_STATE->parameters[FMVIBRATEB].type = ParameterType_double;
	_STATE->parameters[FMVIBRATEB].initvalue = 3.;
	_STATE->parameters[FMVIBRATEB].progress = .5;
	_STATE->parameters[FMVIBRATEB].digits = 1;
	_STATE->parameters[FMVIBRATEB].flags |= Param::MidiParam;

	_STATE->parameters[FMVIBDEPTHB].category = fm;
	_STATE->parameters[FMVIBDEPTHB].min = 0.;
	_STATE->parameters[FMVIBDEPTHB].max = 1.;
	_STATE->parameters[FMVIBDEPTHB].name = "VIBAMTB";
	_STATE->parameters[FMVIBDEPTHB].valuename = " ";
	_STATE->parameters[FMVIBDEPTHB].type = ParameterType_double;
	_STATE->parameters[FMVIBDEPTHB].initvalue = 0.05;
	_STATE->parameters[FMVIBDEPTHB].progress = .01;
	_STATE->parameters[FMVIBDEPTHB].digits = 2;
	_STATE->parameters[FMVIBDEPTHB].flags |= Param::MidiParam;

	_STATE->parameters[FMDETUNE].category = fm;
	_STATE->parameters[FMDETUNE].min = 0.;
	_STATE->parameters[FMDETUNE].max = 1.;
	_STATE->parameters[FMDETUNE].name = "DETUNE";
	_STATE->parameters[FMDETUNE].valuename = " ";
	_STATE->parameters[FMDETUNE].type = ParameterType_double;
	_STATE->parameters[FMDETUNE].initvalue = 1.0;
	_STATE->parameters[FMDETUNE].progress = 0.01;
	_STATE->parameters[FMDETUNE].digits = 2;
	// _STATE->parameters[FMDETUNE].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FMDETUNE].flags |= Param::MidiParam;


	_STATE->parameters[FMDETLR].category = fm;
	_STATE->parameters[FMDETLR].min = 0.;
	_STATE->parameters[FMDETLR].max = 50.;
	_STATE->parameters[FMDETLR].name = "DETLR";
	_STATE->parameters[FMDETLR].valuename = "CENTS";
	_STATE->parameters[FMDETLR].type = ParameterType_double;
	_STATE->parameters[FMDETLR].initvalue = 0;
	_STATE->parameters[FMDETLR].progress = 1;
	_STATE->parameters[FMDETLR].digits = 2;
	// _STATE->parameters[FMDETLR].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[FMDETLR].flags |= Param::MidiParam;

	_STATE->parameters[FMDRY].category = fm;
	_STATE->parameters[FMDRY].min = -60;
	_STATE->parameters[FMDRY].max = 60.;
	_STATE->parameters[FMDRY].name = "DRY";
	_STATE->parameters[FMDRY].valuename = "dB";
	_STATE->parameters[FMDRY].type = ParameterType_double;
	_STATE->parameters[FMDRY].digits = 0;
	_STATE->parameters[FMDRY].initvalue = -6;
	_STATE->parameters[FMDRY].progress = 1;
	_STATE->parameters[FMDRY].flags |= Param::MidiParam;

	_STATE->parameters[FMWET].category = fm;
	_STATE->parameters[FMWET].min = -60;
	_STATE->parameters[FMWET].max = 60.;
	_STATE->parameters[FMWET].name = "WET";
	_STATE->parameters[FMWET].valuename = "dB";
	_STATE->parameters[FMWET].type = ParameterType_double;
	_STATE->parameters[FMWET].digits = 0;
	_STATE->parameters[FMWET].initvalue = -6;
	_STATE->parameters[FMWET].progress = 1;
	_STATE->parameters[FMWET].flags |= Param::MidiParam;

	_STATE->parameters[FMHOLD].category = fm;
	_STATE->parameters[FMHOLD].name = "HOLD";
	_STATE->parameters[FMHOLD].type = ParameterType_bool;
	_STATE->parameters[FMHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	_STATE->parameters[FMFOLLOW].category = fm;
	_STATE->parameters[FMFOLLOW].name = "FOLLOW";
	_STATE->parameters[FMFOLLOW].type = ParameterType_bool;
	_STATE->parameters[FMFOLLOW].flags |= Param::MidiParam;


	float semis[6] = { 0, 7, 12, 4, 14, 19 };
	for (int32_t i = 0; i < 6; i++) {
		_STATE->parameters[FMTYPE0 + i].category = fm;
		_STATE->parameters[FMTYPE0 + i].subcategory = fmunitnames[i].data();
		_STATE->parameters[FMTYPE0 + i].name = "INSTRUMENT";
		_STATE->parameters[FMTYPE0 + i].names = fminstnames;
		_STATE->parameters[FMTYPE0 + i].values = fminstvalues;
		_STATE->parameters[FMTYPE0 + i].type = ParameterType_enum;
		_STATE->parameters[FMTYPE0 + i].flags |= Param::MidiParam;

		_STATE->parameters[FMSEM0 + i].category = fm;
		_STATE->parameters[FMSEM0 + i].subcategory = fmunitnames[i].data();
		_STATE->parameters[FMSEM0 + i].min = 0;
		_STATE->parameters[FMSEM0 + i].max = +36;
		_STATE->parameters[FMSEM0 + i].name = "SEMIS";
		_STATE->parameters[FMSEM0 + i].valuename = " ";
		_STATE->parameters[FMSEM0 + i].type = ParameterType_double;
		_STATE->parameters[FMSEM0 + i].initvalue = semis[i];
		_STATE->parameters[FMSEM0 + i].progress = 1;
		_STATE->parameters[FMSEM0 + i].digits = 1;
		_STATE->parameters[FMSEM0 + i].flags |= Param::MidiParam;

		_STATE->parameters[FMGAIN0 + i].category = fm;
		_STATE->parameters[FMGAIN0 + i].subcategory = fmunitnames[i].data();
		_STATE->parameters[FMGAIN0 + i].min = -60;
		_STATE->parameters[FMGAIN0 + i].max = +60;
		_STATE->parameters[FMGAIN0 + i].name = "GAIN";
		_STATE->parameters[FMGAIN0 + i].valuename = "dB";
		_STATE->parameters[FMGAIN0 + i].type = ParameterType_double;
		_STATE->parameters[FMGAIN0 + i].initvalue = -20;
		_STATE->parameters[FMGAIN0 + i].progress = 1;
		_STATE->parameters[FMGAIN0 + i].digits = 0;
		_STATE->parameters[FMGAIN0 + i].flags |= Param::MidiParam;

		_STATE->parameters[FMPOW0 + i].category = fm;
		_STATE->parameters[FMPOW0 + i].subcategory = namePower;
		_STATE->parameters[FMPOW0 + i].name = fmunitnames[i].data();
		_STATE->parameters[FMPOW0 + i].type = ParameterType_bool;
		_STATE->parameters[FMPOW0 + i].flags |= Param::MidiParam;
	}

	_STATE->parameters[FMPOW0].initvalue = 1;

	const char* reverb5 = "REVER5";

	_STATE->parameters[REV5DAMP2].category = reverb5;
	_STATE->parameters[REV5DAMP2].min = LOG10D20F(1000.);
	_STATE->parameters[REV5DAMP2].max = LOG10D20F(_STATE->sr * .5f);
	_STATE->parameters[REV5DAMP2].name = "DAMPING";
	_STATE->parameters[REV5DAMP2].valuename = "Hz";
	_STATE->parameters[REV5DAMP2].type = ParameterType_double;
	_STATE->parameters[REV5DAMP2].digits = 0;
	_STATE->parameters[REV5DAMP2].initvalue = LOG10D20F(5000.);;
	_STATE->parameters[REV5DAMP2].progress = 1;
	_STATE->parameters[REV5DAMP2].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV5DAMP2].flags |= Param::MidiParam;


	_STATE->parameters[REV5T60LOW].category = reverb5;
	_STATE->parameters[REV5T60LOW].min = LOG10D20F(.5f);
	_STATE->parameters[REV5T60LOW].max = LOG10D20F(100.);
	_STATE->parameters[REV5T60LOW].name = "T60LOW";
	_STATE->parameters[REV5T60LOW].valuename = "s";
	_STATE->parameters[REV5T60LOW].type = ParameterType_double;
	_STATE->parameters[REV5T60LOW].digits = 1;
	_STATE->parameters[REV5T60LOW].initvalue = LOG10D20F(3.);;
	_STATE->parameters[REV5T60LOW].progress = 1;
	_STATE->parameters[REV5T60LOW].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV5T60LOW].flags |= Param::MidiParam;

	_STATE->parameters[REV5T60HI].category = reverb5;
	_STATE->parameters[REV5T60HI].min = LOG10D20F(.5f);
	_STATE->parameters[REV5T60HI].max = LOG10D20F(100.);
	_STATE->parameters[REV5T60HI].name = "T60MID";
	_STATE->parameters[REV5T60HI].valuename = "s";
	_STATE->parameters[REV5T60HI].type = ParameterType_double;
	_STATE->parameters[REV5T60HI].digits = 1;
	_STATE->parameters[REV5T60HI].initvalue = LOG10D20F(1.5f);
	_STATE->parameters[REV5T60HI].progress = 1;
	_STATE->parameters[REV5T60HI].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV5T60HI].flags |= Param::MidiParam;

	_STATE->parameters[REV5XOVER].category = reverb5;
	_STATE->parameters[REV5XOVER].min = LOG10D20F(50.);
	_STATE->parameters[REV5XOVER].max = LOG10D20F(1000.);
	_STATE->parameters[REV5XOVER].name = "CROSSOVER";
	_STATE->parameters[REV5XOVER].valuename = "Hz";
	_STATE->parameters[REV5XOVER].type = ParameterType_double;
	_STATE->parameters[REV5XOVER].digits = 0;
	_STATE->parameters[REV5XOVER].initvalue = LOG10D20F(200.);;
	_STATE->parameters[REV5XOVER].progress = 1;
	_STATE->parameters[REV5XOVER].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV5XOVER].flags |= Param::MidiParam;

	const char* reverb6 = "REVERB6";

	_STATE->parameters[REV7SHIFT].category = reverb6;
	_STATE->parameters[REV7SHIFT].min = -12;
	_STATE->parameters[REV7SHIFT].max = 12;
	_STATE->parameters[REV7SHIFT].name = "SHIFT";
	_STATE->parameters[REV7SHIFT].valuename = "SEMITONES";
	_STATE->parameters[REV7SHIFT].type = ParameterType_double;
	_STATE->parameters[REV7SHIFT].digits = 2;
	_STATE->parameters[REV7SHIFT].initvalue = 12;
	_STATE->parameters[REV7SHIFT].progress = 1;
	_STATE->parameters[REV7SHIFT].flags |= Param::MidiParam;

	_STATE->parameters[REV7SHIFTMODE].category = reverb6;
	_STATE->parameters[REV7SHIFTMODE].name = "SHIFT";
	_STATE->parameters[REV7SHIFTMODE].initvalue = 1;
	_STATE->parameters[REV7SHIFTMODE].type = ParameterType_enum;
	_STATE->parameters[REV7SHIFTMODE].names = shimmermodesnames;
	_STATE->parameters[REV7SHIFTMODE].flags |= Param::MidiParam;

	_STATE->parameters[REV7FB].category = reverb6;
	_STATE->parameters[REV7FB].min = 0.;
	_STATE->parameters[REV7FB].max = 1.;
	_STATE->parameters[REV7FB].name = "FB";
	_STATE->parameters[REV7FB].valuename = " ";
	_STATE->parameters[REV7FB].type = ParameterType_double;
	_STATE->parameters[REV7FB].digits = 2;
	_STATE->parameters[REV7FB].initvalue = 0.7f;
	_STATE->parameters[REV7FB].progress = .05;
	_STATE->parameters[REV7FB].flags |= Param::MidiParam;

	_STATE->parameters[REV7SIZE].category = reverb6;
	_STATE->parameters[REV7SIZE].min = 0.;
	_STATE->parameters[REV7SIZE].max = 1.;
	_STATE->parameters[REV7SIZE].name = "SIZE";
	_STATE->parameters[REV7SIZE].valuename = " ";
	_STATE->parameters[REV7SIZE].type = ParameterType_double;
	_STATE->parameters[REV7SIZE].digits = 2;
	_STATE->parameters[REV7SIZE].initvalue = 0.5;
	_STATE->parameters[REV7SIZE].progress = .05;
	_STATE->parameters[REV7SIZE].flags |= Param::MidiParam;

	_STATE->parameters[REV7DIFF].category = reverb6;
	_STATE->parameters[REV7DIFF].min = 0.;
	_STATE->parameters[REV7DIFF].max = 1.;
	_STATE->parameters[REV7DIFF].name = "DIFF";
	_STATE->parameters[REV7DIFF].valuename = " ";
	_STATE->parameters[REV7DIFF].type = ParameterType_double;
	_STATE->parameters[REV7DIFF].digits = 2;
	_STATE->parameters[REV7DIFF].initvalue = 0.61;
	_STATE->parameters[REV7DIFF].progress = .05;
	_STATE->parameters[REV7DIFF].flags |= Param::MidiParam;

	_STATE->parameters[REV7DEPTH].category = reverb6;
	_STATE->parameters[REV7DEPTH].min = 0.;
	_STATE->parameters[REV7DEPTH].max = 1.;
	_STATE->parameters[REV7DEPTH].name = "DEPTH";
	_STATE->parameters[REV7DEPTH].valuename = " ";
	_STATE->parameters[REV7DEPTH].type = ParameterType_double;
	_STATE->parameters[REV7DEPTH].digits = 2;
	_STATE->parameters[REV7DEPTH].initvalue = .5;
	_STATE->parameters[REV7DEPTH].progress = .05;
	_STATE->parameters[REV7DEPTH].flags |= Param::MidiParam;

	_STATE->parameters[REV7RATE].category = reverb6;
	_STATE->parameters[REV7RATE].min = 0;
	_STATE->parameters[REV7RATE].max = 1;
	_STATE->parameters[REV7RATE].name = "RATE";
	_STATE->parameters[REV7RATE].valuename = " ";
	_STATE->parameters[REV7RATE].type = ParameterType_double;
	_STATE->parameters[REV7RATE].digits = 2;
	_STATE->parameters[REV7RATE].initvalue = 1.;
	_STATE->parameters[REV7RATE].progress = 0.05;
	_STATE->parameters[REV7RATE].flags |= Param::MidiParam;

	_STATE->parameters[REV7GAIN].category = reverb6;
	_STATE->parameters[REV7GAIN].min = -60;
	_STATE->parameters[REV7GAIN].max = 60;
	_STATE->parameters[REV7GAIN].name = "GAIN";
	_STATE->parameters[REV7GAIN].valuename = "dB";
	_STATE->parameters[REV7GAIN].type = ParameterType_double;
	_STATE->parameters[REV7GAIN].digits = 0;
	_STATE->parameters[REV7GAIN].initvalue = LOG10D20F(1.0);
	_STATE->parameters[REV7GAIN].progress = 1;
	_STATE->parameters[REV7GAIN].flags |= Param::MidiParam;

	_STATE->parameters[REV7MIX].category = reverb6;
	_STATE->parameters[REV7MIX].min = 0.;
	_STATE->parameters[REV7MIX].max = 1.;
	_STATE->parameters[REV7MIX].name = "MIX";
	_STATE->parameters[REV7MIX].valuename = " ";
	_STATE->parameters[REV7MIX].type = ParameterType_double;
	_STATE->parameters[REV7MIX].digits = 2;
	_STATE->parameters[REV7MIX].initvalue = .5;
	_STATE->parameters[REV7MIX].progress = .05;
	_STATE->parameters[REV7MIX].flags |= Param::MidiParam;

	_STATE->parameters[REV7HPCUT].category = reverb6;
	_STATE->parameters[REV7HPCUT].min = LOG10D20F(18.);
	_STATE->parameters[REV7HPCUT].max = LOG10D20F(1000.);
	_STATE->parameters[REV7HPCUT].name = "HPCUT";
	_STATE->parameters[REV7HPCUT].valuename = "Hz";
	_STATE->parameters[REV7HPCUT].type = ParameterType_double;
	_STATE->parameters[REV7HPCUT].digits = 0;
	_STATE->parameters[REV7HPCUT].initvalue = LOG10D20F(18.);
	_STATE->parameters[REV7HPCUT].progress = 1;
	_STATE->parameters[REV7HPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV7HPCUT].flags |= Param::MidiParam;

	_STATE->parameters[REV7LPCUT].category = reverb6;
	_STATE->parameters[REV7LPCUT].min = LOG10D20F(1000.);
	_STATE->parameters[REV7LPCUT].max = LOG10D20F(_STATE->sr / 2 / 2.);
	_STATE->parameters[REV7LPCUT].name = "LPCUT";
	_STATE->parameters[REV7LPCUT].valuename = "Hz";
	_STATE->parameters[REV7LPCUT].type = ParameterType_double;
	_STATE->parameters[REV7LPCUT].digits = 0;
	_STATE->parameters[REV7LPCUT].initvalue = LOG10D20F(6000.);
	_STATE->parameters[REV7LPCUT].progress = 1;
	_STATE->parameters[REV7LPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV7LPCUT].flags |= Param::MidiParam;


	_STATE->parameters[SHIMMERDARKMODE].category = reverb6;
	_STATE->parameters[SHIMMERDARKMODE].name = "MODE";
	_STATE->parameters[SHIMMERDARKMODE].initvalue = 1;
	_STATE->parameters[SHIMMERDARKMODE].type = ParameterType_enum;
	_STATE->parameters[SHIMMERDARKMODE].names = shimmerdarkmodenames;
	_STATE->parameters[SHIMMERDARKMODE].flags |= Param::MidiParam;

	const char* reverb2 = "REVERB2";

	_STATE->parameters[REV2DAMP].category = reverb2;
	_STATE->parameters[REV2DAMP].min = 0;
	_STATE->parameters[REV2DAMP].max = 1;
	_STATE->parameters[REV2DAMP].name = "DAMPING";
	_STATE->parameters[REV2DAMP].valuename = " ";
	_STATE->parameters[REV2DAMP].type = ParameterType_double;
	_STATE->parameters[REV2DAMP].digits = 2;
	_STATE->parameters[REV2DAMP].initvalue = .5;
	_STATE->parameters[REV2DAMP].progress = .1;
	_STATE->parameters[REV2DAMP].flags |= Param::MidiParam;

	const char* reverb3 = "REVERB3";

	_STATE->parameters[REV3DAMP].category = reverb3;
	_STATE->parameters[REV3DAMP].min = 0;
	_STATE->parameters[REV3DAMP].max = 1;
	_STATE->parameters[REV3DAMP].name = "DAMPING";
	_STATE->parameters[REV3DAMP].valuename = " ";
	_STATE->parameters[REV3DAMP].type = ParameterType_double;
	_STATE->parameters[REV3DAMP].digits = 2;
	_STATE->parameters[REV3DAMP].initvalue = .5;
	_STATE->parameters[REV3DAMP].progress = .1;
	_STATE->parameters[REV3DAMP].flags |= Param::MidiParam;

	const char* reverb1 = "REVERB1";

	_STATE->parameters[REV1T60].category = reverb1;
	_STATE->parameters[REV1T60].min = LOG10D20F(.5f);
	_STATE->parameters[REV1T60].max = LOG10D20F(10.);
	_STATE->parameters[REV1T60].name = "T60";
	_STATE->parameters[REV1T60].valuename = "s";
	_STATE->parameters[REV1T60].type = ParameterType_double;
	_STATE->parameters[REV1T60].digits = 1;
	_STATE->parameters[REV1T60].initvalue = LOG10D20F(2.);;
	_STATE->parameters[REV1T60].progress = 1;
	_STATE->parameters[REV1T60].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV1T60].flags |= Param::MidiParam;

	_STATE->parameters[REV2T60].category = reverb2;
	_STATE->parameters[REV2T60].min = LOG10D20F(.5f);
	_STATE->parameters[REV2T60].max = LOG10D20F(10.);
	_STATE->parameters[REV2T60].name = "T60";
	_STATE->parameters[REV2T60].valuename = "s";
	_STATE->parameters[REV2T60].type = ParameterType_double;
	_STATE->parameters[REV2T60].digits = 1;
	_STATE->parameters[REV2T60].initvalue = LOG10D20F(2.);;
	_STATE->parameters[REV2T60].progress = 1;
	_STATE->parameters[REV2T60].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV2T60].flags |= Param::MidiParam;


	_STATE->parameters[REV4HPCUT].category = r4;
	_STATE->parameters[REV4HPCUT].min = LOG10D20F(18.);
	_STATE->parameters[REV4HPCUT].max = LOG10D20F(1000.);
	_STATE->parameters[REV4HPCUT].name = "HPCUT";
	_STATE->parameters[REV4HPCUT].valuename = "Hz";
	_STATE->parameters[REV4HPCUT].type = ParameterType_double;
	_STATE->parameters[REV4HPCUT].digits = 0;
	_STATE->parameters[REV4HPCUT].initvalue = LOG10D20F(18.);
	_STATE->parameters[REV4HPCUT].progress = 1;
	_STATE->parameters[REV4HPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV4HPCUT].flags |= Param::MidiParam;

	_STATE->parameters[REV4LPCUT].category = r4;
	_STATE->parameters[REV4LPCUT].min = LOG10D20F(1000.);
	_STATE->parameters[REV4LPCUT].max = LOG10D20F(12000.);
	_STATE->parameters[REV4LPCUT].name = "LPCUT";
	_STATE->parameters[REV4LPCUT].valuename = "Hz";
	_STATE->parameters[REV4LPCUT].type = ParameterType_double;
	_STATE->parameters[REV4LPCUT].digits = 0;
	_STATE->parameters[REV4LPCUT].initvalue = LOG10D20F(6000.);
	_STATE->parameters[REV4LPCUT].progress = 1;
	_STATE->parameters[REV4LPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV4LPCUT].flags |= Param::MidiParam;


	_STATE->parameters[CHORUS2RATE].category = chorus;
	_STATE->parameters[CHORUS2RATE].min = LOG10D20F(FLUID_CHORUS_DEFAULT_SPEED);
	_STATE->parameters[CHORUS2RATE].max = LOG10D20F(CHORUS_MAX_SPEED_HZ);
	_STATE->parameters[CHORUS2RATE].name = "RATE";
	_STATE->parameters[CHORUS2RATE].valuename = "Hz";
	_STATE->parameters[CHORUS2RATE].type = ParameterType_double;
	_STATE->parameters[CHORUS2RATE].digits = 2;
	_STATE->parameters[CHORUS2RATE].initvalue = LOG10D20F(1.0);
	_STATE->parameters[CHORUS2RATE].progress = 1;
	_STATE->parameters[CHORUS2RATE].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CHORUS2RATE].flags |= Param::MidiParam;

	_STATE->parameters[CHORUS2TAPS].category = chorus;
	_STATE->parameters[CHORUS2TAPS].min = 1;
	_STATE->parameters[CHORUS2TAPS].max = 10;
	_STATE->parameters[CHORUS2TAPS].name = "TAPS";
	_STATE->parameters[CHORUS2TAPS].valuename = " ";
	_STATE->parameters[CHORUS2TAPS].type = ParameterType_double;
	_STATE->parameters[CHORUS2TAPS].digits = 0;
	_STATE->parameters[CHORUS2TAPS].initvalue = 3;
	_STATE->parameters[CHORUS2TAPS].progress = 1;
	_STATE->parameters[CHORUS2TAPS].flags |= Param::MidiParam;
	_STATE->parameters[CHORUS2TAPS].flags |= Param::CastInt;

	_STATE->parameters[CHORUS2WIDTH].category = chorus;
	_STATE->parameters[CHORUS2WIDTH].min = 0;
	_STATE->parameters[CHORUS2WIDTH].max = 1;
	_STATE->parameters[CHORUS2WIDTH].name = "WIDTH";
	_STATE->parameters[CHORUS2WIDTH].valuename = " ";
	_STATE->parameters[CHORUS2WIDTH].type = ParameterType_double;
	_STATE->parameters[CHORUS2WIDTH].digits = 2;
	_STATE->parameters[CHORUS2WIDTH].initvalue = 1;
	_STATE->parameters[CHORUS2WIDTH].progress = 0.05;
	_STATE->parameters[CHORUS2WIDTH].flags |= Param::MidiParam;


	_STATE->parameters[CHORUSPHASE].category = chorus;
	_STATE->parameters[CHORUSPHASE].min = 0;
	_STATE->parameters[CHORUSPHASE].max = 1;
	_STATE->parameters[CHORUSPHASE].name = "PHASELR";
	_STATE->parameters[CHORUSPHASE].valuename = " ";
	_STATE->parameters[CHORUSPHASE].type = ParameterType_double;
	_STATE->parameters[CHORUSPHASE].digits = 2;
	_STATE->parameters[CHORUSPHASE].initvalue = 1;
	_STATE->parameters[CHORUSPHASE].progress = 0.05;
	_STATE->parameters[CHORUSPHASE].flags |= Param::MidiParam;


	_STATE->parameters[CHORUSDELAY].category = chorus;
	_STATE->parameters[CHORUSDELAY].min = 0;
	_STATE->parameters[CHORUSDELAY].max = 1000;
	_STATE->parameters[CHORUSDELAY].name = "DELAY";
	_STATE->parameters[CHORUSDELAY].valuename = "ms";
	_STATE->parameters[CHORUSDELAY].type = ParameterType_double;
	_STATE->parameters[CHORUSDELAY].digits = 0;
	_STATE->parameters[CHORUSDELAY].initvalue = 0;
	_STATE->parameters[CHORUSDELAY].progress = 1;
	_STATE->parameters[CHORUSDELAY].flags |= Param::MidiParam;

	_STATE->parameters[REV3PREDELAY].category = reverb3;
	_STATE->parameters[REV3PREDELAY].min = 0;
	_STATE->parameters[REV3PREDELAY].max = 1000;
	_STATE->parameters[REV3PREDELAY].name = "DELAY";
	_STATE->parameters[REV3PREDELAY].valuename = "ms";
	_STATE->parameters[REV3PREDELAY].type = ParameterType_double;
	_STATE->parameters[REV3PREDELAY].digits = 0;
	_STATE->parameters[REV3PREDELAY].initvalue = 0;
	_STATE->parameters[REV3PREDELAY].progress = 1;
	_STATE->parameters[REV3PREDELAY].flags |= Param::MidiParam;


	_STATE->parameters[REV3HPCUT].category = reverb3;
	_STATE->parameters[REV3HPCUT].min = LOG10D20F(18.);
	_STATE->parameters[REV3HPCUT].max = LOG10D20F(1000.);
	_STATE->parameters[REV3HPCUT].name = "HPCUT";
	_STATE->parameters[REV3HPCUT].valuename = "Hz";
	_STATE->parameters[REV3HPCUT].type = ParameterType_double;
	_STATE->parameters[REV3HPCUT].digits = 0;
	_STATE->parameters[REV3HPCUT].initvalue = LOG10D20F(18.);
	_STATE->parameters[REV3HPCUT].progress = 1;
	_STATE->parameters[REV3HPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV3HPCUT].flags |= Param::MidiParam;

	_STATE->parameters[REV3LPCUT].category = reverb3;
	_STATE->parameters[REV3LPCUT].min = LOG10D20F(1000.);
	_STATE->parameters[REV3LPCUT].max = LOG10D20F(12000.);
	_STATE->parameters[REV3LPCUT].name = "LPCUT";
	_STATE->parameters[REV3LPCUT].valuename = "Hz";
	_STATE->parameters[REV3LPCUT].type = ParameterType_double;
	_STATE->parameters[REV3LPCUT].digits = 0;
	_STATE->parameters[REV3LPCUT].initvalue = LOG10D20F(6000.);
	_STATE->parameters[REV3LPCUT].progress = 1;
	_STATE->parameters[REV3LPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV3LPCUT].flags |= Param::MidiParam;


	_STATE->parameters[REV4PREDELAY].category = r4;
	_STATE->parameters[REV4PREDELAY].min = 0;
	_STATE->parameters[REV4PREDELAY].max = 1000;
	_STATE->parameters[REV4PREDELAY].name = "DELAY";
	_STATE->parameters[REV4PREDELAY].valuename = "ms";
	_STATE->parameters[REV4PREDELAY].type = ParameterType_double;
	_STATE->parameters[REV4PREDELAY].digits = 0;
	_STATE->parameters[REV4PREDELAY].initvalue = 0;
	_STATE->parameters[REV4PREDELAY].progress = 1;
	_STATE->parameters[REV4PREDELAY].flags |= Param::MidiParam;

	_STATE->parameters[REV5PREDELAY].category = reverb5;
	_STATE->parameters[REV5PREDELAY].min = 0;
	_STATE->parameters[REV5PREDELAY].max = 1000;
	_STATE->parameters[REV5PREDELAY].name = "DELAY";
	_STATE->parameters[REV5PREDELAY].valuename = "ms";
	_STATE->parameters[REV5PREDELAY].type = ParameterType_double;
	_STATE->parameters[REV5PREDELAY].digits = 0;
	_STATE->parameters[REV5PREDELAY].initvalue = 0;
	_STATE->parameters[REV5PREDELAY].progress = 1;
	_STATE->parameters[REV5PREDELAY].flags |= Param::MidiParam;



	_STATE->parameters[LOOPERFADE].category = "TAPE MODE";
	_STATE->parameters[LOOPERFADE].min = 0;
	_STATE->parameters[LOOPERFADE].max = 2000;
	_STATE->parameters[LOOPERFADE].name = "FADE";
	_STATE->parameters[LOOPERFADE].valuename = "ms";
	_STATE->parameters[LOOPERFADE].type = ParameterType_double;
	_STATE->parameters[LOOPERFADE].digits = 1;
	_STATE->parameters[LOOPERFADE].initvalue = 0;
	_STATE->parameters[LOOPERFADE].progress = 10;
	//_STATE->parameters[LOOPERFADE].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[LOOPERFADE].flags |= Param::MidiParam;

	_STATE->parameters[MIDICLOCKMULTI].name = "MIDI x";
	_STATE->parameters[STC_CURRENTGAIN].initvalue = 1.0;
	_STATE->parameters[MONOCOMPGAIN0].initvalue = 1.0;
	_STATE->parameters[MONOCOMPGAIN1].initvalue = 1.0;


	_STATE->parameters[ASFX].initvalue = SPACE_SSB;
	_STATE->parameters[ASFX].type = ParameterType_enum;
	_STATE->parameters[ASFX].name = "MONO EFFECTS";
	//_STATE->parameters[ASFX].flags |= Param::MidiParam;

	_STATE->parameters[ASSTFX].initvalue = SPACE_CHORUS;
	_STATE->parameters[ASSTFX].type = ParameterType_enum;
	_STATE->parameters[ASSTFX].name = "STEREO EFFECTS";
	//_STATE->parameters[ASSTFX].flags |= Param::MidiParam;

	_STATE->parameters[FFT_SIZE].initvalue = 1024;
	_STATE->parameters[FFT_SIZE].name = "FFT SIZE";
	_STATE->parameters[FFT_SIZE].type = ParameterType_enum;
	_STATE->parameters[FFT_SIZE].flags |= Param::MidiParam;

	const char* polar = "POLAR FORM";

	_STATE->parameters[CROSS_MAG].category = cross;
	_STATE->parameters[CROSS_MAG].subcategory = polar;
	_STATE->parameters[CROSS_MAG].name = "MAG";
	_STATE->parameters[CROSS_MAG].type = ParameterType_enum;
	_STATE->parameters[CROSS_MAG].names = cross_mag_types;
	_STATE->parameters[CROSS_MAG].flags |= Param::MidiParam;

	_STATE->parameters[CROSS_PHASE].category = cross;
	_STATE->parameters[CROSS_PHASE].subcategory = polar;
	_STATE->parameters[CROSS_PHASE].name = "PHASE";
	_STATE->parameters[CROSS_PHASE].initvalue = 1;
	_STATE->parameters[CROSS_PHASE].type = ParameterType_enum;
	_STATE->parameters[CROSS_PHASE].names = cross_mag_types;
	_STATE->parameters[CROSS_PHASE].flags |= Param::MidiParam;

	_STATE->parameters[ASCROSS].category = cross;
	_STATE->parameters[ASCROSS].name = "ALGORITHM";
	_STATE->parameters[ASCROSS].type = ParameterType_enum;
	_STATE->parameters[ASCROSS].names = cross_types;
	//_STATE->parameters[ASCROSS].flags |= Param::MidiParam;

	_STATE->parameters[ASPV].category = pv;
	_STATE->parameters[ASPV].name = "ALGORITHM";
	_STATE->parameters[ASPV].type = ParameterType_enum;
	_STATE->parameters[ASPV].names = pv_types;
	//_STATE->parameters[ASPV].flags |= Param::MidiParam;

	// ── CROSS: TRANSPORT ────────────────────────────────────────────────────
	const char* transport = "TRANSPORT";

	_STATE->parameters[CROSSTRANSAMT].category = cross;
	_STATE->parameters[CROSSTRANSAMT].subcategory = transport;
	_STATE->parameters[CROSSTRANSAMT].min = 0;
	_STATE->parameters[CROSSTRANSAMT].max = 1;
	_STATE->parameters[CROSSTRANSAMT].name = "AMOUNT";
	_STATE->parameters[CROSSTRANSAMT].valuename = " ";
	_STATE->parameters[CROSSTRANSAMT].type = ParameterType_double;
	_STATE->parameters[CROSSTRANSAMT].digits = 2;
	_STATE->parameters[CROSSTRANSAMT].initvalue = 1.;
	_STATE->parameters[CROSSTRANSAMT].progress = .01;
	_STATE->parameters[CROSSTRANSAMT].flags |= Param::MidiParam;

	// Mapped to a one-pole coefficient by 1-(1-x)^3 in cross_transport, so the
	// knob is linear in character rather than in the coefficient (which spends
	// its whole useful range above 0.9).
	_STATE->parameters[CROSSTRANSSMOOTH] = _STATE->parameters[CROSSTRANSAMT];
	_STATE->parameters[CROSSTRANSSMOOTH].name = "SMOOTH";
	_STATE->parameters[CROSSTRANSSMOOTH].initvalue = .35;

	// Mapped to 0.5*x^3 of the modulator's peak: the useful floors are all tiny
	// fractions, and a linear knob would put them in the first hundredth.
	_STATE->parameters[CROSSTRANSFLOOR] = _STATE->parameters[CROSSTRANSAMT];
	_STATE->parameters[CROSSTRANSFLOOR].name = "FLOOR";
	_STATE->parameters[CROSSTRANSFLOOR].initvalue = .15;

	// ── CROSS: PARTIAL STACK ────────────────────────────────────────────────
	const char* stack = "PARTIAL STACK";

	_STATE->parameters[CROSSSTACKN].category = cross;
	_STATE->parameters[CROSSSTACKN].subcategory = stack;
	_STATE->parameters[CROSSSTACKN].min = 1;
	_STATE->parameters[CROSSSTACKN].max = CROSS_STACK_MAX_PEAKS;
	_STATE->parameters[CROSSSTACKN].name = "PEAKS";
	_STATE->parameters[CROSSSTACKN].valuename = " ";
	_STATE->parameters[CROSSSTACKN].type = ParameterType_double;
	_STATE->parameters[CROSSSTACKN].digits = 0;
	_STATE->parameters[CROSSSTACKN].initvalue = 6.;
	_STATE->parameters[CROSSSTACKN].progress = 1.;
	_STATE->parameters[CROSSSTACKN].flags |= Param::MidiParam | Param::CastInt;

	// Exponent 0..2 on each peak's magnitude as a mixing weight; 0.5 (exponent 1)
	// is straight proportional mixing.
	_STATE->parameters[CROSSSTACKTILT].category = cross;
	_STATE->parameters[CROSSSTACKTILT].subcategory = stack;
	_STATE->parameters[CROSSSTACKTILT].min = 0;
	_STATE->parameters[CROSSSTACKTILT].max = 1;
	_STATE->parameters[CROSSSTACKTILT].name = "TILT";
	_STATE->parameters[CROSSSTACKTILT].valuename = " ";
	_STATE->parameters[CROSSSTACKTILT].type = ParameterType_double;
	_STATE->parameters[CROSSSTACKTILT].digits = 2;
	_STATE->parameters[CROSSSTACKTILT].initvalue = .5;
	_STATE->parameters[CROSSSTACKTILT].progress = .01;
	_STATE->parameters[CROSSSTACKTILT].flags |= Param::MidiParam;

	_STATE->parameters[CROSSSTACKFORM].category = cross;
	_STATE->parameters[CROSSSTACKFORM].subcategory = stack;
	_STATE->parameters[CROSSSTACKFORM].name = "KEEP FORMANTS";
	_STATE->parameters[CROSSSTACKFORM].type = ParameterType_bool;
	_STATE->parameters[CROSSSTACKFORM].initvalue = 1.;
	_STATE->parameters[CROSSSTACKFORM].setFlag(Param::OwnInitValue, true);
	_STATE->parameters[CROSSSTACKFORM].flags |= Param::MidiParam;

	// ── PV: HARM/PERC ───────────────────────────────────────────────────────
	const char* hp = "HARM/PERC";

	_STATE->parameters[PVHPSSMIX].category = pv;
	_STATE->parameters[PVHPSSMIX].subcategory = hp;
	_STATE->parameters[PVHPSSMIX].min = 0;
	_STATE->parameters[PVHPSSMIX].max = 1;
	_STATE->parameters[PVHPSSMIX].name = "PERC/HARM";
	_STATE->parameters[PVHPSSMIX].valuename = " ";
	_STATE->parameters[PVHPSSMIX].type = ParameterType_double;
	_STATE->parameters[PVHPSSMIX].digits = 2;
	_STATE->parameters[PVHPSSMIX].initvalue = 1.;
	_STATE->parameters[PVHPSSMIX].progress = .01;
	_STATE->parameters[PVHPSSMIX].flags |= Param::MidiParam;

	// Mapped to an odd bin count 3..PV_HPSS_MAX_WIDTH.
	_STATE->parameters[PVHPSSWIDTH] = _STATE->parameters[PVHPSSMIX];
	_STATE->parameters[PVHPSSWIDTH].name = "WIDTH";
	_STATE->parameters[PVHPSSWIDTH].initvalue = .5;

	_STATE->parameters[PVHPSSMASK].category = pv;
	_STATE->parameters[PVHPSSMASK].subcategory = hp;
	_STATE->parameters[PVHPSSMASK].name = "MASK";
	_STATE->parameters[PVHPSSMASK].type = ParameterType_enum;
	_STATE->parameters[PVHPSSMASK].names = pv_hpss_mask_types;
	_STATE->parameters[PVHPSSMASK].initvalue = 1.;
	_STATE->parameters[PVHPSSMASK].setFlag(Param::OwnInitValue, true);
	_STATE->parameters[PVHPSSMASK].flags |= Param::MidiParam;

	// ── PV: CONTRAST ────────────────────────────────────────────────────────
	const char* contrast = "CONTRAST";

	// Mapped to an exponent 2^((x-0.5)*4), i.e. 0.25 .. 4 with unity at the
	// centre detent, so the knob is symmetric around no-op.
	_STATE->parameters[PVCONTRAST].category = pv;
	_STATE->parameters[PVCONTRAST].subcategory = contrast;
	_STATE->parameters[PVCONTRAST].min = 0;
	_STATE->parameters[PVCONTRAST].max = 1;
	_STATE->parameters[PVCONTRAST].name = "CONTRAST";
	_STATE->parameters[PVCONTRAST].valuename = " ";
	_STATE->parameters[PVCONTRAST].type = ParameterType_double;
	_STATE->parameters[PVCONTRAST].digits = 2;
	_STATE->parameters[PVCONTRAST].initvalue = .5;
	_STATE->parameters[PVCONTRAST].progress = .01;
	_STATE->parameters[PVCONTRAST].flags |= Param::MidiParam;

	// Mapped to 1..48 geometric bands whose energy is held across the exponent.
	_STATE->parameters[PVCONTRASTBANDS] = _STATE->parameters[PVCONTRAST];
	_STATE->parameters[PVCONTRASTBANDS].name = "BANDS";
	_STATE->parameters[PVCONTRASTBANDS].initvalue = .32;

	// Mapped to -120..-12 dB below the frame peak.
	_STATE->parameters[PVCONTRASTFLOOR] = _STATE->parameters[PVCONTRAST];
	_STATE->parameters[PVCONTRASTFLOOR].name = "FLOOR";
	_STATE->parameters[PVCONTRASTFLOOR].initvalue = .2;

	// ── PV: SPECTRAL SNAP ───────────────────────────────────────────────────
	const char* snap = "SPECTRAL SNAP";

	_STATE->parameters[PVSNAPAMT].category = pv;
	_STATE->parameters[PVSNAPAMT].subcategory = snap;
	_STATE->parameters[PVSNAPAMT].min = 0;
	_STATE->parameters[PVSNAPAMT].max = 1;
	_STATE->parameters[PVSNAPAMT].name = "AMOUNT";
	_STATE->parameters[PVSNAPAMT].valuename = " ";
	_STATE->parameters[PVSNAPAMT].type = ParameterType_double;
	_STATE->parameters[PVSNAPAMT].digits = 2;
	_STATE->parameters[PVSNAPAMT].initvalue = 1.;
	_STATE->parameters[PVSNAPAMT].progress = .01;
	_STATE->parameters[PVSNAPAMT].flags |= Param::MidiParam;

	_STATE->parameters[PVSNAPMODE].category = pv;
	_STATE->parameters[PVSNAPMODE].subcategory = snap;
	_STATE->parameters[PVSNAPMODE].name = "GRID";
	_STATE->parameters[PVSNAPMODE].type = ParameterType_enum;
	_STATE->parameters[PVSNAPMODE].names = pv_snap_modes;
	_STATE->parameters[PVSNAPMODE].flags |= Param::MidiParam;

	// Kept in Hz rather than normalised: this one names a pitch, and a 0..1
	// readout for the root of a harmonic series is unusable. Same dB-log storage
	// and Log10 curve as VOC2HP, so the display is Hz.
	_STATE->parameters[PVSNAPROOT].category = pv;
	_STATE->parameters[PVSNAPROOT].subcategory = snap;
	_STATE->parameters[PVSNAPROOT].min = LOG10D20F(20.);
	_STATE->parameters[PVSNAPROOT].max = LOG10D20F(2000.);
	_STATE->parameters[PVSNAPROOT].name = "ROOT";
	_STATE->parameters[PVSNAPROOT].valuename = "Hz";
	_STATE->parameters[PVSNAPROOT].type = ParameterType_double;
	_STATE->parameters[PVSNAPROOT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PVSNAPROOT].digits = 1;
	_STATE->parameters[PVSNAPROOT].initvalue = LOG10D20F(110.);
	_STATE->parameters[PVSNAPROOT].setFlag(Param::OwnInitValue, true);
	_STATE->parameters[PVSNAPROOT].progress = 1.;
	_STATE->parameters[PVSNAPROOT].flags |= Param::MidiParam;

	// ── PV: SPECTRAL RES ─────────────────────────────────
	const char* sres = "SPECTRAL RES";

	// Names a pitch, so it keeps the dB-log storage and Log10 curve PVSNAPROOT
	// uses rather than a 0..1 readout. The LFO interpolates in that stored domain,
	// which is what makes a swept root move the bank in pitch rather than in hertz.
	_STATE->parameters[PVRESROOT].category = pv;
	_STATE->parameters[PVRESROOT].subcategory = sres;
	_STATE->parameters[PVRESROOT].min = LOG10D20F(20.);
	_STATE->parameters[PVRESROOT].max = LOG10D20F(2000.);
	_STATE->parameters[PVRESROOT].name = "ROOT";
	_STATE->parameters[PVRESROOT].valuename = "Hz";
	_STATE->parameters[PVRESROOT].type = ParameterType_double;
	_STATE->parameters[PVRESROOT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PVRESROOT].digits = 1;
	_STATE->parameters[PVRESROOT].initvalue = LOG10D20F(110.);
	_STATE->parameters[PVRESROOT].setFlag(Param::OwnInitValue, true);
	_STATE->parameters[PVRESROOT].progress = 1.;
	_STATE->parameters[PVRESROOT].flags |= Param::MidiParam;

	// Mapped to a T60 of 20 ms .. 20 s. The hop comes from DENSITY, so this reads
	// in real seconds whatever FFT SIZE is set to.
	_STATE->parameters[PVRESDECAY].category = pv;
	_STATE->parameters[PVRESDECAY].subcategory = sres;
	_STATE->parameters[PVRESDECAY].min = 0;
	_STATE->parameters[PVRESDECAY].max = 1;
	_STATE->parameters[PVRESDECAY].name = "DECAY";
	_STATE->parameters[PVRESDECAY].valuename = " ";
	_STATE->parameters[PVRESDECAY].type = ParameterType_double;
	_STATE->parameters[PVRESDECAY].digits = 2;
	_STATE->parameters[PVRESDECAY].initvalue = .5;
	_STATE->parameters[PVRESDECAY].progress = .01;
	_STATE->parameters[PVRESDECAY].flags |= Param::MidiParam;

	// High partials decay as r^-DAMP, exponent mapped to 0..2. At 0 the bank is a
	// row of identical filters; turned up the tail darkens the way a struck object
	// does, which is most of what separates the two.
	_STATE->parameters[PVRESDAMP] = _STATE->parameters[PVRESDECAY];
	_STATE->parameters[PVRESDAMP].name = "DAMP";
	_STATE->parameters[PVRESDAMP].initvalue = .35;

	// Stiff-string stretch f_n = n*root*sqrt(1 + B n^2), B mapped to 0..0.004 as
	// 0.004*x^3: string at the bottom, piano just above it, bell and gong at the top.
	_STATE->parameters[PVRESINHARM] = _STATE->parameters[PVRESDECAY];
	_STATE->parameters[PVRESINHARM].name = "INHARM";
	_STATE->parameters[PVRESINHARM].initvalue = 0.;

	// Dry side is the grain as it arrived, so 0 is a bypass and 1 is the bank alone.
	_STATE->parameters[PVRESMIX] = _STATE->parameters[PVRESDECAY];
	_STATE->parameters[PVRESMIX].name = "MIX";
	_STATE->parameters[PVRESMIX].initvalue = 1.;

	_STATE->parameters[PVRESMODE].category = pv;
	_STATE->parameters[PVRESMODE].subcategory = sres;
	_STATE->parameters[PVRESMODE].name = "SERIES";
	_STATE->parameters[PVRESMODE].type = ParameterType_enum;
	_STATE->parameters[PVRESMODE].names = pv_res_modes;
	_STATE->parameters[PVRESMODE].flags |= Param::MidiParam;

	// PH CORRECTION III (dephase_tracked) is parked: its PVPH3* ids live past
	// NUM_PARAMS, so initialising them while parked would write out of bounds.
	// Compiled out rather than deleted so re-enabling is moving the enum ids
	// back before NUM_PARAMS and dropping the #if.
#if 0
	const char* sph3 = "PH CORRECTION III";

	// 0 is plain per-bin propagation (what PHASE CORRECTION does on its own),
	// 1 rotates each peak's whole region rigidly. Blended along the shorter arc,
	// so the middle is a real interpolation and not a crossfade of two signals.
	_STATE->parameters[PVPH3LOCK].category = pv;
	_STATE->parameters[PVPH3LOCK].subcategory = sph3;
	_STATE->parameters[PVPH3LOCK].min = 0;
	_STATE->parameters[PVPH3LOCK].max = 1;
	_STATE->parameters[PVPH3LOCK].name = "LOCK";
	_STATE->parameters[PVPH3LOCK].valuename = " ";
	_STATE->parameters[PVPH3LOCK].type = ParameterType_double;
	_STATE->parameters[PVPH3LOCK].digits = 2;
	_STATE->parameters[PVPH3LOCK].initvalue = 1.;
	_STATE->parameters[PVPH3LOCK].progress = .01;
	_STATE->parameters[PVPH3LOCK].flags |= Param::MidiParam;

	// Onset sensitivity. At 0 the detector is off and the algorithm is a
	// peak-tracked vocoder; turned up, a transient resets every phase to the
	// measured one AND pauses the stretch until the attack has cleared the
	// window, which is what stops attacks smearing into pre/post-echo. Too high
	// and steady material resets constantly, goes grainy, and the stretch
	// stalls -- that is the trade to hear.
	_STATE->parameters[PVPH3TRANS] = _STATE->parameters[PVPH3LOCK];
	_STATE->parameters[PVPH3TRANS].name = "TRANS";
	_STATE->parameters[PVPH3TRANS].initvalue = .5;

	// How far below the frame maximum a mainlobe still counts as a partial, so
	// how much of the spectrum gets locked rather than left per-bin.
	_STATE->parameters[PVPH3PEAKS] = _STATE->parameters[PVPH3LOCK];
	_STATE->parameters[PVPH3PEAKS].name = "PEAKS";
	_STATE->parameters[PVPH3PEAKS].initvalue = .6;
#endif

	// CEPSTRUM II whiten/impose strength: 0 passes the carrier untouched, 1 is
	// the exact cross-filter, .5 matches the legacy modulator strength (see
	// docepstrum2).
	_STATE->parameters[CROSSCEP2DEPTH].category = cross;
	_STATE->parameters[CROSSCEP2DEPTH].subcategory = cepw;
	_STATE->parameters[CROSSCEP2DEPTH].min = 0;
	_STATE->parameters[CROSSCEP2DEPTH].max = 1;
	_STATE->parameters[CROSSCEP2DEPTH].name = "DEPTH";
	_STATE->parameters[CROSSCEP2DEPTH].valuename = " ";
	_STATE->parameters[CROSSCEP2DEPTH].type = ParameterType_double;
	_STATE->parameters[CROSSCEP2DEPTH].digits = 2;
	_STATE->parameters[CROSSCEP2DEPTH].initvalue = .5;
	_STATE->parameters[CROSSCEP2DEPTH].progress = .01;
	_STATE->parameters[CROSSCEP2DEPTH].flags |= Param::MidiParam;

	// ── PV: SPECTRAL FREEZE ─────────────────────────────────────────────────
	const char* sfreeze = "SPECTRAL FREEZE";

	// 0 tracks the input (transparent at speed 1), 1 holds the captured
	// spectrum; in between the held spectrum lags the input -- the slew is the
	// interpolation, see spectral_freeze(). The default is deliberately NOT 1:
	// selecting the algorithm at full freeze captures whatever happens to be
	// there -- silence included -- and then holds it, which reads as the
	// algorithm being broken. At .5 it always follows the input.
	_STATE->parameters[PVFREEZEAMT].category = pv;
	_STATE->parameters[PVFREEZEAMT].subcategory = sfreeze;
	_STATE->parameters[PVFREEZEAMT].min = 0;
	_STATE->parameters[PVFREEZEAMT].max = 1;
	_STATE->parameters[PVFREEZEAMT].name = "FREEZE";
	_STATE->parameters[PVFREEZEAMT].valuename = " ";
	_STATE->parameters[PVFREEZEAMT].type = ParameterType_double;
	_STATE->parameters[PVFREEZEAMT].digits = 2;
	_STATE->parameters[PVFREEZEAMT].initvalue = .5;
	_STATE->parameters[PVFREEZEAMT].progress = .01;
	_STATE->parameters[PVFREEZEAMT].flags |= Param::MidiParam;

	// Fraction of bins frozen, decided by a stable per-bin hash: raising it
	// freezes more bins, lowering it releases the same ones.
	_STATE->parameters[PVFREEZEPROB] = _STATE->parameters[PVFREEZEAMT];
	_STATE->parameters[PVFREEZEPROB].name = "BINS";
	_STATE->parameters[PVFREEZEPROB].initvalue = 1.;

	// Complex blend with the dry grain; 0 is a bypass, 1 the frozen resynthesis
	// alone.
	_STATE->parameters[PVFREEZEMIX] = _STATE->parameters[PVFREEZEAMT];
	_STATE->parameters[PVFREEZEMIX].name = "MIX";
	_STATE->parameters[PVFREEZEMIX].initvalue = 1.;

	// ── CROSS: SPECTRAL DUCK ────────────────────────────────────────────────
	const char* sduck = "SPECTRAL DUCK";

	// Mapped to -60..+20 dB relative to the modulator frame's RMS magnitude.
	_STATE->parameters[CROSSDUCKTHRESH].category = cross;
	_STATE->parameters[CROSSDUCKTHRESH].subcategory = sduck;
	_STATE->parameters[CROSSDUCKTHRESH].min = 0;
	_STATE->parameters[CROSSDUCKTHRESH].max = 1;
	_STATE->parameters[CROSSDUCKTHRESH].name = "THRESH";
	_STATE->parameters[CROSSDUCKTHRESH].valuename = " ";
	_STATE->parameters[CROSSDUCKTHRESH].type = ParameterType_double;
	_STATE->parameters[CROSSDUCKTHRESH].digits = 2;
	_STATE->parameters[CROSSDUCKTHRESH].initvalue = .5;
	_STATE->parameters[CROSSDUCKTHRESH].progress = .01;
	_STATE->parameters[CROSSDUCKTHRESH].flags |= Param::MidiParam;

	// Knee width around the threshold, 0..40 dB squared (0 is a binary mask).
	_STATE->parameters[CROSSDUCKSOFT] = _STATE->parameters[CROSSDUCKTHRESH];
	_STATE->parameters[CROSSDUCKSOFT].name = "SOFT";
	_STATE->parameters[CROSSDUCKSOFT].initvalue = .35;

	// Per-bin gain smoothing across grains, cubed to a 0..2 s time constant.
	_STATE->parameters[CROSSDUCKSMOOTH] = _STATE->parameters[CROSSDUCKTHRESH];
	_STATE->parameters[CROSSDUCKSMOOTH].name = "SMOOTH";
	_STATE->parameters[CROSSDUCKSMOOTH].initvalue = .35;

	// How far down a closed bin goes: 0 removes it, otherwise -60..0 dB.
	_STATE->parameters[CROSSDUCKFLOOR] = _STATE->parameters[CROSSDUCKTHRESH];
	_STATE->parameters[CROSSDUCKFLOOR].name = "FLOOR";
	_STATE->parameters[CROSSDUCKFLOOR].initvalue = 0.;

	_STATE->parameters[CROSSDUCKMODE].category = cross;
	_STATE->parameters[CROSSDUCKMODE].subcategory = sduck;
	_STATE->parameters[CROSSDUCKMODE].name = "MODE";
	_STATE->parameters[CROSSDUCKMODE].type = ParameterType_enum;
	_STATE->parameters[CROSSDUCKMODE].names = cross_duck_modes;
	_STATE->parameters[CROSSDUCKMODE].flags |= Param::MidiParam;


	_STATE->parameters[POWERButton].name = "POWER";
	_STATE->parameters[POWERButton].flags |= Param::MidiParam;
	_STATE->parameters[POWERButton].type = ParameterType_bool;

	_STATE->parameters[RECORDButton].name = "RECORD";
	_STATE->parameters[RECORDButton].flags |= Param::MidiParam;
	_STATE->parameters[RECORDButton].type = ParameterType_bool;

	_STATE->parameters[MICROPHONEButton].name = "MIC";
	_STATE->parameters[MICROPHONEButton].flags |= Param::MidiParam;
	_STATE->parameters[MICROPHONEButton].type = ParameterType_bool;


	_STATE->parameters[RECLOOPButton].name = "REC LOOP";
	_STATE->parameters[RECLOOPButton].flags |= Param::MidiParam;
	_STATE->parameters[RECLOOPButton].type = ParameterType_bool;

	_STATE->parameters[POWERTRACK].name = "POWER TRACK";
	_STATE->parameters[POWERTRACK].flags |= Param::MidiParam;
	_STATE->parameters[POWERTRACK].type = ParameterType_bool;
	_STATE->parameters[POWERTRACK].flags |= Param::NoAssignment;

	const char* chdir = "CHANGE DIR", * skipback = "SKIP BACK", * skipforw = "SKIP FORWARD", * stop = "STOP", * play = "PLAY";
	const char* loop = "LOOP";

	_STATE->parameters[STEPBACK].name = skipback;
	_STATE->parameters[STEPBACK].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[STEPBACK].type = ParameterType_bool;

	_STATE->parameters[STEPFORW].name = skipforw;
	_STATE->parameters[STEPFORW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[STEPFORW].type = ParameterType_bool;

	_STATE->parameters[STOPButton].name = stop;
	_STATE->parameters[STOPButton].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[STOPButton].type = ParameterType_bool;

	_STATE->parameters[TRACKSTOPPED].name = play;
	_STATE->parameters[TRACKSTOPPED].category = loop;
	_STATE->parameters[TRACKSTOPPED].type = ParameterType_bool;
	_STATE->parameters[TRACKSTOPPED].names = stoppedNames;
	_STATE->parameters[TRACKSTOPPED].flags |= Param::HasAfterChange;

	_STATE->parameters[PLAYButton].name = play;
	_STATE->parameters[PLAYButton].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[PLAYButton].type = ParameterType_bool;

	_STATE->parameters[DIRButton].name = chdir;
	_STATE->parameters[DIRButton].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[DIRButton].type = ParameterType_bool;

	_STATE->parameters[SLOWButton].name = nameVelDown;
	_STATE->parameters[SLOWButton].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[SLOWButton].type = ParameterType_bool;

	_STATE->parameters[FASTButton].name = nameVelup;
	_STATE->parameters[FASTButton].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[FASTButton].type = ParameterType_bool;

	_STATE->parameters[SYNCButton].name = nameSync;// "LOOP SYNC";
	_STATE->parameters[SYNCButton].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[SYNCButton].type = ParameterType_bool;

	_STATE->parameters[TRACK_SYNC_FACTOR].name = "TRACK SYNC FACTOR";
	_STATE->parameters[TRACK_SYNC_FACTOR].initvalue = 2.0;
	_STATE->parameters[TRACK_SYNC_FACTOR].flags |= Param::NoAssignment;
	_STATE->parameters[TRACK_SYNC_FACTOR].type = ParameterType_bool;


	_STATE->parameters[SYNCBACK].name = skipback;
	_STATE->parameters[SYNCBACK].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[SYNCBACK].type = ParameterType_bool;

	_STATE->parameters[SYNCFORW].name = skipforw;
	_STATE->parameters[SYNCFORW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[SYNCFORW].type = ParameterType_bool;

	_STATE->parameters[TRACK_CONTROLS_ACTIVE].name = nameSyncing;
	_STATE->parameters[TRACK_CONTROLS_ACTIVE].flags |= Param::MidiParam;
	_STATE->parameters[TRACK_CONTROLS_ACTIVE].type = ParameterType_bool;
	_STATE->parameters[TRACK_CONTROLS_ACTIVE].flags |= Param::NoAssignment;

	_STATE->parameters[POWERENVOBS].name = "POWER ENVF";
	_STATE->parameters[POWERENVOBS].flags |= Param::MidiParam;
	_STATE->parameters[POWERENVOBS].type = ParameterType_bool;

	_STATE->parameters[FOLLOWER1POW].name = "POWER ENVF";
	_STATE->parameters[FOLLOWER1POW].flags |= Param::MidiParam;
	_STATE->parameters[FOLLOWER1POW].type = ParameterType_bool;

	_STATE->parameters[FOLLOWER2POW].name = "POWER ENVF";
	_STATE->parameters[FOLLOWER2POW].flags |= Param::MidiParam;
	_STATE->parameters[FOLLOWER2POW].type = ParameterType_bool;

	_STATE->parameters[FOLLOWER3POW].name = "POWER ENVF";
	_STATE->parameters[FOLLOWER3POW].flags |= Param::MidiParam;
	_STATE->parameters[FOLLOWER3POW].type = ParameterType_bool;


	_STATE->parameters[OFFGRAIN].name = "POWER GRAINFX";
	_STATE->parameters[OFFGRAIN].flags |= Param::MidiParam;
	_STATE->parameters[OFFGRAIN].type = ParameterType_bool;

	_STATE->parameters[OFFFX].name = "POWER FX";
	_STATE->parameters[OFFFX].flags |= Param::MidiParam;
	_STATE->parameters[OFFFX].type = ParameterType_bool;

	_STATE->parameters[OFFSTEREOFX].name = "POWER STEREOFX";
	_STATE->parameters[OFFSTEREOFX].flags |= Param::MidiParam;
	_STATE->parameters[OFFSTEREOFX].type = ParameterType_bool;

	_STATE->parameters[BYPASSGRAINFX].name = "BYPASS GRAINFX";
	_STATE->parameters[BYPASSGRAINFX].flags |= Param::MidiParam;
	_STATE->parameters[BYPASSGRAINFX].type = ParameterType_bool;

	_STATE->parameters[BYPASSFX].name = "BYPASS FX";
	_STATE->parameters[BYPASSFX].flags |= Param::MidiParam;
	_STATE->parameters[BYPASSFX].type = ParameterType_bool;

	_STATE->parameters[BYPASSSTEREOFX].name = "BYPASS STEREOFX";
	_STATE->parameters[BYPASSSTEREOFX].flags |= Param::MidiParam;
	_STATE->parameters[BYPASSSTEREOFX].type = ParameterType_bool;

	_STATE->parameters[ENVF1_DEST].initvalue = SSBMODRATE;
	_STATE->parameters[ENVF2_DEST].initvalue = BPCENTER;
	_STATE->parameters[ENVF3_DEST].initvalue = DISTMIX;
	_STATE->parameters[ENVF1_DEST].flags |= Param::NoAssignment;
	_STATE->parameters[ENVF2_DEST].flags |= Param::NoAssignment;
	_STATE->parameters[ENVF3_DEST].flags |= Param::NoAssignment;
	_STATE->parameters[FOLLOWER1SRC].flags |= Param::NoAssignment;
	_STATE->parameters[FOLLOWER2SRC].flags |= Param::NoAssignment;
	_STATE->parameters[FOLLOWER3SRC].flags |= Param::NoAssignment;
	_STATE->parameters[AS].flags |= Param::NoAssignment;
	_STATE->parameters[ASGRAN].flags |= Param::NoAssignment;// | Param::MidiParam;
	_STATE->parameters[ASGRAN].type = ParameterType_enum;
	_STATE->parameters[ASGRAN].name = grainParams;
	;

	_STATE->parameters[ASLFO].flags |= Param::NoAssignment;
	_STATE->parameters[ASLFO].max = 3;

	_STATE->parameters[ASFX].flags |= Param::NoAssignment;


	_STATE->parameters[ASSTFX].flags |= Param::NoAssignment;
	_STATE->parameters[ASMDEL].flags |= Param::NoAssignment;
	_STATE->parameters[ASMDEL].max = 8;
	_STATE->parameters[ASFOL].flags |= Param::NoAssignment;


	_STATE->parameters[CROSSCEP1CUTMOD].category = cross;
	_STATE->parameters[CROSSCEP1CUTMOD].subcategory = cep;
	_STATE->parameters[CROSSCEP1CUTMOD].min = LOG10D20F(CUTOFFMIN);
	_STATE->parameters[CROSSCEP1CUTMOD].max = LOG10D20F(1. + CUTOFFMIN);
	_STATE->parameters[CROSSCEP1CUTMOD].name = "CUT MOD";
	_STATE->parameters[CROSSCEP1CUTMOD].valuename = " ";
	_STATE->parameters[CROSSCEP1CUTMOD].type = ParameterType_double;
	_STATE->parameters[CROSSCEP1CUTMOD].digits = 2;
	_STATE->parameters[CROSSCEP1CUTMOD].flags |= Param::MidiParam;
	_STATE->parameters[CROSSCEP1CUTMOD].initvalue = LOG10D20F(.04f + CUTOFFMIN);
	_STATE->parameters[CROSSCEP1CUTMOD].progress = 1;
	_STATE->parameters[CROSSCEP1CUTMOD].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CROSSCEP1CUTMOD].offset = CUTOFFMIN;

	_STATE->parameters[CROSSCEP2CUTMOD].category = cross;
	_STATE->parameters[CROSSCEP2CUTMOD].subcategory = cepw;
	_STATE->parameters[CROSSCEP2CUTMOD].min = LOG10D20F(CUTOFFMIN);
	_STATE->parameters[CROSSCEP2CUTMOD].max = LOG10D20F(1. + CUTOFFMIN);
	_STATE->parameters[CROSSCEP2CUTMOD].name = "CUT MOD";
	_STATE->parameters[CROSSCEP2CUTMOD].valuename = " ";
	_STATE->parameters[CROSSCEP2CUTMOD].type = ParameterType_double;
	_STATE->parameters[CROSSCEP2CUTMOD].digits = 2;
	_STATE->parameters[CROSSCEP2CUTMOD].flags |= Param::MidiParam;
	_STATE->parameters[CROSSCEP2CUTMOD].initvalue = LOG10D20F(.04f + CUTOFFMIN);
	_STATE->parameters[CROSSCEP2CUTMOD].progress = 1;
	_STATE->parameters[CROSSCEP2CUTMOD].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CROSSCEP2CUTMOD].offset = CUTOFFMIN;

	_STATE->parameters[CROSSCEP2CUTSRC].category = cross;
	_STATE->parameters[CROSSCEP2CUTSRC].subcategory = cepw;
	_STATE->parameters[CROSSCEP2CUTSRC].min = LOG10D20F(CUTOFFMIN);
	_STATE->parameters[CROSSCEP2CUTSRC].max = LOG10D20F(1. + CUTOFFMIN);
	_STATE->parameters[CROSSCEP2CUTSRC].name = "CUT SRC";
	_STATE->parameters[CROSSCEP2CUTSRC].valuename = " ";
	_STATE->parameters[CROSSCEP2CUTSRC].type = ParameterType_double;
	_STATE->parameters[CROSSCEP2CUTSRC].digits = 2;
	_STATE->parameters[CROSSCEP2CUTSRC].flags |= Param::MidiParam;
	_STATE->parameters[CROSSCEP2CUTSRC].initvalue = LOG10D20F(.04f + CUTOFFMIN);
	_STATE->parameters[CROSSCEP2CUTSRC].progress = 1;
	_STATE->parameters[CROSSCEP2CUTSRC].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CROSSCEP2CUTSRC].offset = CUTOFFMIN;

	_STATE->parameters[CROSSINTCUTMOD].category = cross;
	_STATE->parameters[CROSSINTCUTMOD].subcategory = ipol;
	_STATE->parameters[CROSSINTCUTMOD].min = LOG10D20F(CUTOFFMIN);
	_STATE->parameters[CROSSINTCUTMOD].max = LOG10D20F(1. + CUTOFFMIN);
	_STATE->parameters[CROSSINTCUTMOD].name = "CUT MOD";
	_STATE->parameters[CROSSINTCUTMOD].valuename = " ";
	_STATE->parameters[CROSSINTCUTMOD].type = ParameterType_double;
	_STATE->parameters[CROSSINTCUTMOD].digits = 2;
	_STATE->parameters[CROSSINTCUTMOD].flags |= Param::MidiParam;
	_STATE->parameters[CROSSINTCUTMOD].initvalue = LOG10D20F(.04f + CUTOFFMIN);
	_STATE->parameters[CROSSINTCUTMOD].progress = 1;
	_STATE->parameters[CROSSINTCUTMOD].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CROSSINTCUTMOD].offset = CUTOFFMIN;

	_STATE->parameters[CROSSINTCUTSRC].category = cross;
	_STATE->parameters[CROSSINTCUTSRC].subcategory = ipol;
	_STATE->parameters[CROSSINTCUTSRC].min = LOG10D20F(CUTOFFMIN);
	_STATE->parameters[CROSSINTCUTSRC].max = LOG10D20F(1. + CUTOFFMIN);
	_STATE->parameters[CROSSINTCUTSRC].name = "CUT SRC";
	_STATE->parameters[CROSSINTCUTSRC].valuename = " ";
	_STATE->parameters[CROSSINTCUTSRC].type = ParameterType_double;
	_STATE->parameters[CROSSINTCUTSRC].digits = 2;
	_STATE->parameters[CROSSINTCUTSRC].flags |= Param::MidiParam;
	_STATE->parameters[CROSSINTCUTSRC].initvalue = LOG10D20F(.04f + CUTOFFMIN);
	_STATE->parameters[CROSSINTCUTSRC].progress = 1;
	_STATE->parameters[CROSSINTCUTSRC].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[CROSSINTCUTSRC].offset = CUTOFFMIN;

	_STATE->parameters[REV5LPCUT].category = reverb5;
	_STATE->parameters[REV5LPCUT].min = LOG10D20F(1000.);
	_STATE->parameters[REV5LPCUT].max = LOG10D20F(12000.);
	_STATE->parameters[REV5LPCUT].name = "LPCUT";
	_STATE->parameters[REV5LPCUT].valuename = "Hz";
	_STATE->parameters[REV5LPCUT].type = ParameterType_double;
	_STATE->parameters[REV5LPCUT].digits = 0;
	_STATE->parameters[REV5LPCUT].initvalue = LOG10D20F(6000.);
	_STATE->parameters[REV5LPCUT].progress = 1;
	_STATE->parameters[REV5LPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV5LPCUT].flags |= Param::MidiParam;


	_STATE->parameters[REV5HPCUT].category = reverb5;
	_STATE->parameters[REV5HPCUT].min = LOG10D20F(18.);;
	_STATE->parameters[REV5HPCUT].max = LOG10D20F(1000.);
	_STATE->parameters[REV5HPCUT].name = "HPCUT";
	_STATE->parameters[REV5HPCUT].valuename = "Hz";
	_STATE->parameters[REV5HPCUT].type = ParameterType_double;
	_STATE->parameters[REV5HPCUT].digits = 0;
	_STATE->parameters[REV5HPCUT].initvalue = LOG10D20F(18.);;
	_STATE->parameters[REV5HPCUT].progress = 1;
	_STATE->parameters[REV5HPCUT].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[REV5HPCUT].flags |= Param::MidiParam;

	_STATE->parameters[GRAINPAN].category = grainParams;
	_STATE->parameters[GRAINPAN].min = 0.;
	_STATE->parameters[GRAINPAN].max = 1.;
	_STATE->parameters[GRAINPAN].name = "PANNING";
	_STATE->parameters[GRAINPAN].valuename = " ";
	_STATE->parameters[GRAINPAN].type = ParameterType_double;
	_STATE->parameters[GRAINPAN].digits = 2;
	_STATE->parameters[GRAINPAN].initvalue = .5;
	_STATE->parameters[GRAINPAN].progress = .05;
	_STATE->parameters[GRAINPAN].flags |= Param::MidiParam;

	const char* graingen = "GRAINGEN";
	const char* rnd1 = "RND1", * rnd2 = "RND2", * bounce = "BOUNCE", * spline = "SPLINE", * follow = "FOLLOW";

	_STATE->parameters[DENSDEV].category = graingen;
	_STATE->parameters[DENSDEV].subcategory = rnd1;
	_STATE->parameters[DENSDEV].min = 0.;
	_STATE->parameters[DENSDEV].max = 1.;
	_STATE->parameters[DENSDEV].name = "DEVIATION";
	_STATE->parameters[DENSDEV].valuename = " ";
	_STATE->parameters[DENSDEV].type = ParameterType_double;
	_STATE->parameters[DENSDEV].digits = 2;
	_STATE->parameters[DENSDEV].initvalue = 0;
	_STATE->parameters[DENSDEV].progress = .01;
	_STATE->parameters[DENSDEV].flags |= Param::MidiParam;

	_STATE->parameters[GRAINSEQVEL].category = seqName;
	_STATE->parameters[GRAINSEQVEL].min = BPMMIN;
	_STATE->parameters[GRAINSEQVEL].max = BPMMAX;
	_STATE->parameters[GRAINSEQVEL].name = "BPM";
	_STATE->parameters[GRAINSEQVEL].valuename = " ";
	_STATE->parameters[GRAINSEQVEL].type = ParameterType_double;
	_STATE->parameters[GRAINSEQVEL].digits = 1;
	_STATE->parameters[GRAINSEQVEL].initvalue = 120;
	_STATE->parameters[GRAINSEQVEL].progress = 1;
	_STATE->parameters[GRAINSEQVEL].flags |= Param::MidiParam;


	_STATE->parameters[GRAINSEQSTEPS].category = seqName;
	_STATE->parameters[GRAINSEQSTEPS].min = 1;
	_STATE->parameters[GRAINSEQSTEPS].max = 16;
	_STATE->parameters[GRAINSEQSTEPS].name = "STEPS";
	_STATE->parameters[GRAINSEQSTEPS].valuename = " ";
	_STATE->parameters[GRAINSEQSTEPS].type = ParameterType_double;
	_STATE->parameters[GRAINSEQSTEPS].digits = 0;
	_STATE->parameters[GRAINSEQSTEPS].initvalue = 4.;
	_STATE->parameters[GRAINSEQSTEPS].progress = 1;
	_STATE->parameters[GRAINSEQSTEPS].flags |= Param::MidiParam;

	_STATE->parameters[GRAINSEQFACT].category = seqName;
	_STATE->parameters[GRAINSEQFACT].initvalue = 2.;
	_STATE->parameters[GRAINSEQFACT].name = nameSyncFact;

	_STATE->parameters[GRAINSEQDIR].category = seqName;
	_STATE->parameters[GRAINSEQDIR].name = chdir;
	_STATE->parameters[GRAINSEQDIR].initvalue = 1.0;


	static const char* notename[16] = { "1", "2", "3", "4", "5", "6", "7", "8", "9",
											   "10",
											   "11", "12", "13", "14", "15", "16" };


	for (int32_t i = 0; i < 16; i++) {
		_STATE->parameters[GRAINSEQPITCH01 + i].category = seqName;
		_STATE->parameters[GRAINSEQPITCH01 + i].subcategory = notename[i];
		_STATE->parameters[GRAINSEQPITCH01 + i].min = -12.;
		_STATE->parameters[GRAINSEQPITCH01 + i].max = 12.;
		_STATE->parameters[GRAINSEQPITCH01 + i].name = "PITCH";
		_STATE->parameters[GRAINSEQPITCH01 + i].valuename = "Semitones";
		_STATE->parameters[GRAINSEQPITCH01 + i].type = ParameterType_double;
		_STATE->parameters[GRAINSEQPITCH01 + i].digits = 2;
		_STATE->parameters[GRAINSEQPITCH01 + i].initvalue = 0;
		_STATE->parameters[GRAINSEQPITCH01 + i].progress = 1;
		_STATE->parameters[GRAINSEQPITCH01 + i].flags |= Param::MidiParam;


		_STATE->parameters[GRAINSEQGAIN01 + i].category = seqName;
		_STATE->parameters[GRAINSEQGAIN01 + i].subcategory = notename[i];
		_STATE->parameters[GRAINSEQGAIN01 + i].min = -60;
		_STATE->parameters[GRAINSEQGAIN01 + i].max = 60;
		_STATE->parameters[GRAINSEQGAIN01 + i].name = "GAIN";
		_STATE->parameters[GRAINSEQGAIN01 + i].valuename = "dB";
		_STATE->parameters[GRAINSEQGAIN01 + i].type = ParameterType_double;
		_STATE->parameters[GRAINSEQGAIN01 + i].digits = 0;
		_STATE->parameters[GRAINSEQGAIN01 + i].initvalue = LOG10D20F(1.0);
		_STATE->parameters[GRAINSEQGAIN01 + i].progress = 1;
		_STATE->parameters[GRAINSEQGAIN01 + i].flags |= Param::MidiParam;

		_STATE->parameters[GRAINSEQSIZE01 + i].category = seqName;
		_STATE->parameters[GRAINSEQSIZE01 + i].subcategory = notename[i];
		_STATE->parameters[GRAINSEQSIZE01 + i].min = 0.;
		_STATE->parameters[GRAINSEQSIZE01 + i].max = 1.;
		_STATE->parameters[GRAINSEQSIZE01 + i].name = "SIZE";
		_STATE->parameters[GRAINSEQSIZE01 + i].valuename = "x";
		_STATE->parameters[GRAINSEQSIZE01 + i].type = ParameterType_double;
		_STATE->parameters[GRAINSEQSIZE01 + i].digits = 2;
		_STATE->parameters[GRAINSEQSIZE01 + i].initvalue = 1.0;
		_STATE->parameters[GRAINSEQSIZE01 + i].progress = 0.01;
		_STATE->parameters[GRAINSEQSIZE01 + i].flags |= Param::MidiParam;
	}

	for (int32_t z = 0; z < 4 * 48; z += 48) {
		for (int32_t i = 0; i < 16; i++) {
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].min = -12.;
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].max = 12.;
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].name = "PITCH";
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].valuename = "Semitones";
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].type = ParameterType_double;
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].digits = 2;
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].initvalue = 0;
			_STATE->parameters[GRAINSEQ0PITCH01 + i + z].progress = 1;
			_STATE->parameters[GRAINSEQ0PITCH01 + i +
				z].flags |= Param::MidiParam;


			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].min = -60;
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].max = 60;
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].name = "GAIN";
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].valuename = "dB";
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].type = ParameterType_double;
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].digits = 0;
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].initvalue = LOG10D20F(1.0);
			_STATE->parameters[GRAINSEQ0GAIN01 + i + z].progress = 1;
			_STATE->parameters[GRAINSEQ0GAIN01 + i +
				z].flags |= Param::MidiParam;

			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].min = 0.;
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].max = 1.;
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].name = "SIZE";
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].valuename = "x";
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].type = ParameterType_double;
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].digits = 2;
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].initvalue = 1.0;
			_STATE->parameters[GRAINSEQ0SIZE01 + i + z].progress = 0.01;
			_STATE->parameters[GRAINSEQ0SIZE01 + i +
				z].flags |= Param::MidiParam;
		}
	}


	_STATE->parameters[GRAINSEQSYNC].category = seqName;
	_STATE->parameters[GRAINSEQSYNC].name = nameSync;
	_STATE->parameters[GRAINSEQSYNC].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[GRAINSEQFAST].category = seqName;
	_STATE->parameters[GRAINSEQFAST].name = nameVelup;
	_STATE->parameters[GRAINSEQFAST].type = ParameterType_bool;
	_STATE->parameters[GRAINSEQFAST].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[GRAINSEQSLOW].category = seqName;
	_STATE->parameters[GRAINSEQSLOW].name = nameVelDown;
	_STATE->parameters[GRAINSEQSLOW].type = ParameterType_bool;
	_STATE->parameters[GRAINSEQSLOW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[GRAINSEQFORW].category = seqName;
	_STATE->parameters[GRAINSEQFORW].name = skipforw;
	_STATE->parameters[GRAINSEQFORW].type = ParameterType_bool;
	_STATE->parameters[GRAINSEQFORW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[GRAINSEQBACKW].category = seqName;
	_STATE->parameters[GRAINSEQBACKW].name = skipback;
	_STATE->parameters[GRAINSEQBACKW].type = ParameterType_bool;
	_STATE->parameters[GRAINSEQBACKW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[GRAINSEQDIR].category = seqName;
	_STATE->parameters[GRAINSEQDIR].name = chdir;
	_STATE->parameters[GRAINSEQDIR].initvalue = 1.;
	_STATE->parameters[GRAINSEQDIR].type = ParameterType_bool;
	_STATE->parameters[GRAINSEQDIR].flags |= Param::MidiParam;

	_STATE->parameters[GRAINSEQINPUT].category = seqName;
	_STATE->parameters[GRAINSEQINPUT].name = nameSyncing;
	_STATE->parameters[GRAINSEQINPUT].type = ParameterType_bool;
	_STATE->parameters[GRAINSEQINPUT].flags |= Param::MidiParam;
	_STATE->parameters[GRAINSEQINPUT].flags |= Param::NoAssignment;
	_STATE->parameters[GRAINSEQINPUT].setFlag(Param::NoAssignment, true);

	_STATE->parameters[ASLFO].max = 3;

	const char* rndName = "RND";

	_STATE->parameters[LFO1DEST].initvalue = GRAINSIZE;
	_STATE->parameters[LFO2DEST].initvalue = SPEED;
	_STATE->parameters[LFO3DEST].initvalue = PREGAIN;

	_STATE->parameters[LFOPOW].name = "LFO POWER";
	_STATE->parameters[LFOPOW].flags |= Param::MidiParam;
	_STATE->parameters[LFOPOW].type = ParameterType_bool;

	_STATE->parameters[LFOSYNC].name = "LFO SYNC";
	_STATE->parameters[LFOSYNC].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOSYNC].type = ParameterType_bool;

	_STATE->parameters[LFOBACKW].name = "LFO SKIP BACK";
	_STATE->parameters[LFOBACKW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOBACKW].type = ParameterType_bool;

	_STATE->parameters[LFOSTOP].name = "LFO STOP";
	_STATE->parameters[LFOSTOP].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOSTOP].type = ParameterType_bool;

	_STATE->parameters[LFOPLAY].name = "LFO PLAY";
	_STATE->parameters[LFOPLAY].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOPLAY].type = ParameterType_bool;

	_STATE->parameters[LFOFORW].name = "LFO SKIP FORW";
	_STATE->parameters[LFOFORW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOFORW].type = ParameterType_bool;

	_STATE->parameters[LFODIR].name = "LFO CHANGE DIR";
	_STATE->parameters[LFODIR].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFODIR].type = ParameterType_bool;

	_STATE->parameters[LFOSLOW].name = "LFO VEL DOWN";
	_STATE->parameters[LFOSLOW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOSLOW].type = ParameterType_bool;

	_STATE->parameters[LFOFAST].name = "LFO VEL UP";
	_STATE->parameters[LFOFAST].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);
	_STATE->parameters[LFOFAST].type = ParameterType_bool;

	_STATE->parameters[LFODESTPOWER].flags |= Param::MidiParam;
	_STATE->parameters[LFODESTPOWER].type = ParameterType_bool;


	for (int32_t tmp = 0; tmp < 3; tmp++) {

		_STATE->parameters[LFO1SPACE + tmp].flags |= Param::NoAssignment;
		_STATE->parameters[LFO1SPACE + tmp].max = _DATA->isRunningAsPlugin ? 3. : 2.;

		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].min = 0.;
		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].max = 1.;
		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].name = "PHASE";
		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].valuename = " ";
		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].type = ParameterType_double;
		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].digits = 3;
		_STATE->parameters[LFO1PHS + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].min = LOG10D20F(0.001);
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].max = LOG10D20F(20.);
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].name = "CPS";
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].valuename = "Hz";
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].type = ParameterType_double;
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].digits = 3;
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].initvalue = LOG10D20F(.2);
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].progress = .25;
		_STATE->parameters[LFO1CPS + tmp * LFONUMPARAMS].category = lfonames[tmp].data();



		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].min = 0.001;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].max = 20.;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].name = "MIDI x";
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].valuename = " ";
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].type = ParameterType_double;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].digits = 3;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].initvalue = 1.0;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].progress = .1;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1DEST + tmp * LFONUMPARAMS].setFlag(Param::OwnInitValue, true);
		_STATE->parameters[LFO1DEST + tmp * LFONUMPARAMS].flags |= Param::NoAssignment;
		_STATE->parameters[LFO1DEST + tmp * LFONUMPARAMS].setFlag(Param::NoAssignment, true);
		_STATE->parameters[LFO1DEST + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1DEST + tmp * LFONUMPARAMS].name = "TARGET";

		_STATE->parameters[LFO1CONTROLSACTIVE + tmp * LFONUMPARAMS].name = nameSyncing;
		_STATE->parameters[LFO1CONTROLSACTIVE + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1CONTROLSACTIVE + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1CONTROLSACTIVE + tmp * LFONUMPARAMS].setFlag(Param::NoAssignment, true);
		_STATE->parameters[LFO1CONTROLSACTIVE + tmp * LFONUMPARAMS].flags |= Param::NoAssignment;
		_STATE->parameters[LFO1CONTROLSACTIVE + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].name = nameSyncFact;
		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].initvalue = 2.0;
		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].setFlag(Param::NoAssignment, true);
		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].flags |= Param::NoAssignment;
		_STATE->parameters[LFO1SYNCFACT + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1CURVE + tmp * LFONUMPARAMS].initvalue = 0.0;
		_STATE->parameters[LFO1CURVE + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1CURVE + tmp * LFONUMPARAMS].name = "CURVE";
		_STATE->parameters[LFO1CURVE + tmp * LFONUMPARAMS].type = ParameterType_enum;
		_STATE->parameters[LFO1CURVE + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1CURVE + tmp * LFONUMPARAMS].names = lfo_envelopes_names;

		_STATE->parameters[LFO1DIR + tmp * LFONUMPARAMS].initvalue = 1.0;
		_STATE->parameters[LFO1DIR + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1DIR + tmp * LFONUMPARAMS].name = chdir;
		_STATE->parameters[LFO1DIR + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1DIR + tmp * LFONUMPARAMS].flags |= Param::MidiParam;

		_STATE->parameters[LFO1JOIN + tmp * LFONUMPARAMS].name = "JOIN ENDS";
		_STATE->parameters[LFO1JOIN + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1JOIN + tmp * LFONUMPARAMS].initvalue = 1.0;
		_STATE->parameters[LFO1JOIN + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].name = namePower;
		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1QUANT + tmp * LFONUMPARAMS].name = "QUANT";
		_STATE->parameters[LFO1QUANT + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1QUANT + tmp * LFONUMPARAMS].initvalue = 1.f;


		_STATE->parameters[LFO1EDITFUNC + tmp * LFONUMPARAMS].name = "EDIT CURVE";
		_STATE->parameters[LFO1EDITFUNC + tmp * LFONUMPARAMS].type = ParameterType_enum;
		_STATE->parameters[LFO1EDITFUNC + tmp * LFONUMPARAMS].names = editorcurvenames;
		_STATE->parameters[LFO1EDITFUNC + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].name = "SEGMENTS";
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].valuename = "/CYCLE";
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].min = 1;
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].progress = 1;
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].max = MAX_SEGMENTS_LFO;
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].initvalue = 8;
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1NSEGS + tmp * LFONUMPARAMS].type = ParameterType_double;
		_STATE->parameters[LFO1RECOMPUTE + tmp * LFONUMPARAMS].flags |= Param::NoAssignment;
		_STATE->parameters[LFO1REDRAW + tmp * LFONUMPARAMS].flags |= Param::NoAssignment;


		float pos1 = 0.;
		float val = 0;
		float inc = 1.f / 7.f;
		for (int32_t i = 0; i <= 8; i++) {
			_STATE->parameters[LFO1ENVX0 + tmp * LFONUMPARAMS +
				i].initvalue = pos1;
			_STATE->parameters[LFO1ENVX0 + tmp * LFONUMPARAMS + i].min = 0.;
			_STATE->parameters[LFO1ENVX0 + tmp * LFONUMPARAMS + i].max = 1.;
			_STATE->parameters[LFO1ENVY0 + tmp * LFONUMPARAMS +
				i].initvalue = val;
			_STATE->parameters[LFO1ENVY0 + tmp * LFONUMPARAMS + i].min = 0.;
			_STATE->parameters[LFO1ENVY0 + tmp * LFONUMPARAMS + i].max = 1.;
			_STATE->parameters[LFO1ENVX0 + tmp * LFONUMPARAMS + i].type = _STATE->parameters[LFO1ENVY0 + tmp * LFONUMPARAMS + i].type = ParameterType_double;

			val += inc;
			pos1 += .125f;
		}
		_STATE->parameters[LFO1ENVY8 + tmp * LFONUMPARAMS].initvalue = 0;

		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].initvalue = 1.f;
		_STATE->parameters[LFO1CLOCKMULTI + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1ZOOM + tmp * LFONUMPARAMS].initvalue = .8;
		_STATE->parameters[LFO1ZOOM + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1BOUNDA + tmp * 3].category = lfonames[tmp].data();
		_STATE->parameters[LFO1BOUNDA + tmp * 3].name = bounda;
		_STATE->parameters[LFO1BOUNDA + tmp * 3].min = 0;
		_STATE->parameters[LFO1BOUNDA + tmp * 3].max = 1.;
		_STATE->parameters[LFO1BOUNDA + tmp * 3].valuename = " ";
		_STATE->parameters[LFO1BOUNDA + tmp * 3].type = ParameterType_double;
		_STATE->parameters[LFO1BOUNDA + tmp * 3].digits = 2;
		_STATE->parameters[LFO1BOUNDA + tmp * 3].flags |= Param::MidiParam;//callback_midi_control_change_lfo_bounds;

		_STATE->parameters[LFO1BOUNDB + tmp * 3].category = lfonames[tmp].data();
		_STATE->parameters[LFO1BOUNDB + tmp * 3].name = boundb;
		_STATE->parameters[LFO1BOUNDB + tmp * 3].min = 0;
		_STATE->parameters[LFO1BOUNDB + tmp * 3].max = 1.;
		_STATE->parameters[LFO1BOUNDB + tmp * 3].valuename = " ";
		_STATE->parameters[LFO1BOUNDB + tmp * 3].type = ParameterType_double;
		_STATE->parameters[LFO1BOUNDB + tmp * 3].digits = 2;
		_STATE->parameters[LFO1BOUNDB + tmp * 3].flags |= Param::MidiParam;//callback_midi_control_change_lfo_bounds;



		_STATE->parameters[LFO1CPSMIN + tmp].min = LOG10D20F(0.001);
		_STATE->parameters[LFO1CPSMIN + tmp].max = LOG10D20F(20.);
		_STATE->parameters[LFO1CPSMIN + tmp].name = "CPS A";
		_STATE->parameters[LFO1CPSMIN + tmp].valuename = "Hz";
		_STATE->parameters[LFO1CPSMIN + tmp].type = ParameterType_double;
		_STATE->parameters[LFO1CPSMIN + tmp].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[LFO1CPSMIN + tmp].digits = 3;
		_STATE->parameters[LFO1CPSMIN + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1CPSMIN + tmp].initvalue = LOG10D20F(.2);
		_STATE->parameters[LFO1CPSMIN + tmp].progress = .25;
		_STATE->parameters[LFO1CPSMIN + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1CPSMIN + tmp].subcategory = rndName;

		_STATE->parameters[LFO1CPSMAX + tmp].min = LOG10D20F(0.001);
		_STATE->parameters[LFO1CPSMAX + tmp].max = LOG10D20F(20.);
		_STATE->parameters[LFO1CPSMAX + tmp].name = "CPS B";
		_STATE->parameters[LFO1CPSMAX + tmp].valuename = "Hz";
		_STATE->parameters[LFO1CPSMAX + tmp].type = ParameterType_double;
		_STATE->parameters[LFO1CPSMAX + tmp].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[LFO1CPSMAX + tmp].digits = 3;
		_STATE->parameters[LFO1CPSMAX + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1CPSMAX + tmp].initvalue = LOG10D20F(.2);
		_STATE->parameters[LFO1CPSMAX + tmp].progress = .25;
		_STATE->parameters[LFO1CPSMAX + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1CPSMAX].subcategory = rndName;

		_STATE->parameters[LFO1RNDDEPTH + tmp].min = LOG10D20F(0.0001f);
		_STATE->parameters[LFO1RNDDEPTH + tmp].max = LOG10D20(.25f);
		_STATE->parameters[LFO1RNDDEPTH + tmp].name = "R";
		_STATE->parameters[LFO1RNDDEPTH + tmp].valuename = " ";
		_STATE->parameters[LFO1RNDDEPTH + tmp].type = ParameterType_double;
		_STATE->parameters[LFO1RNDDEPTH + tmp].digits = 4;
		_STATE->parameters[LFO1RNDDEPTH + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1RNDDEPTH + tmp].initvalue = LOG10D20(0.1);
		_STATE->parameters[LFO1RNDDEPTH + tmp].progress = .1;
		_STATE->parameters[LFO1RNDDEPTH + tmp].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[LFO1RNDDEPTH + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1RNDDEPTH + tmp].subcategory = rndName;

		_STATE->parameters[LFO1RNDTYPE + tmp].initvalue = 1.;
		_STATE->parameters[LFO1RNDTYPE + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1RNDTYPE + tmp].subcategory = rndName;
		_STATE->parameters[LFO1RNDTYPE + tmp].type = ParameterType_enum;
		_STATE->parameters[LFO1RNDTYPE + tmp].name = "DISTR";
		_STATE->parameters[LFO1RNDTYPE + tmp].names = lforanddistr;
		_STATE->parameters[LFO1RNDTYPE + tmp].flags |= Param::MidiParam;

		_STATE->parameters[LFO1RNDCURVE + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1RNDCURVE + tmp].subcategory = rndName;
		_STATE->parameters[LFO1RNDCURVE + tmp].type = ParameterType_enum;
		_STATE->parameters[LFO1RNDCURVE + tmp].name = "CURVE";
		_STATE->parameters[LFO1RNDCURVE + tmp].names = lforandcurve;
		_STATE->parameters[LFO1RNDCURVE + tmp].flags |= Param::MidiParam;


		_STATE->parameters[LFO1RNDALPHA + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1RNDALPHA + tmp].subcategory = rndName;
		_STATE->parameters[LFO1RNDALPHA + tmp].min = LOG10D20F(0.01);
		_STATE->parameters[LFO1RNDALPHA + tmp].max = LOG10D20F(10.);
		_STATE->parameters[LFO1RNDALPHA + tmp].name = "ALPHA";
		_STATE->parameters[LFO1RNDALPHA + tmp].valuename = " ";
		_STATE->parameters[LFO1RNDALPHA + tmp].type = ParameterType_double;
		_STATE->parameters[LFO1RNDALPHA + tmp].digits = 2;
		_STATE->parameters[LFO1RNDALPHA + tmp].initvalue = LOG10D20F(1.);
		_STATE->parameters[LFO1RNDALPHA + tmp].progress = 1;
		_STATE->parameters[LFO1RNDALPHA + tmp].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[LFO1RNDALPHA + tmp].flags |= Param::MidiParam;

		_STATE->parameters[LFO1RNDBETA + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1RNDBETA + tmp].subcategory = rndName;
		_STATE->parameters[LFO1RNDBETA + tmp].min = LOG10D20F(0.01);
		_STATE->parameters[LFO1RNDBETA + tmp].max = LOG10D20F(10.);
		_STATE->parameters[LFO1RNDBETA + tmp].name = "BETA";
		_STATE->parameters[LFO1RNDBETA + tmp].valuename = " ";
		_STATE->parameters[LFO1RNDBETA + tmp].type = ParameterType_double;
		_STATE->parameters[LFO1RNDBETA + tmp].digits = 2;
		_STATE->parameters[LFO1RNDBETA + tmp].initvalue = LOG10D20F(1.);
		_STATE->parameters[LFO1RNDBETA + tmp].progress = 1;
		_STATE->parameters[LFO1RNDBETA + tmp].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[LFO1RNDBETA + tmp].flags |= Param::MidiParam;

		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].name = namePower;
		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1POWER + tmp * LFONUMPARAMS].category = lfonames[tmp].data();

		_STATE->parameters[LFO1SYNC + tmp].name = nameSync;
		_STATE->parameters[LFO1SYNC + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1SYNC + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1SYNC + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1SYNC + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);

		_STATE->parameters[LFO1BACKW + tmp].name = skipback;
		_STATE->parameters[LFO1BACKW + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1BACKW + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1BACKW + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1BACKW + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);

		_STATE->parameters[LFO1STOP + tmp].name = stop;
		_STATE->parameters[LFO1STOP + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1STOP + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1STOP + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1STOP + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);

		_STATE->parameters[LFO1STOPPED + tmp * LFONUMPARAMS].name = play;
		_STATE->parameters[LFO1STOPPED + tmp * LFONUMPARAMS].flags |= Param::MidiParam;
		_STATE->parameters[LFO1STOPPED + tmp * LFONUMPARAMS].type = ParameterType_bool;
		_STATE->parameters[LFO1STOPPED + tmp * LFONUMPARAMS].category = lfonames[tmp].data();
		_STATE->parameters[LFO1STOPPED + tmp * LFONUMPARAMS].names = stoppedNames;
		_STATE->parameters[LFO1STOPPED + tmp * LFONUMPARAMS].flags |= Param::HasAfterChange;

		_STATE->parameters[LFO1PLAY + tmp].name = play;
		_STATE->parameters[LFO1PLAY + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1PLAY + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1PLAY + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1PLAY + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);

		_STATE->parameters[LFO1FORW + tmp].name = skipforw;
		_STATE->parameters[LFO1FORW + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1FORW + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1FORW + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1FORW + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);

		_STATE->parameters[LFO1SLOW + tmp].name = nameVelDown;
		_STATE->parameters[LFO1SLOW + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1SLOW + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1SLOW + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1SLOW + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);

		_STATE->parameters[LFO1FAST + tmp].name = nameVelup;
		_STATE->parameters[LFO1FAST + tmp].flags |= Param::MidiParam;
		_STATE->parameters[LFO1FAST + tmp].type = ParameterType_bool;
		_STATE->parameters[LFO1FAST + tmp].category = lfonames[tmp].data();
		_STATE->parameters[LFO1FAST + tmp].flags |= (Param::NoAssignment | Param::MidiParam | Param::NoValue);
	}


	const uint16_t lfoMultisNumLFOPars[] = { LFO1CURVE, LFO1POWER, LFO1ZOOM, LFO1QUANT, LFO1EDITFUNC, LFO1NSEGS, LFO1DIR, LFO1JOIN, LFO1POWER, LFO1SYNCFACT, LFO1CONTROLSACTIVE, LFO1CPS, LFO1CLOCKMULTI, LFO1DEST };
	const uint16_t lfoMultis1[] = {
		LFO1SPACE,
		LFO1CPSMIN,
		LFO1CPSMAX,
		LFO1RNDDEPTH,
		LFO1RNDALPHA,
		LFO1RNDBETA,
		LFO1RNDTYPE,
		LFO1RNDCURVE,
		LFO1SYNC,
		LFO1SYNCFACT,
		LFO1STOP,
		LFO1PLAY,
		LFO1SLOW,
		LFO1FAST,
		LFO1BACKW,
		LFO1FORW
	};
	const uint16_t lfoMultis3[] = {
		LFO1BOUNDA,
		LFO1BOUNDB,

	};

	for (auto m : lfoMultisNumLFOPars) {
		_STATE->parameters[m].paramOffset = ASLFO;
		_STATE->parameters[m].offsetFact = LFONUMPARAMS;
	};
	for (auto m : lfoMultis3) {
		_STATE->parameters[m].paramOffset = ASLFO;
		_STATE->parameters[m].offsetFact = 3;
	};
	for (auto m : lfoMultis1) {
		_STATE->parameters[m].paramOffset = ASLFO;
	}



	const char* lim = "LIMITER";

	_STATE->parameters[LIMREL].category = lim;
	_STATE->parameters[LIMREL].min = LOG10D20F(1.);
	_STATE->parameters[LIMREL].max = LOG10D20F(1000.);;
	_STATE->parameters[LIMREL].name = "REL";
	_STATE->parameters[LIMREL].valuename = "ms";
	_STATE->parameters[LIMREL].type = ParameterType_double;
	_STATE->parameters[LIMREL].digits = 0;
	_STATE->parameters[LIMREL].initvalue = LOG10D20F(50.);
	_STATE->parameters[LIMREL].progress = 1;
	_STATE->parameters[LIMREL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[LIMREL].flags |= Param::MidiParam;

	_STATE->parameters[LIMTHRES].category = lim;
	_STATE->parameters[LIMTHRES].min = -6;
	_STATE->parameters[LIMTHRES].max = 0;
	_STATE->parameters[LIMTHRES].name = "THRESH";
	_STATE->parameters[LIMTHRES].valuename = "dB";
	_STATE->parameters[LIMTHRES].type = ParameterType_double;
	_STATE->parameters[LIMTHRES].digits = 1;
	_STATE->parameters[LIMTHRES].initvalue = 0;
	_STATE->parameters[LIMTHRES].progress = 1;
	_STATE->parameters[LIMTHRES].flags |= Param::MidiParam;

	const char* reverb7 = "REVERB7";

	_STATE->parameters[MODALREVMODES].category = reverb7;
	_STATE->parameters[MODALREVMODES].name = "MODE";
	_STATE->parameters[MODALREVMODES].type = ParameterType_enum;
	/* without .names the AU range is std::size({}) - 1 = -1, i.e. degenerate;
	   this is also what makes modes 10..12 addressable by the host */
	_STATE->parameters[MODALREVMODES].names = modalrevmodes;
	_STATE->parameters[MODALREVMODES].digits = 0;
	_STATE->parameters[MODALREVMODES].initvalue = 3;
	_STATE->parameters[MODALREVMODES].progress = 1;
	_STATE->parameters[MODALREVMODES].flags |= Param::MidiParam;

	_STATE->parameters[MODALREVHOLD].category = reverb7;
	_STATE->parameters[MODALREVHOLD].name = "HOLD";
	_STATE->parameters[MODALREVHOLD].type = ParameterType_bool;
	_STATE->parameters[MODALREVHOLD].flags |= Param::MidiParam;

	_STATE->parameters[MODALREVDELAY].category = reverb7;
	_STATE->parameters[MODALREVDELAY].min = LOG10D20F(.05f);
	_STATE->parameters[MODALREVDELAY].max = LOG10D20F(100.);
	_STATE->parameters[MODALREVDELAY].name = "T60";
	_STATE->parameters[MODALREVDELAY].valuename = "s";
	_STATE->parameters[MODALREVDELAY].type = ParameterType_double;
	_STATE->parameters[MODALREVDELAY].digits = 2;
	_STATE->parameters[MODALREVDELAY].initvalue = LOG10D20(3.);
	_STATE->parameters[MODALREVDELAY].progress = 1;
	_STATE->parameters[MODALREVDELAY].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[MODALREVDELAY].flags |= Param::MidiParam;


	_STATE->parameters[MODALREVWIDTH].category = reverb7;
	_STATE->parameters[MODALREVWIDTH].min = 0;
	_STATE->parameters[MODALREVWIDTH].max = 1;
	_STATE->parameters[MODALREVWIDTH].name = "WIDTH";
	_STATE->parameters[MODALREVWIDTH].valuename = " ";
	_STATE->parameters[MODALREVWIDTH].type = ParameterType_double;
	_STATE->parameters[MODALREVWIDTH].digits = 2;
	_STATE->parameters[MODALREVWIDTH].initvalue = 1.;
	_STATE->parameters[MODALREVWIDTH].progress = 0.05;
	_STATE->parameters[MODALREVWIDTH].flags |= Param::MidiParam;

	_STATE->parameters[MODALREVMORPH].category = reverb7;
	_STATE->parameters[MODALREVMORPH].min = 0;
	_STATE->parameters[MODALREVMORPH].max = 1;
	_STATE->parameters[MODALREVMORPH].name = "MORPH";
	_STATE->parameters[MODALREVMORPH].valuename = " ";
	_STATE->parameters[MODALREVMORPH].type = ParameterType_double;
	_STATE->parameters[MODALREVMORPH].digits = 2;
	_STATE->parameters[MODALREVMORPH].initvalue = 0.;
	_STATE->parameters[MODALREVMORPH].progress = 0.05;
	_STATE->parameters[MODALREVMORPH].flags |= Param::MidiParam;

	_STATE->parameters[MODALREVDUCK].category = reverb7;
	_STATE->parameters[MODALREVDUCK].min = 0;
	_STATE->parameters[MODALREVDUCK].max = 1;
	_STATE->parameters[MODALREVDUCK].name = "DUCK";
	_STATE->parameters[MODALREVDUCK].valuename = " ";
	_STATE->parameters[MODALREVDUCK].type = ParameterType_double;
	_STATE->parameters[MODALREVDUCK].digits = 2;
	_STATE->parameters[MODALREVDUCK].initvalue = 0.;
	_STATE->parameters[MODALREVDUCK].progress = 0.05;
	_STATE->parameters[MODALREVDUCK].flags |= Param::MidiParam;

#if GS_MODALREV_ADAPTIVE
	/* PURITY: adaptive MODES only - 1 = pure resynthesis (no random modes at
	   all), 0 = classic random tail. A live gain blend, no rebuild. */
	_STATE->parameters[MODALREVPURITY].category = reverb7;
	_STATE->parameters[MODALREVPURITY].min = 0;
	_STATE->parameters[MODALREVPURITY].max = 1;
	_STATE->parameters[MODALREVPURITY].name = "PURITY";
	_STATE->parameters[MODALREVPURITY].valuename = " ";
	_STATE->parameters[MODALREVPURITY].type = ParameterType_double;
	_STATE->parameters[MODALREVPURITY].digits = 2;
	_STATE->parameters[MODALREVPURITY].initvalue = 1.;
	_STATE->parameters[MODALREVPURITY].progress = 0.05;
	_STATE->parameters[MODALREVPURITY].flags |= Param::MidiParam;
#endif

	_STATE->parameters[MODALREVPITCH].category = reverb7;
	_STATE->parameters[MODALREVPITCH].min = -36.;
	_STATE->parameters[MODALREVPITCH].max = 36.;
	_STATE->parameters[MODALREVPITCH].name = "PITCH";
	_STATE->parameters[MODALREVPITCH].valuename = "Semitones";
	_STATE->parameters[MODALREVPITCH].type = ParameterType_double;
	_STATE->parameters[MODALREVPITCH].digits = 1;
	_STATE->parameters[MODALREVPITCH].initvalue = 0;
	_STATE->parameters[MODALREVPITCH].progress = 1;
	_STATE->parameters[MODALREVPITCH].flags |= Param::MidiParam;

	_STATE->parameters[MODALREVDRY].category = reverb7;
	_STATE->parameters[MODALREVDRY].min = -120;
	_STATE->parameters[MODALREVDRY].max = 120;
	_STATE->parameters[MODALREVDRY].name = "DRY";
	_STATE->parameters[MODALREVDRY].valuename = "dB";
	_STATE->parameters[MODALREVDRY].type = ParameterType_double;
	_STATE->parameters[MODALREVDRY].digits = 0;
	_STATE->parameters[MODALREVDRY].initvalue = -6.;
	_STATE->parameters[MODALREVDRY].progress = 1;
	_STATE->parameters[MODALREVDRY].flags |= Param::MidiParam;

	_STATE->parameters[MODALREVWET].category = reverb7;
	_STATE->parameters[MODALREVWET].min = -120;
	_STATE->parameters[MODALREVWET].max = 120;
	_STATE->parameters[MODALREVWET].name = "WET";
	_STATE->parameters[MODALREVWET].valuename = "dB";
	_STATE->parameters[MODALREVWET].type = ParameterType_double;
	_STATE->parameters[MODALREVWET].digits = 0;
	_STATE->parameters[MODALREVWET].initvalue = -6.;
	_STATE->parameters[MODALREVWET].progress = 1;
	_STATE->parameters[MODALREVWET].flags |= Param::MidiParam;

	_STATE->parameters[DISTRSOURCE].type = ParameterType_enum;
	_STATE->parameters[DISTRSOURCE].name = "TRACK INPUT";
	_STATE->parameters[DISTRSOURCE].names = tracknames;

	const char* modal = "MODAL";

	_STATE->parameters[MODALDRY].category = modal;
	_STATE->parameters[MODALDRY].min = -120;
	_STATE->parameters[MODALDRY].max = 60.;
	_STATE->parameters[MODALDRY].name = "DRY";
	_STATE->parameters[MODALDRY].valuename = "dB";
	_STATE->parameters[MODALDRY].type = ParameterType_double;
	_STATE->parameters[MODALDRY].initvalue = -120;
	_STATE->parameters[MODALDRY].progress = 1;
	_STATE->parameters[MODALDRY].flags |= Param::MidiParam;

	_STATE->parameters[MODALWET].category = modal;
	_STATE->parameters[MODALWET].min = -120;
	_STATE->parameters[MODALWET].max = 60.;
	_STATE->parameters[MODALWET].name = "WET";
	_STATE->parameters[MODALWET].valuename = "dB";
	_STATE->parameters[MODALWET].type = ParameterType_double;
	_STATE->parameters[MODALWET].initvalue = -12;
	_STATE->parameters[MODALWET].progress = 1;
	_STATE->parameters[MODALWET].flags |= Param::MidiParam;

	_STATE->parameters[MODALFREQ].category = modal;
	_STATE->parameters[MODALFREQ].min = LOG10D20F(20.);
	_STATE->parameters[MODALFREQ].max = LOG10D20F(14000.);
	_STATE->parameters[MODALFREQ].name = "CPS";
	_STATE->parameters[MODALFREQ].valuename = "Hz";
	_STATE->parameters[MODALFREQ].type = ParameterType_double;
	_STATE->parameters[MODALFREQ].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[MODALFREQ].initvalue = LOG10D20F(220.);
	_STATE->parameters[MODALFREQ].progress = 1.;
	_STATE->parameters[MODALFREQ].flags |= Param::MidiParam;

	_STATE->parameters[MODALQ].category = modal;
	_STATE->parameters[MODALQ].min = 0;
	_STATE->parameters[MODALQ].max = 1;
	_STATE->parameters[MODALQ].name = "RES";
	_STATE->parameters[MODALQ].valuename = " ";
	_STATE->parameters[MODALQ].type = ParameterType_double;
	_STATE->parameters[MODALQ].initvalue = .33;
	_STATE->parameters[MODALQ].progress = .05;
	_STATE->parameters[MODALQ].digits = 2;
	_STATE->parameters[MODALQ].flags |= Param::MidiParam;

	_STATE->parameters[MODALMODE].category = modal;
	_STATE->parameters[MODALMODE].name = "INSTRUMENT";
	_STATE->parameters[MODALMODE].initvalue = 0;
	_STATE->parameters[MODALMODE].type = ParameterType_enum;
	_STATE->parameters[MODALMODE].names = modal_names;
	_STATE->parameters[MODALMODE].flags |= Param::MidiParam;

	_STATE->parameters[MODALFOLLOW].category = modal;
	_STATE->parameters[MODALFOLLOW].name = "FOLLOW";
	_STATE->parameters[MODALFOLLOW].flags |= Param::MidiParam;

	_STATE->parameters[MODALHOLD].category = modal;
	_STATE->parameters[MODALHOLD].name = "HOLD";
	_STATE->parameters[MODALHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	_STATE->parameters[MODALFOLLOW].type = _STATE->parameters[MODALHOLD].type = ParameterType_bool;

	const char* fw = "FREQ WARP";

	_STATE->parameters[PVSPECBOUNDAIN].category = pv;
	_STATE->parameters[PVSPECBOUNDAIN].subcategory = fw;
	_STATE->parameters[PVSPECBOUNDAIN].min = 0;
	_STATE->parameters[PVSPECBOUNDAIN].max = 1;
	_STATE->parameters[PVSPECBOUNDAIN].name = "FROM A";
	_STATE->parameters[PVSPECBOUNDAIN].valuename = " ";
	_STATE->parameters[PVSPECBOUNDAIN].type = ParameterType_double;
	_STATE->parameters[PVSPECBOUNDAIN].initvalue = 0;
	_STATE->parameters[PVSPECBOUNDAIN].progress = .05;
	_STATE->parameters[PVSPECBOUNDAIN].digits = 2;
	_STATE->parameters[PVSPECBOUNDAIN].flags |= Param::MidiParam;

	_STATE->parameters[PVSPECBOUNDBIN].category = pv;
	_STATE->parameters[PVSPECBOUNDBIN].subcategory = fw;
	_STATE->parameters[PVSPECBOUNDBIN].min = 0;
	_STATE->parameters[PVSPECBOUNDBIN].max = 1;
	_STATE->parameters[PVSPECBOUNDBIN].name = "FROM B";
	_STATE->parameters[PVSPECBOUNDBIN].valuename = " ";
	_STATE->parameters[PVSPECBOUNDBIN].type = ParameterType_double;
	_STATE->parameters[PVSPECBOUNDBIN].initvalue = 1.;
	_STATE->parameters[PVSPECBOUNDBIN].progress = .05;
	_STATE->parameters[PVSPECBOUNDBIN].digits = 2;
	_STATE->parameters[PVSPECBOUNDBIN].flags |= Param::MidiParam;

	_STATE->parameters[PVSPECBOUNDAOUT].category = pv;
	_STATE->parameters[PVSPECBOUNDAOUT].subcategory = fw;
	_STATE->parameters[PVSPECBOUNDAOUT].min = 0;
	_STATE->parameters[PVSPECBOUNDAOUT].max = 1;
	_STATE->parameters[PVSPECBOUNDAOUT].name = "TO A";
	_STATE->parameters[PVSPECBOUNDAOUT].valuename = " ";
	_STATE->parameters[PVSPECBOUNDAOUT].type = ParameterType_double;
	_STATE->parameters[PVSPECBOUNDAOUT].initvalue = 0.;
	_STATE->parameters[PVSPECBOUNDAOUT].progress = .05;
	_STATE->parameters[PVSPECBOUNDAOUT].digits = 2;
	_STATE->parameters[PVSPECBOUNDAOUT].flags |= Param::MidiParam;

	_STATE->parameters[PVSPECBOUNDBOUT].category = pv;
	_STATE->parameters[PVSPECBOUNDBOUT].subcategory = fw;
	_STATE->parameters[PVSPECBOUNDBOUT].min = 0;
	_STATE->parameters[PVSPECBOUNDBOUT].max = 1;
	_STATE->parameters[PVSPECBOUNDBOUT].name = "TO B";
	_STATE->parameters[PVSPECBOUNDBOUT].valuename = " ";
	_STATE->parameters[PVSPECBOUNDBOUT].type = ParameterType_double;
	_STATE->parameters[PVSPECBOUNDBOUT].initvalue = 1.;
	_STATE->parameters[PVSPECBOUNDBOUT].progress = .05;
	_STATE->parameters[PVSPECBOUNDBOUT].digits = 2;
	_STATE->parameters[PVSPECBOUNDBOUT].flags |= Param::MidiParam;

	_STATE->parameters[PVSPECINV].category = pv;
	_STATE->parameters[PVSPECINV].subcategory = fw;
	_STATE->parameters[PVSPECINV].name = "INVERSE";
	_STATE->parameters[PVSPECINV].flags |= Param::MidiParam;
	_STATE->parameters[PVSPECINV].type = ParameterType_bool;



	_STATE->parameters[LOOP_POS].category = grainParams;
	_STATE->parameters[LOOP_POS].min = 0;
	_STATE->parameters[LOOP_POS].max = 1;
	_STATE->parameters[LOOP_POS].name = "LOOP POS";
	_STATE->parameters[LOOP_POS].valuename = " ";
	_STATE->parameters[LOOP_POS].type = ParameterType_double;
	_STATE->parameters[LOOP_POS].initvalue = .5;
	_STATE->parameters[LOOP_POS].progress = .05;
	_STATE->parameters[LOOP_POS].digits = 2;
	_STATE->parameters[LOOP_POS].flags |= Param::MidiParam;

	_STATE->parameters[REVERSEGRAINS].category = grainParams;
	_STATE->parameters[REVERSEGRAINS].min = 0;
	_STATE->parameters[REVERSEGRAINS].max = 1;
	_STATE->parameters[REVERSEGRAINS].name = "REVERSE";
	_STATE->parameters[REVERSEGRAINS].valuename = " ";
	_STATE->parameters[REVERSEGRAINS].type = ParameterType_double;
	_STATE->parameters[REVERSEGRAINS].initvalue = 0.;
	_STATE->parameters[REVERSEGRAINS].progress = .05;
	_STATE->parameters[REVERSEGRAINS].digits = 2;
	_STATE->parameters[REVERSEGRAINS].flags |= Param::MidiParam;


	_STATE->parameters[GRAINSIZE2].category = grainParams;
	_STATE->parameters[GRAINSIZE2].min = LOG10D20(0.005f);
	_STATE->parameters[GRAINSIZE2].max = LOG10D20(100.);
	_STATE->parameters[GRAINSIZE2].name = "GRAINSIZE";
	_STATE->parameters[GRAINSIZE2].valuename = "s";
	_STATE->parameters[GRAINSIZE2].type = ParameterType_double;
	_STATE->parameters[GRAINSIZE2].initvalue = LOG10D20(0.021f);
	_STATE->parameters[GRAINSIZE2].progress = 1;
	_STATE->parameters[GRAINSIZE2].digits = 3;
	_STATE->parameters[GRAINSIZE2].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINSIZE2].flags |= Param::MidiParam;

	static const char* loads[] = { "LOAD1", "LOAD2", "LOAD3", "LOAD4", "SAVE1", "SAVE2", "SAVE3",
								  "SAVE4" };
	for (int32_t i = 0; i < 4; i++) {
		_STATE->parameters[LOADSEQUENCE1 + i].category = seqName;
		_STATE->parameters[LOADSEQUENCE1 + i].name = loads[i];
		_STATE->parameters[LOADSEQUENCE1 + i].type = ParameterType_bool;
		_STATE->parameters[LOADSEQUENCE1 + i].flags |= (Param::MidiParam | Param::NoValue);
		_STATE->parameters[SAVESEQUENCE1 + i].category = seqName;
		_STATE->parameters[SAVESEQUENCE1 + i].name = loads[i + 4];
		_STATE->parameters[SAVESEQUENCE1 + i].type = ParameterType_bool;
		_STATE->parameters[SAVESEQUENCE1 + i].flags |= (Param::MidiParam | Param::NoValue);
	}
	const char* mcBands[] = { "BAND1", "BAND2", "BAND3" };
	const char* mcName = "MULTICOMP";

	_STATE->parameters[SPACEMULTICOMP].max = 3;
	_STATE->parameters[SPACEMULTICOMP].flags |= Param::NoAssignment;

	for (int32_t i = 0; i < 3; i++) {
		_STATE->parameters[MULTICOMPTHR1 + i].min = -60;
		_STATE->parameters[MULTICOMPTHR1 + i].max = LOG10D20F(1.);
		_STATE->parameters[MULTICOMPTHR1 + i].name = "THRES";
		_STATE->parameters[MULTICOMPTHR1 + i].valuename = "dB";
		_STATE->parameters[MULTICOMPTHR1 + i].type = ParameterType_double;
		_STATE->parameters[MULTICOMPTHR1 + i].flags |= Param::MidiParam;
		_STATE->parameters[MULTICOMPTHR1 + i].initvalue = -20;
		_STATE->parameters[MULTICOMPTHR1 + i].progress = 1.;
		_STATE->parameters[MULTICOMPTHR1 + i].category = mcName;
		_STATE->parameters[MULTICOMPTHR1 + i].subcategory = mcBands[i];

		_STATE->parameters[MULTICOMPMAKE1 + i].min = -60;
		_STATE->parameters[MULTICOMPMAKE1 + i].max = 60;
		_STATE->parameters[MULTICOMPMAKE1 + i].name = "MAKEUP";
		_STATE->parameters[MULTICOMPMAKE1 + i].valuename = "dB";
		_STATE->parameters[MULTICOMPMAKE1 + i].type = ParameterType_double;
		_STATE->parameters[MULTICOMPMAKE1 + i].flags |= Param::MidiParam;
		_STATE->parameters[MULTICOMPMAKE1 + i].initvalue = 0;
		_STATE->parameters[MULTICOMPMAKE1 + i].progress = 1;
		_STATE->parameters[MULTICOMPMAKE1 + i].category = mcName;
		_STATE->parameters[MULTICOMPMAKE1 + i].subcategory = mcBands[i];


		_STATE->parameters[MULTICOMPKNEE1 + i].min = 0;
		_STATE->parameters[MULTICOMPKNEE1 + i].max = 60.;
		_STATE->parameters[MULTICOMPKNEE1 + i].name = "KNEE";
		_STATE->parameters[MULTICOMPKNEE1 + i].valuename = "dB";
		_STATE->parameters[MULTICOMPKNEE1 + i].type = ParameterType_double;
		_STATE->parameters[MULTICOMPKNEE1 + i].flags |= Param::MidiParam;
		_STATE->parameters[MULTICOMPKNEE1 + i].initvalue = 10;
		_STATE->parameters[MULTICOMPKNEE1 + i].progress = 1;
		_STATE->parameters[MULTICOMPKNEE1 + i].category = mcName;
		_STATE->parameters[MULTICOMPKNEE1 + i].subcategory = mcBands[i];

		_STATE->parameters[MULTICOMPATTACK1 + i].min = LOG10D20F(0.1);
		_STATE->parameters[MULTICOMPATTACK1 + i].max = LOG10D20F(70.);
		_STATE->parameters[MULTICOMPATTACK1 + i].name = "ATTACK";
		_STATE->parameters[MULTICOMPATTACK1 + i].valuename = "ms";
		_STATE->parameters[MULTICOMPATTACK1 + i].type = ParameterType_double;
		_STATE->parameters[MULTICOMPATTACK1 + i].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[MULTICOMPATTACK1 + i].digits = 1;
		_STATE->parameters[MULTICOMPATTACK1 + i].initvalue = LOG10D20F(3.);
		_STATE->parameters[MULTICOMPATTACK1 + i].progress = 1;
		_STATE->parameters[MULTICOMPATTACK1 + i].flags |= Param::MidiParam;
		_STATE->parameters[MULTICOMPATTACK1 + i].category = mcName;
		_STATE->parameters[MULTICOMPATTACK1 + i].subcategory = mcBands[i];

		_STATE->parameters[MULTICOMPRELEASE1 + i].min = LOG10D20F(1.);
		_STATE->parameters[MULTICOMPRELEASE1 + i].max = LOG10D20F(200.);
		_STATE->parameters[MULTICOMPRELEASE1 + i].name = "RELEASE";
		_STATE->parameters[MULTICOMPRELEASE1 + i].valuename = "ms";
		_STATE->parameters[MULTICOMPRELEASE1 + i].type = ParameterType_double;
		_STATE->parameters[MULTICOMPRELEASE1 + i].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[MULTICOMPRELEASE1 + i].digits = 1;
		_STATE->parameters[MULTICOMPRELEASE1 + i].initvalue = LOG10D20F(30.);
		_STATE->parameters[MULTICOMPRELEASE1 + i].progress = 1;
		_STATE->parameters[MULTICOMPRELEASE1 + i].flags |= Param::MidiParam;
		_STATE->parameters[MULTICOMPRELEASE1 + i].category = mcName;
		_STATE->parameters[MULTICOMPRELEASE1 + i].subcategory = mcBands[i];


		_STATE->parameters[MULTICOMPRATIO1 + i].min = 0;
		_STATE->parameters[MULTICOMPRATIO1 + i].max = 1.;
		_STATE->parameters[MULTICOMPRATIO1 + i].name = "RATIO";
		_STATE->parameters[MULTICOMPRATIO1 + i].valuename = " ";
		_STATE->parameters[MULTICOMPRATIO1 + i].type = ParameterType_double;
		_STATE->parameters[MULTICOMPRATIO1 + i].flags |= Param::MidiParam;
		_STATE->parameters[MULTICOMPRATIO1 + i].initvalue = .5;
		_STATE->parameters[MULTICOMPRATIO1 + i].progress = .01;
		_STATE->parameters[MULTICOMPRATIO1 + i].category = mcName;
		_STATE->parameters[MULTICOMPRATIO1 + i].subcategory = mcBands[i];
	}

	const uint16_t mCompMultis[] = { MULTICOMPMAKE1, MULTICOMPATTACK1, MULTICOMPRELEASE1, MULTICOMPKNEE1, MULTICOMPRATIO1, MULTICOMPTHR1 };

	for (auto m : mCompMultis) {
		_STATE->parameters[m].paramOffset = SPACEMULTICOMP;
	};

	_STATE->parameters[MULTICOMPCROSS1].min = LOG10D20(50);
	_STATE->parameters[MULTICOMPCROSS1].max = LOG10D20(10000);
	_STATE->parameters[MULTICOMPCROSS1].name = "CROSS1";
	_STATE->parameters[MULTICOMPCROSS1].valuename = "Hz";
	_STATE->parameters[MULTICOMPCROSS1].type = ParameterType_double;
	_STATE->parameters[MULTICOMPCROSS1].digits = 0;
	_STATE->parameters[MULTICOMPCROSS1].initvalue = LOG10D20(120);
	_STATE->parameters[MULTICOMPCROSS1].progress = 1;
	_STATE->parameters[MULTICOMPCROSS1].flags |= Param::MidiParam;
	_STATE->parameters[MULTICOMPCROSS1].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[MULTICOMPCROSS1].category = mcName;

	_STATE->parameters[MULTICOMPCROSS2].min = LOG10D20(50);
	_STATE->parameters[MULTICOMPCROSS2].max = LOG10D20(10000);
	_STATE->parameters[MULTICOMPCROSS2].name = "CROSS2";
	_STATE->parameters[MULTICOMPCROSS2].valuename = "Hz";
	_STATE->parameters[MULTICOMPCROSS2].type = ParameterType_double;
	_STATE->parameters[MULTICOMPCROSS2].digits = 0;
	_STATE->parameters[MULTICOMPCROSS2].initvalue = LOG10D20(1000);
	_STATE->parameters[MULTICOMPCROSS2].progress = 1;
	_STATE->parameters[MULTICOMPCROSS2].flags |= Param::MidiParam;
	_STATE->parameters[MULTICOMPCROSS2].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[MULTICOMPCROSS2].category = mcName;

	_STATE->parameters[MULTICOMPGAIN].min = -60;
	_STATE->parameters[MULTICOMPGAIN].max = 60;
	_STATE->parameters[MULTICOMPGAIN].name = "GAIN";
	_STATE->parameters[MULTICOMPGAIN].valuename = "dB";
	_STATE->parameters[MULTICOMPGAIN].type = ParameterType_double;
	_STATE->parameters[MULTICOMPGAIN].digits = 0;
	_STATE->parameters[MULTICOMPGAIN].initvalue = 0;
	_STATE->parameters[MULTICOMPGAIN].progress = 1;
	_STATE->parameters[MULTICOMPGAIN].flags |= Param::MidiParam;
	_STATE->parameters[MULTICOMPGAIN].category = mcName;

	_STATE->parameters[STEREOWIDTH].category = "STEREOWIDTH";
	_STATE->parameters[STEREOWIDTH].min = 0;
	_STATE->parameters[STEREOWIDTH].max = 1;
	_STATE->parameters[STEREOWIDTH].name = "WIDTH";
	_STATE->parameters[STEREOWIDTH].valuename = " ";
	_STATE->parameters[STEREOWIDTH].type = ParameterType_double;
	_STATE->parameters[STEREOWIDTH].initvalue = 1.;
	_STATE->parameters[STEREOWIDTH].progress = .05;
	_STATE->parameters[STEREOWIDTH].digits = 2;
	_STATE->parameters[STEREOWIDTH].flags |= Param::MidiParam;

	//EQ5
	const char* dynCat = "EQ5DYN";

	_STATE->parameters[DYNEQ5LOWCF].min = LOG10D20F(18.);
	_STATE->parameters[DYNEQ5LOWCF].max = LOG10D20F(20000.);
	_STATE->parameters[DYNEQ5LOWCF].name = "LCF";
	_STATE->parameters[DYNEQ5LOWCF].valuename = "Hz";
	_STATE->parameters[DYNEQ5LOWCF].type = ParameterType_double;
	_STATE->parameters[DYNEQ5LOWCF].initvalue = LOG10D20F(100.);
	_STATE->parameters[DYNEQ5LOWCF].progress = 1.;
	_STATE->parameters[DYNEQ5LOWCF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5LOWGAIN].min = -24;
	_STATE->parameters[DYNEQ5LOWGAIN].max = 24.;
	_STATE->parameters[DYNEQ5LOWGAIN].name = "LGN";
	_STATE->parameters[DYNEQ5LOWGAIN].valuename = "dB";
	_STATE->parameters[DYNEQ5LOWGAIN].type = ParameterType_double;
	_STATE->parameters[DYNEQ5LOWGAIN].initvalue = 0;
	_STATE->parameters[DYNEQ5LOWGAIN].progress = 1;

	_STATE->parameters[DYNEQ50CF].min = LOG10D20F(100.);
	_STATE->parameters[DYNEQ50CF].max = LOG10D20F(400.);
	_STATE->parameters[DYNEQ50CF].name = "0CF";
	_STATE->parameters[DYNEQ50CF].valuename = "Hz";
	_STATE->parameters[DYNEQ50CF].type = ParameterType_double;
	_STATE->parameters[DYNEQ50CF].initvalue = LOG10D20F(300.);
	_STATE->parameters[DYNEQ50CF].progress = 1.;
	_STATE->parameters[DYNEQ50CF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ50GAIN].min = -24;
	_STATE->parameters[DYNEQ50GAIN].max = 24.;
	_STATE->parameters[DYNEQ50GAIN].name = "0GN";
	_STATE->parameters[DYNEQ50GAIN].valuename = "dB";
	_STATE->parameters[DYNEQ50GAIN].type = ParameterType_double;
	_STATE->parameters[DYNEQ50GAIN].initvalue = 0;
	_STATE->parameters[DYNEQ50GAIN].progress = 1;

	_STATE->parameters[DYNEQ51CF].min = LOG10D20F(400.);
	_STATE->parameters[DYNEQ51CF].max = LOG10D20F(1200.);
	_STATE->parameters[DYNEQ51CF].name = "1CF";
	_STATE->parameters[DYNEQ51CF].valuename = "Hz";
	_STATE->parameters[DYNEQ51CF].type = ParameterType_double;
	_STATE->parameters[DYNEQ51CF].initvalue = LOG10D20F(800.);
	_STATE->parameters[DYNEQ51CF].progress = 1.;
	_STATE->parameters[DYNEQ51CF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ51GAIN].min = -24;
	_STATE->parameters[DYNEQ51GAIN].max = 24.;
	_STATE->parameters[DYNEQ51GAIN].name = "1GN";
	_STATE->parameters[DYNEQ51GAIN].valuename = "dB";
	_STATE->parameters[DYNEQ51GAIN].type = ParameterType_double;
	_STATE->parameters[DYNEQ51GAIN].initvalue = 0;
	_STATE->parameters[DYNEQ51GAIN].progress = 1;

	_STATE->parameters[DYNEQ52CF].min = LOG10D20F(1200.);
	_STATE->parameters[DYNEQ52CF].max = LOG10D20F(2400.);
	_STATE->parameters[DYNEQ52CF].name = "2CF";
	_STATE->parameters[DYNEQ52CF].valuename = "Hz";
	_STATE->parameters[DYNEQ52CF].type = ParameterType_double;
	_STATE->parameters[DYNEQ52CF].initvalue = LOG10D20F(1240.);
	_STATE->parameters[DYNEQ52CF].progress = 1.;
	_STATE->parameters[DYNEQ52CF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ52GAIN].min = -24;
	_STATE->parameters[DYNEQ52GAIN].max = 24.;
	_STATE->parameters[DYNEQ52GAIN].name = "2GN";
	_STATE->parameters[DYNEQ52GAIN].valuename = "dB";
	_STATE->parameters[DYNEQ52GAIN].type = ParameterType_double;
	_STATE->parameters[DYNEQ52GAIN].initvalue = 0;
	_STATE->parameters[DYNEQ52GAIN].progress = 1;

	_STATE->parameters[DYNEQ5HIGHCF].min = LOG10D20F(2400.);
	_STATE->parameters[DYNEQ5HIGHCF].max = LOG10D20F(20000.);
	_STATE->parameters[DYNEQ5HIGHCF].name = "HIG";
	_STATE->parameters[DYNEQ5HIGHCF].valuename = "Hz";
	_STATE->parameters[DYNEQ5HIGHCF].type = ParameterType_double;
	_STATE->parameters[DYNEQ5HIGHCF].initvalue = LOG10D20F(3200.);
	_STATE->parameters[DYNEQ5HIGHCF].progress = 1.;
	_STATE->parameters[DYNEQ5HIGHCF].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5HIGHGAIN].min = -24;
	_STATE->parameters[DYNEQ5HIGHGAIN].max = 24.;
	_STATE->parameters[DYNEQ5HIGHGAIN].name = "HGN";
	_STATE->parameters[DYNEQ5HIGHGAIN].valuename = "dB";
	_STATE->parameters[DYNEQ5HIGHGAIN].type = ParameterType_double;
	_STATE->parameters[DYNEQ5HIGHGAIN].initvalue = 0;
	_STATE->parameters[DYNEQ5HIGHGAIN].progress = 1;


	_STATE->parameters[DYNEQ5QLOW].min = LOG10D20F(.01);
	_STATE->parameters[DYNEQ5QLOW].max = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5QLOW].name = "Q";
	_STATE->parameters[DYNEQ5QLOW].valuename = " ";
	_STATE->parameters[DYNEQ5QLOW].type = ParameterType_double;
	_STATE->parameters[DYNEQ5QLOW].initvalue = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5QLOW].progress = 1;
	_STATE->parameters[DYNEQ5QLOW].digits = 2;
	_STATE->parameters[DYNEQ5QLOW].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5Q0].min = LOG10D20F(.01);
	_STATE->parameters[DYNEQ5Q0].max = LOG10D20F(10.);
	_STATE->parameters[DYNEQ5Q0].name = "Q";
	_STATE->parameters[DYNEQ5Q0].valuename = " ";
	_STATE->parameters[DYNEQ5Q0].type = ParameterType_double;
	_STATE->parameters[DYNEQ5Q0].initvalue = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5Q0].progress = 1;
	_STATE->parameters[DYNEQ5Q0].digits = 2;
	_STATE->parameters[DYNEQ5Q0].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5Q1].min = LOG10D20F(.01);
	_STATE->parameters[DYNEQ5Q1].max = LOG10D20F(10.);
	_STATE->parameters[DYNEQ5Q1].name = "Q";
	_STATE->parameters[DYNEQ5Q1].valuename = " ";
	_STATE->parameters[DYNEQ5Q1].type = ParameterType_double;
	_STATE->parameters[DYNEQ5Q1].initvalue = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5Q1].progress = 1;
	_STATE->parameters[DYNEQ5Q1].digits = 2;
	_STATE->parameters[DYNEQ5Q1].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5Q2].min = LOG10D20F(.01);
	_STATE->parameters[DYNEQ5Q2].max = LOG10D20F(10.);
	_STATE->parameters[DYNEQ5Q2].name = "Q";
	_STATE->parameters[DYNEQ5Q2].valuename = " ";
	_STATE->parameters[DYNEQ5Q2].type = ParameterType_double;
	_STATE->parameters[DYNEQ5Q2].initvalue = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5Q2].progress = 1;
	_STATE->parameters[DYNEQ5Q2].digits = 2;
	_STATE->parameters[DYNEQ5Q2].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5QHIGH].min = LOG10D20F(.01);
	_STATE->parameters[DYNEQ5QHIGH].max = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5QHIGH].name = "Q";
	_STATE->parameters[DYNEQ5QHIGH].valuename = " ";
	_STATE->parameters[DYNEQ5QHIGH].type = ParameterType_double;
	_STATE->parameters[DYNEQ5QHIGH].initvalue = LOG10D20F(1.);
	_STATE->parameters[DYNEQ5QHIGH].progress = 1;
	_STATE->parameters[DYNEQ5QHIGH].digits = 2;
	_STATE->parameters[DYNEQ5QHIGH].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[DYNEQ5GAIN].min = -60;
	_STATE->parameters[DYNEQ5GAIN].max = 60.;
	_STATE->parameters[DYNEQ5GAIN].name = "GAIN";
	_STATE->parameters[DYNEQ5GAIN].valuename = "dB";
	_STATE->parameters[DYNEQ5GAIN].type = ParameterType_double;
	_STATE->parameters[DYNEQ5GAIN].initvalue = 0;
	_STATE->parameters[DYNEQ5GAIN].progress = 1;
	_STATE->parameters[DYNEQ5GAIN].flags |= Param::MidiParam;
	_STATE->parameters[DYNEQ5GAIN].category = dynCat;

	_STATE->parameters[SPACEDYNEQ].max = 5;

	const char* dynSubcat[] = { "LOW", "PEAK1", "PEAK2", "PEAK3", "HIGH" };

	_STATE->parameters[DYNEQFILT0].type = ParameterType_bool;
	_STATE->parameters[DYNEQFILT0].name = "PREBP";
	_STATE->parameters[DYNEQFILT0].flags |= Param::MidiParam;;




	for (int32_t i = 0; i < 5; i++) {
		_STATE->parameters[DYNEQFILT0 + i].category = dynCat;
		_STATE->parameters[DYNEQFILT0 + i].name = "PREBP";
		_STATE->parameters[DYNEQFILT0 + i].subcategory = dynSubcat[i];
		_STATE->parameters[DYNEQFILT0 + i].initvalue = 1;

		_STATE->parameters[DYNEQBELOW0 + i].name = "MODE";
		_STATE->parameters[DYNEQBELOW0 + i].subcategory = dynSubcat[i];
		_STATE->parameters[DYNEQBELOW0 + i].category = dynCat;
		_STATE->parameters[DYNEQBELOW0 + i].type = ParameterType_enum;
		_STATE->parameters[DYNEQBELOW0 + i].names = dyns2;
		_STATE->parameters[DYNEQBELOW0 + i].initvalue = 1.0;
		_STATE->parameters[DYNEQBELOW0 + i].flags |= Param::MidiParam;


		_STATE->parameters[DYNEQ5ENV10 + i].initvalue = 1.0;
		_STATE->parameters[DYNEQ5ENV20 + i].initvalue = 1.0;

		_STATE->parameters[DYNEQ5THR0 + i].min = -60;
		_STATE->parameters[DYNEQ5THR0 + i].max = 60.;
		_STATE->parameters[DYNEQ5THR0 + i].name = "THRES";
		_STATE->parameters[DYNEQ5THR0 + i].valuename = "dB";
		_STATE->parameters[DYNEQ5THR0 + i].type = ParameterType_double;
		_STATE->parameters[DYNEQ5THR0 + i].initvalue = 0;
		_STATE->parameters[DYNEQ5THR0 + i].progress = 1;
		_STATE->parameters[DYNEQ5THR0 + i].flags |= Param::MidiParam;
		_STATE->parameters[DYNEQ5THR0 + i].subcategory = dynSubcat[i];
		_STATE->parameters[DYNEQ5THR0 + i].category = dynCat;

		// Log10 (stored as 20*log10(ms)), on the ORIGINAL ids -- the separate
		// DYNEQ5ATTLOG/DYNEQ5RELLOG ids are retired, since presets before 22 are
		// converted on load instead. Slow end widened to 200/1000 ms, which is
		// where the authority measured: on 10 ms hits every 100 ms, ATT 1->50 ms
		// moved the mean applied gain 1.12 dB and 1->200 ms moves it 2.68 dB;
		// REL 1->200 was 4.89 dB and 1->1000 is 8.12 dB (2000 adds only 0.56
		// more). Minimums stay at 1 ms -- 0.1 ms and 1 ms differ by 0.02 dB.
		_STATE->parameters[DYNEQ5ATT0 + i].min = LOG10D20F(1.);
		_STATE->parameters[DYNEQ5ATT0 + i].max = LOG10D20F(200.);
		_STATE->parameters[DYNEQ5ATT0 + i].name = "ATT";
		_STATE->parameters[DYNEQ5ATT0 + i].valuename = "ms";
		_STATE->parameters[DYNEQ5ATT0 + i].type = ParameterType_double;
		_STATE->parameters[DYNEQ5ATT0 + i].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[DYNEQ5ATT0 + i].digits = 1;
		_STATE->parameters[DYNEQ5ATT0 + i].initvalue = LOG10D20F(5.);
		_STATE->parameters[DYNEQ5ATT0 + i].progress = 1;
		_STATE->parameters[DYNEQ5ATT0 + i].flags |= Param::MidiParam;
		_STATE->parameters[DYNEQ5ATT0 + i].subcategory = dynSubcat[i];
		_STATE->parameters[DYNEQ5ATT0 + i].category = dynCat;

		_STATE->parameters[DYNEQ5REL0 + i].min = LOG10D20F(1.);
		_STATE->parameters[DYNEQ5REL0 + i].max = LOG10D20F(1000.);
		_STATE->parameters[DYNEQ5REL0 + i].name = "REL";
		_STATE->parameters[DYNEQ5REL0 + i].valuename = "ms";
		_STATE->parameters[DYNEQ5REL0 + i].type = ParameterType_double;
		_STATE->parameters[DYNEQ5REL0 + i].paramCurve = Param::ParamCurve::Log10;
		_STATE->parameters[DYNEQ5REL0 + i].digits = 1;
		_STATE->parameters[DYNEQ5REL0 + i].initvalue = LOG10D20F(20.);
		_STATE->parameters[DYNEQ5REL0 + i].progress = 1;
		_STATE->parameters[DYNEQ5REL0 + i].flags |= Param::MidiParam;
		_STATE->parameters[DYNEQ5REL0 + i].subcategory = dynSubcat[i];
		_STATE->parameters[DYNEQ5REL0 + i].category = dynCat;

		// DYNEQ5ATTLOG0..4 / DYNEQ5RELLOG0..4 are retired: the Log10 curve now
		// lives on the original DYNEQ5ATT0/DYNEQ5REL0 ids above. The ids stay in
		// the enum -- ids never move -- but nothing initialises or reads them.
	}

	uint16_t dyneqmultis[] = { DYNEQFILT0 , DYNEQBELOW0 , DYNEQ5THR0, DYNEQ5ATT0, DYNEQ5REL0 };
	for (auto m : dyneqmultis) _STATE->parameters[m].paramOffset = SPACEDYNEQ;

	_STATE->parameters[SPACEDYNEQ].flags |= Param::NoAssignment;
	_STATE->parameters[SPACEDYNEQ].max = 5;

	const char* clip = "CLIPPER";

	_STATE->parameters[CLIPPERTHRS].category = clip;
	_STATE->parameters[CLIPPERTHRS].min = LOG10D20(0.1);
	_STATE->parameters[CLIPPERTHRS].max = LOG10D20(1.0);
	_STATE->parameters[CLIPPERTHRS].name = "CLIP";
	_STATE->parameters[CLIPPERTHRS].valuename = "db";
	_STATE->parameters[CLIPPERTHRS].type = ParameterType_double;
	_STATE->parameters[CLIPPERTHRS].initvalue = -6;
	_STATE->parameters[CLIPPERTHRS].progress = 1;
	_STATE->parameters[CLIPPERTHRS].flags |= Param::MidiParam;

	_STATE->parameters[CLIPPERSTART].category = clip;
	_STATE->parameters[CLIPPERSTART].min = 0.0;
	_STATE->parameters[CLIPPERSTART].max = 1.0;
	_STATE->parameters[CLIPPERSTART].name = "SOFTNESS";
	_STATE->parameters[CLIPPERSTART].valuename = " ";
	_STATE->parameters[CLIPPERSTART].type = ParameterType_double;
	_STATE->parameters[CLIPPERSTART].initvalue = 0.5;
	_STATE->parameters[CLIPPERSTART].progress = 0.05;
	_STATE->parameters[CLIPPERSTART].digits = 2;
	_STATE->parameters[CLIPPERSTART].flags |= Param::MidiParam;

	_STATE->parameters[ASGRAINGEN].category = graingen;
	_STATE->parameters[ASGRAINGEN].type = ParameterType_enum;
	_STATE->parameters[ASGRAINGEN].name = "ALGORITHM";
	_STATE->parameters[ASGRAINGEN].names = graingen_types;
	//_STATE->parameters[ASGRAINGEN].flags |= Param::MidiParam;

	_STATE->parameters[ARP_SWING].category = graingen;
	_STATE->parameters[ARP_SWING].min = 0;
	_STATE->parameters[ARP_SWING].max = 1;
	_STATE->parameters[ARP_SWING].name = "SWING";
	_STATE->parameters[ARP_SWING].valuename = " ";
	_STATE->parameters[ARP_SWING].type = ParameterType_double;
	_STATE->parameters[ARP_SWING].initvalue = 0;
	_STATE->parameters[ARP_SWING].progress = 0.05;
	_STATE->parameters[ARP_SWING].digits = 2;
	_STATE->parameters[ARP_SWING].flags |= Param::MidiParam;


	_STATE->parameters[GRAINGENMONO].category = graingen;
	_STATE->parameters[GRAINGENMONO].name = "SYNC L/R";
	_STATE->parameters[GRAINGENMONO].type = ParameterType_bool;
	_STATE->parameters[GRAINGENMONO].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENLOOP].category = graingen;
	_STATE->parameters[GRAINGENLOOP].subcategory = bounce;
	_STATE->parameters[GRAINGENLOOP].name = "LOOP";
	_STATE->parameters[GRAINGENLOOP].type = ParameterType_bool;


	_STATE->parameters[GRAINGENREST].category = graingen;
	_STATE->parameters[GRAINGENREST].subcategory = bounce;
	_STATE->parameters[GRAINGENREST].name = "HOLD";
	_STATE->parameters[GRAINGENREST].type = ParameterType_bool;
	_STATE->parameters[GRAINGENREST].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENHEIGHTA].category = graingen;
	_STATE->parameters[GRAINGENHEIGHTA].subcategory = bounce;
	_STATE->parameters[GRAINGENHEIGHTA].min = LOG10D20(0.001);
	_STATE->parameters[GRAINGENHEIGHTA].max = LOG10D20(100.);
	_STATE->parameters[GRAINGENHEIGHTA].name = "BOUNDA";
	_STATE->parameters[GRAINGENHEIGHTA].valuename = "m";
	_STATE->parameters[GRAINGENHEIGHTA].type = ParameterType_double;
	_STATE->parameters[GRAINGENHEIGHTA].initvalue = LOG10D20(10.);
	_STATE->parameters[GRAINGENHEIGHTA].progress = 1;
	_STATE->parameters[GRAINGENHEIGHTA].digits = 3;
	_STATE->parameters[GRAINGENHEIGHTA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENHEIGHTA].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENHEIGHTB].category = graingen;
	_STATE->parameters[GRAINGENHEIGHTB].subcategory = bounce;
	_STATE->parameters[GRAINGENHEIGHTB].min = LOG10D20(0.001);
	_STATE->parameters[GRAINGENHEIGHTB].max = LOG10D20(100.);
	_STATE->parameters[GRAINGENHEIGHTB].name = "BOUNDB";
	_STATE->parameters[GRAINGENHEIGHTB].valuename = "m";
	_STATE->parameters[GRAINGENHEIGHTB].type = ParameterType_double;
	_STATE->parameters[GRAINGENHEIGHTB].initvalue = LOG10D20(0.01);
	_STATE->parameters[GRAINGENHEIGHTB].progress = 1;
	_STATE->parameters[GRAINGENHEIGHTB].digits = 3;
	_STATE->parameters[GRAINGENHEIGHTB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENHEIGHTB].flags |= Param::MidiParam;

	// The engine has always read this (the BOUNCE period is 2h / VEL), but it had
	// no widget and no init, so it sat at 0 -- which on this Log10 curve IS 1 m/s,
	// the value the algorithm was effectively fixed at. Giving it a knob and an
	// initvalue of LOG10D20(1.) therefore changes nothing until it is turned.
	_STATE->parameters[GRAINGENVEL].category = graingen;
	_STATE->parameters[GRAINGENVEL].subcategory = bounce;
	_STATE->parameters[GRAINGENVEL].min = LOG10D20(0.1);
	_STATE->parameters[GRAINGENVEL].max = LOG10D20(20.);
	_STATE->parameters[GRAINGENVEL].name = "SPEED";
	_STATE->parameters[GRAINGENVEL].valuename = "m/s";
	_STATE->parameters[GRAINGENVEL].type = ParameterType_double;
	_STATE->parameters[GRAINGENVEL].initvalue = LOG10D20(1.);
	_STATE->parameters[GRAINGENVEL].progress = 1;
	_STATE->parameters[GRAINGENVEL].digits = 2;
	_STATE->parameters[GRAINGENVEL].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENVEL].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENSPLINECPSA].category = graingen;
	_STATE->parameters[GRAINGENSPLINECPSA].subcategory = spline;
	_STATE->parameters[GRAINGENSPLINECPSA].min = LOG10D20(0.05);
	_STATE->parameters[GRAINGENSPLINECPSA].max = LOG10D20(20);
	_STATE->parameters[GRAINGENSPLINECPSA].name = "CPSA";
	_STATE->parameters[GRAINGENSPLINECPSA].valuename = "Hz";
	_STATE->parameters[GRAINGENSPLINECPSA].type = ParameterType_double;
	_STATE->parameters[GRAINGENSPLINECPSA].initvalue = LOG10D20(0.5);
	_STATE->parameters[GRAINGENSPLINECPSA].progress = 1;
	_STATE->parameters[GRAINGENSPLINECPSA].digits = 2;
	_STATE->parameters[GRAINGENSPLINECPSA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENSPLINECPSA].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENSPLINECPSB].category = graingen;
	_STATE->parameters[GRAINGENSPLINECPSB].subcategory = spline;
	_STATE->parameters[GRAINGENSPLINECPSB].min = LOG10D20(0.05);
	_STATE->parameters[GRAINGENSPLINECPSB].max = LOG10D20(20);
	_STATE->parameters[GRAINGENSPLINECPSB].name = "CPSB";
	_STATE->parameters[GRAINGENSPLINECPSB].valuename = "Hz";
	_STATE->parameters[GRAINGENSPLINECPSB].type = ParameterType_double;
	_STATE->parameters[GRAINGENSPLINECPSB].initvalue = LOG10D20(1.0);
	_STATE->parameters[GRAINGENSPLINECPSB].progress = 1;
	_STATE->parameters[GRAINGENSPLINECPSB].digits = 2;
	_STATE->parameters[GRAINGENSPLINECPSB].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENSPLINECPSB].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENSPLINEDENSA].category = graingen;
	_STATE->parameters[GRAINGENSPLINEDENSA].subcategory = spline;
	_STATE->parameters[GRAINGENSPLINEDENSA].min = LOG10D20(0.1);
	_STATE->parameters[GRAINGENSPLINEDENSA].max = LOG10D20(_STATE->parameters[DENSITY].max);
	_STATE->parameters[GRAINGENSPLINEDENSA].name = "DENS A";
	_STATE->parameters[GRAINGENSPLINEDENSA].valuename = _STATE->parameters[DENSITY].valuename;
	_STATE->parameters[GRAINGENSPLINEDENSA].type = ParameterType_double;
	_STATE->parameters[GRAINGENSPLINEDENSA].initvalue = LOG10D20(1.5);
	_STATE->parameters[GRAINGENSPLINEDENSA].progress = 1;
	_STATE->parameters[GRAINGENSPLINEDENSA].digits = 2;
	_STATE->parameters[GRAINGENSPLINEDENSA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENSPLINEDENSA].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENSPLINEDENSB].category = graingen;
	_STATE->parameters[GRAINGENSPLINEDENSB].subcategory = spline;
	_STATE->parameters[GRAINGENSPLINEDENSB].min = LOG10D20(0.1);
	_STATE->parameters[GRAINGENSPLINEDENSB].max = LOG10D20(_STATE->parameters[DENSITY].max);
	_STATE->parameters[GRAINGENSPLINEDENSB].name = "DENS B";
	_STATE->parameters[GRAINGENSPLINEDENSB].valuename = _STATE->parameters[DENSITY].valuename;
	_STATE->parameters[GRAINGENSPLINEDENSB].type = ParameterType_double;
	_STATE->parameters[GRAINGENSPLINEDENSB].initvalue = LOG10D20(10);
	_STATE->parameters[GRAINGENSPLINEDENSB].progress = 1;
	_STATE->parameters[GRAINGENSPLINEDENSB].digits = 2;
	_STATE->parameters[GRAINGENSPLINEDENSB].flags |= Param::MidiParam;
	_STATE->parameters[GRAINGENSPLINEDENSB].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[GRAINGENFOLDENSA].category = graingen;
	_STATE->parameters[GRAINGENFOLDENSA].subcategory = follow;
	_STATE->parameters[GRAINGENFOLDENSA].min = LOG10D20(0.1);
	_STATE->parameters[GRAINGENFOLDENSA].max = LOG10D20(_STATE->parameters[DENSITY].max);
	_STATE->parameters[GRAINGENFOLDENSA].name = "DENS A";
	_STATE->parameters[GRAINGENFOLDENSA].valuename = _STATE->parameters[DENSITY].valuename;
	_STATE->parameters[GRAINGENFOLDENSA].type = ParameterType_double;
	_STATE->parameters[GRAINGENFOLDENSA].initvalue = LOG10D20(50);
	_STATE->parameters[GRAINGENFOLDENSA].progress = 1;
	_STATE->parameters[GRAINGENFOLDENSA].digits = 2;
	_STATE->parameters[GRAINGENFOLDENSA].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENFOLDENSA].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENFOLDENSB].category = graingen;
	_STATE->parameters[GRAINGENFOLDENSB].subcategory = follow;
	_STATE->parameters[GRAINGENFOLDENSB].min = LOG10D20(0.1);
	_STATE->parameters[GRAINGENFOLDENSB].max = LOG10D20(_STATE->parameters[DENSITY].max);
	_STATE->parameters[GRAINGENFOLDENSB].name = "DENS B";
	_STATE->parameters[GRAINGENFOLDENSB].valuename = _STATE->parameters[DENSITY].valuename;
	_STATE->parameters[GRAINGENFOLDENSB].type = ParameterType_double;
	_STATE->parameters[GRAINGENFOLDENSB].initvalue = LOG10D20(150);
	_STATE->parameters[GRAINGENFOLDENSB].progress = 1;
	_STATE->parameters[GRAINGENFOLDENSB].digits = 2;
	_STATE->parameters[GRAINGENFOLDENSB].flags |= Param::MidiParam;
	_STATE->parameters[GRAINGENFOLDENSB].paramCurve = Param::ParamCurve::Log10;

	_STATE->parameters[GRAINGENFOLGAIN].category = graingen;
	_STATE->parameters[GRAINGENFOLGAIN].subcategory = follow;
	_STATE->parameters[GRAINGENFOLGAIN].min = -60;
	_STATE->parameters[GRAINGENFOLGAIN].max = 60.;
	_STATE->parameters[GRAINGENFOLGAIN].name = "GAIN";
	_STATE->parameters[GRAINGENFOLGAIN].valuename = "dB";
	_STATE->parameters[GRAINGENFOLGAIN].type = ParameterType_double;
	_STATE->parameters[GRAINGENFOLGAIN].flags |= Param::MidiParam;
	_STATE->parameters[GRAINGENFOLGAIN].initvalue = 0;
	_STATE->parameters[GRAINGENFOLGAIN].progress = 1;

	_STATE->parameters[GRAINGENFOLATTACK].category = graingen;
	_STATE->parameters[GRAINGENFOLATTACK].subcategory = follow;
	_STATE->parameters[GRAINGENFOLATTACK].min = 0;
	_STATE->parameters[GRAINGENFOLATTACK].max = 200.;
	_STATE->parameters[GRAINGENFOLATTACK].name = "ATTACK";
	_STATE->parameters[GRAINGENFOLATTACK].valuename = "ms";
	_STATE->parameters[GRAINGENFOLATTACK].type = ParameterType_double;
	_STATE->parameters[GRAINGENFOLATTACK].flags |= Param::MidiParam;
	_STATE->parameters[GRAINGENFOLATTACK].initvalue = 50;
	_STATE->parameters[GRAINGENFOLATTACK].progress = 2;

	_STATE->parameters[GRAINGENFOLDECAY].category = graingen;
	_STATE->parameters[GRAINGENFOLDECAY].subcategory = follow;
	_STATE->parameters[GRAINGENFOLDECAY].min = 0;
	_STATE->parameters[GRAINGENFOLDECAY].max = 200.;
	_STATE->parameters[GRAINGENFOLDECAY].name = "DECAY";
	_STATE->parameters[GRAINGENFOLDECAY].valuename = "ms";
	_STATE->parameters[GRAINGENFOLDECAY].type = ParameterType_double;
	_STATE->parameters[GRAINGENFOLDECAY].flags |= Param::MidiParam;
	_STATE->parameters[GRAINGENFOLDECAY].initvalue = 50;
	_STATE->parameters[GRAINGENFOLDECAY].progress = 2;

	const char* chimes = "CHIMES", * figure = "FIGURE", * glissgen = "GLISS";

	_STATE->parameters[GRAINGENCHIMEWIND].category = graingen;
	_STATE->parameters[GRAINGENCHIMEWIND].subcategory = chimes;
	_STATE->parameters[GRAINGENCHIMEWIND].min = 0.;
	_STATE->parameters[GRAINGENCHIMEWIND].max = 1.;
	_STATE->parameters[GRAINGENCHIMEWIND].name = "WIND";
	_STATE->parameters[GRAINGENCHIMEWIND].valuename = " ";
	_STATE->parameters[GRAINGENCHIMEWIND].type = ParameterType_double;
	_STATE->parameters[GRAINGENCHIMEWIND].initvalue = 0.45;
	_STATE->parameters[GRAINGENCHIMEWIND].progress = 0.05;
	_STATE->parameters[GRAINGENCHIMEWIND].digits = 2;
	_STATE->parameters[GRAINGENCHIMEWIND].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENCHIMEGUST].category = graingen;
	_STATE->parameters[GRAINGENCHIMEGUST].subcategory = chimes;
	_STATE->parameters[GRAINGENCHIMEGUST].min = LOG10D20(0.02);
	_STATE->parameters[GRAINGENCHIMEGUST].max = LOG10D20(2.);
	_STATE->parameters[GRAINGENCHIMEGUST].name = "GUST";
	_STATE->parameters[GRAINGENCHIMEGUST].valuename = "Hz";
	_STATE->parameters[GRAINGENCHIMEGUST].type = ParameterType_double;
	_STATE->parameters[GRAINGENCHIMEGUST].initvalue = LOG10D20(0.12);
	_STATE->parameters[GRAINGENCHIMEGUST].progress = 1;
	_STATE->parameters[GRAINGENCHIMEGUST].digits = 2;
	_STATE->parameters[GRAINGENCHIMEGUST].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENCHIMEGUST].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENCHIMESPAN].category = graingen;
	_STATE->parameters[GRAINGENCHIMESPAN].subcategory = chimes;
	_STATE->parameters[GRAINGENCHIMESPAN].min = 2;
	_STATE->parameters[GRAINGENCHIMESPAN].max = 24;
	_STATE->parameters[GRAINGENCHIMESPAN].name = "SPAN";
	_STATE->parameters[GRAINGENCHIMESPAN].valuename = "Semitones";
	_STATE->parameters[GRAINGENCHIMESPAN].type = ParameterType_double;
	_STATE->parameters[GRAINGENCHIMESPAN].initvalue = 12;
	_STATE->parameters[GRAINGENCHIMESPAN].progress = 1;
	_STATE->parameters[GRAINGENCHIMESPAN].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENFIGSHAPE].category = graingen;
	_STATE->parameters[GRAINGENFIGSHAPE].subcategory = figure;
	_STATE->parameters[GRAINGENFIGSHAPE].name = "SHAPE";
	_STATE->parameters[GRAINGENFIGSHAPE].names = graingen_figure_shapes;
	_STATE->parameters[GRAINGENFIGSHAPE].type = ParameterType_enum;
	_STATE->parameters[GRAINGENFIGSHAPE].initvalue = 0;
	_STATE->parameters[GRAINGENFIGSHAPE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENFIGNOTES].category = graingen;
	_STATE->parameters[GRAINGENFIGNOTES].subcategory = figure;
	_STATE->parameters[GRAINGENFIGNOTES].min = 2;
	_STATE->parameters[GRAINGENFIGNOTES].max = 16;
	_STATE->parameters[GRAINGENFIGNOTES].name = "NOTES";
	_STATE->parameters[GRAINGENFIGNOTES].valuename = " ";
	_STATE->parameters[GRAINGENFIGNOTES].type = ParameterType_double;
	_STATE->parameters[GRAINGENFIGNOTES].initvalue = 6;
	_STATE->parameters[GRAINGENFIGNOTES].progress = 1;
	_STATE->parameters[GRAINGENFIGNOTES].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENFIGRANGE].category = graingen;
	_STATE->parameters[GRAINGENFIGRANGE].subcategory = figure;
	_STATE->parameters[GRAINGENFIGRANGE].min = 3;
	_STATE->parameters[GRAINGENFIGRANGE].max = 36;
	_STATE->parameters[GRAINGENFIGRANGE].name = "RANGE";
	_STATE->parameters[GRAINGENFIGRANGE].valuename = "Semitones";
	_STATE->parameters[GRAINGENFIGRANGE].type = ParameterType_double;
	_STATE->parameters[GRAINGENFIGRANGE].initvalue = 12;
	_STATE->parameters[GRAINGENFIGRANGE].progress = 1;
	_STATE->parameters[GRAINGENFIGRANGE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENFIGREST].category = graingen;
	_STATE->parameters[GRAINGENFIGREST].subcategory = figure;
	_STATE->parameters[GRAINGENFIGREST].min = LOG10D20(0.05);
	_STATE->parameters[GRAINGENFIGREST].max = LOG10D20(12.);
	_STATE->parameters[GRAINGENFIGREST].name = "REST";
	_STATE->parameters[GRAINGENFIGREST].valuename = "s";
	_STATE->parameters[GRAINGENFIGREST].type = ParameterType_double;
	_STATE->parameters[GRAINGENFIGREST].initvalue = LOG10D20(0.8);
	_STATE->parameters[GRAINGENFIGREST].progress = 1;
	_STATE->parameters[GRAINGENFIGREST].digits = 2;
	_STATE->parameters[GRAINGENFIGREST].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENFIGREST].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENGLSMODE].category = graingen;
	_STATE->parameters[GRAINGENGLSMODE].subcategory = glissgen;
	_STATE->parameters[GRAINGENGLSMODE].name = "MODE";
	_STATE->parameters[GRAINGENGLSMODE].names = graingen_gliss_modes;
	_STATE->parameters[GRAINGENGLSMODE].type = ParameterType_enum;
	_STATE->parameters[GRAINGENGLSMODE].initvalue = 0;
	_STATE->parameters[GRAINGENGLSMODE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENGLSRANGE].category = graingen;
	_STATE->parameters[GRAINGENGLSRANGE].subcategory = glissgen;
	_STATE->parameters[GRAINGENGLSRANGE].min = 1;
	_STATE->parameters[GRAINGENGLSRANGE].max = 36;
	_STATE->parameters[GRAINGENGLSRANGE].name = "RANGE";
	_STATE->parameters[GRAINGENGLSRANGE].valuename = "Semitones";
	_STATE->parameters[GRAINGENGLSRANGE].type = ParameterType_double;
	_STATE->parameters[GRAINGENGLSRANGE].initvalue = 12;
	_STATE->parameters[GRAINGENGLSRANGE].progress = 1;
	_STATE->parameters[GRAINGENGLSRANGE].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENGLSTIME].category = graingen;
	_STATE->parameters[GRAINGENGLSTIME].subcategory = glissgen;
	_STATE->parameters[GRAINGENGLSTIME].min = LOG10D20(0.1);
	_STATE->parameters[GRAINGENGLSTIME].max = LOG10D20(30.);
	_STATE->parameters[GRAINGENGLSTIME].name = "TIME";
	_STATE->parameters[GRAINGENGLSTIME].valuename = "s";
	_STATE->parameters[GRAINGENGLSTIME].type = ParameterType_double;
	_STATE->parameters[GRAINGENGLSTIME].initvalue = LOG10D20(3.);
	_STATE->parameters[GRAINGENGLSTIME].progress = 1;
	_STATE->parameters[GRAINGENGLSTIME].digits = 2;
	_STATE->parameters[GRAINGENGLSTIME].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENGLSTIME].flags |= Param::MidiParam;

	_STATE->parameters[GRAINGENGLSREST].category = graingen;
	_STATE->parameters[GRAINGENGLSREST].subcategory = glissgen;
	_STATE->parameters[GRAINGENGLSREST].min = LOG10D20(0.02);
	_STATE->parameters[GRAINGENGLSREST].max = LOG10D20(12.);
	_STATE->parameters[GRAINGENGLSREST].name = "REST";
	_STATE->parameters[GRAINGENGLSREST].valuename = "s";
	_STATE->parameters[GRAINGENGLSREST].type = ParameterType_double;
	_STATE->parameters[GRAINGENGLSREST].initvalue = LOG10D20(0.4);
	_STATE->parameters[GRAINGENGLSREST].progress = 1;
	_STATE->parameters[GRAINGENGLSREST].digits = 2;
	_STATE->parameters[GRAINGENGLSREST].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[GRAINGENGLSREST].flags |= Param::MidiParam;

	const char* loadNames[] = { "LOAD LOOP 1", "LOAD LOOP 2", "LOAD LOOP 3", "LOAD LOOP 4", "LOAD LOOP 5", "LOAD LOOP 6", "LOAD LOOP 7", "LOAD LOOP 8" };
	const char* saveNames[] = { "SAVE LOOP 1", "SAVE LOOP 2", "SAVE LOOP 3", "SAVE LOOP 4", "SAVE LOOP 5", "SAVE LOOP 6", "SAVE LOOP 7", "SAVE LOOP 8" };

	for (int i = 0;i < 8;i++) {
		_STATE->parameters[LOOPLOAD0 + i].name = loadNames[i];
		_STATE->parameters[LOOPSAVE0 + i].name = saveNames[i];
		_STATE->parameters[LOOPSAVE0 + i].type = _STATE->parameters[LOOPLOAD0 + i].type = ParameterType_bool;
		_STATE->parameters[LOOPSAVE0 + i].flags |= (Param::MidiParam | Param::NoValue);
		_STATE->parameters[LOOPLOAD0 + i].flags |= (Param::MidiParam | Param::NoValue);
	}


	const char* arp = "ARP";

	_STATE->parameters[ARP_INTERVAL].category = arp;
	_STATE->parameters[ARP_INTERVAL].min = 0;
	_STATE->parameters[ARP_INTERVAL].max = 12;
	_STATE->parameters[ARP_INTERVAL].name = "INTERVAL";
	_STATE->parameters[ARP_INTERVAL].valuename = "Semitones";
	_STATE->parameters[ARP_INTERVAL].type = ParameterType_double;
	_STATE->parameters[ARP_INTERVAL].initvalue = 7;
	_STATE->parameters[ARP_INTERVAL].progress = 1;
	_STATE->parameters[ARP_INTERVAL].digits = 2;
	_STATE->parameters[ARP_INTERVAL].flags |= Param::MidiParam;

	_STATE->parameters[ARP_CYCLES].category = arp;
	_STATE->parameters[ARP_CYCLES].min = 1;
	_STATE->parameters[ARP_CYCLES].max = 32;
	_STATE->parameters[ARP_CYCLES].name = "STEPS";
	_STATE->parameters[ARP_CYCLES].valuename = " ";
	_STATE->parameters[ARP_CYCLES].type = ParameterType_double;
	_STATE->parameters[ARP_CYCLES].initvalue = 3;
	_STATE->parameters[ARP_CYCLES].progress = 1;
	_STATE->parameters[ARP_CYCLES].digits = 0;
	_STATE->parameters[ARP_CYCLES].flags |= Param::MidiParam;


	_STATE->parameters[ARP_CYCLEMODE].category = arp;
	_STATE->parameters[ARP_CYCLEMODE].name = "LOOP";
	_STATE->parameters[ARP_CYCLEMODE].initvalue = 0;
	_STATE->parameters[ARP_CYCLEMODE].type = ParameterType_enum;
	_STATE->parameters[ARP_CYCLEMODE].names = cycle_modes;
	_STATE->parameters[ARP_CYCLEMODE].flags |= Param::MidiParam;

	for (int32_t z = SEQSAVE01; z < SEQSAVE01 + 4 *
		grainsequencer::NUM_LOADPARAMS; z += grainsequencer::NUM_LOADPARAMS) {
		_STATE->parameters[z +
			grainsequencer::SEQSTEPS].initvalue = _STATE->parameters[GRAINSEQSTEPS].initvalue;
		_STATE->parameters[z +
			grainsequencer::SEQGRAINSCOUNT].initvalue = _STATE->parameters[
				z +
					grainsequencer::SEQGRAINS].initvalue = _STATE->parameters[GRAINS].initvalue;
				_STATE->parameters[z +
					grainsequencer::SEQSILENCE].initvalue = _STATE->parameters[SILENCE].initvalue;
				_STATE->parameters[z +
					grainsequencer::SEQMODE].initvalue = _STATE->parameters[SEQ_MODE].initvalue;
				_STATE->parameters[z +
					grainsequencer::SEQARPMODE].initvalue = _STATE->parameters[ARP_CYCLEMODE].initvalue;
				_STATE->parameters[z +
					grainsequencer::SEQARPCYCLES].initvalue = _STATE->parameters[ARP_CYCLES].initvalue;
				_STATE->parameters[z +
					grainsequencer::SEQINT].initvalue = _STATE->parameters[ARP_INTERVAL].initvalue;
	}

	const char* ppd = "PP DELAY";

	_STATE->parameters[PPDELAY].category = ppd;
	_STATE->parameters[PPDELAY].min = LOG10D20F(5.);
	_STATE->parameters[PPDELAY].max = LOG10D20F(1000.0);
	_STATE->parameters[PPDELAY].name = "DELAY";
	_STATE->parameters[PPDELAY].valuename = "ms";
	_STATE->parameters[PPDELAY].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PPDELAY].type = ParameterType_double;
	_STATE->parameters[PPDELAY].digits = 1;
	_STATE->parameters[PPDELAY].initvalue = LOG10D20F(500.);
	_STATE->parameters[PPDELAY].progress = .125;
	_STATE->parameters[PPDELAY].flags |= Param::MidiParam;

	_STATE->parameters[PPFB].category = ppd;
	_STATE->parameters[PPFB].min = -1.;
	_STATE->parameters[PPFB].max = 1.;
	_STATE->parameters[PPFB].name = "FB";
	_STATE->parameters[PPFB].valuename = " ";
	_STATE->parameters[PPFB].type = ParameterType_double;
	_STATE->parameters[PPFB].digits = 2;
	_STATE->parameters[PPFB].initvalue = .75;
	_STATE->parameters[PPFB].progress = .01;
	_STATE->parameters[PPFB].flags |= Param::MidiParam;


	_STATE->parameters[PPHP].category = ppd;
	_STATE->parameters[PPHP].min = LOG10D20F(18.);
	_STATE->parameters[PPHP].max = LOG10D20F(20000.);
	_STATE->parameters[PPHP].name = "HP CUT";
	_STATE->parameters[PPHP].valuename = "Hz";
	_STATE->parameters[PPHP].type = ParameterType_double;
	_STATE->parameters[PPHP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PPHP].initvalue = LOG10D20F(18.);
	_STATE->parameters[PPHP].progress = .25;
	_STATE->parameters[PPHP].flags |= Param::MidiParam;

	_STATE->parameters[PPLP].category = ppd;
	_STATE->parameters[PPLP].min = LOG10D20F(18.);
	_STATE->parameters[PPLP].max = LOG10D20F(20000.);
	_STATE->parameters[PPLP].name = "LP CUT";
	_STATE->parameters[PPLP].valuename = "Hz";
	_STATE->parameters[PPLP].type = ParameterType_double;
	_STATE->parameters[PPLP].paramCurve = Param::ParamCurve::Log10;
	_STATE->parameters[PPLP].initvalue = LOG10D20F(20000);
	_STATE->parameters[PPLP].progress = .25;
	_STATE->parameters[PPLP].flags |= Param::MidiParam;

	_STATE->parameters[PPSHIFT].category = ppd;
	_STATE->parameters[PPSHIFT].min = -12.;
	_STATE->parameters[PPSHIFT].max = 12.;
	_STATE->parameters[PPSHIFT].name = "SHIFT";
	_STATE->parameters[PPSHIFT].valuename = "Semitones";
	_STATE->parameters[PPSHIFT].type = ParameterType_double;
	_STATE->parameters[PPSHIFT].digits = 1;
	_STATE->parameters[PPSHIFT].initvalue = 1;
	_STATE->parameters[PPSHIFT].progress = 1;
	_STATE->parameters[PPSHIFT].flags |= Param::MidiParam;

	_STATE->parameters[PPSHIFTMIX].category = ppd;
	_STATE->parameters[PPSHIFTMIX].min = 0.;
	_STATE->parameters[PPSHIFTMIX].max = 1.;
	_STATE->parameters[PPSHIFTMIX].name = "SHMIX";
	_STATE->parameters[PPSHIFTMIX].valuename = " ";
	_STATE->parameters[PPSHIFTMIX].type = ParameterType_double;
	_STATE->parameters[PPSHIFTMIX].digits = 2;
	_STATE->parameters[PPSHIFTMIX].initvalue = 0;
	_STATE->parameters[PPSHIFTMIX].progress = .05;
	_STATE->parameters[PPSHIFTMIX].flags |= Param::MidiParam;


	_STATE->parameters[PPDRY].category = ppd;
	_STATE->parameters[PPDRY].min = -60;
	_STATE->parameters[PPDRY].max = 60.;
	_STATE->parameters[PPDRY].name = "DRY";
	_STATE->parameters[PPDRY].valuename = "dB";
	_STATE->parameters[PPDRY].type = ParameterType_double;
	_STATE->parameters[PPDRY].initvalue = -6;
	_STATE->parameters[PPDRY].progress = 1;
	_STATE->parameters[PPDRY].flags |= Param::MidiParam;

	_STATE->parameters[PPWET].category = ppd;
	_STATE->parameters[PPWET].min = -60;
	_STATE->parameters[PPWET].max = 60.;
	_STATE->parameters[PPWET].name = "WET";
	_STATE->parameters[PPWET].valuename = "dB";
	_STATE->parameters[PPWET].type = ParameterType_double;
	_STATE->parameters[PPWET].initvalue = -6;
	_STATE->parameters[PPWET].progress = 1;
	_STATE->parameters[PPWET].flags |= Param::MidiParam;

	_STATE->parameters[PPHOLD].category = ppd;
	_STATE->parameters[PPHOLD].flags |= Param::MidiParam;
	_STATE->parameters[PPHOLD].name = "FREEZE";
	_STATE->parameters[PPHOLD].type = ParameterType_bool;
	_STATE->parameters[PPHOLD].flags |= Param::NoAssignment | Param::MidiParam;

	_STATE->parameters[PPREVERSE].category = ppd;
	_STATE->parameters[PPREVERSE].flags |= Param::MidiParam;
	_STATE->parameters[PPREVERSE].name = "BACKW";
	_STATE->parameters[PPREVERSE].type = ParameterType_bool;

	_STATE->parameters[PP_CONTROLS_ACTIVE].category = ppd;
	_STATE->parameters[PP_CONTROLS_ACTIVE].name = nameSyncing;
	_STATE->parameters[PP_CONTROLS_ACTIVE].flags |= Param::MidiParam;
	_STATE->parameters[PP_CONTROLS_ACTIVE].type = ParameterType_bool;
	_STATE->parameters[PP_CONTROLS_ACTIVE].flags |= Param::NoAssignment;


	_STATE->parameters[PP_SYNC].category = ppd;
	_STATE->parameters[PP_SYNC].name = nameSync;
	_STATE->parameters[PP_SYNC].type = ParameterType_bool;
	_STATE->parameters[PP_SYNC].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[PP_FAST].category = ppd;
	_STATE->parameters[PP_FAST].name = nameVelup;
	_STATE->parameters[PP_FAST].type = ParameterType_bool;
	_STATE->parameters[PP_FAST].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[PP_SLOW].category = ppd;
	_STATE->parameters[PP_SLOW].name = nameVelDown;
	_STATE->parameters[PP_SLOW].type = ParameterType_bool;
	_STATE->parameters[PP_SLOW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[PP_SYNC_FACTOR].category = ppd;
	_STATE->parameters[PP_SYNC_FACTOR].name = nameSyncFact;
	_STATE->parameters[PP_SYNC_FACTOR].flags |= Param::NoAssignment;
	_STATE->parameters[PP_SYNC_FACTOR].type = ParameterType_bool;
	_STATE->parameters[PP_SYNC_FACTOR].initvalue = 2.;


	const char* bpm = "BPM";
	_STATE->parameters[BPMSYNCSYNC].category = bpm;
	_STATE->parameters[BPMSYNCSYNC].name = nameSync;
	_STATE->parameters[BPMSYNCSYNC].type = ParameterType_bool;
	_STATE->parameters[BPMSYNCSYNC].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[BPMSYNCINPUT].category = bpm;
	_STATE->parameters[BPMSYNCINPUT].name = nameSyncing;
	_STATE->parameters[BPMSYNCINPUT].type = ParameterType_bool;
	_STATE->parameters[BPMSYNCINPUT].flags |= (Param::MidiParam | Param::NoAssignment);


	_STATE->parameters[BPMSYNCFAST].category = bpm;
	_STATE->parameters[BPMSYNCFAST].name = nameVelup;
	_STATE->parameters[BPMSYNCFAST].type = ParameterType_bool;
	_STATE->parameters[BPMSYNCFAST].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[BPMSYNCSLOW].category = bpm;
	_STATE->parameters[BPMSYNCSLOW].name = nameVelDown;
	_STATE->parameters[BPMSYNCSLOW].type = ParameterType_bool;
	_STATE->parameters[BPMSYNCSLOW].flags |= (Param::MidiParam | Param::NoValue | Param::NoAssignment);

	_STATE->parameters[BPMSYNCSYNCFACTOR].category = bpm;
	_STATE->parameters[BPMSYNCSYNCFACTOR].name = nameSyncFact;
	_STATE->parameters[BPMSYNCSYNCFACTOR].flags |= Param::NoAssignment;
	_STATE->parameters[BPMSYNCSYNCFACTOR].type = ParameterType_bool;
	_STATE->parameters[BPMSYNCSYNCFACTOR].initvalue = 2.;
	_STATE->parameters[BPMSYNCSYNCFACTOR].setFlag(Param::NoAssignment, true);

	_STATE->parameters[BPMSYNC].category = bpm;
	_STATE->parameters[BPMSYNC].min = BPMSyncTarget::minBPM;
	_STATE->parameters[BPMSYNC].max = BPMSyncTarget::maxBPM;
	_STATE->parameters[BPMSYNC].name = "BPM";
	_STATE->parameters[BPMSYNC].valuename = "BPM";
	_STATE->parameters[BPMSYNC].type = ParameterType_double;
	_STATE->parameters[BPMSYNC].initvalue = 60 * 172;
	_STATE->parameters[BPMSYNC].progress = 1.;
	_STATE->parameters[BPMSYNC].digits = 3;
	_STATE->parameters[BPMSYNC].flags |= Param::MidiParam;

	static constexpr const char* names[] = { "COPY", "CUT", "INSERT", "PASTE", "FADE IN",
											"FADE OUT", "APPLY GRAINENV",
											"INSERT SILENCE", "REVERSE", "MAXIMIZE", "UNDO/REDO",
											"LOOPTO1", "LOOPTO2", "LOOPTO3", "LOOPTO4", "RECTO1",
											"RECTO2", "RECTO3", "RECTO4", "LOOPTODISK" };
	for (int32_t i = 0; i < ARRAY_LEN(names); i++) {
		_STATE->parameters[EDITORCOPY + i].name = names[i];
		_STATE->parameters[EDITORCOPY + i].flags |= Param::MidiParam;
		_STATE->parameters[EDITORCOPY + i].type = ParameterType_bool;
	}

	const char* ng = "GRAIN NOISE";
	const char* noise = "NOISE";

	_STATE->parameters[NOISEMONOGAIN].category = noise;
	_STATE->parameters[NOISEMONOGAIN].min = -60;
	_STATE->parameters[NOISEMONOGAIN].max = 60;
	_STATE->parameters[NOISEMONOGAIN].name = "GAIN";
	_STATE->parameters[NOISEMONOGAIN].valuename = "dB";
	_STATE->parameters[NOISEMONOGAIN].type = ParameterType_double;
	_STATE->parameters[NOISEMONOGAIN].digits = 0;
	_STATE->parameters[NOISEMONOGAIN].initvalue = LOG10D20F(1.0);
	_STATE->parameters[NOISEMONOGAIN].progress = 1;
	_STATE->parameters[NOISEMONOGAIN].flags |= Param::MidiParam;

	_STATE->parameters[NOISEMONOMIX].category = noise;
	_STATE->parameters[NOISEMONOMIX].min = 0.;
	_STATE->parameters[NOISEMONOMIX].max = 1.;
	_STATE->parameters[NOISEMONOMIX].name = "MIX";
	_STATE->parameters[NOISEMONOMIX].valuename = " ";
	_STATE->parameters[NOISEMONOMIX].type = ParameterType_double;
	_STATE->parameters[NOISEMONOMIX].digits = 2;
	_STATE->parameters[NOISEMONOMIX].initvalue = .5;
	_STATE->parameters[NOISEMONOMIX].progress = .05;
	_STATE->parameters[NOISEMONOMIX].flags |= Param::MidiParam;

	_STATE->parameters[NOISEMONOTYPE].category = noise;
	_STATE->parameters[NOISEMONOTYPE].name = "TYPE";
	_STATE->parameters[NOISEMONOTYPE].valuename = " ";
	_STATE->parameters[NOISEMONOTYPE].type = ParameterType_enum;
	_STATE->parameters[NOISEMONOTYPE].flags |= Param::MidiParam;
	_STATE->parameters[NOISEMONOTYPE].names = noise_types;

	_STATE->parameters[NOISEGRAINGAIN].category = ng;
	_STATE->parameters[NOISEGRAINGAIN].min = -60;
	_STATE->parameters[NOISEGRAINGAIN].max = 60;
	_STATE->parameters[NOISEGRAINGAIN].name = "GAIN";
	_STATE->parameters[NOISEGRAINGAIN].valuename = "dB";
	_STATE->parameters[NOISEGRAINGAIN].type = ParameterType_double;
	_STATE->parameters[NOISEGRAINGAIN].digits = 0;
	_STATE->parameters[NOISEGRAINGAIN].initvalue = LOG10D20F(1.0);
	_STATE->parameters[NOISEGRAINGAIN].progress = 1;
	_STATE->parameters[NOISEGRAINGAIN].flags |= Param::MidiParam;

	_STATE->parameters[NOISEGRAINMIX].category = ng;
	_STATE->parameters[NOISEGRAINMIX].min = 0.;
	_STATE->parameters[NOISEGRAINMIX].max = 1.;
	_STATE->parameters[NOISEGRAINMIX].name = "MIX";
	_STATE->parameters[NOISEGRAINMIX].valuename = " ";
	_STATE->parameters[NOISEGRAINMIX].type = ParameterType_double;
	_STATE->parameters[NOISEGRAINMIX].digits = 2;
	_STATE->parameters[NOISEGRAINMIX].initvalue = .5;
	_STATE->parameters[NOISEGRAINMIX].progress = .05;
	_STATE->parameters[NOISEGRAINMIX].flags |= Param::MidiParam;

	_STATE->parameters[NOISEGRAINTYPE].category = ng;
	_STATE->parameters[NOISEGRAINTYPE].name = "TYPE";
	_STATE->parameters[NOISEGRAINTYPE].valuename = " ";
	_STATE->parameters[NOISEGRAINTYPE].type = ParameterType_enum;
	_STATE->parameters[NOISEGRAINTYPE].flags |= Param::MidiParam;
	_STATE->parameters[NOISEGRAINTYPE].names = noise_types;

	_STATE->parameters[ENVPHASE].category = grainParams;
	_STATE->parameters[ENVPHASE].min = 0.0;
	_STATE->parameters[ENVPHASE].max = 1.0;
	_STATE->parameters[ENVPHASE].name = "ENV PHASE";
	_STATE->parameters[ENVPHASE].valuename = " ";
	_STATE->parameters[ENVPHASE].type = ParameterType_double;
	_STATE->parameters[ENVPHASE].initvalue = 0;
	_STATE->parameters[ENVPHASE].progress = .05;
	_STATE->parameters[ENVPHASE].digits = 2;
	_STATE->parameters[ENVPHASE].flags |= Param::MidiParam;

	


	_STATE->parameters[SEQSYNCDAWTRANSPORT].flags |= Param::MidiParam;
	_STATE->parameters[LOOPSYNCDAWTRANSPORT].flags |= Param::MidiParam;

	_STATE->parameters[SEQSYNCDAWRESETONSTART].name = "RESET ON START";
	_STATE->parameters[LFO1SYNCDAWTRANSPORT].name = _STATE->parameters[LFO2SYNCDAWTRANSPORT].name = _STATE->parameters[LFO3SYNCDAWTRANSPORT].name = _STATE->parameters[SEQSYNCDAWTRANSPORT].name = _STATE->parameters[LOOPSYNCDAWTRANSPORT].name = "SYNC HOST TRANSPORT";
	_STATE->parameters[LFO1SYNCDAWTRANSPORT].flags |= Param::MidiParam;
	_STATE->parameters[LFO2SYNCDAWTRANSPORT].flags |= Param::MidiParam;
	_STATE->parameters[LFO3SYNCDAWTRANSPORT].flags |= Param::MidiParam;
	const MYFLOAT OCTAVE_STEP = 1.0f / 12.0f / 8.0f; // semitone over 8 octave range

	_STATE->parameters[LFO1SYNCDAWTIMING].min = 0.;
	_STATE->parameters[LFO1SYNCDAWTIMING].max = 1.;
	_STATE->parameters[LFO1SYNCDAWTIMING].name = "CYCLESPERBAR";
	_STATE->parameters[LFO1SYNCDAWTIMING].valuename = "CYCLES";
	_STATE->parameters[LFO1SYNCDAWTIMING].type = ParameterType_double;
	_STATE->parameters[LFO1SYNCDAWTIMING].digits = 3;
	_STATE->parameters[LFO1SYNCDAWTIMING].initvalue = 0.5;
	_STATE->parameters[LFO1SYNCDAWTIMING].progress = OCTAVE_STEP;
	_STATE->parameters[LFO1SYNCDAWTIMING].flags |= Param::MidiParam;
	_STATE->parameters[LFO1SYNCDAWTIMING].paramCurve = Param::ParamCurve::Octave;

	_STATE->parameters[LOOPSYNCDAWBEATS] = _STATE->parameters[LFO2SYNCDAWTIMING] = _STATE->parameters[LFO3SYNCDAWTIMING] = _STATE->parameters[LFO1SYNCDAWTIMING];
	_STATE->parameters[LOOPSYNCDAWBEATS].name = "LOOPSPERBAR";
	_STATE->parameters[LOOPSYNCDAWBEATS].valuename = "LOOPS";

	_STATE->parameters[INPUT_GAIN_DAW].min = -60;
	_STATE->parameters[INPUT_GAIN_DAW].max = 60;
	_STATE->parameters[INPUT_GAIN_DAW].name = "GAIN";
	_STATE->parameters[INPUT_GAIN_DAW].valuename = "dB";
	_STATE->parameters[INPUT_GAIN_DAW].type = ParameterType_double;
	_STATE->parameters[INPUT_GAIN_DAW].digits = 0;
	_STATE->parameters[INPUT_GAIN_DAW].initvalue = -60;
	_STATE->parameters[INPUT_GAIN_DAW].progress = 1;
	_STATE->parameters[INPUT_GAIN_DAW].flags |= Param::MidiParam;

	_STATE->parameters[GRAININPUT_GAIN_DAW].min = -60;
	_STATE->parameters[GRAININPUT_GAIN_DAW].max = 60;
	_STATE->parameters[GRAININPUT_GAIN_DAW].name = _DATA->isRunningAsPlugin ? "GAIN EXT" : "GAIN MIC";
	_STATE->parameters[GRAININPUT_GAIN_DAW].valuename = "dB";
	_STATE->parameters[GRAININPUT_GAIN_DAW].type = ParameterType_double;
	_STATE->parameters[GRAININPUT_GAIN_DAW].digits = 0;
	_STATE->parameters[GRAININPUT_GAIN_DAW].initvalue = -60;
	_STATE->parameters[GRAININPUT_GAIN_DAW].progress = 1;
	_STATE->parameters[GRAININPUT_GAIN_DAW].flags |= Param::MidiParam;

	_STATE->parameters[INPUT_GAIN_SAMPLER].min = -60;
	_STATE->parameters[INPUT_GAIN_SAMPLER].max = 60;
	_STATE->parameters[INPUT_GAIN_SAMPLER].name = "GAIN SAMPLER";
	_STATE->parameters[INPUT_GAIN_SAMPLER].valuename = "dB";
	_STATE->parameters[INPUT_GAIN_SAMPLER].type = ParameterType_double;
	_STATE->parameters[INPUT_GAIN_SAMPLER].digits = 0;
	_STATE->parameters[INPUT_GAIN_SAMPLER].initvalue = 0;
	_STATE->parameters[INPUT_GAIN_SAMPLER].progress = 1;
	_STATE->parameters[INPUT_GAIN_SAMPLER].flags |= Param::MidiParam;


	for (int i = 0; i < 8; i++) {
		_STATE->parameters[LOOPSTART0 + i * 5].type = ParameterType_double;
		_STATE->parameters[LOOPSTART0 + i * 5].initvalue = 0;

		_STATE->parameters[LOOPSTOP0 + i * 5].type = ParameterType_double;
		_STATE->parameters[LOOPSTOP0 + i * 5].initvalue = 0;

		_STATE->parameters[LOOPPOS0 + i * 5].type = ParameterType_double;
		_STATE->parameters[LOOPPOS0 + i * 5].initvalue = 0;

		_STATE->parameters[LOOPTYPE0 + i * 5].type = ParameterType_double;
		_STATE->parameters[LOOPTYPE0 + i * 5].initvalue = NO_BOUNCE;
		_STATE->parameters[LOOPTYPE0 + i * 5].flags |= Param::CastInt;


		_STATE->parameters[LOOPDIR0 + i * 5].type = ParameterType_double;
		_STATE->parameters[LOOPDIR0 + i * 5].initvalue = 1.;


		_STATE->parameters[WAVEFORMPOS01 + i * 2].type = ParameterType_double;
		_STATE->parameters[WAVEFORMPOS01 + i * 2].initvalue = 0;

		_STATE->parameters[WAVEFORMZOOM01 + i * 2].type = ParameterType_double;
		_STATE->parameters[WAVEFORMZOOM01 + i * 2].initvalue = 1.;


		_STATE->parameters[REDOBUTTON].type = ParameterType_bool;
		_STATE->parameters[REDOBUTTON].flags |= (Param::MidiParam | Param::NoAssignment | Param::NoValue);
		_STATE->parameters[REDOBUTTON].name = "REDO";

		_STATE->parameters[UNDOBUTTON].type = ParameterType_bool;
		_STATE->parameters[UNDOBUTTON].flags |= (Param::MidiParam | Param::NoAssignment | Param::NoValue);
		_STATE->parameters[UNDOBUTTON].name = "REDO";

	}
	constexpr ParameterNum onChangeParams[] = { 
		LFO1NSEGS,
		LFO2NSEGS,
		LFO3NSEGS,
		LFO1JOIN,
		LFO2JOIN,
		LFO3JOIN,
		LFO1SPACE,
		LFO2SPACE,
		LFO3SPACE,
		LFO1DEST,
		LFO2DEST,
		LFO3DEST,
		ASLFO,
		GRAINNSEGS,
		GRAINJOIN,
		ASGRAINGEN,
	    ASPV,
		SPECDELMODE,
		ASCROSS,
		ASSTFX,
		ASGRAN,
		ASFX,
		GRAINFILTERJOINENDS,
		SPECDEL2JOINENDS,
		GRAINFILTERNSEGS,
		SPECDEL2NSEGS };
	for (auto val : onChangeParams) {
		_STATE->parameters[val].setFlag(Param::HasOnChange, true);
	}
	constexpr ParameterNum afterChangeParams[] = {
		GRAINENVSPACE,
		GRAINCURVE,
		ENVOUTER,
		ENVOUTER2,
		ENVINNER,
		ENVINNER2,
		AOUTERCYCLES,
		AOUTERCYCLES2,
		AOUTERDEPTH,
		AOUTERDEPTH2,
		GRAINENVINTERPOL,
		SPEED,
		COMPTHR,
		COMPKNEE,
		COMPRATIO,
		STCOMPTHR,
		STCOMPKNEE,
		STCOMPRATIO,
		FOLLOWERDEST,
		ASMDEL,
		SPACEMULTICOMP,
		SPACEDYNEQ,
		GRAINFILTERCURVE,
		SPECDEL2CURVE,
		LFO1EDITFUNC,
		LFO2EDITFUNC,
		LFO3EDITFUNC
	};
	for (auto val : afterChangeParams) {
		_STATE->parameters[val].setFlag(Param::HasAfterChange, true);
	}

	_STATE->parameters[LOOPSTARTFROMEXT].type = ParameterType_double;
	_STATE->parameters[LOOPSTARTFROMEXT].flags |= (Param::MidiParam | Param::NoAssignment);
	_STATE->parameters[LOOPSTARTFROMEXT].category = loop;
	_STATE->parameters[LOOPSTARTFROMEXT].name = "STARTPOS";
	_STATE->parameters[LOOPSTARTFROMEXT].max = 1.0;
	_STATE->parameters[LOOPSTARTFROMEXT].valuename = " ";
	_STATE->parameters[LOOPSTARTFROMEXT].digits = 3;

	_STATE->parameters[LOOPSTOPFROMEXT].type = ParameterType_double;
	_STATE->parameters[LOOPSTOPFROMEXT].flags |= (Param::MidiParam | Param::NoAssignment);
	_STATE->parameters[LOOPSTOPFROMEXT].category = loop;
	_STATE->parameters[LOOPSTOPFROMEXT].name = "STOPPOS";
	_STATE->parameters[LOOPSTOPFROMEXT].max = 1.0;
	_STATE->parameters[LOOPSTOPFROMEXT].valuename = " ";
	_STATE->parameters[LOOPSTOPFROMEXT].digits = 3;

	_STATE->parameters[LOOPPOSFROMEXT].type = ParameterType_double;
	_STATE->parameters[LOOPPOSFROMEXT].flags |= (Param::MidiParam | Param::NoAssignment);
	_STATE->parameters[LOOPPOSFROMEXT].category = loop;
	_STATE->parameters[LOOPPOSFROMEXT].name = "READPOS";
	_STATE->parameters[LOOPPOSFROMEXT].max = 1.0;
	_STATE->parameters[LOOPPOSFROMEXT].valuename = " ";
	_STATE->parameters[LOOPPOSFROMEXT].digits = 3;

#if defined(PLUGIN_MODE) || defined(OS_IOS)
	_STATE->parameters[OFFGRAIN].pluginIndex = _STATE->parameters[OFFFX].pluginIndex = _STATE->parameters[OFFSTEREOFX].pluginIndex = _STATE->parameters[BYPASSGRAINFX].pluginIndex = _STATE->parameters[BYPASSFX].pluginIndex = _STATE->parameters[BYPASSSTEREOFX].pluginIndex = 1;
#endif

}
