/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "utest.h"
#include <clog/clog.h>
#include <imprint/default_setup.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/out_serialize.h>
#include <nimble-steps/steps.h>
#include <seer/seer.h>

typedef struct AppSpecificState {
    int x;
    int time;
} AppSpecificState;

typedef struct AppSpecificParticipantInput {
    int horizontalAxis;
} AppSpecificParticipantInput;

typedef struct AppSpecificVm {
    AppSpecificState appSpecificState;
} AppSpecificVm;

void appSpecificTick(void* _self, const TransmuteInput* input)
{
    AppSpecificVm* self = (AppSpecificVm*) _self;

    if (input->participantCount > 0) {
        const AppSpecificParticipantInput* appSpecificInput = (AppSpecificParticipantInput*) input->participantInputs[0]
                                                                  .input;
        if (appSpecificInput->horizontalAxis > 0) {
            self->appSpecificState.x++;
            CLOG_DEBUG("app: tick with input %d, walking to the right", appSpecificInput->horizontalAxis)
        } else {
            CLOG_DEBUG("app: tick with input %d, not walking to the right", appSpecificInput->horizontalAxis)
        }
    } else {
        CLOG_DEBUG("app: tick with no input")
    }

    self->appSpecificState.time++;
}

TransmuteState appSpecificGetState(const void* _self)
{
    TransmuteState state;

    AppSpecificVm* self = (AppSpecificVm*) _self;

    state.octetSize = sizeof(AppSpecificState);
    state.state = (const void*) &self->appSpecificState;

    return state;
}

void appSpecificSetState(void* _self, const TransmuteState* state)
{
    AppSpecificVm* self = (AppSpecificVm*) _self;

    self->appSpecificState = *(AppSpecificState*) state->state;
}

int appSpecificStateToString(void* _self, const TransmuteState* state, char* target, size_t maxTargetOctetSize)
{
    (void) _self;

    const AppSpecificState* appState = (AppSpecificState*) state->state;
    return tc_snprintf(target, maxTargetOctetSize, "state: time: %d pos.x: %d", appState->time, appState->x);
}

int appSpecificInputToString(void* _self, const TransmuteParticipantInput* input, char* target,
                             size_t maxTargetOctetSize)
{
    (void) _self;
    const AppSpecificParticipantInput* participantInput = (AppSpecificParticipantInput*) input->input;
    return tc_snprintf(target, maxTargetOctetSize, "input: horizontalAxis: %d", participantInput->horizontalAxis);
}

UTEST(Assent, verify)
{
    ImprintDefaultSetup imprint;
    imprintDefaultSetupInit(&imprint, 16 * 1024 * 1024);

    Seer seer;

    AppSpecificVm appSpecificVm;

    TransmuteVm transmuteVm;
    TransmuteVmSetup setup;
    setup.tickFn = appSpecificTick;
    setup.tickDurationMs = 16;
    setup.setStateFn = appSpecificSetState;
    setup.getStateFn = appSpecificGetState;
    setup.inputToString = appSpecificInputToString;
    setup.stateToString = appSpecificStateToString;

    Clog subLog;

    subLog.config = &g_clog;
    subLog.constantPrefix = "AuthoritativeVm";

    transmuteVmInit(&transmuteVm, &appSpecificVm, setup, subLog);

    AppSpecificState initialAppState;
    initialAppState.time = 0;
    initialAppState.x = 0;

    TransmuteState initialTransmuteState;
    initialTransmuteState.state = &initialAppState;
    initialTransmuteState.octetSize = sizeof(initialAppState);

    StepId initialStepId = {101};

    Clog predictSubLog;
    predictSubLog.constantPrefix = "seer";
    predictSubLog.config = &g_clog;

    SeerSetup seerSetup;
    seerSetup.allocator = &imprint.slabAllocator.info.allocator;
    seerSetup.maxTicksFromAuthoritative = 10;
    seerSetup.maxPlayers = 16;
    seerSetup.maxStepOctetSizeForSingleParticipant = 12;
    seerSetup.log = predictSubLog;

    seerInit(&seer, transmuteVm, seerSetup, initialTransmuteState, initialStepId);

    NimbleStepsOutSerializeLocalParticipants data;
    AppSpecificParticipantInput gameInput;
    gameInput.horizontalAxis = 24;

    data.participants[0].participantIndex = 1;
    data.participants[0].payload = (const uint8_t*) &gameInput;
    data.participants[0].payloadCount = sizeof(gameInput);
    data.participantCount = 1;


    TransmuteInput transmuteInput;
    TransmuteParticipantInput participantInputs[1];
    participantInputs[0].input = &gameInput;
    participantInputs[0].octetSize = sizeof(gameInput);
    participantInputs[0].participantId = 1;

    transmuteInput.participantInputs = participantInputs;
    transmuteInput.participantCount = 1;

    seerAddPredictedStep(&seer, &transmuteInput, initialStepId);

    ASSERT_EQ(0, appSpecificVm.appSpecificState.x);
    ASSERT_EQ(0, appSpecificVm.appSpecificState.time);

    ASSERT_EQ(0, seerUpdate(&seer));

    StepId expectedStepId;
    TransmuteState currentState = seerGetState(&seer, &expectedStepId);
    const AppSpecificState* currentAppState = (const AppSpecificState*) currentState.state;

    ASSERT_EQ(initialStepId + 1, expectedStepId);
    ASSERT_EQ(1, currentAppState->x);
    ASSERT_EQ(1, currentAppState->time);
}
