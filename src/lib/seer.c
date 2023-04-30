/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-steps-serialize/out_serialize.h"
#include <imprint/allocator.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <seer/seer.h>

void seerInit(Seer* self, TransmuteVm transmuteVm, SeerSetup setup, TransmuteState state, StepId stepId)
{
    self->transmuteVm = transmuteVm;
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
    transmuteVmSetState(&self->transmuteVm, &state);
    self->stepId = stepId;

    self->log = setup.log;
}

void seerDestroy(Seer* self)
{
    self->transmuteVm.vmPointer = 0;
}

void seerSetState(Seer* self, TransmuteState state, StepId stepId)
{
    // Check that the stepId is greater than that has been set previously
    // Check if we have steps for this step in the buffer
    // Discard older steps
    transmuteVmSetState(&self->transmuteVm, &state);
    nbsStepsDiscardUpTo(&self->predictedSteps, stepId);
    self->stepId = stepId;
    self->maxPredictionTickId = self->stepId + self->maxPredictionTicksFromAuthoritative;
}

int seerUpdate(Seer* self)
{
    // We don't want to predict too far in the future, for several reasons
    // The predictions have the risk of being so far from the actual truth, so the reconciliation with the authoritative
    // state will look jarring and/or erroneous.

    while (1) {
        if (self->stepId >= self->maxPredictionTickId) {
            CLOG_C_NOTICE(&self->log,
                          "we have predicted enough from the last authoritative state"
                          "max: %04X actual: %04X",
                          self->maxPredictionTickId, self->stepId)
            return 1;
        }

        int infoIndex = nbsStepsGetIndexForStep(&self->predictedSteps, self->stepId);
        if (infoIndex < 0) {
            CLOG_C_VERBOSE(&self->log, "we don't have a predicted input for step %04X", self->stepId)
            return 0;
        }

        int payloadOctetCount = nbsStepsReadAtIndex(&self->predictedSteps, infoIndex, self->readTempBuffer,
                                                    self->readTempBufferSize);
        if (payloadOctetCount <= 0) {
            return payloadOctetCount;
        }

        struct NimbleStepsOutSerializeLocalParticipants participants;

        nbsStepsInSerializeAuthoritativeStepHelper(&participants, self->readTempBuffer, payloadOctetCount);
        CLOG_C_VERBOSE(&self->log, "read predicted step %016X octetCount: %d", self->stepId, payloadOctetCount);
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            CLOG_C_VERBOSE(&self->log, " participant %d octetCount: %zu", participant->participantIndex, participant->payloadCount);
        }

        if (participants.participantCount > self->maxPlayerCount) {
            CLOG_C_SOFT_ERROR(&self->log, "Too many participants %zu", participants.participantCount);
            return -99;
        }
        self->cachedTransmuteInput.participantCount = participants.participantCount;
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            self->cachedTransmuteInput.participantInputs[i].input = participant->payload;
            self->cachedTransmuteInput.participantInputs[i].octetSize = participant->payloadCount;
        }

        transmuteVmTick(&self->transmuteVm, &self->cachedTransmuteInput);
        self->stepId++;
    }

    return 0;
}

TransmuteState seerGetState(const Seer* self, StepId* outStepId)
{
    *outStepId = self->stepId;
    return transmuteVmGetState(&self->transmuteVm);
}

int seerAddPredictedStep(Seer* self, const TransmuteInput* input, StepId tickId)
{
    NimbleStepsOutSerializeLocalParticipants data;

    for (size_t i = 0; i < input->participantCount; ++i) {
        data.participants[i].participantIndex = i;
        data.participants[i].payload = input->participantInputs[i].input;
        data.participants[i].payloadCount = input->participantInputs[i].octetSize;
    }

    data.participantCount = input->participantCount;

    int octetCount = nbsStepsOutSerializeStep(&data, self->readTempBuffer, self->readTempBufferSize);
    if (octetCount < 0) {
        CLOG_ERROR("seerAddPredictedSteps: could not serialize")
        return octetCount;
    }

    return seerAddPredictedStepRaw(self, self->readTempBuffer, octetCount, tickId);
}

int seerAddPredictedStepRaw(Seer* self, const uint8_t* combinedBuffer, size_t octetCount, StepId tickId)
{
    return nbsStepsWrite(&self->predictedSteps, tickId, combinedBuffer, octetCount);
}
