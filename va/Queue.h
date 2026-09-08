//
// Created by pr on 26.01.19.
//

#ifndef GRAINSTORM_Queue_H
#define GRAINSTORM_Queue_H

#include <cstdint>
#include <atomic>
#include <mutex>

/**
 * A lock-free queue for single consumer, single producer. Not thread-safe when using multiple
 * consumers or producers.
 *
 * Example code:
 *
 * LockFreeQueue<int, 1024> myQueue;
 * int value = 123;
 * myQueue.push(value);
 * myQueue.pop(value);
 *
 * @tparam T - The item type
 * @tparam CAPACITY - Maximum number of items which can be held in the queue. Must be a power of 2.
 * Must be less than the maximum value permissible in INDEX_TYPE
 * @tparam INDEX_TYPE - The internal index type, defaults to uint32_t. Changing this will affect
 * the maximum capacity. Included for ease of unit testing because testing queue lengths of
 * UINT32_MAX can be time consuming and is not always possible.
 */

template <typename T, uint32_t CAPACITY, typename INDEX_TYPE = uint32_t>
class LockFreeQueue {
public:

    /**
     * Implementation details:
     *
     * We have 2 counters: readCounter and writeCounter. Each will increment until it reaches
     * INDEX_TYPE_MAX, then wrap to zero. Unsigned integer overflow is defined behaviour in C++.
     *
 *
     * Each time we need to access our data array we call mask() which gives us the index into the
     * array. This approach avoids having a "dead item" in the buffer to distinguish between full
     * and empty states. It also allows us to have a size() method which is easily calculated.
     *
     * IMPORTANT: This implementation is only thread-safe with a single reader thread and a single
     * writer thread. Have more than one of either will result in Bad Things™.
     */

    static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }
    static_assert(isPowerOfTwo(CAPACITY), "Capacity must be a power of 2");
    static_assert(std::is_unsigned<INDEX_TYPE>::value, "Index type must be unsigned");

    /**
     * Pop a value off the head of the queue
     *
  * @param val - element will be stored in this variable
     * @return true if value was popped successfully, false if the queue is empty
     */
    bool pop(T &val) {
        if (isEmpty()){
            return false;
        } else {
            val = buffer[mask(readCounter)];
            ++readCounter;
            return true;
        }
    }

    /**
     * Add an item to the back of the queue
     *
     * @param item - The item to add
     * @return true if item was added, false if the queue was full
     */
    bool push(const T& item) {
        if (isFull()){
    return false;
        } else {
            buffer[mask(writeCounter)] = item;
            ++writeCounter;
            return true;
        }
    }

    /**
     * Get the item at the front of the queue but do not remove it
     *
     * @param item - item will be stored in this variable
     * @return true if item was stored, false if the queue was empty
     */
    bool peek(T &item) const {
        if (isEmpty()){
            return false;
        } else {
            item = buffer[mask(readCounter)];
            return true;
        }
 }

    /**
     * Get the number of items in the queue
     *
     * @return number of items in the queue
     */
    INDEX_TYPE size() const {

        /**
         * This is worth some explanation:
         *
         * Whilst writeCounter is greater than readCounter the result of (write - read) will always
         * be positive. Simple.
         *
  * But when writeCounter is equal to INDEX_TYPE_MAX (e.g. UINT32_MAX) the next push will
         * wrap it around to zero, the start of the buffer, making writeCounter less than
         * readCounter so the result of (write - read) will be negative.
         *
         * But because we're returning an unsigned type return value will be as follows:
         *
         * returnValue = INDEX_TYPE_MAX - (write - read)
         *
         * e.g. if write is 0, read is 150 and the INDEX_TYPE is uint8_t where the max value is
         * 255 the return value will be (255 - (0 - 150)) = 105.
         *
         */
        return writeCounter - readCounter;
    };


private:

    bool isEmpty() const { return readCounter == writeCounter; }

    bool isFull() const { return size() == CAPACITY; }

    INDEX_TYPE mask(INDEX_TYPE n) const { return static_cast<INDEX_TYPE>(n & (CAPACITY - 1)); }

    T buffer[CAPACITY];
    std::atomic<INDEX_TYPE> writeCounter { 0 };
    std::atomic<INDEX_TYPE> readCounter { 0 };

};

