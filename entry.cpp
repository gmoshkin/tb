#include <termbox.h>
#include <iostream>
#include <vector>
#include <memory>

using std::cerr;
using std::endl;
using std::move;
using std::make_unique;
using std::vector;

template <typename T>
using uptr = std::unique_ptr<T>;

constexpr uint32_t Pixel = L'\u2584';

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
/* Entities                                                                   */

class Entity
{
public:
    Entity(int x, int y) : x{x}, y{y} {}
    virtual void draw() const = 0;
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

    void draw() const override
    {
        tb_change_cell(x, y, Pixel,
                       static_cast<uint16_t>(color),
                       static_cast<uint16_t>(Color::Default));
    }
private:
    Color color = Color::White;
};

/******************************************************************************/
/* Screen                                                                     */

class Screen
{
public:
    void draw()
    {
        for (auto &&e : entities) {
            e->draw();
        }
    }

    void addEntity(uptr<Entity> &&entity)
    {
        entities.add(move(entity));
    }

private:
    Entities entities;
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
    }

    void processKey()
    {
        if (currEvent.key == quitKey) {
            running = false;
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
            tb_clear();
            screen->draw();
            tb_present();
        }
    }

    void setScreen(uptr<Screen> screen)
    {
        this->screen = move(screen);
    }

    using Key = unsigned;

private:
    uptr<Screen> screen;
    int frameRate = 60;
    tb_event currEvent;
    Key quitKey = TB_KEY_CTRL_C;
    bool running = true;
};

/******************************************************************************/
/* Main                                                                       */

int main(int argc, char *argv[])
{
    Termbox tb{};
    if (not tb.init()) {
        return -1;
    }
    auto screen = make_unique<Screen>();
    screen->addEntity(make_unique<Point>(0, 0, Color::White));
    screen->addEntity(make_unique<Point>(1, 1, Color::White));
    screen->addEntity(make_unique<Point>(2, 2, Color::White));
    screen->addEntity(make_unique<Point>(3, 3, Color::White));
    screen->addEntity(make_unique<Point>(4, 4, Color::Red));
    tb.setScreen(move(screen));
    tb.loop();
    return 0;
}
