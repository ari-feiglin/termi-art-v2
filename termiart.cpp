#include <iostream>
#include <print>
#include <vector>
#include <algorithm>
#include <set>
#include <cmath>
#include <termios.h>
#include <unistd.h>
#include <sstream>
#include <csignal>
#include <fstream>
#include <sys/ioctl.h>
#include <unistd.h>

std::string version_no = "v0.0.1";

typedef unsigned char uchar;
typedef unsigned int uint;

template <typename T>
struct Point {
    T x;
    T y;

    Point(T x, T y) : x{x}, y{y} {}
    Point(const Point& p) : x{p.x}, y{p.y} {}
    Point(Point&& p) : x{p.x}, y{p.y} {}
    Point& operator=(const Point& p) {
        x = p.x;
        y = p.y;
        return *this;
    }
    Point& operator=(Point&& p) {
        x = std::move(p.x);
        y = std::move(p.y);
        return *this;
    }
    Point operator*(T t) {
        return Point(x * t, y * t);
    }
    Point operator+(Point p) {
        return Point(x + p.x, y + p.y);
    }
    Point& operator+=(Point p) {
        x += p.x;
        y += p.y;
        return *this;
    }
    bool operator<(const Point p) const {
        if (x < p.x) return true;
        else if (p.x < x) return false;
        else if (y < p.y) return true;
        return false;
    }
    bool operator==(Point p) {
        return p.x == x && p.y == y;
    }
    bool operator!=(Point p) {
        return !(p == *this);
    }
    double distance(Point p) {
        T dx = p.x - x;
        T dy = p.y - y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

enum PixelCode { NONE = 0, BOUNDARY = 1, TEMP = 2 };

struct Pixel {
    static Pixel white;
    static Pixel black;
    static Pixel red;
    static Pixel blue;
    static Pixel green;
    static Pixel temp;

    uchar r;
    uchar g;
    uchar b;

    PixelCode code;
    uchar fg_r;
    uchar fg_g;
    uchar fg_b;
    std::string text;

    Pixel() : r{255}, g{255}, b{255}, code{NONE}, text{"  "} {}
    Pixel(uchar r, uchar g, uchar b, uchar fg_r = 0, uchar fg_g = 0, uchar fg_b = 0, std::string text = "  ", PixelCode code = NONE) : r{r}, g{g}, b{b}, fg_r{fg_r}, fg_g{fg_g}, fg_b{fg_b}, code{code}, text{text} {}
    Pixel(const Pixel& c) : r{c.r}, g{c.g}, b{c.b}, fg_r{c.fg_r}, fg_g{c.fg_g}, fg_b{c.fg_b}, code{c.code}, text{c.text} {}
    Pixel(Pixel&& c) : r{c.r}, g{c.g}, b{c.b}, fg_r{c.fg_r}, fg_g{c.fg_g}, fg_b{c.fg_b}, code{c.code}, text{c.text} {}
    Pixel& operator=(const Pixel& c) {
        if (c.code == TEMP) {
            code = TEMP;
            return *this;
        }
        r = c.r;
        g = c.g;
        b = c.b;
        code = c.code;
        fg_r = c.fg_r;
        fg_g = c.fg_g;
        fg_b = c.fg_b;
        text = c.text;
        return *this;
    }
    Pixel& operator=(Pixel&& c) {
        if (c.code == TEMP) {
            code = TEMP;
            return *this;
        }
        r = std::move(c.r);
        g = std::move(c.g);
        b = std::move(c.b);
        code = std::move(c.code);
        fg_r = std::move(c.fg_r);
        fg_g = std::move(c.fg_g);
        fg_b = std::move(c.fg_b);
        text = std::move(c.text);
        return *this;
    }

    Pixel get_reverse() const {
        return Pixel(255 - r, 255 - g, 255 - b);
    }

    Pixel& reverse() {
        r = 255 - r;
        g = 255 - g;
        b = 255 - b;
        return *this;
    }

    Pixel& set_code(PixelCode code) {
        this->code = code;
        return *this;
    }

    std::string bg() const {
        return std::format("\033[48;2;{};{};{}m", r, g, b);
    }

    std::string fg() const {
        return std::format("\033[38;2;{};{};{}m", r, g, b);
    }
};

Pixel Pixel::white(255,255,255);
Pixel Pixel::black(0,0,0);
Pixel Pixel::red(255,0,0);
Pixel Pixel::green(0,255,0);
Pixel Pixel::blue(0,0,255);
Pixel Pixel::temp(0,0,0,0,0,0,"  ",TEMP);

class Canvas {
    private:
        struct CanvasHolder {
            Pixel* canvas;
            uint width;
            uint height;

            CanvasHolder(Pixel* canvas, uint width, uint height) : canvas{canvas}, width{width}, height{height} {}
        };

        Pixel* canvas;
        std::vector<CanvasHolder> past_canvases;
        std::set<uint> update_lines;
        std::set<Point<Point<uint>>> boundary_points;
        uint width;
        uint height;

        void check_point(Point<uint> p) {
            if (p.x >= width || p.y >= height) throw std::invalid_argument(std::format("Invalid point ({},{}) in dimensions {}x{}", p.x, p.y, width, height).c_str());
        }

        bool intersects(Point<uint> p1, Point<uint> p2, Point<uint> p3, Point<uint> p4) {
            int x1 = p1.x;
            int x2 = p2.x;
            int x3 = p3.x;
            int x4 = p4.x;
            int y1 = p1.y;
            int y2 = p2.y;
            int y3 = p3.y;
            int y4 = p4.y;
            int a = x1 - x2;
            int b = x4 - x3;
            int c = y1 - y2;
            int d = y4 - y3;
            int e = x4 - x2;
            int f = y4 - y2;
            int det = a * d - b * c;

            if (det != 0) { // Matrix is invertible; single solution
                int t = d * e - b * f;
                int s = a * f - c * e;

                if (det > 0) {
                    return 0 <= t && t <= det && 0 <= s && s <= det;
                } else {
                    return det <= t && t <= 0 && det <= s && s <= 0;
                }
            } else { //  Matrix is singular
                if (a != 0) {
                    double alpha = (double)b / a;
                    if (a * f != e * c) return false; // No solutions
                    if (alpha > 0) {
                        return std::max(0., (double)e / a - alpha) <= std::min(1., (double)e / a);
                    } else {
                        return std::max(0., (double)e / a) <= std::min(1., (double)e / a - alpha);
                    }
                } else {
                    if (d != 0) {
                        double alpha = (double)c / d;
                        if (b != 0) {
                            double s = (double)e / b;
                            double t = ((double)f / d - s) / alpha;
                            return 0 <= s && s <= 1 && 0 <= t && t <= 1;
                        } else {
                            if (e != 0) return false;
                            if (alpha > 0) {
                                return std::max(0., (double)f / d - alpha) <= std::min(1., (double)f / d);
                            } else {
                                return std::max(0., (double)f / d) <= std::min(1., (double)f / d - alpha);
                            }
                        }
                    } else {
                        if (e != 0 || f != 0) return false;
                        return true;
                    }
                }
            }
        }

        bool in_area(Point<uint> p, Point<uint> center) {
            uint count = 0;

            for (const Point<Point<uint>>& b : boundary_points) {
                if (intersects(b.x, b.y, p, center)) count++;
            }

            return count % 2 == 0;
        }

        void reset_temp() {
            for (int i = 0; i < width; i++) {
                for (int j = 0; j < height; j++) {
                    Pixel& c = canvas[j * width + i];
                    if (c.code == TEMP) {
                        c.code = NONE;
                        update_lines.insert(j);
                    }
                }
            }
        }

    public:
        Canvas(uint width, uint height, Pixel bg = Pixel::white) : width{width}, height{height} {
            canvas = new Pixel[width * height];
            for (int i = 0; i < width * height; i++) canvas[i] = bg;
            for (int i = 0; i < height; i++) update_lines.insert(i);
        }

        Canvas(std::string filename) {
            std::ifstream input_file;
            input_file.open(filename, std::ios::binary | std::ios::in);

            std::string vno;
            char c;

            while (true) {
                input_file >> c;
                if (c != 0) {
                    vno += c;
                } else break;
            }

            if (vno != version_no) throw std::invalid_argument(std::format("Invalid version: current {} vs {}", version_no, vno).c_str());

            input_file.read(reinterpret_cast<char*>(&width), sizeof(width));
            input_file.read(reinterpret_cast<char*>(&height), sizeof(height));
            canvas = new Pixel[width * height];

            uchar r,g,b,fg_r,fg_g,fg_b;
            char buf[3] = {0};

            for (int i = 0; i < width * height; i++) {
                input_file.read(reinterpret_cast<char*>(&r), 1);
                input_file.read(reinterpret_cast<char*>(&g), 1);
                input_file.read(reinterpret_cast<char*>(&b), 1);
                input_file.read(reinterpret_cast<char*>(&fg_r), 1);
                input_file.read(reinterpret_cast<char*>(&fg_g), 1);
                input_file.read(reinterpret_cast<char*>(&fg_b), 1);
                input_file.read(buf, 2);
                canvas[i] = Pixel(r,g,b,fg_r,fg_g,fg_b,std::string(buf));
            }

            for (int i = 0; i < height; i++) update_lines.insert(i);
        }

        ~Canvas() {
            delete[] canvas;
            for (int i = 0; i < past_canvases.size(); i++) delete[] past_canvases[i].canvas;
        }

        uint get_width() { return width; }
        uint get_height() { return height; }

        void save(std::string file) {
            std::ofstream output_file;
            output_file.open(file, std::ios::binary | std::ios::out | std::ios::trunc);

            output_file << version_no << '\0';

            output_file.write(reinterpret_cast<char*>(&width), sizeof(width));
            output_file.write(reinterpret_cast<char*>(&height), sizeof(height));

            for (int i = 0; i < width * height; i++) {
                output_file << canvas[i].r << canvas[i].g << canvas[i].b << canvas[i].fg_r << canvas[i].fg_g << canvas[i].fg_b;
                output_file.write(canvas[i].text.c_str(), 2);
            }

            output_file.close();
        }

        void save_old() {
            Pixel* old_canvas = new Pixel[width * height];
            for (int i = 0; i < width * height; i++) old_canvas[i] = canvas[i];
            past_canvases.push_back(CanvasHolder(old_canvas, width, height));
        }

        void undo(int times = 1) {
            bool flag = false;
            CanvasHolder holder(NULL, 0,0);

            for (int i = 0; i < height; i++) update_lines.insert(i);

            for (int i = 0; i < times; i++) {
                if (past_canvases.empty()) break;
                flag = true;
                delete[] canvas;
                holder = past_canvases.back();
                past_canvases.pop_back();
            }

            if (!flag) return;

            width = holder.width;
            height = holder.height;
            canvas = holder.canvas;

        }

        void resize(uint width, uint height) {
            save_old();
            Pixel* new_canvas = new Pixel[width * height]();

            update_lines.clear();

            for (int i = 0; i < std::min(height, this->height); i++) {
                for (int j = 0; j < std::min(width, this->width); j++)
                    new_canvas[i * width + j] = canvas[i * this->width + j];
            }

            for (int i = 0; i < height; i++) update_lines.insert(i);

            delete[] canvas;
            canvas = new_canvas;

            this->width = width;
            this->height = height;
        }

        const Pixel& operator[](uint i, uint j) { return canvas[j * width + i]; }

        void update_line(uint i) { update_lines.insert(i); }

        void point(Pixel c, Point<uint> p) {
            check_point(p);
            save_old();
            canvas[p.y * width + p.x] = c;
            update_lines.insert(p.y);
        }

        void fill_area(Point<uint> start, Point<uint> end, Pixel c) {
            check_point(start);
            check_point(end);

            save_old();

            Point<uint> b = Point<uint>(std::min(start.x, end.x), std::min(start.y, end.y));
            Point<uint> e = Point<uint>(std::max(start.x, end.x), std::max(start.y, end.y));

            for (int i = b.y; i <= e.y; i++) {
                for (int j = b.x; j <= e.x; j++)
                    canvas[i * width + j] = c;
                update_lines.insert(i);
            }
        }

        void add_text(Point<uint> p, std::string text, uchar r, uchar g, uchar b) {
            for (int i = p.x, j = 0; i < width && j < text.length(); i++, j += 2) {
                Pixel& c = canvas[p.y * width + i];
                c.text = std::format("{}{}", text[j], (j + 1 < text.length()) ? text[j+1] : ' ');
                c.fg_r = r;
                c.fg_g = g;
                c.fg_b = b;
            }
        }

        void draw() {
            for (const int& i : update_lines) {
                std::string line;
                for (int j = 0; j < width; j++) {
                    const Pixel& c = canvas[i * width + j];
                    Pixel r = c.get_reverse();
                    switch (c.code) {
                        case NONE: {
                            std::string t = c.text.substr(0,2);
                            if (t == "  ")
                                line += std::format("\033[48;2;{};{};{}m\033[38;2;{};{};{}m{}", c.r, c.g, c.b, r.r, r.g, r.b, t);
                            else
                                line += std::format("\033[48;2;{};{};{}m\033[38;2;{};{};{}m{}", c.r, c.g, c.b, c.fg_r, c.fg_g, c.fg_b, t);
                            break;
                        }
                        case BOUNDARY: line += std::format("\033[48;2;{};{};{}m\033[38;2;{};{};{}m::", c.r, c.g, c.b, r.r, r.g, r.b); break;
                        case TEMP: line += "\033[48;2;255;255;255m\033[38;0;0;0m##"; break;
                        default: line += "  "; break;
                    }
                }

                std::print("\033[{};{}H{}", i + 1, 1, line);
            }

            update_lines.clear();
            reset_temp();
        }

        void draw_boundary_line(Point<uint> start, Point<uint> end, uint fineness = 100) {
            check_point(start);
            check_point(end);

            save_old();

            double delta = 1 / (double)fineness;
            Point<double> b(start.x, start.y);
            Point<double> e(end.x, end.y);
            Point<double> d = Point((e.x - b.x), (e.y - b.y)) * delta;

            for (int i = 0; i <= fineness; i++) {
                Pixel& c = canvas[std::lround(b.y) * width + std::lround(b.x)];
                if (c.code != BOUNDARY) {
                    c.set_code(BOUNDARY);
                    update_lines.insert(std::lround(b.y));
                }
                b += d;
            }

            boundary_points.insert(Point(start, end));
        }

        void draw_line(Point<uint> start, Point<uint> end, Pixel c, uint fineness = 1000) {
            check_point(start);
            check_point(end);

            save_old();

            double delta = 1 / (double)fineness;
            Point<double> b(start.x, start.y);
            Point<double> e(end.x, end.y);
            Point<double> d = Point((e.x - b.x), (e.y - b.y)) * delta;

            for (int i = 0; i <= fineness; i++) {
                canvas[std::lround(b.y) * width + std::lround(b.x)] = c;
                update_lines.insert(std::lround(b.y));
                b += d;
            }

            if (c.code != TEMP) reset_temp();
        }

        void fill_area(Point<uint> p, Pixel c) {
            check_point(p);

            save_old();

            for (int i = 0; i < width; i++) {
                for (int j = 0; j < height; j++) {
                    Pixel& curr = canvas[j * width + i];
                    if (in_area(p, Point<uint>(i, j)) || curr.code == BOUNDARY) {
                        curr = c;
                        update_lines.insert(j);
                    }
                }
            }

            if (c.code != TEMP) reset_temp();
        }

        void draw_circle(Point<uint> p, uint r, Pixel c) {
            check_point(p);

            save_old();

            for (int i = 0; i < width; i++) {
                for (int j = 0; j < height; j++) {
                    int dx = i - p.x;
                    int dy = j - p.y;
                    int a = dx * dx + dy * dy;
                    int r2 = r * r;
                    if (r2 - r <= a && a <= r2 + r) {
                        canvas[j * width + i] = c;
                        update_lines.insert(j);
                    }
                }
            }

            if (c.code != TEMP) reset_temp();
        }

        void draw_ellipse(Point<uint> p, int r1, int r2, Pixel c) {
            if (r1 == 0 || r2 == 0) return;
            check_point(p);

            save_old();

            r1 = std::abs(r1);
            r2 = std::abs(r2);
            double r12 = r1 * r1;
            double r22 = r2 * r2;

            /*for (int i = 0; i < width; i++) {
                for (int j = 0; j < height; j++) {
                    int dx = i - p.x;
                    int dy = j - p.y;
                    int a = r2 * r2 * dx * dx + r1 * r1 * dy * dy;
                    int r = r1 * r2;
                    if (r * r - r * std::sqrt(r) <= a && a <= r * r + r * std::sqrt(r)) {
                        canvas[j * width + i] = c;
                        update_lines.insert(j);
                    }
                }
            }*/

            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    Point<double> diffs[] = {Point<double>(-.5,.5), Point<double>(.5,.5), Point<double>(.5,-.5), Point<double>(-.5,-.5)};
                    double vals[] = {0,0,0,0};
                    for (int k = 0; k < 4; k++) {
                        double dx = (double)j - p.x - diffs[k].x;
                        double dy = (double)i - p.y - diffs[k].y;
                        vals[k] = r22 * dx * dx + r12 * dy * dy;
                    }

                    int state = 0;
                    bool flag = false;
                    for (int k = 0; k < 4; k++) {
                        if (vals[k] == r12 * r22) {
                            flag = true;
                            break;
                        } else if (vals[k] < r12 * r22) {
                            if (state == 1) {
                                flag = true;
                                break;
                            }
                            state = -1;
                        } else if (vals[k] > r12 * r22) {
                            if (state == -1) {
                                flag = true;
                                break;
                            }
                            state = 1;
                        }
                    }

                    if (flag) {
                        canvas[i * width + j] = c;
                        update_lines.insert(i);
                    }
                }
            }

            if (c.code != TEMP) reset_temp();
        }