/**
 * A bounded lock-free queue that is safe with MULTIPLE producer threads and a single
 * consumer (Vyukov's bounded queue, used MPSC). LockFreeQueue above is SPSC only:
 * its push() reads writeCounter, stores, then increments, so two producers can claim
 * the same slot — one event is overwritten and the consumer later reads a slot that
 * was never written this round, replaying a stale old event. Here each slot carries a
 * sequence number: a producer CLAIMS its slot with a CAS on writeCounter and only then
 * fills it, publishing by advancing the slot's sequence. The consumer waits on the
 * head slot's sequence, so a claimed-but-unfilled slot reads as "empty" rather than as
 * garbage — events stay FIFO even while a slower producer is mid-publish.
 *
 * push() returns false when full, exactly like LockFreeQueue — callers that cannot
 * afford to lose an event (a NOTE_OFF) must latch the failure somewhere the consumer
 * will see it; see DATA::lostNoteOffs.
 */
template <typename T, uint32_t CAPACITY>
class MpscQueue {
public:
    static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }
    static_assert(isPowerOfTwo(CAPACITY), "Capacity must be a power of 2");

    MpscQueue() {
        for (uint32_t i = 0; i < CAPACITY; i++)
            cells[i].seq.store(i, std::memory_order_relaxed);
    }

    bool push(const T& item) {
        Cell* c;
        uint32_t pos = writeCounter.load(std::memory_order_relaxed);
        for (;;) {
            c = &cells[pos & (CAPACITY - 1)];
            const uint32_t seq = c->seq.load(std::memory_order_acquire);
            const int32_t dif = (int32_t)(seq - pos);
            if (dif == 0) {
                if (writeCounter.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;                    // slot claimed; pos holds our ticket
            } else if (dif < 0) {
                return false;                 // full
            } else {
                pos = writeCounter.load(std::memory_order_relaxed);
            }
        }
        c->data = item;
        c->seq.store(pos + 1, std::memory_order_release);   // publish
        return true;
    }

    // Single consumer only.
    bool pop(T& val) {
        Cell* c = &cells[readCounter & (CAPACITY - 1)];
        const uint32_t seq = c->seq.load(std::memory_order_acquire);
        if ((int32_t)(seq - (readCounter + 1)) < 0)
            return false;                     // empty, or head slot not yet published
        val = c->data;
        c->seq.store(readCounter + CAPACITY, std::memory_order_release);
        ++readCounter;
        return true;
    }

private:
    struct Cell {
        std::atomic<uint32_t> seq{0};
        T data{};
    };
    Cell cells[CAPACITY];
    std::atomic<uint32_t> writeCounter{0};
    uint32_t readCounter{0};   // touched by the one consumer only
};

template <uint32_t CAPACITY, typename INDEX_TYPE = uint32_t>
class ConsumerLockFreeQueue {
public:

    /**
     * Implementation details:
     *
     * We have 2 counters: readCounter and writeCounter. Each will increment until it reaches
     * INDEX_TYPE_MAX, then wrap to zero. Unsigned integer overflow is defined behaviour in C++.
     *
 *
     * Each time we need to access our data array we call mask() which gives us the index into the
     * array. This approach avoids having a "dead item" in the buffer to distinguish between full
     * and empty states. It also allows us to have a size() method which is easily calculated.
     *
     * IMPORTANT: This implementation is only thread-safe with a single reader thread and a single
     * writer thread. Have more than one of either will result in Bad Things™.
     */

