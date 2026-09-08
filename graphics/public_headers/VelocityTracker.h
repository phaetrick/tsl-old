#pragma once
//
// Created by pr on 02.08.22.
//

#ifndef GRAINSTORM_VELOCITYTRACKER_H
#define GRAINSTORM_VELOCITYTRACKER_H

#include "logger.h"
#include "Input.h"
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <vector>


#define MAX_POINTERS_TRACKER  16

namespace tsl::graphics {

    class VelocityTracker {
        class TSLVelocityTracker {
        public:
            struct Position {
                float x{}, y{};
            };
            struct Estimator {
                static constexpr size_t MAX_DEGREE = 4;
                // Estimator time base.
                int64_t time{};
                // Polynomial coefficients describing motion in X and Y.
                float xCoeff[MAX_DEGREE + 1]{}, yCoeff[MAX_DEGREE + 1]{};
                // Polynomial degree (number of coefficients), or zero if no information is
                // available.
                uint32_t degree{};

                inline void clear() {
                    time = 0;
                    degree = 0;
                    for (size_t i = 0; i <= MAX_DEGREE; i++) {
                        xCoeff[i] = 0;
                        yCoeff[i] = 0;
                    }
                }
            };


            void clear() {
                for(int i=0;i<HISTORY_SIZE;i++)
                    events[i].pointer_id = events[i].time = -1;
            }

            void addMovement(const InputEvent &event) {
                if (events[mIndex].time != event.time) {
                    // When ACTION_POINTER_DOWN happens, we will first receive ACTION_MOVE with the coordinates
                    // of the existing pointers, and then ACTION_POINTER_DOWN with the coordinates that include
                    // the new pointer. If the eventtimes for both events are identical, just update the data
                    // for this time.
                    // We only compare against the last value, as it is likely that addMovement is called
                    // in chronological order as events occur.
                    mIndex++;
                }
                if (mIndex == HISTORY_SIZE) {
                    mIndex = 0;
                }
                events[mIndex] = event;
            }

            bool getEstimator(uint32_t id, Estimator *outEstimator) const {
                outEstimator->clear();
                // Iterate over movement samples in reverse time order and collect samples.
                float x[HISTORY_SIZE];
                float y[HISTORY_SIZE];
                int64_t time[HISTORY_SIZE];
                size_t m = 0; // number of points that will be used for fitting
                size_t index = mIndex;
                const InputEvent &newestEvent = events[mIndex];
                do {
                    const InputEvent &event = events[index];
                    if (event.pointer_id != id) {
                        break;
                    }
                    int64_t age = newestEvent.time - event.time;
                    if (age > HORIZON) {
                        break;
                    }
                    x[m] = event.x;
                    y[m] = event.y;
                    time[m] = event.time;
                    index = (index == 0 ? HISTORY_SIZE : index) - 1;
                } while (++m < HISTORY_SIZE);
                if (m == 0) {
                    return false; // no data
                }
                outEstimator->xCoeff[0] = 0;
                outEstimator->yCoeff[0] = 0;
                outEstimator->xCoeff[1] = calculateImpulseVelocity(time, x, m);
                outEstimator->yCoeff[1] = calculateImpulseVelocity(time, y, m);
                outEstimator->xCoeff[2] = 0;
                outEstimator->yCoeff[2] = 0;
                outEstimator->time = newestEvent.time;
                outEstimator->degree = 2; // similar results to 2nd degree fit
#if DEBUG_STRATEGY
                LOGD("velocity: (%f, %f)", outEstimator->xCoeff[1], outEstimator->yCoeff[1]);
#endif
                return true;
            }

        private:
            int mIndex{};
            InputEvent events[20]{};

            static constexpr int64_t HORIZON = 100 * 1000000; // 100 ms

            // Number of samples to keep.
            static constexpr size_t HISTORY_SIZE = 20;

