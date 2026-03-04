#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <raylib.h>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

const Color OFF_WHITE = {245, 240, 230, 255};
const Color OFF_BLACK = {90, 84, 76, 255};
const Color OFF_GREY = {238, 231, 221, 255};

const Color MAY_ORANGE = {224, 127, 19, 255};
const Color MAY_MAGENTA = {215, 92, 123, 255};
const Color MAY_PURPLE = {166, 109, 211, 255};
const Color MAY_SKYBLUE = {28, 145, 228, 255};

const Color BG_COLOR = OFF_WHITE;
const Color FG_COLOR = OFF_BLACK;

enum Phase { WORK, SHORT_BREAK, LONG_BREAK };

const char *APP_NAME = "Tomayto";
const int SCREEN_WIDTH = 600;
const int SCREEN_HEIGHT = 380;

const float WORK_DUR = 25 * 60;       // 25 minutes, work
const float SHORT_BREAK_DUR = 5 * 60; // 5  minutes, short break
const float LONG_BREAK_DUR = 15 * 60; // 15 minutes, short break
// short testing values:
// const float WORK_DUR = 20;       // 25 minutes, work
// const float SHORT_BREAK_DUR = 5; // 5  minutes, short break
// const float LONG_BREAK_DUR = 15; // 15 minutes, short break

// non-const globals.
// i figured that these are, in fact, just things that the whole application should know about.
// like, since this is a pomodoro timer, there's not really any risk of things like "which pomodoro
// cycle are we on?" being traded out for some other implementation
int pomodoro_nr = 1;
Phase phase = WORK;
Color ACCENT = MAY_ORANGE; // non-const as the accent color changes with phase

void draw_header();
int update_time_on_disk(int increment = WORK_DUR);

class Button {
  private:
    inline static vector<Button *> buttons;

    Rectangle rect;
    Color body_color;

    string text;
    int font_size;
    Color text_color;

    // when this key is pressed, treat it the same as thought the button had been clicked
    int key_equiv;

    bool pressed = false;
    // store the temporary state of what color should be used to draw the button; we figure
    // out what color the button should be in update(), but need it for later in draw()
    Color next_draw_color;

    void update() {
        // press events need to be run on release rather than press, because press events will keep
        // happening frame after frame, while release events only run once
        // (difference between saying the button is constantly flipping back/forth every frame
        // the user happens to hold a button down for vs toggling it once)
        pressed = false;

        // derive what color the button should be from base color, as
        // well as from if the button is being hovered / clicked
        if (CheckCollisionPointRec(GetMousePosition(), this->rect)) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);

            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                next_draw_color = ColorBrightness(body_color, -0.1f);
            } else {
                next_draw_color = ColorBrightness(body_color, 0.1f);
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                pressed = true;
            }
        } else {
            next_draw_color = body_color;
        }

        if (IsKeyDown(key_equiv)) {
            next_draw_color = ColorBrightness(body_color, -0.1f);
        }
        if (IsKeyReleased(key_equiv)) {
            pressed = true;
        }
    }

    void draw() {
        DrawRectangleRec(this->rect, next_draw_color);
        if (text != "") {
            // calculate the x/y the text needs to be drawn at to be centered within the button
            int button_text_width = MeasureText(text.c_str(), font_size);

            int btn_center_h = (rect.width / 2) + rect.x;
            int btn_text_x = btn_center_h - (button_text_width / 2);

            int btn_center_v = (rect.height / 2) + rect.y;
            int btn_text_y = btn_center_v - (font_size / 2);

            DrawText(text.c_str(), btn_text_x, btn_text_y, font_size, text_color);
        }
    }

  public:
    static void update_buttons() {
        // reset mouse cursor as if any button is being pressed, it will be changed
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        for (auto btn : buttons) {
            btn->update();
        }
    }

    void set_body_color(Color new_color) { this->body_color = new_color; }

    static void draw_buttons() {
        for (auto btn : buttons) {
            btn->draw();
        }
    }

    void set_text(const char *text) { this->text = text; }
    bool just_pressed() { return pressed; }

    Button(Rectangle rect, Color body_color, string text, int font_size, Color text_color,
           int key_equiv)
        : rect(rect), body_color(body_color), text(text), font_size(font_size),
          text_color(text_color), key_equiv(key_equiv) {
        buttons.push_back(this);
    };
};