    static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }
    static_assert(isPowerOfTwo(CAPACITY), "Capacity must be a power of 2");
    static_assert(std::is_unsigned<INDEX_TYPE>::value, "Index type must be unsigned");

    /**
     * Pop a value off the head of the queue
     *
  * @param val - element will be stored in this variable
     * @return true if value was popped successfully, false if the queue is empty
     */
    bool pop(float **l, float **r, float **le, float **re, float *gain) {
        if (isEmpty()){
            return false;
        } else {
            *l = buffer[mask(readCounter)][0];
            *r = buffer[mask(readCounter)][1];
             *le = buffer[mask(readCounter)][2];
            *re = buffer[mask(readCounter)][3];
            *gain = gains[mask(readCounter)];
            ++readCounter;
            return true;
        }
    }

    /**
     * Add an item to the back of the queue
     *
     * @param item - The item to add
     * @return true if item was added, false if the queue was full
     */
    bool push(float *l, float *r, float *le, float *re, float gain) {
        mutex.lock();
        if (isFull()){
            mutex.unlock();
            return false;
        } else {
            buffer[mask(writeCounter)][0] = l;
            buffer[mask(writeCounter)][1] = r;
            buffer[mask(writeCounter)][2] = le;
            buffer[mask(writeCounter)][3] = re;
            gains[mask(writeCounter)] = gain;
            ++writeCounter;
            mutex.unlock();
            return true;
        }
    }

    /**
     * Get the item at the front of the queue but do not remove it
     *
     * @param item - item will be stored in this variable
     * @return true if item was stored, false if the queue was empty
     */
    bool peek(float **l, float **r, float **le, float **re, float *gain) const {
        if (isEmpty()){
            return false;
        } else {
            *l = buffer[mask(readCounter)][0];
            *r = buffer[mask(readCounter)][1];
            *le = buffer[mask(readCounter)][2];
            *re = buffer[mask(readCounter)][3];
            *gain = gains[mask(readCounter)];
            return true;
        }
    }

    /**
     * Get the number of items in the queue
     *
     * @return number of items in the queue
     */
    INDEX_TYPE size() const {

        /**
         * This is worth some explanation:
         *
         * Whilst writeCounter is greater than readCounter the result of (write - read) will always
         * be positive. Simple.
         *
  * But when writeCounter is equal to INDEX_TYPE_MAX (e.g. UINT32_MAX) the next push will
         * wrap it around to zero, the start of the buffer, making writeCounter less than
         * readCounter so the result of (write - read) will be negative.
         *
         * But because we're returning an unsigned type return value will be as follows:
         *
         * returnValue = INDEX_TYPE_MAX - (write - read)
         *
         * e.g. if write is 0, read is 150 and the INDEX_TYPE is uint8_t where the max value is
         * 255 the return value will be (255 - (0 - 150)) = 105.
         *
         */
        return writeCounter - readCounter;
    };


private:
    bool isEmpty() const { return readCounter == writeCounter; }

    bool isFull() const { return size() == CAPACITY; }

    INDEX_TYPE mask(INDEX_TYPE n) const { return static_cast<INDEX_TYPE>(n & (CAPACITY - 1)); }

    float *buffer[CAPACITY][4];
    float gains[CAPACITY];
    std::atomic<INDEX_TYPE> writeCounter { 0 };
    std::atomic<INDEX_TYPE> readCounter { 0 };
    std::mutex mutex;
};

template <typename T, uint32_t CAPACITY, typename INDEX_TYPE = uint32_t>
class SortedHeap {
public:
    INDEX_TYPE size() const {
        return heapsize;
    };

    void insert(T *val, INDEX_TYPE nelements){
        reset();
        for(INDEX_TYPE i=0; i<nelements;i++)
            insert(val[i]);
    }
    void insert(T val)
    {
        INDEX_TYPE pos, pos2;

        /* Grow heap if necessary */
        if (heapsize == capacity)
            return;

        pos = heapsize;
        heapsize++;


        while (pos > 0)
        {
            /* printf("pos %i\n",pos); */
            pos2 = (pos - 1) >> 1;

            if (buffer[pos2] < val )
                buffer[pos] = buffer[pos2];
            else
                break;

            pos = pos2;
        }
        buffer[pos] = val;
    }

    T get()
    {
        INDEX_TYPE pos, pos2;
        T maxchildkey, val;

        if (heapsize == 0) return -1;
        /* Extract first element */
        T ret = buffer[0];
        val = buffer[heapsize - 1];

        heapsize--;

        pos = 0;
        pos2 = 1;

        while (pos2 < heapsize)
        {
            if ( (pos2 + 2 > heapsize) ||
                 (buffer[pos2] >= buffer[pos2 + 1]) )
                maxchildkey = buffer[pos2];
            else
                maxchildkey = buffer[++pos2];

            if (maxchildkey > val)
                buffer[pos] = buffer[pos2];
            else
                break;

            pos = pos2;
            pos2 = (pos << 1) + 1;
        }

        buffer[pos] = val;

        return ret;
    }

    void fill(T *arr){
        T val;
        INDEX_TYPE index = 0;
        while((val = get()) != -1){
            arr[index++] = val;
        }

    }

    void reset() {heapsize = 0;}
private:
    INDEX_TYPE capacity{CAPACITY};
    INDEX_TYPE heapsize {0};
    T buffer[CAPACITY];
};


#endif //GRAINSTORM_Queue_H
