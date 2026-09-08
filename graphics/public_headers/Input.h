#pragma once
//
// Created by pr on 19.12.20.
//

#ifndef GRAINSTORM_INPUT_H
#define GRAINSTORM_INPUT_H
#include "InputEvent.h"
#include "tools.h"
#include <cmath>
#include <deque>
#include <cstddef>
#include <array>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <limits>

namespace tsl {
    struct AppState;
    namespace graphics {
        class View;
        namespace InputSystem {
            // Optimized distance calculation with fast inverse square root option
            inline float spacing(const float x0, const float x1, const float y0, const float y1) {
                const float dx = x0 - x1;
                const float dy = y0 - y1;
                return std::sqrt(dx * dx + dy * dy);
            }

            // Fast distance squared (avoids sqrt when only relative distances matter)
            inline float spacing_squared(const float x0, const float x1, const float y0, const float y1) {
                const float dx = x0 - x1;
                const float dy = y0 - y1;
                return dx * dx + dy * dy;
            }

            enum class WinState : int8_t {
                WINPOINTER = 0,
                WINDRAG = -1,
                WINZOOM = -2
            };



            struct Pointer {
                int id;
                float xpos;
                float ypos;
                WinState mode;
                int8_t target;
                union {
                    float xposWhenCreated{};
                    uint32_t userData3;
                    
                };
                union {
                    float yposWhenCreated{};
                    uint32_t  userData4;
                };
                union {
                    float distanceTravelled{};
                    uint32_t  userData1;
                };
                
                union {
                    float userData2Float{};
                    uint32_t  userData2;
                };
                // Default constructor
                Pointer() : id(0), xpos(0.0f), ypos(0.0f), xposWhenCreated(0), yposWhenCreated(0), distanceTravelled(0), mode(WinState::WINPOINTER), target(0) {}

                // Constructor without target
                Pointer(int _id, float _xpos, float _ypos, WinState _mode)
                        : id(_id), xpos(_xpos), ypos(_ypos), mode(_mode), target(0), xposWhenCreated(_xpos), yposWhenCreated(_ypos), distanceTravelled(0) {}

                // Constructor with target
                Pointer(int _id, float _xpos, float _ypos, WinState _mode, int _target)
                        : id(_id), xpos(_xpos), ypos(_ypos), mode(_mode), target(_target), xposWhenCreated(_xpos), yposWhenCreated(_ypos), distanceTravelled(0) {}

                // Equality operator
                bool operator==(const Pointer& other) const {
                    return (id == other.id && xpos == other.xpos && ypos == other.ypos && mode == other.mode);
                }

                // Update position
                void updatePosition(float x, float y) {
                    xpos = x;
                    ypos = y;
                }

                // Check if this is a window interaction pointer
                bool isWindowPointer() const {
                    return static_cast<int>(mode) < static_cast<int>(WinState::WINPOINTER);
                }

                // Distance to another pointer
                float distanceTo(const Pointer& other) const {
                    return spacing(xpos, other.xpos, ypos, other.ypos);
                }

                // Distance squared to another pointer (faster)
                float distanceSquaredTo(const Pointer& other) const {
                    return spacing_squared(xpos, other.xpos, ypos, other.ypos);
                }

                // Distance to a point
                float distanceTo(float x, float y) const {
                    return spacing(xpos, x, ypos, y);
                }

                // Distance squared to a point (faster)
                float distanceSquaredTo(float x, float y) const {
                    return spacing_squared(xpos, x, ypos, y);
                }
                
            };

            class InputState {
            private:
                static constexpr size_t MAX_POINTERS1 = 32; // Reasonable limit for touch/pointer inputs
                static constexpr size_t INITIAL_CAPACITY = 16;

                std::vector<Pointer> pointers_;
                std::unordered_map<int, size_t> id_to_index_; // Fast ID-based lookup
                std::unordered_map<int, std::vector<size_t>> target_to_indices_; // Fast target-based lookup

                // Pre-allocated static arrays for caching
                mutable std::array<Pointer*, MAX_POINTERS1> window_pointers_cache_{};
                mutable std::array<Pointer*, MAX_POINTERS1> zoom_pointers_cache_{};
                mutable size_t window_cache_size_ = 0;
                mutable size_t zoom_cache_size_ = 0;
                mutable bool cache_valid_ = false;

                void invalidateCache() const {
                    cache_valid_ = false;
                }

