#include "Page.h"

#include <ArduinoJson.h>

#include "Settings.h"

namespace pu {

namespace {
const uint32_t kHoldTimeoutMs = 1000; // a held button lets go this long after the last sign of life
const uint32_t kSampleIntervalMs = 250; // how often live values are read
const size_t kMaxPendingEvents = 32;
const size_t kMaxTextLength = 200;

const char* colorName(Color color) {
    switch (color) {
    case Color::Green:
        return "green";
    case Color::Orange:
        return "orange";
    case Color::Red:
        return "red";
    default:
        return "";
    }
}
} // namespace

// ---------- Widget ----------

Widget& Widget::unit(const char* text) {
    unit_ = text;
    return *this;
}

Widget& Widget::min(double value) {
    range_.hasMin = true;
    range_.min = value;
    return *this;
}

Widget& Widget::max(double value) {
    range_.hasMax = true;
    range_.max = value;
    return *this;
}

Widget& Widget::step(double value) {
    hasStep_ = true;
    step_ = value;
    return *this;
}

Widget& Widget::color(Color value) {
    color_ = value;
    return *this;
}

Widget& Widget::decimals(uint8_t digits) {
    decimals_ = digits;
    return *this;
}

Widget& Widget::onChange(std::function<void()> callback) {
    onChange_ = std::move(callback);
    return *this;
}

Widget& Widget::immediate() {
    immediate_ = true;
    return *this;
}

// ---------- Page: building ----------

Page::Page(String path, String title) : path_(std::move(path)), title_(std::move(title)) {}

Widget& Page::add(Widget::Kind kind, const char* label) {
    widgets_.emplace_back();
    Widget& widget = widgets_.back();
    widget.kind_ = kind;
    widget.label_ = label;
    return widget;
}

Widget& Page::addBound(Widget::Kind kind, Settings& settings, const char* key, const char* label) {
    Widget& widget = add(kind, label);
    widget.settings_ = &settings;
    widget.key_ = key;
    return widget;
}

Widget& Page::section(const char* title) {
    return add(Widget::Kind::Section, title);
}

Widget& Page::number(Settings& settings, const char* key, const char* label) {
    return addBound(Widget::Kind::Number, settings, key, label);
}

Widget& Page::slider(Settings& settings, const char* key, const char* label) {
    return addBound(Widget::Kind::Slider, settings, key, label).min(0).max(100);
}

Widget& Page::toggle(Settings& settings, const char* key, const char* label) {
    return addBound(Widget::Kind::Toggle, settings, key, label);
}

Widget& Page::text(Settings& settings, const char* key, const char* label) {
    return addBound(Widget::Kind::Text, settings, key, label);
}

Widget& Page::select(Settings& settings, const char* key, const char* label,
                     std::initializer_list<const char*> options) {
    Widget& widget = addBound(Widget::Kind::Select, settings, key, label);
    for (const char* option : options) {
        widget.options_.emplace_back(option);
    }
    return widget;
}

Widget& Page::number(const char* label, std::function<double()> get,
                     std::function<void(double)> set) {
    Widget& widget = add(Widget::Kind::Number, label);
    widget.getNumber_ = std::move(get);
    widget.setNumber_ = std::move(set);
    return widget;
}

Widget& Page::slider(const char* label, std::function<double()> get,
                     std::function<void(double)> set) {
    Widget& widget = add(Widget::Kind::Slider, label).min(0).max(100);
    widget.getNumber_ = std::move(get);
    widget.setNumber_ = std::move(set);
    return widget;
}

Widget& Page::toggle(const char* label, std::function<bool()> get, std::function<void(bool)> set) {
    Widget& widget = add(Widget::Kind::Toggle, label);
    widget.getBool_ = std::move(get);
    widget.setBool_ = std::move(set);
    return widget;
}

Widget& Page::button(const char* label, std::function<void()> onClick) {
    Widget& widget = add(Widget::Kind::Button, label);
    widget.onClick_ = std::move(onClick);
    return widget;
}

Widget& Page::holdButton(const char* label, std::function<void(bool)> onHold) {
    Widget& widget = add(Widget::Kind::HoldButton, label);
    widget.onHold_ = std::move(onHold);
    return widget;
}

Widget& Page::numberValue(const char* label, std::function<double()> getter) {
    Widget& widget = add(Widget::Kind::Value, label);
    widget.getNumber_ = std::move(getter);
    return widget;
}

Widget& Page::textValue(const char* label, std::function<String()> getter) {
    Widget& widget = add(Widget::Kind::Value, label);
    widget.getText_ = std::move(getter);
    return widget;
}

Widget& Page::indicator(const char* label, std::function<bool()> getter) {
    Widget& widget = add(Widget::Kind::Indicator, label);
    widget.getBool_ = std::move(getter);
    return widget;
}

// ---------- Page: running ----------

void Page::dispatch(const Event& event) {
    const Widget& widget = widgets_[event.index];
    switch (event.type) {
    case EventType::Click:
        if (widget.onClick_) {
            widget.onClick_();
        }
        break;
    case EventType::HoldStart:
    case EventType::HoldEnd:
        if (widget.onHold_) {
            widget.onHold_(event.type == EventType::HoldStart);
        }
        break;
    case EventType::SetNumber:
        if (widget.setNumber_) {
            widget.setNumber_(event.number);
        }
        break;
    case EventType::SetBool:
        if (widget.setBool_) {
            widget.setBool_(event.number != 0.0);
        }
        break;
    case EventType::Changed:
        break;
    }
    const bool isInput = event.type == EventType::SetNumber || event.type == EventType::SetBool ||
                         event.type == EventType::Changed;
    if (isInput && widget.onChange_) {
        widget.onChange_();
    }
}

int Page::emit(size_t index, EventType type, double number, String& message) {
    const Event event = {index, type, number};
    if (widgets_[index].immediate_) {
        dispatch(event);
        return 200;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.size() >= kMaxPendingEvents) {
        message = "device is busy, try again";
        return 503;
    }
    events_.push_back(event);
    return 200;
}

void Page::sample() {
    // Getters are user code: call them without holding the lock, then store the results.
    for (Widget& widget : widgets_) {
        if (widget.getNumber_) {
            const double value = widget.getNumber_();
            std::lock_guard<std::mutex> lock(mutex_);
            widget.numberSample_ = value;
        } else if (widget.getBool_) {
            const bool value = widget.getBool_();
            std::lock_guard<std::mutex> lock(mutex_);
            widget.boolSample_ = value;
        } else if (widget.getText_) {
            String value = widget.getText_();
            std::lock_guard<std::mutex> lock(mutex_);
            widget.textSample_ = std::move(value);
        }
    }
}

void Page::loop() {
    std::vector<Event> pending;
    const uint32_t now = millis();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(events_);
        for (size_t index = 0; index < widgets_.size(); ++index) {
            if (widgets_[index].hold_.expire(now, kHoldTimeoutMs)) {
                pending.push_back({index, EventType::HoldEnd, 0.0});
            }
        }
    }
    for (const Event& event : pending) {
        dispatch(event);
    }
    if (lastSampleMs_ == 0 || now - lastSampleMs_ >= kSampleIntervalMs || !pending.empty()) {
        lastSampleMs_ = now == 0 ? 1 : now;
        sample();
    }
}

