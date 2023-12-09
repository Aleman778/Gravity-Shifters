
#define BACKGROUND_COLOR BLACK
#define TILE_SIZE 16
#define GRAVITY 10
#define JUMP_VELOCITY -14

#include "game.h"

#include "format_tmx.cpp"
#include "physics.cpp"
#include "draw.cpp"

const s32 first_gid = 41;
const s32 player_gid = 41;
const s32 player_inverted_gid = 42;
const Entity_Type gid_to_entity_type[] = {
    Player, Player, Spikes, Spikes_Top, Coin,
};

void
configure_entity(Game_State* game, Entity* entity) {
    switch (entity->type) {
        case Player: {
            entity->size = vec2(0.6f, 1.7f);
            entity->health = 1;
            entity->is_rigidbody = true;
            entity->max_speed.x = 5.0f;
            game->player = entity;
        } break;
        
        case Coin: {
            entity->sprite = &game->texture_coin;
            entity->frames = 5;
            entity->size = vec2(1.0f, 1.0f);
            entity->frame_duration = 2.0f;
        } break;
        
        case Spikes: {
            entity->sprite = &game->texture_spikes;
            entity->size = vec2(1.0f, 1.0f);
            entity->is_solid = false;
        } break;
        
        case Spikes_Top: {
            entity->sprite = &game->texture_spikes;
            entity->sprite_flipv = true;
            entity->size = vec2(1.0f, 1.0f);
            entity->is_solid = true;
        } break;
    }
    
}

void
save_game_state(Game_State* game) {
    for_array(game->entities, entity, i) {
        Saved_Entity* saved_entity = &game->saved_entities[i];
        saved_entity->type = entity->type;
        saved_entity->p = entity->p;
        saved_entity->invert_gravity = entity->invert_gravity;
    }
}

void
restore_game_state(Game_State* game) {
    for_array(game->entities, entity, i) {
        Saved_Entity* saved_entity = &game->saved_entities[i];
        *entity = {};
        entity->type = saved_entity->type;
        entity->p = saved_entity->p;
        entity->invert_gravity = saved_entity->invert_gravity;
        configure_entity(game, entity);
    }
}

Entity*
convert_tmx_object_to_entity(Game_State* game, Tmx_Object* object) {
    Entity* entity = spawn_entity(game, None);
    entity->p = object->p;
    entity->size = object->size;
    s32 index = object->gid - first_gid;
    if (index >= 0 && index < fixed_array_count(gid_to_entity_type)) {
        entity->type = gid_to_entity_type[index];
        
        configure_entity(game, entity);
        if (object->gid == player_gid) {
            entity->p.y -= entity->size.y - object->size.y;
        } else if (object->gid == player_inverted_gid) {
            entity->invert_gravity = true;
        } 
    } else {
        pln("warning: unknown object (gid=%d) at %f, %f", object->gid, object->p.x, object->p.y);
    }
    
    return entity;
}

void
game_setup_level(Game_State* game, Memory_Arena* level_arena, string filename) {
    clear(level_arena);
    game->entity_count = 0;
    
    Loaded_Tmx tmx = read_tmx_map_data(filename, level_arena);
    game->tile_map = tmx.tile_map;
    game->tile_map_width = tmx.tile_map_width;
    game->tile_map_height = tmx.tile_map_height;
    
    for (int object_index = 0; object_index < tmx.object_count; object_index++) {
        Tmx_Object* object = &tmx.objects[object_index];
        switch (object->group) {
            case TmxObjectGroup_Entities: {
                convert_tmx_object_to_entity(game, object);
            } break;
            
            case TmxObjectGroup_Colliders: {
                add_collider(game, object->p, object->size);
            } break;
            
            case TmxObjectGroup_Checkpoints: {
                add_checkpoint(game, object->p, object->size);
            } break;
        }
    }
    
    assert(game->player);
    save_game_state(game);
}

void
kill_player(Game_State* game) {
    game->player->health = 0;
}

