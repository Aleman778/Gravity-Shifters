
#define BACKGROUND_COLOR BLACK
#define TILE_SIZE 16
#define GRAVITY 10
#define JUMP_VELOCITY -14

#include "game.h"

#include "format_tmx.cpp"
#include "physics.cpp"
#include "draw.cpp"

const s32 first_gid = 40;
const s32 box_collider_gid = 40;
const s32 player_gid = 41;
const s32 spikes_gid = 42;
const s32 coin_gid = 43;
const Entity_Type gid_to_entity_type[] = {
    Box_Collider, Player_Start, Spikes, Coin,
};

Entity*
convert_tmx_object_to_entity(Game_State* game, Tmx_Object* object) {
    if (object->gid == player_gid) {
        game->player->p = object->p;
        game->player->p.y -= 1;
        return game->player;
    }
    
    Entity* entity = spawn_entity(game, None);
    entity->p = object->p;
    entity->size = object->size;
    s32 index = object->gid - first_gid;
    if (index >= 0 && index < fixed_array_count(gid_to_entity_type)) {
        entity->type = gid_to_entity_type[index];
    } else {
        pln("warning: unknown object (gid=%d) at %f, %f", object->gid, object->p.x, object->p.y);
    }
    
    return entity;
}

void
game_setup_level(Game_State* game, Memory_Arena* level_arena, string filename) {
    Loaded_Tmx tmx = read_tmx_map_data(filename, level_arena);
    game->tile_map = tmx.tile_map;
    game->tile_map_width = tmx.tile_map_width;
    game->tile_map_height = tmx.tile_map_height;
    
    game->player = spawn_entity(game, Player);
    game->player->size = vec2(1.0f, 2.0f);
    game->player->is_rigidbody = true;
    game->player->max_speed.x = 5.0f;
    
    for (int object_index = 0; object_index < tmx.object_count; object_index++) {
        Tmx_Object* object = &tmx.objects[object_index];
        convert_tmx_object_to_entity(game, object);
    }
}

void
game_update_and_render(Game_State* game, RenderTexture2D render_target) {
    Game_Controller controller = get_controller(game);
    
    game->is_moon_gravity = false;
    if (game->is_moon_gravity) {
        game->gravity = 0.6f;
        game->fall_gravity = 0.8f;
        game->jump_velocity = -4;
        
    } else {
        game->gravity = 20;
        game->fall_gravity = 50;
        game->jump_velocity = -14;
    }
    
    // Update game
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        
        switch (entity->type) {
            case Player: {
                // Jump
                if (entity->is_grounded && controller.jump_pressed) {
                    entity->velocity.y = game->jump_velocity;
                    entity->is_jumping = true;
                }
                if (!controller.jump_down || entity->velocity.y >= 0.0f) {
                    entity->is_jumping = false;
                }
                
                // TODO(Alexander): must have!
                // Add cayote timer
                // Add jump buffering
                
                // Gravity
                entity->acceleration.y = game->gravity;
                if (!entity->is_jumping && !entity->is_grounded) {
                    entity->acceleration.y = game->fall_gravity;
                }
                
                // Walking
                entity->acceleration.x = controller.dir.x * 20.0f;
                if (game->is_moon_gravity) {
                    entity->acceleration.x = controller.dir.x * 2.0f;
                }
            } break;
        }
        
        update_rigidbody(game, entity);
    }
    
    // Camera
    const f32 camera_max_offset = 2;
    f32 camera_offset = game->camera_p.x - (game->player->p.x - game->game_width/2.0f);
    if (camera_offset < -camera_max_offset) {
        game->camera_p.x = game->player->p.x - game->game_width/2.0f - camera_max_offset;
    }
    if (camera_offset > camera_max_offset) {
        game->camera_p.x = game->player->p.x - game->game_width/2.0f + camera_max_offset;
    }
    
    if (game->camera_p.x < 0) {
        game->camera_p.x = 0;
    }
    
    
    // Render game
    BeginTextureMode(render_target);
    ClearBackground(BACKGROUND_COLOR);
    
    draw_tilemap(game);
    
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        v2s p = to_pixel(game, entity->p);
        v2s size = to_pixel_size(game, entity->size);
        
        DrawRectangle(p.x, p.y, size.width, size.height, RED);
    }
    
    EndTextureMode();
}


int
main() {
    Game_State game = {};
    game.game_width = 30;
    game.game_height = 18;
    game.game_scale = 2;
    game.render_width = game.game_width * TILE_SIZE;
    game.render_height = game.game_height * TILE_SIZE;
    game.screen_width = game.render_width * game.game_scale;
    game.screen_height = game.render_height * game.game_scale;
    
    InitWindow(game.screen_width, game.screen_height, "Bigmode Game Jam 2023");
    SetTargetFPS(60);
    
    Memory_Arena level_arena = {};
    game.meters_to_pixels = TILE_SIZE;
    game.pixels_to_meters = 1.0f/game.meters_to_pixels;
    game.texture_tiles = LoadTexture("assets/tileset_grass.png");
    
    RenderTexture2D render_target = LoadRenderTexture(game.render_width, game.render_height);
    SetTextureFilter(render_target.texture, TEXTURE_FILTER_POINT);
    
    game_setup_level(&game, &level_arena, string_lit("assets/level1.tmx"));
    
    while (!WindowShouldClose()) {
        game_update_and_render(&game, render_target);
        draw_texture_to_screen(&game, render_target);
    }
    
    CloseWindow();
    return 0;
}