// ---------- Page: requests ----------

int Page::setNumber(size_t index, const String& text, String& message) {
    Widget& widget = widgets_[index];
    double value = 0.0;
    if (!parseNumber(text.c_str(), value)) {
        message = "not a number";
        return 400;
    }
    if (!widget.range_.contains(value)) {
        message = "value is outside the allowed range";
        return 400;
    }
    if (widget.settings_ == nullptr) {
        return emit(index, EventType::SetNumber, value, message);
    }

    Settings::Type type = Settings::Type::Int;
    if (!widget.settings_->typeOf(widget.key_.c_str(), type)) {
        message = "setting '" + widget.key_ + "' is not defined";
        return 500;
    }
    esp_err_t error = ESP_ERR_INVALID_ARG;
    if (type == Settings::Type::Int) {
        if (!isWholeNumber(value)) {
            message = "must be a whole number";
            return 400;
        }
        error = widget.settings_->set(widget.key_.c_str(), static_cast<int>(value));
    } else if (type == Settings::Type::Float) {
        error = widget.settings_->set(widget.key_.c_str(), static_cast<float>(value));
    } else if (type == Settings::Type::Double) {
        error = widget.settings_->set(widget.key_.c_str(), value);
    }
    if (error != ESP_OK) {
        message = "could not save the setting";
        return 500;
    }
    return emit(index, EventType::Changed, 0.0, message);
}

int Page::setValue(size_t index, const String& text, String& message) {
    Widget& widget = widgets_[index];
    switch (widget.kind_) {
    case Widget::Kind::Number:
    case Widget::Kind::Slider:
        return setNumber(index, text, message);

    case Widget::Kind::Toggle: {
        if (text != "true" && text != "false") {
            message = "value must be true or false";
            return 400;
        }
        const bool value = text == "true";
        if (widget.settings_ == nullptr) {
            return emit(index, EventType::SetBool, value ? 1.0 : 0.0, message);
        }
        if (widget.settings_->set(widget.key_.c_str(), value) != ESP_OK) {
            message = "could not save the setting";
            return 500;
        }
        return emit(index, EventType::Changed, 0.0, message);
    }

    case Widget::Kind::Text:
        if (text.length() > kMaxTextLength) {
            message = "text is too long";
            return 400;
        }
        if (widget.settings_->set(widget.key_.c_str(), text) != ESP_OK) {
            message = "could not save the setting";
            return 500;
        }
        return emit(index, EventType::Changed, 0.0, message);

    case Widget::Kind::Select: {
        int position = -1;
        for (size_t option = 0; option < widget.options_.size(); ++option) {
            if (widget.options_[option] == text) {
                position = static_cast<int>(option);
            }
        }
        if (position < 0) {
            message = "not one of the options";
            return 400;
        }
        Settings::Type type = Settings::Type::String;
        widget.settings_->typeOf(widget.key_.c_str(), type);
        const esp_err_t error = type == Settings::Type::Int
                                    ? widget.settings_->set(widget.key_.c_str(), position)
                                    : widget.settings_->set(widget.key_.c_str(), text);
        if (error != ESP_OK) {
            message = "could not save the setting";
            return 500;
        }
        return emit(index, EventType::Changed, 0.0, message);
    }

    default:
        message = "this element has no value";
        return 400;
    }
}

