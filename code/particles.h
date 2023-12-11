
struct Particle {
    v2 p;
    v2 v;
    f32 t;
};

struct Particle_System {
    Particle* particles;
    int max_particle_count;
    int particle_count;
    
    v2 start_min_p;
    v2 start_max_p;
    
    f32 min_speed;
    f32 max_speed;
    
    f32 min_angle;
    f32 max_angle;
    
    f32 min_scale;
    f32 max_scale;
    
    f32 spawn_rate;
    f32 delta_t;
};

#define for_particle(ps, it, it_index) \
int it_index = 0; \
for (Particle* it = ps->particles; \
it_index < ps->particle_count; \
it_index++, it++)

void reset_particle_system(Particle_System* ps);