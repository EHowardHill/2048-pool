#include "bn_core.h"
#include "bn_log.h"
#include <bn_fixed.h>
#include <bn_vector.h>
#include <bn_random.h>
#include <bn_math.h>
#include <bn_keypad.h>
#include "bn_blending.h"

#include <bn_sprite_ptr.h>
#include "bn_sound_items.h"

#include "bn_sprite_items_a_van_car.h"
#include "bn_sprite_items_arrow.h"

#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_items_bg_space.h"

#include "bn_sprite_items_numbers.h"

using namespace bn;

#define elasticity 30.0

template <typename T>
int signum(T value) {
    return (T(0) < value) - (value < T(0));
}

int delay = 50;
int ball_id = 0;
bool canplay = true;

class Ball {
public:
    sprite_ptr sprite = sprite_items::a_van_car.create_sprite(0, 0);
    fixed_t<12> velocity_x = 0;
    fixed_t<12> velocity_y = 0;
    int id = 0;
    int stage = 0;

    void update() {
        if (sprite.x() > 100 && velocity_x > 0) {
            velocity_x = -velocity_x;
        } else if (sprite.x() < -100 && velocity_x < 0) {
            velocity_x = -velocity_x;
        }

        if (sprite.y() > 60 && velocity_y > 0) {
            velocity_y = -velocity_y;
        } else if (sprite.y() < -60 && velocity_y < 0) {
            velocity_y = -velocity_y;
        }

        const fixed_t<12> friction = 0.02;
        velocity_x = abs(velocity_x) > friction ? velocity_x - signum(velocity_x) * friction : 0;
        velocity_y = abs(velocity_y) > friction ? velocity_y - signum(velocity_y) * friction : 0;

        sprite.set_position(sprite.x() + velocity_x, sprite.y() + velocity_y);
    }
};

int handleCollision(Ball& ball1, Ball& ball2, fixed_t<12> distance) {

    //const fixed_t<12> distance = abs(ball1.sprite.x() - ball2.sprite.x()) + abs(ball1.sprite.y() - ball2.sprite.y());
    fixed_t<12> separation = 20 - distance;

    if (separation > 0) {
        if (ball1.stage == ball2.stage) {
            ball2.velocity_x += ball1.velocity_x;
            ball2.velocity_y += ball1.velocity_y;
            sound_items::sfx_blip.play(0.5);

            if (ball2.stage > 8) {
                return 0;
            }

            ball2.stage++;
            ball2.sprite = sprite_items::a_van_car.create_sprite(ball2.sprite.x(), ball2.sprite.y(), ball2.stage);
            ball2.sprite.set_scale(1.0 + (0.1 * ball2.stage));
            return 0;

        } else {

            // Recalculate relative velocity with new positions
            fixed_t<12> dx = ball2.sprite.x() - ball1.sprite.x();
            fixed_t<12> dy = ball2.sprite.y() - ball1.sprite.y();

            fixed_t<12> displacement = separation / 2;
            fixed_t<12> angle = atan2(dy.integer(), dx.integer());

            // Apply displacement to move balls apart
            ball1.sprite.set_position(ball1.sprite.x() - displacement * cos(angle), ball1.sprite.y() - displacement * sin(angle));
            ball2.sprite.set_position(ball2.sprite.x() + displacement * cos(angle), ball2.sprite.y() + displacement * sin(angle));

            fixed_t<12> m1 = 0.5; // Mass of ball 1
            fixed_t<12> m2 = 0.5; // Mass of ball 2

            // Calculate initial velocities along the line of collision
            fixed_t<12> u1 = ball1.velocity_x * cos(angle) + ball1.velocity_y * sin(angle);
            fixed_t<12> u2 = ball2.velocity_x * cos(angle) + ball2.velocity_y * sin(angle);

            // Calculate final velocities using 1D elastic collision formula with elasticity coefficient
            fixed_t<12> v1 = ((1.0 + elasticity) * u1 * (m1 - m2) + 2.0 * m2 * u2) / (m1 + m2);
            fixed_t<12> v2 = ((1.0 + elasticity) * u2 * (m2 - m1) + 2.0 * m1 * u1) / (m1 + m2);

            // Update velocities of balls
            ball1.velocity_x = v1 * cos(angle);
            ball1.velocity_y = v1 * sin(angle);

            ball2.velocity_x = v2 * cos(angle);
            ball2.velocity_y = v2 * sin(angle);

            if (canplay) {
                fixed_t<12> volume = ball1.velocity_x;
                if (ball1.velocity_y > volume) volume = ball1.velocity_y;
                if (ball2.velocity_x > volume) volume = ball2.velocity_x;
                if (ball2.velocity_y > volume) volume = ball2.velocity_y;

                volume = volume / 2;
                if (volume > 1) volume = 1;
                if (volume < 0.001) volume = 0;
                sound_items::sfx_poolball.play(volume);
                canplay = false;
            }

            return 1;
        }
    }

    return 2;
}

