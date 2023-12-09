#include "raylib.h"
#include "types.h"
#include "math.h"
#include "tokenizer.h"
#include "memory.h"

enum Entity_Type {
    None = 0,
    Player,
    Spikes,
    Spikes_Top,
    Coin,
    Enemy_Plum,
    Enemy_Plum_Dead,
};

enum Entity_Layer {
    Layer_Background,
    Layer_Player,
    Layer_Foreground,
    
    Layer_Count,
};

enum Collision_Result {
    Col_None   = 0,
    Col_Left   = bit(1),
    Col_Right  = bit(2),
    Col_Top    = bit(3),
    Col_Bottom = bit(4),
};

struct Box {
    v2 p;
    v2 size;
};

struct Entity {
    // Physics/ collider (and render shape)
    Entity* collided_with;
    Collision_Result collision;
    int map_collision;
    
    union {
        struct {
            v2 p;
            v2 size;
        };
        Box collider;
    };
    v2 velocity;
    v2 acceleration;
    v2 max_speed;
    v2 direction;
    
    bool invert_gravity;
    f32 gravity;
    f32 fall_gravity;
    
    // Rendering
    Texture2D* sprite;
    f32 frame_advance;
    f32 frame_duration;
    int frames;
    
    Entity_Type type;
    Entity_Layer layer;
    
    s32 health;
    s32 coins;
    int safe_frames;
    
    bool is_solid;
    bool is_rigidbody;
    bool is_grounded;
    bool is_jumping;
};

enum Game_Mode {
    GameMode_Level,
    GameMode_Death_Screen,
};

struct Saved_Entity {
    Entity_Type type;
    v2 p;
    bool invert_gravity;
};

#define DEF_TEXUTRE2D \
TEX2D(tiles, "tileset_grass.png") \
TEX2D(coin, "coin.png") \
TEX2D(ui_coin, "ui_coin.png") \
TEX2D(spikes, "spikes.png") \
TEX2D(plum, "plum.png") \
TEX2D(plum_dead, "plum_dead.png")

#define MAX_ENTITY_COUNT 255
#define MAX_COLLIDER_COUNT 255
#define MAX_TRIGGER_COUNT 10

struct Game_State {
    Entity* player;
    
    Entity entities[MAX_ENTITY_COUNT];
    int entity_count;
    
    Saved_Entity saved_entities[MAX_ENTITY_COUNT];
    //int saved_entity_count;
    
    Box colliders[MAX_COLLIDER_COUNT];
    int collider_count;
    
    Box triggers[MAX_TRIGGER_COUNT];
    int trigger_count;
    
    Game_Mode mode;
    s32 cutscene_timer;
    
    v2 camera_p;
    
    f32 normal_gravity;
    f32 fall_gravity;
    bool is_moon_gravity;
    
    // Resource
#define TEX2D(name, ...) Texture2D texture_##name;
    DEF_TEXUTRE2D
#undef TEX2D
    Font font_default;
    
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

#define for_array(arr, it, it_index) \
int it_index = 0; \
for (auto it = arr; \
it_index < fixed_array_count(arr); \
it_index++, it++)

void game_draw_ui(Game_State* game, f32 scale);

struct Game_Controller {
    v2 dir;
    
    bool jump_pressed;
    bool jump_down;
    bool action_pressed;
    
    bool is_gamepad;
};

Game_Controller
get_controller(Game_State* game, int gamepad_index=0) {
    
    Game_Controller result;
    
    if (IsGamepadAvailable(gamepad_index)) {
        int jump_button = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        int action_button = GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
        
        
        result.dir.x = GetGamepadAxisMovement(gamepad_index, 0);
        result.dir.y = GetGamepadAxisMovement(gamepad_index, 1);
        result.jump_pressed = IsGamepadButtonPressed(gamepad_index, jump_button);
        result.jump_down = IsGamepadButtonDown(gamepad_index, jump_button);
        result.action_pressed = IsGamepadButtonPressed(gamepad_index, action_button);
        
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
        int key_action = KEY_ENTER;
        
        result.dir.x = IsKeyDown(key_left)*-1.0f + IsKeyDown(key_right)*1.0f;
        result.dir.y = IsKeyDown(key_up)*-1.0f + IsKeyDown(key_down)*1.0f;
        result.jump_pressed = IsKeyPressed(key_jump);
        result.jump_down = IsKeyDown(key_jump);
        
        result.action_pressed = IsKeyPressed(key_action);
    }
    return result;
}

inline Entity*
add_entity(Game_State* game, Entity_Type type) {
    assert(game->entity_count < MAX_ENTITY_COUNT && "too many entities");
    Entity* entity = &game->entities[game->entity_count++];
    *entity = {};
    entity->type = type;
    return entity;
}

inline void
add_collider(Game_State* game, v2 p, v2 size) {
    assert(game->collider_count < MAX_ENTITY_COUNT && "too many colliders");
    Box* collider = &game->colliders[game->collider_count++];
    collider->p = p;
    collider->size = size;
}

inline void
add_trigger(Game_State* game, v2 p, v2 size) {
    assert(game->trigger_count < MAX_ENTITY_COUNT && "too many colliders");
    Box* collider = &game->triggers[game->trigger_count++];
    collider->p = p;
    collider->size = size;
}

inline s32
round_f32_to_s32(f32 value) {
    return (s32) round(value);
}

inline v2
to_pixel_v2(Game_State* game, v2 world_p) {
    return (world_p - game->camera_p) * game->meters_to_pixels;
}

inline v2
to_pixel_size_v2(Game_State* game, v2 world_p) {
    return world_p * game->meters_to_pixels;
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