class Timer {
  private:
    const float durations[3] = {WORK_DUR, SHORT_BREAK, LONG_BREAK};
    float timer = WORK_DUR;
    bool is_paused_ = true;

    bool pause_just_changed_ = false;
    bool phase_just_changed_ = false;
    bool time_ran_out_ = false;

    // if 'forward' is true, skip the phase forward,
    // if false, skip backwards
    void change_phase(bool forward) {
        is_paused_ = true;
        phase_just_changed_ = true;
        pause_just_changed_ = true;

        // these are what i'm calling "song skip mechanics". if you're in a phase, say halfway
        // through, and skip back, you'll first skip to the beginning of that phase.
        /*
        if (!forward) {
            switch (phase) {
            case WORK:
                if (WORK_DUR - ceil(timer) > 5) {

                } else {
                }
                break;
            case SHORT_BREAK:
                if (SHORT_BREAK_DUR - ceil(timer) > 5) {

                } else {
                }
                break;
            case LONG_BREAK:
                if (LONG_BREAK_DUR - ceil(timer) > 5) {

                } else {
                }
                break;
            }
            if (phase == WORK && (WORK_DUR - ceil(timer) < 5)) {
            }
        }
        */

        if (phase == WORK) {

            if (!forward) {
                pomodoro_nr--;
            }

            if ((pomodoro_nr % 4) == 0) {
                phase = LONG_BREAK;
                timer = durations[LONG_BREAK];
            } else {
                phase = SHORT_BREAK;
                timer = durations[SHORT_BREAK];
            }

        } else {
            phase = WORK;
            timer = durations[WORK];

            if (forward) {
                pomodoro_nr++;
            }
        }
    }

  public:
    void skip_forward() { change_phase(true); }

    void skip_back() {
        if ((ceil(timer) == durations[phase]) && !(pomodoro_nr == 1 && phase == WORK)) {
            // less than 5 seconds have passed, skip back a phase
            // also prevent skipping back to pomodoro numbers
            change_phase(false);
        } else {
            // more than 5 seconds have passed, skip to the beginning of this phase
            is_paused_ = true;
            pause_just_changed_ = true;
            timer = durations[phase];
        }
    }

    void playpause() {
        this->is_paused_ = !this->is_paused_;
        this->pause_just_changed_ = true;
    }

    // reset to pomodoro #1
    void reset() {
        phase = WORK;
        pomodoro_nr = 1;

        timer = durations[WORK];
        is_paused_ = true;
        pause_just_changed_ = true;
    }

    bool is_paused() { return this->is_paused_; }

    bool pause_just_changed() { return this->pause_just_changed_; }

    bool phase_just_changed() { return this->phase_just_changed_; }

    bool time_ran_out() { return this->time_ran_out_; }

    void update() {
        float delta = GetFrameTime();
        if (!is_paused_) {
            timer -= delta;
        }

        if (timer < 0) {
            change_phase(true);
            time_ran_out_ = true;
        } else {
            phase_just_changed_ = false;
            time_ran_out_ = false;
        }
    }

