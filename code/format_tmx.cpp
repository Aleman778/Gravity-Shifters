
// NOTE(Alexander): hardcoded and may be invalidated whenever tilesets is changed

struct Box {
    v2 min_p;
    v2 max_p;
};

struct Tmx_Object {
    s32 gid; 
    v2 p;
    v2 size;
};

struct Loaded_Tmx {
    Tmx_Object* objects;
    
    u8* tile_map;
    s32 tile_map_count;
    s32 tile_map_width;
    s32 tile_map_height;
    s32 tile_width;
    s32 tile_height;
    
    s32 object_count;
    
    s32 is_loaded;
};

struct Read_File_Result {
    void* contents;
    int contents_size;
};

bool
string_equals(string a, string b) {
    if (a.count != b.count) {
        return false;
    }
    
    for (smm index = 0; index < a.count; index++) {
        if (a.data[index] != b.data[index]) {
            return false;
        }
    }
    
    return true;
}


Read_File_Result
read_entire_file(cstring filename) {
    Read_File_Result result;
    result.contents = LoadFileData(filename, &result.contents_size);
    return result;
}

//Loaded_Tmx read_tmx_map_data(u8* scan, Memory_Arena* arena);
void read_tmx_tile_map(u8** scanner, Loaded_Tmx* result);
void read_tmx_colliders(u8** scanner, Memory_Arena* arena, Loaded_Tmx* result);
void read_tmx_objects(u8** scanner, Memory_Arena* arena, Loaded_Tmx* result);


// TODO(Alexander): we are currently storing resulting objects and colliders
// in contiguous array, what if the arena runs out of memory, how do we handle this?
// Either we set a hard constraint that a level can only be 10kB or if we decide to
// stream chunks instead we have to store the entites in a more sophisticated manner.

Loaded_Tmx
read_tmx_map_data(u8* scan, Memory_Arena* arena) {
    Loaded_Tmx result = {};
    
    // NOTE(Alexander): first loads general information about the map
    for (; *scan; scan++) {
        if (eat_string(&scan, "<map")) {
            for (; *scan; scan++) {
                if (eat_string(&scan, ">")) {
                    break;
                }
                if (eat_string(&scan, " width=\"")) {
                    result.tile_map_width = eat_integer(&scan);
                } else if (eat_string(&scan, " height=\"")) {
                    result.tile_map_height = eat_integer(&scan);
                } else if (eat_string(&scan, " tilewidth=\"")) {
                    result.tile_width = eat_integer(&scan);
                } else if (eat_string(&scan, " tileheight=\"")) {
                    result.tile_height = eat_integer(&scan);
                }
            }
            
            break;
        }
    }
    
    if (result.tile_map_width == 0 || result.tile_map_height == 0 || 
        result.tile_width == 0 || result.tile_height == 0) {
        pln("Tiled map is corrupt");// TODO(Alexander): logging system
        return result;
    }
    
    int tile_count = result.tile_map_width * result.tile_map_height;
    result.tile_map = push_array_of_structs(arena, tile_count, u8);
    result.tile_map_count = tile_count;
    //result.tile_map_count = tile_count;
    
    //result.tile_map.data = (Tile*) push_size(arena, tile_count*sizeof(Tile));
    
    // NOTE(Alexander): Load all the layers
    for (; *scan; scan++) {
        if (eat_string(&scan, "<layer")) {
            for (; *scan; scan++) {
                if (eat_string(&scan, "</layer>")) {
                    break;
                }
                if (eat_string(&scan, "<data encoding=\"csv\">")) {
                    read_tmx_tile_map(&scan, &result);
                }
            }
        }
        
        if (eat_string(&scan, "<objectgroup")) {
            for (; *scan; scan++) {
                if (eat_string(&scan, " name=\"")) {
                    string name = eat_until(&scan,'"');
                    
                    if (string_equals(name, string_lit("Entities"))) {
                        read_tmx_objects(&scan, arena, &result);
                    } else if (string_equals(name, string_lit("Collisions"))) {
                        read_tmx_colliders(&scan, arena, &result);
                    } else {
                        //pln("Invalid object group: %", name);
                        assert(0 && "invalid objectgroup found");
                    }
                    break;
                }
            }
        }
    }
    
    result.is_loaded = true;
    return result;
}

Loaded_Tmx
read_tmx_map_data(string filename,
                  Memory_Arena* arena) {
    
    
    cstring cfilename = string_to_cstring(filename);
    Read_File_Result file = read_entire_file(cfilename);
    cstring_free(cfilename);
    
    
    Loaded_Tmx result = read_tmx_map_data((u8*) file.contents, arena);
    //free_file_memory(file.contents);
    return result;
}