void
update_player(Game_State* game, Entity* entity, Game_Controller* controller) {
    const f32 gravity = 20;
    const f32 fall_gravity = 50;
    const f32 jump_velocity = -14;
    f32 gravity_sign = 1;
    if (entity->invert_gravity) {
        gravity_sign = -1;
    }
    
    // Jump
    if (entity->is_grounded && controller->jump_pressed) {
        entity->velocity.y = jump_velocity*gravity_sign;
        entity->is_jumping = true;
    }
    if (!controller->jump_down || entity->velocity.y*gravity_sign >= 0.0f) {
        entity->is_jumping = false;
    }
    
    // Change gravity
    if (entity->is_grounded && controller->action_pressed) {
        entity->invert_gravity = !entity->invert_gravity;
        entity->is_grounded = false;
    }
    
    
    // TODO(Alexander): must have!
    // Add cayote timer
    // Add jump buffering
    
    // Gravity
    entity->acceleration.y = gravity*gravity_sign;
    if (!entity->is_jumping && !entity->is_grounded) {
        entity->acceleration.y = fall_gravity*gravity_sign;
    }
    
    // Walking
    entity->acceleration.x = controller->dir.x * 20.0f;
    if (game->is_moon_gravity) {
        entity->acceleration.x = controller->dir.x * 2.0f;
    }
    
    // Run physics
    update_rigidbody(game, entity);
    
    // Check collisions with entites
    if (entity->collided_with) {
        Entity* other = entity->collided_with;
        switch (other->type) {
            case Spikes: {
                if (!other->sprite_flipv && entity->collision == Col_Bottom) {
                    kill_player(game);
                }
                if (other->sprite_flipv && entity->collision == Col_Top) {
                    kill_player(game);
                }
            } break;
        }
    }
    
    if (entity->is_grounded && entity->health > 0) {
        // Save restore point (within checkpoint regions)
        
        for_array(game->checkpoints, check, _) {
            if (box_check(entity->collider, *check)) {
                save_game_state(game);
            }
        }
    }
    
    // Pickup items
    for (int j = 0; j < game->entity_count; j++) {
        Entity* other = &game->entities[j];
        bool collided = false;
        if (other->type == Coin) {
            collided = box_check(entity->p, entity->p + entity->size, 
                                 other->p, other->p + other->size);
        }
        if (!collided) continue;
        
        switch (other->type) {
            case Coin: {
                other->type = None;
                game->player->coins++;
            } break;
        }
    }
}

void
game_update_and_render_level(Game_State* game) {
    Game_Controller controller = get_controller(game);
    
    // Update game
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        
        switch (entity->type) {
            case Player: {
                update_player(game, entity, &controller);
            } break;
            
            default: {
                update_rigidbody(game, entity);
            } break;
        }
        
        // Kill areas
        if (entity->p.y < -entity->size.y - 0.5f ||
            entity->p.y > game->game_height + 0.5f) {
            
            entity->health = 0;
            if (entity->type != Player) {
                entity->type = None;
            }
        }
    }
    
    if (game->player->health <= 0) {
        game->mode = GameMode_Death_Screen;
    }
    
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        
        
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
    
    draw_tilemap(game);
    
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        draw_entity(game, entity);
    }
}

void
game_update_and_render_death_screen(Game_State* game) {
    // Play death animation and restore gameplay
    restore_game_state(game);
    game->mode = GameMode_Level;
    
    draw_tilemap(game);
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        draw_entity(game, entity);
    }
}


void
game_update_and_render(Game_State* game, RenderTexture2D render_target) {
    
    // Render game
    BeginTextureMode(render_target);
    ClearBackground(DARKGRAY);
    
    switch (game->mode) {
        case GameMode_Level: {
            game_update_and_render_level(game);
        } break;
        
        case GameMode_Death_Screen: {
            game_update_and_render_death_screen(game);
        } break;
    }
    
    EndTextureMode();
}

void
game_draw_ui(Game_State* game, f32 scale) {
    cstring coins = TextFormat("%d", game->player->coins);
    Vector2 p = { 8*scale, 8*scale };
    DrawTextureEx(game->texture_ui_coin, p, 0, scale, WHITE);
    p.x += (game->texture_ui_coin.width + 1.0f) * scale;
    p.y += scale;
    DrawTextEx(game->font_default, coins, p, 14*scale, 0, WHITE);
}

int
main() {
    Game_State game = {};
    game.game_width = 40;
    game.game_height = 21;
    game.game_scale = 3;
    game.render_width = game.game_width * TILE_SIZE;
    game.render_height = game.game_height * TILE_SIZE;
    game.screen_width = game.render_width * game.game_scale;
    game.screen_height = game.render_height * game.game_scale;
    
    InitWindow(game.screen_width, game.screen_height, "Bigmode Game Jam 2023");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    
    Memory_Arena level_arena = {};
    game.meters_to_pixels = TILE_SIZE;
    game.pixels_to_meters = 1.0f/game.meters_to_pixels;
#define TEX2D(name, filename) game.texture_##name = LoadTexture("assets/"##filename);
    DEF_TEXUTRE2D
#undef TEX2D
    
    game.font_default = LoadFontEx("assets/Retro Gaming.ttf", 14*4, 0, 0);
    SetTextureFilter(game.font_default.texture, TEXTURE_FILTER_POINT);
    
    RenderTexture2D render_target = LoadRenderTexture(game.render_width, game.render_height);
    SetTextureFilter(render_target.texture, TEXTURE_FILTER_POINT);
    
    game_setup_level(&game, &level_arena, string_lit("assets/level1.tmx"));
    
    while (!WindowShouldClose()) {
        
        
        if (IsKeyPressed(KEY_F)) {
            ToggleFullscreen();
        }
        
        // TODO: maybe resize the game size on window resize?
        //if (IsWindowResized()) {
        //UnloadRenderTexture(render_target);
        //render_target = LoadRenderTexture(game.render_width, game.render_height);
        //SetTextureFilter(render_target.texture, TEXTURE_FILTER_POINT);
        //}
        
        
        game_update_and_render(&game, render_target);
        draw_texture_to_screen(&game, render_target);
    }
    
    CloseWindow();
    return 0;
}