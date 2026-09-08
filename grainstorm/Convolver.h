#pragma once
//
// Created by pr on 19.12.18.
//

#ifndef GRAINSTORM_CONVOLVER_H
#define GRAINSTORM_CONVOLVER_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>
#include <mutex>
#include <random>
#include "tools/aligned_memalloc.h"
#include "types.h"
#include "grainstorm.h"
#include "ffttools.h"
#include "app.h"
#include "tools/RingBufferQueue.h"

#define SPECDELMAXDELMS 10000.
#define SPECDELMINDELMS 50.
/* IR morph rate is a log parameter, so it needs an offset to reach an exact 0
   (a parked front) at the bottom of its range: stored value -> LOG2NORMAL -
   this. It doubles as the smallest nonzero rate. */
#define SPECDELRATEOFFS .01
#define SPECDELRATEMAX 8.

#include <ComplexMultiply.h>
#include "dplimit1.h"

class LiveConv {
public:
	int32_t totalSize{};

	LiveConv(int32_t partSize, int irLen) {
		/* set p->partSize to the initial partition length, iPartLen */
		_partSize = partSize;
		if ((_partSize < 4 || (_partSize & (_partSize - 1)) != 0)) {
			LOGE("liveconv: invalid impulse response partition length");
			return;
		}
		/* Calculate the total length  */
		auto n = irLen;
		if (UNLIKELY(n <= 0)) {
			LOGE("liveconv: invalid length, or insuffient IR data for convolution");
			return;
		}

		// Compute the number of partitions (total length / partition size)
		_nPartitions = (n + (_partSize - 1)) / _partSize;
		totalSize = _partSize * _nPartitions;
		/*
		** Calculate the amount of aux space to allocate (in bytes) and
		** allocate if necessary
		** Function of partition size and number of partitions
		*/

		auto nBytes = buf_bytes_alloc();
		_buf.resize(nBytes);

		/*
		** From here on is initialization of data
		*/

		/* initialize buffer pointers */
		set_buf_pointers();

		/* Initialize load bookkeeping (real objects — load_t holds a std::vector
		   and must be properly constructed/destructed) */
		_slots.resize(_nPartitions + 1);
		_loader.begin = _slots.data();
		init_load(&_loader, (_nPartitions + 1));

		/* clear ring buffer to zero */
		n = (_partSize << 1u) * _nPartitions;
		memset(_ringBuf, 0, n * sizeof(MYFLOAT));

		/* initialize buffer indices */
		_cnt = 0;
		_rbCnt = 0;

		fft = std::make_unique<FFT>(_partSize << 1u);

		/* clear IR buffer to zero */
		memset(_IR_Data, 0, n * sizeof(MYFLOAT));

		/* clear output buffers to zero */
		memset(_outBuf, 0, (_partSize << 1u) * sizeof(MYFLOAT));

		/*
		** After initialization:
		**    Buffer indexes are zero
		**    tmpBuf is filled with rubish
		**    ringBuf and outBuf are filled with zero
		**    IR_Data buffers are filled with zero
		*/
	}

	template<typename T>
	void LOADIR(std::vector<MYFLOAT>& ir) {
		queue.try_push(ir);
	}

	template<typename Iterator>
	void load_ir(Iterator start, Iterator end) {
		queue.try_push(std::vector<MYFLOAT>(start, end));
	}

	tsl::RingBufferMPSCQueue<std::vector<MYFLOAT>,16> queue{};

	/* Live morph controls, set once per block from the audio thread.
	   rate      partitions advanced per partition-block. 1.0 advances one
	             partition per block = exactly one tap per sample, the same
	             rate at which a sound travels along the IR - which is what
	             makes every sound keep the IR that was current when it
	             entered. 0 parks the front where it is.
	   gateOpen  may this stage take a new IR at all.
	   limitTaps where the front stops, in taps of THIS stage's segment.
	             Forward it is a ceiling - the front writes below it; reverse
	             it is a floor - the front descends to it and stops. Either
	             way whatever the previous IR left past it stays in place.
	             INT32_MAX forward means "no limit", which is what lets a
	             shorter IR erase the old tail beyond its own length.
	   reverse   latched by each new load, which then fills tail-first. */
	void setMorph(MYFLOAT rate, bool gateOpen, int32_t limitTaps, bool reverse) {
		_morphRate = rate < FL(0.0) ? FL(0.0) : rate;
		_gateOpen = gateOpen;
		_frontLimit = limitTaps < 0 ? 0 : limitTaps;
		_reverse = reverse;
	}

	/* Is an IR still being swapped in? _pendingHead matters: a load accepted
	   part-way through a block is not linked into the chain until the block
	   ends, and reporting "idle" in that window makes a caller think the sweep
	   is over and re-close the gates under a live load. */
	bool loading() const {
		return _loader.head->status != NO_LOAD || _pendingHead;
	}

	/* Is an IR waiting for a free slot or for the front to be released? */
	bool pending() const { return _hasPending; }

	inline MYFLOAT tick(const MYFLOAT in) {

		// uint32_t                numLoad = p->nPartitions + 1;
		/* Only continue if initialized */

		auto nSamples = _partSize;   /* Length of partition */
		/* Pointer to a partition of the ring buffer */
		auto rBuf = &(_ringBuf[_rbCnt * (nSamples << 1u)]);


		/* If clear flag is set: empty buffers and reset indexes */
		//auto clearBuf = (int) *_kClear;
		if (false) {

			/* clear ring buffer to zero */
			auto n = (nSamples << 1u) * _nPartitions;
			std::memset(_ringBuf, 0, n * sizeof(MYFLOAT));

			/* initialize buffer index */
			_cnt = 0;
			_rbCnt = 0;

			/* clear output buffers to zero */
			std::memset(_outBuf, 0, (nSamples << 1u) * sizeof(MYFLOAT));
		}

		/*
		** How to handle the kUpdate input:
		** -1: Gradually clear the IR buffer
		**      0: Do nothing
		**  1: Gradually load the IR buffer
		*/

		if (_loader.available) {
			auto load_ptr = previous_load(&_loader, _loader.head);
			// Always drain down to the NEWEST pending IR, even when the front
			// is frozen or the slot before head is still busy. Intermediate
			// IRs are obsolete, and the queue is small: a stage that stops
			// popping would overflow it (try_push silently drops) and come
			// back on a stale IR while the faster stages moved on. Parking
			// the newest one here instead means a frozen front picks up
			// exactly the current IR the moment it is released.
			while (auto t = queue.try_pop()) {
				_pending = std::move(*t);
				_hasPending = true;
			}
			// The buffer before the head position is the temporary buffer, and
			// it has to be free before it can be reused. At the default rate a
			// load always finishes within _nPartitions blocks so the ring can
			// never fill, but a front running below real time holds its slot
			// for longer.
			if (_hasPending && load_ptr->status == NO_LOAD && _gateOpen) {
				_hasPending = false;
				load_ptr->status = LOADING;
				load_ptr->pos = 0;
				load_ptr->reverse = _reverse;
				load_ptr->ir = std::move(_pending);
				_pending.clear();   /* moved-from: put it back in a known state */
				auto needed = (int32_t)((load_ptr->ir.size() + _partSize - 1) / _partSize);
				load_ptr->target = needed < _nPartitions ? needed : _nPartitions;
				/* Cover the union of the old and the new IR - everything past
				   that is already zero, so there is nothing to erase there.
				   This is also where a reverse load starts from. */
				load_ptr->nSteps = load_ptr->target > _nActive ? load_ptr->target : _nActive;
				/* while old + new IR coexist, cover both */
				if (load_ptr->target > _nActive)
					_nActive = load_ptr->target;

				_loader.available = 0;

				/* Special case: At a partition border: Make the temporary buffer
				   head position - otherwise defer it to the end of the block */
				if (_cnt == 0)
					_loader.head = load_ptr;
				else
					_pendingHead = true;
			}
		}

		/* store input signal in buffer */
		rBuf[_cnt] = in;

		/* copy output signals from buffer (contains data from previous
		   convolution pass) */
		auto out = _outBuf[_cnt];

		/* is input buffer full ? */
		if (++_cnt < nSamples)
			return out;                   /* no, continue with next sample */

		/* Check if there are any IR partitions to load/unload. _morphRate scales
		   how far the replacement front travels per block; the fractional part
		   is carried so rates below 1 simply advance less often. */
		{
			_rateAcc += _morphRate;
			int32_t steps = (int32_t)_rateAcc;
			_rateAcc -= (MYFLOAT)steps;   /* carry only the fraction */
			if (steps > _nPartitions)
				steps = _nPartitions;

			/* forward: the front stops after this many whole partitions */
			const int32_t limit = _frontLimit >= _nPartitions * nSamples
				? _nPartitions
				: (_frontLimit + nSamples - 1) / nSamples;

			/* Walk the live loads OLDEST FIRST. Below rate 1 the fronts of two
			   loads accepted a block apart advance by less than a partition
			   per block, so their integer positions tie and both write the
			   same partition in the same block - and whoever writes last owns
			   it forever, since neither revisits. Oldest-first makes that the
			   newest load. (At rate >= 1 fronts stay a partition apart and the
			   order never matters.) Bounded by the slot count: a slow front
			   can fill the ring, and then there is no NO_LOAD slot to
			   terminate on. */
			const int32_t nSlots = (int32_t)(_loader.end - _loader.begin);
			int32_t nLive = 0;
			{
				auto lp = _loader.head;
				while (nLive < nSlots && lp->status != NO_LOAD) {
					nLive++;
					lp = next_load(&_loader, lp);
				}
			}
			for (int32_t o = nLive - 1; o >= 0; o--) {
				auto lp = _loader.head;
				for (int32_t i = 0; i < o; i++)
					lp = next_load(&_loader, lp);
				for (int32_t s = 0; s < steps && lp->status != NO_LOAD; s++)
					advance_load(lp, nSamples, limit);
			}
		}
		_loader.available = 1;

		// A load accepted mid-block becomes head now. Tracked with a flag rather
		// than inferred from the slot's status: when the front runs slow enough
		// for the ring to fill, the slot before head is the OLDEST live load,
		// and moving head onto it would reorder the chain.
		if (_pendingHead) {
			_loader.head = previous_load(&_loader, _loader.head);
			_pendingHead = false;
		}

		/* head always points at the newest active load; once it is NO_LOAD all
		   loads have finished and only the last IR's partitions remain nonzero */
		if (_loader.head->status == NO_LOAD)
			_nActive = _lastTarget;

		/* Now the partition is filled with input --> start calculate the
		   convolution */
		_cnt = 0; /* reset buffer position */

		/* pad input in ring buffer with zeros to double length */
		for (int32_t i = nSamples; i < (nSamples << 1u); i++)
			rBuf[i] = FL(0.0);

		/* calculate FFT of input (always, so the ring holds valid history
		   for the moment a new IR gets loaded) */
		fft->forward(rBuf);

		/* update ring buffer position */
		_rbCnt++;
		if (_rbCnt >= _nPartitions)
			_rbCnt = 0;
		auto rBufPos = _rbCnt * (nSamples << 1u);

		/* Move to next partition in ring buffer (used in next iteration to
		   store the next input sample) */
		rBuf = &(_ringBuf[rBufPos]);

		auto x = &(_outBuf[0]);
		if (_nActive > 0) {
			/* multiply complex arrays --> multiplication in the frequency domain.
			   IR partitions are stored in reverse order, so the unused (zero)
			   partitions of a short IR occupy the leading slots — skip them. */
			multiply_fft_buffers(_tmpBuf, _ringBuf, _IR_Data,
				nSamples, _nPartitions, rBufPos, _nPartitions - _nActive);

			/* inverse FFT */
			fft->backward(_tmpBuf);

			/*
			** Copy IFFT result to output buffer
			** The second half is left as "tail" for next iteration
			** The first half is overlapped with "tail" of previous block
			*/
			for (int32_t i = 0; i < nSamples; i++) {
				x[i] = _tmpBuf[i] + x[i + nSamples];
				x[i + nSamples] = _tmpBuf[i + nSamples];
			}
		}
		else {
			/* IR is silent: flush the remaining tail, output decays to zero */
			for (int32_t i = 0; i < nSamples; i++) {
				x[i] = x[i + nSamples];
				x[i + nSamples] = FL(0.0);
			}
		}
		return out;
	}


