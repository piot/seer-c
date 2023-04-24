/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef SEER_H
#define SEER_H

#include <seer/predicted_steps.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <transmute/transmute.h>

struct ImprintAllocator;

typedef struct Seer {
    TransmuteVm transmuteVm;
    size_t maxPlayerCount;
    size_t maxTicksPerRead;
    uint8_t* readTempBuffer;
    size_t readTempBufferSize;
    SeerPredictedSteps predictedSteps;
    TransmuteInput cachedTransmuteInput;
    size_t maxPredictionTicksFromAuthoritative;
    StepId stepId;
    Clog log;
} Seer;

void seerInit(Seer* self, TransmuteVm transmuteVm, struct ImprintAllocator* allocator, size_t maxInputOctetSize,
              size_t maxPlayers, Clog log);
void seerDestroy(Seer* self);
int seerUpdate(Seer* self);
void seerSetState(Seer* self, TransmuteState state, StepId stepId);
TransmuteState seerGetState(const Seer* self, StepId* outStepId);

#endif
