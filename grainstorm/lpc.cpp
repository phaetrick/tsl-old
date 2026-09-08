//
// Created by pr on 26.03.19.
//

#include "lpc.h"
#include "defines.h"
#include "ffttools.h"
#include "Convolver.h"
#include "correlation.h"


#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <cstring>
#include "logger.h"
#include "vocoder.h"
#include <cassert>


inline void apply_window(MYFLOAT x[], MYFLOAT y[], MYFLOAT w[], int32_t s) {
    for (int32_t i = 0; i < s; i++)
        y[i] = x[i] * w[i];
}


void preemphasis(MYFLOAT *x, int32_t len, MYFLOAT alpha) {
    //-.95 1.9025 -.95
   // MYFLOAT del1 = x[0], del2 = x[1];
    //for(int32_t i=2;i<len;i++){
      //  MYFLOAT res = -.95f * x[i] + 1.9025f * del1 - .95f * del2;
        //del2 = del1;
        //del1 = x[i];
        //x[i] = res;
   // }
    for (int32_t i = len - 1; i > 0; i--)
        x[i] = x[i] - alpha * x[i - 1];
}


void deemphasis(MYFLOAT *x, int32_t len, MYFLOAT alpha) {
    for (int32_t i = len - 1; i > 0; i--)
        x[i] = alpha * x[i] + (1.0f - alpha) * x[i - 1];
}


static void lpc_apply_welch_window_f(const MYFLOAT *data,
                                     MYFLOAT *w_data) {
    int32_t i;
    MYFLOAT w;
    MYFLOAT c;

    const uint32_t len = LPC_BUFLEN;
    const uint32_t n2 = (len >> 1u);
    c = 2.0f / (len - 1.0f);

    if (len & 1u) {
        for (i = 0; i < n2; i++) {
            w = c - i - 1.0f;
            w = 1.0f - (w * w);
            w_data[i] = data[i] * w;
            w_data[len - 1 - i] = data[len - 1 - i] * w;
        }
        return;
    }

    w_data += n2;
    data += n2;
    for (i = 0; i < n2; i++) {
        w = c - n2 + i;
        w = 1.0f - (w * w);
        w_data[-i - 1] = data[-i - 1] * w;
        w_data[+i] = data[+i] * w;
    }
}


//-----------------------------------------------------------------------------
// name: autocorrelate()
// desc: ...
//-----------------------------------------------------------------------------
void autocorrelate(MYFLOAT *x, MYFLOAT *corr, int32_t order) {
    // refer to pp. 89 for variable name consistency
    for (int32_t n = 0; n <= order; n++) {
        MYFLOAT temp = 0.0;
        for (int32_t i = 0; i < LPC_BUFLEN - n - 1; i++)
            temp += x[i] * x[i + n];
        corr[n] = temp;
    }

    // why are we doing this?
    const MYFLOAT norm = 1.0f / LPC_BUFLEN;
    const int32_t k = LPC_BUFLEN;

    // normalize, we think
    for (int32_t i = 0; i <= order; i++)
        corr[i] *= (k - i) * norm;
}

static int32_t compute_lp_coefficients(const MYFLOAT r[], int order, MYFLOAT coeffs[]) {

    MYFLOAT pe = r[0];
    std::vector<MYFLOAT> pc(order + 1);
    pc[0] = 1.0;

    for (int32_t k = 1; k <= order; k++) {

        MYFLOAT sum = 0;
        for (int32_t i = 1; i <= k; i++)
            sum -= pc[k - i] * r[i];
        pc[k] = sum / pe;

        for (int32_t i = 1; i <= k / 2; i++) {
            MYFLOAT pci = pc[i] + pc[k] * pc[k - i];
            MYFLOAT pcki = pc[k - i] + pc[k] * pc[i];
            pc[i] = pci;
            pc[k - i] = pcki;
        }

        pe = pe * (1.0 - pc[k] * pc[k]);
        if (pe <= 0)
            return 1;
    }
    for (int32_t j = 0; j < order; j++)
        coeffs[j] = (MYFLOAT) (-pc[j + 1]); /* negate FIR filter coeff to get predictor coeff */
    return 0;
}


void lpc_apply_filter(MYFLOAT *y, int32_t len, MYFLOAT *coefs,
                      int32_t order, MYFLOAT power) {
    int32_t i, j;
    MYFLOAT output;

    for (i = 0; i < len; i++) {
        output = y[i];
        for (j = 0; j < order; j++) {
            output += (i - j - 1 < 0 ? 0 : y[i - j - 1] * coefs[j]);
        }
        y[i] = output;
    }
}