                void updateCache() const {
                    if (cache_valid_) return;

                    window_cache_size_ = 0;
                    zoom_cache_size_ = 0;

                    for (auto& pointer : pointers_) {
                        if (pointer.isWindowPointer() && window_cache_size_ < MAX_POINTERS1) {
                            window_pointers_cache_[window_cache_size_++] = const_cast<Pointer*>(&pointer);
                            if (pointer.mode == WinState::WINZOOM && zoom_cache_size_ < MAX_POINTERS1) {
                                zoom_pointers_cache_[zoom_cache_size_++] = const_cast<Pointer*>(&pointer);
                            }
                        }
                    }
                    cache_valid_ = true;
                }

                void rebuildMaps() {
                    id_to_index_.clear();
                    target_to_indices_.clear();

                    for (size_t i = 0; i < pointers_.size(); ++i) {
                        const auto& pointer = pointers_[i];
                        id_to_index_[pointer.id] = i;
                        target_to_indices_[pointer.target].push_back(i);
                    }
                }

            public:
                tsl::time timer;
                int lastpointer;

                InputState() : lastpointer(-10000) {
                    // Reserve space to minimize allocations
                    pointers_.reserve(INITIAL_CAPACITY);
                    id_to_index_.reserve(INITIAL_CAPACITY);
                    target_to_indices_.reserve(INITIAL_CAPACITY / 2); // Assuming fewer unique targets
                }

                // Add a pointer
                void addPointer(const Pointer& pointer) {
                    // An existing entry under this id is a leftover whose UP never
                    // reached this view (the OS reuses ids, so a dropped gesture's
                    // entry collides with the next one). Replace it: a second entry
                    // under the same id is seen by every range-for walk, while
                    // removePointer only drops the one the id map points at.
                    auto it = id_to_index_.find(pointer.id);
                    if (it != id_to_index_.end() && it->second < pointers_.size()) {
                        pointers_[it->second] = pointer;
                        rebuildMaps();
                        invalidateCache();
                        return;
                    }

                    if (pointers_.size() >= MAX_POINTERS1) {
                        // Handle overflow - could log warning or replace oldest
                        return;
                    }

                    pointers_.push_back(pointer);
                    size_t index = pointers_.size() - 1;
                    id_to_index_[pointer.id] = index;
                    target_to_indices_[pointer.target].push_back(index);
                    invalidateCache();
                }
                void addPointer(int id, float x, float y, WinState mode, int target) {
                    if (pointers_.size() >= MAX_POINTERS1) {
                        return;
                    }
                    Pointer pt{id,x,y,mode,target};
                    addPointer(pt);
                }
                // Remove pointer by ID (optimized to avoid rebuilding maps when possible)
                bool removePointer(int id) {
                    auto it = id_to_index_.find(id);
                    if (it == id_to_index_.end()) return false;

                    size_t index = it->second;
                    if (index >= pointers_.size()) return false;

                    // If removing the last element, we can avoid rebuilding maps
                    if (index == pointers_.size() - 1) {
                        int target = pointers_[index].target;
                        auto& target_indices = target_to_indices_[target];
                        target_indices.erase(
                                std::remove(target_indices.begin(), target_indices.end(), index),
                                target_indices.end()
                        );

                        pointers_.pop_back();
                        id_to_index_.erase(it);
                    } else {
                        // Remove from target map
                        int target = pointers_[index].target;
                        auto& target_indices = target_to_indices_[target];
                        target_indices.erase(
                                std::remove(target_indices.begin(), target_indices.end(), index),
                                target_indices.end()
                        );

                        // Remove from pointers and rebuild maps
                        pointers_.erase(pointers_.begin() + index);
                        rebuildMaps();
                    }

                    invalidateCache();
                    return true;
                }

                // Update pointer position
                bool updatePointer(int id, float x, float y) {
                    Pointer* pointer = getById(id);
                    if (pointer) {
                        pointer->updatePosition(x, y);
                        return true;
                    }
                    return false;
                }

                // Get number of window pointers
                int numPointersWindow() const {
                    updateCache();
                    return static_cast<int>(window_cache_size_);
                }

                // Optimized zoom check
                void checkZoom() {
                    updateCache();

                    size_t found = 0;
                    Pointer* single_pointer = nullptr;

                    // Set zoom mode for first two window pointers, drag for others
                    for (size_t i = 0; i < window_cache_size_; ++i) {
                        if (found < 2) {
                            window_pointers_cache_[i]->mode = WinState::WINZOOM;
                            if (found == 0) {
                                single_pointer = window_pointers_cache_[i];
                            }
                        } else {
                            window_pointers_cache_[i]->mode = WinState::WINDRAG;
                        }
                        ++found;
                    }

                    // If only one pointer, set it to drag mode
                    if (found == 1 && single_pointer) {
                        single_pointer->mode = WinState::WINDRAG;
                    }

                    invalidateCache();
                }

