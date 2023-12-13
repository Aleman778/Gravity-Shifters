
void
draw_tilemap(Game_State* game) {
    Vector2 origin = {};
    
    v2s screen_p;
    screen_p.x = (s32) floor(game->camera_p.x);
    screen_p.y = (s32) floor(game->camera_p.y);
    
    int tile_xcount = (int) (game->texture_tiles.width/game->meters_to_pixels);
    assert(tile_xcount);
    for (int y = 0; y < game->tile_map_height; y++) {
        for (int x = 0; x < game->tile_map_width; x++) {
            
            if (x >= screen_p.x && x <= screen_p.x + game->game_width) {
                
                u8 tile = game->tile_map[y*game->tile_map_width + x];
                if (tile == 0) continue;
                tile--;
                
                Rectangle src = { 0, 0, game->meters_to_pixels, game->meters_to_pixels };
                src.x = (tile % tile_xcount) * game->meters_to_pixels;
                src.y = (tile / tile_xcount) * game->meters_to_pixels;
                
                Rectangle dest = { 0, 0, game->meters_to_pixels, game->meters_to_pixels };
                dest.x = floorf((x - game->camera_p.x) * game->meters_to_pixels);
                dest.y = floorf((y - game->camera_p.y) * game->meters_to_pixels);
                DrawTexturePro(game->texture_tiles, src, dest, origin, 0.0f, WHITE);
            }
        }
    }
}

int
get_frame_index(Entity* entity) {
    int frame = (int) entity->frame_advance;
    return frame;
}

void
draw_sprite(Game_State* game, Texture2D* sprite, v2 p, v2 sprite_offset, v2 size, v2 dir, int frame=0) {
    p = to_pixel_v2(game, p);
    sprite_offset = to_pixel_size_v2(game, sprite_offset);
    size = to_pixel_size_v2(game, size);
    
    Rectangle src = { sprite_offset.x, sprite_offset.y, size.width, size.height };
    Rectangle dest = { p.x, p.y, size.width, size.height };
    
    if (frame > 0) {
        src.x += size.width*frame;
    }
    
    if (dir.x < 0) {
        src.width = -src.width;
    }
    if (dir.y < 0) {
        src.height = -src.height;
    }
    
    Vector2 origin = {};
    DrawTexturePro(*sprite, src, dest, origin, 0, WHITE);
}

#define VINE_COLOR CLITERAL(Color){ 47, 87, 83, 255 }

void
draw_entity(Game_State* game, Entity* entity, Entity_Layer layer) {
    if (entity->type == None) return;
    if (entity->layer != layer) return;
    
    int frame = get_frame_index(entity);
    v2 dir = entity->direction;
    if (dir.y > 0 && entity->invert_gravity) {
        dir.y = -1;
    }
    if (dir.y < 0 && !entity->invert_gravity) {
        dir.y = 1;
    }
    
    switch (entity->type) {
        case Vine: {
            if (entity->size.x > 1.0f) { 
                f32 y_offset = entity->direction.y < 0 ? 0.0f : 1.0f;
                v2s s = to_pixel(game, entity->p + vec2(entity->offset.x, y_offset));
                v2s e = to_pixel(game, entity->p + vec2(entity->size.x + entity->offset.x, y_offset));
                DrawLine(s.x, s.y, e.x, e.y, VINE_COLOR);
                DrawLine(s.x, s.y + 1, e.x, e.y + 1, VINE_COLOR);
            } else {
                draw_sprite(game, entity->sprite, entity->p + vec2(0.0f, 0.0f), {}, vec2(1.0f, 1.0f), entity->direction);
            }
        } break;
        
        case Gravity_Inverted:
        case Gravity_Normal: {
            static v2 scroll_space = {};
            scroll_space += 0.002f;
            v2 sprite_offset = scroll_space + game->camera_p.x*0.2f;
            draw_sprite(game, entity->sprite, entity->p, sprite_offset, entity->size, {}, 0);
        } break;
        
        case Player: {
            v2 sprite_offset = {};
            sprite_offset.x = 0.0f;
            if (dir.y > 0) {
                sprite_offset.y = 0.6f;//25f;
                draw_sprite(game, &game->texture_character, entity->p - sprite_offset, {}, vec2(1.0f, 2.0f), dir, frame);
            } else {
                sprite_offset.y = 0.15f;//25f;
                draw_sprite(game, &game->texture_character_inv, entity->p - sprite_offset, {}, vec2(1.0f, 2.7f), dir, frame);
            }
        } break;
        
        default: {
            if (entity->sprite) {
                draw_sprite(game, entity->sprite, entity->p, {}, entity->size, dir, frame);
            } else {
                v2s p = to_pixel(game, entity->p);
                v2s size = to_pixel_size(game, entity->size);
                DrawRectangle(p.x, p.y, size.width, size.height, RED);
            }
        }
    }
    
}

