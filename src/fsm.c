#include <string.h>
#include "fsm.h"


state_id_t fsm_step( const state_t *state, uint8_t c, state_id_t current, void *ctx ) {
    /* finds a transition matching the current character */
    for( size_t i = 0; i< state->num_transitions; i++ ) {
        transition_t transition = state->transitions[i];
        if( transition.values == ANY || memchr( transition.values, c, transition.values_len ) != NULL ) {
            if( transition.action != NULL && !transition.action( ctx, c ) ) {
                return FSM_ERROR_TRANSITION;
            }
            return transition.next_state;
        }
    }

    /* no transition matched */
    return FSM_ERROR_NO_MATCH;
}


state_id_t fsm_run( const state_t *states, stream_t *stream, void *ctx ) {
    uint8_t c;
    state_id_t state = FSM_INITIAL_STATE;

    while( stream_get( stream, &c ) ) {
        state = fsm_step( &states[state], c, state, ctx );
        if( state < 0 || state == FSM_END_STATE ) {
            return state;
        }
    }

    if( stream->error ) {
        return FSM_ERROR_STREAM;
    }

    /* executes the end of file transition */
    if( states[state].transition_eof.action ) {
        if( !states[state].transition_eof.action( ctx ) ) {
            return FSM_ERROR_TRANSITION;
        }
    }
    return states[state].transition_eof.next_state;
}
