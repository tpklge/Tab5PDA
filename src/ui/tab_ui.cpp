#include "tab_ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tab5_pda {
namespace {
constexpr uint32_t background = 0xedf2f7, navy = 0x16324f, ink = 0x172b4d;
const char *names[] = {"Hoje",         "Contatos",    "Agenda",      "Tarefas",     "Notas",
                       "Aniversários", "Despesas",    "Calculadora", "Conversores", "Relógios",
                       "Calendário",   "Busca geral", "Backup",      "Ajustes",     "Wi-Fi / hora",
                       "Bloqueio",     "Diagnóstico"};
const uint32_t colors[] = {0x285e61, 0x2563eb, 0x7c3aed, 0x16834a, 0xb45309, 0xc02678,
                           0x9c4221, 0xbe3047, 0x047857, 0x334e68, 0x7c3aed, 0x6b46c1,
                           0x475569, 0x0e7490, 0x0369a1, 0x334155, 0x164e63};
const char *accents[] = {"á", "à", "â", "ã", "é", "ê", "í", "ó", "ô", "õ", "ú", "ü", "ç", "ñ",
                         "Á", "À", "Â", "Ã", "É", "Ê", "Í", "Ó", "Ô", "Õ", "Ú", "Ü", "Ç", "Ñ"};
int recordApp(pda::RecordKind k) {
    return k == pda::RecordKind::Contact       ? 1
           : k == pda::RecordKind::Appointment ? 2
           : k == pda::RecordKind::Task        ? 3
           : k == pda::RecordKind::Anniversary ? 5
           : k == pda::RecordKind::Expense     ? 6
                                               : 4;
}
bool integer(const std::string &s, int min, int max, int &out) {
    std::size_t start = !s.empty() && s[0] == '-' ? 1 : 0;
    if (s.size() > 6 || s.size() == start ||
        s.substr(start).find_first_not_of("0123456789") != std::string::npos)
        return false;
    out = std::atoi(s.c_str());
    return out >= min && out <= max;
}
std::string shortName(const std::string &s) {
    auto p = s.find(':');
    return p == std::string::npos ? s : s.substr(0, p);
}
} // namespace
lv_obj_t *Ui::panel(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    auto *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(o, &pda_font_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(ink), 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
lv_obj_t *Ui::label(lv_obj_t *parent, int x, int y, int w, const std::string &s, uint32_t color) {
    auto *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_label_set_text(o, s.c_str());
    lv_obj_set_style_text_font(o, &pda_font_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    return o;
}
lv_obj_t *Ui::button(lv_obj_t *parent, int x, int y, int w, int h, const std::string &s, Command c,
                     uint32_t arg, uint32_t color) {
    auto *o = lv_btn_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color ? color : colors[app_]), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 6, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(navy), LV_STATE_PRESSED);
    auto *l = label(o, 6, 0, w - 12, s, 0xffffff);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
    bindings_.push_back({this, c, arg});
    lv_obj_add_event_cb(o, clicked, LV_EVENT_CLICKED, &bindings_.back());
    return o;
}
lv_obj_t *Ui::input(lv_obj_t *parent, int x, int y, int w, int h, const std::string &s,
                    std::size_t max, bool multiline, bool password) {
    auto *o = lv_textarea_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_text_font(o, &pda_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_font(o, &pda_font_16, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_color(o, lv_color_hex(ink), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(colors[app_]), 0);
    lv_obj_set_style_border_width(o, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(o, lv_color_hex(theme_->accentText), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(o, lv_color_hex(theme_->muted), LV_PART_TEXTAREA_PLACEHOLDER);
    constexpr lv_style_selector_t focusedCursor =
        static_cast<lv_style_selector_t>(LV_PART_CURSOR) |
        static_cast<lv_style_selector_t>(LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_border_width(o, 0, LV_PART_CURSOR);
    lv_obj_set_style_border_width(o, 1, focusedCursor);
    lv_obj_set_style_border_side(o, LV_BORDER_SIDE_LEFT, focusedCursor);
    lv_obj_set_style_border_color(o, lv_color_hex(theme_->accentText), focusedCursor);
    lv_obj_set_style_anim_time(o, 500, focusedCursor);
    lv_obj_set_style_pad_all(o, 10, 0);
    lv_obj_set_style_radius(o, 5, 0);
    lv_textarea_set_one_line(o, !multiline);
    // Setting a nonzero LVGL character limit before loading inserts every
    // character separately and recalculates the layout for each one.
    lv_textarea_set_text(o, s.c_str());
    lv_textarea_set_max_length(o, max);
    lv_textarea_set_password_mode(o, password);
    lv_textarea_set_password_show_time(o, 0);
    lv_obj_add_event_cb(o, focused, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(o, changed, LV_EVENT_VALUE_CHANGED, this);
    return o;
}
std::size_t Ui::inputByteLimit() const {
    auto it = std::find(inputs_.begin(), inputs_.end(), editor_);
    if (it != inputs_.end()) {
        auto index = std::size_t(std::distance(inputs_.begin(), it));
        if (index < inputLimits_.size() && inputLimits_[index])
            return inputLimits_[index];
    }
    if (editor_ && lv_obj_check_type(editor_, &lv_textarea_class)) {
        auto limit = lv_textarea_get_max_length(editor_);
        if (limit)
            return limit;
    }
    return 96;
}
bool Ui::brazilian() const {
    return pda::value(service_.state().settings.fields, "dateFormat", "ISO") == "BR";
}
std::string Ui::fieldCaption(const std::string &caption) const {
    std::string s = caption;
    if (brazilian()) {
        auto p = s.find("AAAA-MM-DD");
        if (p != std::string::npos)
            s.replace(p, 10, "DD/MM/AAAA");
    }
    if (brazilian()) {
        auto p = s.find("123.45");
        if (p != std::string::npos)
            s.replace(p, 6, "123,45");
    }
    return s;
}
std::string Ui::fieldText(const std::string &key, const std::string &value) const {
    if (!brazilian())
        return value;
    if (key == "when" || key == "until" || key == "repeatEnd" || key == "clock")
        return dateToBR(value);
    std::string s = value;
    if (key == "amount")
        std::replace(s.begin(), s.end(), '.', ',');
    return s;
}
std::string Ui::inputValue(lv_obj_t *o) const {
    if (lv_obj_check_type(o, &lv_slider_class))
        return std::to_string(lv_slider_get_value(o));
    if (lv_obj_check_type(o, &lv_dropdown_class)) {
        char text[80];
        lv_dropdown_get_selected_str(o, text, sizeof(text));
        return text;
    }
    std::string text = lv_textarea_get_text(o);
    for (const auto &m : masks_)
        if (m.object == o && (m.kind == InputMask::DateBR || m.kind == InputMask::DateTimeBR))
            return dateToISO(text);
    return text;
}
void Ui::controlChanged(lv_event_t *e) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(e));
    ui->activity();
    auto *o = lv_event_get_target(e);
    ui->editor_ = o;
    auto i = std::find(ui->inputs_.begin(), ui->inputs_.end(), o);
    if (i != ui->inputs_.end())
        ui->field_ = std::distance(ui->inputs_.begin(), i);
    if (ui->mode_ == Mode::Exchange && ui->inputs_.size() == 2 && o == ui->inputs_[1]) {
        const char *names[] = {"calendar.ics", "contacts.vcf", "expenses.csv", "notes.txt",
                               "notes.md"};
        ui->exchangeFormat_ = lv_dropdown_get_selected(o);
        lv_textarea_set_text(ui->inputs_[0], names[ui->exchangeFormat_]);
    }
    if (lv_obj_check_type(o, &lv_slider_class))
        for (auto &pair : ui->sliderLabels_)
            if (pair.first == o)
                lv_label_set_text(pair.second, std::to_string(lv_slider_get_value(o)).c_str());
}
lv_obj_t *Ui::slider(lv_obj_t *parent, int x, int y, int min, int max, int value, int w) {
    auto *o = lv_slider_create(parent);
    lv_obj_set_pos(o, x + 14, y + 17);
    lv_obj_set_size(o, w - 112, 22);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xcbd5e1), LV_PART_MAIN);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(o, 8, LV_PART_KNOB);
    lv_obj_set_ext_click_area(o, 14);
    lv_slider_set_range(o, min, max);
    lv_slider_set_value(o, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(o, lv_color_hex(theme_->accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(o, lv_color_hex(theme_->accent), LV_PART_KNOB);
    auto *number = label(parent, x + w - 80, y + 14, 76, std::to_string(value));
    sliderLabels_.push_back({o, number});
    lv_obj_add_event_cb(o, controlChanged, LV_EVENT_VALUE_CHANGED, this);
    return o;
}
lv_obj_t *Ui::dropdown(lv_obj_t *parent, int x, int y, const std::string &options,
                       unsigned selected) {
    auto *o = lv_dropdown_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, 460, 56);
    lv_obj_set_style_text_font(o, &pda_font_16, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(o, lv_color_hex(theme_->surface), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(colors[app_]), LV_PART_MAIN);
    lv_obj_set_style_radius(o, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 12, LV_PART_MAIN);
    lv_obj_set_style_text_color(o, lv_color_hex(ink), LV_PART_MAIN);
    lv_obj_set_style_text_font(o, &lv_font_montserrat_16, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(o, lv_color_hex(theme_->ink), LV_PART_INDICATOR);
    lv_dropdown_set_symbol(o, LV_SYMBOL_DOWN);
    lv_dropdown_set_options(o, options.c_str());
    lv_dropdown_set_selected(o, selected);
    auto *list = lv_dropdown_get_list(o);
    lv_obj_set_style_text_font(list, &pda_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(list, 19, LV_PART_MAIN); // 40 px touch rows.
    lv_obj_set_style_max_height(list, 260, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, lv_color_hex(theme_->surface), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(theme_->border), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(theme_->ink), LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(list, 10, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, lv_color_hex(theme_->accent), LV_PART_SELECTED);
    lv_obj_set_style_text_color(list, lv_color_hex(0xffffff), LV_PART_SELECTED);
    lv_obj_add_event_cb(o, controlChanged, LV_EVENT_VALUE_CHANGED, this);
    return o;
}
void Ui::mask(lv_obj_t *target, InputMask kind, const char *hint, unsigned max) {
    lv_textarea_set_max_length(target, max);
    const char *allowed = kind == InputMask::Money      ? "0123456789.,-"
                          : kind == InputMask::Signed   ? "0123456789-"
                          : kind == InputMask::Unsigned ? "0123456789"
                                                        : "0123456789-/: ";
    lv_textarea_set_accepted_chars(target, allowed);
    lv_textarea_set_placeholder_text(target, hint);
    masks_.push_back({target, kind, lv_textarea_get_text(target)});
}
void Ui::clicked(lv_event_t *event) {
    auto *b = static_cast<Binding *>(lv_event_get_user_data(event));
    b->ui->activity();
    b->ui->commands_.push_back({b->command, b->argument});
}
void Ui::changed(lv_event_t *event) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
    if (ui->masking_ || ui->casing_)
        return;
    auto *target = lv_event_get_target(event);
    for (const auto &field : ui->casedInputs_)
        if (field.object == target) {
            auto text = std::string(lv_textarea_get_text(target));
            auto formatted = capitalize(text, field.mode);
            if (text != formatted) {
                auto cursor = lv_textarea_get_cursor_pos(target);
                auto limit = lv_textarea_get_max_length(target);
                ui->casing_ = true;
                lv_textarea_set_max_length(target, 0);
                lv_textarea_set_text(target, formatted.c_str());
                lv_textarea_set_max_length(target, limit);
                lv_textarea_set_cursor_pos(target, cursor);
                ui->casing_ = false;
            }
            break;
        }
    for (auto &m : ui->masks_) {
        if (m.object != target)
            continue;
        std::string raw = lv_textarea_get_text(target), formatted;
        unsigned cursor = lv_textarea_get_cursor_pos(target);
        // Backspace on an automatically inserted separator removes the preceding
        // digit as well, so the cursor never gets stuck on a permanent separator.
        bool date = m.kind == InputMask::Date || m.kind == InputMask::DateTime ||
                    m.kind == InputMask::DateBR || m.kind == InputMask::DateTimeBR ||
                    m.kind == InputMask::Period;
        if (date && m.previous.size() == raw.size() + 1 && cursor < m.previous.size()) {
            char removed = m.previous[cursor];
            if ((removed == '-' || removed == '/' || removed == ':' || removed == ' ') &&
                m.previous.substr(0, cursor) + m.previous.substr(cursor + 1) == raw && cursor > 0) {
                raw.erase(cursor - 1, 1);
                --cursor;
            }
        }
        unsigned digitsBefore = 0;
        for (unsigned i = 0; i < std::min<unsigned>(cursor, raw.size()); ++i)
            if (raw[i] >= '0' && raw[i] <= '9')
                ++digitsBefore;
        bool valid = maskedText(m.kind, raw, formatted);
        if (!valid)
            formatted = m.previous;
        if (formatted != lv_textarea_get_text(target)) {
            ui->masking_ = true;
            lv_textarea_set_text(target, formatted.c_str());
            if (date && valid) {
                unsigned pos = 0, seen = 0;
                while (pos < formatted.size() && seen < digitsBefore) {
                    if (formatted[pos] >= '0' && formatted[pos] <= '9')
                        ++seen;
                    ++pos;
                }
                lv_textarea_set_cursor_pos(target, pos);
            } else
                lv_textarea_set_cursor_pos(target, std::min<unsigned>(cursor, formatted.size()));
            ui->masking_ = false;
        }
        m.previous = formatted;
        break;
    }
    ui->activity();
    ui->lastEdit_ = ui->platform_.nowMs();
    if (ui->mode_ == Mode::Edit) {
        ui->dirty_ = true;
        ui->overlapConfirmed_ = false;
    }
}
void Ui::focused(lv_event_t *event) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
    ui->activity();
    auto *target = lv_event_get_target(event);
    if (ui->editor_ && ui->editor_ != target)
        lv_obj_clear_state(ui->editor_, LV_STATE_FOCUSED);
    ui->editor_ = target;
    lv_obj_add_state(target, LV_STATE_FOCUSED);
    auto i = std::find(ui->inputs_.begin(), ui->inputs_.end(), target);
    if (i != ui->inputs_.end()) {
        ui->field_ = std::distance(ui->inputs_.begin(), i);
        ui->draft_.field = ui->field_;
    }
    ui->keyboard(target);
}
// The Tab5 keyboard firmware resolves Aa and Sym into HID modifiers/keycodes.
void Ui::physicalKey(uint8_t modifier, uint8_t code) {
    if (code < 4 || code == 255)
        return; // Releases, rollover, empty FIFO.
    activity();
    if (picker_) {
        if (code == 0x29)
            dispatch(Command::PickerCancel);
        else if (code == 0x28)
            dispatch(Command::PickerApply);
        else if (code == 0x2a || code == 0x4c)
            dispatch(Command::PickerClear);
        else if (pickerOptions_.empty() && (code == 0x50 || code == 0x4f))
            dispatch(Command::PickerAdjust, code == 0x50 ? 0 : 1);
        return;
    }
    const bool ctrl = modifier & 0x11, shift = modifier & 0x22;
    if (modifier & 0xcc)
        return; // Alt/GUI chords are not text.
    if (ctrl) {
        if (code == 0x16) { // Ctrl+S
            if (mode_ == Mode::Edit)
                dispatch(Command::Save);
            else if (mode_ == Mode::Preferences)
                dispatch(Command::SettingsSave);
            else if (mode_ == Mode::Productivity && (productPage_ == 1 || productPage_ == 2))
                dispatch(Command::ProductSave);
        } else if (code == 0x09 && !locked_ && mode_ == Mode::Normal)
            dispatch(Command::Search);
        return;
    }
    dispatch(Command::KeyboardHide); // Hardware input keeps the full form visible.
    if (code == 0x29) {
        if (!locked_)
            dispatch(Command::Cancel);
        return;
    }
    if (code == 0x2b && !inputs_.empty()) {
        if (editor_)
            lv_obj_clear_state(editor_, LV_STATE_FOCUSED);
        // Follow the visible row order; the shared schema places detail second,
        // while Tab5 renders that full-width field after the other inputs.
        std::vector<int> order;
        for (int i = 0; i < int(inputs_.size()); ++i)
            order.push_back(i);
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            int ay = lv_obj_get_y(inputs_[a]), by = lv_obj_get_y(inputs_[b]);
            return ay == by ? lv_obj_get_x(inputs_[a]) < lv_obj_get_x(inputs_[b]) : ay < by;
        });
        auto current = std::find(order.begin(), order.end(), field_) - order.begin();
        field_ = order[(current + (shift ? order.size() - 1 : 1)) % order.size()];
        editor_ = inputs_[field_];
        draft_.field = field_;
        lv_obj_add_state(editor_, LV_STATE_FOCUSED);
        lv_obj_scroll_to_view(editor_, LV_ANIM_OFF);
        return;
    }
    if (!editor_) {
        if (!locked_ && mode_ == Mode::Normal &&
            (code == 0x4f || code == 0x50 || code == 0x51 || code == 0x52))
            dispatch(Command::App, (app_ + ((code == 0x50 || code == 0x52) ? 16 : 1)) % 17);
        return;
    }
    lv_obj_add_state(editor_, LV_STATE_FOCUSED);
    if (lv_obj_check_type(editor_, &lv_slider_class)) {
        int step = (code == 0x4f || code == 0x52) ? 1 : (code == 0x50 || code == 0x51) ? -1 : 0;
        if (step) {
            lv_slider_set_value(editor_, lv_slider_get_value(editor_) + step, LV_ANIM_OFF);
            lv_event_send(editor_, LV_EVENT_VALUE_CHANGED, nullptr);
        }
        return;
    }
    if (lv_obj_check_type(editor_, &lv_dropdown_class)) {
        int step = (code == 0x4f || code == 0x51) ? 1 : (code == 0x50 || code == 0x52) ? -1 : 0;
        if (step) {
            int n = lv_dropdown_get_selected(editor_) + step;
            lv_dropdown_set_selected(
                editor_, std::clamp(n, 0, int(lv_dropdown_get_option_cnt(editor_)) - 1));
            lv_event_send(editor_, LV_EVENT_VALUE_CHANGED, nullptr);
        }
        return;
    }
    if (code == 0x2a) {
        lv_textarea_del_char(editor_);
        return;
    }
    if (code == 0x4c) {
        lv_textarea_del_char_forward(editor_);
        return;
    }
    if (code == 0x50) {
        lv_textarea_cursor_left(editor_);
        return;
    }
    if (code == 0x4f) {
        lv_textarea_cursor_right(editor_);
        return;
    }
    if (code == 0x52) {
        lv_textarea_cursor_up(editor_);
        return;
    }
    if (code == 0x51) {
        lv_textarea_cursor_down(editor_);
        return;
    }
    if (code == 0x4a) {
        lv_textarea_set_cursor_pos(editor_, 0);
        return;
    }
    if (code == 0x4d) {
        lv_textarea_set_cursor_pos(editor_, LV_TEXTAREA_CURSOR_LAST);
        return;
    }
    char ch = 0;
    if (code == 0x28) {
        if (mode_ == Mode::Lock) {
            dispatch(Command::Unlock);
            return;
        }
        if (mode_ == Mode::Search) {
            dispatch(Command::Search, 1);
            return;
        }
        if (mode_ == Mode::Normal && app_ == 7 && calculatorMode_ != 2) {
            dispatch(Command::Calculate);
            return;
        }
        if (mode_ == Mode::Normal && app_ == 8) {
            dispatch(Command::Convert);
            return;
        }
        if (lv_textarea_get_one_line(editor_)) {
            physicalKey(shift ? 2 : 0, 0x2b);
            return;
        }
        ch = '\n';
    } else if (code >= 4 && code <= 29)
        ch = (shift ? 'A' : 'a') + code - 4;
    else if (code >= 30 && code <= 39)
        ch = (shift ? "!@#$%^&*()" : "1234567890")[code - 30];
    else if (code == 0x2c)
        ch = ' ';
    else if (code >= 0x2d && code <= 0x38) {
        // HID 0x32 is the non-US hash key; use a table indexed explicitly.
        static const char lowerMap[] = {'-', '=',  '[', ']', '\\', '#',
                                        ';', '\'', '`', ',', '.',  '/'};
        static const char upperMap[] = {'_', '+', '{', '}', '|', '~', ':', '"', '~', '<', '>', '?'};
        ch = (shift ? upperMap : lowerMap)[code - 0x2d];
    }
    if (!ch)
        return;
    lv_obj_add_state(editor_, LV_STATE_FOCUSED);
    auto limit = inputByteLimit();
    if (std::string(lv_textarea_get_text(editor_)).size() >= limit) {
        message("Limite do campo atingido", true);
        return;
    }
    lv_textarea_add_char(editor_, ch);
}
void Ui::typed(lv_event_t *event) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
    ui->activity();
    auto *kb = lv_event_get_target(event);
    auto id = lv_btnmatrix_get_selected_btn(kb);
    if (id == LV_BTNMATRIX_BTN_NONE || !ui->editor_)
        return;
    std::string key = lv_btnmatrix_get_btn_text(kb, id);
    if (key == "Fechar") {
        ui->commands_.push_back({Command::KeyboardHide, 0});
        return;
    }
    if (key == "Acentos") {
        ui->commands_.push_back({Command::Accents, 0});
        return;
    }
    if (key == "Shift") {
        ui->uppercase_ = !ui->uppercase_;
        ui->keyboard(ui->editor_);
        return;
    }
    if (key == "Símbolos" || key == "ABC") {
        ui->symbols_ = key == "Símbolos";
        ui->keyboard(ui->editor_);
        return;
    }
    if (key == "Apagar") {
        lv_textarea_del_char(ui->editor_);
        return;
    }
    if (key == "Espaço")
        key = " ";
    else if (key == "Nova linha" || key == "Enter") {
        if (lv_textarea_get_one_line(ui->editor_)) {
            if (ui->mode_ == Mode::Normal && ui->app_ == 7 && ui->calculatorMode_ != 2)
                ui->commands_.push_back({Command::Calculate, 0});
            else if (ui->mode_ == Mode::Normal && ui->app_ == 8)
                ui->commands_.push_back({Command::Convert, 0});
            return;
        }
        key = "\n";
    }
    if (ui->uppercase_ && key.size() == 1 && key[0] >= 'a' && key[0] <= 'z')
        key[0] -= 32;
    auto limit = ui->inputByteLimit();
    if (std::string(lv_textarea_get_text(ui->editor_)).size() + key.size() > limit) {
        ui->message("Limite do campo atingido", true);
        return;
    }
    lv_textarea_add_text(ui->editor_, key.c_str());
}
void Ui::keyboard(lv_obj_t *target) {
    if (platform_.physicalKeyboardConnected()) {
        if (keyboard_)
            lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
        if (accentPalette_)
            lv_obj_add_flag(accentPalette_, LV_OBJ_FLAG_HIDDEN);
        fitForm(target, 542);
        return;
    }
    static const char *lower[] = {
        "1",        "2",       "3",      "4",      "5",          "6",      "7", "8", "9", "0", "\n",
        "q",        "w",       "e",      "r",      "t",          "y",      "u", "i", "o", "p", "\n",
        "a",        "s",       "d",      "f",      "g",          "h",      "j", "k", "l", "ç", "\n",
        "Shift",    "z",       "x",      "c",      "v",          "b",      "n", "m", ",", ".", "\n",
        "-",        ":",       ";",      "/",      "@",          "+",      "*", "%", "(", ")", "\n",
        "Símbolos", "Acentos", "Espaço", "Apagar", "Nova linha", "Fechar", ""};
    static const char *upper[] = {
        "1",        "2",       "3",      "4",      "5",          "6",      "7", "8", "9", "0", "\n",
        "Q",        "W",       "E",      "R",      "T",          "Y",      "U", "I", "O", "P", "\n",
        "A",        "S",       "D",      "F",      "G",          "H",      "J", "K", "L", "Ç", "\n",
        "Shift",    "Z",       "X",      "C",      "V",          "B",      "N", "M", ",", ".", "\n",
        "-",        ":",       ";",      "/",      "@",          "+",      "*", "%", "(", ")", "\n",
        "Símbolos", "Acentos", "Espaço", "Apagar", "Nova linha", "Fechar", ""};
    static const char *symbols[] = {
        "1", "2",  "3",   "4",      "5",      "6",          "7",      "8", "9",  "0", "\n",
        "!", "@",  "#",   "$",      "%",      "^",          "&",      "*", "(",  ")", "\n",
        "-", "_",  "=",   "+",      "[",      "]",          "{",      "}", ";",  ":", "\n",
        "'", "\"", ",",   ".",      "<",      ">",          "/",      "?", "\\", "|", "\n",
        "`", "~",  "ABC", "Espaço", "Apagar", "Nova linha", "Fechar", ""};
    static const char *numberKeys[] = {"7",  "8", "9",  "\n",     "4",      "5", "6",
                                       "\n", "1", "2",  "3",      "\n",     "-", "0",
                                       ",",  ".", "\n", "Apagar", "Fechar", ""};
    static const char *plainNumbers[] = {"7", "8", "9", "\n", "4", "5",      "6",      "\n",
                                         "1", "2", "3", "\n", "0", "Apagar", "Fechar", ""};
    static const char *signedNumbers[] = {"7", "8", "9",  "\n", "4", "5",      "6",      "\n", "1",
                                          "2", "3", "\n", "-",  "0", "Apagar", "Fechar", ""};
    auto masked = std::find_if(masks_.begin(), masks_.end(),
                               [&](const MaskedInput &m) { return m.object == target; });
    bool numericInput = mode_ == Mode::Lock || masked != masks_.end();
    const char **numericMap = plainNumbers;
    if (masked != masks_.end()) {
        if (masked->kind == InputMask::Money)
            numericMap = numberKeys;
        else if (masked->kind == InputMask::Signed)
            numericMap = signedNumbers;
    }
    const char *enter = mode_ == Mode::Normal && (app_ == 8 || (app_ == 7 && calculatorMode_ != 2))
                            ? "Enter"
                            : "Nova linha";
    for (auto map : {lower, upper, symbols})
        for (unsigned i = 0; map[i][0]; ++i)
            if (std::string(map[i]) == "Nova linha" || std::string(map[i]) == "Enter")
                map[i] = enter;
    static lv_btnmatrix_ctrl_t controls[56];
    static bool configured = false;
    if (!configured) {
        for (auto &c : controls)
            c = 1 | LV_BTNMATRIX_CTRL_NO_REPEAT;
        configured = true;
    }
    if (!keyboard_) {
        keyboard_ = lv_keyboard_create(screen_);
        lv_obj_set_align(keyboard_, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keyboard_, 280, 392);
        lv_obj_set_size(keyboard_, 1000, 288);
        lv_obj_set_style_text_font(keyboard_, &pda_font_16, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard_, lv_color_hex(navy), 0);
        lv_obj_set_style_bg_opa(keyboard_, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(keyboard_, 6, 0);
        lv_obj_set_style_bg_color(keyboard_, lv_color_hex(0xffffff), LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(keyboard_, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_text_color(keyboard_, lv_color_hex(ink), LV_PART_ITEMS);
        lv_obj_set_style_border_width(keyboard_, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(keyboard_, lv_color_hex(0xcbd5e1), LV_PART_ITEMS);
        lv_obj_remove_event_cb(keyboard_, lv_keyboard_def_event_cb);
        lv_obj_add_event_cb(keyboard_, typed, LV_EVENT_VALUE_CHANGED, this);
    }
    lv_keyboard_set_map(keyboard_, LV_KEYBOARD_MODE_TEXT_LOWER,
                        numericInput ? numericMap
                        : symbols_   ? symbols
                        : uppercase_ ? upper
                                     : lower,
                        controls);
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(keyboard_, target);
    lv_obj_move_foreground(keyboard_);
    fitForm(target, 256);
    applyTheme(keyboard_);
}
void Ui::fitForm(lv_obj_t *target, int height) {
    if (!form_)
        return;
    lv_obj_set_height(form_, height);
    for (std::size_t i = 0; i < inputs_.size(); ++i) {
        int h = inputHeights_[i];
        if (inputs_[i] == target && height < 542)
            h = std::min(h, height - 48);
        lv_obj_set_height(inputs_[i], h);
    }
    lv_obj_update_layout(form_);
    if (height == 542) {
        lv_obj_scroll_to_y(form_, 0, LV_ANIM_OFF);
        return;
    }
    if (!target)
        return;
    lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF);
    lv_point_t caret;
    lv_label_get_letter_pos(lv_textarea_get_label(target), lv_textarea_get_cursor_pos(target),
                            &caret);
    int line = pda_font_16.line_height, scroll = lv_obj_get_scroll_top(target),
        content = lv_obj_get_content_height(target);
    if (caret.y < scroll)
        lv_obj_scroll_to_y(target, caret.y, LV_ANIM_OFF);
    else if (caret.y + line - scroll > content)
        lv_obj_scroll_to_y(target, caret.y + line - content, LV_ANIM_OFF);
}
void Ui::begin() {
    lastKeyboardConnected_ = platform_.physicalKeyboardConnected();
    screen_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen_, &pda_font_16, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(screen_);
    lastActivity_ = platform_.nowMs();
    auto now = platform_.localMinute();
    calendarDay_ = now.empty() ? "2026-01-01" : now.substr(0, 10);
    if (service_.state().draft.active) {
        draft_ = service_.state().draft;
        mode_ = Mode::Recovery;
    }
    if (!pda::value(service_.state().settings.fields, "pin").empty()) {
        locked_ = true;
        mode_ = Mode::Lock;
    }
    render();
    message(service_.storageMessage(), !service_.writable());
}
void Ui::activity() {
    lastActivity_ = platform_.nowMs();
}
pda::RecordKind Ui::kind() const {
    return app_ == 1   ? pda::RecordKind::Contact
           : app_ == 2 ? pda::RecordKind::Appointment
           : app_ == 3 ? pda::RecordKind::Task
           : app_ == 5 ? pda::RecordKind::Anniversary
           : app_ == 6 ? pda::RecordKind::Expense
                       : pda::RecordKind::Note;
}
const pda::Record *Ui::selectedRecord() const {
    return pda::findRecord(service_.state(), selected_);
}
void Ui::message(const std::string &s, bool error) {
    lv_label_set_text(status_, s.c_str());
    bool dark = std::string(theme_->id) == "ardosia";
    lv_obj_set_style_text_color(
        status_, lv_color_hex(error ? (dark ? 0xffb5b5 : 0xb91c1c) : (dark ? 0xb4d5a8 : 0x166534)),
        0);
}
void Ui::render() {
    if (mode_ == Mode::Normal && app_ == 13) {
        openPreferences("settings");
        return;
    }
    theme_ = &palette(pda::value(service_.state().settings.fields, "theme", "sereno"));
    lv_obj_set_style_bg_color(screen_, lv_color_hex(theme_->background), 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(theme_->ink), 0);
    picker_ = pickerContent_ = pickerTarget_ = pickerHour_ = pickerMinute_ = nullptr;
    pickerFields_.clear();
    productEditor_ = nullptr;
    inkCanvas_ = extrasLive_ = nullptr;
    editor_ = nullptr;
    keyboard_ = nullptr;
    accentPalette_ = nullptr;
    form_ = nullptr;
    masks_.clear();
    casedInputs_.clear();
    sliderLabels_.clear();
    inputs_.clear();
    inputLimits_.clear();
    inputHeights_.clear();
    lv_obj_clean(screen_);
    bindings_.clear();
    auto *head = panel(screen_, 0, 0, 1280, 56, navy);
    const char *productTitles[] = {"Novos aplicativos",
                                   "Miniplanilha",
                                   "Documentos",
                                   "Livros",
                                   "Galeria",
                                   "Finanças",
                                   "Localizar e substituir",
                                   "Lançamento financeiro",
                                   "Orçamento"};
    auto title = mode_ == Mode::Productivity && productPage_ >= 0 && productPage_ <= 8
                     ? productTitles[productPage_]
                     : names[app_];
    label(head, 16, 16, 600, "Tab5PDA  /  " + std::string(title), 0xffffff);
    clock_ = label(head, 680, 16, 580,
                   platform_.localMinute().empty()
                       ? "Hora não ajustada"
                       : pda::displayMinute(platform_.localMinute(), service_.state().settings),
                   0xffffff);
    auto *nav = panel(screen_, 0, 56, 272, 664, 0xdbe5ef);
    label(nav, 12, 12, 248, "Aplicações", navy);
    for (int i = 0; i < 17; ++i) {
        auto *b = button(nav, 10 + (i % 2) * 130, 44 + (i / 2) * 60, 122, 52, names[i],
                         Command::App, i, colors[i]);
        if (i == app_) {
            lv_obj_set_style_border_width(b, 3, 0);
            lv_obj_set_style_border_color(b, lv_color_hex(0xffffff), 0);
        }
    }
    label(nav, 12, 600, 248, "Agenda pessoal\nDados no microSD", navy);
    body_ = panel(screen_, 280, 56, 1000, 624, background);
    status_ = label(screen_, 292, 687, 972, service_.storageMessage());
    if (mode_ == Mode::Productivity)
        renderProductivity();
    else if (mode_ == Mode::Extras || mode_ == Mode::Drawing || mode_ == Mode::FileView ||
             mode_ == Mode::FileDelete || mode_ == Mode::SyncPreview)
        renderExtras();
    else if (mode_ == Mode::Edit)
        renderEditor();
    else if (mode_ == Mode::Preferences)
        renderPreferences();
    else if (mode_ == Mode::Trash || mode_ == Mode::Purge || mode_ == Mode::Exchange ||
             mode_ == Mode::ImportPreview || mode_ == Mode::MoveConfirm)
        renderPriority();
    else if (mode_ != Mode::Normal)
        renderOverlay();
    else if (app_ == 6 && !result_.empty()) {
        button(body_, 12, 8, 220, 52, "Voltar às despesas", Command::App, 6);
        label(body_, 12, 96, 960, result_);
    } else if (app_ == 0)
        renderToday();
    else if ((app_ >= 1 && app_ <= 6) || app_ == 11)
        renderRecords();
    else if (app_ == 10) {
        if (scheduleMode_)
            renderSchedule();
        else
            renderCalendar();
    } else
        renderTools();
    if ((mode_ == Mode::Productivity || (mode_ == Mode::Normal && (app_ == 7 || app_ == 8))) &&
        !inputs_.empty()) {
        // Keep the toolbar above a scrollable form, as in the organizer editor.
        // The same keyboard sizing logic can then expose every active field.
        std::vector<lv_obj_t *> fields;
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(body_); ++i) {
            auto *child = lv_obj_get_child(body_, i);
            if (lv_obj_get_y_aligned(child) >= 68)
                fields.push_back(child);
        }
        form_ = panel(body_, 0, 68, 1000, 542, background);
        lv_obj_add_flag(form_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(form_, LV_DIR_VER);
        for (auto *child : fields) {
            auto x = lv_obj_get_x_aligned(child), y = lv_obj_get_y_aligned(child);
            lv_obj_set_parent(child, form_);
            lv_obj_set_pos(child, x, y - 68);
        }
    }
    applyTheme(screen_);
}
void Ui::renderDetail(lv_obj_t *parent, const pda::Record *r, int y) {
    if (!r) {
        label(parent, 18, y + 12, 574, "Selecione um registro.");
        return;
    }
    constexpr int width = 574;
    auto height = [&](const std::string &text, const lv_font_t *font) {
        lv_point_t size;
        lv_txt_get_size(&size, text.c_str(), font, 0, 3, width, LV_TEXT_FLAG_NONE);
        return size.y;
    };
    auto *title = label(parent, 18, y + 10, width, r->title);
    lv_obj_set_style_text_font(title, &pda_font_28, 0);
    lv_obj_set_style_text_line_space(title, 3, 0);
    y += height(r->title, &pda_font_28) + 24;
    label(parent, 18, y, width, "Registro #" + std::to_string(r->id), theme_->muted);
    y += 40;
    auto row = [&](const std::string &caption, const std::string &value, bool prose = false) {
        if (value.empty())
            return;
        auto *name = label(parent, 18, y, width, caption);
        lv_obj_set_style_text_font(name, &pda_font_16_bold, 0);
        y += 28;
        auto text = prose ? justified(value, width) : value;
        auto *body = label(parent, 18, y, width, text);
        lv_obj_set_style_text_line_space(body, 3, 0);
        y += height(text, &pda_font_16) + 24;
    };
    if (r->kind == pda::RecordKind::Task)
        row("Situação", r->completed ? "Concluída" : "Pendente");
    for (const auto &field : pda::recordFields(r->kind)) {
        std::string key = field.key;
        if (key == "title" || key == "detail")
            continue;
        auto value = pda::recordValue(*r, key);
        if (value.empty())
            continue;
        if (key == "when" || key == "until") {
            pda::CivilMinute date;
            if (pda::parseCivilMinute(value, date)) {
                int offset =
                    std::atoi(pda::value(r->fields, "offset",
                                         std::to_string(service_.state().settings.utcOffsetMinutes))
                                  .c_str());
                auto at = pda::formatCivilMinute(pda::unixToCivil(
                    pda::civilToUnix(date, offset), service_.state().settings.utcOffsetMinutes));
                if (key == "when" && !dailyFilter_.empty())
                    pda::localOccurrenceOn(*r, dailyFilter_,
                                           service_.state().settings.utcOffsetMinutes, at);
                value = pda::displayMinute(at, service_.state().settings);
            }
        } else if (key == "repeat") {
            value = value == "daily"     ? "Diária"
                    : value == "weekly"  ? "Semanal"
                    : value == "monthly" ? "Mensal"
                    : value == "yearly"  ? "Anual"
                                         : "Não repetir";
        } else if (key == "priority")
            value = value == "1" ? "Alta" : value == "3" ? "Baixa" : "Média";
        else if (key == "allDay" || key == "favorite")
            value = value == "1" ? "Sim" : "Não";
        else if (key == "reminder")
            value = value == "-1"  ? "Desativado"
                    : value == "0" ? "No horário"
                                   : value + " minutos antes";
        else if (key == "parent" || key == "contact") {
            auto *linked =
                pda::findRecord(service_.state(), std::strtoul(value.c_str(), nullptr, 10));
            if (linked)
                value = linked->title;
        }
        auto caption = shortName(field.label);
        auto optional = caption.find(" (opcional)");
        if (optional != std::string::npos)
            caption.erase(optional);
        row(caption, value, key == "address");
    }
    row(r->kind == pda::RecordKind::Note ? "Texto" : "Observações", r->detail, true);
    row("Concluída em", pda::value(r->fields, "completedAt"));
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
}
void Ui::renderRecords() {
    button(body_, 12, 8, 120, 46, "Novo", Command::New);
    button(body_, 144, 8, 140, 46, "Buscar", Command::Search);
    button(body_, 296, 8, 120, 46, "Editar", Command::Edit);
    button(body_, 428, 8, 120, 46, "Excluir", Command::Delete);
    if (app_ == 2) {
        button(body_, 560, 8, 160, 46, "Calendário", Command::Calendar);
        button(body_, 732, 8, 250, 46, "Editar ocorrência", Command::Occurrence);
    }
    if (app_ == 3) {
        button(body_, 560, 8, 220, 46, "Concluir / reabrir", Command::Toggle);
        const char *filters[] = {"Todas", "Pendentes", "Atrasadas", "Prioridade alta",
                                 "Concluídas"};
        button(body_, 792, 8, 190, 46, filters[taskFilter_], Command::TaskFilter);
    }
    if (app_ == 4) {
        button(body_, 560, 8, 200, 46, "Cadernos", Command::Notebook, UINT32_MAX);
        button(body_, 772, 8, 210, 46, "Mais ferramentas", Command::Extras);
    }
    if (app_ == 6)
        button(body_, 560, 8, 220, 46, "Totais por período", Command::Totals);
    std::vector<const pda::Record *> records;
    for (const auto &r : service_.state().records) {
        if (pda::trashed(r))
            continue;
        if (app_ == 4 &&
            ((!notebookFilter_.empty() && pda::value(r.fields, "notebook") != notebookFilter_) ||
             (favoritesOnly_ && pda::value(r.fields, "favorite") != "1")))
            continue;
        bool type = app_ == 11 || r.kind == kind() ||
                    (!dailyFilter_.empty() && r.kind == pda::RecordKind::Anniversary);
        std::string at;
        bool taskMatch = true;
        if (app_ == 3) {
            const auto now = platform_.localMinute();
            auto due = r.when;
            pda::CivilMinute t;
            if (pda::parseCivilMinute(due, t))
                due = pda::formatCivilMinute(pda::unixToCivil(
                    pda::civilToUnix(
                        t, std::atoi(pda::value(
                                         r.fields, "offset",
                                         std::to_string(service_.state().settings.utcOffsetMinutes))
                                         .c_str())),
                    service_.state().settings.utcOffsetMinutes));
            taskMatch =
                taskFilter_ == 0 || (taskFilter_ == 1 && !r.completed) ||
                (taskFilter_ == 2 && !r.completed && !due.empty() && !now.empty() && due < now) ||
                (taskFilter_ == 3 && pda::value(r.fields, "priority", "2") == "1") ||
                (taskFilter_ == 4 && r.completed);
        }
        if (type && taskMatch && pda::matches(r, query_) &&
            (dailyFilter_.empty() ||
             pda::localOccurrenceOn(r, dailyFilter_, service_.state().settings.utcOffsetMinutes,
                                    at)))
            records.push_back(&r);
    }
    std::sort(records.begin(), records.end(), [&](const pda::Record *a, const pda::Record *b) {
        if (app_ == 3) {
            if (a->completed != b->completed)
                return !a->completed;
            auto pa = pda::value(a->fields, "priority", "2"),
                 pb = pda::value(b->fields, "priority", "2");
            if (pa != pb)
                return pa < pb;
        }
        if ((app_ == 2 || app_ == 3 || app_ == 6) && a->when != b->when)
            return a->when < b->when;
        return pda::searchKey(a->title) < pda::searchKey(b->title);
    });
    if (!std::any_of(records.begin(), records.end(),
                     [&](const pda::Record *r) { return r->id == selected_; }))
        selected_ = records.empty() ? 0 : records.front()->id;
    auto *list = panel(body_, 12, 68, 346, 542);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    label(list, 10, 8, 326,
          std::to_string(records.size()) + " registros" +
              (dailyFilter_.empty() ? "" : " / " + dailyFilter_));
    listPage_ = std::clamp(listPage_, 0, records.empty() ? 0 : int((records.size() - 1) / 32));
    button(list, 8, 38, 150, 44, "Página -", Command::ListPage, 0);
    button(list, 170, 38, 150, 44, "Página +", Command::ListPage, 1);
    const std::size_t firstRow = listPage_ * 32;
    for (std::size_t i = firstRow; i < std::min(records.size(), firstRow + 32); ++i) {
        auto *r = records[i];
        button(list, 8, 92 + (i - firstRow) * 64, 330, 56,
               (r->kind == pda::RecordKind::Task ? (r->completed ? "[x] " : "[ ] ") : "") +
                   r->title,
               Command::Select, r->id, r->id == selected_ ? colors[app_] : 0x64748b);
    }
    auto *detail = panel(body_, 370, 68, 618, 542);
    if ((app_ == 4 || app_ == 2) && selectedRecord()) {
        if (app_ == 4)
            button(detail, 14, 8, 174, 46, "Favorita", Command::Favorite, selected_);
        button(detail, 200, 8, 174, 46, "Áudio", Command::Extras, 2);
        button(detail, 386, 8, 174, 46, "Desenhar", Command::Draw, selected_);
        renderDetail(detail, selectedRecord(), 70);
    } else if (app_ == 3 && selectedRecord()) {
        button(detail, 14, 8, 260, 46, "Nova subtarefa", Command::NewSubtask, selected_);
        int y = 64;
        int shown = 0;
        for (const auto &r : service_.state().records)
            if (shown < 24 && !pda::trashed(r) &&
                pda::value(r.fields, "parent") == std::to_string(selected_)) {
                button(detail, 14, y, 574, 46, (r.completed ? "[x] " : "[ ] ") + r.title,
                       Command::ToggleChild, r.id);
                y += 54;
                ++shown;
            }
        renderDetail(detail, selectedRecord(), y);
    } else
        renderDetail(detail, selectedRecord());
}
void Ui::renderCalendar() {
    pda::CivilMinute date;
    pda::parseCivilMinute(calendarDay_ + " 00:00", date);
    button(body_, 12, 8, 100, 46, "Anterior", Command::Month, 0);
    label(body_, 130, 20, 280, calendarDay_.substr(0, 7), colors[10]);
    button(body_, 420, 8, 100, 46, "Próximo", Command::Month, 1);
    button(body_, 548, 8, 220, 46, "Novo compromisso", Command::New);
    button(body_, 780, 8, 208, 46, "Lista do dia", Command::Calendar, 1);
    button(body_, 12, 64, 160, 44, "Mês", Command::ScheduleMode, 0);
    button(body_, 184, 64, 160, 44, "Semana", Command::ScheduleMode, 2);
    button(body_, 356, 64, 160, 44, "Dia", Command::ScheduleMode, 1);
    auto first = date;
    first.day = 1;
    int weekday = (pda::civilToUnix(first, 0) / 86400 + 4) % 7;
    const char *week[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
    for (int i = 0; i < 7; ++i)
        label(body_, 14 + i * 78, 118, 74, week[i], colors[10]);
    for (int day = 1; day <= pda::daysInMonth(date.year, date.month); ++day) {
        auto d = date;
        d.day = day;
        auto civil = pda::formatCivilMinute(d).substr(0, 10);
        bool marker = false;
        for (const auto &r : service_.state().records) {
            if (pda::trashed(r))
                continue;
            std::string at;
            if ((r.kind == pda::RecordKind::Appointment ||
                 r.kind == pda::RecordKind::Anniversary) &&
                pda::localOccurrenceOn(r, civil, service_.state().settings.utcOffsetMinutes, at))
                marker = true;
        }
        int pos = weekday + day - 1;
        button(body_, 12 + (pos % 7) * 78, 148 + (pos / 7) * 68, 72, 60,
               std::to_string(day) + (marker ? " *" : ""), Command::Day, day,
               day == date.day ? colors[10] : 0x64748b);
    }
    auto *details = panel(body_, 568, 68, 420, 542);
    std::string list = "Compromissos de " + calendarDay_ + "\n\n";
    for (const auto &r : service_.state().records) {
        if (pda::trashed(r))
            continue;
        std::string at;
        if ((r.kind == pda::RecordKind::Appointment || r.kind == pda::RecordKind::Anniversary) &&
            pda::localOccurrenceOn(r, calendarDay_, service_.state().settings.utcOffsetMinutes, at))
            list += at.substr(11) + "  " + r.title + "\n\n";
    }
    label(details, 14, 14, 390, list);
    lv_obj_add_flag(details, LV_OBJ_FLAG_SCROLLABLE);
}
std::vector<std::pair<std::string, std::string>> Ui::fieldOptions(const std::string &key) const {
    if (key == "repeat")
        return {{"none", "Não repetir"},
                {"daily", "Diariamente"},
                {"weekly", "Semanalmente"},
                {"monthly", "Mensalmente"},
                {"yearly", "Anualmente"}};
    if (key == "priority")
        return {{"1", "Alta"}, {"2", "Média"}, {"3", "Baixa"}};
    if (key == "favorite")
        return {{"0", "Não"}, {"1", "Sim"}};
    if (key == "allDay")
        return {{"0", "Não"}, {"1", "Sim"}};
    if (key == "leap")
        return {{"feb28", "Usar 28 de fevereiro"}, {"skip", "Só nos anos bissextos"}};
    if (key == "currency")
        return {{"BRL", "Real (BRL)"}, {"USD", "Dólar (USD)"}, {"EUR", "Euro (EUR)"}};
    if (key == "reminder")
        return {{"-1", "Sem aviso"},        {"0", "Na hora"},           {"5", "5 minutos antes"},
                {"15", "15 minutos antes"}, {"30", "30 minutos antes"}, {"60", "1 hora antes"},
                {"1440", "1 dia antes"}};
    if (key == "category" || key == "contact" || key == "parent" || key == "project") {
        std::vector<std::pair<std::string, std::string>> options{{"", "Sem seleção"}};
        for (const auto &r : service_.state().records) {
            if (pda::trashed(r))
                continue;
            if (key == "parent" && r.kind == pda::RecordKind::Task && r.id != draft_.record.id)
                options.push_back({std::to_string(r.id), r.title});
            if (key == "project") {
                auto project = pda::value(r.fields, "project");
                if (!project.empty() &&
                    std::none_of(options.begin(), options.end(),
                                 [&](const auto &o) { return o.first == project; }))
                    options.push_back({project, project});
            }
            if (key == "contact" && r.kind == pda::RecordKind::Contact)
                options.push_back({std::to_string(r.id), r.title});
            if (key == "category") {
                auto text = pda::value(r.fields, "category");
                if (!text.empty() && std::none_of(options.begin(), options.end(),
                                                  [&](const auto &o) { return o.first == text; }))
                    options.push_back({text, text});
            }
        }
        return options;
    }
    return {};
}
void Ui::pickerButton(lv_obj_t *parent, int x, int y, lv_obj_t *target, const std::string &key) {
    const int width = lv_obj_get_width(target);
    lv_obj_set_width(target, width - 134);
    pickerFields_.push_back({target, key});
    button(parent, x + width - 126, y, 126, lv_obj_get_height(target), "Selecionar",
           Command::PickerOpen, pickerFields_.size() - 1);
}
void Ui::openPicker(unsigned index) {
    if (index >= pickerFields_.size() || picker_)
        return;
    dispatch(Command::KeyboardHide);
    pickerTarget_ = pickerFields_[index].target;
    pickerKey_ = pickerFields_[index].key;
    pickerOptions_ = fieldOptions(pickerKey_);
    pickerPage_ = 0;
    pickerDateOnly_ =
        pickerKey_ == "repeatEnd" || pickerKey_ == "occurrence" || pickerKey_ == "ledgerDate";
    auto value = inputValue(pickerTarget_);
    if (pickerDateOnly_ && !value.empty())
        value += " 00:00";
    if (!pda::parseCivilMinute(value, pickerDate_)) {
        capture();
        auto seed = pickerKey_ == "until" || pickerKey_ == "repeatEnd" ? draft_.record.when
                                                                       : platform_.localMinute();
        if (!pda::parseCivilMinute(seed, pickerDate_))
            pickerDate_ = pda::CivilMinute{};
    }
    pickerBindingsStart_ = bindings_.size();
    picker_ = panel(screen_, 0, 56, 1280, 624, 0x64748b);
    drawPicker();
}
void Ui::readPickerTime() {
    if (pickerHour_ && pickerMinute_) {
        pickerDate_.hour = lv_dropdown_get_selected(pickerHour_);
        pickerDate_.minute = lv_dropdown_get_selected(pickerMinute_);
    }
}
void Ui::closePicker() {
    lv_obj_del(picker_);
    while (bindings_.size() > pickerBindingsStart_)
        bindings_.pop_back();
    picker_ = pickerContent_ = pickerHour_ = pickerMinute_ = nullptr;
    pickerTarget_ = nullptr;
}
void Ui::drawPicker() {
    if (pickerContent_)
        lv_obj_del(pickerContent_);
    while (bindings_.size() > pickerBindingsStart_)
        bindings_.pop_back();
    pickerHour_ = pickerMinute_ = nullptr;
    pickerContent_ = panel(picker_, 290, 12, 980, 600);
    auto *p = pickerContent_;
    label(p, 20, 14, 940, pickerOptions_.empty() ? "Selecionar data e hora" : "Selecionar opção",
          navy);
    if (!pickerOptions_.empty()) {
        auto *list = panel(p, 16, 54, 948, 440, background);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        auto current = inputValue(pickerTarget_);
        pickerPage_ =
            std::clamp(pickerPage_, 0, std::max(0, int((pickerOptions_.size() - 1) / 32)));
        button(list, 8, 8, 220, 46, "Página -", Command::PickerPage, 0);
        button(list, 240, 8, 220, 46, "Página +", Command::PickerPage, 1);
        const auto first = std::size_t(pickerPage_ * 32);
        for (std::size_t i = first; i < std::min(pickerOptions_.size(), first + 32); ++i)
            button(list, 8, 66 + (i - first) * 58, 916, 50,
                   (current == pickerOptions_[i].first ? "> " : "") + pickerOptions_[i].second,
                   Command::PickerOption, i);
    } else {
        button(p, 20, 52, 100, 46, "Ano -", Command::PickerAdjust, 2);
        button(p, 128, 52, 100, 46, "Mês -", Command::PickerAdjust, 0);
        char heading[32];
        std::snprintf(heading, sizeof(heading), "%02d / %04d", pickerDate_.month, pickerDate_.year);
        label(p, 252, 66, 220, heading, navy);
        button(p, 486, 52, 100, 46, "Mês +", Command::PickerAdjust, 1);
        button(p, 594, 52, 100, 46, "Ano +", Command::PickerAdjust, 3);
        const char *week[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
        for (int i = 0; i < 7; ++i)
            label(p, 24 + i * 96, 112, 90, week[i]);
        pda::CivilMinute first{pickerDate_.year, pickerDate_.month, 1, 0, 0};
        int weekday = (pda::civilToUnix(first, 0) / 86400 + 4) % 7;
        for (int day = 1; day <= pda::daysInMonth(first.year, first.month); ++day) {
            int cell = weekday + day - 1;
            button(p, 20 + cell % 7 * 96, 140 + cell / 7 * 56, 88, 48, std::to_string(day),
                   Command::PickerDay, day, day == pickerDate_.day ? colors[app_] : navy);
        }
        if (!pickerDateOnly_) {
            label(p, 728, 112, 220, "Hora");
            std::string hours, minutes;
            for (int i = 0; i < 60; ++i) {
                char n[8];
                std::snprintf(n, sizeof(n), "%02d", i);
                if (i < 24)
                    hours += (i ? "\n" : "") + std::string(n);
                minutes += (i ? "\n" : "") + std::string(n);
            }
            pickerHour_ = dropdown(p, 728, 140, hours, pickerDate_.hour);
            lv_obj_set_width(pickerHour_, 100);
            label(p, 728, 222, 220, "Minuto");
            pickerMinute_ = dropdown(p, 728, 250, minutes, pickerDate_.minute);
            lv_obj_set_width(pickerMinute_, 100);
            // The modal controls do not change the form's active input.
            lv_obj_remove_event_cb(pickerHour_, controlChanged);
            lv_obj_remove_event_cb(pickerMinute_, controlChanged);
        }
        label(p, 20, 486, 930, "Toque no dia e escolha o horário. Aplicar confirma a seleção.");
        button(p, 20, 534, 240, 50, "Aplicar data", Command::PickerApply);
    }
    button(p, 278, 534, 240, 50, "Limpar campo", Command::PickerClear);
    button(p, 536, 534, 240, 50, "Cancelar seleção", Command::PickerCancel);
    applyTheme(picker_);
}

void Ui::renderEditor() {
    button(body_, 12, 8, 150, 46, "Salvar", Command::Save);
    button(body_, 174, 8, 150, 46, "Voltar", Command::Cancel);
    button(body_, 336, 8, 150, 46, "Acentos", Command::Accents);
    button(body_, 498, 8, 200, 46, "Fechar teclado", Command::KeyboardHide);
    button(body_, 710, 8, 274, 46, "Guardar rascunho", Command::Shelf);
    form_ = panel(body_, 12, 68, 976, 542);
    lv_obj_add_flag(form_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(form_, LV_DIR_VER);
    const auto &fields = pda::recordFields(draft_.record.kind);
    inputs_.resize(fields.size());
    inputLimits_.resize(fields.size());
    inputHeights_.resize(fields.size(), 50);
    const bool noteForm = draft_.record.kind == pda::RecordKind::Note;
    const int columns = noteForm ? 3 : 2, rowHeight = noteForm ? 68 : 84;
    int position = 0, detail = -1;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto &f = fields[i];
        inputLimits_[i] = f.limit;
        if (std::string(f.key) == "detail") {
            detail = i;
            continue;
        }
        int x = 12 + (position % columns) * (noteForm ? 322 : 484),
            y = 10 + (position / columns) * rowHeight;
        ++position;
        label(form_, x, y, noteForm ? 310 : 460, fieldCaption(f.label), colors[app_]);
        inputs_[i] = input(form_, x, y + 24, noteForm ? 310 : 460, noteForm ? 40 : 50,
                           fieldText(f.key, pda::recordValue(draft_.record, f.key)), f.limit);
        inputHeights_[i] = noteForm ? 40 : 50;
        std::string key = f.key;
        if (key == "title")
            caseInput(inputs_[i], draft_.record.kind == pda::RecordKind::Contact ||
                                          draft_.record.kind == pda::RecordKind::Anniversary
                                      ? TextCase::Words
                                      : TextCase::Sentence);
        else if (key == "company" || key == "location" || key == "category" || key == "project" ||
                 key == "notebook")
            caseInput(inputs_[i], TextCase::Words);
        else if (key == "address")
            caseInput(inputs_[i], TextCase::Sentence);
        else if (key == "currency")
            caseInput(inputs_[i], TextCase::Upper);
        if (key == "when" || key == "until")
            mask(inputs_[i], brazilian() ? InputMask::DateTimeBR : InputMask::DateTime,
                 brazilian() ? "DD/MM/AAAA HH:MM" : "AAAA-MM-DD HH:MM", 16);
        else if (key == "repeatEnd")
            mask(inputs_[i], brazilian() ? InputMask::DateBR : InputMask::Date,
                 brazilian() ? "DD/MM/AAAA" : "AAAA-MM-DD", 10);
        else if (key == "amount")
            mask(inputs_[i], InputMask::Money, "0,00 (até 2 casas decimais)", 16);
        else if (key == "reminder")
            mask(inputs_[i], InputMask::Signed, "Minutos; -1 desliga", 5);
        else if (key == "priority" || key == "allDay")
            mask(inputs_[i], InputMask::Unsigned, "Número", 1);
        else if (key == "birthYear")
            mask(inputs_[i], InputMask::Unsigned, "AAAA", 4);
        else if (key == "contact" || key == "parent")
            mask(inputs_[i], InputMask::Unsigned, "ID numérico (opcional)", 10);
        if (key == "when" || key == "until" || key == "repeatEnd" || !fieldOptions(key).empty())
            pickerButton(form_, x, y + 24, inputs_[i], key);
    }
    if (detail >= 0) {
        int y = 10 + ((position + columns - 1) / columns) * rowHeight;
        label(form_, 12, y, 952, fields[detail].label, colors[app_]);
        inputs_[detail] =
            input(form_, 12, y + 24, 952, std::max(76, 530 - y - 24),
                  pda::recordValue(draft_.record, "detail"), fields[detail].limit, true);
        inputHeights_[detail] = std::max(76, 530 - y - 24);
        caseInput(inputs_[detail], TextCase::Sentence);
    }
    field_ = std::clamp(field_, 0, int(inputs_.size()) - 1);
    editor_ = inputs_[field_];
}
void Ui::capture() {
    if (mode_ == Mode::Exchange) {
        captureExchange();
        return;
    }
    if (mode_ == Mode::Edit) {
        const auto &fields = pda::recordFields(draft_.record.kind);
        for (std::size_t i = 0; i < inputs_.size(); ++i)
            pda::setRecordValue(draft_.record, fields[i].key, inputValue(inputs_[i]));
        int64_t cents;
        if (draft_.record.kind == pda::RecordKind::Expense &&
            pda::parseMoney(pda::value(draft_.record.fields, "amount"), cents))
            draft_.record.fields["amount"] = pda::formatMoney(cents);
        draft_.field = field_;
    } else if (mode_ == Mode::Preferences) {
        for (std::size_t i = 0; i < inputs_.size(); ++i)
            preferences_[i].second = inputValue(inputs_[i]);
    } else if (editor_ && lv_obj_check_type(editor_, &lv_textarea_class)) {
        if (mode_ == Mode::Search)
            query_ = lv_textarea_get_text(editor_);
        else if (mode_ == Mode::Occurrence)
            calendarDay_ = inputValue(editor_);
        else if (mode_ == Mode::Normal && (app_ == 7 || app_ == 8))
            captureTools();
    }
}
void Ui::startEdit(const pda::Record *r) {
    if (!service_.writable()) {
        message("microSD necessário para editar; use Diagnóstico / Reabrir", true);
        return;
    }
    if (draft_.active || service_.state().draft.active) {
        pendingCreate_ = true;
        pendingApp_ = app_;
        pendingExisting_ = r != nullptr;
        pendingRecord_ = r ? *r : pda::Record{};
        if (!draft_.active)
            draft_ = service_.state().draft;
        mode_ = Mode::Recovery;
        render();
        return;
    }
    activeShelf_.clear();
    pendingCreate_ = false;
    draft_ = pda::Draft{};
    draft_.active = true;
    if (r)
        draft_.record = *r;
    else {
        draft_.record.kind = kind();
        if (app_ == 6)
            draft_.record.fields["currency"] = "BRL";
        if (draft_.record.kind == pda::RecordKind::Note) {
            if (!notebookFilter_.empty())
                draft_.record.fields["notebook"] = notebookFilter_;
            if (favoritesOnly_)
                draft_.record.fields["favorite"] = "1";
        }
        if (!dailyFilter_.empty())
            draft_.record.when = dailyFilter_ + " 09:00";
        if (!newWhen_.empty()) {
            draft_.record.when = newWhen_;
            pda::CivilMinute t;
            if (pda::parseCivilMinute(newWhen_, t))
                draft_.record.fields["until"] =
                    pda::formatCivilMinute(pda::unixToCivil(pda::civilToUnix(t, 0) + 3600, 0));
        }
        if (!subtaskParent_.empty()) {
            draft_.record.fields["parent"] = subtaskParent_;
            auto *parent = pda::findRecord(service_.state(), std::stoul(subtaskParent_));
            if (parent)
                draft_.record.fields["project"] = pda::value(parent->fields, "project");
        }
    }
    newWhen_.clear();
    subtaskParent_.clear();
    if (!r && draft_.record.kind == pda::RecordKind::Note && template_ >= 0) {
        draft_.record.title = template_ == 0 ? "Reunião" : template_ == 1 ? "Lista" : "Diário";
        draft_.record.detail = template_ == 0
                                   ? "Participantes:\n\nPauta:\n\nDecisões:\n\nPróximas ações:\n"
                               : template_ == 1 ? "[ ] \n[ ] \n[ ] "
                                                : platform_.localMinute() + "\n\n";
    }
    template_ = -1;
    field_ = 0;
    dirty_ = true;
    overlapConfirmed_ = false;
    lastEdit_ = platform_.nowMs();
    mode_ = Mode::Edit;
    render();
}
void Ui::persistDraft() {
    if (!dirty_ || !draft_.active)
        return;
    if (mode_ == Mode::Edit)
        capture();
    lastDraft_ = platform_.nowMs();
    if (service_.saveDraft(draft_)) {
        dirty_ = false;
        message("Rascunho salvo no microSD");
    } else
        message(service_.error() + " / texto mantido na RAM", true);
}
void Ui::saveEditor() {
    capture();
    pda::normalizeRecord(draft_.record);
    const auto &formFields = pda::recordFields(draft_.record.kind);
    for (std::size_t i = 0; i < inputs_.size(); ++i) {
        auto value = pda::recordValue(draft_.record, formFields[i].key);
        auto displayed = fieldText(formFields[i].key, value);
        if (displayed != lv_textarea_get_text(inputs_[i]))
            lv_textarea_set_text(inputs_[i], displayed.c_str());
    }
    std::string error;
    if (!pda::validRecord(draft_.record, false, error)) {
        message(error, true);
        return;
    }
    if (draft_.record.kind == pda::RecordKind::Appointment && !overlapConfirmed_) {
        pda::CivilMinute a, b;
        pda::parseCivilMinute(draft_.record.when, a);
        pda::parseCivilMinute(pda::value(draft_.record.fields, "until", draft_.record.when), b);
        int offset =
            std::atoi(pda::value(draft_.record.fields, "offset",
                                 std::to_string(service_.state().settings.utcOffsetMinutes))
                          .c_str());
        if (pda::value(draft_.record.fields, "allDay", "0") == "1")
            offset = service_.state().settings.utcOffsetMinutes;
        auto start = pda::civilToUnix(a, offset), end = pda::civilToUnix(b, offset);
        for (const auto &r : service_.state().records) {
            if (pda::trashed(r))
                continue;
            if (r.kind != pda::RecordKind::Appointment || r.id == draft_.record.id ||
                std::to_string(r.id) == pda::value(draft_.record.fields, "origin"))
                continue;
            pda::CivilMinute x, y, z;
            pda::parseCivilMinute(r.when, y);
            pda::parseCivilMinute(pda::value(r.fields, "until", r.when), z);
            auto duration = pda::civilToUnix(z, 0) - pda::civilToUnix(y, 0);
            int source =
                std::atoi(pda::value(r.fields, "offset",
                                     std::to_string(service_.state().settings.utcOffsetMinutes))
                              .c_str());
            if (pda::value(r.fields, "allDay", "0") == "1")
                source = service_.state().settings.utcOffsetMinutes;
            auto from = pda::formatCivilMinute(
                pda::unixToCivil(start - duration + (duration ? 60 : 0), source));
            if (from < "2000-01-01 00:00")
                from = "2000-01-01 00:00";
            auto at = pda::nextOccurrence(r, from);
            if (!pda::parseCivilMinute(at, x))
                continue;
            auto old = pda::civilToUnix(x, source);
            bool conflict = start == end
                                ? old <= start && (duration ? start < old + duration : start == old)
                            : duration ? start < old + duration && end > old
                                       : start <= old && old < end;
            if (conflict) {
                overlapConfirmed_ = true;
                message("Horários sobrepostos: toque Salvar novamente para confirmar", true);
                return;
            }
        }
    }
    if (draft_.record.kind == pda::RecordKind::Task && draft_.record.id) {
        if (auto *current = pda::findRecord(service_.state(), draft_.record.id)) {
            draft_.record.completed = current->completed;
            if (current->fields.count("completedAt"))
                draft_.record.fields["completedAt"] = current->fields.at("completedAt");
            else
                draft_.record.fields.erase("completedAt");
        }
    }
    if (!service_.saveRecord(draft_.record)) {
        message(service_.error(), true);
        return;
    }
    if (draft_.record.id)
        selected_ = draft_.record.id;
    else
        selected_ = service_.state().nextId - 1;
    if (!activeShelf_.empty()) {
        platform_.removeFile(activeShelf_);
        activeShelf_.clear();
    }
    draft_ = pda::Draft{};
    dirty_ = false;
    mode_ = Mode::Normal;
    render();
    message("Registro salvo no microSD");
}
void Ui::openPreferences(const std::string &purpose) {
    field_ = 0;
    purpose_ = purpose;
    if (purpose == "settings")
        initialClock_ = platform_.localMinute();
    preferences_.clear();
    const auto &s = service_.state().settings;
    if (purpose == "settings")
        preferences_ = {
            {"Brilho: 16 a 255", std::to_string(s.brightness)},
            {"Volume: 0 a 255", std::to_string(s.volume)},
            {"Tela: segundos (0 sempre ligada)", std::to_string(s.screenTimeoutSeconds)},
            {"Fuso UTC: horas", std::to_string(s.utcOffsetMinutes / 60)},
            {"Formato de data: ISO ou BR", pda::value(s.fields, "dateFormat", "ISO")},
            {"Hora manual: AAAA-MM-DD HH:MM", platform_.localMinute()},
            {"Tema visual", palette(pda::value(s.fields, "theme", "sereno")).name}};
    if (purpose == "network")
        preferences_ = {{"SSID Wi-Fi (vazio desliga)", pda::value(s.fields, "wifiSSID")},
                        {"Senha Wi-Fi", pda::value(s.fields, "wifiPass")}};
    if (purpose == "pin")
        preferences_ = {{"Novo PIN: 4 a 8 dígitos", ""},
                        {"Bloqueio automático: segundos (0 desliga)",
                         pda::value(s.fields, "lockSeconds", "120")}};
    if (purpose == "world")
        for (int i = 0; i < 3; ++i) {
            auto key = "city" + std::to_string(i);
            preferences_.push_back(
                {"Cidade " + std::to_string(i + 1), pda::value(s.fields, key,
                                                               i == 0   ? "Cuiabá"
                                                               : i == 1 ? "Londres"
                                                                        : "Tóquio")});
            int offset = std::atoi(pda::value(s.fields, key + "Offset",
                                              i == 0   ? "-240"
                                              : i == 1 ? "0"
                                                       : "540")
                                       .c_str());
            preferences_.push_back({"Fuso UTC: horas", std::to_string(offset / 60)});
        }
    if (purpose == "totals")
        preferences_ = {{"Período: AAAA-MM, AAAA ou vazio", ""}};
    mode_ = Mode::Preferences;
    render();
}
void Ui::renderPreferences() {
    button(body_, 12, 8, 150, 46, "Salvar", Command::SettingsSave);
    button(body_, 174, 8, 150, 46, "Cancelar", Command::Cancel);
    button(body_, 336, 8, 200, 46, "Fechar teclado", Command::KeyboardHide);
    label(body_, 560, 20, 428, "Todos os campos na mesma tela", colors[app_]);
    form_ = panel(body_, 12, 68, 976, 542);
    lv_obj_add_flag(form_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(form_, LV_DIR_VER);
    for (std::size_t i = 0; i < preferences_.size(); ++i) {
        int x = 12 + (i % 2) * 484, y = 10 + (i / 2) * 104;
        label(form_, x, y, 460, fieldCaption(preferences_[i].first));
        lv_obj_t *o = nullptr;
        bool control = false;
        if (purpose_ == "settings" && i < 2) {
            o = slider(form_, x, y + 28, i == 0 ? 16 : 0, 255,
                       std::atoi(preferences_[i].second.c_str()), 460);
            control = true;
        } else if ((purpose_ == "settings" && i == 2) || (purpose_ == "pin" && i == 1)) {
            std::string options = "0\n15\n30\n60\n120\n300\n600\n1800\n3600";
            if (("\n" + options + "\n").find("\n" + preferences_[i].second + "\n") ==
                std::string::npos)
                options += "\n" + preferences_[i].second;
            o = dropdown(form_, x, y + 28, options, 0);
            for (unsigned n = 0; n < lv_dropdown_get_option_cnt(o); ++n) {
                lv_dropdown_set_selected(o, n);
                if (inputValue(o) == preferences_[i].second)
                    break;
            }
            control = true;
        } else if ((purpose_ == "settings" && i == 3) || (purpose_ == "world" && i % 2)) {
            std::string options;
            for (int h = -12; h <= 14; ++h)
                options += (h == -12 ? "" : "\n") + std::to_string(h);
            o = dropdown(form_, x, y + 28, options,
                         std::clamp(std::atoi(preferences_[i].second.c_str()) + 12, 0, 26));
            control = true;
        } else if (purpose_ == "settings" && i == 4) {
            o = dropdown(form_, x, y + 28, "ISO\nBR", preferences_[i].second == "BR" ? 1 : 0);
            control = true;
        } else if (purpose_ == "settings" && i == 6) {
            auto id = pda::value(service_.state().settings.fields, "theme", "sereno");
            o = dropdown(form_, x, y + 28, "Sereno\nAreia\nArdósia",
                         id == "areia"     ? 1
                         : id == "ardosia" ? 2
                                           : 0);
            control = true;
        } else {
            o = input(form_, x, y + 28, 460, 64,
                      purpose_ == "settings" && i == 5 ? fieldText("clock", preferences_[i].second)
                                                       : preferences_[i].second,
                      128, false,
                      (purpose_ == "pin" && i == 0) || (purpose_ == "network" && i == 1));
        }
        if (!control && purpose_ == "settings") {
            if (i == 5)
                mask(o, brazilian() ? InputMask::DateTimeBR : InputMask::DateTime,
                     brazilian() ? "DD/MM/AAAA HH:MM" : "AAAA-MM-DD HH:MM", 16);

        } else if (!control && purpose_ == "pin")
            mask(o, InputMask::Unsigned, "Somente dígitos", i == 0 ? 8 : 5);

        else if (purpose_ == "totals")
            mask(o, InputMask::Period, "AAAA-MM, AAAA ou vazio", 7);
        if (purpose_ == "settings" && i == 5)
            pickerButton(form_, x, y + 28, o, "clock");
        if (purpose_ == "world" && i % 2 == 0)
            caseInput(o, TextCase::Words);
        inputs_.push_back(o);
        inputLimits_.push_back(128);
        inputHeights_.push_back(lv_obj_check_type(o, &lv_slider_class) ? 22 : control ? 56 : 64);
    }
    editor_ = inputs_.front();
    if (purpose_ == "network")
        button(form_, 12, 250, 460, 56, "Salvar e conectar", Command::ConnectNetwork);
}
void Ui::savePreferences() {
    capture();
    auto s = service_.state().settings;
    int n;
    std::string error;
    if (purpose_ == "settings") {
        if (!integer(preferences_[0].second, 16, 255, n)) {
            message("Brilho: 16 a 255", true);
            return;
        }
        s.brightness = n;
        if (!integer(preferences_[1].second, 0, 255, n)) {
            message("Volume: 0 a 255", true);
            return;
        }
        s.volume = n;
        if (!integer(preferences_[2].second, 0, 3600, n)) {
            message("Tela: 0 a 3600 segundos", true);
            return;
        }
        s.screenTimeoutSeconds = n;
        if (!integer(preferences_[3].second, -12, 14, n)) {
            message("Fuso: -12 a 14 horas", true);
            return;
        }
        s.utcOffsetMinutes = n * 60;
        s.fields["dateFormat"] = preferences_[4].second;
        if (preferences_[6].second != "Sereno" && preferences_[6].second != "Areia" &&
            preferences_[6].second != "Ardósia") {
            message("Escolha um tema disponível", true);
            return;
        }
        s.fields["theme"] = preferences_[6].second == "Areia"     ? "areia"
                            : preferences_[6].second == "Ardósia" ? "ardosia"
                                                                  : "sereno";
        if (!preferences_[5].second.empty() && preferences_[5].second != initialClock_) {
            pda::CivilMinute t;
            if (!pda::parseCivilMinute(preferences_[5].second, t)) {
                message("Hora: AAAA-MM-DD HH:MM", true);
                return;
            }
            s.lastClockSetting = preferences_[5].second;
        }
    }
    if (purpose_ == "network") {
        if (preferences_[0].second.size() > 32 || preferences_[1].second.size() > 63) {
            message("SSID até 32; senha até 63 bytes", true);
            return;
        }
        s.fields["wifiSSID"] = preferences_[0].second;
        s.fields["wifiPass"] = preferences_[1].second;
    }
    if (purpose_ == "world")
        for (int i = 0; i < 3; ++i) {
            if (preferences_[i * 2].second.empty() || preferences_[i * 2].second.size() > 40 ||
                !integer(preferences_[i * 2 + 1].second, -12, 14, n)) {
                message("Cidade/fuso inválido", true);
                return;
            }
            s.fields["city" + std::to_string(i)] = preferences_[i * 2].second;
            s.fields["city" + std::to_string(i) + "Offset"] = std::to_string(n * 60);
        }
    if (purpose_ == "pin") {
        auto pin = preferences_[0].second;
        if (pin.size() < 4 || pin.size() > 8 ||
            pin.find_first_not_of("0123456789") != std::string::npos ||
            !integer(preferences_[1].second, 0, 3600, n)) {
            message("PIN: 4 a 8 dígitos; timeout: 0 a 3600", true);
            return;
        }
        s.fields["salt"] = platform_.randomSalt();
        s.fields["pin"] = platform_.pinHash(pin, s.fields["salt"]);
        s.fields["lockSeconds"] = std::to_string(n);
        if (s.fields["salt"].empty() || s.fields["pin"].empty()) {
            message("Falha ao proteger o PIN", true);
            return;
        }
    }
    if (purpose_ == "totals") {
        auto period = preferences_[0].second;
        pda::CivilMinute t;
        if (!period.empty() &&
            !((period.size() == 4 && pda::parseCivilMinute(period + "-01-01 00:00", t)) ||
              (period.size() == 7 && pda::parseCivilMinute(period + "-01 00:00", t)))) {
            message("Período: AAAA-MM ou AAAA", true);
            return;
        }
        result_ = "Totais: " + (period.empty() ? "todos os registros" : period);
        for (const auto &p : pda::expenseTotals(service_.state(), period))
            result_ += "\n" + p.first + " " + pda::formatMoney(p.second);
        mode_ = Mode::Normal;
        render();
        return;
    }
    const auto &current = service_.state().settings;
    if (purpose_ == "settings" && s.brightness == current.brightness &&
        s.volume == current.volume && s.screenTimeoutSeconds == current.screenTimeoutSeconds &&
        s.utcOffsetMinutes == current.utcOffsetMinutes &&
        s.lastClockSetting == current.lastClockSetting && s.fields == current.fields) {
        message("Preferências já estão salvas no microSD");
        return;
    }
    if (!service_.saveSettings(s)) {
        message(service_.error(), true);
        return;
    }
    platform_.applySettings(s);
    if (purpose_ == "settings" && !preferences_[5].second.empty() &&
        preferences_[5].second != initialClock_)
        if (!platform_.setClock(preferences_[5].second, s.utcOffsetMinutes, error)) {
            message(error, true);
            return;
        }
    preferences_.clear();
    mode_ = Mode::Normal;
    render();
    message("Preferências salvas no microSD");
}
void Ui::renderTools() {
    if (app_ == 7) {
        renderCalculator();
        return;
    }
    if (app_ == 8) {
        renderConverter();
        return;
    }
    if (app_ == 9) {
        renderClocks();
        return;
    }
    if (app_ == 12) {
        label(body_, 12, 12, 960, "Backup de registros e intercâmbio");
        button(body_, 12, 70, 280, 60, "Criar backup no microSD", Command::Backup);
        button(body_, 308, 70, 280, 60, "Restaurar backup.jsonl", Command::Restore);
        button(body_, 604, 70, 380, 60, "Restaurar recebido por USB", Command::Incoming);
        button(body_, 12, 154, 280, 60, usbEnabled_ ? "Desativar sessão USB" : "Ativar sessão USB",
               Command::Usb);
        button(body_, 12, 234, 220, 52, "Lixeira", Command::Trash);
        button(body_, 244, 234, 240, 52, "Desfazer exclusão", Command::TrashUndo);
        button(body_, 496, 234, 232, 52, "Importar / exportar", Command::Exchange);
        button(body_, 740, 234, 244, 52, "Backup por data", Command::DailyRestore);
        button(body_, 308, 154, 280, 60, "Mais ferramentas", Command::Extras);
        label(body_, 12, 312, 960,
              "A restauração exige confirmação e substitui a base inteira.\n\nArquivos no SD: "
              "/Tab5PDA/backup.jsonl e incoming.jsonl.\n\nO utilitário USB recebe um arquivo "
              "temporário; ele não substitui a agenda.\nBackups incluem preferências, PIN e "
              "credenciais Wi-Fi.\nÁudio, desenhos e rascunhos guardados são arquivos separados: "
              "copie toda a pasta.\nEsquema 2 / pda-backup-v1. Bases ampliadas e campos novos "
              "exigem firmware compatível.");
        return;
    }
    if (app_ == 14) {
        renderNetwork();
        return;
    }
    if (app_ == 15) {
        button(body_, 12, 8, 240, 52, "Definir PIN", Command::Pin);
        button(body_, 266, 8, 240, 52, "Bloquear agora", Command::Lock);
        button(body_, 520, 8, 240, 52, "Remover PIN", Command::RemovePin);
        label(body_, 12, 108, 960,
              (pda::value(service_.state().settings.fields, "pin").empty() ? "PIN desativado."
                                                                           : "PIN ativo.") +
                  std::string(
                      "\n\nPIN de 4 a 8 dígitos, bloqueio manual ou por inatividade.\n\nO PIN "
                      "restringe a interface e a exportação. Os arquivos do SD não são cifrados."));
        return;
    }
    if (app_ == 16) {
        button(body_, 12, 8, 280, 52, "Reabrir microSD", Command::Retry);
        button(body_, 308, 8, 280, 52, "Retomar rascunho", Command::Resume);
        auto *p = panel(body_, 12, 82, 976, 528);
        label(p, 14, 14, 946,
              platform_.diagnostics() + "\n\n" + service_.storageMessage() + "\nGeração: " +
                  std::to_string(service_.generation()) + "\nRegistros (inclui lixeira): " +
                  std::to_string(service_.state().records.size()) + "/" +
                  std::to_string(pda::kMaxRecords) + "\nMemória estimada: " +
                  std::to_string(pda::estimatedStateBytes(service_.state())) + "/" +
                  std::to_string(pda::kMaxResidentBytes) +
                  " bytes\nTexto agregado: até 1 MiB; notas: até 16 KiB");
        lv_obj_add_flag(p, LV_OBJ_FLAG_SCROLLABLE);
        return;
    }
}
void Ui::renderOverlay() {
    if (mode_ == Mode::Search) {
        label(body_, 12, 16, 960, "Pesquisar todos os campos (ignora caixa e acentos)");
        editor_ = input(body_, 12, 60, 976, 64, query_, 64);
        button(body_, 12, 146, 220, 52, "Buscar", Command::Search, 1);
        button(body_, 246, 146, 220, 52, "Cancelar", Command::Cancel);
        keyboard(editor_);
        return;
    }
    if (mode_ == Mode::Lock) {
        label(body_, 12, 28, 960, "Tab5PDA bloqueado", colors[15]);
        label(body_, 12, 88, 960, "Digite o PIN para continuar.");
        editor_ = input(body_, 12, 142, 620, 64, "", 8, false, true);
        lv_textarea_set_accepted_chars(editor_, "0123456789");
        button(body_, 652, 142, 320, 64, "Desbloquear", Command::Unlock);
        keyboard(editor_);
        return;
    }
    if (mode_ == Mode::Recovery) {
        std::string type = names[recordApp(draft_.record.kind)];
        label(body_, 12, 28, 960, draft_.active ? "Rascunho de " + type : "Nenhum rascunho salvo");
        label(body_, 12, 78, 960,
              dirty_ ? "Mantido na RAM; gravação pendente" : "Salvo no microSD");
        label(body_, 12, 122, 960,
              draft_.record.title.empty() ? "Sem título" : draft_.record.title);
        if (pendingCreate_) {
            button(body_, 12, 410, 470, 56, "Guardar rascunho e continuar", Command::ShelfContinue);
            label(body_, 12, 190, 960,
                  "Existe um único rascunho. Para " +
                      std::string(pendingExisting_ ? "editar em " : "criar em ") +
                      names[pendingApp_] +
                      ", descarte o rascunho anterior ou retome o tipo original.");
            button(body_, 12, 290, 300, 60, "Retomar " + type, Command::Resume, 1);
            button(body_, 326, 290, 350, 60,
                   std::string(pendingExisting_ ? "Descartar e editar " : "Descartar e criar ") +
                       names[pendingApp_],
                   Command::DiscardDraft, 1);
            button(body_, 690, 290, 280, 60, "Manter e voltar", Command::Cancel);
        } else {
            button(body_, 12, 210, 280, 60, "Retomar edição", Command::Resume, 1);
            button(body_, 308, 210, 280, 60, "Descartar rascunho", Command::DiscardDraft);
            button(body_, 604, 210, 280, 60, "Manter e voltar", Command::Cancel);
        }
        return;
    }
    if (mode_ == Mode::ConfirmDelete) {
        label(body_, 12, 28, 960, "Mover este registro para a lixeira?", 0xb91c1c);
        label(body_, 12, 90, 960,
              selectedRecord() ? selectedRecord()->title : "Registro indisponível");
        button(body_, 12, 174, 280, 60, "Confirmar exclusão", Command::ConfirmDelete, 0, 0xb91c1c);
        button(body_, 308, 174, 280, 60, "Cancelar", Command::Cancel);
        return;
    }
    if (mode_ == Mode::ConfirmRestore) {
        label(body_, 12, 28, 960, "Restaurar substitui toda a agenda atual.", 0xb91c1c);
        label(body_, 12, 96, 960,
              !restoreFile_.empty() ? "Arquivo: " + restoreFile_
              : incoming_           ? "Arquivo recebido: incoming.jsonl"
                                    : "Arquivo: backup.jsonl");
        button(body_, 12, 174, 280, 60, "Confirmar restauração", Command::ConfirmRestore);
        button(body_, 308, 174, 280, 60, "Cancelar", Command::Cancel);
        return;
    }
    if (mode_ == Mode::Alarm) {
        auto *r = pda::findRecord(service_.state(), alarm_.id);
        label(body_, 12, 28, 960, "Lembrete", 0xb45309);
        label(body_, 12, 96, 960,
              (r ? r->title : "Registro removido") + "\n\n" + alarm_.occurrence);
        button(body_, 12, 220, 280, 60, "Dispensar", Command::AlarmDismiss);
        button(body_, 308, 220, 280, 60,
               "Adiar " + pda::value(service_.state().settings.fields, "snooze", "5") + " minutos",
               Command::AlarmSnooze);
        button(body_, 604, 220, 280, 60, "Manter pendente", Command::AlarmHide);
        return;
    }
    if (mode_ == Mode::Occurrence) {
        label(body_, 12, 20, 960, "Editar ou cancelar apenas uma ocorrência da série");
        editor_ = input(body_, 12, 72, 976, 64, fieldText("when", calendarDay_), 10);
        mask(editor_, brazilian() ? InputMask::DateBR : InputMask::Date,
             brazilian() ? "DD/MM/AAAA" : "AAAA-MM-DD", 10);
        pickerButton(body_, 12, 72, editor_, "occurrence");
        button(body_, 12, 164, 280, 60, "Editar ocorrência", Command::Occurrence, 1);
        button(body_, 308, 164, 280, 60, "Cancelar ocorrência", Command::SkipOccurrence);
        button(body_, 604, 164, 280, 60, "Voltar", Command::Cancel);
        keyboard(editor_);
        return;
    }
}
void Ui::lock() {
    if (pda::value(service_.state().settings.fields, "pin").empty())
        return;
    if (mode_ == Mode::Edit) {
        capture();
        persistDraft();
    }
    if (mode_ == Mode::Productivity) {
        captureProductivity();
        saveProductDraft();
    }
    drawingResume_ = mode_ == Mode::Drawing;
    if (drawingResume_ && inkDirty_)
        handleExtras(Command::DrawSave, 0);
    if (mode_ == Mode::Extras && extrasPage_ == 8 && inputs_.size() == 3) {
        assistantURL_ = inputValue(inputs_[0]);
        assistantToken_ = inputValue(inputs_[1]);
        assistantPrompt_ = inputValue(inputs_[2]);
    }
    platform_.closeExternal();
    sharing_ = false;
    locked_ = true;
    usbEnabled_ = false;
    mode_ = Mode::Lock;
    render();
}
void Ui::dispatch(Command c, uint32_t a) {
    activity();
    if (locked_ && c != Command::Unlock && c != Command::KeyboardHide && c != Command::Accents &&
        c != Command::InsertAccent)
        return;
    if (c == Command::PickerOpen) {
        openPicker(a);
        return;
    }
    if (picker_) {
        if (c == Command::PickerPage) {
            pickerPage_ += a ? 1 : -1;
            drawPicker();
            return;
        }
        if (c == Command::PickerCancel) {
            closePicker();
            return;
        }
        if (c == Command::PickerOption && a < pickerOptions_.size()) {
            lv_textarea_set_text(pickerTarget_, pickerOptions_[a].first.c_str());
            closePicker();
            return;
        }
        if (c == Command::PickerApply || c == Command::PickerClear) {
            readPickerTime();
            std::string text;
            if (c == Command::PickerApply) {
                if (!pickerOptions_.empty())
                    return;
                text = pda::formatCivilMinute(pickerDate_);
                if (pickerDateOnly_)
                    text.resize(10);
                text = fieldText(pickerKey_ == "occurrence" ? "when" : pickerKey_, text);
            }
            lv_textarea_set_text(pickerTarget_, text.c_str());
            closePicker();
            return;
        }
        if (c == Command::PickerDay || c == Command::PickerAdjust) {
            readPickerTime();
            if (c == Command::PickerDay)
                pickerDate_.day = a;
            else {
                auto &d = pickerDate_;
                if (a < 2) {
                    int month = (d.year - 2000) * 12 + d.month - 1 + (a == 0 ? -1 : 1);
                    month = std::clamp(month, 0, 1199);
                    d.year = 2000 + month / 12;
                    d.month = 1 + month % 12;
                } else
                    d.year = std::clamp(d.year + (a == 2 ? -1 : 1), 2000, 2099);
                d.day = std::min(d.day, pda::daysInMonth(d.year, d.month));
            }
            drawPicker();
            return;
        }
        return;
    }
    if (handleToolCommands(c, a))
        return;
    if (handleProductivity(c, a))
        return;
    if (handleExtras(c, a))
        return;
    if (handlePriority(c, a))
        return;
    if (c == Command::KeyboardHide) {
        if (keyboard_)
            lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
        if (accentPalette_)
            lv_obj_add_flag(accentPalette_, LV_OBJ_FLAG_HIDDEN);
        fitForm(editor_, 542);
        return;
    }
    if (c == Command::Accents) {
        if (!editor_ || mode_ == Mode::Lock || platform_.physicalKeyboardConnected())
            return;
        keyboard(editor_);
        fitForm(editor_, 184);
        if (!accentPalette_) {
            accentPalette_ = panel(screen_, 294, 320, 972, 344, navy);
            for (int i = 0; i < 28; ++i)
                button(accentPalette_, 10 + (i % 7) * 137, 10 + (i / 7) * 64, 127, 54, accents[i],
                       Command::InsertAccent, i);
            button(accentPalette_, 10, 274, 952, 54, "Fechar paleta", Command::AccentClose);
        }
        lv_obj_clear_flag(accentPalette_, LV_OBJ_FLAG_HIDDEN);
        applyTheme(accentPalette_);
        lv_obj_move_foreground(accentPalette_);
        return;
    }
    if (c == Command::InsertAccent || c == Command::AccentClose) {
        if (c == Command::InsertAccent && editor_ && a < 28 &&
            !platform_.physicalKeyboardConnected()) {
            auto max = inputByteLimit();
            if (std::string(lv_textarea_get_text(editor_)).size() +
                    std::string(accents[a]).size() <=
                max)
                lv_textarea_add_text(editor_, accents[a]);
            else
                message("Limite do campo atingido", true);
        }
        if (accentPalette_)
            lv_obj_add_flag(accentPalette_, LV_OBJ_FLAG_HIDDEN);
        if (editor_)
            keyboard(editor_);
        return;
    }
    if (c == Command::App) {
        if (mode_ == Mode::Alarm) {
            muted_.insert(alarm_.key);
            mode_ = beforeAlarm_;
        }
        if (mode_ == Mode::Edit) {
            capture();
            persistDraft();
        }
        pendingCreate_ = false;
        listPage_ = 0;
        app_ = std::min<uint32_t>(a, 16);
        mode_ = Mode::Normal;
        selected_ = 0;
        query_.clear();
        dailyFilter_.clear();
        result_.clear();
        usbEnabled_ = false;
        if (app_ == 10) {
            auto now = platform_.localMinute();
            calendarDay_ = now.empty() ? "2026-01-01" : now.substr(0, 10);
        }
        render();
        return;
    }
    if (c == Command::Cancel) {
        if (mode_ == Mode::Preferences && app_ == 13 && purpose_ == "settings")
            app_ = 0;
        newWhen_.clear();
        subtaskParent_.clear();
        moveRecord_ = 0;
        pendingCreate_ = false;
        if (mode_ == Mode::Edit) {
            capture();
            persistDraft();
        }
        mode_ = Mode::Normal;
        render();
        return;
    }
    if (c == Command::Select) {
        selected_ = a;
        render();
        return;
    }
    if (c == Command::New) {
        if (app_ == 11) {
            message("Escolha a aplicação para criar um registro", true);
            return;
        }
        if (app_ == 10) {
            app_ = 2;
            dailyFilter_ = calendarDay_;
        }
        startEdit();
        return;
    }
    if (c == Command::Edit) {
        auto *r = selectedRecord();
        if (r)
            startEdit(r);
        return;
    }
    if (c == Command::Save) {
        if (mode_ == Mode::Edit)
            saveEditor();
        return;
    }
    if (c == Command::Delete) {
        if (selectedRecord()) {
            mode_ = Mode::ConfirmDelete;
            render();
        }
        return;
    }
    if (c == Command::ConfirmDelete) {
        auto id = selected_;
        if (service_.trashRecord(id)) {
            lastTrashed_ = id;
            if (draft_.record.id == id ||
                pda::value(draft_.record.fields, "origin") == std::to_string(id)) {
                draft_ = pda::Draft{};
                dirty_ = false;
            }
            if (pda::value(draft_.record.fields, "contact") == std::to_string(id))
                draft_.record.fields.erase("contact");
            mode_ = Mode::Normal;
            render();
            message("Movido à lixeira; use Backup / Desfazer exclusão");
        } else
            message(service_.error(), true);
        return;
    }
    if (c == Command::Toggle) {
        if (service_.toggleTask(selected_, platform_.localMinute())) {
            render();
            message("Tarefa salva");
        } else
            message(service_.error(), true);
        return;
    }
    if (c == Command::Search) {
        if (a) {
            capture();
            mode_ = Mode::Normal;
            selected_ = 0;
            render();
        } else {
            mode_ = Mode::Search;
            render();
        }
        return;
    }
    if (c == Command::Month) {
        pda::CivilMinute t;
        pda::parseCivilMinute(calendarDay_ + " 00:00", t);
        int m = (t.year - 2000) * 12 + t.month - 1 + (a ? 1 : -1);
        if (m >= 0 && m < 1200) {
            t.year = 2000 + m / 12;
            t.month = m % 12 + 1;
            t.day = std::min(t.day, pda::daysInMonth(t.year, t.month));
            calendarDay_ = pda::formatCivilMinute(t).substr(0, 10);
        }
        render();
        return;
    }
    if (c == Command::Day) {
        pda::CivilMinute t;
        pda::parseCivilMinute(calendarDay_ + " 00:00", t);
        if (a >= 1 && a <= uint32_t(pda::daysInMonth(t.year, t.month)))
            t.day = a;
        calendarDay_ = pda::formatCivilMinute(t).substr(0, 10);
        render();
        return;
    }
    if (c == Command::Calendar) {
        if (a) {
            app_ = 2;
            dailyFilter_ = calendarDay_;
            selected_ = 0;
        } else {
            app_ = 10;
            auto now = platform_.localMinute();
            calendarDay_ = now.empty() ? "2026-01-01" : now.substr(0, 10);
        }
        render();
        return;
    }
    if (c == Command::Occurrence || c == Command::SkipOccurrence) {
        auto *r = selectedRecord();
        if (!r || r->kind != pda::RecordKind::Appointment ||
            pda::value(r->fields, "repeat", "none") == "none") {
            message("Selecione uma série recorrente", true);
            return;
        }
        if (!a && c == Command::Occurrence) {
            calendarDay_ = r->when.substr(0, 10);
            mode_ = Mode::Occurrence;
            render();
            return;
        }
        std::string day = inputValue(editor_), at;
        if (!pda::occurrenceOn(*r, day, at)) {
            message("Não há ocorrência nessa data", true);
            return;
        }
        if (c == Command::SkipOccurrence) {
            if (service_.skipOccurrence(r->id, day)) {
                mode_ = Mode::Normal;
                render();
                message("Ocorrência cancelada");
            } else
                message(service_.error(), true);
            return;
        }
        if (draft_.active || service_.state().draft.active) {
            mode_ = Mode::Recovery;
            if (!draft_.active)
                draft_ = service_.state().draft;
            render();
            return;
        }
        draft_ = {};
        draft_.active = true;
        draft_.record = *r;
        draft_.record.id = 0;
        draft_.record.when = at;
        draft_.record.fields["repeat"] = "none";
        draft_.record.fields.erase("repeatEnd");
        draft_.record.fields.erase("exceptions");
        draft_.record.fields["origin"] = std::to_string(r->id);
        draft_.record.fields["occurrence"] = day;
        if (!pda::value(r->fields, "until").empty()) {
            pda::CivilMinute x, y, z;
            pda::parseCivilMinute(r->when, x);
            pda::parseCivilMinute(r->fields.at("until"), y);
            pda::parseCivilMinute(at, z);
            draft_.record.fields["until"] = pda::formatCivilMinute(pda::unixToCivil(
                pda::civilToUnix(z, 0) + pda::civilToUnix(y, 0) - pda::civilToUnix(x, 0), 0));
        }
        field_ = 0;
        dirty_ = true;
        lastEdit_ = platform_.nowMs();
        mode_ = Mode::Edit;
        render();
        return;
    }
    if (c == Command::Totals) {
        openPreferences("totals");
        return;
    }
    if (c == Command::SettingsField) {
        openPreferences(a == 1 ? "network" : a == 2 ? "world" : "settings");
        return;
    }
    if (c == Command::SettingsSave) {
        if (mode_ == Mode::Preferences)
            savePreferences();
        return;
    }
    if (c == Command::ScanNetworks) {
        if (platform_.networkBusy()) {
            message("Aguarde a operação Wi-Fi terminar", true);
            return;
        }
        platform_.scanNetworks();
        render();
        return;
    }
    if (c == Command::SelectNetwork) {
        if (a >= networks_.size())
            return;
        auto network = networks_[a];
        openPreferences("network");
        if (preferences_[0].second != network.ssid || !network.secured)
            preferences_[1].second.clear();
        preferences_[0].second = network.ssid;
        render();
        return;
    }
    if (c == Command::ConnectNetwork) {
        if (platform_.networkBusy()) {
            message("Aguarde a operação Wi-Fi terminar", true);
            return;
        }
        if (mode_ == Mode::Preferences && purpose_ == "network") {
            savePreferences();
            if (mode_ != Mode::Normal)
                return;
        }
        platform_.connectNetwork();
        render();
        return;
    }
    if (c == Command::DisconnectNetwork) {
        platform_.disconnectNetwork();
        render();
        return;
    }
    if (c == Command::Sync) {
        platform_.syncTime();
        render();
        return;
    }
    if (c == Command::Backup) {
        bool ok = service_.backup();
        message(ok ? "Backup verificado no microSD" : service_.error(), !ok);
        return;
    }
    if (c == Command::Restore || c == Command::Incoming) {
        restoreFile_.clear();
        incoming_ = c == Command::Incoming;
        mode_ = Mode::ConfirmRestore;
        usbEnabled_ = false;
        render();
        return;
    }
    if (c == Command::ConfirmRestore) {
        if (dirty_) {
            message("Retome ou descarte o rascunho na RAM", true);
            return;
        }
        bool restored = false;
        if (!restoreFile_.empty()) {
            std::string data, error;
            pda::State candidate;
            if (service_.readFile(restoreFile_, data) && pda::importJsonl(data, candidate, error))
                restored = service_.restore(candidate);
            else if (!error.empty()) {
                message(error, true);
                return;
            }
        } else
            restored = service_.restoreBackup(incoming_);
        if (restored) {
            draft_ = {};
            platform_.applySettings(service_.state().settings);
            mode_ = Mode::Normal;
            render();
            message("Base restaurada");
            if (!pda::value(service_.state().settings.fields, "pin").empty())
                lock();
        } else
            message(service_.error(), true);
        return;
    }
    if (c == Command::Usb) {
        if (!usbEnabled_ && platform_.networkBusy()) {
            message("Aguarde a sincronização Wi-Fi terminar antes de ativar USB", true);
            return;
        }
        usbEnabled_ = !usbEnabled_;
        render();
        return;
    }
    if (c == Command::Resume) {
        newWhen_.clear();
        subtaskParent_.clear();
        if (!a) {
            pendingCreate_ = false;
            if (!draft_.active)
                draft_ = service_.state().draft;
            mode_ = Mode::Recovery;
            render();
            return;
        }
        if (!draft_.active) {
            message("Nenhum rascunho salvo", true);
            return;
        }
        auto k = draft_.record.kind;
        app_ = recordApp(k);
        pendingCreate_ = false;
        field_ = draft_.field;
        dailyFilter_.clear();
        mode_ = Mode::Edit;
        render();
        return;
    }
    if (c == Command::DiscardDraft) {
        if (service_.clearDraft()) {
            draft_ = {};
            dirty_ = false;
            mode_ = Mode::Normal;
            if (a == 1 && pendingCreate_) {
                app_ = pendingApp_;
                bool existing = pendingExisting_;
                pda::Record record = pendingRecord_;
                pendingCreate_ = false;
                startEdit(existing ? &record : nullptr);
            } else {
                pendingCreate_ = false;
                render();
                message("Rascunho descartado");
            }
        } else
            message(service_.error(), true);
        return;
    }
    if (c == Command::Retry) {
        bool ok =
            platform_.remount() && (draft_.active ? service_.reloadForEdit() : service_.load());
        platform_.applySettings(service_.state().settings);
        render();
        message(ok ? service_.storageMessage() : service_.error(), !ok);
        if (ok && !pda::value(service_.state().settings.fields, "pin").empty())
            lock();
        return;
    }
    if (c == Command::Pin) {
        openPreferences("pin");
        return;
    }
    if (c == Command::Lock) {
        lock();
        return;
    }
    if (c == Command::RemovePin) {
        auto s = service_.state().settings;
        s.fields.erase("pin");
        s.fields.erase("salt");
        if (service_.saveSettings(s)) {
            render();
            message("PIN removido");
        } else
            message(service_.error(), true);
        return;
    }
    if (c == Command::Unlock) {
        auto now = platform_.nowMs();
        if (pinRetry_ && int32_t(now - pinRetry_) < 0) {
            message("Aguarde antes de tentar novamente", true);
            return;
        }
        auto hash = platform_.pinHash(lv_textarea_get_text(editor_),
                                      pda::value(service_.state().settings.fields, "salt"));
        if (!hash.empty() && hash == pda::value(service_.state().settings.fields, "pin")) {
            locked_ = false;
            pinRetry_ = 0;
            muted_.clear();
            notifiedLocked_.clear();
            mode_ = drawingResume_ ? Mode::Drawing : Mode::Normal;
            drawingResume_ = false;
            render();
        } else {
            pinRetry_ = now + 3000;
            lv_textarea_set_text(editor_, "");
            message("PIN incorreto", true);
        }
        return;
    }
    if (c == Command::AlarmDismiss || c == Command::AlarmSnooze || c == Command::AlarmHide) {
        if (c == Command::AlarmHide) {
            muted_.insert(alarm_.key);
            mode_ = beforeAlarm_;
            render();
            return;
        }
        std::string until = "dismissed";
        if (c == Command::AlarmSnooze) {
            pda::CivilMinute t;
            if (!pda::parseCivilMinute(platform_.localMinute(), t)) {
                message("Ajuste a hora primeiro", true);
                return;
            }
            until = pda::formatCivilMinute(pda::unixToCivil(
                pda::civilToUnix(t, service_.state().settings.utcOffsetMinutes) +
                    std::stoul(pda::value(service_.state().settings.fields, "snooze", "5")) * 60,
                0));
        }
        if (!service_.acknowledgeAlarm(alarm_.key, until)) {
            message(service_.error(), true);
            return;
        }
        mode_ = beforeAlarm_;
        render();
        message(c == Command::AlarmDismiss
                    ? "Lembrete dispensado"
                    : "Adiado por " + pda::value(service_.state().settings.fields, "snooze", "5") +
                          " minutos");
        return;
    }
    if (c == Command::AlarmShow) {
        muted_.clear();
        lastAlarm_ = 0;
        return;
    }
}
void Ui::tick() {
    while (!commands_.empty()) {
        auto action = commands_.front();
        commands_.pop_front();
        dispatch(action.first, action.second);
    }
    tickExtras();
    bool connected = platform_.physicalKeyboardConnected();
    if (connected != lastKeyboardConnected_) {
        lastKeyboardConnected_ = connected;
        if (connected)
            dispatch(Command::KeyboardHide);
        else if (!picker_ && editor_ && lv_obj_check_type(editor_, &lv_textarea_class) &&
                 lv_obj_has_state(editor_, LV_STATE_FOCUSED))
            keyboard(editor_);
    }
    auto now = platform_.nowMs();
    if (!locked_ && !pda::value(service_.state().settings.fields, "pin").empty()) {
        int seconds =
            std::atoi(pda::value(service_.state().settings.fields, "lockSeconds", "120").c_str());
        if (seconds && now - lastActivity_ >= uint32_t(seconds) * 1000)
            lock();
    }
    if (mode_ == Mode::Edit && dirty_ && now - lastEdit_ >= 1500 && now - lastDraft_ >= 1500 &&
        service_.writable())
        persistDraft();
    if (now - lastAlarm_ >= 1000) {
        lastAlarm_ = now;
        auto alarms = pda::dueAlarms(service_.state(), platform_.localMinute());
        for (const auto &a : alarms) {
            if (locked_) {
                if (!notifiedLocked_.count(a.key)) {
                    notifiedLocked_.insert(a.key);
                    platform_.notify();
                    message("Lembrete pendente: desbloqueie");
                }
                continue;
            }
            if (mode_ == Mode::Alarm || muted_.count(a.key))
                continue;
            capture();
            if (mode_ == Mode::Edit)
                persistDraft();
            if (mode_ == Mode::Drawing && inkDirty_)
                handleExtras(Command::DrawSave, 0);
            if (mode_ == Mode::Productivity) {
                captureProductivity();
                saveProductDraft();
            }
            beforeAlarm_ = mode_;
            alarm_ = a;
            mode_ = Mode::Alarm;
            usbEnabled_ = false;
            platform_.notify();
            render();
            break;
        }
    }
    const auto day = platform_.localMinute().substr(0, 10);
    if (!locked_ && mode_ == Mode::Normal && !day.empty() && backupDay_ != day &&
        service_.writable() && (backupAt_ == 0 || now - backupAt_ >= 300000)) {
        backupAt_ = now ? now : 1;
        if (service_.dailyBackup(day)) {
            backupDay_ = day;
            backupAt_ = 0;
        } else
            message("Backup diário: " + service_.error(), true);
    }
    if (now - lastClock_ >= 500) {
        lastClock_ = now;
        auto time = platform_.localMinute();
        lv_label_set_text(clock_,
                          (time.empty() ? "Hora não ajustada"
                                        : pda::displayMinute(time, service_.state().settings))
                              .c_str());
        if (mode_ == Mode::Normal && (((app_ == 0 || app_ == 9) && time != lastMinute_) ||
                                      (app_ == 14 && platform_.networkStatus() != lastNetwork_)))
            render();
    }
}
} // namespace tab5_pda