    // TODO: make this function less like-that
    void draw() {
        int time_left = max(0.0f, ceil(this->timer));
        int minutes = time_left / 60;
        int seconds = time_left % 60;
        string min_str = to_string(minutes);
        string sec_str = to_string(seconds);

        string minutes_tens_place = to_string(minutes / 10);
        string minutes_ones_place = to_string(minutes % 10);

        string seconds_tens_place = to_string(seconds / 10);
        string seconds_ones_place = to_string(seconds % 10);

        int number_font_size = 100;
        int word_font_size = number_font_size * 0.6;

        // in the default font, each number character (besides 1) is (font_size / 2) px wide,
        // and will have (font_size / 10) px space between them. the '1' character, however, is
        // far slimmer (font_size / 5), and so simply drawing the '1' character as part of a
        // longer string will mess up the nice alignment that all other numbers have.

        // example with font size 100:
        // so, with most number chars, it's:
        // 50px - 10px - 50px
        // number, space between chars, number
        // but if we have a one in the tens place, it needs to be:
        // 30px - 20px - 10px - 50px
        // padding, '1', space between chars, number

        int max_num_width = MeasureText("00", number_font_size);
        int max_word_width = MeasureText("seconds", word_font_size);

        int number_base_x = (SCREEN_WIDTH / 2) - ((max_word_width + max_num_width + 20) / 2);
        int number_base_y = 60;

        // grey transparency (? i'm not sure if i ought to keep this)
        DrawText("00", number_base_x, number_base_y, number_font_size, OFF_GREY);
        DrawText("00", number_base_x, number_base_y + number_font_size, number_font_size, OFF_GREY);

        // minutes
        if (minutes_tens_place == "1") {
            DrawText(minutes_tens_place.c_str(),
                     number_base_x + (number_font_size / 2) - (number_font_size / 5), number_base_y,
                     number_font_size, ACCENT);
        } else if (minutes_tens_place == "0") {
            // don't draw the leading zero
        } else {
            DrawText(minutes_tens_place.c_str(), number_base_x, number_base_y, number_font_size,
                     ACCENT);
        }

        if (minutes_ones_place == "1") {
            DrawText(minutes_ones_place.c_str(),
                     (number_base_x + (number_font_size / 2)) +
                         ((number_font_size / 2) - (number_font_size / 5)) +
                         (number_font_size / 10),
                     number_base_y, number_font_size, ACCENT);

        } else {
            DrawText(minutes_ones_place.c_str(),
                     number_base_x + (number_font_size / 2) + (number_font_size / 10),
                     number_base_y, number_font_size, ACCENT);
        }

        if (minutes == 1) {
            DrawText("minute", (number_base_x + max_num_width) + (20), number_base_y + (33),
                     word_font_size, OFF_BLACK);
        } else {
            DrawText("minutes", (number_base_x + max_num_width) + (20), number_base_y + (33),
                     word_font_size, OFF_BLACK);
        }

        // seconds
        // DrawText(sec_str.c_str(), number_base_x, number_base_y + number_font_size,
        // number_font_size,
        //          ACCENT);
        if (seconds_tens_place == "1") {
            DrawText(seconds_tens_place.c_str(),
                     number_base_x + (number_font_size / 2) - (number_font_size / 5),
                     number_base_y + number_font_size, number_font_size, ACCENT);
        } else if (seconds_tens_place == "0") {
            // don't draw the leading zero
        } else {
            DrawText(seconds_tens_place.c_str(), number_base_x, number_base_y + number_font_size,
                     number_font_size, ACCENT);
        }

        if (seconds_ones_place == "1") {
            DrawText(seconds_ones_place.c_str(),
                     (number_base_x + (number_font_size / 2)) +
                         ((number_font_size / 2) - (number_font_size / 5)) +
                         (number_font_size / 10),
                     number_base_y + number_font_size, number_font_size, ACCENT);
        } else {
            DrawText(seconds_ones_place.c_str(),
                     number_base_x + (number_font_size / 2) + (number_font_size / 10),
                     number_base_y + number_font_size, number_font_size, ACCENT);
        }
        if (seconds == 1) {
            DrawText("second", number_base_x + max_num_width + 20,
                     number_base_y + number_font_size + 33, word_font_size, FG_COLOR);
        } else {
            DrawText("seconds", number_base_x + max_num_width + 20,
                     number_base_y + number_font_size + 33, word_font_size, FG_COLOR);
        }
    }
};

// TODO: this
void notify() {
#if defined(__linux__)
    std::cout << "linux" << "\n";
#elif defined(_WIN32)
    std::cout << "not on linux" << "\n";
#else

#endif
}

