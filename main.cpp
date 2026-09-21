#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <libgen.h>

/* TYPES & STRUCTURES */

enum Status { TODO = 0, IN_PROGRESS = 1, DONE = 2 };
enum ViewMode { KANBAN = 0, LIST = 1 };

struct Task {
    int id;
    std::string text;
    Status status;
};

struct Palette {
    std::string border;
    std::string active_border;
    std::string text;
    std::string accent;
    std::string selected_bg;
    std::string todo_color;
    std::string in_prog_color;
    std::string done_color;
};

struct Theme {
    std::string name;
    Palette dark_palette;
    Palette light_palette;
};

struct KeyConfig {
    std::string key_up = "UP";
    std::string key_down = "DOWN";
    std::string key_left = "LEFT";
    std::string key_right = "RIGHT";
    std::string key_move_left = "SHIFT_LEFT";
    std::string key_move_right = "SHIFT_RIGHT";
    std::string key_delete = "d";
    std::string key_toggle_mode = "TAB";
    std::string key_quit = "ESC";
};

struct CustomColors {
    std::string todo_dark = "";
    std::string todo_light = "";
    std::string in_prog_dark = "";
    std::string in_prog_light = "";
    std::string done_dark = "";
    std::string done_light = "";
};

/* GLOBAL CONFIGURATION & THEMES */

std::vector<Theme> themes = {
    {
        "Monochrome",
        {"\033[38;2;100;100;100m", "\033[38;2;220;220;220m", "\033[38;2;200;200;200m", "\033[38;2;255;255;255m", "\033[48;2;60;60;60m",   "\033[38;2;255;255;255m", "\033[38;2;120;235;120m", "\033[38;2;40;140;40m"},
        {"\033[38;2;160;160;160m", "\033[38;2;30;30;30m",    "\033[38;2;20;20;20m",    "\033[38;2;0;0;0m",       "\033[48;2;210;210;210m", "\033[38;2;40;40;40m",   "\033[38;2;20;140;20m",  "\033[38;2;10;80;10m"}
    },
    {
        "Gruvbox",
        {"\033[38;2;124;111;100m", "\033[38;2;251;73;52m",   "\033[38;2;235;219;178m", "\033[38;2;254;128;25m",  "\033[48;2;80;63;50m",     "\033[38;2;242;229;188m", "\033[38;2;184;187;38m", "\033[38;2;106;121;12m"},
        {"\033[38;2;189;174;147m", "\033[38;2;157;0;6m",     "\033[38;2;60;56;54m",    "\033[38;2;175;58;3m",    "\033[48;2;235;219;178m",  "\033[38;2;60;56;54m",    "\033[38;2;106;121;12m", "\033[38;2;50;75;10m"}
    },
    {
        "Nord",
        {"\033[38;2;76;86;106m",   "\033[38;2;136;192;208m", "\033[38;2;236;239;244m", "\033[38;2;129;161;193m", "\033[48;2;67;76;94m",     "\033[38;2;236;239;244m", "\033[38;2;163;190;140m","\033[38;2;94;129;100m"},
        {"\033[38;2;143;188;187m", "\033[38;2;94;129;172m",  "\033[38;2;46;52;64m",    "\033[38;2;136;192;208m", "\033[48;2;216;222;233m", "\033[38;2;46;52;64m",    "\033[38;2;80;140;90m",  "\033[38;2;40;80;50m"}
    },
    {
        "Dracula",
        {"\033[38;2;98;114;164m",  "\033[38;2;255;121;198m", "\033[38;2;248;248;242m", "\033[38;2;189;147;249m", "\033[48;2;68;71;90m",     "\033[38;2;248;248;242m", "\033[38;2;80;250;123m", "\033[38;2;40;150;70m"},
        {"\033[38;2;140;140;160m", "\033[38;2;180;50;140m",  "\033[38;2;40;42;54m",    "\033[38;2;120;70;200m",  "\033[48;2;220;220;235m", "\033[38;2;40;42;54m",    "\033[38;2;30;160;60m",  "\033[38;2;15cache;80;30m"}
    },
    {
        "TokyoNight",
        {"\033[38;2;86;95;137m",   "\033[38;2;187;154;247m", "\033[38;2;190;205;251m", "\033[38;2;122;162;247m", "\033[48;2;40;44;70m",     "\033[38;2;220;230;255m", "\033[38;2;158;206;106m","\033[38;2;80;130;60m"},
        {"\033[38;2;120;130;160m", "\033[38;2;100;60;170m",  "\033[38;2;30;35;60m",    "\033[38;2;40;80;180m",   "\033[48;2;210;220;245m",  "\033[38;2;30;35;60m",    "\033[38;2;60;140;40m",  "\033[38;2;30;80;20m"}
    }
};