	/* firstseg > 0 skips the first firstseg partitions (their IR data is known
	   to be zero); the ring buffer pointer advances by the same amount so the
	   input-block/IR-partition pairing is unchanged. Requires firstseg < numsegments. */
	inline static void multiply_fft_buffers(MYFLOAT* out, MYFLOAT* ringbuf, MYFLOAT* IR_Data,
		int32_t segsize, int32_t numsegments,
		int32_t ringbuf_startpos, int32_t firstseg = 0) {
		int32_t complexnum = segsize;
		/* note: partSize must be at least 2 samples */
		segsize <<= 1; /* locale partsize is twice the size of the partition size */
		/* Finding the index of the last sample pair in the output buffer */
		/* The end of the ring buffer */
		auto rbend = ringbuf + (int32_t)(segsize * numsegments);
		auto rb = &(ringbuf[ringbuf_startpos]) + firstseg * segsize; /* Initialize ring buffer pointer */
		if (rb >= rbend)
			rb -= segsize * numsegments; /* wrap (single wrap suffices: startpos < total, firstseg < numsegments) */
		auto ir = IR_Data + firstseg * segsize; /* Initialize impulse data pointer */
		numsegments -= firstseg;

		/* clear output buffer to zero */
		std::memset(out, 0, sizeof(MYFLOAT) * segsize);
		/*
		** Multiply FFTs for each partition and mix to output buffer
		** Note: IRs are stored in reverse partition order
		*/
		auto outbuf = out;

		MYFLOAT zero = 0;
		MYFLOAT one = 0;

		do {
			/* wrap ring buffer position */
			if (rb >= rbend)
				rb = ringbuf;
			zero += (rb[0] * ir[0]);
			one += (rb[1] * ir[1]);

#if (__aarch64__)
			complexMultiplyAccumDouble(outbuf, outbuf, rb, ir, complexnum);
#else
			for (int i = 2; i < segsize; i += 2) {
				auto nimag = i + 1;
				outbuf[i] += rb[i] * ir[i] - rb[nimag] * ir[nimag]; // real part
				outbuf[nimag] += rb[i] * ir[nimag] + rb[nimag] * ir[i]; // imaginary part
			}
#endif
			rb += segsize;
			ir += segsize;
		} while (--numsegments);
		outbuf[0] = zero;
		outbuf[1] = one;
	}


private:
	std::unique_ptr<FFT> fft;
	enum {
		NO_LOAD, LOADING
	};

	struct load_t {
		std::vector<MYFLOAT> ir;
		int32_t status{ NO_LOAD };
		int32_t pos{};
		int32_t target{};    /* partitions this IR needs once fully loaded */
		int32_t nSteps{};    /* partitions this load has to walk: the union of
								the old and the new IR extent */
		bool reverse{};      /* latched at load time - a load keeps its
								direction even if the control moves */
	};

	struct rbload_t {
		load_t* begin;
		load_t* end;
		load_t* head;
		int32_t available;
	};

	static inline
		void init_load(rbload_t* buffer, int32_t size) {
		buffer->head = buffer->begin;
		buffer->end = buffer->begin + size;
		buffer->available = 1;

		for (auto iter = buffer->begin; iter != buffer->end; iter++) {
			iter->status = NO_LOAD;
			iter->pos = 0;
		}
	}

	static inline
		load_t* next_load(const rbload_t* buffer, load_t* const now) {
		load_t* temp = now + 1;
		if (temp == buffer->end) {
			temp = buffer->begin;
		}
		return temp;
	}

	static inline
		load_t* previous_load(const rbload_t* buffer, load_t* const now) {
		return (now == buffer->begin) ? (buffer->end - 1) : (now - 1);
	}

	/* Write one partition of an in-flight load. Partitions are stored in
	   reverse order (IR partition j lives in slot nPartitions-1-j), so a short
	   IR leaves a contiguous run of leading zero slots. Forward fills j = 0,1,2
	   ... so the front runs WITH the sound; reverse fills from nSteps-1 down,
	   so the front runs against it and a single sound hears both IRs. */
	inline void advance_load(load_t* lp, int32_t nSamples, int32_t limit) {
		const int32_t step = lp->pos / nSamples;
		if (step >= lp->nSteps) {
			finish_load(lp, false);
			return;
		}
		const int32_t j = lp->reverse ? (lp->nSteps - 1 - step) : step;
		/* Has the front reached where it should stop? Forward that is a
		   partition count from the bottom, reverse a tap floor it descends to.
		   Either way the rest of the old IR stays where it is - its extent can
		   only have grown. */
		if (lp->reverse ? ((j + 1) * nSamples <= _frontLimit) : (step >= limit)) {
			finish_load(lp, false);
			return;
		}
		/* IR write position, starting with the last! */
		const int32_t n = (nSamples << 1u) * (_nPartitions - 1 - j);
		const size_t src = (size_t)j * (size_t)nSamples;
		if (src < lp->ir.size()) {
			const size_t avail = lp->ir.size() - src;
			const int32_t nCopy = avail < (size_t)nSamples ? (int32_t)avail : nSamples;
			int32_t k = 0;
			/* Fill IR_Data with IR data, zero outside the IR and for the
			   second half (zero padding for the overlap-save block) */
			for (; k < nCopy; k++)
				_IR_Data[n + k] = lp->ir[src + k];
			for (; k < (nSamples << 1u); k++)
				_IR_Data[n + k] = FL(0.0);

			/* calculate FFT (replace in the same buffer) */
			fft->forward(&(_IR_Data[n]));
		}
		else {
			/* past the end of the new IR: erase what the old one left here */
			std::memset(_IR_Data + n, 0, (nSamples << 1u) * sizeof(MYFLOAT));
		}

		lp->pos += nSamples;
		if (lp->pos >= lp->nSteps * nSamples)
			finish_load(lp, true);
	}

	inline void finish_load(load_t* lp, bool complete) {
		lp->status = NO_LOAD;
		lp->ir.clear();
		/* Extent once every load has finished. A complete load defines it on
		   its own; one stopped by the front limit only adds to whatever the
		   previous IR already occupied. */
		_lastTarget = complete
			? lp->target
			: (lp->target > _lastTarget ? lp->target : _lastTarget);
	}
	/*
	**  Input parameters given by user
	*/
	MYFLOAT* aOut{};          // output buffer
	MYFLOAT* aIn{};           // input buffer

	MYFLOAT* iFTNum{};        // impulse respons table
	MYFLOAT* iPartLen{};      // length of impulse response partitions
	// (latency <-> CPU usage)

	MYFLOAT* _kUpdate{};     // Control variable for updating the IR buffer
	// (+1 is start load, -1 is start unload)
	MYFLOAT* _kClear{};      // Clear output buffers

	/*
	** Internal state of opcode maintained outside
	*/
	int32_t _cnt{};            /* buffer position, 0 to partSize - 1       */
	int32_t _nPartitions{};    /* number of convolve partitions            */
	uint32_t _partSize;       /* partition length in sample frames
							 (= iPartLen as integer) */
	int32_t _rbCnt{};          /* ring buffer index, 0 to nPartitions - 1  */
	int32_t _nActive{};        /* partitions that may hold nonzero IR data;
								  multiply loop only covers these */
	int32_t _lastTarget{};     /* _nActive shrinks to this once all loads finish */

	/* IR morph controls - see setMorph() */
	MYFLOAT _morphRate{ FL(1.0) };
	MYFLOAT _rateAcc{};        /* fractional carry of _morphRate */
	int32_t _frontLimit{ INT32_MAX };
	bool _gateOpen{ true };
	bool _reverse{};
	bool _pendingHead{};       /* load accepted mid-block, head not moved yet */
	std::vector<MYFLOAT> _pending;  /* newest IR waiting for a free slot / an
									   unfrozen front */
	bool _hasPending{};

	/* The following pointer point into the auxData buffer */
	MYFLOAT* _tmpBuf{};        /* temporary buffer for accumulating FFTs   */
	MYFLOAT* _ringBuf{};       /* ring buffer of FFTs of input partitions -
							 these buffers are now computed during init */
	MYFLOAT* _IR_Data{};       /* impulse responses (scaled)       */
	MYFLOAT* _outBuf{};        /* output buffer (size=partSize*2)  */

	rbload_t _loader{}; /* Bookkeeping of load/unload operations */
	std::vector<load_t> _slots; /* storage for the load/unload ring */

	inline int32_t buf_bytes_alloc() const {
		int32_t nSmps = (_partSize << 1u);                            /* tmpBuf     */
		nSmps += ((_partSize << 1u) * _nPartitions);           /* ringBuf    */
		nSmps += ((_partSize << 1u) * _nPartitions);           /* IR_Data    */
		nSmps += ((_partSize << 1u));                         /* outBuf */
		nSmps *= (int32_t)sizeof(MYFLOAT);                   /* Buffer type MYFLOAT */
		return nSmps;
	}