        void fill_ellipse(Point<uint> p, int r1, int r2, Pixel c) {
            if (r1 == 0 || r2 == 0) return;
            check_point(p);

            save_old();

            r1 = std::abs(r1);
            r2 = std::abs(r2);
            double r12 = r1 * r1;
            double r22 = r2 * r2;

            for (int i = 0; i < height; i++) {
                for (int j = 0; j <= p.x; j++) {
                    Point<double> diffs[] = {Point<double>(-.5,.5), Point<double>(.5,.5), Point<double>(.5,-.5), Point<double>(-.5,-.5)};
                    double vals[] = {0,0,0,0};
                    for (int k = 0; k < 4; k++) {
                        double dx = (double)j - p.x - diffs[k].x;
                        double dy = (double)i - p.y - diffs[k].y;
                        vals[k] = r22 * dx * dx + r12 * dy * dy;
                    }

                    int state = 0;
                    bool flag = false;
                    for (int k = 0; k < 4; k++) {
                        if (vals[k] == r12 * r22) {
                            flag = true;
                            break;
                        } else if (vals[k] < r12 * r22) {
                            if (state == 1) {
                                flag = true;
                                break;
                            }
                            state = -1;
                        } else if (vals[k] > r12 * r22) {
                            if (state == -1) {
                                flag = true;
                                break;
                            }
                            state = 1;
                        }
                    }

                    if (flag) {
                        for (int k = j; k <= 2 * p.x - j; k++)
                            canvas[i * width + k] = c;
                        update_lines.insert(i);
                    }
                }
            }

            if (c.code != TEMP) reset_temp();
        }