static void compute_residue(const MYFLOAT *x, int32_t len, const MYFLOAT *coefs, int order,
                            MYFLOAT *residue) {
    int32_t i, j;
    MYFLOAT tmp;
    std::vector<MYFLOAT> lpc(order);
    for (i = 0; i < order; i++)
        lpc[i] = x[order - i - 1];

    // set the hope size
    int32_t hope_size = len - order;
    // find the MSE
    for (i = order; i < hope_size + order; i++) {
        tmp = 0.0f;
        for (j = 0; j < order; j++) tmp += lpc[j] * coefs[j];
        for (j = order - 1; j > 0; j--) lpc[j] = lpc[j - 1];
        lpc[0] = x[i];
        residue[i] = x[i] - tmp;
    }
}


int32_t lpc_analyze(AutoCorrelation &autoCorr, MYFLOAT *x, MYFLOAT *coefs, int order, MYFLOAT *residue) {

    //MYFLOAT w[LPC_BUFLEN];
    //lpc_apply_welch_window_f(x, w);
    autoCorr.compute(x);

    if (x[0] == 0.0f) {
        return 1;
    }

    if ((compute_lp_coefficients(x, order, coefs)) == 1) {
        return 1;
    }
    if (residue)
        compute_residue(x, LPC_BUFLEN, coefs, order, residue);
    return 0;
}


MYFLOAT
lpc_analyze3(AutoCorrelation &autoCorr, MYFLOAT *x, MYFLOAT *coefs, int32_t order, MYFLOAT *residue) {

    //MYFLOAT w[LPC_BUFLEN];
    //lpc_apply_welch_window_f(x, w);
    MYFLOAT *sig;

    if (residue != nullptr) {
        memcpy(residue, x, LPC_BUFLEN * sizeof(MYFLOAT));
        sig = residue;
    } else sig = x;

    autoCorr.compute(sig);

    if ((compute_lp_coefficients(sig, order, coefs)) == 1) {
        return 0;
    }
    if (residue)
        compute_residue(x, LPC_BUFLEN, coefs, order, residue);
    return 0;
}


int
lpc_analyze2(AutoCorrelation *autoCorr, MYFLOAT *x, MYFLOAT *coefs, int32_t order, MYFLOAT *residue) {

    std::vector<MYFLOAT> corr(order + 1);
    MYFLOAT w[LPC_BUFLEN];
    lpc_apply_welch_window_f(x, w);
    autocorrelate(w, corr.data(), order);
    if (corr[0] == 0.0f) {
        return 1;
    }
    if ((compute_lp_coefficients(corr.data(), order, coefs)) == 1) {
        return 1;
    }
    if (residue)
        compute_residue(x, LPC_BUFLEN, coefs, order, residue);
    return 0;
}


static inline MYFLOAT magc(tsl::complex<MYFLOAT> c) {
    return std::hypot(c.r, c.i);
}

static inline MYFLOAT phsc(tsl::complex<MYFLOAT> c) {
    return atan2(c.i, c.r);
}


