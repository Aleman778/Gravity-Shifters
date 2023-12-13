#include "raylib.h"
#include "types.h"
#include "math.h"
#include "particles.h"
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
    Vine,
    Gravity_Normal,
    Gravity_Inverted,
};

enum Entity_Layer {
    Layer_Entity,
    Layer_Background,
    
    Layer_Count,
};

enum Collision {
    Col_None   = 0,
    Col_Left   = bit(1),
    Col_Right  = bit(2),
    Col_Top    = bit(3),
    Col_Bottom = bit(4),
    
    Col_All = Col_Left | Col_Right | Col_Top | Col_Bottom
};

enum {
    Tutorial_None = 0,
    Tutorial_Walk = bit(1),
    Tutorial_Jump = bit(2),
    Tutorial_Long_Jump = bit(3),
    Tutorial_Switch_Gravity = bit(4),
};
typedef u8 Tutorial;

struct Box {
    v2 p;
    v2 size;
};

struct Trigger {
    union {
        struct {
            v2 p;
            v2 size;
        };
        Box collider;
    };
    string tag;
};

struct Entity {
    string* tag;
    
    // Physics/ collider (and render shape)
    Entity* collided_with;
    Collision collision;
    int map_collision;
    
    union {
        struct {
            v2 p;
            v2 size;
        };
        Box collider;
    };
    v2 offset;
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
    
    Collision collision_mask; // make solid on certain directions
    bool is_solid;
    bool is_trigger;
    bool is_rigidbody;
    bool is_grounded;
    bool is_jumping;
};

enum Game_Mode {
    GameMode_Level,
    GameMode_Cutscene_Ability,
    GameMode_Death_Screen,
};

struct Saved_Entity {
    Entity_Type type;
    v2 p;
    v2 direction;
    bool invert_gravity;
};

#define DEF_TEXUTRE2D \
TEX2D(tiles, "tileset_rock.png") \
TEX2D(character, "character.png") \
TEX2D(character_inv, "character_inv.png") \
TEX2D(coin, "moon_coin.png") \
TEX2D(ui_coin, "ui_coin.png") \
TEX2D(spikes, "spikes.png") \
TEX2D(plum, "plum.png") \
TEX2D(plum_dead, "plum_dead.png") \
TEX2D(vine, "vine.png") \
TEX2D(space, "space.png") \
TEX2D(ui_walk_keyboard, "ui_walk_keyboard.png") \
TEX2D(ui_jump_keyboard, "ui_jump_keyboard.png") \
TEX2D(ui_long_jump_keyboard, "ui_jump_keyboard.png") \
TEX2D(ui_walk_gamepad, "ui_walk_gamepad.png") \
TEX2D(ui_jump_gamepad, "ui_jump_gamepad.png") \
TEX2D(ui_long_jump_gamepad, "ui_long_jump_gamepad.png") \

struct Game_State {
    Entity* player;
    
    Entity entities[255];
    int entity_count;
    
    Saved_Entity saved_entities[255];
    //int saved_entity_count;
    
    bool ability_unlock_gravity;
    
    Box colliders[255];
    int collider_count;
    
    Box checkpoints[20];
    int checkpoint_count;
    
    Trigger triggers[10];
    int trigger_count;
    
    Game_Mode mode;
    f32 mode_timer;
    
    Tutorial curr_tutorials;
    Tutorial finished_tutorials;
    
    s32 coins;
    s32 saved_coins;
    
    v2 camera_p;
    
    v2 start_p; // start position of the current mode
    Entity* ability_block; // for cutsceen when you get the ability
    
    v2 final_render_offset;
    f32 final_render_zoom;
    f32 final_render_rot;
    
    f32 normal_gravity;
    f32 fall_gravity;
    bool is_moon_gravity;
    
    bool use_gamepad;
    
    // Resource
#define TEX2D(name, ...) Texture2D texture_##name;
    DEF_TEXUTRE2D
#undef TEX2D
    Font font_default;
    
    Particle_System* ps_gravity;
    
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

void
update_tutorial(Game_State* game, bool begin, Tutorial tutorial) {
    if (begin) {
        if (!(game->finished_tutorials & tutorial) && 
            !(game->curr_tutorials & tutorial)) {
            game->curr_tutorials |= tutorial;
        }
    } else {
        if (game->curr_tutorials & tutorial) {
            game->curr_tutorials &= (~tutorial);
            game->finished_tutorials |= tutorial;
        }
    }
}

bool
is_tutorial_active(Game_State* game, Tutorial tutorial) {
    return game->curr_tutorials & tutorial;
}

inline void
set_game_mode(Game_State* game, Game_Mode mode) {
    game->mode = mode;
    game->mode_timer = 0.0f;
    game->final_render_offset = {};
    game->final_render_zoom = 1.0f;
    game->final_render_rot = 0.0f;
    
    if (game->player) {
        game->start_p = game->player->p;
    }
    
    reset_particle_system(game->ps_gravity);
}

#define for_array(arr, it, it_index) \
int it_index = 0; \
for (auto it = arr; \
it_index < fixed_array_count(arr); \
it_index++, it++)

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
        
        game->use_gamepad = true;
        
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
        
        game->use_gamepad = false;
    }
    return result;
}

inline Entity*
add_entity(Game_State* game, Entity_Type type) {
    assert(game->entity_count < fixed_array_count(game->entities) && "too many entities");
    Entity* entity = &game->entities[game->entity_count++]; 
    *entity = {};
    entity->type = type;
    return entity;
}

inline void
add_collider(Game_State* game, v2 p, v2 size) {
    assert(game->collider_count < fixed_array_count(game->colliders) && "too many colliders");
    Box* collider = &game->colliders[game->collider_count++];
    collider->p = p;
    collider->size = size;
}

inline void
add_trigger(Game_State* game, v2 p, v2 size, string tag) {
    assert(game->trigger_count < fixed_array_count(game->triggers) && "too many triggers");
    Trigger* trigger = &game->triggers[game->trigger_count++];
    trigger->p = p;
    trigger->size = size;
    trigger->tag = tag;
}

inline void
add_checkpoint(Game_State* game, v2 p, v2 size) {
    assert(game->checkpoint_count < fixed_array_count(game->checkpoints) && "too many checkpoints");
    Box* collider = &game->checkpoints[game->checkpoint_count++];
    collider->p = p;
    collider->size = size;
}

inline s32
round_f32_to_s32(f32 value) {
    return (s32) round(value);
}

inline v2
to_pixel_v2(Game_State* game, v2 world_p) {
    v2 result = (world_p - game->camera_p) * game->meters_to_pixels;
    result.x = floorf(result.x);
    result.y = floorf(result.y);
    return result;
}

inline v2
to_pixel_size_v2(Game_State* game, v2 world_p) {
    v2 result = world_p * game->meters_to_pixels;
    result.x = floorf(result.x);
    result.y = floorf(result.y);
    return result;
}

inline v2s
to_pixel(Game_State* game, v2 world_p) {
    v2 p = (world_p - game->camera_p) * game->meters_to_pixels;
    v2s result;
    result.x = (int) p.x;
    result.y = (int) p.y;
    return result;
}

inline v2s
to_pixel_size(Game_State* game, v2 world_p) {
    v2 p = world_p * game->meters_to_pixels;
    v2s result;
    result.x = (int) p.x;
    result.y = (int) p.y;
    return result;
}