        void fill_circle(Point<uint> p, uint r, Pixel c) {
            check_point(p);

            save_old();

            for (int i = 0; i <= p.x; i++) {
                for (int j = 0; j < height; j++) {
                    int dx = i - p.x;
                    int dy = j - p.y;
                    int a = dx * dx + dy * dy;
                    int r2 = r * r;
                    if (r2 - r <= a && a <= r2 + r) {
                        for (int k = i; k <= 2 * p.x - i; k++) {
                            canvas[j * width + k] = c;
                            update_lines.insert(j);
                        }
                    }
                }
            }
        }
};

class Drawer;

struct Command {
    Command() {}
    virtual void execute(Drawer& d) =0;
};

struct QuitCommand : public Command {
    QuitCommand() {}
    void execute(Drawer& d) override;
};

struct HelpCommand : public Command {
    std::string fname;
    HelpCommand(std::string fname) : fname{fname} {}
    void execute(Drawer& d) override;
};

struct ResizeCommand : public Command {
    uint width;
    uint height;
    ResizeCommand(uint width, uint height) : width{width}, height{height} {}
    void execute(Drawer& d) override;
};

struct ScrollCommand : public Command {
    int dy;
    ScrollCommand(int dy) : dy{dy} {}
    void execute(Drawer& d) override;
};