            /**
         * Calculate the total impulse provided to the screen and the resulting velocity.
         *
         * The touchscreen is modeled as a physical object.
         * Initial condition is discussed below, but for now suppose that v(t=0) = 0
         *
         * The kinetic energy of the object at the release is E=0.5*m*v^2
         * Then vfinal = sqrt(2E/m). The goal is to calculate E.
         *
         * The kinetic energy at the release is equal to the total work done on the object by the finger.
         * The total work W is the sum of all dW along the path.
         *
        * dW = F*dx, where dx is the piece of path traveled.
         * Force is change of momentum over time, F = dp/dt = m dv/dt.
         * Then substituting:
         * dW = m (dv/dt) * dx = m * v * dv
         *
         * Summing along the path, we get:
         * W = sum(dW) = sum(m * v * dv) = m * sum(v * dv)
         * Since the mass stays constant, the equation for final velocity is:
         * vfinal = sqrt(2*sum(v * dv))
         *
         * Here,
         * dv : change of velocity = (v[i+1]-v[i])
         * dx : change of distance = (x[i+1]-x[i])
         * dt : change of time = (t[i+1]-t[i])
         * v : instantaneous velocity = dx/dt
         *
         * The final formula is:
         * vfinal = sqrt(2) * sqrt(sum((v[i]-v[i-1])*|v[i]|)) for all i
         * The absolute value is needed to properly account for the sign. If the velocity over a
         * particular segment descreases, then this indicates braking, which means that negative
         * work was done. So for two positive, but decreasing, velocities, this contribution would be
         * negative and will cause a smaller final velocity.
         *
         * Initial condition
         * There are two ways to deal with initial condition:
         * 1) Assume that v(0) = 0, which would mean that the screen is initially at rest.
         * This is not entirely accurate. We are only taking the past X ms of touch data, where X is
         * currently equal to 100. However, a touch event that created a fling probably lasted for longer
         * than that, which would mean that the user has already been interacting with the touchscreen
         * and it has probably already been moving.
         * 2) Assume that the touchscreen has already been moving at a certain velocity, calculate this
         * initial velocity and the equivalent energy, and start with this initial energy.
         * Consider an example where we have the following data, consisting of 3 points:
         *                 time: t0, t1, t2
         *                 x   : x0, x1, x2
         *                 v   : 0 , v1, v2
         * Here is what will happen in each of these scenarios:
         * 1) By directly applying the formula above with the v(0) = 0 boundary condition, we will get
         * vfinal = sqrt(2*(|v1|*(v1-v0) + |v2|*(v2-v1))). This can be simplified since v0=0
         * vfinal = sqrt(2*(|v1|*v1 + |v2|*(v2-v1))) = sqrt(2*(v1^2 + |v2|*(v2 - v1)))
         * since velocity is a real number
         * 2) If we treat the screen as already moving, then it must already have an energy (per mass)
         * equal to 1/2*v1^2. Then the initial energy should be 1/2*v1*2, and only the second segment
         * will contribute to the total kinetic energy (since we can effectively consider that v0=v1).
         * This will give the following expression for the final velocity:
         * vfinal = sqrt(2*(1/2*v1^2 + |v2|*(v2-v1)))
         * This analysis can be generalized to an arbitrary number of samples.
         *
         *
         * Comparing the two equations above, we see that the only mathematical difference
         * is the factor of 1/2 in front of the first velocity term.
         * This boundary condition would allow for the "proper" calculation of the case when all of the
         * samples are equally spaced in time and distance, which should suggest a constant velocity.
         *
         * Note that approach 2) is sensitive to the proper ordering of the data in time, since
         * the boundary condition must be applied to the oldest sample to be accurate.
         */
            static float kineticEnergyToVelocity(float work) {
                static constexpr float sqrt2 = 1.41421356237;
                return (work < 0 ? -1.0 : 1.0) * sqrtf(fabsf(work)) * sqrt2;
            }

