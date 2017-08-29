#include <termbox.h>
#include <iostream>
#include <memory>

using std::cerr;
using std::endl;
using std::unique_ptr;

class Screen
{
public:
    void draw()
    {
        tb_change_cell(0, 0, L'\u2584', TB_WHITE, TB_DEFAULT);
    }
};

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

    void setScreen(Screen *screen)
    {
        this->screen = unique_ptr<Screen>{screen};
    }

    using Key = unsigned;

private:
    unique_ptr<Screen> screen;
    int frameRate = 60;
    tb_event currEvent;
    Key quitKey = TB_KEY_CTRL_C;
    bool running = true;
};

int main(int argc, char *argv[])
{
    Termbox tb{};
    if (not tb.init()) {
        return -1;
    }
    tb.setScreen(new Screen{});
    tb.loop();
    return 0;
}