	void set_buf_pointers() {
		auto ptr = (MYFLOAT*)_buf.data();
		_tmpBuf = ptr;
		ptr += (_partSize << 1u);
		_ringBuf = ptr;
		ptr += ((_partSize << 1u) * _nPartitions);
		_IR_Data = ptr;
		ptr += ((_partSize << 1u) * _nPartitions);
		_outBuf = ptr;
	}

	/*
	**  Function to multiply the FFT buffers
	**    outBuf - the output of the operation (called with tmpBuf), single channel only
	**    ringBuf - the partitions of the single input signal
	**    IR_data - the impulse response of a particular channel
	**    partSize - size of partition
	**    nPartitions - number of partitions
	**    ringBuf_startPos - the starting position of the ring buffer
	**                       (corresponds to the start of the partition after the
	**                        last filled partition)
	*/
	static void multiply_fft_buffers2(MYFLOAT* outBuf, MYFLOAT* ringBuf, MYFLOAT* IR_Data,
		int32_t partSize, int32_t nPartitions,
		int32_t ringBuf_startPos) {
		MYFLOAT re, im, re1, re2, im1, im2;
		MYFLOAT* rbPtr, * irPtr, * outBufPtr, * outBufEndPm2, * rbEndP;

		/* note: partSize must be at least 2 samples */
		partSize <<= 1; /* locale partsize is twice the size of the partition size */
		/* Finding the index of the last sample pair in the output buffer */
		outBufEndPm2 = (MYFLOAT*)outBuf + (int32_t)(partSize - 2);
		/* The end of the ring buffer */
		rbEndP = (MYFLOAT*)ringBuf + (int32_t)(partSize * nPartitions);
		rbPtr = &(ringBuf[ringBuf_startPos]);    /* Initialize ring buffer pointer */
		irPtr = IR_Data;                        /* Initialize impulse data pointer */
		outBufPtr = outBuf;                    /* Initialize output buffer pointer */

		/* clear output buffer to zero */
		std::memset(outBuf, 0, sizeof(MYFLOAT) * partSize);

		/*
		** Multiply FFTs for each partition and mix to output buffer
		** Note: IRs are stored in reverse partition order
		*/
		do {
			/* wrap ring buffer position */
			if (rbPtr >= rbEndP)
				rbPtr = ringBuf;
			outBufPtr = outBuf;
			*(outBufPtr++) +=
				*(rbPtr++) * *(irPtr++); /* convolve DC - real part only */
			*(outBufPtr++) +=
				*(rbPtr++) * *(irPtr++); /* convolve Nyquist - real part only */
			re1 = *(rbPtr++);
			im1 = *(rbPtr++);
			re2 = *(irPtr++);
			im2 = *(irPtr++);

			/*
			** Status:
			** outBuf + 2, ringBuf + 4, irBuf + 4
			** re = buf + 2, im = buf + 3
			*/

			re = re1 * re2 - im1 * im2;
			im = re1 * im2 + re2 * im1;
			while (outBufPtr < outBufEndPm2) {
				/* complex multiply */
				re1 = rbPtr[0];
				im1 = rbPtr[1];
				re2 = irPtr[0];
				im2 = irPtr[1];
				outBufPtr[0] += re;
				outBufPtr[1] += im;
				re = re1 * re2 - im1 * im2;
				im = re1 * im2 + re2 * im1;
				re1 = rbPtr[2];
				im1 = rbPtr[3];
				re2 = irPtr[2];
				im2 = irPtr[3];
				outBufPtr[2] += re;
				outBufPtr[3] += im;
				re = re1 * re2 - im1 * im2;
				im = re1 * im2 + re2 * im1;
				outBufPtr += 4;
				rbPtr += 4;
				irPtr += 4;
				/*
				** Status:
				** outBuf + 2 + 4n, ringBuf + 4 + 4n, irBuf + 4 + 4n
				** re = buf + 2 + 4n, im = buf + 3 + 4n
				*/
			}
			outBufPtr[0] += re;
			outBufPtr[1] += im;

		} while (--nPartitions);
	}


	tsl::AlignedVector<char> _buf;
};

class ConvolverUniform {
public:
	int32_t totalSize{};

	ConvolverUniform(int32_t _partsize, int _irLenTotal) : fft((_partsize << 1u)) {
		cnt = ringbufcount = nsegs = segsize = rbindex = irLen = irLenTotal = 0;
		tmpbuf = ringbuffer = ir_fft_data = outbuffer = nullptr;
		segsize = _partsize;
		irLenTotal = _irLenTotal;
		// convolver_init(this);
		// return;

		// Compute the number of partitions (total length / partition size)
		nsegs = (irLenTotal + (segsize - 1)) / segsize;
		totalSize = nsegs * segsize;
		/*
		** Calculate the amount of aux space to allocate (in bytes) and
		** allocate if necessary
		** Function of partition size and number of partitions
		*/

		size_t samples = (segsize << 1);    /* tmpBuf     */
		samples += ((segsize << 1) * nsegs); /* ringBuf    */
		samples += ((segsize << 1) * nsegs); /* IR_Data    */
		samples += ((segsize << 1));         /* outBuf */
		//nbytes *= (int32_t)sizeof(MYFLOAT);   /* Buffer type MYFLT */
		mainbuffer.resize(samples);

		/*
		** From here on is initialization of data
		*/

		/* initialize buffer pointers */
		auto ptr = mainbuffer.data();
		tmpbuf = ptr;
		ptr += (segsize << 1);
		ringbuffer = ptr;
		ptr += ((segsize << 1) * nsegs);
		ir_fft_data = ptr;
		ptr += ((segsize << 1) * nsegs);
		outbuffer = ptr;
		ptr += (segsize << 1);

		/* clear ring buffer to zero */
		int32_t n = (segsize << 1) * nsegs;
		memset(ringbuffer, 0, n * sizeof(MYFLOAT));

		/* initialize buffer indices */
		cnt = 0;
		rbindex = 0;

		/* clear IR buffer to zero */
		memset(ir_fft_data, 0, n * sizeof(MYFLOAT));

		/* clear output buffers to zero */
		memset(outbuffer, 0, (segsize << 1) * sizeof(MYFLOAT));
	};


	template<typename Iterator>
	void load_ir(Iterator start, Iterator end) {
		// Compute the number of partitions (total length / partition size)
		ir.assign(start, end);

		irLen = ir.size();
		if (irLen == 0)
			return;
		nsegs = (irLen + (segsize - 1)) / segsize;

		/* initialize buffer pointers */
		auto ptr = mainbuffer.data();
		tmpbuf = ptr;
		ptr += (segsize << 1);
		ringbuffer = ptr;
		ptr += ((segsize << 1) * nsegs);
		ir_fft_data = ptr;
		ptr += ((segsize << 1) * nsegs);
		outbuffer = ptr;
		ptr += (segsize << 1);

		/* clear ring buffer to zero */
		int32_t n = (segsize << 1) * nsegs;
		std::memset(ringbuffer, 0, n * sizeof(MYFLOAT));

		/* initialize buffer indices */
		cnt = 0;
		rbindex = 0;
		/* clear IR buffer to zero */
		std::memset(ir_fft_data, 0, n * sizeof(MYFLOAT));

		/* clear output buffers to zero */
		std::memset(outbuffer, 0, (segsize << 1) * sizeof(MYFLOAT));
	}

	void deactivate() {
		irLen = 0;
	}

	template<typename T1, typename T2>
	void compute(T1* in, T2* out, size_t size) {
		int32_t i, k, n, _rbpos, _nseg, _cnt;
		/* Only continue if initialized */
		if (irLen <= 0) {
			std::memset(out, 0, sizeof(MYFLOAT) * size);
			return;
		}

		/* Pointer to a partition of the ring buffer */
		auto ringbuf = &(ringbuffer[rbindex * (segsize << 1)]);

		/* For each sample in the audio input buffer (length = ksmps) */
		for (size_t nn = 0; nn < size; nn++) {
			/* store input signal in buffer */
			ringbuf[ringbufcount] = in[nn];

			/* copy output signals from buffer (contains data from previous
			   convolution pass) */
			out[nn] = outbuffer[ringbufcount];

			/* is input buffer full ? */
			if (++ringbufcount < segsize)
				continue; /* no, continue with next sample */

			ringbufcount = 0;
			/* Check if there are any IR partitions to load/unload */
			// LOGE("b4 %d %d %d", p->cnt, p->irLen, p->nsegs_new * (segsize << 1));
			_cnt = cnt;

			if (_cnt < nsegs * segsize) {
				_nseg = _cnt / segsize + 1;

				/* IR write position, starting with the last! */
				n = (segsize << 1) * (nsegs - _nseg);

				/* Iterate over IR partitions in reverse order */
				for (k = 0; k < segsize; k++) {
					/* Fill IR_Data with scaled IR data, or zero if outside the IR buffer */
					ir_fft_data[n + k] =
						(_cnt < (int32_t)irLen) ? ir[_cnt] : 0.0;
					_cnt++;
				}

				/* pad second half of IR to zero */
				for (k = segsize; k < (segsize << 1); k++)
					ir_fft_data[n + k] = 0.0;

				/* calculate FFT (replace in the same buffer) */
				fft.forward(&(ir_fft_data[n]));
				cnt += segsize;
			}

			/* Now the partition is filled with input --> start calculate the
			   convolution */

			   /* pad input in ring buffer with zeros to MYFLOAT length */
			for (i = segsize; i < (segsize << 1); i++)
				ringbuf[i] = 0.0;

			/* calculate FFT of input */
			fft.forward(ringbuf);

			/* update ring buffer position */
			rbindex++;
			if (rbindex >= nsegs)
				rbindex = 0;
			_rbpos = rbindex * (segsize << 1);

			/* Move to next partition in ring buffer (used in next iteration to
			   store the next input sample) */
			ringbuf = &(ringbuffer[_rbpos]);

			/* multiply complex arrays --> multiplication in the frequency domain */

			LiveConv::multiply_fft_buffers(tmpbuf, ringbuffer,
				ir_fft_data,
				segsize, nsegs, _rbpos);
			/* inverse FFT */
			fft.backward(tmpbuf);

			/*
			** Copy IFFT result to output buffer
			** The second half is left as "tail" for next iteration
			** The first half is overlapped with "tail" of previous block
			*/
			for (i = 0; i < segsize; i++) {
				outbuffer[i] = tmpbuf[i] + outbuffer[i + segsize];
				outbuffer[i + segsize] = tmpbuf[i + segsize];
			}
		}
	}

