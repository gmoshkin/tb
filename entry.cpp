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

template <typename T1, typename T2, typename T3>
inline constexpr T1 clamp(T1 val, T2 lo, T3 hi)
{
    if (val < lo) {
        return lo;
    } else if (hi < val) {
        return hi;
    } else {
        return val;
    }
}

inline constexpr double sqr(double x)
{
    return x * x;
}

constexpr bool inEllipse(double x, double y, double cX, double cY, double rX, double rY)
{
    return sqr(x - cX) / sqr(rX) + sqr(y - cY) / sqr(rY) <= 1;
}

template <typename T>
using forArithmetic = std::enable_if_t<
    std::is_arithmetic<
        std::remove_reference_t<T>
    >::value>;

/******************************************************************************/
/* TermSOG                                                                    */

struct SOG {
    static const SOG Black;
    static const SOG White;
    constexpr SOG(int attr) noexcept : attr{uint8_t(clamp(attr, 0, 0x17))} {}

    template <typename T, typename = forArithmetic<T>>
    constexpr SOG operator + (T rhs) const noexcept
    {
        return attr + rhs;
    }

    template <typename T, typename = forArithmetic<T>>
    constexpr SOG operator * (T rhs) const noexcept
    {
        return attr * rhs;
    }

    template <typename T, typename = forArithmetic<T>>
    constexpr SOG operator / (T rhs) const noexcept
    {
        return attr / rhs;
    }

    uint8_t attr;
};

constexpr SOG operator + (const SOG &lhs, const SOG &rhs) noexcept
{
    return lhs.attr + rhs.attr;
}

constexpr SOG operator - (const SOG &lhs, const SOG &rhs) noexcept
{
    return lhs.attr - rhs.attr;
}

/******************************************************************************/
/* TermRGB                                                                    */

struct TermRGB {
    constexpr TermRGB(int r, int g, int b) noexcept : r{0}, g{0}, b{0}
    {
        this->r = clamp(r, 0, 5);
        this->g = clamp(g, 0, 5);
        this->b = clamp(b, 0, 5);
    }

    template <typename T, typename = forArithmetic<T>>
    constexpr TermRGB operator * (T n) noexcept
    {
        return {
            static_cast<uint8_t>(r * n),
            static_cast<uint8_t>(g * n),
            static_cast<uint8_t>(b * n),
        };
    }

    template <typename T, typename = forArithmetic<T>>
    constexpr TermRGB operator / (T n) noexcept
    {
        return {
            static_cast<uint8_t>(r / n),
            static_cast<uint8_t>(g / n),
            static_cast<uint8_t>(b / n),
        };
    }

    uint8_t r, g, b;
};

constexpr TermRGB operator + (const TermRGB &lhs, const TermRGB &rhs) noexcept
{
    return TermRGB{ lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b };
}

constexpr TermRGB operator - (const TermRGB &lhs, const TermRGB &rhs) noexcept
{
    return TermRGB{ lhs.r - rhs.r, lhs.g - rhs.g, lhs.b - rhs.b };
}

/******************************************************************************/
/* Color                                                                      */

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
    static const uint16_t ShadeOfGrayBase;
    static const uint16_t RGBBase;

    static constexpr int toTerm(int c) noexcept
    {
        return int(clamp(c, 0, 0xff) * 6.0 / 256.0);
    }

    constexpr Color(int attr) noexcept : attr{static_cast<uint16_t>(attr)} {}
    constexpr Color(SOG sog) noexcept : attr{static_cast<uint16_t>(sog.attr + ShadeOfGrayBase)} {}
    /* constexpr Color(uint16_t attr) noexcept */
    /*     : attr{static_cast<uint16_t>(attr > 0xff ? 0xff : attr)} {} */
    constexpr Color(uint16_t red, uint16_t green, uint16_t blue) noexcept
    /* constexpr Color(int red, int green, int blue) noexcept */
    {
        attr = RGBBase + 36 * toTerm(red) + 6 * toTerm(green) + toTerm(blue);
    }
    constexpr Color(const TermRGB &rgb) noexcept
    {
        attr = RGBBase + 36 * rgb.r + 6 * rgb.g + rgb.b;
    }

    constexpr operator uint16_t () const noexcept
    {
        return attr;
    }
    constexpr Color reversed() const noexcept
    {
        return attr | TB_REVERSE;
    }

    constexpr bool isTClr() const noexcept
    {
        return attr < RGBBase;
    }
    static constexpr Color makeTClr(int tclr) noexcept
    {
        if (tclr < 0) {
            return Color{0};
        } else if (tclr >= RGBBase) {
            return Color{RGBBase - 1};
        } else {
            return Color{tclr};
        }
    }

    constexpr bool isRGB() const noexcept
    {
        return attr >= RGBBase and attr < ShadeOfGrayBase;
    }
