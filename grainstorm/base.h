#pragma once
//
// Created by pr on 16.06.20.
//

#ifndef GRAINSTORM_BASE_H
#define GRAINSTORM_BASE_H
#include <defines.h>
#include <logger.h>
#include <tools/queuetsl.h>    // For std::move
#include <atomic>
#include <string>
#include <functional>
#include <memory>
#include <array>      // For std::array
#include <vector>     // For std::vector (for _nodes pool)
#include <utility>    // For std::move

struct TRACK;
class LFO;

namespace tsl {
    struct AppState;
}

class Effect : public std::enable_shared_from_this<Effect>{
public:
    enum{
        GRAINEFFECT = 0,
        MONOEFFECT = 1,
        STEREOEFFECT = 2
    };
    enum{
        STARTING = 0,
        READY = 1,
        UPDATING = 2,
        QUIT = 3
    };
    Effect(TRACK *t, int id, int type, const std::function<void()>& activate_ = nullptr, const std::function<void()>& deactivate_ = nullptr, int chan = 0);
    Effect(TRACK *t, int chan, int id, int type, const std::function<void()>& activate_ = nullptr, const std::function<void()> deactivate_ = nullptr) : Effect(t, id, type, activate_, deactivate_, chan){
    };
    virtual ~Effect()= default;
    virtual void compute(MYFLOAT *in, int32_t size){};
    virtual void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t size){};
    static void Compute(Effect*, MYFLOAT*, int){};
    template<typename T>
    static inline T limit(T in, T min, T max){
        if(in<min)
            return min;
        else if(in > max)
            return max;
        return in;
    }
    bool destroyRequested{};
    bool readyToDestroy{};
    int _id;
    int _type;
    std::function<void()> activate, deactivate;
    tsl::AppState *_appState{};
    const MYFLOAT smoothCoeff;
protected:
    const std::atomic<double> *_bypass{};
    const std::atomic<MYFLOAT> *_gain{}, *_mix{};
    TRACK *_track;
    std::atomic<LFO *> *_lfo{};
    std::atomic<int> _state{STARTING};
    const int _chan;
    const MYFLOAT _fadeconst;
    MYFLOAT _fadeinc;
    MYFLOAT _fade{};
    MYFLOAT _smooth1{}, _smooth2{1.f};

    inline MYFLOAT paramSmooth(const MYFLOAT in, MYFLOAT &smooth) const {
        return smooth = smoothCoeff * (smooth - in) + in;
    }

    // Index into an LFO's buffer for a GRAINEFFECT. A grain effect runs on a
    // grain buffer, not on the audio block the LFO was rendered for, so it
    // cannot walk the LFO per sample: it reads the one value at the block
    // position where this grain was launched (granulate.cpp stores that in
    // step_point_grain). An LFO on a grain param is therefore a per-grain
    // sample-and-hold - the same rate as a per-grain random draw, which is why
    // the two are interchangeable on the MIN/MAX effects. Out of line because
    // base.h only forward-declares TRACK.
    int grainLfoIndex() const;

    inline void sm1(MYFLOAT in){
        _smooth1 = smoothCoeff * (_smooth1 - in) + in;
        if(destroyRequested && _smooth1<= 0.01)
            readyToDestroy = true;
    }

    inline void smt(MYFLOAT in, MYFLOAT &mix){
        mix = _smooth1 > 0.99 ? 1 : (_smooth1<0.01 ? 0 : _smooth1);
        _smooth1 = smoothCoeff * (_smooth1 - in) + in;
        if(destroyRequested && _smooth1<= 0.01)
            readyToDestroy = true;
    }


    inline void sm2(MYFLOAT in){
        _smooth2 = smoothCoeff * (_smooth2 - in) + in;
    }

    inline void smmixgain(MYFLOAT in1, MYFLOAT in2){
        _smooth1 = smoothCoeff * (_smooth1 - in1) + in1;
        _smooth2 = smoothCoeff * (_smooth2 - in2) + in2;
        if(destroyRequested && _smooth1<= 0.01)
            readyToDestroy = true;
    }

    inline void smwetdry(MYFLOAT in1, MYFLOAT in2){
        _smooth1 = smoothCoeff * (_smooth1 - in1) + in1;
        _smooth2 = smoothCoeff * (_smooth2 - in2) + in2;
        if(destroyRequested && _smooth1<= 0.01 && _smooth2 >=.99)
            readyToDestroy = true;
    }

    inline MYFLOAT smmix() const{
        return _smooth1;
    }
    inline MYFLOAT smgain() const{
        return _smooth2;
    }
    inline MYFLOAT smwet() const{
        return _smooth1;
    }
    inline MYFLOAT smdry() const{
        return _smooth2;
    }
};