                // Get the other zoom pointer
                Pointer* getTheOther(Pointer* pt) {
                    updateCache();

                    if (zoom_cache_size_ >= 2) {
                        for (size_t i = 0; i < zoom_cache_size_; ++i) {
                            if (zoom_pointers_cache_[i] != pt) {
                                return zoom_pointers_cache_[i];
                            }
                        }
                    }
                    return nullptr;
                }

                // Find nearest pointer to given pointer (excluding zoom mode pointers)
                Pointer* nearest(const Pointer& in) {
                    if (pointers_.empty()) return nullptr;

                    Pointer* result = nullptr;
                    float min_distance_sq = std::numeric_limits<float>::max();

                    for (auto& pointer : pointers_) {
                        if (&pointer != &in && pointer.mode != WinState::WINZOOM) {
                            float distance_sq = in.distanceSquaredTo(pointer);
                            if (distance_sq < min_distance_sq) {
                                min_distance_sq = distance_sq;
                                result = &pointer;
                            }
                        }
                    }
                    return result;
                }

                // Find nearest pointer to given coordinates
                Pointer* nearest(float x, float y) {
                    if (pointers_.empty()) return nullptr;

                    Pointer* result = nullptr;
                    float min_distance_sq = std::numeric_limits<float>::max();

                    for (auto& pointer : pointers_) {
                        float distance_sq = pointer.distanceSquaredTo(x, y);
                        if (distance_sq < min_distance_sq) {
                            min_distance_sq = distance_sq;
                            result = &pointer;
                        }
                    }
                    return result;
                }

                // Find nearest pointer within a given radius
                Pointer* nearestWithinRadius(float x, float y, float radius) {
                    float radius_sq = radius * radius;
                    Pointer* result = nullptr;
                    float min_distance_sq = std::numeric_limits<float>::max();

                    for (auto& pointer : pointers_) {
                        float distance_sq = pointer.distanceSquaredTo(x, y);
                        if (distance_sq <= radius_sq && distance_sq < min_distance_sq) {
                            min_distance_sq = distance_sq;
                            result = &pointer;
                        }
                    }
                    return result;
                }

                // Get other pointer with same target
                Pointer* getTheOther2(const Pointer& in) {
                    const auto& indices = target_to_indices_.find(in.target);
                    if (indices != target_to_indices_.end()) {
                        for (size_t index : indices->second) {
                            if (index < pointers_.size() && &pointers_[index] != &in) {
                                return &pointers_[index];
                            }
                        }
                    }
                    return nullptr;
                }

                // Get pointer by ID (optimized with hash map)
                Pointer* getById(int id) {
                    auto it = id_to_index_.find(id);
                    if (it != id_to_index_.end() && it->second < pointers_.size()) {
                        return &pointers_[it->second];
                    }
                    return nullptr;
                }

                // Const version
                const Pointer* getById(int id) const {
                    auto it = id_to_index_.find(id);
                    if (it != id_to_index_.end() && it->second < pointers_.size()) {
                        return &pointers_[it->second];
                    }
                    return nullptr;
                }

                // Check if target exists
                bool containsTarget(int target) const {
                    auto it = target_to_indices_.find(target);
                    return it != target_to_indices_.end() && !it->second.empty();
                }

                // Get all pointers with a specific target (using static array to avoid allocations)
                template<size_t N>
                size_t getPointersByTarget(int target, std::array<Pointer*, N>& result) {
                    size_t count = 0;
                    const auto& indices = target_to_indices_.find(target);
                    if (indices != target_to_indices_.end()) {
                        for (size_t index : indices->second) {
                            if (index < pointers_.size() && count < N) {
                                result[count++] = &pointers_[index];
                            }
                        }
                    }
                    return count;
                }

                // Get all pointers in a specific mode (using static array)
                template<size_t N>
                size_t getPointersByMode(WinState mode, std::array<Pointer*, N>& result) {
                    size_t count = 0;
                    for (auto& pointer : pointers_) {
                        if (pointer.mode == mode && count < N) {
                            result[count++] = &pointer;
                        }
                    }
                    return count;
                }

