#include <termbox.h>
#include <iostream>
#include <vector>
#include <valarray>
#include <memory>
#include <sstream>

using std::cerr;
using std::cout;
using std::move;
using std::forward;
using std::make_unique;
using std::vector;
using std::slice;
using std::stringstream;
using std::ostream;
using std::string;

std::ostream &endl(std::ostream &os) { return os << std::endl; }

template <typename T>
using uptr = std::unique_ptr<T>;

constexpr uint32_t Pixel = L'\u2584';
constexpr uint32_t EmptyCell = ' ';

inline ostream &concatImpl(ostream &ss) { return ss; }
template <typename T, typename... Ts>
inline ostream &concatImpl(ostream &ss, T &&v, Ts &&...vs)
{
    return concatImpl(ss << forward<T>(v), forward<Ts>(vs)...);
}

template <typename... Ts>
inline string concat(Ts &&...vs)
{
    stringstream ss;
    concatImpl(ss, forward<Ts>(vs)...);
    return ss.str();
}

class Color
{
public:
    static const Color Default;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Yellow;
    static const Color Blue;
    static const Color Magenta;
    static const Color Cyan;
    static const Color White;

    constexpr int toTerm(int c) noexcept
    {
        return int(c * 6.0 / 256.0);
    }
    constexpr Color(uint16_t attr) noexcept : attr{attr} {}
    constexpr Color(uint16_t red, uint16_t green, uint16_t blue) noexcept
    {
        attr = 16 + 36 * toTerm(red) + 6 * toTerm(green) + toTerm(blue);
    }
    constexpr operator uint16_t () const noexcept
    {
        return attr;
    }
    constexpr Color reversed() const noexcept
    {
        return attr | TB_REVERSE;
    }

private:
    uint16_t attr = TB_DEFAULT;
};

// TB_OUTPUT_256 messes up these colors
constexpr Color Color::Default { TB_DEFAULT } ;
constexpr Color Color::Black   { TB_WHITE   } ;
constexpr Color Color::Red     { TB_BLACK   } ;
constexpr Color Color::Green   { TB_RED     } ;
constexpr Color Color::Yellow  { TB_GREEN   } ;
constexpr Color Color::Blue    { TB_YELLOW  } ;
constexpr Color Color::Magenta { TB_BLUE    } ;
constexpr Color Color::Cyan    { TB_MAGENTA } ;
constexpr Color Color::White   { TB_CYAN    } ;
/******************************************************************************/
/* Display                                                                     */

class Display
{
public:
    Display(size_t width, size_t height) : width{width}, height{height} {}
    Display() : width{0}, height{0} {}

    virtual void resize(size_t width, size_t height) = 0;

    virtual void putPoint(int x, int y, Color color) = 0;

    virtual Color getPoint(int x, int y) const = 0;

    virtual void clear() = 0;

    virtual void display() const = 0;

protected:
    size_t width;
    size_t height;
};

/******************************************************************************/
/* Entities                                                                   */

class Entity
{
public:
    Entity(int x, int y) : x{x}, y{y} {}
    virtual void draw(Display &) const = 0;
    virtual void update() {}
protected:
    int x, y;
};

class Entities : public std::vector<uptr<Entity> >
{
public:
    void add(uptr<Entity> &&entity)
    {
        push_back(move(entity));
    }
};

/******************************************************************************/
/* Text                                                                       */

class Text : public Entity
{
public:
    template <typename TextType>
    Text(int x, int y, TextType &&text, Color fg, Color bg)
        : Entity{x, y}, text{forward<TextType>(text)}, fg{fg}, bg{bg} {}

    void draw(Display &) const override {}

    void drawStraightToTermbox() const
    {
        int col = x;
        for (auto &ch : text) {
            tb_change_cell(col++, y, ch, fg, bg);
        }
    }

private:
    string text;
    Color fg;
    Color bg;
};

/******************************************************************************/
/* PixelDisplay                                                               */

class PixelDisplay : public Display
{
public:
    using Cells = std::valarray<Color>;

public:
    PixelDisplay(size_t width, size_t height)
        : Display{width, height}, cells{Color::Default, width * height} {}
    PixelDisplay() : Display{}, cells{} {}

