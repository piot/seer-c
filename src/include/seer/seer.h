/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef SEER_H
#define SEER_H

#include <nimble-steps/steps.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <transmute/transmute.h>

struct ImprintAllocator;

typedef struct Seer {
    TransmuteVm transmuteVm;
    size_t maxPlayerCount;
    uint8_t* readTempBuffer;
    size_t readTempBufferSize;
    NbsSteps predictedSteps;
    TransmuteInput cachedTransmuteInput;
    size_t maxPredictionTicksFromAuthoritative;
    StepId stepId;
    StepId maxPredictionTickId;
    Clog log;
} Seer;

typedef struct SeerSetup {
    struct ImprintAllocator* allocator;
    size_t maxStepOctetSizeForSingleParticipant;
    size_t maxPlayers;
    size_t maxTicksFromAuthoritative;
    Clog log;
} SeerSetup;

void seerInit(Seer* self, TransmuteVm transmuteVm, SeerSetup setup, TransmuteState state, StepId stepId);
void seerDestroy(Seer* self);
int seerUpdate(Seer* self);
void seerSetState(Seer* self, TransmuteState state, StepId stepId);
TransmuteState seerGetState(const Seer* self, StepId* outStepId);
int seerAddPredictedStep(Seer* self, const TransmuteInput* input, StepId tickId);
int seerAddPredictedStepRaw(Seer* self, const uint8_t* combinedStep, size_t octetCount, StepId tickId);

#endif