int current_theme = 0;
ViewMode current_mode = KANBAN;
std::vector<Task> tasks;
std::string search_query = "";
int selected_task_id = -1;
int next_id = 1;

KeyConfig key_cfg;
CustomColors custom_colors;
struct termios orig_termios;

/* UTILITY FUNCTIONS */

std::string get_exe_dir() {
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count != -1) {
        path[count] = '\0';
        return std::string(dirname(path)) + "/";
    }
    return "./";
}

std::string repeat_str(const std::string& str, int times) {
    if (times <= 0) return "";
    std::string result = "";
    for (int i = 0; i < times; ++i) result += str;
    return result;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string parse_escapes(const std::string& input) {
    std::string res;
    res.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 3 < input.size() && input[i+1] == '0' && input[i+2] == '3' && input[i+3] == '3') {
            res += '\033';
            i += 3;
        } else if (input[i] == '\\' && i + 1 < input.size() && input[i+1] == 'e') {
            res += '\033';
            i += 1;
        } else {
            res += input[i];
        }
    }
    return res;
}

/* STORAGE & CONFIGURATION */

void save_config() {
    std::ofstream file(get_exe_dir() + "todo_config.txt");
    file << "theme=" << current_theme << "\n";
    file << "mode=" << (int)current_mode << "\n";
    file << "key_up=" << key_cfg.key_up << "\n";
    file << "key_down=" << key_cfg.key_down << "\n";
    file << "key_left=" << key_cfg.key_left << "\n";
    file << "key_right=" << key_cfg.key_right << "\n";
    file << "key_move_left=" << key_cfg.key_move_left << "\n";
    file << "key_move_right=" << key_cfg.key_move_right << "\n";
    file << "key_delete=" << key_cfg.key_delete << "\n";
    file << "key_toggle_mode=" << key_cfg.key_toggle_mode << "\n";
    file << "key_quit=" << key_cfg.key_quit << "\n";
    file << "todo_color_dark=" << custom_colors.todo_dark << "\n";
    file << "todo_color_light=" << custom_colors.todo_light << "\n";
    file << "in_prog_color_dark=" << custom_colors.in_prog_dark << "\n";
    file << "in_prog_color_light=" << custom_colors.in_prog_light << "\n";
    file << "done_color_dark=" << custom_colors.done_dark << "\n";
    file << "done_color_light=" << custom_colors.done_light << "\n";
}

void load_config() {
    std::ifstream file(get_exe_dir() + "todo_config.txt");
    if (!file.is_open()) {
        save_config();
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "theme") current_theme = std::stoi(val);
        else if (key == "mode") current_mode = (ViewMode)std::stoi(val);
        else if (key == "key_up") key_cfg.key_up = val;
        else if (key == "key_down") key_cfg.key_down = val;
        else if (key == "key_left") key_cfg.key_left = val;
        else if (key == "key_right") key_cfg.key_right = val;
        else if (key == "key_move_left") key_cfg.key_move_left = val;
        else if (key == "key_move_right") key_cfg.key_move_right = val;
        else if (key == "key_delete") key_cfg.key_delete = val;
        else if (key == "key_toggle_mode") key_cfg.key_toggle_mode = val;
        else if (key == "key_quit") key_cfg.key_quit = val;
        else if (key == "todo_color_dark") custom_colors.todo_dark = parse_escapes(val);
        else if (key == "todo_color_light") custom_colors.todo_light = parse_escapes(val);
        else if (key == "in_prog_color_dark") custom_colors.in_prog_dark = parse_escapes(val);
        else if (key == "in_prog_color_light") custom_colors.in_prog_light = parse_escapes(val);
        else if (key == "done_color_dark") custom_colors.done_dark = parse_escapes(val);
        else if (key == "done_color_light") custom_colors.done_light = parse_escapes(val);
    }
    if (current_theme < 0 || current_theme >= (int)themes.size()) current_theme = 0;
}

void save_data() {
    std::ofstream file(get_exe_dir() + "todo_data.txt");
    for (const auto& t : tasks) {
        file << t.id << "\t" << (int)t.status << "\t" << t.text << "\n";
    }
}