	MYFLOAT tick(MYFLOAT in) {
		/* Only continue if initialized */

		/* Pointer to a partition of the ring buffer */
		MYFLOAT* ringbuf = &(ringbuffer[rbindex * (segsize << 1)]);

		/* For each sample in the audio input buffer (length = ksmps) */
		/* store input signal in buffer */
		ringbuf[ringbufcount] = in;

		/* copy output signals from buffer (contains data from previous
		   convolution pass) */
		MYFLOAT out = outbuffer[ringbufcount];

		/* is input buffer full ? */
		if (++ringbufcount < segsize)
			return out; /* no, continue with next sample */

		ringbufcount = 0;
		/* Check if there are any IR partitions to load/unload */
		// LOGE("b4 %d %d %d", p->cnt, p->irLen, p->nsegs_new * (segsize << 1));
		int32_t _cnt = cnt;

		if (_cnt < nsegs * segsize) {
			int32_t _nseg = _cnt / segsize + 1;

			/* IR write position, starting with the last! */
			int32_t n = (segsize << 1) * (nsegs - _nseg);

			/* Iterate over IR partitions in reverse order */
			for (int32_t k = 0; k < segsize; k++) {
				/* Fill IR_Data with scaled IR data, or zero if outside the IR buffer */
				ir_fft_data[n + k] =
					(_cnt < (int32_t)irLen) ? ir[_cnt] : 0.0f;
				_cnt++;
			}

			/* pad second half of IR to zero */
			for (int32_t k = segsize; k < (segsize << 1); k++)
				ir_fft_data[n + k] = 0.0f;

			/* calculate FFT (replace in the same buffer) */
			fft.forward(&(ir_fft_data[n]), &(ir_fft_data[n]));
			cnt += segsize;
		}

		/* Now the partition is filled with input --> start calculate the
		   convolution */

		   /* pad input in ring buffer with zeros to MYFLOAT length */
		for (int32_t i = segsize; i < (segsize << 1); i++)
			ringbuf[i] = 0.0f;

		/* calculate FFT of input */
		fft.forward(ringbuf, ringbuf);

		/* update ring buffer position */
		rbindex++;
		if (rbindex >= nsegs)
			rbindex = 0;
		int32_t _rbpos = rbindex * (segsize << 1);

		/* Move to next partition in ring buffer (used in next iteration to
		   store the next input sample) */
		ringbuf = &(ringbuffer[_rbpos]);

		/* multiply complex arrays --> multiplication in the frequency domain */

		LiveConv::multiply_fft_buffers(tmpbuf, ringbuffer,
			ir_fft_data,
			segsize, nsegs, _rbpos);
		/* inverse FFT */
		fft.backward(tmpbuf, tmpbuf);

		/*
		** Copy IFFT result to output buffer
		** The second half is left as "tail" for next iteration
		** The first half is overlapped with "tail" of previous block
		*/
		for (int32_t i = 0; i < segsize; i++) {
			outbuffer[i] = tmpbuf[i] + outbuffer[i + segsize];
			outbuffer[i + segsize] = tmpbuf[i + segsize];
		}
		return out;
	}

	/*
	** Internal state of opcode maintained outside
	*/
	int32_t cnt; /* buffer position, 0 to partSize - 1       */
	int32_t ringbufcount;
	int32_t nsegs;   /* number of convolve partitions            */
	int32_t segsize; /* partition length in sample frames
					   (= iPartLen as integer) */
	int32_t rbindex; /* ring buffer index, 0 to nPartitions - 1  */
	int32_t irLen, irLenTotal;
	/* The following pointer point into the auxData buffer */
	MYFLOAT* tmpbuf;      /* temporary buffer for accumulating FFTs   */
	MYFLOAT* ringbuffer;  /* ring buffer of FFTs of input partitions -
						these buffers are now computed during init */
	MYFLOAT* ir_fft_data; /* impulse responses (scaled)       */
	MYFLOAT* outbuffer;   /* output buffer (size=partSize*2)  */
	tsl::AlignedVector<MYFLOAT> mainbuffer; /* Aux data buffer allocated in init pass */
	FFT fft;
	std::vector<MYFLOAT> ir;
};

#define PARTSIZE1 32
#define PARTSIZE2 128
#define PARTSIZE3 1024
#define PARTSIZE4 4096

#define ISSMALLER(a, b) (a) < (b) ? (a) : (b);

#include <complex>

class ConvolverNonUniform {
public:
	/* Each LiveConv stage outputs with a latency of exactly its partition size,
	   so stage k must hold the IR taps [P_k, P_{k+1}) — segment lengths are the
	   DIFFERENCES between consecutive partition sizes, not the sizes themselves. */
	ConvolverNonUniform(size_t maxIRLen) : firsize(PARTSIZE1/*bufsize % PARTSIZE1*/),
		c{ {PARTSIZE1, PARTSIZE2 - PARTSIZE1},
		  {PARTSIZE2, PARTSIZE3 - PARTSIZE2},
		  {PARTSIZE3, PARTSIZE4 - PARTSIZE3},
		  {PARTSIZE4,
				  static_cast<int>(maxIRLen - PARTSIZE4)} } {
		_irLen.store(static_cast<int32_t>(maxIRLen), std::memory_order_relaxed);
	}

	/* IR morph controls, fanned out to the stages once per block from the
	   audio thread.
	   rate     1.0 = the front travels at one tap per sample, the rate a sound
	            travels along the IR, so each sound keeps the IR that was
	            current when it entered. Below 1 the front falls behind the
	            sound, above 1 it overtakes it; 0 parks it.
	   front    0..1 of the current IR length. The front stops there and the
	            rest of the previous IR stays in place, so the result is a
	            hybrid split at that point. 0 freezes completely and incoming
	            IRs are ignored.
	   reverse  ONE global front sweeps down from the end of the IR to the
	            start, releasing each stage as it passes that stage's top. The
	            direct FIR is the last thing it reaches. */
	void setMorph(MYFLOAT rate, MYFLOAT front, bool reverse) {
		if (front < FL(0.0))
			front = FL(0.0);
		else if (front > FL(1.0))
			front = FL(1.0);
		_rate = rate < FL(0.0) ? FL(0.0) : rate;
		const MYFLOAT len = (MYFLOAT)_irLen.load(std::memory_order_relaxed);
		/* The replaced range is [0, front*len) forward and [(1-front)*len, len)
		   in reverse, so each stage gets the part of that range covered by its
		   own segment - a stage entirely outside it keeps its old IR. */
		static constexpr int32_t base[4] = { PARTSIZE1, PARTSIZE2, PARTSIZE3, PARTSIZE4 };
		const MYFLOAT lo = (FL(1.0) - front) * len;
		_sweepEnd = lo;

		/* Loads running in opposite directions walk into each other, and
		   whichever reaches a partition LAST owns it - which above the crossing
		   point is the OLDER one, leaving a permanently stale band. So a
		   direction change waits for the current sweep to drain: gates close so
		   nothing NEW starts (the new IR sits in each stage's pending slot),
		   but the in-flight loads keep their limits - a limit of 0 would be
		   read as "finish here" and truncate them half-written. */
		if (reverse != _dir && anyLoading()) {
			for (int32_t k = 0; k < 4; k++)
				c[k].setMorph(_rate, false, _lastLimit[k], _dir);
			_headOpen = false;
			return;
		}
		const bool wasReverse = _dir;
		_dir = reverse;

		if (!reverse) {
			for (int32_t k = 0; k < 4; k++) {
				/* front 1 means no ceiling at all, not "stop at len": the walk
				   runs past the new IR's end to erase whatever a longer
				   previous IR left up there */
				const MYFLOAT local = front * len - (MYFLOAT)base[k];
				const int32_t lim = front >= FL(1.0) ? INT32_MAX
					: (local <= FL(0.0) ? 0 : (int32_t)local);
				_lastLimit[k] = lim;
				c[k].setMorph(_rate, lim > 0, lim, false);
			}
			/* forward, the front starts at tap 0 and so covers the FIR at once */
			_headOpen = _rate > FL(0.0) && front > FL(0.0);
			return;
		}

		/* Reverse is a single sweep across the whole IR. One global front
		   (_sweepPos, advanced per sample in tick) descends from len, and a
		   stage stays gated shut until the front passes that stage's top. Once
		   released it walks its own segment top-down at the same rate - a
		   stage's segment length equals its own traversal time, so it stays in
		   step with the global front and hands over to the next stage exactly
		   as the front crosses the boundary. */

		/* where each stage's data ends - its own top, or the end of the IR */
		MYFLOAT hi[4];
		for (int32_t k = 0; k < 4; k++)
			hi[k] = k < 3 && (MYFLOAT)base[k + 1] < len ? (MYFLOAT)base[k + 1] : len;

		/* Park the front at the top of the IR between sweeps, so the next IR
		   to arrive starts a fresh one from the end. This has to happen here
		   and not in tick(): the gates below are computed from _sweepPos, and
		   a stale value would throw all of them open at once.
		   A sweep counts as over only once the front has bottomed out AND
		   every stage the front actually reaches has taken its IR. Gate checks
		   are block-quantised, so the short stages near the start of the IR
		   usually get their turn a block or two after the front bottoms out -
		   parking on "front is down" alone cuts them off, which is what
		   strands stage 0 and the direct FIR. */
		bool waiting = _hasPendingHeadTaps && lo <= FL(0.0);
		for (int32_t k = 0; k < 4; k++)
			if (hi[k] > lo && c[k].pending())
				waiting = true;
		if (!wasReverse || (_sweepPos <= _sweepEnd && !anyLoading() && !waiting))
			_sweepPos = len;

		/* A stage is released once the front has passed its top AND the stage
		   above it has actually finished. The second half matters: a stage can
		   only accept and write on its own partition boundaries, so the tail
		   stage lags the ideal front by up to one 4096-tap partition. Opening
		   the next stage on schedule regardless would leave a hole at the
		   handover - taps 4096..8192 still old while 1024..4096 are new. */
		for (int32_t k = 3; k >= 0; k--) {
			const bool above = k == 3 || !(c[k + 1].loading() || c[k + 1].pending());
			const bool open = _sweepPos <= hi[k] && above && hi[k] > lo;
			/* reverse limit is a FLOOR: the walk starts at the top of whatever
			   the old and new IR jointly occupy - so the erase region above a
			   shortened IR is covered for free - and descends to here */
			const MYFLOAT floorTaps = lo - (MYFLOAT)base[k];
			const int32_t lim = floorTaps <= FL(0.0) ? 0 : (int32_t)floorTaps;
			_lastLimit[k] = lim;
			c[k].setMorph(_rate, open, lim, true);
		}
		/* the direct FIR is the last thing the sweep reaches */
		_headOpen = _rate > FL(0.0) && lo <= FL(0.0)
			&& _sweepPos <= (MYFLOAT)PARTSIZE1
			&& !(c[0].loading() || c[0].pending());
	}