struct OutputCommand : public Command {
    std::string output;
    OutputCommand(std::string output) : output{output} {}
    void execute(Drawer& d) override;
    std::string get_var(std::string varname, Drawer& d);
};

struct UndoCommand : public Command {
    int times;
    UndoCommand(int times = 1) : times{times} {}
    void execute(Drawer& d) override;
};

struct CursorCommand : public Command {
    int x;
    int y;
    CursorCommand(int x = -1, int y = -1) : x{x}, y{y} {}
    void execute(Drawer& d) override;
};

struct PixelChangeCommand : public Command {
    Pixel c;
    PixelChangeCommand(Pixel c) : c{c} {}
    void execute(Drawer& d) override;
};

struct AddTextCommand : public Command {
    std::string text;
    AddTextCommand(std::string text) : text{text} {}
    void execute(Drawer& d) override;
};

struct DrawLineCommand : public Command {
    Point<uint> b;
    Point<uint> e;
    DrawLineCommand(Point<uint> b, Point<uint> e) : b{b}, e{e} {}
    void execute(Drawer& d) override;
};

struct DrawBoundaryLineCommand : public Command {
    Point<uint> b;
    Point<uint> e;
    DrawBoundaryLineCommand(Point<uint> b, Point<uint> e) : b{b}, e{e} {}
    void execute(Drawer& d) override;
};

struct DrawCircleCommand : public Command {
    Point<uint> p;
    uint r;
    DrawCircleCommand(Point<uint> p, uint r) : p{p}, r{r} {}
    void execute(Drawer& d) override;
};

struct FillCircleCommand : public Command {
    Point<uint> p;
    uint r;
    FillCircleCommand(Point<uint> p, uint r) : p{p}, r{r} {}
    void execute(Drawer& d) override;
};