static int32_t findzeros(int32_t M, MYFLOAT *a, tsl::complex<MYFLOAT> *zero,
                         MYFLOAT *tmpbuf, int32_t itmax) {
    MYFLOAT u, v, w, k, m, f, fm, fc, xm, ym, xr, yr, xc, yc;
    MYFLOAT dx, dy, term, factor, tmp;
    int32_t n1, i, j, p, iter, pt = 0;
    unsigned char conv;;
    factor = 1.0;
    if (a[0] == 0.f) {
        return 0;
    }
    memcpy(&tmpbuf[1], a, sizeof(MYFLOAT) * (M + 1));
    n1 = M;
    while (n1 > 0) {
        if (a[n1] == 0) {
            zero[pt].r = 0, zero[pt].i = 0;
            pt += 1;
            n1 -= 1;
        } else {
            p = n1 - 1;
            xc = 0, yc = 0;
            fc = a[n1] * a[n1];
            fm = fc;
            xm = 0.0, ym = 0.0;
            dx = pow(fabs(a[M] / a[0]), 1. / n1);
            dy = 0;
            iter = 0;
            conv = 0;
            while (!conv) {
                iter += 1;
                if (iter > itmax) {
                    for (i = 0; i <= M; i++)
                        a[i] = tmpbuf[i + 1];
                    return pt;
                }
                for (i = 1; i <= 4; i++) {
                    u = -dy;
                    dy = dx;
                    dx = u;
                    xr = xc + dx, yr = yc + dy;
                    u = 0, v = 0;
                    k = 2 * xr;
                    m = xr * xr + yr * yr;
                    for (j = 0; j <= p; j++) {
                        w = a[j] + k * u - m * v;
                        v = u;
                        u = w;
                    }

                    tmp = a[n1] + u * xr - m * v;
                    f = tmp * tmp + (u * u * yr * yr);
                    if (f < fm) {
                        xm = xr, ym = yr;
                        fm = f;
                    }
                }
                if (fm < fc) {
                    dx = 1.5f * dx, dy = 1.5f * dy;
                    xc = xm, yc = ym;
                    fc = fm;
                } else {
                    u = .4f * dx - .3f * dy;
                    dy = .4f * dy + .3f * dx;
                    dx = u;
                }
                u = fabs(xc) + fabs(yc);
                term = u + (fabs(dx) + fabs(dy)) * factor;
                if ((u == term) || (fc == 0))
                    conv = 1;
            }
            u = 0.0, v = 0.0;
            k = 2.0f * xc;
            m = xc * xc;
            for (j = 0; j <= p; j++) {
                w = a[j] + k * u - m * v;
                v = u;
                u = w;
            }
            tmp = a[n1] + u * xc - m * v;
            if (tmp * tmp <= fc) {
                u = 0.0;
                for (j = 0; j <= p; j++) {
                    a[j] = u * xc + a[j];
                    u = a[j];
                }
                zero[pt].r = xc, zero[pt].i = 0;
                pt += 1;
            } else {
                u = 0, v = 0;
                k = 2 * xc;
                m = xc * xc + yc * yc;
                p = n1 - 2;
                for (j = 0; j <= p; j++) {
                    a[j] += k * u - m * v;
                    w = a[j];
                    v = u;
                    u = w;
                }
                zero[pt].r = xc, zero[pt].i = yc;
                pt += 1;
                zero[pt].r = xc, zero[pt].i = -yc;
                pt += 1;
            }
            n1 = p;
        }
    }
    for (i = 0; i <= M; i++)
        a[i] = tmpbuf[i + 1];

    return pt;
}

template<typename T>
static tsl::complex<T> *invertfilter(int32_t M, tsl::complex<T> *zr) {
    for (int32_t i = 0; i < M; i++) {
        T pr = zr[i].r;
        T pi = zr[i].i;
        T pow = pr * pr + pi * pi;
        zr[i].r = pr / pow;
        zr[i].i = -pi / pow;
    }
    return zr;
}

template<typename T>
static T *zero2coef(int32_t M, tsl::complex<T> *zr, T *c, T *tmp) {
    int32_t j, k;
    T pr, pi, cr, ci;
    c[0] = 1;
    tmp[0] = 0;
    for (j = 0; j < M; j++) {
        c[j + 1] = 1;
        tmp[j + 1] = 0;
        pr = zr[j].r;
        pi = zr[j].i;
        for (k = j; k >= 0; k--) {
            cr = c[k];
            ci = tmp[k];
            c[k] = -(cr * pr - ci * pi);
            tmp[k] = -(ci * pr + cr * pi);
            if (k > 0) {
                c[k] += c[k - 1];
                tmp[k] += tmp[k - 1];
            }
        }
    }
    pr = c[0];
    for (j = 0; j <= M; j++) c[j] = c[j] / pr;
    return c;
}

LPC::LPC(MYFLOAT sr) : autoCorrelation(LPC_BUFLEN), buzz(sr) {
    for (int32_t i = 0; i < LPC_BUFLEN; i++)
        env[i] = 0.5f - 0.5f * cos(2.0f * PI_P * i / LPC_BUFLEN);
    // allocate LP analysis memory if needed
    N = LPC_BUFLEN;
    maxM = M = LPC_MAX_ORDER;
    r.resize(N, 0);
    pk.resize(N, 0);
    am.resize(N, 0);
    k.resize(M + 1, 0);
    coeffs.resize((M + 1), 0);
    pl.resize(M + 1, {0, 0});
    cf.resize(M + 1, 0);
    tmpmem.resize(M + 1, 0);
    del.resize(M + 1, 0);
    pp.resize(M * 2, 0);
}

void LPC::setOrder(int32_t order) {
    if (order >= maxM)
        M = maxM;
    else
        M = order;

};