	int32_t finalSize() {
		return PARTSIZE1 + c[0].totalSize + c[1].totalSize + c[2].totalSize + c[3].totalSize;
	}

	/* Audio thread only. True once the previous IR has been fully swapped in
	   and nothing is waiting on a frozen front - the evolving SPECTRAL DELAY
	   algorithms use this to pace their self-regenerations, so a slow or
	   frozen morph front naturally slows evolution down instead of piling
	   loads up in the queues. */
	bool idle() const {
		if (_hasPendingHeadTaps)
			return false;
		for (const auto& s : c)
			if (s.loading() || s.pending())
				return false;
		return true;
	}

	inline MYFLOAT tick(MYFLOAT in) {
		/* Move the reverse sweep's global front. Parking it back at the top of
		   the IR is setMorph's job (it has to happen before the gates are
		   computed); here it only descends, at one tap per sample times the
		   morph rate, with the stage gates opening behind it. */
		if (_dir && _sweepPos > _sweepEnd) {
			_sweepPos -= _rate;
			if (_sweepPos < _sweepEnd)
				_sweepPos = _sweepEnd;
		}

		/* Apply pending head-tap updates on the audio thread (newest wins) —
		   consistent with how the partitioned stages swap their IR data, and
		   avoids the loader thread writing irdirect mid-sum. */
		while (auto h = headQueue.try_pop()) {
			_pendingHeadTaps = *h;
			_hasPendingHeadTaps = true;
		}
		/* A frozen front freezes the near field too - but the taps are held,
		   not dropped, so releasing it brings the current IR in rather than
		   leaving these 32 taps behind on the previous one. */
		if (_hasPendingHeadTaps && _headOpen) {
			std::memcpy(irdirect, _pendingHeadTaps.data(), sizeof(irdirect));
			_hasPendingHeadTaps = false;
		}

		/* Direct FIR over the first PARTSIZE1 taps. Reg is a DOUBLED circular
		   buffer written at both _regPos and _regPos+PARTSIZE1, so the history
		   is always contiguous from _regPos and the sum is a straight walk -
		   no shifting and no wrap test in the inner loop. The previous version
		   moved all 32 elements down one slot every sample; measured in
		   isolation that shift plus its sum cost 6.2 ms per 5 s of audio
		   against 3.8 ms here, which is about a fifth of a short-IR
		   convolver's total. Tap order is unchanged: Reg[_regPos] is the
		   newest sample and pairs with irdirect[0], exactly as before. */
		MYFLOAT y = 0.0;
		if (firsize) {
			if (--_regPos < 0)
				_regPos = (int32_t)firsize - 1;
			Reg[_regPos] = Reg[_regPos + (int32_t)firsize] = in;

			const MYFLOAT* r = &Reg[_regPos];
			for (int32_t k = 0; k < (int32_t)firsize; k++)
				y += irdirect[k] * r[k];
		}

		return (c[0].tick(in) + c[1].tick(in) + c[2].tick(in) + c[3].tick(in) + y);;
	}

	void loadIR(std::vector<MYFLOAT>& ir) {
		load(ir);
	}

	template<typename Iterator>
	void loadIR(Iterator start, Iterator _end) {
		std::vector<MYFLOAT> ir(start, _end);
		load(ir);
	}


	template<typename T1, typename T2, typename T3>
	inline static T3 dist(T1 a, T2 b) {
		T3 tmp = a - b;
		return tmp < 0 ? -tmp : tmp;
	}

	int32_t count{};

	static int32_t finalSamples(int samples) {
		if (samples <= PARTSIZE4)
			return PARTSIZE4;
		return ((samples - PARTSIZE4 + (PARTSIZE4 - 1)) / PARTSIZE4) * PARTSIZE4 + PARTSIZE4;
	}

private:
	size_t firsize;
	LiveConv c[4];
	MYFLOAT irdirect[PARTSIZE1]{};
	/* doubled so the newest PARTSIZE1 samples are always contiguous - see the
	   head FIR in tick() */
	MYFLOAT Reg[PARTSIZE1 * 2]{};
	int32_t _regPos{};
	/* head taps travel through a queue and are applied in tick(), so only the
	   audio thread ever writes irdirect */
	tsl::RingBufferMPSCQueue<std::array<MYFLOAT, PARTSIZE1>, 16> headQueue{};
	bool _headOpen{ true };        /* audio thread only, from setMorph() */
	std::array<MYFLOAT, PARTSIZE1> _pendingHeadTaps{};
	bool _hasPendingHeadTaps{};
	std::atomic<int32_t> _irLen{}; /* length of the most recent IR, in taps -
									  the front control is a fraction of it */
	/* reverse-sweep state, audio thread only */
	MYFLOAT _sweepPos{};           /* global front, descending, in taps */
	MYFLOAT _sweepEnd{};           /* where the sweep stops = (1-front)*len */
	MYFLOAT _rate{ FL(1.0) };
	bool _dir{};                   /* direction of the loads currently in flight */
	int32_t _lastLimit[4]{ INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX };
	/* per-stage limits as last issued - re-issued unchanged while a direction
	   change is being held, so in-flight loads finish instead of truncating */

	bool anyLoading() const {
		return c[0].loading() || c[1].loading() || c[2].loading() || c[3].loading();
	}

	/* Stage k covers IR taps [P_k, P_{k+1}), matching its latency; the direct
	   FIR covers [0, PARTSIZE1). An empty range means "gradually unload".
	   Index arithmetic only — iterators must never be formed past end(). */
	void load(std::vector<MYFLOAT>& ir) {
		const size_t len = ir.size();
		_irLen.store((int32_t)len, std::memory_order_relaxed);
		if (firsize != 0) {
			std::array<MYFLOAT, PARTSIZE1> head{};
			const size_t n = len < firsize ? len : firsize;
			for (size_t i = 0; i < n; i++)
				head[i] = ir[i];
			headQueue.try_push(head);
		}
		static constexpr size_t bounds[4] = { PARTSIZE1, PARTSIZE2, PARTSIZE3, PARTSIZE4 };
		for (int32_t k = 0; k < 4; k++) {
			const size_t lo = bounds[k] < len ? bounds[k] : len;
			const size_t hi = (k < 3 && bounds[k + 1] < len) ? bounds[k + 1] : len;
			c[k].load_ir(ir.begin() + lo, ir.begin() + hi);
		}
	}
};

class GainSmoother {
public:
	inline MYFLOAT tick(MYFLOAT smoothCoeff, MYFLOAT in) {
		auto out = in * _smoothed;
		_smoothed += smoothCoeff * (currentGain - _smoothed);
		if (std::abs(in) > nextGain)
			nextGain = std::abs(in);
		if (++count == 1024) {
			currentGain = nextGain;
			count = 0;
		}
		return out;
	}

private:
private:
	MYFLOAT _smoothed{}, nextGain{}, currentGain{};
	int32_t count{};
};

constexpr MYFLOAT kMin = 0.001;

/* Energy an IR of one second of full-scale noise carries under the RAW
   scaling, at 48 kHz: CONVMYFLT * sqrt(L/3) = 3.05e-5 * 126.5. CREVNORM's RMS
   mode normalises to this, so turning normalisation on re-levels IRs against
   each other WITHOUT moving the output away from where RAW had it. Deliberately
   a constant and not derived from the sample rate - the point of the mode is
   that the wet level stops depending on anything but GAIN. */
#define CREVNORMENERGY FL(0.004)

class Convolver : public Effect, private Limiter<MYFLOAT> {
public:
	Convolver(TRACK* track, int32_t chan) : Effect(track, chan, SPACE_CONVOLVER, MONOEFFECT),
		convolverNonUniform(_STATE->sr * 10) {
		fol = &track->_STATE->followerMap[track->index].at(CREVMIX);
		_bypass = &track->bypass[SPACE_CONVOLVER];
		_mix = &_STATE->params[track->index][CREVMIX];
		_gain = &_STATE->params[track->index][CREVGAIN];
		_smooth2 = dbToLinear60(_gain->load());
		irloaded = false;
		Limiter<MYFLOAT>::init(_STATE->sr, -1, 50);
		/* CREVERB2 controls */
		_morphrate = &_STATE->params[track->index][CREVMORPHRATE];
		_morphfront = &_STATE->params[track->index][CREVMORPHFRONT];
		_morphrev = &_STATE->params[track->index][CREVMORPHREV];
		_autoreload = &_STATE->params[track->index][CREVAUTO];
		_setupThread = std::thread(&Convolver::setupFunc, this);
	}

	/* Shutdown copied from SpectralDelay, for the same two reasons: publish
	   the exit request BEFORE raising the flag (a worker sitting in
	   wait(false) is released by the flag and must find shouldExit already
	   set), and never wait for the worker to go idle first - it ends its pass
	   with a bare clear() and no notify, so waiting deadlocks. The re-check
	   after clear() in setupFunc catches a worker that was mid-pass. */
	~Convolver() override {
		shouldExit.store(true, std::memory_order_release);
		lock.test_and_set(std::memory_order_release);
		lock.notify_all();
		if (_setupThread.joinable()) _setupThread.join();
	}

