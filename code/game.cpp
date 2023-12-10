
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
    Player, Player, Spikes, Spikes_Top, Coin, Enemy_Plum, None, Vine, Gravity_Inverted, Gravity_Normal
};

void
configure_entity(Game_State* game, Entity* entity) {
    switch (entity->type) {
        case Player: {
            entity->size = vec2(0.6f, 1.5f);
            entity->health = 1;
            entity->is_rigidbody = true;
            entity->max_speed.x = 5.0f;
            entity->max_speed.y = 20.0f;
            if (!entity->invert_gravity) {
                entity->p.y -= entity->size.y - 1.0f;
            }
            game->player = entity;
        } break;
        
        case Coin: {
            entity->sprite = &game->texture_coin;
            entity->frames = 5;
            entity->size = vec2(1.0f, 1.0f);
            entity->frame_duration = 2.0f;
            entity->is_trigger = true;
        } break;
        
        case Spikes: {
            entity->sprite = &game->texture_spikes;
            entity->size = vec2(1.0f, 1.0f);
            entity->is_solid = true;
        } break;
        
        case Spikes_Top: {
            entity->sprite = &game->texture_spikes;
            entity->size = vec2(1.0f, 1.0f);
            entity->is_solid = true;
        } break;
        
        case Enemy_Plum: {
            entity->health = 1;
            entity->sprite = &game->texture_plum;
            entity->max_speed.x = 2.0f;
            entity->max_speed.y = 10.0f;
            if (entity->direction.y < 0) {
                entity->invert_gravity = true;
            }
            entity->size = vec2(1.0f, 1.0f);
            entity->is_rigidbody = true;
        } break;
        
        case Vine: {
            entity->sprite = &game->texture_vine;
            entity->size = vec2(1.0f, 1.0f);
            if (entity->direction.y < 0) {
                entity->offset.y = -1.0f;
                entity->collision_mask = Col_Top;
            } else {
                entity->offset.y = 1.0f;
                entity->collision_mask = Col_Bottom;
            }
        } break;
        
        case Gravity_Normal:
        case Gravity_Inverted: {
            entity->sprite = &game->texture_vine;
            entity->size = vec2(1.0f, 1.0f);
            entity->offset.y += entity->type == Gravity_Inverted ? -0.2f : 0.2f;
            entity->is_trigger = true;
        } break;
    }
}


int
get_tile(Game_State* game, int x, int y) {
    if (x >= 0 && y >= 0 && x < game->tile_map_width && y < game->tile_map_height) {
        return game->tile_map[y*game->tile_map_width + x];
    }
    return 0;
}

struct Surround_Tiles {
    int left;
    int middle;
    int right;
};

Surround_Tiles
get_floor_tiles(Game_State* game, Entity* entity, f32 range=1.0f) {
    int y_under;
    if (entity->invert_gravity) {
        y_under = (int) (entity->p.y - 0.2f);
    } else {
        y_under = (int) (entity->p.y + entity->size.y + 0.2f);
    }
    
    f32 mid = entity->p.x + entity->size.x/2.0f;
    Surround_Tiles result = {};
    result.left = get_tile(game, (int) (mid - range), y_under);
    result.middle = get_tile(game, (int) mid, y_under);
    result.right = get_tile(game, (int) (mid + range), y_under);
    return result;
}


void
save_game_state(Game_State* game) {
    game->saved_coins = game->coins;
    for_array(game->entities, entity, i) {
        Saved_Entity* saved_entity = &game->saved_entities[i];
        saved_entity->type = entity->type;
        saved_entity->p = entity->p;
        saved_entity->direction = entity->direction;
        if (saved_entity->type == Player) {
            
            Surround_Tiles s = get_floor_tiles(game, entity);
            if (s.middle) {
                saved_entity->p.x = ((int) entity->p.x) + 0.2f;
            } else if (s.left) {
                saved_entity->p.x = ((int) entity->p.x - 1) + 0.2f;
            } else if (s.right) {
                saved_entity->p.x = ((int) entity->p.x + 1) + 0.2f;
            }
        }
        saved_entity->invert_gravity = entity->invert_gravity;
    }
}

void
restore_game_state(Game_State* game) {
    game->coins = game->saved_coins;
    for_array(game->entities, entity, i) {
        Saved_Entity* saved_entity = &game->saved_entities[i];
        *entity = {};
        entity->type = saved_entity->type;
        entity->p = saved_entity->p;
        entity->direction = saved_entity->direction;
        entity->invert_gravity = saved_entity->invert_gravity;
        configure_entity(game, entity);
    }
}