int main() {
    // Raylib Initialization -----------------------------------------------------------

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, APP_NAME);
    // SetExitKey(KEY_NULL); // prevent ESC from closing the window

    InitAudioDevice();
    Sound changeover_sound = LoadSound("explosion_good.wav");

    int monitor_nr = GetCurrentMonitor();
    int refresh_rate = GetMonitorRefreshRate(monitor_nr);
    SetTargetFPS(refresh_rate);
    //----------------------------------------------------------------------------------

    // Initialization for all the buttons
    const float start_btn_width = 300;
    const float start_btn_height = 80;
    Rectangle start_btn_rect = {SCREEN_WIDTH / 2.0f - start_btn_width / 2.0f,
                                310 - start_btn_height / 2.0f, start_btn_width, start_btn_height};
    Button *start_btn = new Button(start_btn_rect, ACCENT, "start", 50, BG_COLOR, KEY_SPACE);

    const float seek_btn_side = 80;
    const float seek_padding = 20;
    Rectangle backward_rect = {start_btn_rect.x - seek_btn_side - seek_padding, start_btn_rect.y,
                               seek_btn_side, seek_btn_side};
    Rectangle forward_rect = {start_btn_rect.x + start_btn_rect.width + seek_padding,
                              start_btn_rect.y, seek_btn_side, seek_btn_side};
    Button *forward_btn = new Button(forward_rect, ACCENT, ">", 50, BG_COLOR, KEY_L);
    Button *backward_btn = new Button(backward_rect, ACCENT, "<", 50, BG_COLOR, KEY_H);

    const float reset_btn_padding = 10;
    const float reset_btn_width = 80;
    Rectangle reset_rect = {SCREEN_WIDTH - reset_btn_padding - reset_btn_width, 50, reset_btn_width,
                            30};
    Color reset_btn_color = ColorBrightness(BG_COLOR, -0.2f);
    Button *reset_btn = new Button(reset_rect, reset_btn_color, "reset", 20, FG_COLOR, KEY_R);

    Timer timer = Timer();

    // Main loop
    while (!WindowShouldClose()) {
        // Update
        //----------------------------------------------------------------------------------

        Button::update_buttons();
        timer.update();

        if (start_btn->just_pressed()) {
            timer.playpause();
        }

        if (forward_btn->just_pressed()) {
            timer.skip_forward();
        }
        if (backward_btn->just_pressed()) {
            timer.skip_back();
        }

        if (reset_btn->just_pressed()) {
            timer.reset();
        }

        if (timer.phase_just_changed()) {
            switch (phase) {
            case WORK:
                ACCENT = MAY_ORANGE;
                break;
            case SHORT_BREAK:
            case LONG_BREAK:
                ACCENT = MAY_MAGENTA;
                break;
            }
            start_btn->set_body_color(ACCENT);
            forward_btn->set_body_color(ACCENT);
            backward_btn->set_body_color(ACCENT);
        }

        // only alert the user if the timer runs out on its own
        if (timer.time_ran_out()) {
            PlaySound(changeover_sound);

            string message = "time for ";
            switch (phase) {
            case WORK:
                message += "work!";
                break;
            case SHORT_BREAK:
                message += "a short break!";
                break;
            case LONG_BREAK:
                message += "a long break!";
                break;
            }

            string command =
                "notify-send \"" + string(APP_NAME) + "\"" + " " + "\"" + message + "\"";
            system(command.c_str());
        }

        // above things will affect the value of pause_just_changed, so listen for this last
        if (timer.pause_just_changed()) {
            start_btn->set_text((timer.is_paused()) ? "start" : "pause");
        }

        //----------------------------------------------------------------------------------

        // BEGIN DRAWING
        //----------------------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BG_COLOR);

        draw_header();

        Button::draw_buttons();
        timer.draw();

        EndDrawing();
        // END DRAWING
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // even if we end the phase early, still record that time
    // TODO: update this here, probably add a "get time left" on timer or something
    if (phase == WORK) {
        // int seconds_worked = WORK_DUR - max(0.0f, ceil(timer));
        // update_time_on_disk(seconds_worked);
    }

    UnloadSound(changeover_sound);
    CloseAudioDevice();
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
}

// i don't feel this warrants a class of its own like Timer or Button since there isn't any internal
// state stored by the header itself; all it does is just reference two global variables
void draw_header() {
    string text = "pomodoro #" + to_string(pomodoro_nr) + " - ";

    switch (phase) {
    case WORK:
        text += "work";
        break;
    case SHORT_BREAK:
        text += "short break";
        break;
    case LONG_BREAK:
        text += "long break";
        break;
    }

    int font_size = 30;
    int text_width = MeasureText(text.c_str(), font_size);

    int text_x = (SCREEN_WIDTH * 0.5) - (text_width / 2.0);
    int text_y = 5;

    Rectangle header = {0, 0, SCREEN_WIDTH, 40};
    DrawRectangleRec(header, ACCENT);
    DrawText(text.c_str(), text_x, text_y, font_size, BG_COLOR);
};