	/* LOAD IR button. This used to generate the IR inline - and the button
	   pushes it through toAudioThreadQueue, so all of it (a vector resize of
	   up to 10 s of samples, two passes over it, a std::reverse) ran ON THE
	   AUDIO THREAD. Now it only validates and hands the source track to each
	   channel's worker; the generation happens off-thread and arrives through
	   the same lock-free path GENCREVERB's IRs use. */
	static void loadIR(TRACK* carrier, TRACK* mod) {
		std::shared_ptr<Effect> fx[2] = { nullptr, nullptr };
		auto _appState = carrier->_appState;
		auto rec = mod->filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state == nullptr || rec->off == 0) {
			showToast(_STATE, "EMPTY TRACK.");
			return;
		}
		// Find convolver effects
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto n = carrier->fx_queue[i]._first;
			while (n) {
				if (n->data->_id == SPACE_CONVOLVER) {
					fx[i] = n->data;
					break;
				}
				n = n->next;
			}
		}

		if (fx[0] == nullptr) {
			char text[100];
			snprintf(text, sizeof(text), "%s Load IR. Effect has to be active.", carrier->name);
			showToast(_STATE, text);
			return;
		}

		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto convolver = static_cast<Convolver*>(fx[i].get());
			if (!convolver) continue;
			convolver->requestBuild(mod->index);
		}
	}

	/* Point this convolver at an IR source and ask for a rebuild. Safe from
	   any thread: it publishes two atomics, and compute() hands the request
	   to the worker when the worker is free. */
	void requestBuild(int32_t srcTrack) {
		_srcTrack.store(srcTrack, std::memory_order_release);
		_buildPending.store(true, std::memory_order_release);
	}

	void compute(MYFLOAT* buf, int32_t size) override {
		/* IR replacement front - the same engine call GENCREVERB makes. At the
		   defaults (RATE 1, FRONT 1, forward) this reproduces exactly the
		   behaviour the effect had before the controls existed, because those
		   are the values ConvolverNonUniform already sat at.
		   RATE is a log param with an offset so the bottom of its range is an
		   exact 0. NOTE that RATE 0 and FRONT 0 are BOTH hard freezes, and
		   they are independent: a new IR is then held, not dropped, and
		   arrives the moment the front is released. */
		MYFLOAT morphr = LOG2NORMAL(_morphrate->load()) - SPECDELRATEOFFS;
		if (morphr < FL(0.0))
			morphr = FL(0.0);
		convolverNonUniform.setMorph(morphr, _morphfront->load(),
			_morphrev->load() != 0.);

		/* AUTO RELOAD: rebuild whenever anything that shapes the IR moves, so
		   the envelope, REVERSE, BOUNCE and MAX stop being load-button-only.
		   Paced by idle(): a knob drag then costs one rebuild per completed
		   swap instead of one per block, and a parked morph front pauses
		   rebuilds instead of piling them into the queues - the same rule the
		   evolving GENCREVERB algorithms regenerate on.
		   An explicit LOAD IR press is NOT idle-gated; it only waits for the
		   worker itself, so the button still responds with the front parked. */
		/* Once the effect is on its way out, stop feeding the worker: the
		   destructor joins it, and that join runs on the AUDIO THREAD (the fx
		   queue is reaped inside the per-track loop). destroyRequested is set
		   a fade before readyToDestroy, so by the time the object is deleted
		   the worker has long finished its last pass and the join is free. */
		if (!destroyRequested && _srcTrack.load(std::memory_order_acquire) >= 0) {
			const MYFLOAT sig = irSig();
			if (_autoreload->load() != 0.) {
				if (sig != _sigOld) {
					_sigOld = sig;
					_autoDue = true;
				}
			}
			else {
				/* track the fingerprint while AUTO is off, so switching it on
				   does not fire a rebuild for edits made while it was off */
				_sigOld = sig;
				_autoDue = false;
			}
			const bool explicitReq = _buildPending.load(std::memory_order_acquire);
			if ((explicitReq || (_autoDue && convolverNonUniform.idle())) &&
				!lock.test_and_set(std::memory_order_acquire)) {
				/* cleared only now that the worker has actually taken it - a
				   busy worker leaves the request standing for the next block */
				_buildPending.store(false, std::memory_order_release);
				_autoDue = false;
				lock.notify_one();
			}
		}

		if (!irloaded.load()) {
			for (int32_t i = 0; i < size; i++) {
				smmixgain(0, 1);
			}
			return;
		}

		const MYFLOAT gain = dbToLinear60(_gain->load());
		MYFLOAT mix;
		const bool bypass = (*_bypass || destroyRequested);
		if (bypass) {
			mix = 0;
		}
		else {
			mix = *_mix;
		}
		const bool env_on = fol->prepare(_chan);
		MYFLOAT* envbuf = nullptr;
		if (env_on) {
			auto src = fol->source.load();
			envbuf = src == _track->index ? buf : _DATA->tracks[src]->envf_buffer[_chan];
		}

		for (int32_t i = 0; i < size; i++) {
			MYFLOAT mixfin = (env_on && !bypass) ? (_chan == 0 ? fol->detectL(envbuf[i]) : fol->detectR(envbuf[i])) : _smooth1;
			MYFLOAT mixsrcfin = 1. - mixfin;
			// MYFLOAT val = (*in * mixsrc + *out+
			buf[i] = buf[i] * mixsrcfin +
				Limiter<MYFLOAT>::tick(convolverNonUniform.tick(buf[i])) * mixfin * _smooth2;
			smmixgain(mix, gain);
		}
	}

	std::atomic<bool> irloaded;
	int32_t cnt;
	ConvolverNonUniform convolverNonUniform;
private:
	/* Fingerprint of everything that shapes the IR. Weighted, not a plain sum:
	   the envelope points move against each other all the time and equal and
	   opposite drags on two of them would cancel in a bare sum and skip the
	   rebuild. */
	MYFLOAT irSig() const {
		const auto ti = _track->index;
		MYFLOAT s = _STATE->params[ti][CREVMAXSIZE].load() * FL(1.0)
			+ _STATE->params[ti][CREVREVERSE].load() * FL(3.1)
			+ _STATE->params[ti][CREVBOUNCE].load() * FL(7.3)
			+ _STATE->params[ti][CREVNORM].load() * FL(11.9);
		MYFLOAT w = FL(0.37);
		for (int32_t i = 0; i < 5; i++) {
			s += _STATE->params[ti][CREVENVX0 + i].load() * w;
			w += FL(0.13);
			s += _STATE->params[ti][CREVENVY0 + i].load() * w;
			w += FL(0.13);
		}
		return s;
	}

	/* IR level normalisation, applied last so it also takes the envelope into
	   account. RAW (0) is the historical scaling and nothing else, so no saved
	   session changes loudness. Both normalised modes are referenced to what
	   RAW produces for a full-scale source, so they re-level quiet or short
	   IRs without jumping the output:
	     PEAK - loudest tap = full scale. Keeps the transient shape; the wet
	            level still rises with IR density.
	     RMS  - unit energy against CREVNORMENERGY. This is the one that makes
	            GAIN portable between IRs: convolution gain goes with the
	            square root of IR energy, i.e. with sqrt(length) for a given
	            density, so under RAW a 10x longer tail is 10 dB louder and
	            here it is the same loudness. */
	static void applyNorm(std::vector<MYFLOAT>& ir, int32_t mode) {
		if (mode <= 0 || ir.empty())
			return;
		MYFLOAT sc;
		if (mode == 1) {
			MYFLOAT pk = FL(0.0);
			for (auto v : ir) {
				const MYFLOAT a = v < FL(0.0) ? -v : v;
				if (a > pk) pk = a;
			}
			if (pk < FL(1e-30))
				return;
			sc = (MYFLOAT)CONVMYFLT / pk;
		}
		else {
			MYFLOAT e = FL(0.0);
			for (auto v : ir)
				e += v * v;
			if (e < FL(1e-30))
				return;
			sc = CREVNORMENERGY / std::sqrt(e);
		}
		for (auto& v : ir)
			v *= sc;
	}

	/* Per-instance IR generator thread. One pass = one complete IR for THIS
	   channel, handed over with loadIR() - the same lock-free path the
	   SPECTRAL DELAY setup thread uses, so the audio thread never allocates
	   and never blocks on it. */
	void setupFunc() {
		std::vector<MYFLOAT> data;
		std::vector<MYFLOAT> env(ADSRSIZE);
		do {
			lock.wait(false, std::memory_order_acquire);
			if (shouldExit.load(std::memory_order_acquire))
				break;
			buildIR(data, env);
			lock.clear(std::memory_order_release);
			lock.notify_all();
			/* the destructor may have published shouldExit during the pass;
			   without this the worker goes straight back into wait(false) on
			   a flag it just cleared and never sees it */
			if (shouldExit.load(std::memory_order_acquire))
				break;
		} while (true);
	}

	void buildIR(std::vector<MYFLOAT>& data, std::vector<MYFLOAT>& env) {
		const int32_t srcIdx = _srcTrack.load(std::memory_order_acquire);
		if (srcIdx < 0)
			return;
		auto mod = _DATA->tracks[srcIdx];
		if (!mod)
			return;
		auto rec = mod->filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state == nullptr || rec->off == 0) {
			toast("EMPTY TRACK.");
			return;
		}

		const auto ti = _track->index;
		const auto startpos = state->off_start.load();
		const auto stoppos = state->off_stop.load();
		const auto dir = state->playbackDir.load();
		const auto offset = state->offset.load();
		const auto bounce = _STATE->params[ti][CREVBOUNCE].load() == 1.0;
		const auto maxlength = LOG2NORMAL(_STATE->params[ti][CREVMAXSIZE].load()) *
			_STATE->sr * 0.001;
		const bool reverse = _STATE->params[ti][CREVREVERSE].load() == 1.0;
		const auto normMode = (int32_t)_STATE->params[ti][CREVNORM].load();

		// Validate IR length
		if (stoppos == startpos ||
			(!bounce && dir == 1.0 && DISTANCE(offset, stoppos) == 0) ||
			(!bounce && dir == -1.0 && DISTANCE(offset, startpos) == 0)) {
			toast("IR too short.");
			return;
		}

		const auto first = dir == 1. ? stoppos : startpos;
		const auto second = (first == stoppos) ? startpos : stoppos;

		size_t len = DISTANCE(offset, first) + (bounce ? DISTANCE(first, second) : 0);
		if (len <= 0) {
			toast("IR too short.");
			return;
		}

		/* MAX is a hard ceiling on the convolver's capacity as well, so say so
		   rather than reporting the truncated length as if it were the
		   selection */
		const bool truncated = (MYFLOAT)len > maxlength;
		if (truncated)
			len = (size_t)maxlength;

		/* A mono recording in a stereo session has an empty buffer[1]. Falling
		   back to channel 0 keeps both convolvers on the SAME IR: the old code
		   skipped the channel, which left one side playing whatever IR it had
		   loaded previously, silently and with no way to tell. */
		auto* fb = &rec->buffer[_chan];
		if (fb->empty() && _chan != 0)
			fb = &rec->buffer[0];
		if (fb->empty()) {
			toast("EMPTY TRACK.");
			return;
		}

		const auto fb_begin = fb->begin();
		const auto fb_end = fb->end();
		auto start = fb_begin + (size_t)offset;
		auto stop = fb_begin + (size_t)first;
		if (start >= fb_end || stop > fb_end) {
			toast("IR out of range.");
			return;
		}

		try {
			data.resize(len);   /* every element is written below */
		}
		catch (std::bad_alloc&) {
			toast("Out of memory.");
			return;
		}

		// Load envelope parameters
		MYFLOAT xx[5];
		MYFLOAT yy[5];
		for (int32_t i = 0; i < 5; i++) {
			xx[i] = _STATE->params[ti][CREVENVX0 + i].load();
			yy[i] = (std::pow(10.0f, _STATE->params[ti][CREVENVY0 + i].load() / 20.0) - kMin) / (1.0 - kMin);
		}
		tsl::envelope::compute<MYFLOAT>[0](env.data(), ADSRSIZE, xx, yy, 4, true);

		// Precompute envelope scaling
		const MYFLOAT env_inc = (MYFLOAT)ADSRSIZE / (MYFLOAT)len;

		MYFLOAT* out = data.data();
		int32_t direction = (int32_t)dir;

		// Sample the IR with ping-pong/bounce
		for (size_t samples = 0; samples < len; samples++) {
			*out++ = *start;
			start += direction;

			// Bounce at stop point
			if (start == stop)
				direction = -direction;

			// Clamp to buffer bounds
			if (start >= fb_end) {
				start = fb_end - 1;
				direction = -1;
			}
			else if (start < fb_begin) {
				start = fb_begin;
				direction = 1;
			}
		}

		// Apply envelope in-place
		MYFLOAT env_pos = 0.0f;
		for (size_t j = 0; j < len; j++) {
			const int32_t env_idx = (int32_t)(env_pos) & ADSRANDMASK;
			data[j] *= (CONVMYFLT * env[env_idx]);
			env_pos += env_inc;
		}
		// Reverse if needed
		if (reverse)
			std::reverse(data.begin(), data.end());

		applyNorm(data, normMode);

		convolverNonUniform.loadIR(data);
		irloaded.store(true);

		/* one toast per load, not one per channel */
		if (_chan == 0) {
			std::stringstream s;
			s << "Loading IR. Length: " << (MYFLOAT)len / _STATE->sr << " seconds";
			if (truncated)
				s << " (truncated to MAX)";
			s << ".";
			toast(s.str().c_str());
		}
	}

	void toast(const char* text) const { showToast(_STATE, text); }

	Follower* fol{};
	const std::atomic<MYFLOAT>* _morphrate{}, * _morphfront{}, * _morphrev{},
		* _autoreload{};
	/* IR source track, -1 until a LOAD IR press names one. The IR itself is
	   not part of a saved session, so this is deliberately per-instance and
	   not a parameter. */
	std::atomic<int32_t> _srcTrack{ -1 };
	std::atomic<bool> _buildPending{};
	std::atomic<bool> shouldExit{};
	std::atomic_flag lock = ATOMIC_FLAG_INIT;
	std::thread _setupThread;
	/* audio thread only */
	MYFLOAT _sigOld{};
	bool _autoDue{};
};


