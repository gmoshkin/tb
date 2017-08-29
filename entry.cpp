#include <termbox.h>
#include <iostream>
#include <vector>
#include <memory>

using std::cerr;
using std::endl;
using std::move;
using std::make_unique;
using std::unique_ptr;
using std::vector;

constexpr uint32_t Pixel = L'\u2584';

enum class Color : uint16_t {
    Default = TB_DEFAULT,
    Black = TB_BLACK,
    Red = TB_RED,
    Green = TB_GREEN,
    Yellow = TB_YELLOW,
    Blue = TB_BLUE,
    Magenta = TB_MAGENTA,
    Cyan = TB_CYAN,
    White = TB_WHITE
};

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

class Entities : public std::vector<unique_ptr<Entity> >
{
public:
    void add(unique_ptr<Entity> &&entity)
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

    void addEntity(unique_ptr<Entity> &&entity)
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

    void setScreen(unique_ptr<Screen> screen)
    {
        this->screen = move(screen);
    }

    using Key = unsigned;

private:
    unique_ptr<Screen> screen;
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
