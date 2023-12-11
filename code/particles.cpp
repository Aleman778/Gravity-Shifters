
Particle_System*
init_particle_system(int max_particle_count) {
    Particle_System* ps = (Particle_System*) calloc(1, sizeof(Particle_System));
    ps->particles = (Particle*) malloc(max_particle_count * sizeof(Particle));
    ps->max_particle_count = max_particle_count;
    return ps;
}

void
reset_particle_system(Particle_System* ps) {
    int max_particle_count = ps->max_particle_count;
    Particle* particles = ps->particles;
    *ps = {};
    ps->max_particle_count = max_particle_count;
    ps->particles = particles;
}

void
particle_burst(Particle_System* ps, int max_num_particles, f32 spawn_rate) {
    // Spawn new particles
    for (int i = 0; i < max_num_particles; i++) {
        if (ps->particle_count < ps->max_particle_count && random_f32() < spawn_rate) {
            Particle* p = &ps->particles[ps->particle_count];
            ps->particle_count++;
            v2 rand_box = ps->start_max_p - ps->start_min_p;
            rand_box.x *= random_f32();
            rand_box.y *= random_f32();
            p->p = ps->start_min_p + rand_box;
            
            f32 a = ps->min_angle + random_f32() * (ps->max_angle - ps->min_angle);
            f32 speed = ps->min_speed + random_f32() * (ps->max_speed - ps->min_speed);
            p->v.x = cosf(a)*speed;
            p->v.y = sinf(a)*speed;
            
            p->t = 1.0f;
        }
    }
}

void
update_particle_system(Particle_System* ps, bool spawn_new) {
    // Remove dead particles
    for (int i = 0; i < ps->particle_count; i++) {
        Particle* p = &ps->particles[i];
        if (p->t <= 0.0f) {
            if (ps->particle_count > 0) {
                Particle* last = &ps->particles[ps->particle_count - 1];
                *p = *last;
                i--;
            }
            ps->particle_count--;
        }
    }
    
    if (spawn_new) {
        particle_burst(ps, 10, ps->spawn_rate);
    }
    
    // Update live particles
    for (int i = 0; i < ps->particle_count; i++) {
        Particle* p = &ps->particles[i];
        p->p += p->v;
        p->t -= ps->delta_t;
        if (p->t < 0) {
            p->t = 0;
        }
    }
}