            static float calculateImpulseVelocity(const int64_t *t, const float *x, size_t count) {
                // The input should be in reversed time order (most recent sample at index i=0)
                // t[i] is in nanoseconds, but due to FP arithmetic, convert to seconds inside this function
                static constexpr float SECONDS_PER_NANO = 1E-9;
                if (count < 2) {
                    return 0; // if 0 or 1 points, velocity is zero
                }
                if (t[1] > t[0]) { // Algorithm will still work, but not perfectly
                    LOGE("Samples provided to calculateImpulseVelocity in the wrong order");
                }
                if (count == 2) { // if 2 points, basic linear calculation
                    if (t[1] == t[0]) {
                        LOGE("Events have identical time stamps t= %lld, setting velocity = 0", t[0]);
                        return 0;
                    }
                    return (x[1] - x[0]) / (SECONDS_PER_NANO * (t[1] - t[0]));
                }
                // Guaranteed to have at least 3 points here
                float work = 0;
                for (size_t i = count - 1;
                     i > 0; i--) { // start with the oldest sample and go forward in time
                    if (t[i] == t[i - 1]) {
                        LOGE("Events have identical time stamps t=%ld, skipping sample", t[i]);
                        continue;
                    }
                    float vprev = kineticEnergyToVelocity(work); // v[i-1]
                    float vcurr = (x[i] - x[i - 1]) / (SECONDS_PER_NANO * (t[i] - t[i - 1])); // v[i]
                    work += (vcurr - vprev) * fabsf(vcurr);
                    if (i == count - 1) {
                        work *= 0.5; // initial condition, case 2) above
                    }
                }
                return kineticEnergyToVelocity(work);
            }

        };
    public:
        void clear(){
            for (auto &tmp:trackers) tmp.clear();
        }

        void addMovement(const InputEvent &event) {
            if(event.pointer_id < 0 || event.pointer_id >= MAX_POINTERS_TRACKER)
                return;
            switch (event.action) {
                case ACTION_DOWN:
                    trackers[event.pointer_id].clear();
                case ACTION_MOVE:
                    trackers[event.pointer_id].addMovement(event);
                    return;
                default:
                    break;
            }
        }

        // Gets the velocity of the specified pointer id in position units per second.
        // Returns false and sets the velocity components to zero if there is
        // insufficient movement information for the pointer.
        bool getVelocity(uint32_t id, float *outVx, float *outVy) const {
            TSLVelocityTracker::Estimator estimator;
            if (id >= 0 && id < MAX_POINTERS_TRACKER && trackers[id].getEstimator(id, &estimator) && estimator.degree >= 1) {
                *outVx = estimator.xCoeff[1];
                *outVy = estimator.yCoeff[1];
                return true;
            }
            *outVx = 0;
            *outVy = 0;
            return false;
        }

    private:
        TSLVelocityTracker trackers[MAX_POINTERS_TRACKER]{};
    };

    class Scroller {
    public:
        Scroller(float ppi, bool flywheel = false) {
            mFinished = true;
            mDeceleration = computeDeceleration(SCROLL_FRICTION, ppi);
            mFlywheel = flywheel;
            mPhysicalCoeff = computeDeceleration(0.84f, ppi); // look and feel tuning
            setup();
        }

        /**
         * The amount of friction applied to flings. The default value
         * is {@link ViewConfiguration#getScrollFriction}.
         *
         * @param friction A scalar dimension-less value representing the coefficient of
         *         friction.
         *
        void setFriction(float friction) {
            mDeceleration = computeDeceleration(friction);
            mFlingFriction = friction;
        }
         */
        /**
         *
         * Returns whether the scroller has finished scrolling.
         *
         * @return True if the scroller has finished scrolling, false otherwise.
         */
        bool isFinished() {
            return mFinished;
        }

        /**
         * Force the finished field to a particular value.
         *
         * @param finished The new finished value.
         */
        void forceFinished(bool finished) {
            mFinished = finished;
        }