void load_data() {
    std::ifstream file(get_exe_dir() + "todo_data.txt");
    if (!file.is_open()) {
        tasks.push_back(Task{1, "Smile", TODO});
        tasks.push_back(Task{2, "Live a happy life", IN_PROGRESS});
        tasks.push_back(Task{3, "Install Nixos", DONE});
        next_id = 4;
        save_data();
        return;
    }
    tasks.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        int id, st;
        std::string text;
        ss >> id >> st;
        ss.ignore();
        std::getline(ss, text);
        tasks.push_back(Task{id, text, (Status)st});
        if (id >= next_id) next_id = id + 1;
    }
}

/* TERMINAL CONTROL */

void disable_raw_mode() {
    std::cout << "\033[?1000l\033[?1006l\033[?25h\033[?1049l\033[0m" << std::flush;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    std::cout << "\033[?1049h\033[?1000h\033[?1006h\033[?25l" << std::flush;
}

bool is_terminal_dark() {
    std::cout << "\033]11;?\007" << std::flush;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    struct timeval timeout = {0, 30000};

    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0) {
        std::string resp = "";
        char buf;
        while (read(STDIN_FILENO, &buf, 1) > 0) {
            resp += buf;
            if (buf == '\007' || buf == '\\') break;
        }
        int r, g, b;
        if (sscanf(resp.c_str(), "\033]11;rgb:%x/%x/%x", &r, &g, &b) == 3) {
            int luminance = (r * 299 + g * 587 + b * 114) / 1000;
            return luminance < 0x8000;
        }
    }
    return true;
}

Palette get_active_palette() {
    bool dark = is_terminal_dark();
    Palette p = dark ? themes[current_theme].dark_palette : themes[current_theme].light_palette;

    if (dark) {
        if (!custom_colors.todo_dark.empty()) p.todo_color = custom_colors.todo_dark;
        if (!custom_colors.in_prog_dark.empty()) p.in_prog_color = custom_colors.in_prog_dark;
        if (!custom_colors.done_dark.empty()) p.done_color = custom_colors.done_dark;
    } else {
        if (!custom_colors.todo_light.empty()) p.todo_color = custom_colors.todo_light;
        if (!custom_colors.in_prog_light.empty()) p.in_prog_color = custom_colors.in_prog_light;
        if (!custom_colors.done_light.empty()) p.done_color = custom_colors.done_light;
    }

    return p;
}

/* DATA RENDERING & FILTERING */

std::vector<Task> get_filtered_tasks(int status_filter = -1) {
    std::vector<Task> res;
    for (const auto& task : tasks) {
        if (status_filter != -1 && (int)task.status != status_filter) continue;
        if (!search_query.empty()) {
            std::string stext = task.text, squery = search_query;
            std::transform(stext.begin(), stext.end(), stext.begin(), ::tolower);
            std::transform(squery.begin(), squery.end(), squery.begin(), ::tolower);
            if (stext.find(squery) == std::string::npos) continue;
        }
        res.push_back(task);
    }
    return res;
}

