/* PlatformLayers/os.h */
#ifndef OS_SENTRY
#define OS_SENTRY

#include "defs.h"

/// OS API for the game /// 

// Limited to 4kb (@TODO: remove impl constr)
void os_debug_printf(const char *format, ...);

// @TODO: implement game mem instead of this
// @TODO: maybe u8 instead of void?
typedef struct allocated_mem_tag {
    void *mem;
    u32 byte_size;
} allocated_mem_t;

allocated_mem_t os_allocate_mem(u32 size);
allocated_mem_t os_free_mem(allocated_mem_t *alloc);

// @TODO: make this more robust someday
typedef struct mapped_file_tag {
    void *mem;
    u64 byte_size;
} mapped_file_t;

// @TODO: error codes?
bool os_map_file(const char *path, mapped_file_t *out_file);
void os_unmap_file(mapped_file_t *file);

f32 os_get_time_in_frame();
u64 os_get_clocks_in_frame();

/// Game API for the OS Layer ///

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
    u32 pitch;

    u32 bytes_per_pixel;
    u32 byte_size;
} offscreen_buffer_t;

typedef struct sound_buffer_tag {
    f32 *samples;
    u32 samples_cnt;
    u32 samples_per_sec;
} sound_buffer_t;

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound);
void game_update_and_render(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound, f32 dt);
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

inline bool input_key_is_down(input_state_t *input, u32 key)
{
    if (key >= INPUT_KEY_MAX)
        return false;
    return input->pressed_keys[key].is_down;
}

// @TODO: I dont particularly like this separation
inline bool input_char_is_down(input_state_t *input, u32 c)
{
    return input_key_is_down(input, char_to_input_key(c));
}

/// Utilities for all ///
// @TODO: move this stuff somewhere?

inline bool structs_are_equal(u8 *s1, u8 *s2, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (*s1 != *s2)
            return false;
        s1++;
        s2++;
    }
    
    return true;
}

#define PANIC do { *((u8 *)NULL) = 0; } while (0)

#if USE_ASSERTIONS
  
  #define ASSERT(_e) if(!(_e)) { os_debug_printf("Assertion (" #_e ") failed at %s:%d\n", __FILE__, __LINE__); PANIC; }
  #define ASSERTF(_e, _fmt, ...) if(!(_e)) { os_debug_printf("(%s:%d) " _fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); PANIC; }
  
  #define __ASSERT_GLUE(_a, _b) _a ## _b
  #define _ASSERT_GLUE(_a, _b) __ASSERT_GLUE(_a, _b)
  #define STATIC_ASSERT(_e) enum { _ASSERT_GLUE(s_assert_fail_, __LINE__) = 1 / (int)(!!(_e)) }

#else

  #define ASSERT(_e)
  #define ASSERTF(_e, _fmt, ...)
  #define STATIC_ASSERT(_e)

#endif

// @TODO: different levels of logging
#if USE_LOGGING

  #define LOG(_fmt, ...) os_debug_printf(_fmt, ##__VA_ARGS__);

#else

  #define LOG(_fmt, ...)

#endif

#endif