/* ---- v11 looped-IR kernel generator (PUREVERB) ----
   One build() call is one complete draw: delay-density -> per-bin group
   delays (persistent, walked between builds) -> warped flat-magnitude
   spectrum -> POCS -> un-warp by e^(-lam t) -> DAMP rate tilt -> energy
   normalise. See the big header block above PureKernelGen::build in
   Convolver.cpp for the derivation of every step. One instance per
   independent draw (SpectralDelay is mono, so one per channel instance). */
struct PureKernelGen {
	/* persistent per-bin group delays (setup thread only): the draw is
	   synthesised from an explicit bounded delay-density - phases are the
	   integral of tau over frequency - and evolution is a Metropolis walk
	   on the tau values (re-rolls, random phase walks and deterministic
	   precession all rang - see the build() header). */
	std::vector<MYFLOAT> tau;
	/* fills ir[0..n) and returns the loop gain to pair with it: the
	   kernel is energy-normalised for constant wet loudness and the
	   inverse of that normalisation lands in the gain, so the LOOP
	   product g*|FFT(h*e^(lam t))| - the condition that pins every pole
	   to the RT60 circle - is exact regardless of level. */
	/* bandA/bandB: kernel band as a fraction of Nyquist. Out-of-band bins
	   are held at ZERO - inside the synthesis, the calibration probe and
	   the POCS spectral projection (a zero bin is under the cap, and {0}
	   is convex, so the pole bound and POCS convergence both survive).
	   The band edge softens through the envelope clip, which is fine. */
	MYFLOAT build(MYFLOAT* ir, int n, MYFLOAT sr, MYFLOAT lam, MYFLOAT dmp,
		MYFLOAT bandA, MYFLOAT bandB, std::mt19937& rng);

private:
	static constexpr int KGRID = 1024;
	std::vector<MYFLOAT> spec, dense, dense2, cap;
	/* current + previous delay-density CDF: a density change QUANTILE-
	   remaps the persistent taus (monotone, instant re-equilibration)
	   instead of waiting ~((b-a)/sigma)^2 Metropolis steps */
	std::vector<MYFLOAT> cdf, cdfOld;
	MYFLOAT gridA{}, gridB{}, oldA{}, oldB{}, tAold{ FL(-1.0) };
};

class SpectralDelay : public Effect {
public:
	/* Algorithm ids. 4.. repurpose the ids of the dead experimental generators
	   (never selectable in the UI, so nothing to migrate). Order must match
	   specdelmodes/specdel_subspaces in gs_common.h - static_asserts in
	   Convolver.cpp tie them together. */
	enum {
		SPECDELUP,
		SPECDELDOWN,
		SPECDELRAND,
		SPECDELGAUSS,
		SPECDELFEEDBACK,
		SPECDELSTEPPED,
		SPECDELRIPPLE,
		SPECDELOCTAVE,
		SPECDELDRIFT,
		SPECDELBARBER,
		SPECDELTIDE,
		SPECDELADAPT,
		SPECDELVERB,
		SPECDELPUREVERB,
		SPECDELNUMMODES
	};

	SpectralDelay(TRACK* track, int32_t chan) : Effect(track, chan, SPACE_SPECDEL, MONOEFFECT) {
		del = &_STATE->params[track->index][SPECDELDEL];
		bounda = &_STATE->params[track->index][SPECDELBOUNDA];
		boundb = &_STATE->params[track->index][SPECDELBOUNDB];
		mode = &_STATE->params[track->index][SPECDELMODE];
		morphrate = &_STATE->params[track->index][SPECDELMORPHRATE];
		morphfront = &_STATE->params[track->index][SPECDELMORPHFRONT];
		morphrev = &_STATE->params[track->index][SPECDELMORPHREV];
		shape = &_STATE->params[track->index][SPECDELSHAPE];
		randevo = &_STATE->params[track->index][SPECDELRANDEVO];
		gaussevo = &_STATE->params[track->index][SPECDELGAUSSEVO];
		fb = &_STATE->params[track->index][SPECDELFB];
		damp = &_STATE->params[track->index][SPECDELDAMP];
		steps = &_STATE->params[track->index][SPECDELSTEPS];
		ripples = &_STATE->params[track->index][SPECDELRIPPLES];
		ripplph = &_STATE->params[track->index][SPECDELRIPPLEPH];
		root = &_STATE->params[track->index][SPECDELROOT];
		driftamt = &_STATE->params[track->index][SPECDELDRIFTAMT];
		driftrate = &_STATE->params[track->index][SPECDELDRIFTRATE];
		barbn = &_STATE->params[track->index][SPECDELBARBN];
		barbspeed = &_STATE->params[track->index][SPECDELBARBSPEED];
		tidespeed = &_STATE->params[track->index][SPECDELTIDESPEED];
		adepth = &_STATE->params[track->index][SPECDELADEPTH];
		arate = &_STATE->params[track->index][SPECDELARATE];
		ainv = &_STATE->params[track->index][SPECDELAINV];
		decay = &_STATE->params[track->index][SPECDELDECAY];
		damphf = &_STATE->params[track->index][SPECDELDAMPHF];
		verbrt60 = &_STATE->params[track->index][SPECDELVERBRT60];
		verbevo = &_STATE->params[track->index][SPECDELVERBEVO];
		verbdamp = &_STATE->params[track->index][SPECDELVERBDAMP];
		verbsize = &_STATE->params[track->index][SPECDELVERBSIZE];
		verbxfeed = &_STATE->params[track->index][SPECDELVERBXFEED];
		_mix = &_STATE->params[track->index][SPECDELMIX];
		_gain = &_STATE->params[track->index][SPECDELGAIN];
		_smooth2 = dbToLinear60(_gain->load());
		_bypass = &track->bypass[SPACE_SPECDEL];
		fol = &track->_STATE->followerMap[track->index].at(SPECDELMIX);
		_adaptRing.resize(ADAPTN, FL(0.0));
		/* a few per mille of geometry offset per channel/track: the LONGVERB
		   FDNs are then different rooms and their tails decorrelate instead
		   of collapsing to the centre */
		_fdnDetune = FL(1.0) + FL(0.017) * (MYFLOAT)chan +
			FL(0.006) * (MYFLOAT)(track->index & 3);
		_setupThread = std::thread(&SpectralDelay::setupFunc, this);
	}

	~SpectralDelay() override {
		/* Publish the exit request BEFORE touching the flag, and do NOT
		   wait for the worker to go idle. The old body spun
		   `while (test_and_set()) wait(true)` to grab the flag first, and
		   that deadlocked two independent ways:
		   (1) it blocks while the worker is mid-regeneration, and the
		       worker ends its pass with a bare lock.clear() - no notify -
		       so wait(true) was never woken at all;
		   (2) lost wakeup: if the worker happened to wake from wait(false)
		       in the window after test_and_set but before shouldExit was
		       set, it read shouldExit == false, ran one more full pass,
		       cleared the flag and went back into wait(false) - with the
		       exit request published behind it and never re-read.
		   Setting the flag true here releases a worker sitting in
		   wait(false); the shouldExit re-check after clear in setupFunc
		   catches the worker that was mid-pass. Either way join() returns. */
		shouldExit.store(true, std::memory_order_release);
		lock.test_and_set(std::memory_order_release);
		lock.notify_all();
		if (_setupThread.joinable())_setupThread.join();
	};

	typedef std::complex<MYFLOAT> dcomp;

