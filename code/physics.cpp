
inline bool
box_check(v2 a_min, v2 a_max, v2 b_min, v2 b_max) {
    return !(a_min.x > b_max.x ||
             a_max.x < b_min.x ||
             a_max.y < b_min.y ||
             a_min.y > b_max.y);
}

inline bool
box_check(Box a, Box b) {
    return box_check(a.p, a.p + a.size, b.p, b.p + b.size);
}

#define SKIN_WIDTH 0.0f

Collision
box_collision(Entity* rigidbody, Box other, v2* step_velocity, bool resolve, Collision mask) {
    Collision found = Col_None;
    
    v2 step_position = rigidbody->p + *step_velocity;
    if (step_position.y + rigidbody->size.y > other.p.y && 
        step_position.y < other.p.y + other.size.y) {
        
        if (mask & Col_Left && step_velocity->x < 0.0f && rigidbody->p.x >= other.p.x + other.size.x) {
            f32 x_overlap = other.p.x + other.size.x - step_position.x;
            if (x_overlap > 0.0f) {
                if (resolve) {
                    step_velocity->x = other.p.x + other.size.x - rigidbody->p.x;
                    rigidbody->velocity.x = 0.0f;
                }
                
                found = Col_Left;
            }
        } else if (mask & Col_Right && step_velocity->x > 0.0f && rigidbody->p.x + rigidbody->size.x <= other.p.x) {
            f32 x_overlap = step_position.x - other.p.x + rigidbody->size.x;
            if (x_overlap > 0.0f) {
                if (resolve) {
                    step_velocity->x = other.p.x - rigidbody->size.x - rigidbody->p.x;
                    rigidbody->velocity.x = 0.0f;
                }
                found = Col_Right;
            }
        }
    }
    
    if (rigidbody->p.x + rigidbody->size.x > other.p.x && 
        rigidbody->p.x < other.p.x + other.size.x) {
        if (mask & Col_Top && step_velocity->y < 0.0f && rigidbody->p.y <= other.p.y + other.size.y &&
            rigidbody->p.y + rigidbody->size.y > other.p.y) {
            f32 y_overlap = step_position.y - other.p.y + other.size.y;
            if (y_overlap > SKIN_WIDTH) {
                if (resolve) {
                    step_velocity->y = other.p.y - SKIN_WIDTH + other.size.y - rigidbody->p.y;
                    rigidbody->velocity.y = 0.0f;
                    if (rigidbody->invert_gravity) {
                        rigidbody->is_grounded = true;
                    }
                }
                found = Col_Top;
            }
        } else if (mask & Col_Bottom && step_velocity->y > 0.0f && rigidbody->p.y + rigidbody->size.y <= other.p.y) {
            f32 y_overlap = step_position.y - other.p.y + rigidbody->size.y;
            if (y_overlap > SKIN_WIDTH) {
                if (resolve) {
                    step_velocity->y = other.p.y + SKIN_WIDTH - rigidbody->size.y - rigidbody->p.y;
                    rigidbody->velocity.y = 0.0f;
                    if (!rigidbody->invert_gravity) {
                        rigidbody->is_grounded = true;
                    }
                }
                found = Col_Bottom;
            }
        }
    }
    
    return found;
}

bool
ray_box_collision(Ray ray, f32 min_dist, f32 max_dist, BoundingBox box) {
    RayCollision result = GetRayCollisionBox(ray, box);
    if (result.hit) {
        //pln("dist = %", result.distance);
        return result.distance >= min_dist && result.distance <= max_dist;
    }
    
    return false;
}