private:
    constexpr TermRGB toRGB() const noexcept
    {
        auto term = attr - RGBBase;
        return TermRGB { term / 36, term / 6 % 6, term % 6 };
    }

public:
    constexpr bool isSOG() const noexcept
    {
        return attr >= ShadeOfGrayBase and attr < 0x100;
    }
private:
    constexpr SOG toSOG() const noexcept
    {
        return attr - ShadeOfGrayBase;
    }
public:
    static constexpr Color makeSOG(int sog) noexcept
    {
        if (sog < 0) {
            return Color{ShadeOfGrayBase};
        } else if (sog > 0xff - ShadeOfGrayBase) {
            return Color{0xff};
        } else {
            return Color{static_cast<uint16_t>(sog + ShadeOfGrayBase)};
        }
    }

    friend constexpr Color operator + (const Color &, const Color &) noexcept;
    friend constexpr Color operator - (const Color &, const Color &) noexcept;

    template <typename T, typename = forArithmetic<T>>
    constexpr Color operator + (T n) const noexcept
    {
        if (isTClr()) {
            return makeTClr(attr + n);
        } else if (isSOG()) {
            return toSOG() + n;
        } else {
            return *this;
        }
    }

    template <typename T, typename = forArithmetic<T>>
    constexpr Color operator - (T n) const noexcept
    {
        return *this + (-n);
    }

    template <typename T>
    constexpr Color &operator += (T &&other) noexcept
    {
        return (*this) = (*this + forward<T>(other));
    }
    template <typename T>
    constexpr Color &operator -= (T &&other) noexcept
    {
        return (*this) = (*this - forward<T>(other));
    }
    constexpr Color &operator ++ () noexcept { return (*this) += 1; }
    constexpr Color &operator ++ (int) noexcept { return ++(*this); }
    constexpr Color &operator -- () noexcept { return (*this) -= 1; }
    constexpr Color &operator -- (int) noexcept { return --(*this); }

    template <typename T, typename = forArithmetic<T>>
    constexpr Color operator * (T n) const noexcept
    {
        if (isRGB()) {
            return Color{toRGB() * n};
        } else if (isSOG()) {
            return toSOG() * n;
        } else {
            return *this;
        }
    }
    template <typename T, typename = forArithmetic<T>>
    constexpr Color operator / (T n) const noexcept
    {
        if (isRGB()) {
            return Color{toRGB() / n};
        } else if (isSOG()) {
            return toSOG() / n;
        } else {
            return *this;
        }
    }

    /* static constexpr Color avg(const Color &lhs, const Color &rhs) noexcept */
    /* { */
    /*     return (lhs + rhs) * .5; */
    /* } */

    constexpr bool operator == (const Color &rhs) const noexcept
    {
        return attr == rhs.attr;
    }

    constexpr Color blackToDefault(Color defolt = Color::Default) const noexcept
    {
        if ((isRGB() and *this == Color{0, 0, 0})
            or (isSOG() and *this == SOG::Black)) {
            return defolt;
        }
        return *this;
    }

private:
    uint16_t attr = TB_DEFAULT;
};

constexpr Color operator + (const Color &lhs, const Color &rhs) noexcept
{
    if (lhs.isRGB() and rhs.isRGB()) {
        return {lhs.toRGB() + rhs.toRGB()};
    } else if (lhs.isSOG() and rhs.isSOG()) {
        return {lhs.toSOG() + rhs.toSOG()};
    } else {
        return rhs;
    }
}