void LPC::lpPred(MYFLOAT *x) {
    for (int32_t i = 0; i < N; i++) {
        r[i] = (x[i] = env[i]) + 0.01 * BiRandGab;
    }
    autoCorrelation.compute(r.data());



    if (r[0] == 0) {
        return;
    }

    std::vector<MYFLOAT> pc(M + 1);
    memset(&pc[1], 0, sizeof(MYFLOAT) * M);
    MYFLOAT pe = pc[0] = 1.0;

    for (uint32_t k = 1; k <= M; k++) {
        MYFLOAT sum = 0;
        for (int32_t i = 1; i <= k; i++)
            sum -= pc[k - i] * (r[i] / r[0]);
        pc[k] = sum / pe;

        for (int32_t i = 1; i <= (k >> 1u); i++) {
            MYFLOAT pci = pc[i] + pc[k] * pc[k - i];
            MYFLOAT pcki = pc[k - i] + pc[k] * pc[i];
            pc[i] = pci;
            pc[k - i] = pcki;
        }

        MYFLOAT tmp = pe * (1.0 - pc[k] * pc[k]);
        if (tmp == 0) {
            break;
        } else
            pe = tmp;
    }

    coeffs[0] = std::sqrt(std::abs(pe));
    memcpy(&coeffs[1], &pc[1], sizeof(MYFLOAT) * M);

}

void LPC::compute(MYFLOAT *in, MYFLOAT *ctrl) {


    lpPred(ctrl);
    //MYFLOAT err = SQRT(c[0]);
    MYFLOAT g = coeffs[0];

    if (g > 0) {
        MYFLOAT *cfs = &coeffs[1];
        for (int32_t n = 0; n < N; n++) {
            int32_t tp = rp;
            MYFLOAT y = (MYFLOAT) in[n] * g; /* need to scale input */
            for (int32_t m = 0; m < M; m++) {
                // filter convolution
                y -= cfs[M - m - 1] * del[tp];
                tp = tp != M - 1 ? tp + 1 : 0;
            }
            in[n] = (MYFLOAT) (del[rp] = y);
            rp = rp != M - 1 ? rp + 1 : 0;
        }
    }
}


void LPC::pkpick() {
    int32_t n = 0, t1 = 0, t2 = 0;
    for (int32_t i = 1; i < N; i++) {
        if (r[i] > r[i - 1]) t1 = 1;
        else t1 = 0;
        if (r[i] >= r[i + 1]) t2 = 1;
        else t2 = 0;
        if (t1 && t2) {
            pk[n] = i;
            n += 1;
        }
    }
    for (int32_t i = n; i < N; i++) pk[i] = -1.f;
}


void LPC::pkinterp() {
    for (int32_t i = 0; i < N; i++) {
        int32_t pn = (int) pk[i];
        if (pn > 0) {
            MYFLOAT tmp;
            if (pn != 0) tmp = r[pn - 1];
            else tmp = r[pn + 1];
            MYFLOAT y1 = r[pn] - tmp;
            MYFLOAT y2;
            if (pn < N - 1)
                y2 = r[pn + 1] - tmp;
            else
                y2 = r[pn] - tmp;
            MYFLOAT a = (y2 - 2 * y1) / 2;
            MYFLOAT bb = 1 - y1 / a;
            pk[i] = pn - 1 + bb / 2;
            am[i] = tmp - a * bb * bb / 4;
        } else break;
    }
}


/* autocorrelation CPS
 */
MYFLOAT LPC::lpCps(MYFLOAT sr) {
    int32_t i;
    MYFLOAT mx = 0.f;
    pkpick();
    pkinterp();
    MYFLOAT pmx = pk[0];
    for (i = 0; i < N; i++) {
        if (pk[i] < 0) break;
        if (am[i] > mx) {
            mx = am[i];
            pmx = pk[i];
        }
    }
    return (cps = sr / pmx);
}


#define MAX_ITER 2000

void LPCVocoder3::coef2Pole(const MYFLOAT *c) {
    int32_t M = *_order;
    cf[M] = 1.0;
    for (int32_t i = 0; i < (M + 1) / 2; i++) {
        int32_t j = M - 1 - i;
        cf[i] = c[j];
        cf[j] = c[i];
    }
    //findzeros(M, cf.data(), pl.data(), tmpmem.data(), MAX_ITER);
    invertfilter(M, pl.data());
}

void LPC::pole2Coef() {
    invertfilter(M, pl.data());
    zero2coef(M, pl.data(), cf.data(), tmpmem.data());
}

static int32_t cmpfunc(const void *a, const void *b) {
    MYFLOAT v1 = (phsc(*((tsl::complex<MYFLOAT> *) a)));
    MYFLOAT v2 = (phsc(*((tsl::complex<MYFLOAT> *) b)));
    return (int) ((v1 - v2) * 100000);
}

