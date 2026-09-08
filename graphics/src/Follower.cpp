//
// Created by phaet on 05.06.2024.
//

#include "Follower.h"
#include "app.h"

#define FLT_MIN_PLUS          1.175494351e-38         /* min positive value */
#define FLT_MIN_MINUS        -1.175494351e-38         /* min negative value */

Follower::Follower(const Follower &o) : _appState(o._appState), trackid(o.trackid), id{o.id},
                                        min(o.min.load()), max(o.max.load()), att(o.att.load()),
                                        rel(o.rel.load()), envpower(o.envpower.load()),
                                        source(o.source.load()), gain(o.gain.load()) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        attackOld[i] = o.attackOld[i];
        releaseOld[i] = o.releaseOld[i];
        m_fPreGain[i] = o.m_fPreGain[i];
        m_fAttackTime[i] = o.m_fAttackTime[i];
        m_fReleaseTime[i] = o.m_fReleaseTime[i];
        start[i] = o.start[i];
        range[i] = o.range[i];
        env[i] = o.env[i];
    }
}

void Follower::reset() {
    auto &p = _STATE->parameters[id];
    min.store(p.min);
    max.store(p.max);
    // att/rel live on the FOLLOWERATT/FOLLOWERREL Log10 curve (20*log10(ms)),
    // so take the defaults from the params rather than restating them raw.
    att.store(_STATE->parameters[FOLLOWERATT].initvalue);
    rel.store(_STATE->parameters[FOLLOWERREL].initvalue);
    env[0] = env[1] = 0;
    gain.store(0);
    source.store(trackid);
    envpower.store(false);
}

Follower::Follower(tsl::AppState *state, int trackid_, int id_, MYFLOAT min_, MYFLOAT max_,
                   MYFLOAT att_, MYFLOAT rel_, int s) : _appState(state), trackid{trackid_},
                                                        id{id_}, min(min_), max(max_), att(att_),
                                                        rel(rel_), source(s) {
};


bool Follower::prepare(int chan) {
    if (!envpower.load()) {
        return false;
    }
    m_fPreGain[chan] = pow(10., gain.load() * .05);
    auto att_ = att.load(), rel_ = rel.load();
    if (att_ != attackOld[chan]) {
        setAttackTime(chan, att_);
    }
    if (rel_ != releaseOld[chan]) {
        setReleaseTime(chan, rel_);
    }
    auto a = _STATE->parameters[id].toDSP(_STATE->sr, min.load()), b = _STATE->parameters[id].toDSP(
            _STATE->sr, max.load());
    start[chan] = a;
    range[chan] = b - a;
    envpublic[chan].store(env[chan]);
    return true;
}

bool Follower::prepare() {
    if (!envpower.load()) {

        return false;
    }
    m_fPreGain[0] = m_fPreGain[1] = pow(10., gain.load() * .05);
    auto att_ = att.load(), rel_ = rel.load();
    if (att_ != attackOld[0] || att_ != attackOld[1]) {
        setAttackTime(0, att_);
        m_fAttackTime[1] = m_fAttackTime[0];
        attackOld[1] = attackOld[0];
    }
    if (rel_ != releaseOld[0] || rel_ != releaseOld[1]) {
        setReleaseTime(0, rel_);
        m_fReleaseTime[1] = m_fReleaseTime[0];
        releaseOld[1] = releaseOld[0];
    }
    auto a = _STATE->parameters[id].toDSP(_STATE->sr, min.load()), b = _STATE->parameters[id].toDSP(
            _STATE->sr, max.load());
    start[0] = start[1] = a;
    range[0] = range[1] = b - a;
    envpublic[0].store(env[0]);
    envpublic[1].store(env[1]);
    return true;
}

MYFLOAT Follower::detectL(MYFLOAT fInput) {
    fInput = fInput * m_fPreGain[0];
    fInput = fInput * fInput;

    if (fInput > env[0])
        env[0] = m_fAttackTime[0] * (env[0] - fInput) + fInput;
    else
        env[0] = m_fReleaseTime[0] * (env[0] - fInput) + fInput;

    if (env[0] > 0.0 && env[0] < FLT_MIN_PLUS) env[0] = 0;
    if (env[0] < 0.0 && env[0] > FLT_MIN_MINUS) env[0] = 0;

    return start[0] + CLAMP(env[0], 0., 1.) * range[0];
};