        /**
         * Returns how long the scroll event will take, in milliseconds.
         *
         * @return The duration of the scroll in milliseconds.
         */
        int getDuration() const {
            return mDuration;
        }

        /**
         * Returns the current X offset in the scroll.
         *
         * @return The new X offset as an absolute distance from the origin.
         */
        int getCurrX() const {
            return mCurrX;
        }

        /**
         * Returns the current Y offset in the scroll.
         *
         * @return The new Y offset as an absolute distance from the origin.
         */
        int getCurrY() const {
            return mCurrY;
        }

        /**
         * Returns the current velocity.
         *
         * @return The original velocity less the deceleration. Result may be
         * negative.
         */
        float getCurrVelocity() {
            return mMode == FLING_MODE ?
                   mCurrVelocity : mVelocity - mDeceleration * timePassed() / 2000.0f;
        }

        /**
         * Returns the start X offset in the scroll.
         *
         * @return The start X offset as an absolute distance from the origin.
         */
        int getStartX() const {
            return mStartX;
        }

        /**
         * Returns the start Y offset in the scroll.
         *
         * @return The start Y offset as an absolute distance from the origin.
         */
        int getStartY() const {
            return mStartY;
        }

        /**
         * Returns where the scroll will end. Valid only for "fling" scrolls.
         *
         * @return The final X offset as an absolute distance from the origin.
         */
        int getFinalX() const {
            return mFinalX;
        }

        /**
         * Returns where the scroll will end. Valid only for "fling" scrolls.
         *
         * @return The final Y offset as an absolute distance from the origin.
         */
        int getFinalY() const {
            return mFinalY;
        }