                // Legacy versions that return vectors (for backward compatibility)
                std::vector<Pointer*> getPointersByTarget(int target) {
                    std::vector<Pointer*> result;
                    const auto& indices = target_to_indices_.find(target);
                    if (indices != target_to_indices_.end()) {
                        result.reserve(indices->second.size());
                        for (size_t index : indices->second) {
                            if (index < pointers_.size()) {
                                result.push_back(&pointers_[index]);
                            }
                        }
                    }
                    return result;
                }

                std::vector<Pointer*> getPointersByMode(WinState mode) {
                    std::vector<Pointer*> result;
                    for (auto& pointer : pointers_) {
                        if (pointer.mode == mode) {
                            result.push_back(&pointer);
                        }
                    }
                    return result;
                }

                // Clear all pointers
                void clear() {
                    pointers_.clear();
                    id_to_index_.clear();
                    target_to_indices_.clear();
                    invalidateCache();
                    lastpointer = -10000;
                }

                // Get total number of pointers
                size_t size() const {
                    return pointers_.size();
                }

                // Check if empty
                bool empty() const {
                    return pointers_.empty();
                }

                // Iterator access
                auto begin() { return pointers_.begin(); }
                auto end() { return pointers_.end(); }
                auto begin() const { return pointers_.begin(); }
                auto end() const { return pointers_.end(); }

                // Get zoom center point (average of zoom pointers)
                std::pair<float, float> getZoomCenter() const {
                    updateCache();

                    if (zoom_cache_size_ >= 2) {
                        float total_x = 0.0f, total_y = 0.0f;
                        for (size_t i = 0; i < zoom_cache_size_; ++i) {
                            total_x += zoom_pointers_cache_[i]->xpos;
                            total_y += zoom_pointers_cache_[i]->ypos;
                        }
                        return {total_x / zoom_cache_size_, total_y / zoom_cache_size_};
                    }
                    return {0.0f, 0.0f};
                }

                // Get zoom distance (distance between zoom pointers)
                float getZoomDistance() const {
                    updateCache();

                    if (zoom_cache_size_ >= 2) {
                        return zoom_pointers_cache_[0]->distanceTo(*zoom_pointers_cache_[1]);
                    }
                    return 0.0f;
                }

                // Get current capacity (for debugging/monitoring)
                size_t capacity() const {
                    return pointers_.capacity();
                }

                // Debug: Print current state
                void debugPrint() const {
                    printf("InputState: %zu/%d pointers (capacity: %zu)\n",
                           pointers_.size(), MAX_POINTERS, pointers_.capacity());
                    for (size_t i = 0; i < pointers_.size(); ++i) {
                        const auto& p = pointers_[i];
                        printf("  [%zu] ID:%d Pos:(%.2f,%.2f) Mode:%d Target:%d\n",
                               i, p.id, p.xpos, p.ypos, static_cast<int>(p.mode), p.target);
                    }
                }
            };

        } // namespace InputSystem
        /*
        InputSystem::InputState input;

// Add pointers
        input.addPointer({1, 100.0f, 200.0f, InputSystem::WinState::POINTER, 0});
    input.addPointer({2, 150.0f, 250.0f, InputSystem::WinState::DRAG, 0});

// Fast ID-based lookup
auto* pointer = input.getById(1);

// Find nearest pointer within 50 pixels
auto* nearest = input.nearestWithinRadius(120.0f, 220.0f, 50.0f);

// Get zoom center and distance for gesture handling
auto [center_x, center_y] = input.getZoomCenter();
float zoom_dist = input.getZoomDistance();
        */
         inline float spacing(const float x0, const float x1, const float y0, const float y1) {
            const float x = x0 - x1;
            const float y = y0 - y1;
            return sqrtf(x * x + y * y);
        }

// Event pool for avoiding heap allocations
        class InputEventPool {
        private:
            static constexpr size_t POOL_SIZE = 64;
            static constexpr size_t MAX_ACTIVE_POINTERS = 32;

            std::array<tsl::graphics::InputEvent, POOL_SIZE> pool_;
            std::vector<size_t> available_indices_;
            std::array<tsl::graphics::InputEvent*, MAX_ACTIVE_POINTERS> active_pointers_{};
            size_t active_count_ = 0;

        public:
            InputEventPool() {
                // Initialize available indices
                available_indices_.reserve(POOL_SIZE);
                for (size_t i = 0; i < POOL_SIZE; ++i) {
                    available_indices_.push_back(i);
                }

                // Initialize active pointers array
                active_pointers_.fill(nullptr);
            }

