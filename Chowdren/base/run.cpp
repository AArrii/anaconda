#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include "manager.h"
#include "fpslimit.h"

GameManager manager;

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "chowconfig.h"
#include "framedata.h"
#include "common.h"
#include "fonts.h"
#include "crossrand.h"
#include "media.h"

#if defined(CHOWDREN_IS_DESKTOP)
#include "SDL.h"
#endif

#ifdef CHOWDREN_IS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#ifndef NDEBUG
#define CHOWDREN_DEBUG
#endif

#ifdef CHOWDREN_DEBUG
#define CHOWDREN_SHOW_DEBUGGER
#endif

GameManager::GameManager()
: frame(NULL), window_created(false), fullscreen(false), off_x(0), off_y(0),
  x_size(WINDOW_WIDTH), y_size(WINDOW_HEIGHT), values(NULL), strings(NULL),
  fade_value(0.0f), fade_dir(0.0f), lives(0), ignore_controls(false),
  last_control_flags(0), control_flags(0), player_died(true)
{
}

static Frames static_frames;

void GameManager::init()
{
#ifdef CHOWDREN_USE_PROFILER
    PROFILE_SET_DAMPING(0.0);
#endif
    frame = &static_frames;

#ifdef CHOWDREN_IS_DEMO
    idle_timer_started = false;
    global_time = show_build_timer = reset_timer = manual_reset_timer = 0.0;
#endif
    platform_init();
    set_window(false);

    // application setup
    preload_images();
    reset_globals();
    setup_keys(this);
    media.init();

    // setup random generator from start
    cross_srand((unsigned int)platform_get_global_time());

    fps_limit.set(FRAMERATE);

#if defined(CHOWDREN_IS_AVGN)
    set_frame(0);
#elif defined(CHOWDREN_IS_HFA)
    set_frame(60);
#elif defined(CHOWDREN_IS_FP)
    set_frame(20);
#else
    set_frame(0);
#endif
}

void GameManager::reset_globals()
{
    delete values;
    delete strings;
    values = frame->global_values = new GlobalValues;
    strings = frame->global_strings = new GlobalStrings;
    setup_globals(values, strings);
}

void GameManager::set_window(bool fullscreen)
{
    if (window_created)
        return;
    this->fullscreen = fullscreen;
    window_created = true;

    platform_create_display(fullscreen);

    // OpenGL init
    glc_init();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    init_shaders();

#ifdef CHOWDREN_VSYNC
    platform_set_vsync(true);
#else
    platform_set_vsync(false);
#endif
}

void GameManager::set_window_scale(int scale)
{
    std::cout << "Set window scale: " << scale << std::endl;
}

bool GameManager::is_fullscreen()
{
    return fullscreen;
}

void GameManager::on_key(int key, bool state)
{
#ifdef CHOWDREN_IS_DEMO
    idle_timer_started = true;
    idle_timer = 0.0;
#endif

#ifdef CHOWDREN_IS_DESKTOP
    if (state && key == SDLK_RETURN) {
        bool alt = keyboard.is_pressed(SDLK_LALT) ||
                   keyboard.is_pressed(SDLK_RALT);
        if (alt) {
            fullscreen = !fullscreen;
            platform_set_fullscreen(fullscreen);
            return;
        }
    }
#endif

    if (state)
        keyboard.add(key);
    else
        keyboard.remove(key);
    if (state)
        frame->last_key = key;
}

void GameManager::on_mouse(int key, bool state)
{
    if (state)
        mouse.add(key);
    else
        mouse.remove(key);
}

int GameManager::update_frame()
{
#ifdef CHOWDREN_USE_PROFILER
    PROFILE_FUNC();
#endif
    double dt = fps_limit.dt;
    if (fade_dir != 0.0f) {
        fade_value += fade_dir * (float)dt;
        if (fade_value <= 0.0f || fade_value >= 1.0f) {
            fade_dir = 0.0f;
            fade_value = std::min(1.0f, std::max(0.0f, fade_value));
            return 2;
        }
        return 1;
    }

    if (frame->next_frame != -1 && fade_dir == 0.0f) {
        // platform layer may want to force a swap if double-buffered, etc.
        platform_prepare_frame_change();
        set_frame(frame->next_frame);
    }

    this->dt = float(dt);

#ifdef CHOWDREN_IS_DEMO
    global_time += dt;
    if (idle_timer_started) {
        idle_timer += dt;
        reset_timer += dt;
    }
    if (platform_should_reset())
        manual_reset_timer += dt;
    else
        manual_reset_timer = 0.0;
    if (idle_timer >= 60 || manual_reset_timer >= 3.0) {
        set_frame(-2);
    }
    if (reset_timer >= 60*10) {
        reset_timer = -10.0;
        media.stop_samples();
        set_frame(5);
    }
    if (global_time <= 60) {
        show_build_timer -= dt;
        if (platform_show_build_info())
            show_build_timer = 3.0;
    }
#endif

    bool ret = frame->update();
    if (ret)
        return 1;
    return 0;
}