struct FillAreaCommand : public Command {
    Point<uint> p1;
    Point<uint> p2;
    FillAreaCommand(Point<uint> p1, Point<uint> p2) : p1{p1}, p2{p2} {}
    void execute(Drawer& d) override;
};

struct SaveCommand : public Command {
    std::string filename;
    SaveCommand(std::string filename) : filename{filename} {}
    void execute(Drawer& d) override;
};

std::vector<std::string> split_string_to_lines(std::string s, int length) {
    std::vector<std::string> lines;
    std::string curr = "";

    for (int i = 0, j = 0; i < s.length(); i++, j++) {
        if (s[i] == '\n') {
            lines.push_back(curr);
            j = 0;
            curr = "";
            continue;
        } else if (j == length) {
            lines.push_back(curr);
            j = 0;
            curr = "";
            if (s[i] == ' ') {
                j--;
                continue;
            }
        }
        curr += s[i];
    }

    if (curr != "") lines.push_back(curr);

    return lines;
}

std::vector<std::string> split_string(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}

class OutputTerminal {
    private:
        Point<uint> pos;
        Pixel bg;
        Pixel fg;
        uint width;
        uint height;
        std::vector<std::string> lines;
        int first_line;

    public:
        OutputTerminal(Point<uint> pos, uint width, uint height, Pixel bg, Pixel fg) : pos{pos}, width{width}, height{height}, bg{bg}, fg{fg}, first_line{0}
            {}

        void draw() {
            std::string line = std::string(width, ' ');

            std::print("{}{}", bg.bg(), fg.fg());
            for (int i = 1; i <= height; i++)
                std::print("\033[{};{}H{}", pos.y + i, pos.x + 1, line);
            for (int i = 0; i < std::min((uint)lines.size() - (uint)first_line, height - 2); i++)
                std::print("\033[{};{}H{}", pos.y + i + 2, pos.x + 2, lines[i + first_line]);
        }

        void draw(std::string output) {
            lines = split_string_to_lines(output, width - 2);
            draw();
        }

        void move(int dy) {
            first_line += dy;
            if (first_line < 0) first_line = 0;
            else if (first_line >= lines.size()) first_line = lines.size() - 1;
            draw();
        }
};

class Terminal {
    private:
        Point<uint> pos;
        Pixel bg;
        Pixel fg;
        uint width;
        uint height;
        std::string command;

        Command* to_command() {
            std::vector<std::string> strs = split_string(command, " ");

            if (strs[0] == "quit") {
                return new QuitCommand();
            } if (strs[0] == "help") {
                if (strs.size() > 1 && strs[1] == "terminal")
                    return new HelpCommand("help-terminal.txt");
                return new HelpCommand("help-editor.txt");
            } else if (strs[0] == "resize") {
                if (strs.size() < 3) return NULL;
                return new ResizeCommand(std::stoi(strs[1]), std::stoi(strs[2]));
            } else if (strs[0] == "scroll") {
                if (strs.size() < 2) return NULL;
                return new ScrollCommand(std::stoi(strs[1]));
            } if (strs[0] == "print") {
                return new OutputCommand(command.substr(6));
            } else if (strs[0] == "undo") {
                if (strs.size() > 1)
                    return new UndoCommand(std::stoi(strs[1]));
                return new UndoCommand();
            } else if (strs[0] == "cursor") {
                if (strs.size() == 1)
                    return new CursorCommand();
                else if (strs.size() == 3)
                    return new CursorCommand(std::stoi(strs[1]), std::stoi(strs[2]));
            } else if (strs[0] == "color") {
                if (strs.size() < 4) return NULL;
                try {
                    return new PixelChangeCommand(Pixel(std::stoi(strs[1]), std::stoi(strs[2]), std::stoi(strs[3])));
                } catch (std::invalid_argument e) {
                    return NULL;
                }
            } else if (strs[0] == "text") {
                return new AddTextCommand(command.substr(5));
            } else if (strs[0] == "draw") {
                if (strs.size() < 2) return NULL;
                if (strs[1] == "line") {
                    if (strs.size() < 6) return NULL;
                    return new DrawLineCommand(Point<uint>(std::stoi(strs[2]), std::stoi(strs[3])), Point<uint>(std::stoi(strs[4]), std::stoi(strs[5])));
                } else if (strs[1] == "circle") {
                    if (strs.size() < 5) return NULL;
                    return new DrawCircleCommand(Point<uint>(std::stoi(strs[2]), std::stoi(strs[3])), std::stoi(strs[4]));
                } else if (strs[1] == "boundary") {
                    if (strs.size() < 6) return NULL;
                    return new DrawBoundaryLineCommand(Point<uint>(std::stoi(strs[2]), std::stoi(strs[3])), Point<uint>(std::stoi(strs[4]), std::stoi(strs[5])));
                }
            } else if (strs[0] == "fill") {
                if (strs.size() < 2) return NULL;
                if (strs[1] == "circle") {
                    if (strs.size() < 5) return NULL;
                    return new FillCircleCommand(Point<uint>(std::stoi(strs[2]), std::stoi(strs[3])), std::stoi(strs[4]));
                } else if (strs[1] == "area") {
                    if (strs.size() < 6) return NULL;
                    return new FillAreaCommand(Point<uint>(std::stoi(strs[2]), std::stoi(strs[3])), Point<uint>(std::stoi(strs[4]), std::stoi(strs[5])));
                }
            } else if (strs[0] == "save") {
                if (strs.size() < 2) return NULL;
                return new SaveCommand(strs[1]);
            }
            return NULL;
        }

    public:
        Terminal(Point<uint> pos, uint width, uint height, Pixel bg, Pixel fg) : pos{pos}, width{width}, height{height}, bg{bg}, fg{fg}, command{"Type / and then enter \"help\" for help"}
            {}