int Page::handleAction(size_t index, const String& action, const String* value, String& message) {
    if (index >= widgets_.size()) {
        message = "no such element";
        return 404;
    }
    Widget& widget = widgets_[index];

    if (action == "set") {
        if (value == nullptr) {
            message = "missing parameter 'value'";
            return 400;
        }
        return setValue(index, *value, message);
    }
    if (action == "click" && widget.kind_ == Widget::Kind::Button) {
        return emit(index, EventType::Click, 0.0, message);
    }
    if (widget.kind_ == Widget::Kind::HoldButton) {
        bool changed = false;
        EventType type = EventType::HoldStart;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (action == "press" || action == "hold") {
                changed = widget.hold_.touch(millis());
            } else if (action == "release") {
                changed = widget.hold_.release();
                type = EventType::HoldEnd;
            } else {
                message = "unknown action";
                return 400;
            }
        }
        return changed ? emit(index, type, 0.0, message) : 200;
    }
    message = "unknown action";
    return 400;
}

String Page::valuesJson() {
    JsonDocument doc;
    JsonArray values = doc["values"].to<JsonArray>();
    for (const Widget& widget : widgets_) {
        const char* key = widget.key_.c_str();
        Settings::Type type = Settings::Type::String;
        if (widget.settings_ != nullptr) {
            widget.settings_->typeOf(key, type);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        switch (widget.kind_) {
        case Widget::Kind::Number:
        case Widget::Kind::Slider:
            if (widget.settings_ == nullptr) {
                values.add(widget.numberSample_);
            } else if (type == Settings::Type::Int) {
                values.add(widget.settings_->getInt(key));
            } else if (type == Settings::Type::Float) {
                values.add(widget.settings_->getFloat(key));
            } else {
                values.add(widget.settings_->getDouble(key));
            }
            break;
        case Widget::Kind::Toggle:
            values.add(widget.settings_ != nullptr ? widget.settings_->getBool(key)
                                                   : widget.boolSample_);
            break;
        case Widget::Kind::Text:
            values.add(widget.settings_->getString(key));
            break;
        case Widget::Kind::Select:
            if (type == Settings::Type::Int) {
                const int32_t position = widget.settings_->getInt(key);
                const bool valid =
                    position >= 0 && static_cast<size_t>(position) < widget.options_.size();
                values.add(valid ? widget.options_[position] : String());
            } else {
                values.add(widget.settings_->getString(key));
            }
            break;
        case Widget::Kind::HoldButton:
            values.add(widget.hold_.held());
            break;
        case Widget::Kind::Value:
            if (widget.getNumber_) {
                char text[32];
                snprintf(text, sizeof(text), "%.*f", static_cast<int>(widget.decimals_),
                         widget.numberSample_);
                values.add(String(text));
            } else {
                values.add(widget.textSample_);
            }
            break;
        case Widget::Kind::Indicator:
            values.add(widget.boolSample_);
            break;
        default:
            values.add(nullptr);
            break;
        }
    }
    String body;
    serializeJson(doc, body);
    return body;
}

String Page::layoutJson() {
    static const char* const kKindNames[] = {"section", "number", "text", "toggle", "slider",
                                             "select",  "button", "hold", "value",  "indicator"};
    JsonDocument doc;
    doc["title"] = title_;
    JsonArray widgets = doc["widgets"].to<JsonArray>();
    for (const Widget& widget : widgets_) {
        JsonObject item = widgets.add<JsonObject>();
        item["kind"] = kKindNames[static_cast<size_t>(widget.kind_)];
        item["label"] = widget.label_;
        if (!widget.unit_.isEmpty()) {
            item["unit"] = widget.unit_;
        }
        if (widget.range_.hasMin) {
            item["min"] = widget.range_.min;
        }
        if (widget.range_.hasMax) {
            item["max"] = widget.range_.max;
        }
        if (widget.hasStep_) {
            item["step"] = widget.step_;
        }
        if (widget.color_ != Color::Default) {
            item["color"] = colorName(widget.color_);
        }
        if (!widget.options_.empty()) {
            JsonArray options = item["options"].to<JsonArray>();
            for (const String& option : widget.options_) {
                options.add(option);
            }
        }
    }
    // Reuse valuesJson() so both answers always agree on how a value is represented.
    JsonDocument values;
    deserializeJson(values, valuesJson());
    doc["values"] = values["values"];
    String body;
    serializeJson(doc, body);
    return body;
}

} // namespace pu