constexpr Color operator - (const Color &lhs, const Color &rhs) noexcept
{
    if (lhs.isRGB() and rhs.isRGB()) {
        return {lhs.toRGB() - rhs.toRGB()};
    } else if (lhs.isSOG() and rhs.isSOG()) {
        return {lhs.toSOG() - rhs.toSOG()};
    } else {
        return rhs;
    }
}

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
constexpr uint16_t Color::ShadeOfGrayBase = 0xe8;
constexpr uint16_t Color::RGBBase = 0x10;

constexpr SOG SOG::Black {0};
constexpr SOG SOG::White {0xff - Color::ShadeOfGrayBase};

/******************************************************************************/
/* Text                                                                       */

class Text
{
public:
    template <typename TextType>
    Text(int x, int y, TextType &&text, Color fg = Color::Default, Color bg = Color::Default)
        : x{x}, y{y}, text{forward<TextType>(text)}, fg{fg}, bg{bg} {}

    void drawStraightToTermbox() const
    {
        int col = x;
        for (auto &ch : text) {
            tb_change_cell(col++, y, ch, fg, bg);
        }
    }

private:
    int x, y;
    string text;
    Color fg;
    Color bg;
};

/******************************************************************************/
/* Display                                                                    */

class Display
{
public:
    using Cells = std::valarray<Color>;

public:
    Display(size_t width, size_t height, Color bg = Color::Default) noexcept
        : width{width}, height{height}, cells{bg, width * height}, bg{bg} {}
    Display(Color bg) noexcept
        : bg{bg} {}
    Display() = default;

    void resize(size_t width, size_t height)
    {
        this->width = width;
        this->height = height * 2;
        cells.resize(this->width * this->height, Color::Default);
    }

    void putPoint(int x, int y, Color color)
    {
        if (x < 0 or y < 0 or x >= width or y >= height) {
            return;
        }
        cells[getIndex(x, y)] = color;
    }

    void putPoint(double x, double y, Color color)
    {
        int xFloor = static_cast<int>(x), xCeil = xFloor + 1;
        int yFloor = static_cast<int>(y), yCeil = yFloor + 1;
        double xFraction = x - xFloor, yFraction = y - yFloor;
        /* Color topLeft = getPoint(xFloor, yFloor); */
        /* Color topRight = getPoint(xCeil, yFloor); */
        /* Color botLeft = getPoint(xFloor, yCeil); */
        /* Color botRight = getPoint(xCeil, yCeil); */
        putPoint(xCeil, yCeil,
                 (color * xFraction * yFraction).blackToDefault(bg));
        putPoint(xFloor, yCeil,
                 (color * (1 - xFraction) * yFraction).blackToDefault(bg));
        putPoint(xCeil, yFloor,
                 (color * xFraction * (1 - yFraction)).blackToDefault(bg));
        putPoint(xFloor, yFloor,
                 (color * (1 - xFraction) * (1 - yFraction)).blackToDefault(bg));
    }

    void drawEllipse(double x, double y, double xRadius, double yRadius, Color color)
    {
        for (int col = int(x - xRadius); col <= int(x + xRadius); col++) {
            for (int row = int(y - yRadius); row <= int(y + yRadius); row++) {
                int nCorners = 0;
                nCorners += inEllipse(col, row, x, y, xRadius, yRadius);
                nCorners += inEllipse(col + 1, row, x, y, xRadius, yRadius);
                nCorners += inEllipse(col, row + 1, x, y, xRadius, yRadius);
                nCorners += inEllipse(col + 1, row + 1, x, y, xRadius, yRadius);
                if (nCorners > 0) {
                    putPoint(col, row, color * (nCorners / 4.0));
                }
            }
        }
    }

    Color getPoint(int x, int y) const
    {
        if (x < 0 or y < 0 or x >= width or y >= height) {
            return Color::Default;
        }
        return cells[getIndex(x, y)];
    }

    void clear()
    {
        for (auto &c : cells) {
            c = bg;
        }
    }

