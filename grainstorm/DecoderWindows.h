#pragma once
//
// Created by pr on 15.11.17.
//
#ifndef GRAINSTORM_DECODER_H
#define GRAINSTORM_DECODER_H

#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>
#include <audio/Recording.h>

namespace tsl {
	struct AppState;
}

struct TRACK;

int32_t filebrowsercallback(tsl::AppState*,const std::string &_filename);
std::shared_ptr<tsl::Recording> dec(TRACK *track, std::string name, long gap);

#endif //GRAINSTORM_DECODER_H