// store the amount of working time that's been tracked by tomayto.
// returns the number of SECONDS that have been spent working,
// or -1 on failure of some sort
int update_time_on_disk(int increment) {
    fs::path base_path;

#if defined(__linux__)
    // based on: https://specifications.freedesktop.org/basedir/latest/
    const char *xdg_data_home = getenv("XDG_DATA_HOME");
    if (!xdg_data_home || strcmp(xdg_data_home, "") == 0) {

        // $HOME always set on linux, so don't need to check validity
        fs::path path_to_use = string(getenv("HOME"));
        path_to_use = path_to_use / ".local/share";

        if (!fs::is_directory(path_to_use)) {
            return -1; // TODO: handle this?
        }
        base_path = path_to_use;
    } else {
        // non-empty, non-null XDG_DATA_HOME path given
        if (!fs::is_directory(xdg_data_home)) {
            return -1; // TODO: handle this?
        }
        base_path = xdg_data_home;
    }

#elif defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    cout << "appdata path: " << appdata << "\n";
    return -1;
#else
    return -1;
#endif
    cout << "Base path for data: " << base_path << "\n";

    // create/find tomayto directory
    fs::path tomayto_path;
    tomayto_path = base_path / "tomayto";
    cout << "Tomayto path: " << tomayto_path << "\n";

    if (!fs::is_directory(tomayto_path)) {
        cout << "Creating directory tomayto at " << base_path << "\n";
        if (!fs::create_directory(tomayto_path)) {
            cout << "ERROR: failed to create directory " << tomayto_path << "\n";
            return -1; // TODO: handle this?
        }
    } else {
        cout << "Found Tomayto data folder at " << tomayto_path << "\n";
    }

    // create/locate data.txt file
    fs::path data_file_path = tomayto_path / "data.txt";
    int seconds = 0;
    if (!fs::exists(data_file_path)) {
        ofstream data_file{data_file_path};
        // data_file << 0 << endl;
        // data_file.close();
        cout << "Created new data file at: " << data_file_path << "\n";
    } else {
        ifstream data_file(data_file_path);
        data_file >> seconds;
        data_file.close();
        cout << "Data file found at: " << data_file_path << "\n";
    }

    // update the file with the new time spent working
    seconds += increment;
    ofstream data_file(data_file_path);
    data_file << seconds << endl;
    data_file.close();

    return seconds;

    // TODO: non-linux
    return -1;
}

/* TODO List
 * + display the # of minutes/seconds worked somewhere
 *
 * + improve how the data is written/read, specifically, keep track of the
 *   number of seconds worked /in memory/ and read it in when the application starts
 *
 * + get this whole thing compiling on windows, figure out the project structure for that
 *   + really, use a better build system than just "make with one rule".
 *     This is especially true if i want to figure out windows toast notifications
 *
 * + allow skipping forward/back a phase. allow resetting to Pomodoro #1 so you don't have to
 * just restart the application if you go away from your computer for a while.
 *   + skipping should use song skip mechanics, "back" during a phase puts you at the start of
 * that phase, back again will put you in the previous phase, but "forward" will send you to the
 * next phase.
 *
 *   + also, keyboard shortcuts for these. H and L for back / forward
 *
 * + maybe a tray icon?
 *
 * + reminder to start the timer again? quite a few times now i've finished a work period, but,
 * wanting to just finish some particular /thing/ that i'm on, kept working for a minute or two
 * and then forgotten to take the break because the prompt to take one has already ended, i've
 * alreaady heard the sound.
 *
 * + Sound that plays when you hit start/pause, and when the timer ends. Clapboard? It needs to
 * be a /good/ sound when you hit start, and a neutral (not not good, just an indication) one
 * when you pause, if that makes any sense. You want to hear a /satisfying/ thing when you start
 * the timer up.
 *
 * + Proper icon for the application
 *
 */