    void drawTexts()
    {
        auto sizeText = concat(width, 'x', height);
        int textCol = width - sizeText.size();
        Text t{textCol, 0, sizeText};
        t.drawStraightToTermbox();

        for (auto &t : texts) {
            t.drawStraightToTermbox();
        }
        texts.clear();
    }

    void display()
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
        drawTexts();
    }

    void drawText(int x, int y, const string &text,
                  Color fg = Color::Default, Color bg = Color::Default)
    {
        texts.push_back(Text{x, y, text, fg, bg});
    }

private:
    constexpr size_t getIndex(int x, int y) const noexcept
    {
        return y * width + x;
    }

private:
    size_t width;
    size_t height;
    Cells cells;
    Color bg = Color::Default;
    vector<Text> texts;
};

/******************************************************************************/
/* Entities                                                                   */

class Entity
{
public:
    virtual void draw(Display &) const {}
    virtual void update() {}
};

class Entities : public std::vector<uptr<Entity> >
{
public:
    void add(uptr<Entity> &&entity)
    {
        this->push_back(move(entity));
    }
};

template <typename CoordTy>
class CoordEntity : public Entity
{
public:
    using Base = Entity;
    template <typename T1, typename T2>
    CoordEntity(T1 &&x, T2 &&y) noexcept
        : x{forward<T1>(x)}, y{forward<T2>(y)} {}

    virtual void draw(Display &d) const override { Base::draw(d); }
    virtual void update() override { Base::update(); }

protected:
    CoordTy x, y;
};

template <typename CoordTy>
class ColoredEntity : public CoordEntity<CoordTy>
{
public:
    using Base = CoordEntity<CoordTy>;
    template <typename T1, typename T2>
    ColoredEntity(T1 &&x, T2 &&y, Color color) noexcept
        : Base{forward<T1>(x), forward<T2>(y)}, color{color} {}

    virtual void draw(Display &display) const override { Base::draw(display); }
    virtual void update() override { Base::update(); }

protected:
    Color color = Color::White;
};

/******************************************************************************/
/* Point                                                                      */

class Point : public ColoredEntity<int>
{
public:
    using ColoredEntity::ColoredEntity;

    void draw(Display &display) const override
    {
        display.putPoint(x, y, color);
    }
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
        Text t{0, 0, sizeText};
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
        bool logged = false;
        if (currEvent.key != 0) {
            log("key: ", currEvent.key);
            logged = true;
        } if (currEvent.ch != 0) {
            log(logged ? " " : "", "ch: '", char(currEvent.ch), "'");
            logged = true;
        } if (logged) {
            log(endl);
        }
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
/* FloatPoint                                                                 */

class FloatPoint : public ColoredEntity<double>
{
public:
    using Base = ColoredEntity<double>;
    FloatPoint(double x, double y, Color color)
        : Base{x, y, color}, speed{defaultSpeed} {}

    void update() override
    {
        switch (tb->getCurrentKey()) {
            case '+':
                speed += acceleration;
                break;
            case '-':
                speed -= acceleration;
                break;
            case 'k':
                y -= speed;
                break;
            case 'j':
                y += speed;
                break;
            case 'l':
                x += speed;
                break;
            case 'h':
                x -= speed;
                break;
            default:
                break;
        }
    }

    void draw(Display &display) const override
    {
        display.drawText(0, 2, concat("[", x, ",", y, "]"));
        display.putPoint(x, y, color);
    }

private:
    static const double defaultSpeed;
    static const double acceleration;
    double speed;
};

constexpr double FloatPoint::defaultSpeed = .5;
constexpr double FloatPoint::acceleration = .1;

/******************************************************************************/
/* Circle                                                                     */

class Circle : public ColoredEntity<int>
{
public:
    using Base = ColoredEntity<int>;
    Circle(int x, int y, int radius, Color color)
        : Base{x, y, color}, radius{radius} {}

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
};

class MyCircle : public Circle
{
public:
    using Base = Circle;
    MyCircle(int x, int y, int radius, Color color)
        : Circle{x, y, radius, color} {}