bool
check_tilemap_collision(Game_State* game, Entity* entity, v2* step_velocity) {
    bool result = false;
    
    // TODO(Alexander): this is not super effecient so we shouldn't rely on this
    v2s screen_p;
    screen_p.x = (s32) floor(game->camera_p.x);
    screen_p.y = (s32) floor(game->camera_p.y);
    
    Box collider = {};
    collider.size = vec2(1, 1);
    for (int y = 0; y < game->tile_map_height; y++) {
        for (int x = 0; x < game->tile_map_width; x++) {
            if (x >= screen_p.x && x <= screen_p.x + game->game_width) {
                u8 tile = game->tile_map[y*game->tile_map_width + x];
                if (tile == 0) continue;
                
                collider.p = vec2((f32) x, (f32) y);
                
                bool collided = box_collision(entity, collider, step_velocity, true, Col_All);
                if (collided) {
                    result = true;
                }
            }
        }
    }
    
    return result;
}

void
check_collisions(Game_State* game, Entity* entity, v2* step_velocity) {
    entity->is_grounded = false;
    entity->collided_with = 0;
    entity->collision = Col_None;
    entity->map_collision = Col_None;
    
    for_array(game->colliders, other, col_index) {
        if (col_index >= game->collider_count) break;
        
        Collision collision = box_collision(entity, *other, step_velocity, true, Col_All);
        if (collision) {
            entity->map_collision = entity->map_collision | collision;
        }
    }
    
    for_array(game->entities, other, entity_index) {
        if (entity_index >= game->entity_count) break;
        if (other->type == None) continue;
        
        if (other != entity && (other->is_rigidbody || other->is_solid || other->is_trigger)) {
            Box other_collider = other->collider;
            other_collider.p += other->offset;
            Collision mask = other->collision_mask;
            if (mask == Col_None) {
                mask = Col_All;
            }
            
            if (other->is_trigger) {
                if (box_check(entity->collider, other->collider)) {
                    entity->collided_with = other;
                    entity->collision = Col_All;
                }
            } else {
                Collision collision = box_collision(entity, other_collider, step_velocity, !other->is_rigidbody, mask);
                if (collision) {
                    entity->collided_with = other;
                    entity->collision = collision;
                }
            }
        }
    }
    
    // Collision with the tiles
    //check_tilemap_collision(game, entity, step_velocity);
}

void
update_rigidbody(Game_State* game, Entity* entity) {
    if (!entity->is_rigidbody) return;
    
    f32 delta_time = GetFrameTime();
    
    // Rigidbody physics
    v2 step_velocity = entity->velocity * delta_time + entity->acceleration * delta_time * delta_time * 0.5f;
    
    v2 velocity_before = entity->velocity;
    check_collisions(game, entity, &step_velocity);
    
    entity->p += step_velocity;
    entity->velocity += entity->acceleration * delta_time;
    
    // Play sound effect on player inpact with ground at max speed
    if (entity->type == Player && entity->prev_invert_gravity != entity->invert_gravity) {
        if (entity->is_grounded) {
            entity->prev_invert_gravity = entity->invert_gravity;
            PlaySound(game->snd_gravity_landing);
        }
    }
    
    
    if (entity->frames > 0) {
        entity->frame_advance += step_velocity.x * entity->frame_duration;
        if (fabsf(step_velocity.x) <= 0.01f) {
            entity->frame_advance = 0.0f;
        }
        
        if (entity->frame_advance > entity->frames) {
            entity->frame_advance -= entity->frames;
        }
        
        if (entity->frame_advance < 0.0f) {
            entity->frame_advance += entity->frames;
        }
    }
    
    if (fabsf(entity->velocity.x) > entity->max_speed.x) {
        entity->velocity.x = sign(entity->velocity.x) * entity->max_speed.x;
    }
    
    if (fabsf(entity->velocity.y) > entity->max_speed.y) {
        entity->velocity.y = sign(entity->velocity.y) * entity->max_speed.y;
    }
    
    if (!game->is_moon_gravity || entity->is_grounded) {
        if (fabsf(entity->acceleration.x) > epsilon32 && fabsf(entity->velocity.x) > epsilon32 &&
            sign(entity->acceleration.x) != sign(entity->velocity.x)) {
            entity->acceleration.x *= 2.0f;
        }
        
        if (fabsf(entity->acceleration.x) < epsilon32) {
            entity->velocity.x *= 0.8f;
        }
    }
}