        void draw() {
            std::vector<std::string> lines = split_string_to_lines(command, width - 2);
            std::string line = std::string(width, ' ');

            std::print("{}{}", bg.bg(), fg.fg());
            for (int i = 1; i <= height; i++)
                std::print("\033[{};{}H{}", pos.y + i, pos.x + 1, line);
            for (int i = 0; i < lines.size(); i++)
                std::print("\033[{};{}H{}", pos.y + i + 2, pos.x + 2, lines[i]);
        }

        void clear() { command = ""; }

        Command* main() {
            char c;

            while (true) {
                draw();

                std::cin.get(c);

                if (c == '\n') return to_command();
                else if (c == 127)
                    command = command.substr(0, command.length() - 1);
                else
                    command += c;
            }
        }
};

enum CursorType { BASIC, SPECIAL };

struct Cursor {
    Point<int> pos;
    CursorType type;
    uint width;
    uint height;

    Cursor(Point<int> pos, CursorType type, uint width, uint height) : pos(pos), type{type}, width{width}, height{height}
        {}

    std::string to_string() {
        switch (type) {
            case BASIC: return "[]";
            case SPECIAL: return "()";
            default: return "<>";
        }
    }

    void move_x(int dx) {
        pos.x += dx;
        if (pos.x >= (int)width) pos.x = width - 1;
        else if (pos.x < 0) pos.x = 0;
    }

    void move_y(int dy) {
        pos.y += dy;
        if (pos.y >= (int)height) pos.y = height - 1;
        else if (pos.y < 0) pos.y = 0;
    }

    Point<uint> get_pos() {
        return Point<uint>(pos.x, pos.y);
    }
};

enum Action { ACT_NONE, ACT_DRAW_LINE, ACT_DRAW_CIRCLE, ACT_DRAW_BOUNDARY, ACT_DRAW_ELLIPSE, ACT_FILL_CIRCLE, ACT_FILL_ELLIPSE, ACT_FILL_AREA };

class Drawer {
    private:
        static struct termios attributes;
        static Drawer* d;
        Terminal term;
        bool run;

        static void change_echo(bool on) {
            tcgetattr(STDIN_FILENO, &attributes);

            if (on) attributes.c_lflag |= ECHO | ICANON;
            else attributes.c_lflag &= ~(ECHO | ICANON);

            tcsetattr(STDIN_FILENO, TCSANOW, &attributes);
        }

        static void show_cursor(bool on) {
            if (on) std::cout << "\033[?25h" << std::flush;
            else std::cout << "\033[?25l" << std::flush;
        }

        static void sigint_handler(int signum) {
            d->out.draw("Input the \"quit\" command to quit");
        }

    public:
        Canvas canvas;
        Cursor cursor;
        Pixel curr_pixel;
        OutputTerminal out;
        int rows;
        int cols;

        Drawer(uint width, uint height) :
            canvas(width, height),
            cursor(Point<int>(0,0), BASIC, width, height),
            term(Point<uint>(2 * width + 2, 0),  20, 20, Pixel::black, Pixel::green),
            out(Point<uint>(2 * width + 2, 25), 20, 20, Pixel::black, Pixel::green), run{true}
        {
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            rows = w.ws_row;
            cols = w.ws_col;

            term = Terminal(Point<uint>(2 * width + 2, 0), cols - 2 * width - 2, height / 2, Pixel::black, Pixel::green);
            out = OutputTerminal(Point<uint>(2 * width + 2, height / 2), cols - 2 * width - 2, height - height / 2, Pixel::white, Pixel::black);
            cursor = Cursor(Point<int>(0,0), BASIC, width, height);
        }

        Drawer(std::string filename) :
            canvas(filename),
            cursor(Point<int>(0,0), BASIC, canvas.get_width(), canvas.get_height()),
            out(Point<uint>(2 * canvas.get_width() + 2, 25), 20, 20, Pixel::black, Pixel::green),
            term(Point<uint>(2 * canvas.get_width() + 2, 0), 20, 20, Pixel::black, Pixel::green),
            run{true}
        {
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            rows = w.ws_row;
            cols = w.ws_col;

            uint width = canvas.get_width();
            uint height = canvas.get_height();

            term = Terminal(Point<uint>(2 * width + 2, 0), cols - 2 * width - 2, height / 2, Pixel::black, Pixel::green);
            out = OutputTerminal(Point<uint>(2 * width + 2, height / 2), cols - 2 * width - 2, height - height / 2, Pixel::white, Pixel::black);
            cursor = Cursor(Point<int>(0,0), BASIC, width, height);
        }

        void resize(uint width, uint height) {
            canvas.resize(width, height);
            term = Terminal(Point<uint>(2 * width + 2, 0), cols - 2 * width - 2, height / 2, Pixel::black, Pixel::green);
            out = OutputTerminal(Point<uint>(2 * width + 2, height / 2), cols - 2 * width - 2, height - height / 2, Pixel::white, Pixel::black);
            cursor = Cursor(Point<int>(0,0), BASIC, width, height);
            draw_all();
        }

        void draw_all() {
            std::cout << "\033[2J\033[H" << std::flush;
            draw();
            out.draw();
            term.draw();
        }

        void draw() {
            canvas.draw();
            std::string line(cols, ' ');
            std::print("{}\033[{};1H{}\033[{};1H{}\033[{};1H{}", curr_pixel.bg(), canvas.get_height() + 2, line, canvas.get_height() + 3, line, canvas.get_height() + 4, line);
        }

        void quit() {
            out.draw("Goodbye!");
            run = false;
        }