    virtual void update() noexcept override
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
            case '1':
                color = Color{0};
                break;
            case '2':
                color = Color{0, 0, 0};
                break;
            case '3':
                color = SOG::Black;
                break;
            case '>':
                color++;
                break;
            case '<':
                color--;
                break;
            case 'r':
                color += Color{{1, 0, 0}};
                break;
            case 'R':
                color -= Color{{1, 0, 0}};
                break;
            case 'g':
                color += Color{{0, 1, 0}};
                break;
            case 'G':
                color -= Color{{0, 1, 0}};
                break;
            case 'b':
                color += Color{{0, 0, 1}};
                break;
            case 'B':
                color -= Color{{0, 0, 1}};
                break;
            default:
                break;
        }
    }

    void draw(Display &d) const override
    {
        Base::draw(d);
        d.drawText(0, 1, concat("attr: ", (uint16_t) color, "  "));
    }
};

/******************************************************************************/
/* MyEllipse                                                                  */

class MyEllipse : public ColoredEntity<double>
{
public:
    using Base = ColoredEntity<double>;
    MyEllipse(double x, double y, double rX, double rY, Color color) noexcept
        : Base{x, y, color}, rX{rX}, rY{rY} {}

    virtual void update() noexcept override
    {
        switch (tb->getCurrentKey()) {
            case '+':
                speed += acceleration;
                break;
            case '-':
                speed -= acceleration;
                break;
            case 'x':
                rX += speed;
                break;
            case 'X':
                rX -= speed;
                break;
            case 'y':
                rY += speed;
                break;
            case 'Y':
                rY -= speed;
                break;
            case 'w':
                y -= speed;
                break;
            case 's':
                y += speed;
                break;
            case 'd':
                x += speed;
                break;
            case 'a':
                x -= speed;
                break;
            case '1':
                color = Color{0};
                break;
            case '2':
                color = Color{0, 0, 0};
                break;
            case '3':
                color = SOG::Black;
                break;
            case '>':
                color++;
                break;
            case '<':
                color--;
                break;
            case 'r':
                color += Color{{1, 0, 0}};
                break;
            case 'R':
                color -= Color{{1, 0, 0}};
                break;
            case 'g':
                color += Color{{0, 1, 0}};
                break;
            case 'G':
                color -= Color{{0, 1, 0}};
                break;
            case 'b':
                color += Color{{0, 0, 1}};
                break;
            case 'B':
                color -= Color{{0, 0, 1}};
                break;
            default:
                break;
        }
    }

    void draw(Display &display) const override
    {
        display.drawEllipse(x, y, rX, rY, color);
        display.drawText(0, 1, concat("attr: ", (uint16_t) color, "  "));
        display.drawText(0, 3, concat("[", x, ",", y, "](", rX, ",", rY, ")"));
    }

private:
    double rX, rY;
    double speed = .5;
    double acceleration = .1;
};

/******************************************************************************/
/* Tests                                                                      */

void test_MyCircle(Screen &screen, int)
{
    screen.addEntity(make_unique<MyCircle>(10, 10, 4, Color{0, 255, 255}));
}

void test_colorConsts(Screen &screen, int currCol)
{
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, Color::White));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::White));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Black));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Black));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Red));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Red));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Green));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Green));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Yellow));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Yellow));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Blue));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Blue));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Magenta));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Magenta));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Cyan));
    screen.addEntity(make_unique<Point>(currCol, row++, Color::Cyan));
}

void test_makeSOG(Screen &screen, int currCol)
{
    int i = -1;
    while (++i < 0x100 - Color::ShadeOfGrayBase) {
        screen.addEntity(make_unique<Point>(currCol, i, Color::makeSOG(i)));
    }
    screen.addEntity(make_unique<Point>(currCol, i + 1, Color::makeSOG(1000)));
}

void test_SOG(Screen &screen, int currCol)
{
    int i = -1;
    while (++i < 0x100 - Color::ShadeOfGrayBase) {
        screen.addEntity(make_unique<Point>(currCol, i, SOG{i}));
    }
    screen.addEntity(make_unique<Point>(currCol, i + 1, SOG{1000}));
}