void GameManager::set_framerate(int framerate)
{
    fps_limit.set(framerate);
}

#ifdef CHOWDREN_IS_DEMO
#include "font.h"
FTTextureFont * get_font(int size);
#endif

void GameManager::draw()
{
    if (!window_created)
        return;

    int window_width, window_height;
    platform_get_size(&window_width, &window_height);
    if (window_width <= 0 || window_height <= 0)
        // for some reason, GLFW sets these properties to 0 when minimized.
        return;

#ifdef CHOWDREN_FORCE_REMOTE
    platform_set_remote_value(CHOWDREN_REMOTE_TARGET);
#endif

    PROFILE_FUNC();

    PROFILE_BEGIN(platform_begin_draw);
    platform_begin_draw();
    PROFILE_END();

#ifdef CHOWDREN_USE_SUBAPP
    Frame * render_frame;
    if (SubApplication::current != NULL &&
        SubApplication::current->flags & VISIBLE) {
        render_frame = &SubApplication::current->subapp_frame;
    } else
        render_frame = frame;
#else
    Frame * render_frame = frame;
#endif

    PROFILE_BEGIN(frame_draw);
#ifdef CHOWDREN_IS_WIIU
    int remote_setting = platform_get_remote_value();
    if (remote_setting == CHOWDREN_HYBRID_TARGET) {
        platform_set_display_target(CHOWDREN_REMOTE_TARGET);
        render_frame->draw(CHOWDREN_REMOTE_TARGET);
        draw_fade();
        platform_set_display_target(CHOWDREN_TV_TARGET);
        render_frame->draw(CHOWDREN_TV_TARGET);
        draw_fade();
    } else {
        platform_set_display_target(CHOWDREN_TV_TARGET);
        render_frame->draw(CHOWDREN_HYBRID_TARGET);
        draw_fade();
        if (remote_setting == CHOWDREN_REMOTE_TARGET) {
            platform_clone_buffers();
            platform_set_display_target(CHOWDREN_REMOTE_TARGET);
            render_frame->draw(CHOWDREN_REMOTE_ONLY);
        }
    }
#elif CHOWDREN_IS_3DS
    platform_set_display_target(CHOWDREN_TV_TARGET);
    render_frame->draw(CHOWDREN_TV_TARGET);
    draw_fade();

    // only draw 30 fps on bottom screen
    static int draw_interval = 0;
    if (draw_interval == 0) {
        platform_set_display_target(CHOWDREN_REMOTE_TARGET);
        render_frame->draw(CHOWDREN_REMOTE_TARGET);
        draw_fade();
    }
    draw_interval = (draw_interval + 1) % 3;
#else
    render_frame->draw(CHOWDREN_HYBRID_TARGET);
    draw_fade();
#endif
    PROFILE_END();

    glLoadIdentity();

#ifdef CHOWDREN_IS_DEMO
    if (show_build_timer > 0.0) {
        std::string date(__DATE__);
        std::string tim(__TIME__);
        std::string val = date + " " + tim;
        glPushMatrix();
        glTranslatef(50, 50, 0);
        glScalef(5, -5, 5);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        get_font(24)->Render(val.c_str(), val.size(), FTPoint(),
                             FTPoint());
        glPopMatrix();
    }
#endif

    PROFILE_BEGIN(platform_swap_buffers);
    platform_swap_buffers();
    PROFILE_END();
}

void GameManager::draw_fade()
{
    if (fade_dir == 0.0f)
        return;
    glLoadIdentity();
    glBegin(GL_QUADS);
    glColor4ub(fade_color.r, fade_color.g, fade_color.b,
               int(fade_value * 255));
    glVertex2f(0.0f, 0.0f);
    glVertex2f(WINDOW_WIDTH, 0.0f);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(0.0f, WINDOW_HEIGHT);
    glEnd();
}

void GameManager::set_frame(int index)
{
    ignore_controls = false;

#ifdef CHOWDREN_IS_DEMO
    idle_timer = 0.0;
    idle_timer_started = false;
#endif

#ifndef CHOWDREN_SAMPLES_OVER_FRAMES
    media.stop_samples();
#endif

    if (frame->index != -1)
        frame->on_end();

    if (index == -2) {
        platform_begin_draw();
        media.stop_samples();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        platform_swap_buffers();
#ifdef CHOWDREN_IS_DEMO
        reset_timer = 0.0;
#endif
        index = 0;
        // reset_global_data();
        reset_globals();
    }

    std::cout << "Setting frame: " << index << std::endl;

    frame->set_index(index);
    frame->on_start();

    std::cout << "Frame set" << std::endl;
}

