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

typedef void (*SeerPredictionCopyFromAuthoritativeFn)(void* self, StepId tickId);
typedef void (*SeerPredictionTickFn)(void* self, const TransmuteInput* input, StepId tickId);
typedef void (*SeerPredictionPostPredictionTicksFn)(void* self);

typedef struct SeerCallbackObjectVtbl {
    SeerPredictionCopyFromAuthoritativeFn copyFromAuthoritativeFn;
    SeerPredictionTickFn predictionTickFn;
    SeerPredictionPostPredictionTicksFn postPredictionTicksFn;
} SeerCallbackObjectVtbl;

typedef struct SeerCallbackObject {
    SeerCallbackObjectVtbl* vtbl;
    void* self;
} SeerCallbackObject;

typedef struct Seer {
    SeerCallbackObject callbackObject;
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

void seerInit(Seer* self, SeerCallbackObject callbackObject, SeerSetup setup, StepId stepId);
void seerDestroy(Seer* self);
int seerUpdate(Seer* self);
void seerAuthoritativeGotNewState(Seer* self, StepId stepId);
bool seerShouldAddPredictedStepThisTick(const Seer* self);
int seerAddPredictedStep(Seer* self, const TransmuteInput* input, StepId tickId);
int seerAddPredictedStepRaw(Seer* self, const uint8_t* combinedStep, size_t octetCount, StepId tickId);

#endif
