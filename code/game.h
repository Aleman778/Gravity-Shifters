#include "raylib.h"
#include "types.h"
#include "math.h"
#include "tokenizer.h"
#include "memory.h"

enum Entity_Type {
    None = 0,
    Player,
    Player_Start,
    Spikes,
    Coin,
    Box_Collider
};

struct Entity {
    // Physics/ collider (and render shape)
    Entity* collided_with;
    v2 p;
    v2 size;
    v2 velocity;
    v2 acceleration;
    v2 max_speed;
    
    // Animation
    s32 num_frames;
    f32 frame_advance;
    f32 frame_advance_rate;
    
    Entity_Type type;
    
    bool collided;
    bool is_rigidbody;
    bool is_grounded;
    bool is_jumping;
};

#define MAX_ENTITY_COUNT 255

struct Game_State {
    Entity* player;
    
    Entity entities[MAX_ENTITY_COUNT];
    int entity_count;
    
    v2 camera_p;
    
    f32 gravity;
    f32 fall_gravity;
    bool is_moon_gravity;
    f32 jump_velocity;
    
    Texture2D texture_tiles;
    
    u8* tile_map;
    int tile_map_width;
    int tile_map_height;
    
    int game_width;
    int game_height;
    int game_scale;
    int render_width;
    int render_height;
    int screen_width;
    int screen_height;
    
    f32 meters_to_pixels;
    f32 pixels_to_meters;
};

struct Game_Controller {
    v2 dir;
    
    bool jump_pressed;
    bool jump_down;
    
    bool is_gamepad;
};

Game_Controller
get_controller(Game_State* game, int gamepad_index=0) {
    
    Game_Controller result;
    
    if (IsGamepadAvailable(gamepad_index)) {
        int jump_button = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        
        result.dir.x = GetGamepadAxisMovement(gamepad_index, 0);
        result.dir.y = GetGamepadAxisMovement(gamepad_index, 1);
        result.jump_pressed = IsGamepadButtonPressed(gamepad_index, jump_button);
        result.jump_down = IsGamepadButtonDown(gamepad_index, jump_button);
        
    } else {
        //int left = KEY_LEFT;
        //int right = KEY_RIGHT;
        //int up = KEY_UP;
        //int down = KEY_DOWN;
        int key_left = KEY_A;
        int key_right = KEY_D;
        int key_up = KEY_W;
        int key_down = KEY_S;
        int key_jump = KEY_SPACE;
        
        result.dir.x = IsKeyDown(key_left)*-1.0f + IsKeyDown(key_right)*1.0f;
        result.dir.y = IsKeyDown(key_up)*-1.0f + IsKeyDown(key_down)*1.0f;
        result.jump_pressed = IsKeyPressed(key_jump);
        result.jump_down = IsKeyDown(key_jump);
    }
    return result;
}

inline Entity*
spawn_entity(Game_State* game, Entity_Type type) {
    assert(game->entity_count < MAX_ENTITY_COUNT && "too many entities");
    Entity* entity = &game->entities[game->entity_count++];
    *entity = {};
    entity->type = type;
    return entity;
}

inline s32
round_f32_to_s32(f32 value) {
    return (s32) round(value);
}

inline v2s
to_pixel(Game_State* game, v2 world_p) {
    v2 p = (world_p - game->camera_p) * game->meters_to_pixels;
    v2s result;
    result.x = round_f32_to_s32(p.x);
    result.y = round_f32_to_s32(p.y);
    return result;
}

inline v2s
to_pixel_size(Game_State* game, v2 world_p) {
    v2 p = world_p * game->meters_to_pixels;
    v2s result;
    result.x = round_f32_to_s32(p.x);
    result.y = round_f32_to_s32(p.y);
    return result;
}
