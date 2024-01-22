/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-steps-serialize/out_serialize.h"
#include <imprint/allocator.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <seer/seer.h>

void seerInit(Seer* self, const SeerCallbackObject* callbackObject, SeerSetup setup, StepId stepId)
{
    self->callbackObject = callbackObject;
    self->maxPlayerCount = setup.maxPlayers;
    self->cachedTransmuteInput.participantInputs = IMPRINT_ALLOC_TYPE_COUNT(setup.allocator, TransmuteParticipantInput,
                                                                            setup.maxPlayers);
    self->cachedTransmuteInput.participantCount = 0;
    self->readTempBufferSize = 512;
    self->readTempBuffer = IMPRINT_ALLOC_TYPE_COUNT(setup.allocator, uint8_t, self->readTempBufferSize);
    self->maxPredictionTicksFromAuthoritative = setup.maxTicksFromAuthoritative;
    nbsStepsInit(&self->predictedSteps, setup.allocator, setup.maxStepOctetSizeForSingleParticipant * setup.maxPlayers,
                 setup.log);
    nbsStepsReInit(&self->predictedSteps, stepId);
    self->stepId = stepId;
    self->maxPredictionTickId = (StepId) (self->stepId + self->maxPredictionTicksFromAuthoritative);
    self->log = setup.log;

    self->callbackObject->vtbl->copyFromAuthoritativeFn(self->callbackObject->self, stepId);
}

void seerDestroy(Seer* self)
{
    (void) self;
}

void seerAuthoritativeGotNewState(Seer* self, StepId stepId)
{
    // Check that the stepId is greater than that has been set previously
    // Check if we have steps for this step in the buffer
    // Discard older steps

    int discardedStepCount = nbsStepsDiscardUpTo(&self->predictedSteps, stepId);
#if defined CLOG_LOG_ENABLED
    CLOG_C_VERBOSE(&self->log, "at stepId: %08X discarded %d steps, predicted count is now: %zu", stepId,
                   discardedStepCount, self->predictedSteps.stepsCount)
#else
    (void) discardedStepCount;
#endif
    self->stepId = stepId;
    self->maxPredictionTickId = (StepId) (self->stepId + self->maxPredictionTicksFromAuthoritative);

#if defined CLOG_LOG_ENABLED
    CLOG_C_VERBOSE(&self->log, "callback: copyFromAuthoritativeFn")
#endif
    self->callbackObject->vtbl->copyFromAuthoritativeFn(self->callbackObject->self, stepId);
}

int seerUpdate(Seer* self)
{
    // We don't want to predict too far in the future, for several reasons
    // The predictions have the risk of being so far from the actual truth, so the reconciliation with the authoritative
    // state will look jarring and/or erroneous.

    while (true) {
        if (self->stepId >= self->maxPredictionTickId) {
            // CLOG_C_INFO(&self->log,
            //           "we can not predict further from the last authoritative state. The prediction will be too "
            //         "costly to simulate or uncertainty will be too high"
            //       "max: %04X actual: %04X maxDeltaTicks: %zu",
            //     self->maxPredictionTickId, self->stepId, self->maxPredictionTicksFromAuthoritative)

            self->callbackObject->vtbl->postPredictionTicksFn(self->callbackObject->self);
            return 1;
        }

        int infoIndex = nbsStepsGetIndexForStep(&self->predictedSteps, self->stepId);
        if (infoIndex < 0) {
            CLOG_C_VERBOSE(&self->log, "stop predicting, since we don't have a predicted input for step %04X", self->stepId)
            self->callbackObject->vtbl->postPredictionTicksFn(self->callbackObject->self);
            return 0;
        }

        int payloadOctetCount = nbsStepsReadAtIndex(&self->predictedSteps, infoIndex, self->readTempBuffer,
                                                    self->readTempBufferSize);
        if (payloadOctetCount <= 0) {
            CLOG_C_SOFT_ERROR(&self->log, "can not read index")
            return payloadOctetCount;
        }

        NimbleStepsOutSerializeLocalParticipants participants;

        nbsStepsInSerializeStepsForParticipantsFromOctets(&participants, self->readTempBuffer,
                                                          (size_t) payloadOctetCount);
        CLOG_C_VERBOSE(&self->log, "read predicted step %08X octetCount: %d", self->stepId, payloadOctetCount)
        for (size_t i = 0; i < participants.participantCount; ++i) {
            CLOG_EXECUTE(NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];)
            CLOG_C_VERBOSE(&self->log, " participant %d octetCount: %zu", participant->participantId,
                           participant->payloadCount)
        }

        if (participants.participantCount > self->maxPlayerCount) {
            CLOG_C_SOFT_ERROR(&self->log, "Too many participants %zu", participants.participantCount)
            return -99;
        }
        self->cachedTransmuteInput.participantCount = participants.participantCount;
        for (size_t i = 0U; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            self->cachedTransmuteInput.participantInputs[i].participantId = participant->participantId;
            self->cachedTransmuteInput.participantInputs[i].input = participant->payload;
            self->cachedTransmuteInput.participantInputs[i].octetSize = participant->payloadCount;
            self->cachedTransmuteInput.participantInputs[i].inputType = TransmuteParticipantInputTypeNormal;
        }
        // CLOG_C_INFO(&self->log, "seer tick! %08X", self->stepId)

        self->callbackObject->vtbl->predictionTickFn(self->callbackObject->self, &self->cachedTransmuteInput);
        self->stepId++;
    }
}

bool seerShouldAddPredictedStepThisTick(const Seer* self)
{
    return (self->predictedSteps.stepsCount + 2 < self->maxPredictionTicksFromAuthoritative) &&
           (self->stepId < self->maxPredictionTickId);
}

static NimbleSerializeParticipantConnectState toConnectState(TransmuteParticipantInputType inputType)
{
    switch (inputType) {
        case TransmuteParticipantInputTypeNormal:
            return NimbleSerializeParticipantConnectStateNormal;
        case TransmuteParticipantInputTypeNoInputInTime:
            return NimbleSerializeParticipantConnectStateStepNotProvidedInTime;
        case TransmuteParticipantInputTypeWaitingForReconnect:
            return NimbleSerializeParticipantConnectStateStepWaitingForReconnect;
    }
}

int seerAddPredictedStep(Seer* self, const TransmuteInput* input, StepId tickId)
{
    NimbleStepsOutSerializeLocalParticipants data;

    for (size_t i = 0; i < input->participantCount; ++i) {
        if (input->participantInputs[i].input == 0 || input->participantInputs[i].octetSize == 0) {
            CLOG_C_ERROR(&self->log, "illegal payload for predicted input")
            // return -2;
        }
        data.participants[i].participantId = input->participantInputs[i].participantId;
        data.participants[i].payload = input->participantInputs[i].input;
        data.participants[i].payloadCount = input->participantInputs[i].octetSize;
        data.participants[i].connectState = toConnectState(input->participantInputs[i].inputType);
    }

    data.participantCount = input->participantCount;

    ssize_t octetCount = nbsStepsOutSerializeStep(&data, self->readTempBuffer, self->readTempBufferSize);
    if (octetCount < 0) {
        CLOG_ERROR("seerAddPredictedSteps: could not serialize")
        // return octetCount;
    }

    return seerAddPredictedStepRaw(self, self->readTempBuffer, (size_t) octetCount, tickId);
}

int seerAddPredictedStepRaw(Seer* self, const uint8_t* combinedBuffer, size_t octetCount, StepId tickId)
{
    return nbsStepsWrite(&self->predictedSteps, tickId, combinedBuffer, octetCount);
}