            void invalidateViewEvents(View* view) {
                for (size_t i = 0; i < active_count_; ++i) {
                    if (active_pointers_[i] && active_pointers_[i]->v == view) {
                        active_pointers_[i]->v = nullptr; // Mark as invalid
                    }
                }
            }

            tsl::graphics::InputEvent* acquire() {
                if (available_indices_.empty()) {
                    // Pool exhausted - this shouldn't happen with proper sizing
                    return nullptr;
                }

                size_t idx = available_indices_.back();
                available_indices_.pop_back();

                auto* event = &pool_[idx];
                // Reset the event to default state
                *event = tsl::graphics::InputEvent{};
                return event;
            }

            void release(tsl::graphics::InputEvent* event) {
                if (!event) return;

                // Find index in pool
                size_t idx = event - &pool_[0];
                if (idx >= POOL_SIZE) return;   // not ours (also catches event < pool_[0], which wraps)

                // Idempotent: releasing the same entry twice would hand the same
                // slot to two acquire() calls, so two live pointers would alias
                // one InputEvent. The scan is over <= POOL_SIZE (64) entries and
                // runs once per touch, not per frame.
                for (size_t f : available_indices_)
                    if (f == idx) return;

                available_indices_.push_back(idx);
            }

            void clearAll() {
                for (size_t i = 0; i < active_count_; ++i) {
                    if (active_pointers_[i]) {
                        release(active_pointers_[i]);
                        active_pointers_[i] = nullptr;
                    }
                }
                active_count_ = 0;
            }

            // Takes ownership of `event` (an acquire()d entry). Every path that
            // does not store it must hand it back, or the entry is lost for the
            // life of the process: available_indices_ only ever grows again
            // through release(), and clearAll() is the only other refill.
            //
            // Both early exits below used to just drop it. A pointer_id can
            // legitimately already be active -- ACTION_CANCEL leaves entries in
            // place on purpose -- so every tap after a cancelled gesture leaked
            // one of the POOL_SIZE entries. Once they were gone acquire()
            // returned nullptr forever, handlePointerDown() bailed out before
            // dispatching, and touch input was dead until the process restarted
            // while the draw and audio threads carried on normally.
            void addActive(tsl::graphics::InputEvent* event) {
                if (!event) return;

                // Check if already active (avoid duplicates)
                for (size_t i = 0; i < active_count_; ++i) {
                    if (active_pointers_[i] && active_pointers_[i]->pointer_id == event->pointer_id) {
                        // Update existing instead of adding duplicate
                        if (active_pointers_[i] != event) {
                            *active_pointers_[i] = *event;
                            release(event);
                        }
                        return;
                    }
                }
                if (active_count_ < MAX_ACTIVE_POINTERS) {
                    active_pointers_[active_count_++] = event;
                }
                else
                    release(event);
            }

            // Remove from active pointers and release
            void removeActive(int32_t pointer_id) {
                for (size_t i = 0; i < active_count_; ++i) {
                    if (active_pointers_[i] && active_pointers_[i]->pointer_id == pointer_id) {
                        release(active_pointers_[i]);
                        // Move last element to current position
                        active_pointers_[i] = active_pointers_[--active_count_];
                        active_pointers_[active_count_] = nullptr;
                        break;
                    }
                }
            }

            // Update active pointer positions
            void updateActive(int32_t pointer_id, float x, float y, int action) {
                for (size_t i = 0; i < active_count_; ++i) {
                    if (active_pointers_[i] && active_pointers_[i]->pointer_id == pointer_id) {
                        active_pointers_[i]->x = x;
                        active_pointers_[i]->y = y;
                        active_pointers_[i]->action = action;
                        active_pointers_[i]->time = tsl::time::nanosecondsSinceEpoch();
                        break;
                    }
                }
            }

            // Get active pointer by ID
            tsl::graphics::InputEvent* getActive(int32_t pointer_id) {
                for (size_t i = 0; i < active_count_; ++i) {
                    if (active_pointers_[i] && active_pointers_[i]->pointer_id == pointer_id) {
                        return active_pointers_[i];
                    }
                }
                return nullptr;
            }

            // Get all active pointers for batch processing
            [[nodiscard]] const std::array<tsl::graphics::InputEvent*, MAX_ACTIVE_POINTERS>& getActivePointers() const {
                return active_pointers_;
            }

            [[nodiscard]] size_t getActiveCount() const { return active_count_; }
        };





#ifndef ANDROID
        int32_t callback_input(tsl::AppState *, InputEvent& event);
#endif

    }
}
using pmode_t = tsl::graphics::InputSystem::WinState;

#endif //GRAINSTORM_INPUT_H
