#pragma once
#include "button.h"

	namespace tsl::graphics {
		class SpaceWaveform : public HorizontalLayout {

		public:
			SpaceWaveform(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
					};



		class SpaceGranulation : public HorizontalLayout {
		public:
			SpaceGranulation(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};
			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};

		class SpaceGrainGen : public HorizontalLayout {

		public:
			SpaceGrainGen(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};


		class SpacePv : public HorizontalLayout {

		public:
			SpacePv(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};

		class SpaceCross : public HorizontalLayout {

		public:
			SpaceCross(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};

		class SpaceGrainEnv : public HorizontalLayout {

		public:
			SpaceGrainEnv(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};

		class SpaceLFO : public HorizontalLayout {

		public:
			SpaceLFO(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};
		class SpaceFX : public HorizontalLayout {

		public:
			SpaceFX(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};
		class SpaceFOL : public HorizontalLayout {

		public:
			SpaceFOL(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};
		class SpaceStereo : public HorizontalLayout {

		public:
			SpaceStereo(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};

		/* Second SPECTRAL DELAY (GENCREVERB) space: the algorithm selector
		   plus the active algorithm's sub-view, picked through the
		   specdel_subspaces map (RAMP UP/DOWN share one view). */
		class SpaceSpecDelAlg : public HorizontalLayout {

		public:
			SpaceSpecDelAlg(tsl::AppState* _appState, float a, int b, int align, Layout* parent) : HorizontalLayout{ _appState, a, b, align, parent, true } {};

			void addRecursiveCB() override;
			void addRecursiveDraw()override;
		};
	}

