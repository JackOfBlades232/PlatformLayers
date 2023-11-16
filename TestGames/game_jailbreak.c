/* PlatformLayers/Windows/test_game.c */
#include "os.h"

#include <math.h>
#include <string.h>

// @TODO: impl

static inline bool input_key_is_down(input_state_t *input, u32 key)
{
    if (key >= INPUT_KEY_MAX)
        return false;
    return input->pressed_keys[key].is_down;
}

// @TODO: I dont particularly like this separation
static inline bool input_char_is_down(input_state_t *input, u32 c)
{
    return input_key_is_down(input, char_to_input_key(c));
}

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{

}

void game_deinit(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{

}

void game_update_and_render(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound, float dt)
{
    memset(backbuffer->bitmap_mem, 0, backbuffer->byte_size);
}

void game_redraw(offscreen_buffer_t *backbuffer)
{
    memset(backbuffer->bitmap_mem, 0, backbuffer->byte_size);
}