    void resize(size_t width, size_t height) override
    {
        this->width = width;
        this->height = height * 2;
        cells.resize(this->width * this->height, Color::Default);
    }

    void putPoint(int x, int y, Color color) override
    {
        if (x < 0 or y < 0 or x >= width or y >= height) {
            return;
        }
        cells[getIndex(x, y)] = color;
    }

    Color getPoint(int x, int y) const override
    {
        if (x < 0 or y < 0 or x >= width or y >= height) {
            return Color::Default;
        }
        return cells[getIndex(x, y)];
    }

    void clear() override
    {
        for (auto &c : cells) {
            c = Color::Default;
        }
    }

    void drawSize() const
    {
        auto sizeText = concat(width, 'x', height);
        int textCol = width - sizeText.size();
        Text t{textCol, 0, sizeText, Color::White, Color::Black};
        t.drawStraightToTermbox();
    }

    void display() const override
    {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row < height; row += 2)
            {
                auto top = getPoint(col, row);
                auto bot = getPoint(col, row + 1);
                if (bot == Color::Default) {
                    if (bot == top) {
                        tb_change_cell(col, row / 2, EmptyCell, bot, top);
                    } else {
                        tb_change_cell(col, row / 2, Pixel, top.reversed(), bot);
                    }
                } else {
                    tb_change_cell(col, row / 2, Pixel, bot, top);
                }
            }
        }
        drawSize();
    }

private:
    constexpr size_t getIndex(int x, int y) const noexcept
    {
        return y * width + x;
    }

private:
    Cells cells;
};

/******************************************************************************/
/* Point                                                                      */

class Point : public Entity
{
public:
    Point(int x, int y, Color color) : Entity{x, y}, color{color} {}

    void update() override {}
    void draw(Display &display) const override
    {
        display.putPoint(x, y, color);
    }
private:
    Color color = Color::White;
};

/******************************************************************************/
/* Screen                                                                     */

class Screen
{
public:
    Screen(uptr<Display> &&display) : entities{}, display{move(display)} {}
    void resize(size_t width, size_t height)
    {
        display->resize(width, height);
    }

    void update()
    {
        for (auto &e : entities) {
            e->update();
        }
    }

    void drawSize()
    {
        auto sizeText = concat(tb_width(), 'x', tb_height());
        Text t{0, 0, sizeText, Color::White, Color::Black};
        t.drawStraightToTermbox();
    }

    void draw()
    {
        display->clear();
        for (auto &e : entities) {
            e->draw(*display);
        }
        display->display();
        drawSize();
    }

    void addEntity(uptr<Entity> &&entity)
    {
        entities.add(move(entity));
    }

    void setDisplay(uptr<Display> &&display)
    {
        this->display = move(display);
    }

private:
    Entities entities;
    uptr<Display> display;
};

/******************************************************************************/
/* Termbox                                                                    */

class Termbox
{
public:
    bool init()
    {
        if (auto res = tb_init()) {
            cerr << "tb_init() failed with error code " << res << endl;
            return false;
        }
        tb_select_input_mode(TB_INPUT_ALT | TB_INPUT_MOUSE);
        tb_select_output_mode(TB_OUTPUT_256);
        tb_set_clear_attributes(Color::White, Color::Black);
        return true;
    }

    ~Termbox()
    {
        tb_shutdown();
        cout << "LOGs:" << endl << logStream.str() << endl;
    }

    void processKey()
    {
        for (auto &key : quitKeys) {
            if (currEvent.key == key or currEvent.ch == key) {
                running = false;
            }
        }
    }
    void processResize() {}
    void processMouse() {}
    unsigned getCurrentKey() const
    {
        bool wrote = false;
        if (currEvent.key != 0) {
            log("key: ", currEvent.key);
            wrote = true;
        } if (currEvent.ch != 0) {
            log(wrote ? " " : "", "ch: '", char(currEvent.ch), "'");
            wrote = true;
        } if (wrote) {
            log(endl);
        }
        return currEvent.key != 0 ? currEvent.key : currEvent.ch;
    }

