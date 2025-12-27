#include "render.h"
#include "audio.h"
#include "rlgl.h"
#include <math.h>

static Font default_font;

// Retro color palette
static const Color RETRO_MOUNTAIN_FILL = { 20, 40, 60, 255 };
static const Color RETRO_MOUNTAIN_WIRE = { 60, 120, 180, 255 };
static const Color RETRO_MOUNTAIN_SNOW = { 150, 180, 220, 255 };
static const Color RETRO_LAKE = { 20, 60, 100, 255 };
static const Color RETRO_LAKE_WIRE = { 40, 100, 160, 255 };
static const Color RETRO_STAR = { 200, 200, 255, 255 };
static const Color RETRO_GROUND = { 25, 45, 25, 255 };

void render_init(void)
{
    default_font = GetFontDefault();
}

// Draw a retro wireframe mountain (triangle)
static void draw_mountain(Vector3 base_center, float width, float height, float depth, bool snow_cap)
{
    float hw = width / 2.0f;
    float hd = depth / 2.0f;

    // Mountain vertices
    Vector3 peak = { base_center.x, base_center.y + height, base_center.z };
    Vector3 bl = { base_center.x - hw, base_center.y, base_center.z - hd };
    Vector3 br = { base_center.x + hw, base_center.y, base_center.z - hd };
    Vector3 fl = { base_center.x - hw, base_center.y, base_center.z + hd };
    Vector3 fr = { base_center.x + hw, base_center.y, base_center.z + hd };

    // Draw filled faces (darker)
    DrawTriangle3D(peak, bl, br, RETRO_MOUNTAIN_FILL);
    DrawTriangle3D(peak, br, fr, RETRO_MOUNTAIN_FILL);
    DrawTriangle3D(peak, fr, fl, RETRO_MOUNTAIN_FILL);
    DrawTriangle3D(peak, fl, bl, RETRO_MOUNTAIN_FILL);

    // Draw wireframe edges
    DrawLine3D(peak, bl, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(peak, br, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(peak, fl, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(peak, fr, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(bl, br, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(br, fr, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(fr, fl, RETRO_MOUNTAIN_WIRE);
    DrawLine3D(fl, bl, RETRO_MOUNTAIN_WIRE);

    // Snow cap
    if (snow_cap && height > 15.0f) {
        float snow_height = height * 0.7f;
        Vector3 snow_peak = peak;
        Vector3 snow_bl = { base_center.x - hw * 0.3f, base_center.y + snow_height, base_center.z - hd * 0.3f };
        Vector3 snow_br = { base_center.x + hw * 0.3f, base_center.y + snow_height, base_center.z - hd * 0.3f };
        Vector3 snow_fl = { base_center.x - hw * 0.3f, base_center.y + snow_height, base_center.z + hd * 0.3f };
        Vector3 snow_fr = { base_center.x + hw * 0.3f, base_center.y + snow_height, base_center.z + hd * 0.3f };

        DrawTriangle3D(snow_peak, snow_bl, snow_br, RETRO_MOUNTAIN_SNOW);
        DrawTriangle3D(snow_peak, snow_br, snow_fr, RETRO_MOUNTAIN_SNOW);
        DrawTriangle3D(snow_peak, snow_fr, snow_fl, RETRO_MOUNTAIN_SNOW);
        DrawTriangle3D(snow_peak, snow_fl, snow_bl, RETRO_MOUNTAIN_SNOW);
    }
}

// Draw a retro lake (polygon on ground)
static void draw_lake(Vector3 center, float radius, int segments)
{
    // Draw filled lake
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2.0f * PI;
        float angle2 = (float)(i + 1) / segments * 2.0f * PI;

        Vector3 p1 = { center.x + cosf(angle1) * radius, 0.02f, center.z + sinf(angle1) * radius };
        Vector3 p2 = { center.x + cosf(angle2) * radius, 0.02f, center.z + sinf(angle2) * radius };
        Vector3 c = { center.x, 0.02f, center.z };

        DrawTriangle3D(c, p1, p2, RETRO_LAKE);
    }

    // Draw wireframe outline
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2.0f * PI;
        float angle2 = (float)(i + 1) / segments * 2.0f * PI;

        Vector3 p1 = { center.x + cosf(angle1) * radius, 0.03f, center.z + sinf(angle1) * radius };
        Vector3 p2 = { center.x + cosf(angle2) * radius, 0.03f, center.z + sinf(angle2) * radius };

        DrawLine3D(p1, p2, RETRO_LAKE_WIRE);
    }
}

// Draw retro stars in the sky
static void draw_stars(void)
{
    // Fixed star positions (pseudo-random but consistent)
    static const float stars[][3] = {
        { -80, 50, -60 }, { 40, 70, -80 }, { -30, 60, -90 },
        { 70, 45, -70 }, { -60, 80, -50 }, { 20, 55, -85 },
        { -45, 65, -75 }, { 55, 75, -65 }, { -15, 85, -55 },
        { 80, 60, -45 }, { -70, 50, -80 }, { 35, 90, -70 },
        { -25, 70, -60 }, { 60, 55, -90 }, { -50, 85, -40 },
        { 15, 65, -75 }, { -80, 75, -55 }, { 45, 80, -85 },
        { -35, 55, -65 }, { 75, 70, -50 }, { -10, 90, -80 },
        { 50, 45, -60 }, { -65, 60, -70 }, { 25, 85, -45 },
    };
    int num_stars = sizeof(stars) / sizeof(stars[0]);

    for (int i = 0; i < num_stars; i++) {
        Vector3 pos = { stars[i][0], stars[i][1], stars[i][2] };
        float size = 0.3f + (i % 3) * 0.2f;

        // Draw star as small cross
        DrawLine3D(
            (Vector3){ pos.x - size, pos.y, pos.z },
            (Vector3){ pos.x + size, pos.y, pos.z },
            RETRO_STAR
        );
        DrawLine3D(
            (Vector3){ pos.x, pos.y - size, pos.z },
            (Vector3){ pos.x, pos.y + size, pos.z },
            RETRO_STAR
        );
    }
}

// Draw distant terrain (mountains, lakes)
static void draw_terrain(void)
{
    // Draw stars first (background)
    draw_stars();

    // Back mountain range (far)
    draw_mountain((Vector3){ -60, 0, -70 }, 30, 35, 20, true);
    draw_mountain((Vector3){ -25, 0, -80 }, 40, 45, 25, true);
    draw_mountain((Vector3){ 15, 0, -75 }, 35, 40, 22, true);
    draw_mountain((Vector3){ 50, 0, -85 }, 45, 50, 28, true);
    draw_mountain((Vector3){ 85, 0, -70 }, 30, 32, 18, true);

    // Middle mountain range
    draw_mountain((Vector3){ -70, 0, -45 }, 25, 22, 15, false);
    draw_mountain((Vector3){ -40, 0, -50 }, 30, 28, 18, true);
    draw_mountain((Vector3){ 0, 0, -55 }, 35, 30, 20, false);
    draw_mountain((Vector3){ 35, 0, -48 }, 28, 25, 16, false);
    draw_mountain((Vector3){ 70, 0, -52 }, 32, 27, 17, true);

    // Side mountains (left)
    draw_mountain((Vector3){ -80, 0, -20 }, 20, 18, 12, false);
    draw_mountain((Vector3){ -85, 0, 5 }, 25, 20, 14, false);

    // Side mountains (right)
    draw_mountain((Vector3){ 80, 0, -15 }, 22, 19, 13, false);
    draw_mountain((Vector3){ 88, 0, 10 }, 28, 22, 16, false);

    // Lakes
    draw_lake((Vector3){ -35, 0, -25 }, 8, 8);
    draw_lake((Vector3){ 45, 0, -30 }, 12, 10);
    draw_lake((Vector3){ -15, 0, -35 }, 6, 6);
    draw_lake((Vector3){ 70, 0, -20 }, 5, 6);
}

// Draw a single codepoint (character) in 3D space
static void draw_codepoint_3d(Font font, int codepoint, Vector3 position,
                               float font_size, bool backface, Color tint)
{
    int index = GetGlyphIndex(font, codepoint);
    float scale = font_size / (float)font.baseSize;

    position.x += (float)(font.glyphs[index].offsetX - font.glyphPadding) * scale;
    position.z += (float)(font.glyphs[index].offsetY - font.glyphPadding) * scale;

    Rectangle src_rec = {
        font.recs[index].x - (float)font.glyphPadding,
        font.recs[index].y - (float)font.glyphPadding,
        font.recs[index].width + 2.0f * font.glyphPadding,
        font.recs[index].height + 2.0f * font.glyphPadding
    };

    float width = (font.recs[index].width + 2.0f * font.glyphPadding) * scale;
    float height = (font.recs[index].height + 2.0f * font.glyphPadding) * scale;

    if (font.texture.id > 0) {
        float tx = src_rec.x / font.texture.width;
        float ty = src_rec.y / font.texture.height;
        float tw = (src_rec.x + src_rec.width) / font.texture.width;
        float th = (src_rec.y + src_rec.height) / font.texture.height;

        rlSetTexture(font.texture.id);

        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_QUADS);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);

        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(tx, ty); rlVertex3f(0.0f, 0.0f, 0.0f);
        rlTexCoord2f(tx, th); rlVertex3f(0.0f, 0.0f, height);
        rlTexCoord2f(tw, th); rlVertex3f(width, 0.0f, height);
        rlTexCoord2f(tw, ty); rlVertex3f(width, 0.0f, 0.0f);

        if (backface) {
            rlNormal3f(0.0f, -1.0f, 0.0f);
            rlTexCoord2f(tx, ty); rlVertex3f(0.0f, 0.0f, 0.0f);
            rlTexCoord2f(tw, ty); rlVertex3f(width, 0.0f, 0.0f);
            rlTexCoord2f(tw, th); rlVertex3f(width, 0.0f, height);
            rlTexCoord2f(tx, th); rlVertex3f(0.0f, 0.0f, height);
        }
        rlEnd();
        rlPopMatrix();

        rlSetTexture(0);
    }
}

void render_text_3d(Font font, const char *text, Vector3 position,
                    float font_size, float spacing, bool backface, Color tint)
{
    int length = TextLength(text);
    float text_offset_x = 0.0f;
    float text_offset_y = 0.0f;
    float scale = font_size / (float)font.baseSize;

    for (int i = 0; i < length;) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepoint(&text[i], &codepoint_byte_count);
        int index = GetGlyphIndex(font, codepoint);

        if (codepoint == 0x3f) codepoint_byte_count = 1;

        if (codepoint == '\n') {
            text_offset_y += font_size + spacing;
            text_offset_x = 0.0f;
        } else {
            if (codepoint != ' ' && codepoint != '\t') {
                Vector3 char_pos = {
                    position.x + text_offset_x,
                    position.y,
                    position.z + text_offset_y
                };
                draw_codepoint_3d(font, codepoint, char_pos, font_size, backface, tint);
            }

            if (font.glyphs[index].advanceX == 0) {
                text_offset_x += font.recs[index].width * scale + spacing;
            } else {
                text_offset_x += font.glyphs[index].advanceX * scale + spacing;
            }
        }

        i += codepoint_byte_count;
    }
}

// Draw a single letter entity standing upright in 3D
static void render_letter_3d(const Letter *letter)
{
    if (!letter->active) return;

    char text[2] = { letter->character, '\0' };

    rlPushMatrix();
    rlTranslatef(letter->position.x, letter->position.y, letter->position.z);
    rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    rlRotatef(90.0f, 0.0f, 0.0f, -1.0f);

    float font_size = LETTER_SIZE;
    Vector2 text_size = MeasureTextEx(default_font, text, font_size, 0.0f);
    Vector3 text_pos = { -text_size.x / 2.0f, 0.0f, -text_size.y / 2.0f };

    render_text_3d(default_font, text, text_pos, font_size, 0.0f, true, letter->color);

    rlPopMatrix();
}

// Draw the adversary (menacing red X)
static void render_adversary_3d(const Adversary *adv, float time)
{
    if (!adv->active) return;

    // Pulsing effect when in cooldown
    float pulse = 1.0f;
    if (adv->hit_cooldown > 0) {
        pulse = 0.5f + 0.5f * sinf(time * 15.0f);
    }

    rlPushMatrix();
    rlTranslatef(adv->position.x, adv->position.y, adv->position.z);

    // Slow rotation for menacing effect
    rlRotatef(time * 30.0f, 0.0f, 1.0f, 0.0f);
    rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    rlRotatef(90.0f, 0.0f, 0.0f, -1.0f);

    float font_size = 3.0f;  // Bigger than regular letters
    Vector2 text_size = MeasureTextEx(default_font, "X", font_size, 0.0f);
    Vector3 text_pos = { -text_size.x / 2.0f, 0.0f, -text_size.y / 2.0f };

    Color adv_color = (Color){ (unsigned char)(255 * pulse), 0, 0, 255 };
    render_text_3d(default_font, "X", text_pos, font_size, 0.0f, true, adv_color);

    rlPopMatrix();

    // Draw glowing sphere around it
    DrawSphere(adv->position, ADVERSARY_RADIUS * 0.3f, Fade(RED, 0.3f * pulse));
    DrawSphereWires(adv->position, ADVERSARY_RADIUS, 8, 8, Fade(RED, 0.5f * pulse));
}

void render_frame(const GameState *state)
{
    BeginDrawing();
    ClearBackground((Color){ 5, 5, 15, 255 });

    BeginMode3D(state->camera);

    // Draw distant terrain (mountains, lakes, stars)
    draw_terrain();

    // Draw ground plane
    DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 150, 80 }, RETRO_GROUND);

    // Draw landing pad
    DrawCube((Vector3){ 0, 0.05f, 15 }, 6, 0.1f, 6, (Color){ 50, 50, 70, 255 });
    DrawCubeWires((Vector3){ 0, 0.05f, 15 }, 6, 0.1f, 6, YELLOW);

    // Draw retro grid lines on ground
    Color grid_color = { 35, 55, 35, 255 };
    for (int i = -75; i <= 75; i += 5) {
        DrawLine3D((Vector3){ (float)i, 0.01f, -40 }, (Vector3){ (float)i, 0.01f, 40 }, grid_color);
    }
    for (int i = -40; i <= 40; i += 5) {
        DrawLine3D((Vector3){ -75, 0.01f, (float)i }, (Vector3){ 75, 0.01f, (float)i }, grid_color);
    }

    // Draw all active letters
    for (int i = 0; i < MAX_LETTERS; i++) {
        render_letter_3d(&state->letters[i]);

        if (state->letters[i].active) {
            DrawSphereWires(state->letters[i].position, LETTER_COLLISION_RADIUS, 8, 8,
                           Fade(state->letters[i].color, 0.3f));
        }
    }

    // Draw adversary
    render_adversary_3d(&state->adversary, (float)GetTime());

    // Draw player
    const Player *player = &state->player;

    // Thrust flame effect
    if (player->is_thrusting) {
        Vector3 flame_pos = player->position;
        flame_pos.y -= 0.5f;
        DrawSphere(flame_pos, 0.4f, ORANGE);
        DrawSphere(flame_pos, 0.3f, YELLOW);
    }

    // Draw player collision sphere
    DrawSphereWires(player->position, PLAYER_RADIUS, 8, 8, Fade(WHITE, 0.3f));

    EndMode3D();

    // Draw score
    const char *score_text = TextFormat("SCORE: %d", state->score);
    int score_width = MeasureText(score_text, 50);
    DrawText(score_text, (SCREEN_WIDTH - score_width) / 2, 20, 50, GREEN);

    // Draw fuel gauge (left side)
    DrawRectangle(20, 180, 40, 300, Fade(BLACK, 0.7f));
    DrawRectangleLines(20, 180, 40, 300, ORANGE);
    float fuel_normalized = player->fuel / MAX_FUEL;
    if (fuel_normalized > 1.0f) fuel_normalized = 1.0f;
    if (fuel_normalized < 0.0f) fuel_normalized = 0.0f;
    int fuel_height = (int)(fuel_normalized * 280);
    Color fuel_color = ORANGE;
    if (player->fuel < 20.0f) fuel_color = RED;
    else if (player->fuel < 40.0f) fuel_color = YELLOW;
    DrawRectangle(25, 470 - fuel_height, 30, fuel_height, fuel_color);
    DrawText("FUEL", 22, 485, 14, ORANGE);
    DrawText(TextFormat("%.0f", player->fuel), 25, 165, 14, WHITE);

    // Low fuel warning
    if (player->fuel < 20.0f && player->fuel > 0) {
        DrawText("! LOW FUEL !", (SCREEN_WIDTH - MeasureText("! LOW FUEL !", 20)) / 2,
                 SCREEN_HEIGHT / 2 - 50, 20, RED);
    } else if (player->fuel <= 0) {
        DrawText("!! EMPTY !!", (SCREEN_WIDTH - MeasureText("!! EMPTY !!", 24)) / 2,
                 SCREEN_HEIGHT / 2 - 50, 24, RED);
    }

    // Draw altitude meter (right side)
    DrawRectangle(SCREEN_WIDTH - 60, 100, 40, 400, Fade(BLACK, 0.7f));
    DrawRectangleLines(SCREEN_WIDTH - 60, 100, 40, 400, GREEN);
    float alt_normalized = player->position.y / 20.0f;
    if (alt_normalized > 1.0f) alt_normalized = 1.0f;
    if (alt_normalized < 0.0f) alt_normalized = 0.0f;
    int alt_height = (int)(alt_normalized * 380);
    DrawRectangle(SCREEN_WIDTH - 55, 495 - alt_height, 30, alt_height,
                  player->is_grounded ? YELLOW : GREEN);
    DrawText("ALT", SCREEN_WIDTH - 55, 505, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f", player->position.y), SCREEN_WIDTH - 58, 80, 14, WHITE);

    // Draw velocity indicator
    DrawText(TextFormat("VEL Y: %.1f", player->velocity.y),
             SCREEN_WIDTH - 150, SCREEN_HEIGHT - 60, 16,
             player->velocity.y < -5 ? RED : LIGHTGRAY);

    // Draw thrust indicator
    if (player->is_thrusting && player->fuel > 0) {
        DrawText(">>> THRUST <<<", (SCREEN_WIDTH - MeasureText(">>> THRUST <<<", 24)) / 2,
                 SCREEN_HEIGHT - 80, 24, ORANGE);
    }

    // Draw grounded indicator
    if (player->is_grounded) {
        DrawText("LANDED", (SCREEN_WIDTH - MeasureText("LANDED", 20)) / 2,
                 SCREEN_HEIGHT - 50, 20, YELLOW);
    }

    // Hardcore mode indicator
    if (audio_is_hardcore()) {
        DrawRectangle(SCREEN_WIDTH/2 - 100, 75, 200, 30, Fade(RED, 0.8f));
        DrawText("!! HARDCORE MODE !!", SCREEN_WIDTH/2 - 90, 80, 20, WHITE);
    }

    // Adversary proximity warning
    float adv_dx = state->adversary.position.x - player->position.x;
    float adv_dy = state->adversary.position.y - player->position.y;
    float adv_dz = state->adversary.position.z - player->position.z;
    float adv_dist = sqrtf(adv_dx*adv_dx + adv_dy*adv_dy + adv_dz*adv_dz);
    if (adv_dist < 15.0f && state->adversary.hit_cooldown <= 0) {
        float flash = sinf((float)GetTime() * 10.0f) > 0 ? 1.0f : 0.5f;
        DrawText("!! DANGER - X NEARBY !!", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2, 20,
                 Fade(RED, flash));
    }

    // Draw UI overlay
    DrawRectangle(10, 10, 260, 165, Fade(BLACK, 0.7f));
    DrawRectangleLines(10, 10, 260, 165, audio_is_hardcore() ? RED : GREEN);

    DrawText("LUNAR LETTERS", 20, 20, 16, audio_is_hardcore() ? RED : GREEN);
    DrawText("SPACE - Thrust (uses fuel)", 25, 42, 11, ORANGE);
    DrawText("WASD - Move, Arrows - Look", 25, 56, 11, LIGHTGRAY);
    DrawText("R - Reset, M - Mute", 25, 70, 11, LIGHTGRAY);
    DrawText("GREEN = +Fuel +2pts", 25, 90, 12, GREEN);
    DrawText("RED = -Fuel +1pt", 25, 105, 12, RED);
    DrawText("X = Chaser! -5pts", 25, 120, 12, MAROON);
    DrawText("Keep hitting green for HARDCORE!", 25, 140, 10, YELLOW);

    // Draw letter count
    int active_count = 0;
    for (int i = 0; i < MAX_LETTERS; i++) {
        if (state->letters[i].active) active_count++;
    }
    DrawText(TextFormat("Letters: %d", active_count), 20, SCREEN_HEIGHT - 30, 16, LIGHTGRAY);

    DrawFPS(SCREEN_WIDTH - 100, 10);

    EndDrawing();
}

void render_cleanup(void)
{
    // Nothing to cleanup
}
