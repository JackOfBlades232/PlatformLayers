/* PlatformLayers/Windows/test_game.c */
#include "../os.h"

#include <math.h>

#define M_PI 3.14159265359

// @TEST Controls
typedef struct movement_input_tag {
    f32 x, y;
} movement_input_t;

static movement_input_t get_movement_input(input_state_t *input)
{
    movement_input_t movement = { 0 };
    // @TODO: fix pressed letters testing
    if (input_key_is_down(input, INPUT_KEY_LEFT) || input_char_is_down(input, 'a'))  movement.x -= 1;
    if (input_key_is_down(input, INPUT_KEY_RIGHT) || input_char_is_down(input, 'd')) movement.x += 1;
    if (input_key_is_down(input, INPUT_KEY_UP) || input_char_is_down(input, 'w'))    movement.y -= 1;
    if (input_key_is_down(input, INPUT_KEY_DOWN) || input_char_is_down(input, 's'))  movement.y += 1;

    return movement;
}

// @TEST Graphics
static f32 x_offset  = 0;
static f32 y_offset  = 0;
static f32 dx        = 0;
static f32 dy        = 0;

static void render_gradient(offscreen_buffer_t *buffer, f32 x_offset, f32 y_offset)
{
    u32 width = buffer->width;
    u32 height = buffer->height;
    u32 pitch = buffer->pitch;

    u8 *row = (u8 *)buffer->bitmap_mem;
    for (u32 y = 0; y < height; y++) {
        u32 *pix = (u32 *)row;
        for (u32 x = 0; x < width; x++) {
            u8 blue = x + x_offset;
            u8 green = y + y_offset;
            *(pix++) = (green << 8) | blue;
        }

        row += pitch;
    }
}

static void update_gardient(input_state_t *input, f32 *x_offset, f32 *y_offset, f32 dt, u32 screen_w, u32 screen_h)
{
    const s32 offset_per_s = 648;

    if (input_key_is_down(input, INPUT_KEY_LMOUSE)) {
        *x_offset = screen_w - input->mouse_x;
        *y_offset = screen_h - input->mouse_y;
    } else {
        movement_input_t movement = get_movement_input(input);
        dx = offset_per_s * movement.x;
        dy = offset_per_s * movement.y;

        *x_offset += dx*dt;
        *y_offset += dy*dt;
    }
}

// @TEST Sound
static void output_audio_tone(sound_buffer_t *sbuf, input_state_t *input)
{
    enum wave_type_t { wt_square, wt_sine };

    //const enum wave_type_t wtype = wt_square;
    const enum wave_type_t wtype = wt_sine;
    const u32 base_tone_hz = 384;
    const u32 offset_tone_hz = 128;

    const f32 wave_volume = 0.25f;

    static u32 wave_counter = 0;
    static u32 prev_wave_period = 0;

    movement_input_t movement = get_movement_input(input);
    u32 wave_freq = base_tone_hz - offset_tone_hz*movement.y;
    u32 wave_period = sbuf->samples_per_sec/wave_freq;

    // Smooth tone switch
    if (wave_period != prev_wave_period && prev_wave_period != 0)
        wave_counter = (u32)((f32)wave_period * ((f32)wave_counter/prev_wave_period));

    prev_wave_period = wave_period;

    f32 *sample_out = sbuf->samples;
    for (u32 i = 0; i < sbuf->samples_cnt; i++) {
        if (wave_counter == 0)
            wave_counter = wave_period;

        f32 sample_val = 0;
        if (wtype == wt_square) {
            sample_val = wave_counter > wave_period/2 ?
                wave_volume : -wave_volume;
        } else if (wtype == wt_sine) {
            sample_val = wave_volume *
                sinf((f32)(wave_period-wave_counter)*2*M_PI/wave_period);
        }

        *(sample_out++) = sample_val;
        *(sample_out++) = sample_val;
        wave_counter--;
    }
}

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{

}

void game_deinit(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{

}

void game_update_and_render(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound, float dt)
{
    // @TEST Controls
    if (input_key_is_down(input, INPUT_KEY_ESC))
        input->quit = true; // @NOTE: input seems to be a two way control flow street with .quit
    update_gardient(input, &x_offset, &y_offset, dt,
                    backbuffer->width, backbuffer->height);

    // @TEST Sound
    output_audio_tone(sound, input);

    // @TEST Graphics
    render_gradient(backbuffer, x_offset, y_offset);
}

void game_redraw(offscreen_buffer_t *backbuffer)
{
    render_gradient(backbuffer, x_offset, y_offset);
}