void draw_ui() {
    winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int cols = w.ws_col;
    int rows = w.ws_row;

    Palette t = get_active_palette();

    int pad_x = 4;
    int pad_y = 1;
    int usable_width = cols - (pad_x * 2);
    int usable_height = rows - (pad_y * 2);

    int search_pad = 4; 
    int search_start_x = pad_x + search_pad;
    int search_width = usable_width - (search_pad * 2);

    std::stringstream ss;
    ss << "\033[2J\033[H"; 

    // Search bar
    ss << "\033[" << (pad_y + 1) << ";" << search_start_x << "H" 
       << t.active_border << "╭" << repeat_str("─", search_width - 2) << "╮\033[0m";
    
    std::string input_line = " 󰍉 " + search_query + "│";
    ss << "\033[" << (pad_y + 2) << ";" << search_start_x << "H" 
       << t.active_border << "│\033[0m" << t.accent << input_line << "\033[0m";

    std::string btn_add = "󰐕";
    std::string btn_mode = (current_mode == KANBAN) ? "󰕪" : "󰕶"; 
    std::string btn_theme = "󰌁";

    int right_search_edge = search_start_x + search_width - 1;
    int theme_x = right_search_edge - 3;
    int mode_x = theme_x - 3;
    int add_x = mode_x - 3;

    ss << "\033[" << (pad_y + 2) << ";" << add_x << "H" << t.accent << btn_add << "\033[0m";
    ss << "\033[" << (pad_y + 2) << ";" << mode_x << "H" << t.accent << btn_mode << "\033[0m";
    ss << "\033[" << (pad_y + 2) << ";" << theme_x << "H" << t.accent << btn_theme << "\033[0m";
    ss << "\033[" << (pad_y + 2) << ";" << right_search_edge << "H" << t.active_border << "│\033[0m";

    ss << "\033[" << (pad_y + 3) << ";" << search_start_x << "H" 
       << t.active_border << "╰" << repeat_str("─", search_width - 2) << "╯\033[0m";

    int board_top = pad_y + 5;
    int board_height = usable_height - 6;

    // View: KANBAN
    if (current_mode == KANBAN) {
        int col_width = (usable_width - 4) / 3;
        std::vector<std::string> headers = {" To Do ", " In Progress ", " Done "};
        std::vector<std::string> header_colors = {t.todo_color, t.in_prog_color, t.done_color};

        for (int c = 0; c < 3; ++c) {
            int start_x = pad_x + c * (col_width + 2);
            int end_x = start_x + col_width - 1;

            ss << "\033[" << board_top << ";" << start_x << "H" << t.border << "╭" << repeat_str("─", col_width - 2) << "╮\033[0m";
            
            ss << "\033[" << (board_top + 1) << ";" << start_x << "H" << t.border << "│\033[0m";
            std::string h = headers[c];
            int p = (col_width - 2 - (int)h.length()) / 2;
            ss << std::string(p, ' ') << header_colors[c] << h << "\033[0m" << std::string(col_width - 2 - (int)h.length() - p, ' ');
            ss << "\033[" << (board_top + 1) << ";" << end_x << "H" << t.border << "│\033[0m";

            ss << "\033[" << (board_top + 2) << ";" << start_x << "H" << t.border << "├" << repeat_str("─", col_width - 2) << "┤\033[0m";

            int curr_y = board_top + 3;
            auto filtered = get_filtered_tasks(c);

            for (auto& task : filtered) {
                if (curr_y >= board_top + board_height - 1) break;

                ss << "\033[" << curr_y << ";" << start_x << "H" << t.border << "│\033[0m";

                bool is_sel = (task.id == selected_task_id);
                if (is_sel) ss << t.selected_bg;
                
                std::string item_color = (task.status == TODO) ? t.todo_color : (task.status == IN_PROGRESS ? t.in_prog_color : t.done_color);
                ss << item_color;

                std::string label = " " + std::to_string(task.id) + ". " + task.text;
                if ((int)label.length() > col_width - 3) label = label.substr(0, col_width - 6) + "...";
                label.resize(col_width - 2, ' ');
                ss << label << "\033[0m";

                ss << "\033[" << curr_y << ";" << end_x << "H" << t.border << "│\033[0m";
                curr_y++;
            }

            for (; curr_y < board_top + board_height - 1; ++curr_y) {
                ss << "\033[" << curr_y << ";" << start_x << "H" << t.border << "│\033[0m" 
                   << std::string(col_width - 2, ' ') 
                   << "\033[" << curr_y << ";" << end_x << "H" << t.border << "│\033[0m";
            }

            ss << "\033[" << (board_top + board_height - 1) << ";" << start_x << "H" << t.border << "╰" << repeat_str("─", col_width - 2) << "╯\033[0m";
        }
    } 
    // View: LIST
    else {
        int list_width = (usable_width - 4) / 3 + 12;
        int list_start_x = pad_x + (usable_width - list_width) / 2;
        int list_end_x = list_start_x + list_width - 1;

        ss << "\033[" << board_top << ";" << list_start_x << "H" << t.border << "╭" << repeat_str("─", list_width - 2) << "╮\033[0m";
        int curr_y = board_top + 1;

        auto filtered = get_filtered_tasks();

        for (auto& task : filtered) {
            if (curr_y >= board_top + board_height - 1) break;

            ss << "\033[" << curr_y << ";" << list_start_x << "H" << t.border << "│\033[0m";

            std::string symbol = "󰄱";
            std::string color = t.todo_color;
            if (task.status == IN_PROGRESS) { symbol = "󰡖"; color = t.in_prog_color; }
            else if (task.status == DONE) { symbol = "󰄵"; color = t.done_color; }

            bool is_sel = (task.id == selected_task_id);
            if (is_sel) ss << t.selected_bg;

            ss << " " << color << symbol << "\033[0m ";
            if (is_sel) ss << t.selected_bg << color; else ss << color;

            std::string label = std::to_string(task.id) + ". " + task.text;
            if ((int)label.length() > list_width - 10) label = label.substr(0, list_width - 13) + "...";
            label.resize(list_width - 8, ' ');
            ss << label << "\033[0m";

            ss << "\033[" << curr_y << ";" << list_end_x << "H" << t.border << "│\033[0m";
            curr_y++;
        }

        for (; curr_y < board_top + board_height - 1; ++curr_y) {
            ss << "\033[" << curr_y << ";" << list_start_x << "H" << t.border << "│\033[0m" 
               << std::string(list_width - 2, ' ') 
               << "\033[" << curr_y << ";" << list_end_x << "H" << t.border << "│\033[0m";
        }

        ss << "\033[" << (board_top + board_height - 1) << ";" << list_start_x << "H" << t.border << "╰" << repeat_str("─", list_width - 2) << "╯\033[0m";
    }

    // Status bar
    int bottom_y = rows - pad_y;
    ss << "\033[" << bottom_y << ";" << pad_x << "H\033[0m";
    if (selected_task_id != -1) {
        ss << t.accent << "ACTIONS: " << "\033[0m"
           << t.selected_bg << " [Shift+◄/► Move Status] " << "\033[0m "
           << t.selected_bg << " [◄/►/▲/▼ Navigate] " << "\033[0m "
           << t.selected_bg << " [✕ Del (" << key_cfg.key_delete << ")] " << "\033[0m ";
    } else {
        std::string bottom_str = "Arrows: Navigate | Shift+Arrows: Move | Tab: View | Esc: Quit | Theme: " + themes[current_theme].name;
        ss << t.border << bottom_str << "\033[0m";
    }

    std::cout << ss.str() << std::flush;
}

