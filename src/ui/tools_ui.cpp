#include "tab_ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace tab5_pda {
namespace {
std::string number(double value, bool money = false) {
    char text[64];
    std::snprintf(text, sizeof(text), money ? "%.2f" : "%.12g", value);
    return text;
}
bool parseNumber(std::string text, double &value) {
    std::replace(text.begin(), text.end(), ',', '.');
    if (text.empty() || text.size() > 64)
        return false;
    char *end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end != text.c_str() && !*end && std::isfinite(value);
}
std::string options(const std::vector<std::string> &values) {
    std::string text;
    for (const auto &value : values)
        text += (text.empty() ? "" : "\n") + value;
    return text;
}
} // namespace
void Ui::caseInput(lv_obj_t *object, TextCase mode) {
    if (mode == TextCase::None)
        return;
    casedInputs_.push_back({object, mode});
    auto text = std::string(lv_textarea_get_text(object));
    auto formatted = capitalize(text, mode);
    if (text != formatted) {
        auto limit = lv_textarea_get_max_length(object);
        casing_ = true;
        lv_textarea_set_max_length(object, 0);
        lv_textarea_set_text(object, formatted.c_str());
        lv_textarea_set_max_length(object, limit);
        casing_ = false;
    }
}
void Ui::applyTheme(lv_obj_t *object) {
    auto mapped = [&](lv_color_t color, bool text) {
        auto same = [&](uint32_t rgb) { return color.full == lv_color_hex(rgb).full; };
        if (same(0x172b4d))
            return lv_color_hex(theme_->ink);
        if (same(0x16324f))
            return lv_color_hex(text ? theme_->accentText : theme_->header);
        if (same(0xedf2f7))
            return lv_color_hex(theme_->background);
        if (same(0xdbe5ef))
            return lv_color_hex(theme_->nav);
        if (!text && same(0xffffff))
            return lv_color_hex(theme_->surface);
        if (same(0x64748b))
            return lv_color_hex(text ? theme_->muted : theme_->header);
        if (text && std::string(theme_->id) == "ardosia" && (same(0xb91c1c) || same(0xa52834)))
            return lv_color_hex(0xffb5b5);
        if (same(0xcbd5e1) || same(0xc6d2dc))
            return lv_color_hex(theme_->border);
        static const uint32_t accents[] = {
            0x285e61, 0x2563eb, 0x7c3aed, 0x16834a, 0xb45309, 0xc02678, 0x9c4221, 0xbe3047,
            0x047857, 0x334e68, 0x6b46c1, 0x475569, 0x0e7490, 0x0369a1, 0x334155, 0x164e63};
        for (auto accent : accents)
            if (same(accent))
                return lv_color_hex(text ? theme_->accentText : theme_->accent);
        return color;
    };
    auto part = [&](lv_style_selector_t selector) {
        auto bg = lv_obj_get_style_bg_color(object, selector & LV_PART_ANY);
        auto fg = lv_obj_get_style_text_color(object, selector & LV_PART_ANY);
        auto border = lv_obj_get_style_border_color(object, selector & LV_PART_ANY);
        auto newBg = mapped(bg, false), newFg = mapped(fg, true), newBorder = mapped(border, false);
        if (newBg.full != bg.full)
            lv_obj_set_style_bg_color(object, newBg, selector);
        if (newFg.full != fg.full)
            lv_obj_set_style_text_color(object, newFg, selector);
        if (newBorder.full != border.full)
            lv_obj_set_style_border_color(object, newBorder, selector);
    };
    part(LV_PART_MAIN);
    if (lv_obj_check_type(object, &lv_table_class) || lv_obj_check_type(object, &lv_keyboard_class))
        part(LV_PART_ITEMS);
    if (lv_obj_check_type(object, &lv_btn_class))
        lv_obj_set_style_bg_color(object, lv_color_hex(theme_->header), LV_STATE_PRESSED);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(object); ++i)
        applyTheme(lv_obj_get_child(object, i));
}
std::string Ui::justified(const std::string &text, int width) {
    std::istringstream paragraphs(text);
    std::string paragraph, output;
    const auto *font = &pda_font_16;
    auto measure = [&](const std::string &s) {
        return lv_txt_get_width(s.c_str(), s.size(), font, 0, LV_TEXT_FLAG_NONE);
    };
    int space = std::max<int>(1, measure(" "));
    while (std::getline(paragraphs, paragraph)) {
        std::istringstream words(paragraph);
        std::vector<std::string> line;
        int used = 0;
        auto flush = [&](bool spread) {
            int extra = spread && line.size() > 1 ? std::max(0, width - used) / space : 0;
            for (std::size_t i = 0; i < line.size(); ++i) {
                output += line[i];
                if (i + 1 < line.size()) {
                    auto left = line.size() - i - 1;
                    int add = extra / left;
                    output += std::string(1 + add, ' ');
                    extra -= add;
                }
            }
            output += '\n';
            line.clear();
            used = 0;
        };
        std::string word;
        while (words >> word) {
            int size = measure(word);
            if (!line.empty() && used + space + size > width)
                flush(true);
            used += (line.empty() ? 0 : space) + size;
            line.push_back(word);
        }
        flush(false);
    }
    if (!output.empty())
        output.pop_back();
    return output;
}
void Ui::captureTools() {
    if (mode_ != Mode::Normal)
        return;
    if (app_ == 7 && calculatorMode_ != 2 && editor_)
        expression_ = lv_textarea_get_text(editor_);
    else if (app_ == 7 && calculatorMode_ == 2 && inputs_.size() == 6) {
        for (unsigned i = 0; i < 5; ++i)
            financeCalc_[i] = inputValue(inputs_[i]);
        financeBeginning_ = lv_dropdown_get_selected(inputs_[5]) == 1;
    } else if (app_ == 8 && inputs_.size() >= 3) {
        converterValue_ = inputValue(inputs_[0]);
        converterFrom_ = lv_dropdown_get_selected(inputs_[1]);
        converterTo_ = lv_dropdown_get_selected(inputs_[2]);
        if (inputs_.size() == 4)
            converterRate_ = inputValue(inputs_[3]);
    }
}
void Ui::renderCalculator() {
    const char *modes[] = {"Básica", "Científica", "Financeira"};
    for (int i = 0; i < 3; ++i) {
        auto *tab = button(body_, 12 + i * 328, 8, 312, 46, modes[i], Command::CalculatorMode, i);
        if (i == calculatorMode_) {
            lv_obj_set_style_border_width(tab, 2, 0);
            lv_obj_set_style_border_color(tab, lv_color_hex(theme_->border), 0);
        }
    }
    if (calculatorMode_ == 2) {
        auto *screen = panel(body_, 12, 70, 976, 92);
        auto *value = label(screen, 18, 22, 936, calculatorResult_);
        lv_obj_set_style_text_font(value, &pda_font_28, 0);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        const char *captions[] = {"Períodos (n)", "Taxa por período (%)", "Valor presente (VP)",
                                  "Parcela (PMT)", "Valor futuro (VF)"};
        for (int i = 0; i < 5; ++i) {
            int x = 12 + i % 2 * 490, y = 176 + i / 2 * 84;
            label(body_, x, y, 460, captions[i]);
            inputs_.push_back(input(body_, x, y + 26, 460, 48, financeCalc_[i], 32));
            inputLimits_.push_back(32);
            inputHeights_.push_back(48);
        }
        label(body_, 502, 344, 460, "Pagamento da parcela");
        inputs_.push_back(dropdown(body_, 502, 370, "Fim do período\nInício do período",
                                   financeBeginning_ ? 1 : 0));
        inputLimits_.push_back(0);
        inputHeights_.push_back(48);
        lv_obj_set_width(inputs_.back(), 460);
        lv_obj_set_height(inputs_.back(), 48);
        const char *solve[] = {"Calcular n", "Calcular taxa", "Calcular VP", "Calcular parcela",
                               "Calcular VF"};
        for (unsigned i = 0; i < 5; ++i)
            button(body_, 12 + i * 196, 450, 184, 60, solve[i], Command::FinancialSolve, i);
        label(body_, 12, 536, 960,
              "Use sinais opostos para entradas e saídas. Taxa e períodos devem ter a mesma "
              "frequência.\nDigite os valores conhecidos; o botão calcula o campo escolhido.");
        editor_ = inputs_[0];
        field_ = 0;
        return;
    }
    label(body_, 12, 74, 720, "Expressão — Enter para calcular");
    editor_ = input(body_, 12, 110, 976, 76, expression_, 256);
    lv_obj_set_style_text_font(editor_, &pda_font_28, 0);
    lv_obj_set_style_text_align(editor_, LV_TEXT_ALIGN_RIGHT, 0);
    inputs_.push_back(editor_);
    inputLimits_.push_back(256);
    inputHeights_.push_back(76);
    field_ = 0;
    auto *display = panel(body_, 12, 204, 976, 102);
    auto *value = label(display, 16, 22, 944, calculatorResult_);
    lv_obj_set_style_text_font(value, &pda_font_48, 0);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    if (calculatorMode_ == 0) {
        label(body_, 12, 350, 960,
              "Digite a conta no teclado. Enter mostra o resultado.\n\nAceita parênteses e "
              "percentual. Os modos acima oferecem funções adicionais.");
        return;
    }
    button(body_, 12, 326, 184, 46, degrees_ ? "Graus (DEG)" : "Radianos (RAD)",
           Command::AngleMode);
    const char *functions[] = {"sen", "cos", "tan", "Raiz", "x²", "1/x",
                               "ln",  "log", "exp", "x!",   "π",  "e"};
    for (unsigned i = 0; i < 12; ++i)
        button(body_, 12 + i % 6 * 164, 390 + i / 6 * 70, 152, 58, functions[i],
               Command::ScientificFunction, i);
    button(body_, 12, 540, 180, 50, "M+", Command::MemoryAdd);
    button(body_, 208, 540, 180, 50, "M-", Command::MemorySubtract);
    button(body_, 404, 540, 180, 50, "MR", Command::MemoryRecall);
    button(body_, 600, 540, 180, 50, "MC", Command::MemoryClear);
    label(body_, 800, 554, 180, "M: " + number(memory_));
}
void Ui::renderConverter() {
    auto *heading = label(body_, 12, 8, 720, "Conversores");
    lv_obj_set_style_text_font(heading, &pda_font_28, 0);
    std::vector<std::string> groups;
    for (const auto &group : unitGroups())
        groups.push_back(group.name);
    label(body_, 12, 76, 460, "Grandeza");
    auto *group = dropdown(body_, 12, 108, options(groups), converterGroup_);
    lv_obj_set_width(group, 460);
    lv_obj_set_height(group, 54);
    // This selector changes the units, so handle its event after LVGL dispatch.
    lv_obj_add_event_cb(
        group,
        [](lv_event_t *event) {
            auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
            ui->commands_.push_back(
                {Command::ConverterGroup, lv_dropdown_get_selected(lv_event_get_target(event))});
        },
        LV_EVENT_VALUE_CHANGED, this);
    label(body_, 502, 76, 460, "Valor");
    inputs_.push_back(input(body_, 502, 108, 460, 54, converterValue_, 32));
    inputLimits_.push_back(32);
    inputHeights_.push_back(54);
    auto units = options(unitGroups()[converterGroup_].units);
    label(body_, 12, 200, 460, "Unidade de origem");
    inputs_.push_back(dropdown(body_, 12, 232, units, converterFrom_));
    label(body_, 502, 200, 460, "Unidade de destino");
    inputs_.push_back(dropdown(body_, 502, 232, units, converterTo_));
    for (unsigned i = 1; i < 3; ++i) {
        lv_obj_set_width(inputs_[i], 460);
        lv_obj_set_height(inputs_[i], 54);
        inputLimits_.push_back(0);
        inputHeights_.push_back(54);
    }
    if (converterGroup_ == 4) {
        label(body_, 12, 316, 940, "Taxa manual — unidades de destino por unidade de origem");
        inputs_.push_back(input(body_, 12, 348, 460, 54, converterRate_, 32));
        inputLimits_.push_back(32);
        inputHeights_.push_back(54);
    }
    button(body_, 12, 426, 460, 56, "Converter", Command::Convert);
    button(body_, 502, 426, 460, 56, "Inverter unidades", Command::ConverterSwap);
    auto *answer = panel(body_, 12, 508, 976, 94);
    auto *value = label(answer, 18, 22, 936,
                        converterResult_.empty() ? "Escolha as unidades e informe o valor"
                                                 : converterResult_);
    lv_obj_set_style_text_font(value, &pda_font_28, 0);
    editor_ = inputs_[0];
    field_ = 0;
}
void Ui::renderClocks() {
    auto *title = label(body_, 12, 8, 650, "Relógios mundiais");
    lv_obj_set_style_text_font(title, &pda_font_28, 0);
    button(body_, 680, 8, 304, 52, "Editar cidades e fusos", Command::SettingsField, 2);
    pda::CivilMinute local;
    auto now = platform_.localMinute();
    bool valid = pda::parseCivilMinute(now, local);
    auto utc = valid ? pda::civilToUnix(local, service_.state().settings.utcOffsetMinutes) : 0;
    for (int i = 0; i < 3; ++i) {
        auto key = "city" + std::to_string(i);
        auto city = pda::value(service_.state().settings.fields, key,
                               i == 0   ? "Cuiabá"
                               : i == 1 ? "Londres"
                                        : "Tóquio");
        auto offset = std::atoi(pda::value(service_.state().settings.fields, key + "Offset",
                                           i == 0   ? "-240"
                                           : i == 1 ? "0"
                                                    : "540")
                                    .c_str());
        auto *card = panel(body_, 12 + i * 328, 112, 312, 354);
        lv_obj_set_style_radius(card, 14, 0);
        auto *name = label(card, 16, 34, 280, city);
        lv_obj_set_style_text_font(name, &pda_font_28, 0);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_height(name, 70);
        lv_obj_update_layout(name);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        auto at = valid ? pda::formatCivilMinute(pda::unixToCivil(utc, offset)) : std::string{};
        auto *digital = label(card, 12, 132, 288, valid ? at.substr(11, 5) : "--:--");
        lv_obj_set_style_text_font(digital, &pda_font_48, 0);
        lv_obj_set_style_text_align(digital, LV_TEXT_ALIGN_CENTER, 0);
        auto *date = label(card, 16, 228, 280,
                           valid ? pda::displayMinute(at, service_.state().settings).substr(0, 10)
                                 : "Ajuste a hora");
        lv_obj_set_style_text_align(date, LV_TEXT_ALIGN_CENTER, 0);
        auto *zone = label(card, 16, 282, 280,
                           "UTC " + std::string(offset >= 0 ? "+" : "") + number(offset / 60.0));
        lv_obj_set_style_text_align(zone, LV_TEXT_ALIGN_CENTER, 0);
    }
    label(body_, 12, 526, 960,
          "Fusos definidos por cidade. Ajuste os offsets quando houver horário de verão.");
    lastMinute_ = now;
}
void Ui::renderNetwork() {
    auto *title = label(body_, 12, 8, 640, "Wi-Fi e sincronização");
    lv_obj_set_style_text_font(title, &pda_font_28, 0);
    button(body_, 710, 8, 274, 52, "Configurar Wi-Fi", Command::SettingsField, 1);
    auto *status = panel(body_, 12, 82, 976, 98);
    auto *saved =
        label(status, 16, 12, 944,
              "Rede salva: " + pda::value(service_.state().settings.fields, "wifiSSID", "nenhuma"));
    lv_obj_set_style_text_font(saved, &pda_font_16_bold, 0);
    auto *state = label(status, 16, 46, 944, platform_.networkStatus());
    lv_obj_set_height(state, 38);
    lv_label_set_long_mode(state, LV_LABEL_LONG_DOT);
    const char *captions[] = {"Buscar redes", "Conectar", "Desconectar", "Sincronizar hora NTP"};
    const Command commands[] = {Command::ScanNetworks, Command::ConnectNetwork,
                                Command::DisconnectNetwork, Command::Sync};
    for (int i = 0; i < 4; ++i)
        button(body_, 12 + i * 246, 200, 234, 54, captions[i], commands[i]);
    label(body_, 12, 278, 960, "Redes disponíveis — selecione para informar a senha");
    auto *list = panel(body_, 12, 318, 976, 286);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    networks_ = platform_.networks();
    for (unsigned i = 0; i < networks_.size(); ++i) {
        const auto &network = networks_[i];
        button(list, 12, 10 + i * 76, 952, 66,
               network.ssid + "\n" + std::to_string(network.rssi) + " dBm  /  " +
                   (network.secured ? "Protegida" : "Aberta"),
               Command::SelectNetwork, i);
    }
    if (networks_.empty())
        label(list, 18, 22, 932, "Busque redes próximas ou configure uma rede oculta pelo nome.");
    lastNetwork_ = platform_.networkStatus();
}
bool Ui::handleToolCommands(Command command, uint32_t argument) {
    if (mode_ == Mode::Normal && (app_ == 7 || app_ == 8) &&
        (command == Command::App || command == Command::Cancel ||
         command == Command::SettingsField))
        captureTools();
    if (command == Command::CalculatorMode) {
        if (mode_ != Mode::Normal || app_ != 7 || argument > 2)
            return true;
        captureTools();
        calculatorMode_ = argument;
        render();
        return true;
    }
    if (command == Command::ConverterGroup) {
        if (mode_ != Mode::Normal || app_ != 8 || argument >= unitGroups().size())
            return true;
        captureTools();
        converterGroup_ = argument;
        converterFrom_ = 0;
        converterTo_ = 1;
        converterResult_.clear();
        render();
        return true;
    }
    if (command == Command::AngleMode) {
        if (mode_ == Mode::Normal && app_ == 7) {
            captureTools();
            degrees_ = !degrees_;
            render();
        }
        return true;
    }
    if (command == Command::Convert || command == Command::ConverterSwap) {
        if (mode_ != Mode::Normal || app_ != 8 || inputs_.size() < 3)
            return true;
        captureTools();
        if (command == Command::ConverterSwap) {
            if (converterGroup_ == 4) {
                double rate;
                if (!parseNumber(converterRate_, rate) || rate <= 0 || !std::isfinite(1 / rate)) {
                    message("Informe uma taxa positiva para inverter", true);
                    return true;
                }
                converterRate_ = number(1 / rate);
            }
            std::swap(converterFrom_, converterTo_);
            converterResult_.clear();
            render();
            return true;
        }
        double value, rate = 1, result;
        if (!parseNumber(converterValue_, value) ||
            (converterGroup_ == 4 && converterFrom_ != converterTo_ &&
             (!parseNumber(converterRate_, rate) || rate <= 0))) {
            message("Informe valor válido e taxa positiva", true);
            return true;
        }
        const auto &units = unitGroups()[converterGroup_].units;
        if (converterFrom_ >= int(units.size()) || converterTo_ >= int(units.size()))
            return true;
        auto expression = number(value) + ";" + units[converterFrom_] + ";" + units[converterTo_];
        if (converterGroup_ == 4)
            expression += ";" + number(rate);
        std::string error;
        if (!pda::convert(expression, result, error)) {
            message(error, true);
            return true;
        }
        converterResult_ = number(result) + " " + units[converterTo_];
        render();
        return true;
    }
    if (command == Command::FinancialSolve) {
        if (mode_ != Mode::Normal || app_ != 7 || calculatorMode_ != 2 || inputs_.size() != 6 ||
            argument > 4)
            return true;
        captureTools();
        TimeValue value;
        double *values[] = {&value.periods, &value.ratePercent, &value.present, &value.payment,
                            &value.future};
        for (unsigned i = 0; i < 5; ++i) {
            if (i == argument) {
                *values[i] = 0;
                continue;
            }
            if (!parseNumber(financeCalc_[i], *values[i])) {
                message("Preencha os valores conhecidos", true);
                return true;
            }
        }
        value.beginning = financeBeginning_;
        double result;
        std::string error;
        if (!financialCalculate(value, static_cast<FinancialUnknown>(argument), result, error)) {
            message(error, true);
            return true;
        }
        financeCalc_[argument] = number(result);
        calculatorResult_ = number(result, argument >= 2) + (argument == 0   ? " períodos"
                                                             : argument == 1 ? "% por período"
                                                                             : "");
        render();
        return true;
    }
    bool math = command == Command::Calculate || command == Command::ScientificFunction ||
                command == Command::MemoryAdd || command == Command::MemorySubtract ||
                command == Command::MemoryRecall || command == Command::MemoryClear;
    if (!math)
        return false;
    if (mode_ != Mode::Normal || app_ != 7 || calculatorMode_ == 2 || !editor_)
        return true;
    captureTools();
    if (command == Command::MemoryClear) {
        memory_ = 0;
        render();
        return true;
    }
    if (command == Command::MemoryRecall) {
        expression_ = number(memory_);
        render();
        return true;
    }
    auto candidate = expression_;
    if (command == Command::ScientificFunction) {
        if (calculatorMode_ != 1 || argument >= 12)
            return true;
        const char *functions[] = {"sin", "cos", "tan", "sqrt", "",   "",
                                   "ln",  "log", "exp", "fact", "pi", "e"};
        if (argument >= 10)
            candidate = functions[argument];
        else if (argument == 4)
            candidate = "(" + expression_ + ")^2";
        else if (argument == 5)
            candidate = "1/(" + expression_ + ")";
        else
            candidate = std::string(functions[argument]) + "(" + expression_ + ")";
        if (candidate.size() > 256) {
            message("Expressão extensa demais", true);
            return true;
        }
    }
    double result;
    std::string error;
    if (!scientificCalculate(candidate, degrees_, result, error)) {
        message(error, true);
        return true;
    }
    expression_ = candidate;
    if (command == Command::MemoryAdd || command == Command::MemorySubtract) {
        auto next = memory_ + (command == Command::MemoryAdd ? result : -result);
        if (!std::isfinite(next)) {
            message("Memória fora do limite", true);
            return true;
        }
        memory_ = next;
    }
    calculatorResult_ = number(result);
    render();
    return true;
}
} // namespace tab5_pda
