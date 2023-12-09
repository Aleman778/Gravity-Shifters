
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
                dest.x = (x - game->camera_p.x) * game->meters_to_pixels;
                dest.y = (y - game->camera_p.y) * game->meters_to_pixels;
                DrawTexturePro(game->texture_tiles, src, dest, origin, 0.0f, WHITE);
            }
        }
    }
}

void
draw_entity(Game_State* game, Entity* entity) {
    if (entity->type == None) return;
    
    if (entity->sprite) {
        v2 p = to_pixel_v2(game, entity->p);
        v2 size = to_pixel_size_v2(game, entity->size);
        
        Rectangle src = { 0, 0, size.width, size.height };
        Rectangle dest = { p.x, p.y, size.width, size.height  };
        
        if (entity->frames > 1) {
            f32 anim_duration = entity->frame_duration * entity->frames;
            int frame = (int) (entity->frame_advance * anim_duration);
            src.x += size.width * frame;
            
            entity->frame_advance += GetFrameTime();
            if (entity->frame_advance > anim_duration) {
                entity->frame_advance -= anim_duration;
            }
        }
        
        if (entity->sprite_flipv) {
            src.height = -src.height;
        }
        
        Vector2 origin = {};
        DrawTexturePro(*entity->sprite, src, dest, origin, 0, WHITE);
    } else if (entity->type != Box_Collider) {
        v2s p = to_pixel(game, entity->p);
        v2s size = to_pixel_size(game, entity->size);
        
        DrawRectangle(p.x, p.y, size.width, size.height, RED);
    }
}

void
draw_texture_to_screen(Game_State* game, RenderTexture2D render_target) {
    Vector2 origin =  {};
    
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    
    // Render to screen
    BeginDrawing();
    ClearBackground(BACKGROUND_COLOR);
    
    int scale = height / game->render_height;
    //pln("scale = %d", scale);
    Texture render_texture = render_target.texture;
    Rectangle src = { 0, 0, (f32) render_texture.width, (f32) (-render_texture.height) };
    Rectangle dest = { 0, 0, 0, (f32) game->render_height*scale };
    
    f32 aspect_ratio = (f32) game->game_width / (f32) game->game_height;
    dest.width = aspect_ratio*dest.height;
    dest.x = (width - dest.width)/2.0f;
    dest.y = (height - dest.height)/2.0f;
    //pln("dest w=%f, h=%f", dest.width, dest.height);
    DrawTexturePro(render_texture, src, dest, origin, 0, WHITE);
    
    game_draw_ui(game, (f32) scale);
    
#if BUILD_DEBUG
    DrawFPS(8, game->screen_height - 24);
#endif
    EndDrawing();
}