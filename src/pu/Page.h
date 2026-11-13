#pragma once

#include <Arduino.h>
#include <deque>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <type_traits>
#include <vector>

#include "HoldState.h"
#include "Numbers.h"

namespace pu {

class Settings;

enum class Color : uint8_t { Default, Green, Orange, Red };

/// One element of a Page. Returned by the Page methods so details can be chained:
///   page.number(settings, "volume", "Volume").unit("L").min(0).step(0.1);
class Widget {
  public:
    Widget& unit(const char* text);
    Widget& min(double value);
    Widget& max(double value);
    Widget& step(double value);
    Widget& color(Color value);
    /// Digits after the decimal point of a live number (default 2).
    Widget& decimals(uint8_t digits);
    /// Called from loop() after the user changed this input.
    Widget& onChange(std::function<void()> callback);
    /// Run this widget's callbacks right away on the network task instead of from loop().
    /// For things that must work while loop() is busy, such as an emergency stop. The callback
    /// must be short and safe to run next to loop().
    Widget& immediate();

  private:
    friend class Page;
    enum class Kind : uint8_t {
        Section,
        Number,
        Text,
        Password,
        Toggle,
        Slider,
        Select,
        Button,
        HoldButton,
        Value,
        Indicator,
    };

    Kind kind_ = Kind::Section;
    String label_;
    String unit_;
    NumberRange range_;
    bool hasStep_ = false;
    double step_ = 0.0;
    Color color_ = Color::Default;
    uint8_t decimals_ = 2;
    bool immediate_ = false;
    std::vector<String> options_;

    // Inputs are either bound to a setting ...
    Settings* settings_ = nullptr;
    String key_;
    // ... or to callbacks.
    std::function<double()> getNumber_;
    std::function<void(double)> setNumber_;
    std::function<bool()> getBool_;
    std::function<void(bool)> setBool_;
    std::function<String()> getText_;
    std::function<void()> onClick_;
    std::function<void(bool)> onHold_;
    std::function<void()> onChange_;

    // Latest values taken from the getters by Page::loop(), served to the browser.
    double numberSample_ = 0.0;
    bool boolSample_ = false;
    String textSample_;
    HoldState hold_;
};

/// A web page declared in C++: inputs bound to settings, buttons and live values. The library
/// renders it in the shared style and handles all requests. Create pages in setup() through
/// ProjectUtils.web().home() or .page(); add all widgets there, before the first request.
///
/// Callbacks run from ProjectUtils.loop() unless the widget is marked immediate().
class Page {
  public:
    Page(String path, String title);

    /// Heading that starts a new group of widgets.
    Widget& section(const char* title);

    // Inputs bound to a setting: shown with its current value, validated and saved on change.
    /// Number field for an int, float or double setting.
    Widget& number(Settings& settings, const char* key, const char* label);
    /// Slider for an int, float or double setting. Defaults to 0..100.
    Widget& slider(Settings& settings, const char* key, const char* label);
    /// Switch for a bool setting.
    Widget& toggle(Settings& settings, const char* key, const char* label);
    /// Text field for a string setting.
    Widget& text(Settings& settings, const char* key, const char* label);
    /// Password field for a string setting. The stored value is never sent to the browser;
    /// the page only shows whether one is set. Saving an empty field clears it.
    Widget& password(Settings& settings, const char* key, const char* label);
    /// Drop-down list. A string setting stores the chosen text, an int setting its position.
    Widget& select(Settings& settings, const char* key, const char* label,
                   std::initializer_list<const char*> options);

    // Inputs bound to your own code instead of a setting (nothing is stored).
    Widget& number(const char* label, std::function<double()> get, std::function<void(double)> set);
    Widget& slider(const char* label, std::function<double()> get, std::function<void(double)> set);
    Widget& toggle(const char* label, std::function<bool()> get, std::function<void(bool)> set);

    /// Button that runs `onClick` once per click.
    Widget& button(const char* label, std::function<void()> onClick);
    /// Button that is active while held down: `onHold(true)` on press, `onHold(false)` on
    /// release, and also when the browser stops responding for a second.
    Widget& holdButton(const char* label, std::function<void(bool)> onHold);

    /// Live value. `getter` may return a number or text (String, const char*).
    template <typename Getter> Widget& value(const char* label, Getter getter) {
        return addValue(label, getter, std::is_arithmetic<decltype(getter())>());
    }
    /// Status light that is lit while `getter` returns true.
    Widget& indicator(const char* label, std::function<bool()> getter);

    const String& path() const { return path_; }
    const String& title() const { return title_; }

    // --- used by the web interface ---
    void loop();
    /// {"title": ..., "widgets": [...], "values": [...]}
    String layoutJson();
    /// {"values": [...]}, one entry per widget, null where a widget has no value.
    String valuesJson();
    /// Applies a browser action. Returns the HTTP status and fills `message` on failure.
    int handleAction(size_t index, const String& action, const String* value, String& message);

  private:
    enum class EventType : uint8_t { Click, HoldStart, HoldEnd, SetNumber, SetBool, Changed };
    struct Event {
        size_t index;
        EventType type;
        double number;
    };

    template <typename Getter>
    Widget& addValue(const char* label, Getter getter, std::true_type /*isNumber*/) {
        return numberValue(label, [getter]() { return static_cast<double>(getter()); });
    }
    template <typename Getter>
    Widget& addValue(const char* label, Getter getter, std::false_type /*isNumber*/) {
        return textValue(label, [getter]() { return String(getter()); });
    }
    Widget& numberValue(const char* label, std::function<double()> getter);
    Widget& textValue(const char* label, std::function<String()> getter);

    Widget& add(Widget::Kind kind, const char* label);
    Widget& addBound(Widget::Kind kind, Settings& settings, const char* key, const char* label);
    int setValue(size_t index, const String& text, String& message);
    int setNumber(size_t index, const String& text, String& message);
    int emit(size_t index, EventType type, double number, String& message);
    void dispatch(const Event& event);
    void sample();

    String path_;
    String title_;
    std::deque<Widget> widgets_; // deque: references handed out by add() stay valid
    std::mutex mutex_;           // guards events_, hold states and samples
    std::vector<Event> events_;
    uint32_t lastSampleMs_ = 0;
};

} // namespace pu
