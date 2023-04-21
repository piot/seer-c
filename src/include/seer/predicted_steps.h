/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef SEER_PREDICTED_STEPS_H
#define SEER_PREDICTED_STEPS_H

#define SEER_PREDICTED_STEPS_WINDOW_SIZE (64)

#include <monotonic-time/monotonic_time.h>
#include <nimble-steps/steps.h>
#include <stdbool.h>

struct TransmuteInput;

#define SEER_PREDICTED_STEPS_MAX_SERIALIZED_OCTET_COUNT (128)

typedef struct SeerPredictedStep {
    StepId stepId;
    const uint8_t serializedInputListItems[SEER_PREDICTED_STEPS_MAX_SERIALIZED_OCTET_COUNT];
    size_t serializedInputOctetCount;
    MonotonicTimeMs optionalTimestamp;
} SeerPredictedStep;

typedef struct SeerPredictedSteps {
    SeerPredictedStep steps[SEER_PREDICTED_STEPS_WINDOW_SIZE];
    int tailIndex;
    int headIndex;
    size_t count;
    StepId expectedWriteStepId;
    StepId expectedReadStepId;
} SeerPredictedSteps;

void seerPredictedStepsInit(SeerPredictedSteps* self, StepId stepId);
int seerPredictedStepsAdd(SeerPredictedSteps* self, const struct TransmuteInput* inputValueList, MonotonicTimeMs now);
int seerPredictedStepsGetIndex(const SeerPredictedSteps* self, StepId stepId);
int seerPredictedStepsDiscardUpTo(SeerPredictedSteps* self, StepId stepIdToSave);
bool seerPredictedStepsHasStepId(const SeerPredictedSteps* self, StepId stepId);
int seerPredictedStepsHead(const SeerPredictedSteps* self, const struct TransmuteInput* outPointer, StepId* stepId);
bool seerPredictedStepsLastStepId(const SeerPredictedSteps* self, StepId* stepId);

#endif
