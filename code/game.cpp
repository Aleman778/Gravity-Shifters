
#define BACKGROUND_COLOR BLACK
#define TILE_SIZE 16
#define GRAVITY 10
#define JUMP_VELOCITY -14

#include "game.h"

#include "particles.cpp"
#include "format_tmx.cpp"
#include "physics.cpp"
#include "draw.cpp"

const s32 first_gid = 41;
const s32 player_inverted_gid = 42;
const Entity_Type gid_to_entity_type[] = {
    Player, Player, Spikes, Spikes, Coin, Enemy_Plum, None, Vine, Gravity_Inverted, Gravity_Normal
};

void
configure_entity(Game_State* game, Entity* entity) {
    switch (entity->type) {
        case Player: {
            entity->size = vec2(0.6f, 1.25f);
            entity->health = 1;
            entity->offset.x = -0.1f;
            entity->is_rigidbody = true;
            entity->max_speed.x = 5.0f;
            entity->max_speed.y = 20.0f;
            entity->frame_duration = 2.0f;
            entity->frames = 6;
            if (!entity->invert_gravity) {
                entity->p.y -= entity->size.y - 1.0f;
            }
            entity->sprite = &game->texture_character;
            entity->prev_invert_gravity = entity->invert_gravity;
            game->player = entity;
        } break;
        
        case Coin: {
            entity->sprite = &game->texture_coin;
            entity->frames = 8;
            entity->frame_duration = 0.1f;
            entity->size = vec2(1.0f, 1.0f);
            entity->offset = {};
            entity->is_trigger = true;
        } break;
        
        case Spikes: {
            entity->sprite = &game->texture_spikes;
            entity->size = vec2(1.0f, 1.0f);
            entity->is_solid = true;
            entity->invert_gravity = entity->direction.y < 0.0f;
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
            entity->layer = Layer_Background;
            entity->sprite = &game->texture_space;
            entity->size = vec2(1.0f, 1.0f);
            entity->offset.y += entity->type == Gravity_Inverted ? -0.2f : 0.2f;
            entity->is_trigger = true;
            entity->is_solid = false;
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
get_floor_tiles(Game_State* game, Entity* entity, f32 range=0.5f) {
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
        
        if (object->name.data) {
            entity->tag = &object->name;
            pln("TAG: %.*s", (int) entity->tag->count, entity->tag->data);
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
                add_trigger(game, object->p, object->size, object->name);
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
    
    // Invert gravity
    if (controller->action_pressed && player->is_grounded && game->ability_unlock_gravity) {
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
    if (fabsf(controller->dir.x) > 0.3f) {
        player->direction.x = controller->dir.x;
        player->acceleration.x = controller->dir.x * 20.0f;
        if (game->is_moon_gravity) {
            player->acceleration.x = controller->dir.x * 2.0f;
        }
    } else {
        player->acceleration.x = 0.0f;
    }
    
    // Run physics
    update_rigidbody(game, player);
    
    // Check collisions with entites
    if (player->collided_with) {
        Entity* other = player->collided_with;
        switch (other->type) {
            case Spikes:
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
                PlaySound(game->snd_pickup_moon);
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
    
    // Check triggers
    if (player->is_grounded && player->health > 0) {
        for_array(game->triggers, trigger, trigger_index) {
            if (trigger_index >= game->trigger_count) break;
            bool overlap = box_check(player->collider, trigger->collider);
            if (string_equals(trigger->tag, string_lit("tutorial_walk"))) {
                update_tutorial(game, overlap, Tutorial_Walk);
            }
            
            if (string_equals(trigger->tag, string_lit("tutorial_jump"))) {
                update_tutorial(game, overlap, Tutorial_Jump);
            }
            
            if (string_equals(trigger->tag, string_lit("tutorial_longjump"))) {
                update_tutorial(game, overlap, Tutorial_Long_Jump);
            }
            
            if (string_equals(trigger->tag, string_lit("tutorial_gravity"))) {
                update_tutorial(game, game->ability_unlock_gravity && overlap, Tutorial_Switch_Gravity);
            }
            
            
            if (overlap && string_equals(trigger->tag, string_lit("ability"))) {
                if (!game->ability_unlock_gravity) {
                    set_game_mode(game, GameMode_Cutscene_Ability);
                }
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
render_level(Game_State* game, bool skip_enemies=false, bool skip_tilemap=false) {
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        
        if (skip_tilemap && (entity->type == Gravity_Normal ||
                             entity->type == Gravity_Inverted)) {
            continue;
        }
        
        draw_entity(game, entity, Layer_Background);
    }
    
    if (!skip_tilemap) {
        draw_tilemap(game);
    }
    
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        
        if (skip_enemies && (entity->type == Enemy_Plum ||
                             entity->type == Enemy_Plum_Dead ||
                             entity->type == Spikes ||
                             entity->type == Coin)) {
            continue;
        }
        
        draw_entity(game, entity, Layer_Entity);
    }
}

inline void
center_camera_on_entity(Game_State* game, Entity* entity) {
    game->camera_p.x = entity->p.x - game->game_width/2.0f;
    game->camera_p.y = entity->p.y - game->game_height/2.0f;
}

inline f32
norm_t(Game_State* game, f32 start_t, f32 end_t) {
    return (game->mode_timer - start_t)/(end_t - start_t);
}

inline void
center_camera_zoom(Game_State* game, f32 start_zoom, f32 end_zoom, f32 start_t, f32 end_t, f32 (*easing_func)(f32)) {
    f32 t = norm_t(game, start_t, end_t);
    f32 zoom = start_zoom + (end_zoom - start_zoom)*easing_func(t);
    f32 offset = (zoom - 1.0f)/2.0f;
    game->final_render_offset = vec2(offset, offset);
    game->final_render_zoom = zoom;
    
}

inline v2
get_camera_to_target_p(Game_State* game) {
    v2 result = {};
    result.x = game->camera_p.x + game->game_width/2.0f;
    result.y = game->camera_p.x + game->game_height/2.0f;
    return result;
}

inline void
center_camera_on_v2(Game_State* game, v2 start_p, v2 end_p, f32 start_t, f32 end_t, f32 (*easing_func)(f32)) {
    f32 t = (game->mode_timer - start_t)/(end_t - start_t);
    v2 diff_p = end_p - start_p;
    
    game->camera_p.x = start_p.x + diff_p.x*easing_func(t) - game->game_width/2.0f;
    //game->camera_p.y = start_p.y + diff_p.y*easing_func(t) - game->game_height/2.0f;
}

void
camera_lock_y_to_zero(Game_State* game) {
    game->camera_p.y = 0;
}

void
camera_follow_entity_x(Game_State* game, Entity* entity, f32 camera_max_offset=2) {
    f32 camera_offset = game->camera_p.x - (entity->p.x - game->game_width/2.0f);
    if (camera_offset < -camera_max_offset) {
        game->camera_p.x = entity->p.x - game->game_width/2.0f - camera_max_offset;
    }
    if (camera_offset > camera_max_offset) {
        game->camera_p.x = entity->p.x - game->game_width/2.0f + camera_max_offset;
    }
    
    if (game->camera_p.x < 0) {
        game->camera_p.x = 0;
    }
}

void
camera_follow_entity_y(Game_State* game, Entity* entity, f32 camera_max_offset=2) {
    f32 camera_offset = game->camera_p.y - (entity->p.y - game->game_width/2.0f);
    if (camera_offset < -camera_max_offset) {
        game->camera_p.y = entity->p.y - game->game_width/2.0f - camera_max_offset;
    }
    if (camera_offset > camera_max_offset) {
        game->camera_p.y = entity->p.y - game->game_width/2.0f + camera_max_offset;
    }
    
    if (game->camera_p.y < 0) {
        game->camera_p.y = 0;
    }
}

void
update_and_render_level(Game_State* game) {
    Game_Controller controller = get_controller(game);
    
    Box sim_window;
    sim_window.p = game->camera_p - vec2(2.0f, 2.0f);
    sim_window.size = vec2(game->game_width + 4.0f, game->game_height + 4.0f);
    
    // Update game
    for (int entity_index = 0; entity_index < game->entity_count; entity_index++) {
        Entity* entity = &game->entities[entity_index];
        
        switch (entity->type) {
            case Player: {
                update_player(game, entity, &controller);
            } break;
            
            case Enemy_Plum: {
                if (box_check(sim_window, entity->collider)) {
                    update_enemy_plum(game, entity);
                }
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
            entity->frame_advance += GetFrameTime() * (1.0f / entity->frame_duration);
            if (entity->frame_advance >= entity->frames) {
                entity->frame_advance -= entity->frames;
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
        set_game_mode(game, GameMode_Death_Screen);
        PlaySound(game->snd_death);
    }
    
    camera_follow_entity_x(game, game->player);
    camera_lock_y_to_zero(game);
    
    render_level(game);
}

void
update_and_render_death_screen(Game_State* game) {
    // Play death animation and restore gameplay
    restore_game_state(game);
    render_level(game);
    
    set_game_mode(game, GameMode_Level);
}

Color colors[] = {
    WHITE, 
    { 255, 82,  119, 255 },
    { 255, 174, 112, 255 }, 
    { 146, 232, 192, 255 },
    { 255, 238, 131, 255 }
};

void
update_and_render_cutscene_ability(Game_State* game) {
    Entity* player = game->player;
    const v2 target = vec2(176, 14);
    game->ps_gravity->start_min_p = target;
    game->ps_gravity->start_max_p = target + vec2(1, 1);
    game->ps_gravity->min_angle = -PI_F32 + 0.05f;
    game->ps_gravity->max_angle = 0.05f;
    game->ps_gravity->min_speed = 0.05f; game->ps_gravity->max_speed = 0.6f;
    game->ps_gravity->spawn_rate = 0.05f;
    game->ps_gravity->delta_t = 0.005f;
    
    if (game->mode_timer < 1.5f) {
        game->ability_block = 0;
        
        // Setup
        player->acceleration.x = 0.0f;
        player->acceleration.y = 0.0f;
        game->start_p = get_camera_to_target_p(game);
        
    } else if (game->mode_timer < 3.0f) {
        start_music_crossfade(game, {}, 2.0f);
        // Walk to the target
        center_camera_on_v2(game, game->start_p, target + vec2(0.5f, 0.5f), 1.5f, 3.0f, cubic_ease_in_out);
        
        f32 player_target = target.x - 3.0f;
        player->acceleration.y = 9.0f;
        if (player->p.x < player_target) {
            player->acceleration.x = 10.0f;
        } else {
            player->acceleration.x = 0.0f;
        }
        
    } else if (game->mode_timer < 4.5f) {
        // Zoom in
        center_camera_zoom(game, 1.0f, 2.0f, 3.0f, 4.5f, cubic_ease_in_out);
        
    } else if (game->mode_timer < 6.0f) {
        v2s bottom = to_pixel(game, target);
        
        static bool played_explosion = false;
        if (!played_explosion && game->mode_timer >= 5.4f) {
            PlaySound(game->snd_explosion);
            played_explosion = true;
        }
        
        int height = (int) ((game->mode_timer - 5.5f)*6.0f*bottom.y);
        height = clamp(height, 0, bottom.y);
        int expand = (int) ((game->mode_timer - 5.7f)*64.0f);
        expand = clamp(expand, 1, 8);
        DrawRectangle(bottom.x + 8 - expand, 0, expand*2, height, WHITE);
    } else if (game->mode_timer < 8.0f) {
        if (game->mode_timer > 7.0f) {
            start_music_crossfade(game, game->music_gravity_unlock, 3.0f);
        }
        if (!game->ability_block) {
            // TODO(Alexander): this is really ugly hack to find the target entity, tagging didn't work
            for_array(game->entities, it, _) {
                if (it->p.x == target.x && it->p.y == target.y) {
                    if (it->type == Gravity_Inverted) {
                        *it = {};
                        game->tile_map[(int) target.y*game->tile_map_width + (int) target.x] = 0;
                        game->ability_block = it;
                        particle_burst(game->ps_gravity, 200, 1.0f);
                        
                        for_particle(game->ps_gravity, pa, ci) {
                            pa->v.y += -0.05f;
                        }
                    }
                }
            }
        }
        
        update_particle_system(game->ps_gravity, false);
    } else {
        
        v2 game_box = vec2(game->game_width/2.0f, game->game_height/2.0f); 
        game->ps_gravity->min_angle = -PI_F32/2.0f;
        game->ps_gravity->max_angle = -PI_F32/2.0f;
        game->ps_gravity->start_min_p = target - game_box;
        game->ps_gravity->start_max_p = target + game_box;
        game->ps_gravity->max_speed = 0.05f;
        game->ps_gravity->delta_t = 0.01f;
        update_particle_system(game->ps_gravity, true);
        
        if (game->mode_timer < 11.0f) {
            center_camera_zoom(game, 2.0f, 1.0f, 8.0f, 11.0f, cubic_ease_in_out);
            
            //} else if (game->mode_timer < 11.0f) {
            center_camera_on_v2(game, target + vec2(0.5f, 0.5f), player->p, 8.0f, 11.0f, cubic_ease_in_out);
        }
        
        if (game->mode_timer > 11.0f) {
            Game_Controller controller = get_controller(game);
            game->ability_unlock_gravity = true;
            update_tutorial(game, true, Tutorial_Switch_Gravity);
            
            if (controller.action_pressed) {
                start_music_crossfade(game, {}, 1.5f);
                set_game_mode(game, GameMode_Level);
                player->invert_gravity = false;
                player->velocity.y = 6.0f;
                return;
            }
        }
        
        player->invert_gravity = true;
        if (player->p.y > 7) {
            player->acceleration.y = -2;
        } else {
            player->acceleration.y = 2;
        }
        
        
        if (fabsf(player->velocity.y) > 4.0f) {
            player->velocity.y = sign(player->velocity.y)*4.0f;
        }
    }
    
    update_rigidbody(game, player);
    
    render_level(game, game->mode_timer >= 8.0f, game->mode_timer >= 8.0f);
    
    for_particle(game->ps_gravity, pa, ci) {
        Color c = colors[ci % fixed_array_count(colors)];
        c.a = (u8) (pa->t * 255);
        
        v2s p = to_pixel(game, pa->p);
        v2s p0 = to_pixel(game, pa->p - pa->v);
        pa->v *= 0.96f;
        DrawLine(p0.x, p0.y, p.x, p.y, c);
        DrawPixel(p.x, p.y, c);
    }
}


void
game_update_and_render(Game_State* game, RenderTexture2D render_target) {
    
    // Render game
    BeginTextureMode(render_target);
    ClearBackground(BACKGROUND_COLOR);
    
    switch (game->mode) {
        case GameMode_Level: {
            update_and_render_level(game);
        } break;
        
        case GameMode_Death_Screen: {
            update_and_render_death_screen(game);
        } break;
        
        case GameMode_Cutscene_Ability: {
            update_and_render_cutscene_ability(game);
        } break;
    }
    
    game->mode_timer += GetFrameTime();
    
    EndTextureMode();
}

#define DEF_LEVEL1 \
LVL("level1") \
LVL("level1_2") \
LVL("level1_3") \
LVL("level1_4") \
LVL("level1_5")

cstring level_assets[] = {
#define LVL(filename) "assets/" filename ".tmx", 
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
    game.ps_gravity = init_particle_system(200);
    
    game.normal_gravity = 20;
    game.fall_gravity = 50;
    
    set_game_mode(&game, GameMode_Level);
    
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(game.screen_width, game.screen_height, "Bigmode Game Jam 2023");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    
    MaximizeWindow();
    ToggleFullscreen();
    
    InitAudioDevice();
    
    //int monitor = GetCurrentMonitor();
    //int target_fps = GetMonitorRefreshRate(monitor);
    //SetTargetFPS(60);
    SetExitKey(0);
    
    Memory_Arena level_arena = {};
    game.meters_to_pixels = TILE_SIZE;
    game.pixels_to_meters = 1.0f/game.meters_to_pixels;
#define TEX2D(name, filename) game.texture_##name = LoadTexture("assets/" filename);
    DEF_TEXUTRE2D
#undef TEX2D
    
#define SND(name, filename) game.snd_##name = LoadSound("assets/" filename);
    DEF_SOUND
#undef SND
    
#define MUSIC(name, filename) game.music_##name = LoadMusicStream("assets/" filename);
    DEF_MUSIC
#undef MUSIC
    
    game.font_default = LoadFontEx("assets/Retro Gaming.ttf", 14*4, 0, 0);
    SetTextureFilter(game.font_default.texture, TEXTURE_FILTER_POINT);
    
    RenderTexture2D render_target = LoadRenderTexture(game.render_width, game.render_height);
    SetTextureFilter(render_target.texture, TEXTURE_FILTER_POINT);
    
    cstring filename = level_assets[0];
    game_setup_level(&game, &level_arena, string_lit(filename));
    
    start_music(&game, game.music_level1_intro);
    
    while (!WindowShouldClose()) {
        game.global_timer += GetFrameTime();
        
        // Update music
        if (game.cross_fade_duration > 0.0f) {
            f32 fade_t = game.global_timer - game.cross_fade_timer;
            f32 fade_volume = clamp(fade_t * (1.0f/game.cross_fade_duration), 0.0f, 1.0f);
            SetMusicVolume(game.music_playing, fade_volume);
            SetMusicVolume(game.music_fade_out, 1.0f - fade_volume);
            
            if (fade_volume < 1.0f) {
                UpdateMusicStream(game.music_fade_out);
            } else {
                StopMusicStream(game.music_fade_out);
            }
        }
        UpdateMusicStream(game.music_playing);
        
        // Fullscreen
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
    
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
    }
    
    CloseWindow();
    return 0;
}