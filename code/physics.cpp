
bool
box_collision(Entity* rigidbody, Entity* other, v2* step_velocity, bool resolve) {
    bool found = false;
    
    v2 step_position = rigidbody->p + *step_velocity;
    if (step_position.y + rigidbody->size.y - 0.01f > other->p.y && 
        step_position.y < other->p.y + other->size.y) {
        
        if (step_velocity->x < 0.0f && rigidbody->p.x >= other->p.x + other->size.x) {
            f32 x_overlap = other->p.x + other->size.x - step_position.x;
            if (x_overlap > 0.0f) {
                if (resolve) {
                    step_velocity->x = other->p.x + other->size.x - rigidbody->p.x;
                    rigidbody->velocity.x = 0.0f;
                }
                found = true;
            }
        } else if (step_velocity->x > 0.0f && rigidbody->p.x + rigidbody->size.x <= other->p.x) {
            f32 x_overlap = step_position.x - other->p.x + rigidbody->size.x;
            if (x_overlap > 0.0f) {
                if (resolve) {
                    step_velocity->x = other->p.x - rigidbody->size.x - rigidbody->p.x;
                    rigidbody->velocity.x = 0.0f;
                }
                found = true;
            }
        }
    }
    
    if (rigidbody->p.x + rigidbody->size.x > other->p.x && 
        rigidbody->p.x < other->p.x + other->size.x) {
        if (step_velocity->y < 0.0f && rigidbody->p.y < other->p.y + other->size.y &&
            rigidbody->p.y + rigidbody->size.y > other->p.y) {
            f32 y_overlap = step_position.y - other->p.y + other->size.y;
            if (y_overlap > 0.0f) {
                if (resolve) {
                    step_velocity->y = other->p.y + other->size.y - rigidbody->p.y;
                    rigidbody->velocity.y = 0.0f;
                }
                found = true;
            }
        } else if (step_velocity->y > 0.0f && rigidbody->p.y + rigidbody->size.y <= other->p.y) {
            f32 y_overlap = step_position.y - other->p.y + rigidbody->size.y;
            if (y_overlap > 0.0f) {
                if (resolve) {
                    step_velocity->y = other->p.y - rigidbody->size.y - rigidbody->p.y;
                    rigidbody->velocity.y = 0.0f;
                    rigidbody->is_grounded = true;
                }
                found = true;
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
    
    Entity collider = {};
    collider.type = Box_Collider;
    collider.size = vec2(1, 1);
    for (int y = 0; y < game->tile_map_height; y++) {
        for (int x = 0; x < game->tile_map_width; x++) {
            if (x >= screen_p.x && x <= screen_p.x + game->game_width) {
                u8 tile = game->tile_map[y*game->tile_map_width + x];
                if (tile == 0) continue;
                
                collider.p = vec2((f32) x, (f32) y);
                
                bool collided = box_collision(entity, &collider, step_velocity, true);
                if (collided) 
                    entity->collided = true;
                result = true;
            }
        }
    }
    
    return result;
}


bool
check_collisions(Game_State* game, Entity* entity, v2* step_velocity) {
    bool result = false;
    entity->collided = false;
    entity->collided_with = 0;
    
    for (int j = 0; j < game->entity_count; j++) {
        Entity* other = &game->entities[j];
        if ((other != entity && other->is_rigidbody) || 
            other->type == Box_Collider) {
            
            bool collided = box_collision(entity, other, step_velocity, !other->is_rigidbody);
            if (collided) {
                entity->collided = true;
                entity->collided_with = other;
                result = true;
            }
        }
    }
    
    // Collision with the tiles
    bool tilemap_result = check_tilemap_collision(game, entity, step_velocity);
    result = result | tilemap_result;
    
    return result;
}


void
update_rigidbody(Game_State* game, Entity* entity) {
    if (!entity->is_rigidbody) return;
    
    f32 delta_time = GetFrameTime();
    
    // Rigidbody physics
    v2 step_velocity = entity->velocity * delta_time + entity->acceleration * delta_time * delta_time * 0.5f;
    entity->is_grounded = false;
    check_collisions(game, entity, &step_velocity);
    
    entity->p += step_velocity;
    entity->velocity += entity->acceleration * delta_time;
    
    if (entity->num_frames > 0) {
        entity->frame_advance += step_velocity.x * entity->frame_advance_rate;
        if (fabsf(step_velocity.x) <= 0.01f) {
            entity->frame_advance = 0.0f;
        }
        
        if (entity->frame_advance > entity->num_frames) {
            entity->frame_advance -= entity->num_frames;
        }
        
        if (entity->frame_advance < 0.0f) {
            entity->frame_advance += entity->num_frames;
        }
    }
    
    if (fabsf(entity->velocity.x) > entity->max_speed.x) {
        entity->velocity.x = sign(entity->velocity.x) * entity->max_speed.x;
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