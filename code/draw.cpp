
void
draw_tilemap(Game_State* game) {
    Vector2 origin = {};
    
    v2s screen_p;
    screen_p.x = (s32) floor(game->camera_p.x);
    screen_p.y = (s32) floor(game->camera_p.y);
    
    int tile_xcount = (int) (game->texture_tiles.width/game->meters_to_pixels);
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
draw_texture_to_screen(Game_State* game, RenderTexture2D render_target) {
    Vector2 origin =  {};
    
    // Render to screen
    BeginDrawing();
    ClearBackground(BACKGROUND_COLOR);
    
    Texture render_texture = render_target.texture;
    Rectangle src = { 0, 0, (f32) render_texture.width, (f32) (-render_texture.height) };
    Rectangle dest = { 0, 0, 0, (f32) game->screen_height };
    f32 aspect_ratio = (f32) render_texture.width / render_texture.height;
    dest.width = aspect_ratio*game->screen_height;
    dest.x = (game->screen_width - dest.width)/2.0f;
    DrawTexturePro(render_texture, src, dest, origin, 0, WHITE);
    
#if BUILD_DEBUG
    DrawFPS(8, game->screen_height - 24);
#endif
    EndDrawing();
}