        /**
         * Call this when you want to know the new location.  If it returns true,
         * the animation is not yet finished.
         */
        bool computeScrollOffset() {
            if (mFinished) {
                return false;
            }
            int timePassed = (int) (tsl::time::millisecondsSinceEpoch() - mStartTime);

            if (timePassed < mDuration) {
                switch (mMode) {
                    case SCROLL_MODE: {
                        const float x = getInterpolation(
                                timePassed * mDurationReciprocal);
                        mCurrX = mStartX + std::round(x * mDeltaX);
                        mCurrY = mStartY + std::round(x * mDeltaY);
                        break;
                    }
                    case FLING_MODE: {
                        const float t = (float) timePassed / mDuration;
                        const int index = (int) (NB_SAMPLES * t);
                        float distanceCoef = 1.f;
                        float velocityCoef = 0.f;
                        if (index < NB_SAMPLES) {
                            const float t_inf = (float) index / NB_SAMPLES;
                            const float t_sup = (float) (index + 1) / NB_SAMPLES;
                            const float d_inf = SPLINE_POSITION[index];
                            const float d_sup = SPLINE_POSITION[index + 1];
                            velocityCoef = (d_sup - d_inf) / (t_sup - t_inf);
                            distanceCoef = d_inf + (t - t_inf) * velocityCoef;
                        }
                        mCurrVelocity = velocityCoef * mDistance / mDuration * 1000.0f;

                        mCurrX = mStartX + std::round(distanceCoef * (mFinalX - mStartX));
                        // Pin to mMinX <= mCurrX <= mMaxX
                        mCurrX = std::min(mCurrX, mMaxX);
                        mCurrX = std::max(mCurrX, mMinX);

                        mCurrY = mStartY + std::round(distanceCoef * (mFinalY - mStartY));
                        // Pin to mMinY <= mCurrY <= mMaxY
                        mCurrY = std::min(mCurrY, mMaxY);
                        mCurrY = std::max(mCurrY, mMinY);
                        if (mCurrX == mFinalX && mCurrY == mFinalY) {
                            mFinished = true;
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else {
                mCurrX = mFinalX;
                mCurrY = mFinalY;
                mFinished = true;
            }
            return true;
        }

        /**
         * Start scrolling by providing a starting point and the distance to travel.
         * The scroll will use the default value of 250 milliseconds for the
         * duration.
         *
         * @param startX Starting horizontal scroll offset in pixels. Positive
         *        numbers will scroll the content to the left.
         * @param startY Starting vertical scroll offset in pixels. Positive numbers
         *        will scroll the content up.
         * @param dx Horizontal distance to travel. Positive numbers will scroll the
         *        content to the left.
         * @param dy Vertical distance to travel. Positive numbers will scroll the
         *        content up.
         */
        void startScroll(int startX, int startY, int dx, int dy) {
            startScroll(startX, startY, dx, dy, DEFAULT_DURATION);
        }

        /**
         * Start scrolling by providing a starting point, the distance to travel,
         * and the duration of the scroll.
         *
         * @param startX Starting horizontal scroll offset in pixels. Positive
         *        numbers will scroll the content to the left.
         * @param startY Starting vertical scroll offset in pixels. Positive numbers
         *        will scroll the content up.
         * @param dx Horizontal distance to travel. Positive numbers will scroll the
         *        content to the left.
         * @param dy Vertical distance to travel. Positive numbers will scroll the
         *        content up.
         * @param duration Duration of the scroll in milliseconds.
         */
        void startScroll(int startX, int startY, int dx, int dy, int duration) {
            mMode = SCROLL_MODE;
            mFinished = false;
            mDuration = duration;
            mStartTime = tsl::time::millisecondsSinceEpoch();
            mStartX = startX;
            mStartY = startY;
            mFinalX = startX + dx;
            mFinalY = startY + dy;
            mDeltaX = dx;
            mDeltaY = dy;
            mDurationReciprocal = 1.0f / (float) mDuration;
        }

        /**
         * Start scrolling based on a fling gesture. The distance travelled will
         * depend on the initial velocity of the fling.
         *
         * @param startX Starting point of the scroll (X)
         * @param startY Starting point of the scroll (Y)
         * @param velocityX Initial velocity of the fling (X) measured in pixels per
         *        second.
         * @param velocityY Initial velocity of the fling (Y) measured in pixels per
         *        second
         * @param minX Minimum X value. The scroller will not scroll past this
         *        point.
         * @param maxX Maximum X value. The scroller will not scroll past this
         *        point.
         * @param minY Minimum Y value. The scroller will not scroll past this
         *        point.
         * @param maxY Maximum Y value. The scroller will not scroll past this
         *        point.
         */
        void fling(int startX, int startY, int velocityX, int velocityY,
                   int minX, int maxX, int minY, int maxY) {
            // Continue a scroll or fling in progress
            if (mFlywheel && !mFinished) {
                float oldVel = getCurrVelocity();
                float dx = (float) (mFinalX - mStartX);
                float dy = (float) (mFinalY - mStartY);
                float hyp = (float) std::hypot(dx, dy);
                float ndx = dx / hyp;
                float ndy = dy / hyp;
                float oldVelocityX = ndx * oldVel;
                float oldVelocityY = ndy * oldVel;
                if (std::abs(velocityX) == std::abs(oldVelocityX) &&
                    std::abs(velocityY) == std::abs(oldVelocityY)) {
                    velocityX += oldVelocityX;
                    velocityY += oldVelocityY;
                }
            }
            mMode = FLING_MODE;
            mFinished = false;
            float velocity = (float) std::hypot(velocityX, velocityY);

            mVelocity = velocity;
            mDuration = getSplineFlingDuration(velocity);
            mStartTime = tsl::time::millisecondsSinceEpoch();
            mStartX = startX;
            mStartY = startY;
            float coeffX = velocity == 0 ? 1.0f : velocityX / velocity;
            float coeffY = velocity == 0 ? 1.0f : velocityY / velocity;
            double totalDistance = getSplineFlingDistance(velocity);
            mDistance = (int) (totalDistance * std::abs(velocity));

            mMinX = minX;
            mMaxX = maxX;
            mMinY = minY;
            mMaxY = maxY;
            mFinalX = startX + (int) std::round(totalDistance * coeffX);
            // Pin to mMinX <= mFinalX <= mMaxX
            mFinalX = std::min(mFinalX, mMaxX);
            mFinalX = std::max(mFinalX, mMinX);

            mFinalY = startY + (int) std::round(totalDistance * coeffY);
            // Pin to mMinY <= mFinalY <= mMaxY
            mFinalY = std::min(mFinalY, mMaxY);
            mFinalY = std::max(mFinalY, mMinY);
        }

        /**
         * Stops the animation. Contrary to {@link #forceFinished(boolean)},
         * aborting the animating cause the scroller to move to the final x and y
         * position
         *
         * @see #forceFinished(boolean)
         */
        void abortAnimation() {
            mCurrX = mFinalX;
            mCurrY = mFinalY;
            mFinished = true;
        }

        /**
         * Extend the scroll animation. This allows a running animation to scroll
         * further and longer, when used with {@link #setFinalX(int)} or {@link #setFinalY(int)}.
         *
         * @param extend Additional time to scroll in milliseconds.
         * @see #setFinalX(int)
         * @see #setFinalY(int)
         */
        void extendDuration(int extend) {
            int passed = timePassed();
            mDuration = passed + extend;
            mDurationReciprocal = 1.0f / mDuration;
            mFinished = false;
        }

        /**
         * Returns the time elapsed since the beginning of the scrolling.
         *
         * @return The elapsed time in milliseconds.
         */
        int timePassed() {
            return (int) (tsl::time::millisecondsSinceEpoch() - mStartTime);
        }

        /**
         * Sets the final position (X) for this scroller.
         *
         * @param newX The new X offset as an absolute distance from the origin.
         * @see #extendDuration(int)
         * @see #setFinalY(int)
         */
        void setFinalX(int newX) {
            mFinalX = newX;
            mDeltaX = mFinalX - mStartX;
            mFinished = false;
        }

        /**
         * Sets the final position (Y) for this scroller.
         *
         * @param newY The new Y offset as an absolute distance from the origin.
         * @see #extendDuration(int)
         * @see #setFinalX(int)
         */
        void setFinalY(int newY) {
            mFinalY = newY;
            mDeltaY = mFinalY - mStartY;
            mFinished = false;
        }

        void setup() {
            VISCOUS_FLUID_NORMALIZE = 1.0f / viscousFluid(1.0f);
            VISCOUS_FLUID_OFFSET = 1.0f - VISCOUS_FLUID_NORMALIZE * viscousFluid(1.0f);
            DECELERATION_RATE = (float) (std::log(0.78) / std::log(0.9));
            float x_min = 0.0f;
            float y_min = 0.0f;
            for (int i = 0; i < NB_SAMPLES; i++) {
                const float alpha = (float) i / NB_SAMPLES;
                float x_max = 1.0f;
                float x, tx, coef;
                while (true) {
                    x = x_min + (x_max - x_min) / 2.0f;
                    coef = 3.0f * x * (1.0f - x);
                    tx = coef * ((1.0f - x) * P1 + x * P2) + x * x * x;
                    if (std::abs(tx - alpha) < 1E-5) break;
                    if (tx > alpha) x_max = x;
                    else x_min = x;
                }
                SPLINE_POSITION[i] = coef * ((1.0f - x) * START_TENSION + x) + x * x * x;
                float y_max = 1.0f;
                float y, dy;
                while (true) {
                    y = y_min + (y_max - y_min) / 2.0f;
                    coef = 3.0f * y * (1.0f - y);
                    dy = coef * ((1.0f - y) * START_TENSION + y) + y * y * y;
                    if (std::abs(dy - alpha) < 1E-5) break;
                    if (dy > alpha) y_max = y;
                    else y_min = y;
                }
                SPLINE_TIME[i] = coef * ((1.0f - y) * P1 + y * P2) + y * y * y;
            }
            SPLINE_POSITION[NB_SAMPLES] = SPLINE_TIME[NB_SAMPLES] = 1.0f;
        }

    private:
        int mMode{};
        int mStartX{};
        int mStartY{};
        int mFinalX{};
        int mFinalY{};
        int mMinX{};
        int mMaxX{};
        int mMinY{};
        int mMaxY{};
        int mCurrX{};
        int mCurrY{};
        long mStartTime{};
        int mDuration{};
        float mDurationReciprocal{};
        float mDeltaX{};
        float mDeltaY{};
        std::atomic<bool> mFinished;
        bool mFlywheel;
        float mVelocity{};
        float mCurrVelocity{};
        int mDistance{};
        float mFlingFriction = SCROLL_FRICTION;
        float mDeceleration;
        // A context-specific coefficient adjusted to physical values.
        float mPhysicalCoeff;

        static constexpr int DEFAULT_DURATION = 250;
        static constexpr int SCROLL_MODE = 0;
        static constexpr int FLING_MODE = 1;
        static constexpr float INFLEXION = 0.35f; // Tension lines cross at (INFLEXION, 1)
        static constexpr float START_TENSION = 0.5f;
        static constexpr float END_TENSION = 1.0f;
        static constexpr float P1 = START_TENSION * INFLEXION;
        static constexpr float P2 = 1.0f - END_TENSION * (1.0f - INFLEXION);
        static constexpr int NB_SAMPLES = 100;
        static constexpr float SCROLL_FRICTION = 0.015f;
        float SPLINE_POSITION[NB_SAMPLES + 1]{};
        float SPLINE_TIME[NB_SAMPLES + 1]{};
        float DECELERATION_RATE{};
        /** Controls the viscous fluid effect (how much of it). */
        static constexpr float VISCOUS_FLUID_SCALE = 8.0f;
        float VISCOUS_FLUID_NORMALIZE{};
        float VISCOUS_FLUID_OFFSET{};


        static constexpr float computeDeceleration(float friction, float ppi) {
            return 9.80665f //GRAVITY_EARTH   // g (m/s^2)
                   * 39.37f               // inch/meter
                   * ppi                 // pixels per inch
                   * friction;
        }

        bool isScrollingInDirection(float xvel, float yvel) {
            return !mFinished && std::abs(xvel) == std::abs(mFinalX - mStartX) &&
                   std::abs(yvel) == std::abs(mFinalY - mStartY);
        }

        float getInterpolation(float input) {
            const float interpolated = VISCOUS_FLUID_NORMALIZE * viscousFluid(input);
            if (interpolated > 0) {
                return interpolated + VISCOUS_FLUID_OFFSET;
            }
            return interpolated;
        }

        static constexpr float viscousFluid(float x) {
            x *= VISCOUS_FLUID_SCALE;
            if (x < 1.0f) {
                x -= (1.0f - (float) std::exp(-x));
            } else {
                float start = 0.36787944117f;   // 1/e == exp(-1)
                x = 1.0f - (float) std::exp(1.0f - x);
                x = start + x * (1.0f - start);
            }
            return x;
        }
        double getSplineDeceleration(float velocity) {
            return std::log(INFLEXION * std::abs(velocity) / (mFlingFriction * mPhysicalCoeff));
        }

        int getSplineFlingDuration(float velocity) {
            const double l = getSplineDeceleration(velocity);
            const double decelMinusOne = DECELERATION_RATE - 1.0;
            return (int) (1000.0 * std::exp(l / decelMinusOne));
        }

        double getSplineFlingDistance(float velocity) {
            const double l = getSplineDeceleration(velocity);
            const double decelMinusOne = DECELERATION_RATE - 1.0;
            return mFlingFriction * mPhysicalCoeff *
                   std::exp(DECELERATION_RATE / decelMinusOne * l);
        }


    };



} // namespace android

#endif //GRAINSTORM_VELOCITYTRACKER_H
