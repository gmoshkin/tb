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

std::ostream &endl(std::ostream &os) { return os << std::endl; }

template <typename T>
using uptr = std::unique_ptr<T>;

constexpr uint32_t Pixel = L'\u2584';
constexpr uint32_t EmptyCell = ' ';

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

    constexpr Color(uint16_t attr) : attr{attr} {}
    constexpr operator uint16_t () const
    {
        return attr;
    }
    constexpr Color reversed() const
    {
        return attr | TB_REVERSE;
    }

private:
    uint16_t attr = TB_DEFAULT;
};

constexpr Color Color::Default{TB_DEFAULT};
constexpr Color Color::Black{TB_BLACK};
constexpr Color Color::Red{TB_RED};
constexpr Color Color::Green{TB_GREEN};
constexpr Color Color::Yellow{TB_YELLOW};
constexpr Color Color::Blue{TB_BLUE};
constexpr Color Color::Magenta{TB_MAGENTA};
constexpr Color Color::Cyan{TB_CYAN};
constexpr Color Color::White{TB_WHITE};

/******************************************************************************/
/* Display                                                                     */

class Display
{
public:
    using Cells = std::valarray<Color>;

public:
    Display(size_t width, size_t height)
        : width{width}, height{height}, cells{Color::Default, width * height} {}
    Display() : width{0}, height{0}, cells{} {}

    void resize(size_t width, size_t height)
    {
        this->width = width;
        this->height = height;
        cells.resize(width * height, Color::Default);
    }

    void putPoint(int x, int y, Color color)
    {
        if (x < 0 or y < 0 or x >= width or y >= height) {
            return;
        }
        cells[getIndex(x, y)] = color;
    }

    Color getPoint(int x, int y) const
    {
        if (x < 0 or y < 0 or x >= width or y >= height) {
            return Color::Default;
        }
        return cells[getIndex(x, y)];
    }

    void display() const
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
    }
private:
    constexpr size_t getIndex(int x, int y) const noexcept
    {
        return y * width + x;
    }

private:
    size_t width;
    size_t height;
private:
    Cells cells;
};

/******************************************************************************/
/* Entities                                                                   */

class Entity
{
public:
    Entity(int x, int y) : x{x}, y{y} {}
    virtual void draw(Display &) const = 0;
    virtual void update() = 0;
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
    Screen() : entities{}, display{make_unique<Display>()} {}
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
        auto width = std::to_string(tb_width());
        auto height = std::to_string(tb_height());
        int col = 0;
        for (auto &c : width) {
            tb_change_cell(col++, 0, c, Color::White, Color::Black);
        }
        tb_change_cell(col++, 0, 'x', Color::White, Color::Black);
        for (auto &c : height) {
            tb_change_cell(col++, 0, c, Color::White, Color::Black);
        }
    }

    void draw()
    {
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
/* Main                                                                       */

int main(int argc, char *argv[])
{
    tb = make_unique<Termbox>();
    if (not tb->init()) {
        return -1;
    }
    auto screen = make_unique<Screen>();
    screen->addEntity(make_unique<Point>(0, 0, Color::White));
    screen->addEntity(make_unique<Point>(1, 1, Color::White));
    screen->addEntity(make_unique<Point>(2, 2, Color::White));
    screen->addEntity(make_unique<Point>(3, 3, Color::Blue));
    screen->addEntity(make_unique<Point>(4, 4, Color::Red));
    tb->setScreen(move(screen));
    tb->loop();
    return 0;
}