/* EVENT HANDLING & NAVIGATION */

void handle_click(int x, int y) {
    winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int cols = w.ws_col;
    int rows = w.ws_row;
    int pad_x = 4;
    int pad_y = 1;
    int usable_width = cols - (pad_x * 2);

    int search_pad = 4;
    int search_start_x = pad_x + search_pad;
    int search_width = usable_width - (search_pad * 2);

    int right_search_edge = search_start_x + search_width - 1;

    int theme_x = right_search_edge - 3;
    int mode_x = theme_x - 3;
    int add_x = mode_x - 3;

    if (y == pad_y + 2) {
        if (x >= theme_x && x < right_search_edge) {
            current_theme = (current_theme + 1) % themes.size();
            save_config();
            return;
        }
        if (x >= mode_x && x < theme_x) {
            current_mode = (current_mode == KANBAN) ? LIST : KANBAN;
            save_config();
            return;
        }
        if (x >= add_x && x < mode_x) {
            if (!search_query.empty()) {
                tasks.push_back(Task{next_id++, search_query, TODO});
                search_query = "";
                save_data();
            }
            return;
        }
    }

    int board_top = pad_y + 5;
    bool clicked_any_task = false;

    if (current_mode == KANBAN) {
        int col_width = (usable_width - 4) / 3;
        for (int c = 0; c < 3; ++c) {
            int start_x = pad_x + c * (col_width + 2);
            if (x >= start_x && x <= start_x + col_width) {
                int curr_y = board_top + 3;
                auto filtered = get_filtered_tasks(c);
                for (auto& task : filtered) {
                    if (y == curr_y) {
                        selected_task_id = task.id;
                        clicked_any_task = true;
                        return;
                    }
                    curr_y++;
                }
            }
        }
    } else {
        int list_width = (usable_width - 4) / 3 + 12;
        int list_start_x = pad_x + (usable_width - list_width) / 2;

        if (x >= list_start_x && x <= list_start_x + list_width) {
            int curr_y = board_top + 1;
            auto filtered = get_filtered_tasks();
            for (auto& task : filtered) {
                if (y == curr_y) {
                    selected_task_id = task.id;
                    clicked_any_task = true;
                    return;
                }
                curr_y++;
            }
        }
    }

    if (!clicked_any_task) {
        selected_task_id = -1;
    }
}