class GrainEffect{
public:
    virtual MYFLOAT tick(MYFLOAT, int, int) = 0;
};

// --- Fixed-Size Queue Class for Effects ---

template<size_t CAPACITY = 250> // Fixed capacity as a template parameter
class EffectQueue : public tsl::QueueUnsafe<std::shared_ptr<Effect>, CAPACITY> {
private:
    using Base = tsl::QueueUnsafe<std::shared_ptr<Effect>, CAPACITY>;
    using EffectPtr = std::shared_ptr<Effect>;

public:
    int delById(int effect_id) {
        auto temp = this->_first;
        while (temp) {
            if (temp->data && temp->data->_id == effect_id) {
                del(temp); // Call the NodeQEffect* version
                return 1; // Element found and deleted
            }
            temp = temp->next;
        }
        return 0; // Element not found
    }

    // Append an effect to the beginning of the queue (returns 1 on success, 0 if full)
    int append_first(const std::shared_ptr<Effect>& data) {
        if (!data) return 0; // Cannot append null effect
        if (this->isFull()) return 0;

        auto* node_new = this->newNode();
        node_new->data = data;
        node_new->prio = 0; // Default priority

        if (this->_first == nullptr) { // Case: Empty list
            this->_first = this->_last = node_new;
        } else {
            node_new->next = this->_first;
            this->_first->prev = node_new;
            this->_first = node_new;
        }
        return 1;
    }

    // Find an effect by ID and return a shared_ptr to it (nullptr if not found)
    [[nodiscard]] std::shared_ptr<Effect> find(int effect_id) const {
        auto node = this->_first;
        while (node) {
            if (node->data && node->data->_id == effect_id) {
                return node->data; // Found, return a copy of the shared_ptr
            }
            node = node->next;
        }
        return nullptr; // Not found
    }

    // Find an effect by ID and return its position (index) in the queue (-1 if not found)
    [[nodiscard]] int pos(int effect_id) const {
        auto node = this->_first;
        int current_pos = 0;
        while (node) {
            if (node->data && node->data->_id == effect_id) {
                return current_pos;
            }
            current_pos++;
            node = node->next;
        }
        return -1;
    }