	void compute(MYFLOAT* in, int32_t s) override;

private:
	void setupFunc();
	/* seconds between self-triggered regenerations for the evolving
	   algorithms; 0 = parked. Log map so the low end is slow. */
	static MYFLOAT evoSeconds(MYFLOAT v) {
		return v <= FL(0.0) ? FL(0.0) : FL(20.0) * pow(FL(0.0025), v);
	}
	int32_t N, ND2, NYQ, M;
	MYFLOAT delold{}, boundaold{}, boundbold{}, modeold{}, sigold{};
	std::atomic<MYFLOAT>* del, * bounda, * boundb, * mode;
	std::atomic<MYFLOAT>* morphrate, * morphfront, * morphrev;
	std::atomic<MYFLOAT>* shape, * randevo, * gaussevo, * fb, * damp, * steps,
		* ripples, * ripplph, * root, * driftamt, * driftrate, * barbn,
		* barbspeed, * tidespeed, * adepth, * arate, * ainv, * decay, * damphf,
		* verbrt60, * verbevo, * verbdamp, * verbsize, * verbxfeed;
	/* LONGVERB loop state - audio thread only. An 8-line FDN in FRONT of the
	   convolver: u = FDN(in), wet = conv(u).

	   The kernel is OUTSIDE the loop. That is the whole point: a convolution
	   with no feedback around it has no poles, so the kernel's magnitude
	   ripple is colour and can never become a spread of decay rates, and the
	   tail length stops being tied to the kernel length. Inside the loop
	   there are only pure delays and Schroeder allpasses - the two elements
	   with exactly flat |H| at every frequency - so every mode decays at
	   exactly the rate the line gains say, and RT60 is honest at any DELAY.
	   (What this replaces: an input-side comb of one IR length with the RT60
	   baked into the IR envelope. The tail then repeated at the kernel period
	   and read as a delay, and RT60 was only honest while DELAY >= RT60/10.)

	   Line lengths are read FRACTIONALLY at a rate-limited distance, so SIZE
	   glides (a slight tape-like bend) instead of jumping the read pointer
	   into buffer content that no longer lines up. Gains, damping and the
	   mixing angle are recomputed per BLOCK from the params, so RT60/DAMP/
	   SIZE/XFEED are all instant and never regenerate the kernel - only
	   DELAY and EVOLVE do. Allpass lengths are fixed at the SMALLEST room so
	   they stay a minor part of the round trip and their buffers never
	   resize under the audio thread. */
	static constexpr int32_t NFDN = 8;
	static constexpr int32_t NFDNAP = 2;
	static constexpr MYFLOAT FDNMINMS = 20.0, FDNMAXMS = 80.0;
	/* where the per-line damping hits its target gain. The three DAMP tuning
	   levers are this, and the 1.0/3.0 absorb/tilt slopes in compute; at
	   4 kHz and 3.0 a full-DAMP 4 s room measures 1.96 s at 80-250 Hz and
	   0.58 s at 2.5-8 kHz (lo/hi 3.4), which is where the log-tilt DAMP of
	   the standalone version was ear-approved. */
	static constexpr MYFLOAT FDNDAMPREF = 4000.0;
	/* mutually incommensurate line ratios - no two lines share a period, so
	   the modes never stack into a grid */
	static constexpr MYFLOAT FDNRATIO[NFDN] = { 1.0, 1.13, 1.29, 1.42,
		1.61, 1.77, 1.94, 2.13 };
	std::vector<MYFLOAT> _fdnDl[NFDN];
	std::vector<MYFLOAT> _fdnAp[NFDN][NFDNAP];
	MYFLOAT _fdnTgt[NFDN]{};       /* settled length: prime, and an INTEGER */
	MYFLOAT _fdnSizeOld{ -1.0 };
	MYFLOAT _fdnLen[NFDN]{};       /* current fractional read distance */
	int32_t _fdnW[NFDN]{};
	int32_t _fdnApLen[NFDN][NFDNAP]{}, _fdnApPos[NFDN][NFDNAP]{};
	MYFLOAT _fdnTap[NFDN]{}, _fdnLp[NFDN]{};
	/* per-channel/track geometry offset: two mono instances would otherwise
	   be the identical room and the tail would collapse to the middle */
	MYFLOAT _fdnDetune{ 1.0 };
	bool _loopOn{};
	int64_t _evoCnt{};             /* samples since the last self-regeneration */
	/* evolution state - setup thread only */
	std::vector<MYFLOAT> _driftLaw;
	bool _driftInit{};
	/* LONGVERB kernel phase memory - setup thread only. EVOLVE STEPS these
	   by a bounded random walk instead of re-rolling them: the onset
	   envelope concentrates most of the kernel energy in its head, so two
	   INDEPENDENT draws differ audibly there and every completed morph
	   sweep landed as a lurch (period = one sweep, about DELAY at default
	   rates - the reported discontinuity). Consecutive walked draws are
	   ~96% correlated, so the front crossfades between nearly identical
	   kernels and evolution is a drift, not a step. No stability concern:
	   the kernel is OUTSIDE the loop, walks have no poles to move. On a
	   DELAY change the phases are RESAMPLED onto the new bin grid - group
	   delay in samples is invariant under that mapping (the frequency axis
	   does not move), so the taps stay aligned and DELAY moves crossfade
	   instead of jumping. */
	std::vector<MYFLOAT> _verbPh;
	int32_t _verbPhN{};
	/* PUREVERB - wet feedback straight through the pole-bounded kernel,
	   nothing else in the loop (v = in + g*hp(w_prev), w = conv(v)). The
	   generator persists across builds (its taus are the evolution state).
	   RT60 is HARDWIRED to PUREVERBRTX * DELAY: the kernel-in-the-loop
	   topology is only honest while RT60/DELAY stays under ~8-10 (erosion
	   spreads the decay rates as RT60/T beyond that - measured T30/knob
	   0.80 at ratio 7.8, 0.32 at 500), so the ceiling IS the setting and
	   DELAY doubles as the tail-length control. EVOLVE is hardwired
	   continuous (one Metropolis step per completed sweep) and DAMP to the
	   family default; BOUND band-limits the kernel, so only the band
	   reverberates. The alg has no knobs of its own. */
	static constexpr MYFLOAT PUREVERBRTX = 8.0;
	static constexpr MYFLOAT PUREVERBDAMP = 0.2;
	/* early-reflections tap: how much of the diffuser-chain output is
	   mixed STRAIGHT into the wet, around the main convolver. The loop
	   kernel's density has time constant T/4 (the anti-ripple law), so
	   its onset slows with DELAY - 18.5% of energy in the first 50 ms at
	   1 s but 2.0% at 10 s, the same class as the rectangle bug - and
	   nothing routed THROUGH it can fix that. The tap supplies the early
	   energy instead; level = PUREVERBER*sqrt(1 - 1s/DELAY) is calibrated
	   so total 50 ms energy lands near the DELAY-1s reference at every
	   setting (derived, then checked: wanted 0.47/0.60/0.64 at 2/5/10 s,
	   map gives 0.49/0.63/0.66). Zero at DELAY <= 1 s, where the kernel
	   is snappy by itself. NEVER feeds back - the loop taps the main
	   convolver's output only. */
	static constexpr MYFLOAT PUREVERBER = 0.7;
	PureKernelGen _pkGen;
	/* loop gain paired with the CURRENT kernel (setup -> audio) */
	std::atomic<MYFLOAT> _loopg{};
	/* true once the loaded kernel is a pole-bounded PUREVERB draw - the
	   loop must NOT close around a leftover kernel from another algorithm
	   (those are unit-ENERGY colour draws with +20 dB spectral ripple:
	   at g ~ 0.94 that is an over-unity loop). Set by the setup thread,
	   cleared by any non-PUREVERB build. */
	std::atomic<bool> _pureKernelIn{};
	/* audio thread: feedback state + the engage latch (feedback stays off
	   until the pure kernel has fully swept in ONCE; later sweeps are
	   hybrid-safe walked draws and stay closed) */
	MYFLOAT _pvFbPrev{}, _pvHpLp{};
	bool _pureOn{}, _pvFbOn{};
	/* PUREVERB input diffuser chain - the long-DELAY transient fix. A
	   transient through a long loop kernel comes back as slow repeating
	   pulses: generation k of the recirculation is the kernel convolved
	   with itself k times, the generation envelopes hump ~T/4 apart, and
	   at long DELAY the ear separates them (at short DELAY it fuses
	   them). The early generations dominate (g^k), and generation 1 is
	   excited by the RAW hit - so the fix is to pre-smear the INPUT:
	   four unit-energy flat-magnitude noise stages of length n/256,
	   n/64, n/16, n/4 in series, each stage's decay tuned to the NEXT
	   stage's length (60 dB over L_next = ~15 dB across its own support,
	   the same decay-to-length ratio as the main draw). Envelopes
	   convolve, so a hit reaches the main kernel already ~T/4 long and
	   the generation humps are gone. The stage OUTPUTS ARE TAPPED AND
	   SUMMED (1/sqrt(4)): one arrival per smear length, so the wet keeps
	   a near-sharp attack (first tap, -12 dB) that thickens tap by tap
	   into the full wash - serial-only lost the attack completely.
	   INPUT SIDE ONLY, outside the loop:
	   the recirculation is already wash and does not re-enter the chain,
	   so the pole condition and RT60 are untouched. Rebuilt only when
	   DELAY changes - EVOLVE must not re-roll a static colour stage (the
	   lurch lesson). _pvDrain zero-ticks the chain after the alg is
	   switched away so a re-engage does not replay stale history. */
	static constexpr int32_t PVNPRE = 4;
	std::unique_ptr<ConvolverNonUniform> _pvPre[PVNPRE];
	std::atomic<int32_t> _pvPreN{ -1 }; /* main n the chain was built for */
	/* chain draws are BAND-LIMITED like the main kernel: the ER tap goes
	   around the main convolver, so a full-band chain would leak
	   out-of-band diffusion into the wet and break BOUND. Setup thread. */
	MYFLOAT _pvPreA{ FL(-1.0) }, _pvPreB{ FL(-1.0) };
	std::atomic<bool> _pvPreReady{};
	int64_t _pvDrain{};              /* audio thread: flush ticks left */
	MYFLOAT _barberPhase{}, _tidePos{};
	/* ADAPTIVE: the last ADAPTN input samples. Written by the audio thread,
	   read without synchronisation by the setup thread - it only feeds an
	   envelope estimate, a torn read is inaudible. */
	static constexpr int32_t ADAPTN = 4096;
	std::vector<MYFLOAT> _adaptRing;
	int32_t _adaptW{};
	std::unique_ptr<FFT> _adaptFft;
	tsl::AlignedVector<MYFLOAT> _buf;
	std::unique_ptr<FFT> fft;
	std::unique_ptr<ConvolverNonUniform> convolverNonUniform;
	Follower* fol{};
	MYFLOAT* fir;
	std::atomic<bool> shouldExit{ false };
	std::atomic_flag lock{};
	std::atomic<bool> thread{};
	std::thread _setupThread;
};


#endif // GRAINSTORM_CONVOLVER_H
