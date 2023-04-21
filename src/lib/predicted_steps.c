/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <seer/predicted_steps.h>
#include <transmute/transmute.h>

void seerPredictedStepsInit(SeerPredictedSteps* self, StepId stepId)
{
    self->count = 0;
    self->tailIndex = 0;
    self->headIndex = 0;
    self->expectedWriteStepId = stepId;
    self->expectedReadStepId = stepId;
    CLOG_VERBOSE("predicted steps set to %08X", stepId);

    for (size_t i = 0; i < SEER_PREDICTED_STEPS_WINDOW_SIZE; ++i) {
        self->steps[i].stepId = NIMBLE_STEP_MAX;
        self->steps[i].serializedInputOctetCount = 0;
    }
}

int seerPredictedStepsHead(const SeerPredictedSteps* self, const TransmuteInput* outPointer, StepId* stepId)
{
    if (self->count == 0) {
        //*outPointer = 0;
        *stepId = NIMBLE_STEP_MAX;
        return -1;
    }

    int previousHeadIndex = tc_modulo((self->headIndex - 1), SEER_PREDICTED_STEPS_WINDOW_SIZE);
    const SeerPredictedStep* entry = &self->steps[previousHeadIndex];

    // outPointer-> =entry->serializedInputListItems;
    *stepId = entry->stepId;

    return (int) entry->serializedInputOctetCount;
}

bool seerPredictedStepsLastStepId(const SeerPredictedSteps* self, StepId* stepId)
{
    if (self->count == 0) {
        *stepId = NIMBLE_STEP_MAX;
        return false;
    }

    *stepId = self->expectedWriteStepId - 1;
    return true;
}

int seerPredictedStepsAdd(SeerPredictedSteps* self, const TransmuteInput* input, MonotonicTimeMs now)
{
    StepId stepId = self->expectedWriteStepId;
    CLOG_INFO("predicted add %08X", stepId);
    if (self->count >= SEER_PREDICTED_STEPS_WINDOW_SIZE) {
        CLOG_SOFT_ERROR("predicted steps: buffer is full")
        return -4;
    }

    /*
    if (stepId != self->expectedWriteStepId) {
        CLOG_SOFT_ERROR("predicted steps: I needed %08X but you provided %08X", self->expectedWriteStepId, stepId)
        return -6;
    }
 */
    // CLOG_VERBOSE("predicted steps: added %08X", stepId);

    SeerPredictedStep* entry = &self->steps[self->headIndex];
    entry->stepId = stepId;
    if (entry->serializedInputOctetCount != 0) {
        CLOG_ERROR("logical error. entry is not empty")
    }

#if 0
#define TEMP_DEBUG_COUNT (512)
   static char debugChar[TEMP_DEBUG_COUNT];
   CLOG_INFO("store predicted input %04X %s", stepId,
             swampDumpToAsciiString(entry->serializedInputListItems, (const SwtiType*) inputValueListType, 0,
                                    debugChar, TEMP_DEBUG_COUNT));
#endif

    size_t octetCount = 0; // TODO:

    CLOG_VERBOSE("store %p  %zu octets of predicted list items for step %04X", (void*) entry->serializedInputListItems,
                 octetCount, stepId);
    entry->serializedInputOctetCount = octetCount;
    entry->optionalTimestamp = now;

    self->headIndex++;
    self->headIndex %= SEER_PREDICTED_STEPS_WINDOW_SIZE;
    self->count++;

    CLOG_WARN("stored augur predicted steps. count:%zd", self->count)
    self->expectedWriteStepId++;

    return 0;
}

bool seerPredictedStepsHasStepId(const SeerPredictedSteps* self, StepId stepId)
{
    return stepId >= self->expectedReadStepId && stepId < self->expectedWriteStepId;
}

int seerPredictedStepsGetIndex(const SeerPredictedSteps* self, StepId stepId)
{
    if (self->count == 0) {
        CLOG_SOFT_ERROR("predicted steps: can not get any index for you. buffer is empty");
        return -4;
    }

    // CLOG_VERBOSE("predicted steps: get index for %08X", stepId);

    int backwardDelta = (self->expectedWriteStepId - 1) - stepId;
    if (backwardDelta < 0) {
        CLOG_SOFT_ERROR("predicted steps: this was in the future. Step %08X was requested, but waiting for step %08X",
                        stepId, self->expectedWriteStepId);
        return -5;
    }
    if ((size_t) backwardDelta >= self->count) {
        CLOG_SOFT_ERROR("predicted steps: this was too long in the past. it wanted to look back %d but I only have %zu "
                        "in store. Read %08X, Write %08X, requested %08X",
                        backwardDelta, self->count, self->expectedReadStepId, self->expectedWriteStepId, stepId);
        return -6;
    }

    int foundIndex = tc_modulo((self->headIndex - 1) - backwardDelta, SEER_PREDICTED_STEPS_WINDOW_SIZE);

#if 1
    const SeerPredictedStep* entry = &self->steps[foundIndex];
    if (entry->stepId != stepId) {
        CLOG_SOFT_ERROR("something went wrong here")
    }
#endif

    return foundIndex;
}

int seerPredictedStepsDiscardUpTo(SeerPredictedSteps* self, StepId stepIdToSave)
{
    CLOG_WARN("discard Upto %08X before count:%zd", stepIdToSave, self->count)
    if (self->count == 0) {
        // CLOG_VERBOSE("predicted steps: discard up to was ignored, buffer is empty");
        return 0;
    }

    if (stepIdToSave < self->expectedReadStepId) {
        CLOG_VERBOSE("predicted steps: discard up to was ignored, old stepId");
        return 0;
    }

    if (stepIdToSave >= self->expectedWriteStepId) {
        CLOG_VERBOSE("predicted steps: discard up is was way in the future, clearing whole buffer");
        for (size_t i = 0; i < SEER_PREDICTED_STEPS_WINDOW_SIZE; ++i) {
            self->steps[i].stepId = NIMBLE_STEP_MAX;
            self->steps[i].serializedInputOctetCount = 0;
        }
        self->count = 0;
        self->expectedReadStepId = self->expectedWriteStepId;
        self->tailIndex = self->headIndex;
        return 0;
    }

    int discardCount = stepIdToSave - self->expectedReadStepId;
    int tailIndex = self->tailIndex;
    for (size_t i = 0; i < (size_t) discardCount; ++i) {
        SeerPredictedStep* entry = &self->steps[tailIndex];
        // CLOG_VERBOSE("predicted steps: discarding %08X (discarding up to %08X)", entry->stepId, stepIdToSave);
        tailIndex = (tailIndex + 1) % SEER_PREDICTED_STEPS_WINDOW_SIZE;
        entry->serializedInputOctetCount = 0;
        entry->stepId = NIMBLE_STEP_MAX;
    }

    self->tailIndex = tailIndex;
    self->expectedReadStepId = stepIdToSave;
    self->count -= discardCount;

    CLOG_WARN("discard Upto %08X after count:%zd removed:%d", stepIdToSave, self->count, discardCount)

    return 0;
}
