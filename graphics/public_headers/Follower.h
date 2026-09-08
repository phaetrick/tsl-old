//
// Created by phaet on 05.06.2024.
//

#ifndef GRAINSTORM_FOLLOWER_H
#define GRAINSTORM_FOLLOWER_H

#include "defines.h"
#include "view.h"
#include <atomic>
namespace tsl{
struct  AppState;
}
class Follower{
    typedef MYFLOAT (Follower::*FolDetect)(MYFLOAT);
public:
    Follower(const Follower &o);
    Follower(tsl::AppState *state, int trackid_, int id_, MYFLOAT min_, MYFLOAT max_, MYFLOAT att_,  MYFLOAT rel_, int s);
    void reset();
    bool prepare(int chan);
    bool prepare();

    MYFLOAT detectL(MYFLOAT fInput);
    MYFLOAT detectR(MYFLOAT fInput);
    std::atomic<MYFLOAT> min, max, att, rel, gain, envpower{}, source;
    MYFLOAT env[MAX_CHANNELS]{};
    int id{};
    std::atomic<MYFLOAT> envpublic[2]{};
    void setKrate(bool k){kRate = k;}
private:
    bool kRate{false};
    int trackid{};
    double attackOld[MAX_CHANNELS]{-1,-1}, releaseOld[MAX_CHANNELS]{-1,-1};
    MYFLOAT m_fPreGain[MAX_CHANNELS]{};
    MYFLOAT m_fAttackTime[MAX_CHANNELS]{};
    MYFLOAT m_fReleaseTime[MAX_CHANNELS]{};
    MYFLOAT start[MAX_CHANNELS]{}, range[MAX_CHANNELS]{};
    tsl::AppState *_appState;

    // Both take the stored Log10-curve value (20*log10(ms)), not milliseconds.
    void setAttackTime(int chan, MYFLOAT attack_log);

    void setReleaseTime(int chan, MYFLOAT release_log);
};

struct FollowerParams{
    FollowerParams() = default;
    FollowerParams(const Follower &o){
        id = o.id;
        envpower = o.envpower;
        min = o.min.load();
        max = o.max.load();
        att = o.att.load();
        rel = o.rel.load();
        gain = o.gain.load();
        source = o.source.load();
        env[0] = o.env[0];
        env[1] = o.env[1];
    };
    int id;
    MYFLOAT min, max, att, rel, gain;
    MYFLOAT env[MAX_CHANNELS];
    bool envpower;
    int source;
};


namespace tsl{
    namespace graphics{
        class FollowerView : public View{
        public:
            FollowerView(tsl::AppState *appState, const float scalefactor,
                         const int aspect_ratio,
                         const int alignment) : View(appState, scalefactor, aspect_ratio, alignment){};
            void render(void*)override;
        private:

        };
    }
}


#endif //GRAINSTORM_FOLLOWER_H
