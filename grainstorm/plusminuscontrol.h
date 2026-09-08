#pragma once
#ifndef PLUS_MINUS_CONTROL_H
#define PLUS_MINUS_CONTROL_H

#include <stdint.h>
#include <atomic>
#include <tools.h>
#include <PlusMinusControlBase.h>
#include <Input.h>

#define VALUEView 0
#define MINUSView 1
#define PLUSView 2
#define TITLEView 3

namespace tsl {
    namespace graphics {
        class PlusMinusControlQuant : public PlusMinusControl {
        public:
            PlusMinusControlQuant(tsl::AppState *appState, float sc, int32_t as, int al, int _id, const char *_title = "QUANT")
                    : PlusMinusControl(appState, sc, as, al, _id, _title) {
            };

            void render(void *ctx) override;

            void callback(const InputEvent &ev) override;
        };
    }
}
#endif