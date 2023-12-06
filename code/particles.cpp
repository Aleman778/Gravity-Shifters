
struct Particle {
    v2 p;
    v2 v;
    f32 t;
};

struct Particle_System {
    Particle* particles;
    int max_particle_count;
    int particle_count;
    
    v2 start_p;
    
    f32 speed;
    
    f32 min_angle;
    f32 max_angle;
    
    f32 min_scale;
    f32 max_scale;
    
    f32 spawn_rate;
    f32 delta_t;
};
