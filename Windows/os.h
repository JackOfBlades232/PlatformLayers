/* PlatformLayers/Windows/os.h */
#include "defs.h"

// @TEST Graphics
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

enum input_spec_key_tag {
    // @NOTE: 'a'--'z' are considered 0..25 keys, '0'--'9' -- 25..34
    //        other keys start at 35
    INPUT_KEY_LMOUSE = ('z'-'a') + 1 + ('9'-'0') + 1,     
    INPUT_KEY_RMOUSE,     
    INPUT_KEY_MMOUSE,     
    INPUT_KEY_LEFT,
    INPUT_KEY_RIGHT,     
    INPUT_KEY_UP,        
    INPUT_KEY_DOWN,      
    INPUT_KEY_SPACE,     
    INPUT_KEY_ESC,       
    INPUT_KEY_ENTER,     
    INPUT_KEY_TAB,       
    INPUT_KEY_LSHIFT,    
    INPUT_KEY_RSHIFT,    
    INPUT_KEY_LCTRL,     
    INPUT_KEY_RCTRL,     
    INPUT_KEY_LALT,      
    INPUT_KEY_RALT,      
    INPUT_KEY_BACKSPACE, 
    INPUT_KEY_F1,        
    INPUT_KEY_F2,        
    INPUT_KEY_F3,        
    INPUT_KEY_F4,        
    INPUT_KEY_F5,        
    INPUT_KEY_F6,        
    INPUT_KEY_F7,        
    INPUT_KEY_F8,        
    INPUT_KEY_F9,        
    INPUT_KEY_F10,       
    INPUT_KEY_F11,       
    INPUT_KEY_F12,       

    INPUT_KEY_MAX
};

typedef struct input_key_state_tag {
    bool is_down;
    u32 times_pressed;
} input_key_state_t;

typedef struct input_state_tag {
    input_key_state_t pressed_keys[INPUT_KEY_MAX];
    u32 mouse_x, mouse_y; // in pixels
    bool quit;
} input_state_t;

typedef struct offscreen_buffer_tag {
    u32 *bitmap_mem;
    u32 width;
    u32 height;
    u32 bytes_per_pixel;
    u32 byte_size;
    u32 pitch;
} offscreen_buffer_t;

typedef struct sound_buffer_tag {
    f32 *samples;
    u32 samples_cnt;
    u32 samples_per_sec;
} sound_buffer_t;

// @TODO: add Init/Deinit?
void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound);
void game_update_and_render(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound, float dt);
void game_deinit(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound);

// @NOTE: for window manager stuff, may be a good idea to just split the logic
void game_redraw(offscreen_buffer_t *backbuffer);

// @NOTE: this is currently exposed to the os layer for conversion.
//        If i start doing layouts I think the OS should have it's own conversion
// Converts input key and ascii chars to input key idx
inline u32 char_to_input_key(u32 c)
{
    if (c >= 'a' && c <= 'z') // ASCII char variants
        return c - 'a';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A';
    else if (c >= '0' && c <= '9')
        return c - '0' + ('z'-'a'+1);
    else
        return INPUT_KEY_MAX;
}