void
read_tmx_tile_map(u8** scanner, Loaded_Tmx* result) {
    s32 tile_index = 0;
    s32 chunk_width = 0;
    
    u8* scan = *scanner;
    for (; *scan; scan++) {
        if (eat_string(&scan, "</data>")) {
            break;
        }
        
        if (*scan == ',') {
            scan++;
            tile_index++;
            
            assert(tile_index < result->tile_map_count && "number of tiles exceeds its limit");
            
            if (chunk_width > 0) {
                if ((tile_index % chunk_width) == 0) {
                    tile_index += result->tile_map_width - chunk_width;
                }
            }
            
            result->tile_map[tile_index] = 0;
        }
        
        if (*scan == ' ' || *scan == '\n' || *scan == '\t' || *scan == '\r' || 
            eat_string(&scan, "</chunk>")) {
            continue;
        }
        
        if (eat_string(&scan, "<chunk")) {
            int chunk_x = 0;
            int chunk_y = 0;
            for (; *scan; scan++) {
                if (eat_string(&scan, ">")) {
                    assert(chunk_x >= 0 && chunk_y >= 0 && "chunks needs to first be normalized");
                    tile_index = chunk_y*result->tile_map_width + chunk_x;
                    break;
                }
                if (eat_string(&scan, " x=\"")) {
                    chunk_x = eat_integer(&scan);
                } else if (eat_string(&scan, " y=\"")) {
                    chunk_y = eat_integer(&scan);
                } else if (eat_string(&scan, " width=\"")) {
                    chunk_width = eat_integer(&scan);
                }
            }
            
            result->tile_map[tile_index] = 0;
            //pln("parsed chunk: x=%, y=%, width=%, tile_index = %", chunk_x, chunk_y, chunk_width, tile_index);
            continue;
        }
        
        int number = (int) (*scan - '0');
        if (number >= 0 && number <= 9) {
            u8 data = result->tile_map[tile_index];
            result->tile_map[tile_index] = (u8) (data*10 + number);
            //pln("number: %, tile: % = %", number, tile_index, (int) result->tile_map[tile_index]);
        }
    }
}


void
read_tmx_colliders(u8** scanner, Memory_Arena* arena, Loaded_Tmx* result) {
    for (u8* scan = *scanner; *scan; scan++) {
        if (eat_string(&scan, "</objectgroup>")) {
            break;
        }
        
        if (eat_string(&scan, "<object")) {
            Tmx_Object* collider = push_struct(arena, Tmx_Object);
            collider->gid = 40;
            result->object_count++;
            
            if (!result->objects) {
                result->objects = collider;
            }
            
            for (; *scan; scan++) {
                if (eat_string(&scan, "/>") || eat_string(&scan, "</object>")) {
                    break;
                }
                
                if (eat_string(&scan, " x=\"")) {
                    f32 x = (f32) eat_integer(&scan);
                    collider->p.x = x/(f32) result->tile_width;
                } else if (eat_string(&scan, " y=\"")) {
                    f32 y = (f32) eat_integer(&scan);
                    collider->p.y = y/(f32) result->tile_height;
                } else if (eat_string(&scan, " width=\"")) {
                    f32 width  = (f32) eat_integer(&scan);
                    collider->size.width = width/(f32) result->tile_width;
                } else if (eat_string(&scan, " height=\"")) {
                    f32 height = (f32) eat_integer(&scan);
                    collider->size.height = height/(f32) result->tile_height;
                }
                
#if 0 
                if (eat_string(&scan, "<polygon points=\"")) {
                    v2 origin = collider->aabb.min;
                    v2 p0;
                    p0.x = origin.x + (f32) eat_integer(&scan)/(f32) result->tile_width;
                    assert(*scan++ == ',');
                    p0.y = origin.y + (f32) eat_integer(&scan)/(f32) result->tile_height;
                    v2 first_p = p0;
                    if (*scan++ == '"') {
                        break;
                    }
                    
                    for (; *scan && *scan != '"'; scan++) {
                        if (*scan == ' ') continue;
                        
                        v2 p1;
                        p1.x = origin.x + (f32) eat_integer(&scan)/(f32) result->tile_width;
                        assert(*scan++ == ',');
                        p1.y = origin.y + (f32) eat_integer(&scan)/(f32) result->tile_height;
                        
                        collider->type = Collider_Type.Line;
                        collider->line.p = p0;
                        collider->line.r = p1 - p0;
                        p0 = p1;
                        
                        collider = push_struct(arena, Collider);
                        result->collider_count++;
                        
                        if (*scan == '"') {
                            scan++;
                            break;
                        }
                    }
                    
                    collider->type = Collider_Type.Line;
                    collider->line.p = p0;
                    collider->line.r = first_p - p0;
                }
#endif
            }
        }
    }
}

void
read_tmx_objects(u8** scanner, Memory_Arena* arena, Loaded_Tmx* result) {
    for (u8* scan = *scanner; *scan; scan++) {
        if (eat_string(&scan, "</objectgroup>")) {
            break;
        }
        
        if (eat_string(&scan, "<object")) {
            Tmx_Object* object = push_struct(arena, Tmx_Object);
            result->object_count++;
            
            if (!result->objects) {
                result->objects = object;
            }
            
            for (; *scan; scan++) {
                if (eat_string(&scan, "/>") || eat_string(&scan, "</object>")) {
                    break;
                }
                
                if (eat_string(&scan, "gid=\"")) {
                    object->gid = eat_integer(&scan);
                } else if (eat_string(&scan, " x=\"")) {
                    f32 x = (f32) eat_integer(&scan);
                    object->p.x = x/(f32) result->tile_width;
                } else if (eat_string(&scan, " y=\"")) {
                    f32 y = (f32) eat_integer(&scan);
                    object->p.y = y/(f32) result->tile_height - 1.0f;
                }
                else if (eat_string(&scan, " width=\"")) {
                    f32 width = (f32) eat_integer(&scan);
                    object->size.width = width/(f32) result->tile_width;
                } else if (eat_string(&scan, " height=\"")) {
                    f32 height = (f32) eat_integer(&scan);
                    object->size.height = height/(f32) result->tile_height; 
                }
            }
        }
    }
}