MYFLOAT Follower::detectR(MYFLOAT fInput) {
    fInput = fInput * m_fPreGain[1];
    fInput = fInput * fInput;

    if (fInput > env[1])
        env[1] = m_fAttackTime[1] * (env[1] - fInput) + fInput;
    else
        env[1] = m_fReleaseTime[1] * (env[1] - fInput) + fInput;

    if (env[1] > 0.0 && env[1] < FLT_MIN_PLUS) env[1] = 0;
    if (env[1] < 0.0 && env[1] > FLT_MIN_MINUS) env[1] = 0;

    // bound them; can happen when using pre-detector gains of more than 1.0

    return start[1] + CLAMP(env[1], 0., 1.) * range[1];

};


// att/rel are stored on the FOLLOWERATT/FOLLOWERREL Log10 curve, i.e. 20*log10(ms),
// so the knob resolves the fast end finely. Cache the log-domain value -- that is
// what prepare() compares against -- then convert to ms for the coefficient.
void Follower::setAttackTime(int chan, MYFLOAT attack_log) {
    attackOld[chan] = attack_log;
    MYFLOAT attack_in_ms = LOG2NORMAL(attack_log);
    attack_in_ms += 0.25;
    if (attack_in_ms == 0.0)
        m_fAttackTime[chan] = 0;
    else
        m_fAttackTime[chan] = exp(ANALOG_TC / (attack_in_ms * (kRate ? _STATE->ksr : _STATE->sr) * 0.001));
}

void Follower::setReleaseTime(int chan, MYFLOAT release_log) {
    releaseOld[chan] = release_log;
    MYFLOAT release_in_ms = LOG2NORMAL(release_log);
    release_in_ms += 0.25;
    if (release_in_ms == 0.0)
        m_fReleaseTime[chan] = 0;
    else
        m_fReleaseTime[chan] = exp(ANALOG_TC / (release_in_ms * 0.001 * (kRate ? _STATE->ksr : _STATE->sr)));
}

using namespace tsl::graphics;
static constexpr const char *formatvalues[6] = {"%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f"};

void FollowerView::render(void *ctx) {
    perm = true;
    auto c = static_cast<SkCanvas *>(ctx);
    flush(c);

    auto index = _STATE->active_track.load();
    auto dest = static_cast<int>(_STATE->params[index][FOLLOWERDEST].load());
    auto &fol = _STATE->followerMap[index].at(dest);
    auto a = _STATE->parameters[id].toDSP(_STATE->sr,
                                          fol.min.load()), b = _STATE->parameters[id].toDSP(
            _STATE->sr, fol.max.load());
    auto range = b - a;
    auto &param = _STATE->parameters[dest];
    auto fontsize = _STATE->textsize2 * .9f;

    const char *formatvalue = formatvalues[param.digits];

    SkFont font(_STATE->font_normal);


    font.setSize(fontsize);
    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);
    paint.setColor(skcol::text);
    auto valueWidth = param.getDisplayWidthValue(_STATE->sr, _STATE->maxCharWidtht2);


    auto y = starty + (height - _STATE->textsize1) * .5 + _STATE->startyt2;

    float nameWidth = param.getDisplayNameWidth(_STATE->maxCharWidtht2);
    auto iterations = (_STATE->channels == 2 || dest == CHORUSMIX) ? 2 : 1;
    auto start =
            (width / (float) iterations - (valueWidth + nameWidth + _STATE->maxCharWidtht2)) * .5f;
    for (int i = 0; i < iterations; i++) {
        SkRect bounds{};
        char text[100]{};

        auto env = fol.envpublic[i].load();

        const float value = param.toDisplay(_STATE->sr, a + env * range);

        snprintf(text, 100, formatvalue, value);

        int length = strlen(text);
        font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

        float xpos = start + valueWidth - bounds.width();
        //measureTextFixed(width - sxval, View::textsize1, View::font_normal, text, &sxval, &yy,
        //               info->fontsize);

        c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
                          startx + xpos + i * width * .5f,
                          y, font,
                          paint);

        c->drawSimpleText(param.valuename, strlen(param.valuename),
                          SkTextEncoding::kUTF8,
                          startx + start + valueWidth + i * width * .5f + _STATE->maxCharWidtht2, y,
                          font, paint);


    }
}