    // Improved move_element function with better logic and error handling
    int move_element(int effect_id, int new_pos) {
        // Find the node containing the effect
        auto current = this->_first;
        while (current && (!current->data || current->data->_id != effect_id)) {
            current = current->next;
        }

        // Effect not found
        if (!current) {
            return 0;
        }

        // Handle special case: moving to beginning (new_pos == -1)
        if (new_pos == -1) {
            // If already at the beginning, no need to move
            if (current == this->_first) {
                return 1;
            }

            // Remove from current position
            if (current->prev) {
                current->prev->next = current->next;
            }
            if (current->next) {
                current->next->prev = current->prev;
            }
            if (current == this->_last) {
                this->_last = current->prev;
            }

            // Insert at beginning
            current->prev = nullptr;
            current->next = this->_first;
            if (this->_first) {
                this->_first->prev = current;
            }
            this->_first = current;

            // If list was empty, this is also the last node
            if (!this->_last) {
                this->_last = current;
            }

            return 1;
        }

        // Handle regular position moves
        // First, find the target position
        auto target = this->_first;
        int current_pos = 0;

        // Find the node at position new_pos (or the last node if new_pos is beyond end)
        while (target && current_pos <= new_pos) {
            target = target->next;
            current_pos++;
        }

        // If new_pos is beyond the end, insert at the end
        if (!target) {
            // If already at the end, no need to move
            if (current == this->_last) {
                return 1;
            }

            // Remove from current position
            if (current->prev) {
                current->prev->next = current->next;
            }
            if (current->next) {
                current->next->prev = current->prev;
            }
            if (current == this->_first) {
                this->_first = current->next;
            }

            // Insert at end
            current->next = nullptr;
            current->prev = this->_last;
            if (this->_last) {
                this->_last->next = current;
            }
            this->_last = current;

            // If list was empty, this is also the first node
            if (!this->_first) {
                this->_first = current;
            }

            return 1;
        }

        // If target is the same as current, no need to move
        if (target == current) {
            return 1;
        }

        // Remove current from its position
        if (current->prev) {
            current->prev->next = current->next;
        } else {
            this->_first = current->next;
        }

        if (current->next) {
            current->next->prev = current->prev;
        } else {
            this->_last = current->prev;
        }

        // Insert current before target
        current->prev = target->prev;
        current->next = target;

        if (target->prev) {
            target->prev->next = current;
        } else {
            this->_first = current;
        }

        target->prev = current;

        return 1;
    }

// Alternative implementation that's more straightforward:
    int move_element_simple(int effect_id, int new_pos) {
        // Find the node to move
        auto node_to_move = this->_first;
        while (node_to_move && (!node_to_move->data || node_to_move->data->_id != effect_id)) {
            node_to_move = node_to_move->next;
        }

        if (!node_to_move) {
            return 0; // Effect not found
        }

        // Remove the node from its current position
        if (node_to_move->prev) {
            node_to_move->prev->next = node_to_move->next;
        } else {
            this->_first = node_to_move->next;
        }

        if (node_to_move->next) {
            node_to_move->next->prev = node_to_move->prev;
        } else {
            this->_last = node_to_move->prev;
        }

        // Reset the node's links
        node_to_move->prev = nullptr;
        node_to_move->next = nullptr;

        // Insert at new position
        if (new_pos == -1 || this->_first == nullptr) {
            // Insert at beginning or into empty list
            if (this->_first) {
                this->_first->prev = node_to_move;
                node_to_move->next = this->_first;
            } else {
                this->_last = node_to_move; // Empty list case
            }
            this->_first = node_to_move;
        } else {
            // Find insertion point
            auto current = this->_first;
            int pos = 0;

            // Navigate to the position where we want to insert
            while (current && pos < new_pos) {
                current = current->next;
                pos++;
            }

            if (current) {
                // Insert before current
                node_to_move->prev = current->prev;
                node_to_move->next = current;

                if (current->prev) {
                    current->prev->next = node_to_move;
                } else {
                    this->_first = node_to_move;
                }

                current->prev = node_to_move;
            } else {
                // Insert at end
                if (this->_last) {
                    this->_last->next = node_to_move;
                    node_to_move->prev = this->_last;
                } else {
                    this->_first = node_to_move; // This shouldn't happen but just in case
                }
                this->_last = node_to_move;
            }
        }

        return 1;
    }
    int move_element_new(int effect_id, int new_pos) {
        // Find the node to move
        auto node = this->_first;
        while (node && (!node->data || node->data->_id != effect_id))
            node = node->next;
        if (!node) return 0;

        // Unlink from current position
        if (node->prev) node->prev->next = node->next;
        else            this->_first = node->next;
        if (node->next) node->next->prev = node->prev;
        else            this->_last = node->prev;
        node->prev = nullptr;
        node->next = nullptr;

        // Find insertion point — navigate to the node currently at new_pos
        auto current = this->_first;
        for (int i = 0; i < new_pos && current; ++i)
            current = current->next;

        // Insert before current (or at end if current is null)
        if (!current) {
            // append to end
            if (this->_last) {
                this->_last->next = node;
                node->prev = this->_last;
            }
            else {
                this->_first = node;
            }
            this->_last = node;
        }
        else {
            node->prev = current->prev;
            node->next = current;
            if (current->prev) current->prev->next = node;
            else               this->_first = node;
            current->prev = node;
        }
        return 1;
    }
};


#endif //GRAINSTORM_BASE_H