template <typename T, size_t Size>
void render_number(vector<sprite_ptr, Size> &score_vector, int x, int y, int value) {
    score_vector.clear();
    int offset = 0;
    int temp_score = value;
    while (temp_score > 0) {
        int digit = temp_score % 10;
        temp_score /= 10;
        offset += 10;
        auto score_sprite = sprite_items::numbers.create_sprite(x - offset, y, digit);
        score_vector.push_back(score_sprite);
    }
}

int main() {
    core::init();
    
    vector<Ball, 32> balls;
    vector<sprite_ptr, 3> count_vector;
    vector<sprite_ptr, 8> score_vector;
    random rnd;
    int new_stage = 0;
    fixed_t<12> alpha = 0;

    sprite_ptr casting = sprite_items::a_van_car.create_sprite(-100, 0);
    sprite_ptr arrow = sprite_items::arrow.create_sprite(-80, 0);
    regular_bg_ptr bg_space = bn::regular_bg_items::bg_space.create_bg(0, 0);
    casting.set_blending_enabled(true);

    int angle = 90;
    int max_stage = 1;
    int score = 10;

    render_number<sprite_ptr, 3>(count_vector, -70, -60, 32 - balls.size());
    render_number<sprite_ptr, 8>(score_vector, -70, 60, score);

    auto ball_icon = sprite_items::numbers.create_sprite(-100, -60, 10);

    while (true) {

        bg_space.set_x((bg_space.x() + 1).integer() % 256);

        alpha += 0.02;
        
        if (alpha > 2)
            alpha = 0;

        fixed_t<12> d_alpha = alpha;
        if (d_alpha > 1)
            d_alpha = 2 - d_alpha;

        if (keypad::up_held() && angle < 170) {
            angle = (angle + 2) % 360;
        } else if (keypad::down_held() && angle > 20) {
            angle = (angle - 2) % 360;
        }

        arrow.set_rotation_angle((180 + angle) % 360);
        arrow.set_x(-100 + (degrees_sin(angle) * 20));
        arrow.set_y((degrees_cos(angle) * 20));
            
        blending::set_transparency_alpha(d_alpha);

        if (keypad::a_pressed()) {
            canplay = true;
            sound_items::sfx_shoot.play(0.5);
            if (balls.size() < 32) {
                Ball new_ball;
                new_ball.id = balls.size();
                new_ball.sprite = sprite_items::a_van_car.create_sprite(-100, 0, new_stage);
                new_ball.stage = new_stage;
                new_ball.sprite.set_scale(1.0 + (0.1 * new_ball.stage));
                new_ball.velocity_x = degrees_cos((90 + angle) % 360) * 3;
                new_ball.velocity_y = degrees_sin((90 + angle) % 360) * 3;

                new_stage = rnd.get_int(max_stage);
                casting = sprite_items::a_van_car.create_sprite(-100, 0, new_stage);
                casting.set_blending_enabled(true);
                casting.set_scale(1.0 + (0.1 * new_stage));

                balls.push_back(new_ball);

                render_number<sprite_ptr, 3>(count_vector, -70, -60, 32 - balls.size());
            }
        }

        for (auto ballIter = balls.begin(); ballIter != balls.end(); ++ballIter) {
            auto& ball = *ballIter;
            ball.update();
            
            for (auto& other_ball : balls) {
                if (&ball != &other_ball) {
                    const fixed_t<12> distance = abs(ball.sprite.x() - other_ball.sprite.x()) +
                                                abs(ball.sprite.y() - other_ball.sprite.y());
                    const fixed_t<12> needed_distance = (ball.stage + other_ball.stage);
                    if (distance < 20 + needed_distance) {
                        if (handleCollision(ball, other_ball, distance - (needed_distance / 2)) == 0) {

                            score += (ball.stage + 1) ^ 2;
                            render_number<sprite_ptr, 8>(score_vector, -70, 60, score);
                            render_number<sprite_ptr, 3>(count_vector, -70, -60, 32 - balls.size());

                            // Handle other ball
                            if (ball.stage > max_stage) max_stage = ball.stage;
                            balls.erase(ballIter);
                            --ballIter;
                            break;
                        }
                    }
                }
            }
        }

        core::update();
    }
}