void GameManager::set_fade(const Color & color, float fade_dir)
{
    fade_color = color;
    this->fade_dir = fade_dir;
    if (fade_dir < 0.0f)
        fade_value = 1.0f;
    else
        fade_value = 0.0f;
}

static int get_player_control_flags(int player);

#ifndef NDEBUG
struct InstanceCount
{
    int id;
    int count;
};

static bool sort_count(const InstanceCount & a, const InstanceCount & b)
{
    return a.count > b.count;
}

static InstanceCount counts[MAX_OBJECT_ID];

static void print_instance_stats()
{
    int count = 0;
    for (int i = 0; i < MAX_OBJECT_ID; i++) {
        int instance_count = manager.frame->instances.items[i].size();
        count += instance_count;
        counts[i].id = i;
        counts[i].count = instance_count;
    }
    std::cout << "Instance count: " << count << std::endl;

    std::sort(counts, counts + MAX_OBJECT_ID, sort_count);
    for (int i = 0; i < std::min<int>(5, MAX_OBJECT_ID); i++) {
        std::cout << counts[i].id << " has " << counts[i].count
            << " instances" << std::endl;
    }
}
#endif

bool GameManager::update()
{
// #ifndef NDEBUG
    bool show_stats = false;
    static int measure_time = 0;
    measure_time -= 1;
    if (measure_time <= 0) {
        measure_time = 200;
        show_stats = true;
    }
// #endif

    // update input
    keyboard.update();
    mouse.update();

    int new_control = get_player_control_flags(1);
    control_flags = new_control & ~(last_control_flags);
    last_control_flags = new_control;

    fps_limit.start();
    platform_poll_events();

    // update mouse position
    platform_get_mouse_pos(&mouse_x, &mouse_y);

// #ifndef NDEBUG
    if (show_stats)
        std::cout << "Framerate: " << fps_limit.current_framerate
            << std::endl;
// #endif

    if (platform_has_error()) {
        if (platform_display_closed())
            return false;
    } else {
        double event_update_time = platform_get_time();

        int ret = update_frame();

// #ifndef NDEBUG
        if (show_stats)
            std::cout << "Event update took " <<
                platform_get_time() - event_update_time << std::endl;
// #endif

        if (ret == 0)
            return false;
        else if (ret == 2)
            return true;

        if (window_created && platform_display_closed())
            return false;
    }

    double draw_time = platform_get_time();

    draw();

// #ifndef NDEBUG
    if (show_stats) {
        std::cout << "Draw took " << platform_get_time() - draw_time
            << std::endl;
        // print_instance_stats();
        platform_print_stats();
    }
// #endif

    fps_limit.finish();

#ifdef CHOWDREN_USE_PROFILER
    static int profile_time = 0;
    profile_time -= 1;
    if (profile_time <= 0) {
        profile_time += 500;
        PROFILE_UPDATE();
        PROFILE_OUTPUT("data:/profile.txt");
    }
#endif

    return true;
}

#ifdef CHOWDREN_IS_EMSCRIPTEN
static void _emscripten_run()
{
    manager.update();
}
#endif

void GameManager::run()
{
    init();

#ifdef CHOWDREN_IS_EMSCRIPTEN
    emscripten_set_main_loop(_emscripten_run, 0, 1);
#else
    while (true) {
        if (!update())
            break;
    }
    frame->data->on_app_end();
    frame->data->on_end();
    media.stop();
    platform_exit();
#endif
}

// InputList

#define INPUT_STATE_PRESSED 0
#define INPUT_STATE_HOLD 1
#define INPUT_STATE_RELEASED 2

InputList::InputList()
: last(-1)
{
}

void InputList::add(int v)
{
    last = v;

    vector<InputState>::iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        InputState & s = *it;
        if (s.key != v)
            continue;
        s.state = INPUT_STATE_PRESSED;
        return;
    }
    InputState s;
    s.key = v;
    s.state = INPUT_STATE_PRESSED;
    items.push_back(s);
}

void InputList::remove(int v)
{
    vector<InputState>::iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        InputState & s = *it;
        if (s.key != v)
            continue;
        s.state = INPUT_STATE_RELEASED;
        return;
    }
}

bool InputList::is_pressed(int v)
{
    vector<InputState>::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        const InputState & s = *it;
        if (s.key != v)
            continue;
        return s.state != INPUT_STATE_RELEASED;
    }
    return false;
}

bool InputList::is_pressed_once(int v)
{
    vector<InputState>::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        const InputState & s = *it;
        if (s.key != v)
            continue;
        return s.state == INPUT_STATE_PRESSED;
    }
    return false;
}

bool InputList::is_any_pressed()
{
    vector<InputState>::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        const InputState & s = *it;
        if (s.state == INPUT_STATE_RELEASED)
            continue;
        return true;
    }
    return false;
}