void LPCVocoder3::coef2Parm() {
    int32_t M = *_order;

    const MYFLOAT tpidsr = TWOPI_F_P / (MYFLOAT) _STATE->sr;
    MYFLOAT Nyq = _STATE->sr * .5f;
    // simple check for new data
    MYFLOAT tmpsum = 0;
    int32_t j = 0;
    for (int32_t i = 0; i < M; i++) tmpsum += cfsmod[i];
    if (tmpsum != sum) {
        coef2Pole(nullptr/*cfsmod*/);
        for (int32_t i = 0; i < M; i++) {
            MYFLOAT pm = magc(pl[i]);
            if (pm >= 1.) {
                if (true) {
                    MYFLOAT pf = phsc(pl[i]);
                    pm = 1 / pm;
                    pl[i].r = pm * cos(pf);
                    pl[i].i = pm * sin(pf);
                } else {
                    pl[i].r /= pm;
                    pl[i].i /= pm;
                }
            }
        }
        qsort(pl.data(), M, sizeof(tsl::complex<MYFLOAT>), cmpfunc);
        std::fill(pp.begin(), pp.end(), 0);
        for (int32_t i = 0; i < M; i++) {
            MYFLOAT pm = magc(pl[i]);
            MYFLOAT pf = phsc(pl[i]) / tpidsr;
            if (isnan(pf)) {
                pp[j] = 0;
                pp[j + 1] = 0;
            } else {
                if (pf > 0 && pf < Nyq && j < M) {
                    pp[j] = pf;
                    pp[j + 1] = -log(pm) * 2 / tpidsr;
                    j += 2;
                }
            }
        }
    }
    resonord = j;
    sum = tmpsum;
}


void LPC::stabiliseAllpole(MYFLOAT *c, int32_t mode) {
    if (mode) {
//        coef2Pole(c);
        for (int32_t i = 0; i < M; i++) {
            MYFLOAT pm = magc(pl[i]);
            if (pm >= 1.) {
                if (mode == 1) {
                    MYFLOAT pf = phsc(pl[i]);
                    pm = 1 / pm;
                    pl[i].r = pm * cos(pf);
                    pl[i].i = pm * sin(pf);
                } else {
                    pl[i].r /= pm;
                    pl[i].i /= pm;
                }
            }
        }
        pole2Coef();
    };
}


/** LP coeffs to Cepstrum
    takes an array c of N size
    and an array b of M+1 size with M all-pole coefficients
    and squared error E in place of coefficient 0 [E,c1,...,cM]
    returns N cepstrum coefficients
*/
MYFLOAT *csoundLPCeps(MYFLOAT *c, MYFLOAT *b,
                    int32_t N, int M) {
    int32_t n, m;
    MYFLOAT s;
    c[0] = -log(b[0]);
    c[1] = b[1];
    for (n = 2; n < N; n++) {
        if (n > M)
            c[n] = 0;
        else {
            s = 0.;
            for (m = 1; m < n; m++)
                s += (m / n) * c[m] * b[n - m];
            c[n] = b[n] - s;
        }
    }
    for (n = 0; n < N; n++) c[n] *= -1;
    return c;
}

/** Cepstrum to LP coeffs
    takes an array c of N size
    and an array b of M+1 size
    returns M lp coefficients and squared error E in place of
    of coefficient 0 [E,c1,...,cM]
*/

MYFLOAT *csoundCepsLP(MYFLOAT *b, MYFLOAT *c,
                    int32_t M, int N) {
    int32_t n, m;
    MYFLOAT s;
    b[0] = 1;
    b[1] = -c[1];
    for (m = 2; m < M + 1; m++) {
        s = 0.;
        for (n = 1; n < m; n++)
            s -= (m - n) * b[n] * c[m - n];
        b[m] = -c[m] + s / m;
    }
    b[0] = exp(c[0]);
    return b;
}


/** Computes real cepstrum in place from a PVS frame
    buf: non-negative spectrum in PVS_AMP_* format
    size: size of buf (N + 2)
    returns: real-valued cepstrum
*/
MYFLOAT *csoundPvs2RealCepstrum(MYFLOAT *buf, int32_t size) {
    int32_t i;
    for (i = 0; i < size - 2; i += 2) {
        buf[i] = log(buf[i]);
        buf[i + 1] = 0;
    }
    // csoundInverseRealFFT(csound, buf, size - 2);
    buf[size - 2] = buf[size - 1] = 0.f;
    return buf;
}

/** Computes magnitude spectrum of a PVS frame from
    a real-valued cepstrum (in-place)
    buf: real-valued cepstrum
    size: size of buf (N)
    returns: PVS_AMP_* frame
*/
MYFLOAT *csoundRealCepstrum2Pvs(MYFLOAT *buf, int32_t size) {
    int32_t i;
    // csoundRealFFT(csound, buf, size);
    for (i = 2; i < size; i += 2) {
        buf[i] = exp(buf[i]);
        buf[i + 1] = 0.f;
    }
    return buf;
}