        void undo(int times = 1) {
            canvas.undo(times);
            uint width = canvas.get_width();
            uint height = canvas.get_height();
            term = Terminal(Point<uint>(2 * width + 2, 0), cols - 2 * width - 2, height / 2, Pixel::black, Pixel::green);
            out = OutputTerminal(Point<uint>(2 * width + 2, height / 2), cols - 2 * width - 2, height - height / 2, Pixel::white, Pixel::black);
            cursor = Cursor(Point<int>(0,0), BASIC, width, height);
            draw_all();
        }

        void main() {
            Drawer::d = this;
            std::signal(SIGSEGV, Drawer::sigint_handler);
            std::signal(SIGINT, Drawer::sigint_handler);

            change_echo(false);
            show_cursor(false);
            std::cout << "\033[2J\033[H" << std::flush;

            char c;
            Action act = ACT_NONE;
            Point<uint> prev_point(0,0);

            out.draw("");
            term.draw();

            while (run) {
                switch (act) {
                    case ACT_NONE: break;
                    case ACT_DRAW_LINE:
                    case ACT_DRAW_BOUNDARY: canvas.draw_line(prev_point, cursor.get_pos(), Pixel::temp); break;
                    case ACT_FILL_CIRCLE:
                    case ACT_DRAW_CIRCLE: canvas.draw_circle(prev_point, std::roundl(prev_point.distance(cursor.get_pos())), Pixel::temp); break;
                    case ACT_FILL_ELLIPSE:
                    case ACT_DRAW_ELLIPSE: canvas.draw_ellipse(prev_point, cursor.get_pos().x - prev_point.x, cursor.get_pos().y - prev_point.y, Pixel::temp); break;
                    case ACT_FILL_AREA: {
                        uint x1 = prev_point.x;
                        uint x2 = cursor.get_pos().x;
                        uint y1 = prev_point.y;
                        uint y2 = cursor.get_pos().y;
                        canvas.draw_line(Point(x1,y1), Point(x2,y1), Pixel::temp);
                        canvas.draw_line(Point(x2,y1), Point(x2,y2), Pixel::temp);
                        canvas.draw_line(Point(x2,y2), Point(x1,y2), Pixel::temp);
                        canvas.draw_line(Point(x1,y2), Point(x1,y1), Pixel::temp);
                        break;
                    }
                }

                draw();

                const Pixel& on_color = canvas[cursor.pos.x, cursor.pos.y];
                Pixel cursor_color = Pixel::black;
                if (on_color.r + on_color.g + on_color.b < 383) cursor_color = Pixel::white;
                std::print("\033[{};{}H{}{}{}\033[0m", cursor.pos.y + 1, 2 * cursor.pos.x + 1, on_color.bg(), cursor_color.fg(), cursor.to_string());

                std::cin.get(c);

                switch (c) {
                    case 27: act = ACT_NONE; break;
                    case ' ': {
                        switch (act) {
                            case ACT_NONE: canvas.point(curr_pixel, cursor.get_pos()); break;
                            case ACT_DRAW_LINE: canvas.draw_line(prev_point, cursor.get_pos(), curr_pixel); break;
                            case ACT_DRAW_CIRCLE: canvas.draw_circle(prev_point, std::roundl(prev_point.distance(cursor.get_pos())), curr_pixel); break;
                            case ACT_DRAW_BOUNDARY: canvas.draw_boundary_line(prev_point, cursor.get_pos()); break;
                            case ACT_DRAW_ELLIPSE: canvas.draw_ellipse(prev_point, cursor.get_pos().x - prev_point.x, cursor.get_pos().y - prev_point.y, curr_pixel); break;
                            case ACT_FILL_CIRCLE: canvas.fill_circle(prev_point, std::roundl(prev_point.distance(cursor.get_pos())), curr_pixel); break;
                            case ACT_FILL_ELLIPSE: canvas.fill_ellipse(prev_point, cursor.get_pos().x - prev_point.x, cursor.get_pos().y - prev_point.y, curr_pixel); break;
                            case ACT_FILL_AREA: canvas.fill_area(prev_point, cursor.get_pos(), curr_pixel); break;
                        }
                        act = ACT_NONE;
                        break;
                    }
                    case 'w': canvas.update_line(cursor.pos.y); cursor.move_y(-1); break;
                    case 's': canvas.update_line(cursor.pos.y); cursor.move_y(1); break;
                    case 'a': canvas.update_line(cursor.pos.y); cursor.move_x(-1); break;
                    case 'd': canvas.update_line(cursor.pos.y); cursor.move_x(1); break;
                    case '/': {
                        term.clear();
                        Command* command = term.main();
                        if (command == NULL) break;
                        command->execute(*this);
                        delete command;
                        break;
                    }
                    case 'l': {
                        prev_point = cursor.get_pos();
                        act = ACT_DRAW_LINE;
                        break;
                    }
                    case 'c': {
                        prev_point = cursor.get_pos();
                        act = ACT_DRAW_CIRCLE;
                        break;
                    }
                    case 'C': {
                        prev_point = cursor.get_pos();
                        act = ACT_FILL_CIRCLE;
                        break;
                    }
                    case 'b': {
                        prev_point = cursor.get_pos();
                        act = ACT_DRAW_BOUNDARY;
                        break;
                    }
                    case 'F': {
                        prev_point = cursor.get_pos();
                        act = ACT_FILL_AREA;
                        break;
                    }
                    case 'e': {
                        prev_point = cursor.get_pos();
                        act = ACT_DRAW_ELLIPSE;
                        break;
                    }
                    case 'E': {
                        prev_point = cursor.get_pos();
                        act = ACT_FILL_ELLIPSE;
                        break;
                    }
                    case 'f': canvas.fill_area(cursor.get_pos(), curr_pixel); break;
                    case '0': canvas.update_line(cursor.pos.y); cursor.pos.x = 0; break;
                    case '$': canvas.update_line(cursor.pos.y); cursor.pos.x = canvas.get_width() - 1; break;
                    case 'g': canvas.update_line(cursor.pos.y); cursor.pos.y = 0; break;
                    case 'G': canvas.update_line(cursor.pos.y); cursor.pos.y = canvas.get_height() - 1; break;
                    default: std::print("\033[0m\033[40;1H{}", (int)c);
                }
            }

            show_cursor(true);
            change_echo(true);
        }
};