    void loop()
    {
        tb_clear();
        screen->draw();
        tb_present();
        while (running) {
            if (tb_peek_event(&currEvent, 1.0 / frameRate)) {
                switch (currEvent.type) {
                    case TB_EVENT_KEY:
                        processKey();
                    case TB_EVENT_RESIZE:
                        processResize();
                    case TB_EVENT_MOUSE:
                        processMouse();
                }
            }
            screen->update();
            tb_clear();
            screen->draw();
            tb_present();
        }
    }

    void setScreen(uptr<Screen> screen)
    {
        this->screen = move(screen);
        this->screen->resize(getWidth(), getHeight());
    }

    size_t getWidth() const noexcept
    {
        return tb_width();
    }

    size_t getHeight() const noexcept
    {
        return tb_height();
    }

    void log() const {}
    template <typename T, typename... Rest>
    void log(T &&val, Rest &&...rest) const
    {
        logStream << forward<T>(val);
        log(forward<Rest>(rest)...);
    }

    using Key = unsigned;
    using Keys = vector<Key>;

private:
    uptr<Screen> screen;
    int frameRate = 60;
    tb_event currEvent;
    Keys quitKeys = { 'q', TB_KEY_CTRL_C };
    bool running = true;
    mutable stringstream logStream;
};

uptr<Termbox> tb;

/******************************************************************************/
/* Circle                                                                      */

class Circle : public Entity
{
public:
    Circle(int x, int y, int radius, Color color)
        : Entity{x, y}, radius{radius}, color{color} {}

    virtual void update() override {}
    virtual void draw(Display &display) const override
    {
        for (int _x = -radius; _x <= radius; _x++) {
            for (int _y = -radius; _y <= radius; _y++) {
                if (_x * _x + _y * _y <= radius * radius) {
                    display.putPoint(x - _x, y - _y, color);
                }
            }
        }
    }
protected:
    int radius;
    Color color = Color::White;
};

class MyCircle : public Circle
{
public:
    using Circle::Circle;
    virtual void update() override
    {
        switch (tb->getCurrentKey()) {
            case '+':
                radius++;
                break;
            case '-':
                radius--;
                break;
            case 'w':
                y--;
                break;
            case 's':
                y++;
                break;
            case 'd':
                x++;
                break;
            case 'a':
                x--;
                break;
            default:
                break;
        }
    }
};

/******************************************************************************/
/* Main                                                                       */

int main(int argc, char *argv[])
{
    tb = make_unique<Termbox>();
    if (not tb->init()) {
        return -1;
    }
    auto screen = make_unique<Screen>(make_unique<PixelDisplay>());
    screen->addEntity(make_unique<Point>(20, 0, Color::White));
    screen->addEntity(make_unique<Point>(20, 1, Color::White));
    screen->addEntity(make_unique<Point>(20, 2, Color::Black));
    screen->addEntity(make_unique<Point>(20, 3, Color::Black));
    screen->addEntity(make_unique<Point>(20, 4, Color::Red));
    screen->addEntity(make_unique<Point>(20, 5, Color::Red));
    screen->addEntity(make_unique<Point>(20, 6, Color::Green));
    screen->addEntity(make_unique<Point>(20, 7, Color::Green));
    screen->addEntity(make_unique<Point>(20, 8, Color::Yellow));
    screen->addEntity(make_unique<Point>(20, 9, Color::Yellow));
    screen->addEntity(make_unique<Point>(20, 10, Color::Blue));
    screen->addEntity(make_unique<Point>(20, 11, Color::Blue));
    screen->addEntity(make_unique<Point>(20, 12, Color::Magenta));
    screen->addEntity(make_unique<Point>(20, 13, Color::Magenta));
    screen->addEntity(make_unique<Point>(20, 14, Color::Cyan));
    screen->addEntity(make_unique<Point>(20, 15, Color::Cyan));
    screen->addEntity(make_unique<MyCircle>(10, 10, 4, Color{0, 255, 255}));
    tb->setScreen(move(screen));
    tb->loop();
    return 0;
}