Entity*
convert_tmx_object_to_entity(Game_State* game, Tmx_Object* object) {
    Entity* entity = add_entity(game, None);
    entity->p = object->p;
    entity->size = object->size;
    
    entity->direction.x = (object->gid & bit(31)) ? -1.0f : 1.0f;
    entity->direction.y = (object->gid & bit(30)) ? -1.0f : 1.0f;
    object->gid &= bit(30) - 1;
    
    s32 index = object->gid - first_gid;
    if (index >= 0 && index < fixed_array_count(gid_to_entity_type)) {
        entity->type = gid_to_entity_type[index];
        
        if (object->gid == player_inverted_gid) {
            entity->invert_gravity = true;
        } 
        configure_entity(game, entity);
    } else {
        pln("warning: unknown object (gid=%d) at %f, %f", object->gid, object->p.x, object->p.y);
    }
    
    return entity;
}

void
game_setup_level(Game_State* game, Memory_Arena* level_arena, string filename) {
    clear(level_arena);
    game->entity_count = 0;
    game->collider_count = 0;
    game->trigger_count = 0;
    game->checkpoint_count = 0;
    
    memset(game->entities, 0, sizeof(game->entities));
    memset(game->colliders, 0, sizeof(game->colliders));
    memset(game->triggers, 0, sizeof(game->triggers));
    memset(game->checkpoints, 0, sizeof(game->checkpoints));
    
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
            
            case TmxObjectGroup_Triggers: {
                add_trigger(game, object->p, object->size);
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
kill_entity(Entity* entity) {
    entity->health = 0;
}

void
update_player(Game_State* game, Entity* player, Game_Controller* controller) {
    const f32 gravity = game->normal_gravity;
    const f32 fall_gravity = game->fall_gravity;
    const f32 jump_velocity = -14;
    f32 gravity_sign = 1;
    if (player->invert_gravity) {
        gravity_sign = -1;
    }
    
    // Jump
    if (player->is_grounded && controller->jump_pressed) {
        player->velocity.y = jump_velocity*gravity_sign;
        player->is_jumping = true;
    }
    if (!controller->jump_down || player->velocity.y*gravity_sign >= 0.0f) {
        player->is_jumping = false;
    }
    
    // Change gravity
    if (player->is_grounded && controller->action_pressed) {
        player->invert_gravity = !player->invert_gravity;
        player->velocity.y = jump_velocity*gravity_sign;
        player->is_grounded = false;
    }
    
    
    // TODO(Alexander): must have!
    // Add cayote timer
    // Add jump buffering
    
    // Gravity
    player->acceleration.y = gravity*gravity_sign;
    if (!player->is_jumping && !player->is_grounded) {
        player->acceleration.y = fall_gravity*gravity_sign;
    }
    
    // Walking
    player->acceleration.x = controller->dir.x * 20.0f;
    if (game->is_moon_gravity) {
        player->acceleration.x = controller->dir.x * 2.0f;
    }
    
    // Run physics
    update_rigidbody(game, player);
    
    // Check collisions with entites
    if (player->collided_with) {
        Entity* other = player->collided_with;
        switch (other->type) {
            case Spikes: {
                if (player->collision == (Col_Top | Col_Bottom)) {
                    kill_entity(player);
                }
            } break;
            
            case Spikes_Top: {
                if (player->collision & (Col_Top | Col_Bottom)) {
                    kill_entity(player);
                }
            } break;
            
            case Enemy_Plum: {
                if (!player->is_grounded && 
                    ((!other->invert_gravity && player->collision & Col_Bottom) ||
                     (other->invert_gravity && player->collision & Col_Top))) {
                    kill_entity(other);
                    // bounce
                    if (controller->jump_down) {
                        player->velocity.y = jump_velocity*gravity_sign;
                        player->is_jumping = true;
                    } else {
                        player->velocity.y = jump_velocity/2.0f*gravity_sign;
                    }
                } else {
                    kill_entity(player);
                }
            } break;
            
            case Coin: {
                other->type = None;
                game->coins++;
            } break;
            
            case Gravity_Normal: {
                player->invert_gravity = false;
            } break;
            
            case Gravity_Inverted: {
                player->invert_gravity = true;
            } break;
        }
    }
    
    if (player->is_grounded && !player->collided_with && player->health > 0) {
        // Save restore point (within checkpoint regions)
        bool is_within_checkpoint = false;
        for_array(game->checkpoints, checkpoint, checkpoint_index) {
            if (checkpoint_index >= game->checkpoint_count) {
                break;
            }
            
            if (box_check(player->collider, *checkpoint)) {
                is_within_checkpoint = true;
                break;
            }
        }
        
        
        if (is_within_checkpoint) {
            // Make sure we don't save next to an enemy and soft lock the game
            bool is_nearby_enemy = false;
            for_array(game->entities, other, _2) {
                if (other->type == Enemy_Plum) {
                    Box area = other->collider;
                    area.p -= 5.0f;
                    area.size += 10.0f;
                    
                    if (box_check(player->collider, area)) {
                        is_nearby_enemy = true;
                    }
                }
            }
            
            if (!is_nearby_enemy) {
                save_game_state(game);
            }
        }
    }
}

void
update_enemy_plum(Game_State* game, Entity* entity) {
    if (entity->health <= 0) {
        entity->type = Enemy_Plum_Dead;
        entity->sprite = &game->texture_plum_dead;
        entity->is_rigidbody = false;
        return;
    }
    
    f32 gravity_sign = 1;
    if (entity->invert_gravity) {
        gravity_sign = -1;
    }
    
    if (entity->direction.x == 0) {
        entity->direction.x = -1.0f;
    }
    
    if (entity->is_grounded) {
        entity->acceleration.x = 5.0f * entity->direction.x;
    }
    entity->acceleration.y = game->fall_gravity * gravity_sign;
    
    // Run physics
    update_rigidbody(game, entity);
    
    
    if (entity->is_grounded) {
        Surround_Tiles s = get_floor_tiles(game, entity, 1.0f);
        if (!s.left) entity->direction.x = 1.0f;
        if (!s.right) entity->direction.x = -1.0f;
    }
    
    if (entity->collided_with) {
        Entity* other = entity->collided_with;
        switch (other->type) {
            case Player: {
                kill_entity(other);
            } break;
            
            case Gravity_Normal: {
                entity->invert_gravity = false;
            } break;
            
            case Gravity_Inverted: {
                entity->invert_gravity = true;
            } break;
        }
    }
    
    if (entity->map_collision & Col_Left) {
        entity->direction.x = 1.0f;
    } else if (entity->map_collision & Col_Right) {
        entity->direction.x = -1.0f;
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
            
            case Enemy_Plum: {
                update_enemy_plum(game, entity);
            } break;
            
            case Vine: {
                bool expand = entity->direction.y < 0 ? 
                    game->player->invert_gravity : !game->player->invert_gravity;
                
                if (expand) {
                    if (entity->direction.y < 0) {
                        entity->size = vec2(6.0f, 1.0f);
                        //entity->size = vec2(0.2f, 6.0f);
                    } else {
                        entity->size = vec2(6.0f, 1.0f);
                        if (entity->direction.x < 0) {
                            entity->offset.x = -5.0f;
                        }
                    }
                    entity->is_solid = true;
                } else {
                    entity->size = vec2(1.0f, 1.0f);
                    entity->offset.x = 0;
                    entity->is_solid = false;
                }
                
            } break;
            
            default: {
                update_rigidbody(game, entity);
            } break;
        }
        
        // Update animations
        if (entity->frames > 1) {
            entity->frame_advance += GetFrameTime();
            f32 anim_duration = entity->frame_duration * entity->frames;
            if (entity->frame_advance > anim_duration) {
                entity->frame_advance -= anim_duration;
            }
        }
        
        // Kill areas
        if ((entity->invert_gravity  && entity->p.y < -entity->size.y - 0.5f) ||
            (!entity->invert_gravity && entity->p.y > game->game_height + 0.5f)) {
            
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
    
    draw_tilemap(game);
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        draw_entity(game, entity);
    }
    
    game->mode = GameMode_Level;
}


void
game_update_and_render(Game_State* game, RenderTexture2D render_target) {
    
    // Render game
    BeginTextureMode(render_target);
    ClearBackground(BACKGROUND_COLOR);
    
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
    cstring coins = TextFormat("%d", game->coins);
    Vector2 p = { 8*scale, 8*scale };
    DrawTextureEx(game->texture_ui_coin, p, 0, scale, WHITE);
    p.x += (game->texture_ui_coin.width + 1.0f) * scale;
    p.y += scale;
    DrawTextEx(game->font_default, coins, p, 14*scale, 0, WHITE);
}

#define DEF_LEVEL1 \
LVL("level1") \
LVL("level1_2") \
LVL("level1_3") \
LVL("level1_4") \
LVL("level1_5")

cstring level_assets[] = {
#define LVL(filename) "assets/"##filename##".tmx", 
    DEF_LEVEL1
#undef LVL
};

int
main() {
    Game_State game = {};
    game.game_width = 40;
    game.game_height = 22;
    game.game_scale = 2;
    game.render_width = game.game_width * TILE_SIZE;
    game.render_height = game.game_height * TILE_SIZE;
    game.screen_width = game.render_width * game.game_scale;
    game.screen_height = game.render_height * game.game_scale;
    
    game.normal_gravity = 20;
    game.fall_gravity = 50;
    
    //InitWindow(1280, 720, "Bigmode Game Jam 2023");
    InitWindow(game.screen_width, game.screen_height, "Bigmode Game Jam 2023");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    
    SetExitKey(0);
    
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
    
    cstring filename = level_assets[0];
    game_setup_level(&game, &level_arena, string_lit(filename));
    
    while (!WindowShouldClose()) {
        
        
        if (IsKeyPressed(KEY_F)) {
            MaximizeWindow();
            ToggleFullscreen();
        }
        
#if DEVELOPER
        int select_level = GetCharPressed() - '0';
        if (select_level >= 0 && select_level < fixed_array_count(level_assets)) {
            game_setup_level(&game, &level_arena, string_lit(level_assets[select_level]));
        }
#endif
        
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