void test_addSOG(Screen &screen, int currCol)
{
    auto c1 = Color::makeSOG(5);
    auto c2 = Color::makeSOG(9);
    screen.addEntity(make_unique<Point>(currCol, 0, c1));
    screen.addEntity(make_unique<Point>(currCol, 1, c2));
    screen.addEntity(make_unique<Point>(currCol, 2, c1 + c2));
}

void test_mulSOG(Screen &screen, int currCol)
{
    auto white = Color::makeSOG(23);
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, white * 0));
    screen.addEntity(make_unique<Point>(currCol, row++, white * .1));
    screen.addEntity(make_unique<Point>(currCol, row++, white * .2));
    screen.addEntity(make_unique<Point>(currCol, row++, white * .5));
    screen.addEntity(make_unique<Point>(currCol, row++, white * .8));
    screen.addEntity(make_unique<Point>(currCol, row++, white * 2));
}

void test_divSOG(Screen &screen, int currCol)
{
    auto white = Color::makeSOG(23);
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, white / 24));
    screen.addEntity(make_unique<Point>(currCol, row++, white / 5));
    screen.addEntity(make_unique<Point>(currCol, row++, white / 3));
    screen.addEntity(make_unique<Point>(currCol, row++, white / 2));
    screen.addEntity(make_unique<Point>(currCol, row++, white / 1.5));
    screen.addEntity(make_unique<Point>(currCol, row++, white / 1));
    screen.addEntity(make_unique<Point>(currCol, row++, white / .5));
}

void test_fromTermRGB(Screen &screen, int currCol)
{
    auto r = Color{TermRGB{5, 0, 0}};
    auto g = Color{TermRGB{0, 5, 0}};
    auto b = Color{TermRGB{0, 0, 5}};
    auto c = Color{TermRGB{0, 5, 5}};
    auto m = Color{TermRGB{5, 0, 5}};
    auto y = Color{TermRGB{5, 5, 0}};
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, r));
    screen.addEntity(make_unique<Point>(currCol, row++, g));
    screen.addEntity(make_unique<Point>(currCol, row++, b));
    screen.addEntity(make_unique<Point>(currCol, row++, c));
    screen.addEntity(make_unique<Point>(currCol, row++, m));
    screen.addEntity(make_unique<Point>(currCol, row++, y));
}

void test_addRGB(Screen &screen, int currCol)
{
    auto r = Color{0xff, 0x00, 0x00};
    auto g = Color{0x00, 0xff, 0x00};
    auto b = Color{0x00, 0x00, 0xff};
    auto c = Color{0x00, 0xff, 0xff};
    auto m = Color{0xff, 0x00, 0xff};
    auto y = Color{0xff, 0xff, 0x00};
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, r));
    screen.addEntity(make_unique<Point>(currCol, row++, g));
    screen.addEntity(make_unique<Point>(currCol, row++, b));
    screen.addEntity(make_unique<Point>(currCol, row++, c));
    screen.addEntity(make_unique<Point>(currCol, row++, g + b));
    screen.addEntity(make_unique<Point>(currCol, row++, m));
    screen.addEntity(make_unique<Point>(currCol, row++, b + r));
    screen.addEntity(make_unique<Point>(currCol, row++, y));
    screen.addEntity(make_unique<Point>(currCol, row++, r + g));
    screen.addEntity(make_unique<Point>(currCol, row++, r + c));
    screen.addEntity(make_unique<Point>(currCol, row++, g + m));
    screen.addEntity(make_unique<Point>(currCol, row++, b + y));
}

