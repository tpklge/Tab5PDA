#include "presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>

namespace tab5_pda {
std::string capitalize(const std::string &text, TextCase mode) {
    if (mode == TextCase::None)
        return text;
    auto out = text;
    bool start = true;
    for (std::size_t at = 0; at < out.size();) {
        auto ch = static_cast<unsigned char>(out[at]);
        if (ch < 128) {
            bool letter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
            if (letter) {
                if ((start || mode == TextCase::Upper) && ch >= 'a' && ch <= 'z')
                    out[at] -= 'a' - 'A';
                start = false;
            } else if (ch == '\n' || (mode == TextCase::Words &&
                                      (ch == ' ' || ch == '\t' || ch == '-' || ch == '\'')))
                start = true;
            ++at;
        } else if (ch == 0xc3 && at + 1 < out.size()) {
            auto second = static_cast<unsigned char>(out[at + 1]);
            bool latinLetter =
                (second >= 0x80 && second <= 0xbe && second != 0x97 && second != 0xb7);
            if (latinLetter) {
                if ((start || mode == TextCase::Upper) && second >= 0xa0)
                    out[at + 1] = static_cast<char>(second - 32);
                start = false;
            }
            at += 2;
        } else {
            ++at;
            while (at < out.size() && (static_cast<unsigned char>(out[at]) & 0xc0) == 0x80)
                ++at;
        }
    }
    return out;
}
const Palette &palette(const std::string &id) {
    static const Palette themes[] = {{"sereno", "Sereno", 0xedf3f2, 0xffffff, 0x233c42, 0x223e46,
                                      0xdbe8e7, 0x3d7174, 0x365f65, 0x5c7478, 0xb9cdcc},
                                     {"areia", "Areia", 0xf4efe5, 0xfffcf5, 0x443e33, 0x494538,
                                      0xe6dfd0, 0x75724e, 0x686240, 0x736b5d, 0xd1c6b0},
                                     {"ardosia", "Ardósia", 0x1b242c, 0x27343e, 0xe2e9ed, 0x111a20,
                                      0x202e37, 0x456a74, 0xa5ced3, 0xb1c0c9, 0x526873}};
    for (const auto &theme : themes)
        if (id == theme.id)
            return theme;
    return themes[0];
}
const std::vector<UnitGroup> &unitGroups() {
    static const std::vector<UnitGroup> groups = {
        {"Comprimento", {"mm", "cm", "m", "km", "in", "ft", "mi"}},
        {"Massa", {"g", "kg", "lb", "oz"}},
        {"Volume", {"ml", "l", "galUS"}},
        {"Temperatura", {"C", "F", "K"}},
        {"Moedas — taxa manual", {"BRL", "USD", "EUR"}}};
    return groups;
}
bool scientificCalculate(const std::string &source, bool degrees, double &out, std::string &error) {
    auto text = source;
    for (auto &ch : text) {
        if (ch == ',')
            ch = '.';
        else if (ch >= 'A' && ch <= 'Z')
            ch += 'a' - 'A';
    }
    if (text.empty() || text.size() > 256) {
        error = "Informe uma expressão de até 256 bytes";
        return false;
    }
    std::size_t at = 0;
    unsigned depth = 0, work = 0;
    bool okay = true;
    auto spaces = [&] {
        while (at < text.size() && text[at] == ' ')
            ++at;
    };
    std::function<double()> sum, term, unary, power, atom;
    constexpr double pi = 3.14159265358979323846;
    atom = [&]() -> double {
        spaces();
        if (++depth > 24 || ++work > 1024) {
            okay = false;
            --depth;
            return 0;
        }
        double result = 0;
        if (at < text.size() && text[at] == '(') {
            ++at;
            result = sum();
            spaces();
            if (at >= text.size() || text[at++] != ')')
                okay = false;
        } else if (at < text.size() && text[at] >= 'a' && text[at] <= 'z') {
            auto begin = at;
            while (at < text.size() && text[at] >= 'a' && text[at] <= 'z')
                ++at;
            auto name = text.substr(begin, at - begin);
            if (name == "pi")
                result = pi;
            else if (name == "e")
                result = std::exp(1.0);
            else {
                spaces();
                if (at >= text.size() || text[at++] != '(')
                    okay = false;
                if (okay)
                    result = sum();
                spaces();
                if (at >= text.size() || text[at++] != ')')
                    okay = false;
                double angle = degrees ? result * pi / 180 : result;
                if (name == "sin")
                    result = std::sin(angle);
                else if (name == "cos")
                    result = std::cos(angle);
                else if (name == "tan") {
                    if (std::abs(std::cos(angle)) < 1e-12)
                        okay = false;
                    else
                        result = std::tan(angle);
                } else if (name == "sqrt") {
                    if (result < 0)
                        okay = false;
                    else
                        result = std::sqrt(result);
                } else if (name == "ln") {
                    if (result <= 0)
                        okay = false;
                    else
                        result = std::log(result);
                } else if (name == "log") {
                    if (result <= 0)
                        okay = false;
                    else
                        result = std::log10(result);
                } else if (name == "exp")
                    result = std::exp(result);
                else if (name == "abs")
                    result = std::abs(result);
                else if (name == "fact") {
                    if (result < 0 || result > 170 || std::floor(result) != result)
                        okay = false;
                    else
                        result = std::tgamma(result + 1);
                } else
                    okay = false;
            }
        } else {
            char *end = nullptr;
            result = std::strtod(text.c_str() + at, &end);
            if (end == text.c_str() + at)
                okay = false;
            else
                at = std::size_t(end - text.c_str());
        }
        spaces();
        while (okay && at < text.size() && text[at] == '%') {
            result /= 100;
            ++at;
            spaces();
        }
        if (!std::isfinite(result))
            okay = false;
        --depth;
        return result;
    };
    power = [&]() -> double {
        double result = atom();
        spaces();
        if (okay && at < text.size() && text[at] == '^') {
            ++at;
            // Count exponent recursion too, including chains without parentheses.
            if (++depth > 24)
                okay = false;
            double exponent = okay ? unary() : 0;
            --depth;
            result = std::pow(result, exponent);
        }
        if (!std::isfinite(result))
            okay = false;
        return result;
    };
    unary = [&]() -> double {
        spaces();
        if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
            bool negative = text[at++] == '-';
            if (++depth > 24) {
                --depth;
                okay = false;
                return 0;
            }
            auto value = unary();
            --depth;
            return negative ? -value : value;
        }
        return power();
    };
    term = [&]() -> double {
        double result = unary();
        spaces();
        while (okay && at < text.size() && (text[at] == '*' || text[at] == '/')) {
            auto op = text[at++];
            double rhs = unary();
            if (op == '/' && rhs == 0) {
                okay = false;
                break;
            }
            result = op == '*' ? result * rhs : result / rhs;
            spaces();
        }
        return result;
    };
    sum = [&]() -> double {
        double result = term();
        spaces();
        while (okay && at < text.size() && (text[at] == '+' || text[at] == '-')) {
            auto op = text[at++];
            double rhs = term();
            result = op == '+' ? result + rhs : result - rhs;
            spaces();
        }
        return result;
    };
    double result = sum();
    spaces();
    if (!okay || at != text.size() || !std::isfinite(result)) {
        error = "Expressão inválida ou função fora do domínio";
        return false;
    }
    out = result;
    error.clear();
    return true;
}
bool financialCalculate(const TimeValue &v, FinancialUnknown unknown, double &out,
                        std::string &error) {
    auto fail = [&](const char *why) {
        error = why;
        return false;
    };
    for (double value : {v.periods, v.ratePercent, v.present, v.payment, v.future})
        if (!std::isfinite(value) || std::abs(value) > 1e15)
            return fail("Valores financeiros fora do limite");
    if (unknown != FinancialUnknown::Periods && (v.periods <= 0 || v.periods > 1000000))
        return fail("Períodos devem estar entre 0 e 1.000.000");
    if (unknown != FinancialUnknown::Rate && v.ratePercent <= -100)
        return fail("A taxa deve ser maior que -100% por período");
    double rate = v.ratePercent / 100;
    auto factors = [&](double r, double &growth, double &annuity) {
        growth = std::exp(v.periods * std::log1p(r));
        annuity = r == 0 ? v.periods : std::expm1(v.periods * std::log1p(r)) / r;
        annuity *= v.beginning ? 1 + r : 1;
        return growth > 0 && std::isfinite(growth) && std::isfinite(annuity);
    };
    double result = 0, growth, annuity;
    if (unknown == FinancialUnknown::Rate) {
        int changes = 0, previousSign = 0;
        for (double flow : {v.present, v.payment, v.future}) {
            if (!flow)
                continue;
            int sign = flow < 0 ? -1 : 1;
            if (previousSign && sign != previousSign)
                ++changes;
            previousSign = sign;
        }
        if (changes != 1)
            return fail("A taxa exige entradas e saídas com uma única mudança de sinal");
        auto residual = [&](double r) {
            double g, a;
            if (!factors(r, g, a))
                return std::numeric_limits<double>::quiet_NaN();
            return v.present * g + v.payment * a + v.future;
        };
        double previousRate = std::expm1(-20.0), previous = residual(previousRate);
        bool found = false;
        for (unsigned n = 1; n <= 1024 && !found; ++n) {
            double currentRate = std::expm1(-20 + (20 + std::log(101.0)) * n / 1024);
            double current = residual(currentRate);
            if (previous == 0 && current == 0)
                return fail("Taxa indeterminada para esses fluxos");
            if (previous == 0) {
                result = previousRate * 100;
                found = true;
            } else if (current == 0) {
                result = currentRate * 100;
                found = true;
            }
            if (std::isfinite(previous) && std::isfinite(current) && !found &&
                std::signbit(current) != std::signbit(previous)) {
                double lo = previousRate, hi = currentRate;
                for (unsigned it = 0; it < 100; ++it) {
                    auto mid = (lo + hi) / 2, value = residual(mid);
                    if (std::signbit(value) == std::signbit(previous))
                        lo = mid;
                    else
                        hi = mid;
                }
                result = (lo + hi) * 50;
                found = true;
            }
            previousRate = currentRate;
            previous = current;
        }
        if (!found)
            return fail("Não foi encontrada taxa única no intervalo -100% a 10.000%");
    } else if (unknown == FinancialUnknown::Periods) {
        double due = v.beginning ? 1 + rate : 1;
        if (rate == 0) {
            if (!v.payment)
                return fail("Informe uma parcela para calcular os períodos com taxa zero");
            result = -(v.future + v.present) / v.payment;
        } else {
            double a = v.present + v.payment * due / rate;
            double b = v.payment * due / rate - v.future;
            if (!a || b / a <= 0)
                return fail("Os valores não permitem calcular os períodos");
            result = std::log(b / a) / std::log1p(rate);
        }
        if (result <= 0 || result > 1000000)
            return fail("Quantidade de períodos fora do limite");
    } else {
        if (!factors(rate, growth, annuity))
            return fail("Juros acumulados fora do limite");
        if (unknown == FinancialUnknown::Future)
            result = -(v.present * growth + v.payment * annuity);
        else if (unknown == FinancialUnknown::Present) {
            if (!growth)
                return fail("Fator de desconto fora do limite");
            result = -(v.future + v.payment * annuity) / growth;
        } else {
            if (!annuity)
                return fail("Não é possível calcular a parcela com esses valores");
            result = -(v.future + v.present * growth) / annuity;
        }
    }
    if (!std::isfinite(result) || std::abs(result) > 1e15)
        return fail("Resultado fora do limite");
    out = result;
    error.clear();
    return true;
}
} // namespace tab5_pda