void
game_draw_ui(Game_State* game, f32 width, f32 height, f32 scale) {
    
    if (game->mode == GameMode_Level) {
        cstring coins = TextFormat("%d", game->coins);
        Vector2 p = { 8*scale, 8*scale };
        DrawTextureEx(game->texture_ui_coin, p, 0, scale, WHITE);
        p.x += (game->texture_ui_coin.width + 1.0f) * scale;
        p.y += scale;
        DrawTextEx(game->font_default, coins, p, 14*scale, 0, WHITE);
    }
    
    if (game->curr_tutorials) {
        cstring tutorial = "";
        Texture2D tex = {};
        if (is_tutorial_active(game, Tutorial_Walk)) {
            tutorial = "Walk";
            tex = game->use_gamepad ? game->texture_ui_walk_gamepad : game->texture_ui_walk_keyboard;
        } else if (is_tutorial_active(game, Tutorial_Jump)) {
            tutorial = "Jump";
            tex = game->use_gamepad ? game->texture_ui_jump_gamepad : game->texture_ui_jump_keyboard;
        } else if (is_tutorial_active(game, Tutorial_Long_Jump)) {
            tutorial = "Jump (hold for longer jump)";
            tex = game->use_gamepad ? game->texture_ui_long_jump_gamepad : game->texture_ui_long_jump_keyboard;
        }
        
        Vector2 p = { width/2.0f - 64.0f, height - 80.0f };
        DrawTextureEx(tex, p, 0, scale, WHITE);
        p.x += 34.0f*scale;
        if (game->use_gamepad) {
            p.y += 2.0f*scale;
        }
        DrawTextEx(game->font_default, tutorial, p, 14*scale, 0, WHITE);
    }
}

void
draw_texture_to_screen(Game_State* game, RenderTexture2D render_target) {
    Vector2 origin =  {};
    
    int height = GetScreenHeight();
    if (IsWindowFullscreen()) {
        int monitor = GetCurrentMonitor();
        height = GetMonitorHeight(monitor);
        
    }
    
    // Render to screen
    BeginDrawing();
    ClearBackground(BLACK);
    
    int scale = height / game->render_height;
    //pln("scale = %d", scale);
    Texture render_texture = render_target.texture;
    Rectangle src = {
        0, 0,
        (f32) render_texture.width / game->final_render_zoom, 
        (f32) (-render_texture.height / game->final_render_zoom)
    };
    src.x = src.width*game->final_render_offset.x;
    src.y = -src.height*game->final_render_offset.y;
    
    Rectangle dest = { 0, 0, 0, (f32) game->render_height*scale };
    
    f32 aspect_ratio = (f32) game->game_width / (f32) game->game_height;
    dest.width = aspect_ratio*dest.height;
    //dest.x = (width - dest.width)/2.0f;
    //dest.y = (height - dest.height)/2.0f;
    //pln("dest w=%f, h=%f", dest.width, dest.height);
    
    DrawTexturePro(render_texture, src, dest, origin, game->final_render_rot, WHITE);
    
    game_draw_ui(game, dest.width, dest.height, (f32) scale);
    
#if BUILD_DEBUG
    DrawFPS(8, game->screen_height - 24);
#endif
    EndDrawing();
}