void test_subRGB(Screen &screen, int currCol)
{
    auto r = Color{0xff, 0x00, 0x00};
    auto g = Color{0x00, 0xff, 0x00};
    auto b = Color{0x00, 0x00, 0xff};
    auto c = Color{0x00, 0xff, 0xff};
    auto m = Color{0xff, 0x00, 0xff};
    auto y = Color{0xff, 0xff, 0x00};
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, r));
    screen.addEntity(make_unique<Point>(currCol, row++, m - b));
    screen.addEntity(make_unique<Point>(currCol, row++, y - g));
    screen.addEntity(make_unique<Point>(currCol, row++, g));
    screen.addEntity(make_unique<Point>(currCol, row++, c - b));
    screen.addEntity(make_unique<Point>(currCol, row++, y - r));
    screen.addEntity(make_unique<Point>(currCol, row++, b));
    screen.addEntity(make_unique<Point>(currCol, row++, m - r));
    screen.addEntity(make_unique<Point>(currCol, row++, c - g));
    screen.addEntity(make_unique<Point>(currCol, row++, c));
    screen.addEntity(make_unique<Point>(currCol, row++, m));
    screen.addEntity(make_unique<Point>(currCol, row++, y));
    screen.addEntity(make_unique<Point>(currCol, row++, c - g - b));
    screen.addEntity(make_unique<Point>(currCol, row++, m - b - r));
    screen.addEntity(make_unique<Point>(currCol, row++, y - r - g));
}

void test_mulRGB(Screen &screen, int currCol)
{
    auto r = Color{{1, 0, 0}};
    auto g = Color{{0, 1, 0}};
    auto b = Color{{0, 0, 1}};
    auto c = Color{{0, 1, 1}};
    auto m = Color{{1, 0, 1}};
    auto y = Color{{1, 1, 0}};
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, r));
    screen.addEntity(make_unique<Point>(currCol, row++, r * 2));
    screen.addEntity(make_unique<Point>(currCol, row++, g));
    screen.addEntity(make_unique<Point>(currCol, row++, g * 3));
    screen.addEntity(make_unique<Point>(currCol, row++, b));
    screen.addEntity(make_unique<Point>(currCol, row++, b * 5));
    screen.addEntity(make_unique<Point>(currCol, row++, r * 1 + g * 4 + b * 6));
}

void test_divRGB(Screen &screen, int currCol)
{
    auto r = Color{0xff, 0x00, 0x00};
    auto g = Color{0x00, 0xff, 0x00};
    auto b = Color{0x00, 0x00, 0xff};
    auto c = Color{0x00, 0xff, 0xff};
    auto m = Color{0xff, 0x00, 0xff};
    auto y = Color{0xff, 0xff, 0x00};
    int row = 0;
    screen.addEntity(make_unique<Point>(currCol, row++, r));
    screen.addEntity(make_unique<Point>(currCol, row++, r / 2));
    screen.addEntity(make_unique<Point>(currCol, row++, g));
    screen.addEntity(make_unique<Point>(currCol, row++, g / 3));
    screen.addEntity(make_unique<Point>(currCol, row++, b));
    screen.addEntity(make_unique<Point>(currCol, row++, b / 5));
    screen.addEntity(make_unique<Point>(currCol, row++, r / 1 + g / 4 + b / 6));
}

void test_floatPoint(Screen &screen, int)
{
    screen.addEntity(make_unique<FloatPoint>(10, 30, Color::makeSOG(0xff)));
}

void test_MyEllipse(Screen &screen, int)
{
    screen.addEntity(make_unique<MyEllipse>(49.59, 33.33, 22.76, 9.77, Color{202}));
}

/******************************************************************************/
/* Main                                                                       */

int main(int argc, char *argv[])
{
    tb = make_unique<Termbox>();
    if (not tb->init()) {
        return -1;
    }
    auto screen = make_unique<Screen>(make_unique<Display>());
    int currCol = 20;
    test_MyCircle(*screen, currCol++);
    test_colorConsts(*screen, currCol++);
    test_makeSOG(*screen, currCol++);
    test_SOG(*screen, currCol++);
    test_addSOG(*screen, currCol++);
    test_mulSOG(*screen, currCol++);
    test_divSOG(*screen, currCol++);
    test_fromTermRGB(*screen, currCol++);
    test_addRGB(*screen, currCol++);
    test_subRGB(*screen, currCol++);
    test_mulRGB(*screen, currCol++);
    test_divRGB(*screen, currCol++);
    test_floatPoint(*screen, currCol++);
    test_MyEllipse(*screen, currCol++);

    tb->setScreen(move(screen));
    tb->loop();
    return 0;
}