bool InputList::is_any_pressed_once()
{
    vector<InputState>::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        const InputState & s = *it;
        if (s.state != INPUT_STATE_PRESSED)
            continue;
        return true;
    }
    return false;
}

bool InputList::is_released_once(int v)
{
    vector<InputState>::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
        const InputState & s = *it;
        if (s.key != v)
            continue;
        return s.state == INPUT_STATE_RELEASED;
    }
    return false;
}

void InputList::clear()
{
    items.clear();
}

void InputList::update()
{
    vector<InputState>::iterator it = items.begin();
    while (it != items.end()) {
        InputState & s = *it;
        if (s.state != INPUT_STATE_RELEASED) {
            s.state = INPUT_STATE_HOLD;
            ++it;
            continue;
        }
        it = items.erase(it);
    }
}

// input helpers

bool is_mouse_pressed(int button)
{
    if (button < 0)
        return false;
    return manager.mouse.is_pressed(button);
}

bool is_key_pressed(int button)
{
    if (button < 0)
        return false;
    return manager.keyboard.is_pressed(button);
}

bool is_any_key_pressed()
{
    return manager.keyboard.is_any_pressed();
}

bool is_any_key_pressed_once()
{
    return manager.keyboard.is_any_pressed_once();
}

bool is_mouse_pressed_once(int key)
{
    if (key < 0)
        return false;
    return manager.mouse.is_pressed_once(key);
}

bool is_key_released_once(int key)
{
    if (key < 0)
        return false;
    return manager.keyboard.is_released_once(key);
}

bool is_key_pressed_once(int key)
{
    if (key < 0)
        return false;
    return manager.keyboard.is_pressed_once(key);
}

int get_last_key_pressed()
{
    return manager.keyboard.last;
}

enum {
    JOYSTICK_BUTTON1 = CHOWDREN_BUTTON_A,
    JOYSTICK_BUTTON2 = CHOWDREN_BUTTON_B,
    JOYSTICK_BUTTON3 = CHOWDREN_BUTTON_X,
    JOYSTICK_BUTTON4 = CHOWDREN_BUTTON_Y,
};

static int get_player_control_flags(int player)
{
    GameManager & m = manager;
    if (m.ignore_controls)
        return 0;

    int flags = 0;

    if (m.control_type == CONTROL_KEYBOARD) {
        if (is_key_pressed(m.up))
            flags |= CONTROL_UP;
        if (is_key_pressed(m.down))
            flags |= CONTROL_DOWN;
        if (is_key_pressed(m.left))
            flags |= CONTROL_LEFT;
        if (is_key_pressed(m.right))
            flags |= CONTROL_RIGHT;
        if (is_key_pressed(m.button1))
            flags |= CONTROL_BUTTON1;
        if (is_key_pressed(m.button2))
            flags |= CONTROL_BUTTON2;
        if (is_key_pressed(m.button3))
            flags |= CONTROL_BUTTON3;
        if (is_key_pressed(m.button4))
            flags |= CONTROL_BUTTON4;
    } else {
        flags |= get_joystick_direction_flags(player);
        if (is_joystick_pressed(player, JOYSTICK_BUTTON1))
            flags |= CONTROL_BUTTON1;
        if (is_joystick_pressed(player, JOYSTICK_BUTTON2))
            flags |= CONTROL_BUTTON2;
        if (is_joystick_pressed(player, JOYSTICK_BUTTON3))
            flags |= CONTROL_BUTTON3;
        if (is_joystick_pressed(player, JOYSTICK_BUTTON4))
            flags |= CONTROL_BUTTON4;
    }
    return flags;
}

bool is_player_pressed(int player, int flags)
{
    GameManager & m = manager;
    if (m.ignore_controls)
        return false;
    if (flags == 0)
        return m.last_control_flags == 0;
    return (m.last_control_flags & flags) == flags;
}

bool is_player_pressed_once(int player, int flags)
{
    GameManager & m = manager;
    if (m.ignore_controls)
        return false;
    if (flags == 0)
        return m.control_flags == 0;
    return (m.control_flags & flags) == flags;
}

// main function

int main(int argc, char *argv[])
{
#if defined(_WIN32) && defined(CHOWDREN_SHOW_DEBUGGER)
    int outHandle, errHandle, inHandle;
    FILE *outFile, *errFile, *inFile;
    AllocConsole();
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);

    outHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE),
                                _O_TEXT);
    errHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE),
                                _O_TEXT);
    inHandle = _open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE),
                               _O_TEXT);

    outFile = _fdopen(outHandle, "w" );
    errFile = _fdopen(errHandle, "w");
    inFile =  _fdopen(inHandle, "r");

    *stdout = *outFile;
    *stderr = *errFile;
    *stdin = *inFile;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    std::ios::sync_with_stdio();
#endif
    manager.run();
    return 0;
}
