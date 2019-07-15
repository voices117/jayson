#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdlib.h>
#include "stream.h"


/** Value returned by the FSM indicating no transition matched an input. */
#define FSM_ERROR_NO_MATCH -5

/** Value returned by the FSM if a transition callback returned \c false. */
#define FSM_ERROR_TRANSITION -4

/** Value returned by the FSM if the input stream returned error. */
#define FSM_ERROR_STREAM -3

/** Value returned by the FSM indicating no transition matched an input. */
#define FSM_ERROR_STATE -1

/** Value that should be used for the initial FSM state. */
#define FSM_INITIAL_STATE 0

/** Value that should be used for the final FSM state. This value shall always
 *  be greater than \c FSM_INITIAL_STATE */
#define FSM_END_STATE 1

/** Special transition value used to indicate that the transition is always
 *  taken no matter the input value. */
#define ANY NULL


/** State ID type. */
typedef int state_id_t;

/** Callback executed when a transition is taken. */
typedef bool ( *transition_action_cb_t )( void *ctx, char c );

/** Callback executed when a transition to the end of file state is taken. */
typedef bool ( *transition_eof_action_cb_t )( void *ctx );

/** Defines a transition to a new state. */
typedef struct {
    /** Array of values that trigger the transition. */
    const char *values;
    /** Number of values in \c values. */
    size_t values_len;
    /** Next state used if the transition is taken. */
    state_id_t next_state;
    /** Action executed if the transition is taken (or \c NULL). */
    transition_action_cb_t action;
} transition_t;

/** Transition triggered by an end of file. */
typedef struct {
    /** Action executed if the transition is taken (or \c NULL). */
    state_id_t next_state;
    /** Action executed if the transition is taken (or \c NULL). */
    transition_eof_action_cb_t action;
} transition_eof_t;

/** FSM state. */
typedef struct {
    /** Array of transitions that compose the state. */
    transition_t *transitions;
    /** Number of transitions in \c transitions. */
    size_t num_transitions;
    /** Transition triggered if the end of file is reached. */
    transition_eof_t transition_eof;
} state_t;


state_id_t fsm_step( const state_t *state, uint8_t c, state_id_t current, void *ctx );
state_id_t fsm_run( const state_t *states, stream_t *stream, void *ctx );


#endif