void move_selection_vertical(int direction) {
    int current_status_filter = -1;
    if (current_mode == KANBAN && selected_task_id != -1) {
        auto it = std::find_if(tasks.begin(), tasks.end(), [](const Task& t){ return t.id == selected_task_id; });
        if (it != tasks.end()) current_status_filter = (int)it->status;
    }

    auto visible_tasks = get_filtered_tasks(current_status_filter);
    if (visible_tasks.empty()) return;

    if (selected_task_id == -1) {
        selected_task_id = (direction == 1) ? visible_tasks.front().id : visible_tasks.back().id;
        return;
    }

    int current_index = -1;
    for (size_t i = 0; i < visible_tasks.size(); ++i) {
        if (visible_tasks[i].id == selected_task_id) {
            current_index = (int)i;
            break;
        }
    }

    if (current_index == -1) {
        selected_task_id = visible_tasks.front().id;
    } else {
        int new_index = current_index + direction;
        if (new_index >= 0 && new_index < (int)visible_tasks.size()) {
            selected_task_id = visible_tasks[new_index].id;
        }
    }
}

void move_selection_horizontal(int direction) {
    if (current_mode != KANBAN) return;

    int current_status = 0;
    if (selected_task_id != -1) {
        auto it = std::find_if(tasks.begin(), tasks.end(), [](const Task& t){ return t.id == selected_task_id; });
        if (it != tasks.end()) current_status = (int)it->status;
    }

    int next_status = current_status + direction;
    if (next_status < 0 || next_status > 2) return;

    auto target_column_tasks = get_filtered_tasks(next_status);
    if (!target_column_tasks.empty()) {
        selected_task_id = target_column_tasks.front().id;
    }
}

void change_task_status(int delta) {
    if (selected_task_id == -1) return;
    auto it = std::find_if(tasks.begin(), tasks.end(), [](const Task& t){ return t.id == selected_task_id; });
    if (it != tasks.end()) {
        int new_st = (int)it->status + delta;
        if (new_st >= 0 && new_st <= 2) {
            it->status = (Status)new_st;
            save_data();
        }
    }
}

/* MAIN ENTRY POINT */

int main() {
    load_config();
    load_data();
    enable_raw_mode();

    while (true) {
        draw_ui();

        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) continue;

        /* ESCAPE SEQUENCES */
        if (c == 27) { 
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            struct timeval tv = {0, 10000}; 

            if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) == 0) {
                if (key_cfg.key_quit == "ESC") break;
            }

            char seq[5];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) break;
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
                
                // Mouse input
                if (seq[1] == '<') { 
                    std::string mouse_data = "";
                    char m;
                    while (read(STDIN_FILENO, &m, 1) > 0) {
                        mouse_data += m;
                        if (m == 'm' || m == 'M') break;
                    }
                    if (mouse_data.back() == 'M') {
                        int btn, x, y;
                        sscanf(mouse_data.c_str(), "%d;%d;%d", &btn, &x, &y);
                        handle_click(x, y);
                    }
                } 
                // Navigation
                else if (seq[1] == 'A') { move_selection_vertical(-1); } 
                else if (seq[1] == 'B') { move_selection_vertical(1); }  
                else if (seq[1] == 'D') { move_selection_horizontal(-1); } 
                else if (seq[1] == 'C') { move_selection_horizontal(1); }  
                
                // Shift + Navigation
                else if (seq[1] == '1') {
                    char extra[3];
                    if (read(STDIN_FILENO, &extra[0], 3) == 3) {
                        if (extra[0] == ';' && extra[1] == '2') {
                            if (extra[2] == 'D') change_task_status(-1); 
                            if (extra[2] == 'C') change_task_status(1);  
                        }
                    }
                }
            }
        } 
        /* KEYBOARD INPUTS */
        else if (c == '\t') { 
            if (key_cfg.key_toggle_mode == "TAB") {
                current_mode = (current_mode == KANBAN) ? LIST : KANBAN;
                save_config();
            }
        } else if (std::string(1, c) == key_cfg.key_delete && selected_task_id != -1) {
            tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const Task& t){ return t.id == selected_task_id; }), tasks.end());
            selected_task_id = -1;
            save_data();
        } else if (c == 127 || c == 8) { 
            if (!search_query.empty()) search_query.pop_back();
        } else if (c == '\n' || c == '\r') { 
            if (!search_query.empty()) {
                tasks.push_back(Task{next_id++, search_query, TODO});
                search_query = "";
                save_data();
            }
        } else if (isprint(c)) {
            search_query += c;
        }
    }

    return 0;
}