// pvs <-> lpc
/*
int32_t lpcpvs_init(CSOUND *csound, LPCPVS *p) {
    int32_t N = *p->isiz;
    uint32_t Nbytes = N * sizeof(MYFLOAT);
    if (*p->iwin) {
        FUNC *win = csound->FTnp2Find(csound, p->iwin);
        p->win = win->ftable;
        p->wlen = win->flen;
    } else p->win = nullptr;
    p->M = *p->iord;
    p->N = N;

    if ((N & (N - 1)) != 0)
        return csound->InitError(csound, "input size not power of two\n");

    p->setup = csound->LPsetup(csound, N, p->M);
    if (p->buf.auxp == nullptr || Nbytes > p->buf.size)
        csound->AuxAlloc(csound, Nbytes, &p->buf);
    if (p->cbuf.auxp == nullptr || Nbytes > p->cbuf.size)
        csound->AuxAlloc(csound, Nbytes, &p->cbuf);
    if (p->fftframe.auxp == nullptr || Nbytes > p->fftframe.size)
        csound->AuxAlloc(csound, Nbytes, &p->fftframe);

    p->fout->N = N;
    p->fout->sliding = 0;
    p->fout->NB = 0;
    p->fout->overlap = (int32_t) *p->prd;
    p->fout->winsize = N;
    p->fout->wintype = PVS_WIN_HANN;
    p->fout->format = PVS_AMP_FREQ;

    Nbytes = (N + 2) * sizeof(MYFLOAT);
    if (p->fout->frame.auxp == nullptr || Nbytes > p->fout->frame.size)
        csound->AuxAlloc(csound, Nbytes, &p->fout->frame);

    p->cp = 1;
    p->bp = 0;
    return OK;
}

int32_t lpcpvs(CSOUND *csound, LPCPVS *p) {
    MYFLOAT *buf = (MYFLOAT *) p->buf.auxp;
    MYFLOAT *cbuf = (MYFLOAT *) p->cbuf.auxp;
    MYFLOAT *in = p->in;
    int32_t M = p->M;
    int32_t N = p->N;
    int32_t bp = p->bp, cp = p->cp;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t n, nsmps = CS_KSMPS;
    MYFLOAT *c;

    if (UNLIKELY(early))
        nsmps -= early;

    for (n = offset; n < nsmps; n++) {
        cbuf[bp] = in[n];
        bp = bp != N - 1 ? bp + 1 : 0;
        if (--cp == 0) {
            MYFLOAT k, incr = p->wlen / N, g, sr = csound->GetSr(csound);
            MYFLOAT *fftframe = (MYFLOAT *) p->fftframe.auxp;
            MYFLOAT *pvframe = (MYFLOAT *) p->fout->frame.auxp;
            int32_t j, i;
            for (j = bp, i = 0, k = 0; i < N; j++, i++, k += incr) {
                buf[i] = p->win == nullptr ? cbuf[j % N] : p->win[(int) k] * cbuf[j % N];
            }
            c = csound->LPred(csound, p->setup, buf);
            memset(fftframe, 0, sizeof(MYFLOAT) * N);
            memcpy(&fftframe[1], &c[1], sizeof(MYFLOAT) * M);
            fftframe[0] = 1;
            csound->RealFFT(csound, fftframe, N);
            g = SQRT(c[0]) * csoundLPrms(csound, p->setup);
            MYFLOAT cps = csoundLPcps(csound, p->setup);
            int32_t cpsbin = cps * N / sr;
            for (i = 0; i < N + 2; i += 2) {
                MYFLOAT a = 0., inv;
                int32_t bin = i / 2;
                if (i > 0 && i < N)
                    inv = sqrt(fftframe[i] * fftframe[i] + fftframe[i + 1] * fftframe[i + 1]);
                else if (i == N) inv = fftframe[1];
                else inv = fftframe[0];
                if (inv > 0)
                    a = g / inv;
                else a = g;
                pvframe[i] = (MYFLOAT) a;
                if (bin / cpsbin)
                    pvframe[i + 1] = cps * bin / cpsbin;
                else if ((bin + 1) / cpsbin)
                    pvframe[i + 1] = cps * (bin - 1) / cpsbin;
                else if ((bin - 1) / cpsbin)
                    pvframe[i + 1] = cps * (bin + 1) / cpsbin;

            }
            p->fout->framecount += 1;
            cp = (int32_t) (*p->prd > 1 ? *p->prd : 1);
        }
    }
    p->bp = bp;
    p->cp = cp;
    return OK;
}

int32_t pvscoefs_init(CSOUND *csound, PVSCFS *p) {
    uint32_t Nbytes = (p->fin->N + 2) * sizeof(MYFLOAT);
    uint32_t Mbytes = (p->M + 1) * sizeof(MYFLOAT);
    p->N = p->fin->N;
    p->M = *p->iord;
    p->setup = csound->LPsetup(csound, 0, p->M);
    if (p->buf.auxp == nullptr || Nbytes > p->buf.size)
        csound->AuxAlloc(csound, Nbytes, &p->buf);
    if (p->coef.auxp == nullptr || Mbytes > p->coef.size)
        csound->AuxAlloc(csound, Nbytes, &p->coef);
    tabinit(csound, p->out, p->M);
    p->mod = *p->imod;
    return OK;
}

int32_t pvscoefs(CSOUND *csound, PVSCFS *p) {
    MYFLOAT *c = (MYFLOAT *) p->coef.auxp;
    if (p->fin->framecount > p->framecount) {
        MYFLOAT *buf = (MYFLOAT *) p->buf.auxp;
        int32_t i;
        MYFLOAT pow = 0;
        MYFLOAT *pvframe = (MYFLOAT *) p->fin->frame.auxp;
        memset(buf, 0, sizeof(MYFLOAT) * (p->N + 2));
        for (i = 2; i < p->N; i += 2) buf[i] = pvframe[i];
        buf[0] = pvframe[0];
        buf[p->N] = pvframe[p->N];
        for (i = 0; i < p->N + 2; i += 2) pow += buf[i];
        p->rms = pow / (2 * sqrt(2));
        if (p->rms > 0) {
            memset(c, 0, sizeof(MYFLOAT) * (p->M + 1));
            csoundPvs2RealCepstrum(csound, buf, p->N + 2);
            csoundCepsLP(csound, c, buf, p->M, p->N);
            p->err = sqrt(c[0]);
            c = csoundStabiliseAllpole(csound, p->setup, c, p->mod);
            memcpy(p->out->data, &c[1], p->M * sizeof(MYFLOAT));
        }
        p->framecount = p->fin->framecount;
    }
    *p->kerr = p->err;
    *p->krms = p->rms;
    return OK;
}

// coefficients to filter CF/BW
int32_t coef2parm_init(CSOUND *csound, CF2P *p) {
    p->M = p->in->sizes[0];
    p->setup = csound->LPsetup(csound, 0, p->M);
    tabinit(csound, p->out, p->M);
    p->sum = 0.0;
    return OK;
}

static int32_t cmpfunc(const void *a, const void *b) {
    MYFLOAT v1 = (phsc(*((complex *) a)));
    MYFLOAT v2 = (phsc(*((complex *) b)));
    return (int) ((v1 - v2) * 100000);
}

int32_t coef2parm(CSOUND *csound, CF2P *p) {
    complex *pl;
    MYFLOAT *c = p->in->data, pm, pf, sum = 0.0;
    MYFLOAT *pp = p->out->data, Nyq = csound->esr / 2;
    int32_t i, j;
    // simple check for new data
    for (i = 0; i < p->M; i++) sum += c[i];
    if (sum != p->sum) {
        pl = csoundCoef2Pole(csound, p->setup, c);
        qsort(pl, p->M, sizeof(complex), cmpfunc);
        memset(pp, 0, sizeof(MYFLOAT) * p->M);
        for (i = j = 0; i < p->M; i++) {
            pm = magc(pl[i]);
            pf = phsc(pl[i]) / csound->tpidsr;
            if (isnan(pf)) {
                pp[j] = 0;
                pp[j + 1] = 0;
            } else {
                if (pf > 0 && pf < Nyq && j < p->M) {
                    pp[j] = pf;
                    pp[j + 1] = -log(pm) * 2 / csound->tpidsr;
                    j += 2;
                }
            }
        }
    }
    p->sum = sum;
    return OK;
}


int32_t resonbnk_init(CSOUND *csound, RESONB *p) {
    int32_t scale, siz;
    p->scale = scale = (int32_t) *p->iscl;
    p->ord = p->kparm->sizes[0];
    siz = (p->ord + 1) / 2;
    if (!*p->istor && (p->y1m.auxp == nullptr ||
                       (uint32_t) (siz * sizeof(MYFLOAT)) > p->y1m.size))
        csound->AuxAlloc(csound, (int32_t) (siz * sizeof(MYFLOAT)), &p->y1m);
    if (!*p->istor && (p->y2m.auxp == nullptr ||
                       (uint32_t) (siz * sizeof(MYFLOAT)) > p->y2m.size))
        csound->AuxAlloc(csound, (int32_t) (siz * sizeof(MYFLOAT)), &p->y2m);

    if (!*p->istor && (p->y1o.auxp == nullptr ||
                       (uint32_t) (siz * sizeof(MYFLOAT)) > p->y1o.size))
        csound->AuxAlloc(csound, (int32_t) (siz * sizeof(MYFLOAT)), &p->y1o);
    if (!*p->istor && (p->y2o.auxp == nullptr ||
                       (uint32_t) (siz * sizeof(MYFLOAT)) > p->y2o.size))
        csound->AuxAlloc(csound, (int32_t) (siz * sizeof(MYFLOAT)), &p->y2o);

    if (!*p->istor && (p->y1c.auxp == nullptr ||
                       (uint32_t) (siz * sizeof(MYFLOAT)) > p->y1c.size))
        csound->AuxAlloc(csound, (int32_t) (siz * sizeof(MYFLOAT)), &p->y1c);
    if (!*p->istor && (p->y2c.auxp == nullptr ||
                       (uint32_t) (siz * sizeof(MYFLOAT)) > p->y2c.size))
        csound->AuxAlloc(csound, (int32_t) (siz * sizeof(MYFLOAT)), &p->y2c);

    if (UNLIKELY(scale && scale != 1 && scale != 2)) {
        return csound->InitError(csound, Str("illegal reson iscl value, %f"),
                                 *p->iscl);
    }
    if (!(*p->istor)) {
        memset(p->y1m.auxp, 0, siz * sizeof(MYFLOAT));
        memset(p->y2m.auxp, 0, siz * sizeof(MYFLOAT));
        memset(p->y1o.auxp, 0, siz * sizeof(MYFLOAT));
        memset(p->y2o.auxp, 0, siz * sizeof(MYFLOAT));
        memset(p->y1c.auxp, 0, siz * sizeof(MYFLOAT));
        memset(p->y2c.auxp, 0, siz * sizeof(MYFLOAT));
    }
    p->kcnt = 0;
    return OK;
}

void resonbnk(CSOUND *csound, RESONB *p) {
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t n, nsmps = CS_KSMPS;
    int32_t j, k, ord = p->ord, mod = *p->imod;
    MYFLOAT *ar, *asig;
    MYFLOAT c3p1, c3t4, omc3, c2sqr, cosf, cc2, cc3;
    MYFLOAT *yt1, *yt2, c1 = 1., *c2, *c3, x, *c2o, *c3o;
    MYFLOAT bw, cf;
    MYFLOAT kcnt = p->kcnt, prd = *p->iprd, interp, fmin = *p->kmin, fmax = *p->kmax;


    ar = p->ar;
    asig = p->asig;
    yt1 = (MYFLOAT *) p->y1m.auxp;
    yt2 = (MYFLOAT *) p->y2m.auxp;
    c2o = (MYFLOAT *) p->y1o.auxp;
    c3o = (MYFLOAT *) p->y2o.auxp;
    c2 = (MYFLOAT *) p->y1c.auxp;
    c3 = (MYFLOAT *) p->y2c.auxp;


    for (n = offset; n < nsmps; n++) {
        x = asig[n];
        ar[n] = 0.;
        if (kcnt == prd) kcnt = 0;
        interp = kcnt / prd;
        for (k = j = 0; k < ord; j++, k += 2) {
            if (mod) x = asig[n]; // parallel
            if (kcnt == 0) {
                c3o[j] = c3[j];
                c2o[j] = c2[j];
                cf = p->kparm->data[k];
                bw = p->kparm->data[k + 1];
                if (cf > fmin && cf < fmax) {
                    cosf = cos(cf * (MYFLOAT) (csound->tpidsr));
                    c3[j] = exp(bw * (MYFLOAT) (csound->mtpdsr));
                    c3p1 = c3[j] + 1.0;
                    c3t4 = c3[j] * 4.0;
                    c2[j] = c3t4 * cosf / c3p1;
                }
            }
            cc2 = c2o[j] + (c2[j] - c2o[j]) * interp;
            cc3 = c3o[j] + (c3[j] - c3o[j]) * interp;
            if (p->scale) {
                omc3 = 1.0 - cc3;
                c2sqr = cc2 * cc2;
                c3p1 = cc3 + 1.0;
                if (p->scale == 1)
                    c1 = omc3 * sqrt(1.0 - (c2sqr / (4 * cc3)));
                else if (p->scale == 2)
                    c1 = sqrt((c3p1 * c3p1 - c2sqr) * omc3 / c3p1);
            }
            x = c1 * x + cc2 * yt1[j] - cc3 * yt2[j];
            yt2[j] = yt1[j];
            yt1[j] = x;
            if (mod) ar[n] += x; // parallel
        }
        kcnt += 1;
        if (!mod) ar[n] = x;
    }
    p->kcnt = kcnt;
}
*/