struct termios Drawer::attributes;
Drawer* Drawer::d;

void QuitCommand::execute(Drawer& d) {
    d.quit();
}

void HelpCommand::execute(Drawer& d) {
    std::string help_msg;
    std::string line;
    std::ifstream help_file;

    help_file.open(fname);
    while(!help_file.eof()) {
        std::getline(help_file, line);
        help_msg += line + '\n';
    }
    help_file.close();

    d.out.draw(help_msg);
}

void ResizeCommand::execute(Drawer& d) {
    if (width < 4 || height < 4) return;
    d.resize(width, height);
}

void ScrollCommand::execute(Drawer& d) {
    d.out.move(dy);
}

void CursorCommand::execute(Drawer& d) {
    if (0 <= x && x < d.canvas.get_width() && 0 <= y && y < d.canvas.get_height()) {
        d.canvas.update_line(d.cursor.pos.y);
        d.cursor.pos.x = x;
        d.cursor.pos.y = y;
    } else if (x == -1 && y == -1)
        d.out.draw(std::format("({}, {})", d.cursor.pos.x, d.cursor.pos.y));
}

void OutputCommand::execute(Drawer& d) {
    std::string str;
    int i = 0;

    while (i < output.length()) {
        if (output[i] != '$') {
            str += output[i];
            i++;
            continue;
        }

        i++;

        std::string varname;
        while (i < output.length() && output[i] != ' ' && output[i] != '$') {
            varname += output[i];
            i++;
        }

        str += get_var(varname, d);
    }

    d.out.draw(str);
}

void UndoCommand::execute(Drawer& d) {
    d.undo(times);
}

std::string OutputCommand::get_var(std::string varname, Drawer& d) {
    if (varname == "cursor")
        return std::format("({}, {})", d.cursor.pos.x, d.cursor.pos.y);
    else if (varname == "color") {
        const Pixel& c = d.canvas[d.cursor.pos.x, d.cursor.pos.y];
        return std::format("rgb({}, {}, {})", c.r, c.g, c.b);
    } else if (varname == "dimensions")
        return std::format("{}x{}", d.canvas.get_width(), d.canvas.get_height());
    else if (varname == "version")
        return version_no;
    else if (varname == "credits")
        return "Created by Ari Feiglin";
    return "";   
}

void PixelChangeCommand::execute(Drawer& d) {
    d.curr_pixel = c;
}

void AddTextCommand::execute(Drawer& d) {
    d.canvas.add_text(Point<uint>(d.cursor.pos.x, d.cursor.pos.y), text, d.curr_pixel.r, d.curr_pixel.g, d.curr_pixel.b);
}

void DrawLineCommand::execute(Drawer& d) {
    try {
        d.canvas.draw_line(b, e, d.curr_pixel);
    } catch (std::invalid_argument e) {}
}

void DrawBoundaryLineCommand::execute(Drawer& d) {
    try {
        d.canvas.draw_boundary_line(b, e);
    } catch (std::invalid_argument e) {}
}

void DrawCircleCommand::execute(Drawer& d) {
    try {
        d.canvas.draw_circle(p, r, d.curr_pixel);
    } catch (std::invalid_argument e) {}
}

void FillCircleCommand::execute(Drawer& d) {
    try {
        d.canvas.fill_circle(p, r, d.curr_pixel);
    } catch (std::invalid_argument e) {}
}

void FillAreaCommand::execute(Drawer& d) {
    try {
        d.canvas.fill_area(p1, p2, d.curr_pixel);
    } catch (std::invalid_argument e) {}
}

void SaveCommand::execute(Drawer& d) {
    d.canvas.save(filename);
    d.out.draw(std::format("Saved to {}!", filename));
}

int main(int argc, char** argv) {
    uint width = -1;
    uint height = -1;
    std::string fname;

    int i = 1;
    while (i < argc) {
        std::string arg(argv[i]);
        if (arg.rfind("--", 0) != 0) {
            std::print("Invalid flag {} (must begin with --)\n", arg);
            std::exit(1);
        }

        arg = arg.substr(2);
        if (arg == "dimens") {
            if (argc < i+3) {
                std::print("Must provide dimensions\n");
                std::exit(1);
            }
            width = std::stoi(argv[i+1]);
            height = std::stoi(argv[i+2]);
            i += 3;
        } else if (arg == "file") {
            if (argc < i+2) {
                std::print("Must provide filename\n");
                std::exit(1);
            }
            fname = argv[i+1];
            i += 2;
        } else if (arg == "help") {
            std::ifstream help_file;
            help_file.open("help.txt");

            std::string line;
            while (!help_file.eof()) {
                std::getline(help_file, line);
                std::cout << line << std::endl;
            }

            help_file.close();
            i++;
        } else if (arg == "display") {
            if (argc < i + 2) {
                std::print("Must provide filename\n");
                std::exit(1);
            }
            std::cout << "\033[2J\033[H" << std::flush;
            Canvas canvas(argv[i + 1]);
            canvas.draw();
            std::cout << std::endl << std::endl;
            std::exit(0);
        } else {
            std::print("Invalid flag {}\n", arg);
            std::exit(1);
        }
    }

    Drawer* d = NULL;

    if (width != -1 && height != -1) {
        d = new Drawer(width, height);
    } else if (fname != "") {
        d = new Drawer(fname);
    }

    if (d != NULL) {
        d->main();
        delete d;
    }
}

