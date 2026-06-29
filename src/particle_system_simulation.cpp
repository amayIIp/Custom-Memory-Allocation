#include "arena_allocator.hpp"
#include "pool_allocator.hpp"
#include "allocator_utils.hpp"
#include <iostream>
#include <vector>
#include <utility>

struct Particle {
    float x, y;
    float vx, vy;
    float lifetime;
    int id;
};

struct CollisionEvent {
    int particle_id_1;
    int particle_id_2;
    float impact_x;
    float impact_y;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << "Particle System & Frame Simulation Use Case\n";
    std::cout << "========================================================\n\n";

    constexpr int MAX_PARTICLES = 1000;
    custom_alloc::PoolAllocator particle_pool(MAX_PARTICLES, sizeof(Particle), alignof(Particle), false);

    constexpr std::size_t TRANSIENT_ARENA_SIZE = 64 * 1024; // 64 KB
    custom_alloc::ArenaAllocator frame_arena(TRANSIENT_ARENA_SIZE);

    std::vector<Particle*> active_particles;
    active_particles.reserve(MAX_PARTICLES);

    std::uint32_t seed = 999;
    auto get_rand_float = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed % 10000) / 10000.0f;
    };

    for (int i = 0; i < 200; ++i) {
        Particle* p = custom_alloc::create<Particle>(particle_pool);
        if (p) {
            p->x = get_rand_float() * 100.0f;
            p->y = get_rand_float() * 100.0f;
            p->vx = (get_rand_float() - 0.5f) * 10.0f;
            p->vy = (get_rand_float() - 0.5f) * 10.0f;
            p->lifetime = get_rand_float() * 5.0f + 1.0f; // 1 to 6 seconds
            p->id = i;
            active_particles.push_back(p);
        }
    }

    std::cout << "Initially spawned " << active_particles.size() << " particles.\n";

    constexpr float dt = 0.016f;
    for (int frame = 1; frame <= 5; ++frame) {
        std::cout << "\n--- Frame " << frame << " ---\n";

        std::vector<Particle*> live_particles;
        int deaths = 0;
        for (Particle* p : active_particles) {
            p->x += p->vx * dt;
            p->y += p->vy * dt;
            p->lifetime -= dt;

            if (p->lifetime > 0.0f) {
                live_particles.push_back(p);
            } else {
                deaths++;
                custom_alloc::destroy(particle_pool, p);
            }
        }
        active_particles = std::move(live_particles);
        std::cout << "Particles updated. Live: " << active_particles.size() << " (Deaths: " << deaths << ")\n";

        int spawns = 0;
        while (active_particles.size() < 200) {
            Particle* p = custom_alloc::create<Particle>(particle_pool);
            if (p) {
                p->x = get_rand_float() * 100.0f;
                p->y = get_rand_float() * 100.0f;
                p->vx = (get_rand_float() - 0.5f) * 10.0f;
                p->vy = (get_rand_float() - 0.5f) * 10.0f;
                p->lifetime = get_rand_float() * 5.0f + 1.0f;
                p->id = 1000 + spawns;
                active_particles.push_back(p);
                spawns++;
            } else {
                break;
            }
        }
        std::cout << "Spawned " << spawns << " new particles.\n";

        int simulated_collisions = 0;
        std::vector<CollisionEvent*> collisions_this_frame;

        for (std::size_t i = 0; i < active_particles.size(); ++i) {
            for (std::size_t j = i + 1; j < active_particles.size(); ++j) {
                Particle* p1 = active_particles[i];
                Particle* p2 = active_particles[j];
                float dx = p1->x - p2->x;
                float dy = p1->y - p2->y;
                float dist_sq = dx*dx + dy*dy;

                if (dist_sq < 2.0f) { // Collision threshold
                    // Allocate transient event in the frame arena!
                    CollisionEvent* ev = custom_alloc::create<CollisionEvent>(frame_arena);
                    if (ev) {
                        ev->particle_id_1 = p1->id;
                        ev->particle_id_2 = p2->id;
                        ev->impact_x = (p1->x + p2->x) / 2.0f;
                        ev->impact_y = (p1->y + p2->y) / 2.0f;
                        collisions_this_frame.push_back(ev);
                        simulated_collisions++;
                    }
                    if (simulated_collisions >= 5) break; // Limit logging printout per frame
                }
            }
            if (simulated_collisions >= 5) break;
        }

        std::cout << "Frame collisions detected & allocated in Arena: " << simulated_collisions << "\n";
        for (CollisionEvent* ev : collisions_this_frame) {
            std::cout << "  Collision: P" << ev->particle_id_1 << " & P" << ev->particle_id_2 
                      << " at (" << ev->impact_x << ", " << ev->impact_y << ")\n";
        }

        std::cout << "Arena memory used for frame transient events: " << frame_arena.used_bytes() << " bytes.\n";

        frame_arena.reset();
        std::cout << "Arena reset. Used bytes: " << frame_arena.used_bytes() << "\n";
    }

    for (Particle* p : active_particles) {
        custom_alloc::destroy(particle_pool, p);
    }

    std::cout << "\nSimulation shutdown. Pool free chunks: " << particle_pool.free_chunks() 
              << "/" << particle_pool.total_chunks() << "\n";
    std::cout << "========================================================\n";

    return 0;
}
