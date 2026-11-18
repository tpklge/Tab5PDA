#include "memory_files.hpp"
#include "presentation.hpp"
#include <cmath>
#include <iostream>
#include <random>
using namespace tab5_pda;
int main() {
    try {
        CHECK(capitalize("joão da silva", TextCase::Words) == "João Da Silva");
        CHECK(capitalize("árvore e çarles", TextCase::Words) == "Árvore E Çarles");
        CHECK(capitalize("d'ávila são-paulo", TextCase::Words) == "D'Ávila São-Paulo");
        CHECK(capitalize("observação\noutra linha", TextCase::Sentence) ==
              "Observação\nOutra linha");
        CHECK(capitalize("NASA e McDonald", TextCase::Words) == "NASA E McDonald");
        CHECK(capitalize("nome@exemplo.com", TextCase::None) == "nome@exemplo.com");
        CHECK(capitalize("brl", TextCase::Upper) == "BRL");
        CHECK(std::string(palette("areia").name) == "Areia");
        CHECK(std::string(palette("invalid").id) == "sereno");
        auto luminance = [](uint32_t color) {
            auto linear = [](unsigned byte) {
                double c = byte / 255.0;
                return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4);
            };
            return .2126 * linear((color >> 16) & 255) + .7152 * linear((color >> 8) & 255) +
                   .0722 * linear(color & 255);
        };
        auto contrast = [&](uint32_t a, uint32_t b) {
            auto first = luminance(a), second = luminance(b);
            return (std::max(first, second) + .05) / (std::min(first, second) + .05);
        };
        for (auto id : {"sereno", "areia", "ardosia"}) {
            const auto &theme = palette(id);
            CHECK(contrast(theme.ink, theme.surface) >= 4.5);
            CHECK(contrast(theme.accentText, theme.surface) >= 4.5);
            CHECK(contrast(theme.muted, theme.surface) >= 4.5);
            CHECK(contrast(0xffffff, theme.accent) >= 4.5);
        }
        double result;
        std::string error;
        auto evaluate = [&](const std::string &text, double expected, bool degrees = true) {
            CHECK(scientificCalculate(text, degrees, result, error));
            CHECK(std::abs(result - expected) < 1e-9 * std::max(1.0, std::abs(expected)));
        };
        evaluate("200*10%", 20);
        evaluate("1,5+2,5", 4);
        evaluate("SIN(90)", 1);
        evaluate("sin(90)", 1);
        evaluate("cos(pi)", -1, false);
        evaluate("sqrt(81)+log(100)+ln(exp(1))", 12);
        evaluate("-2^2", -4);
        evaluate("2^3^2", 512);
        evaluate("2^-3", .125);
        evaluate("fact(6)", 720);
        for (auto text : {"1/0", "sqrt(-1)", "ln(0)", "tan(90)", "fact(171)", "fact(2.5)",
                          "exp(999)", "sin(", "nan", "1 2", "2^^2"})
            CHECK(!scientificCalculate(text, true, result, error));
        CHECK(!scientificCalculate(std::string(100, '-') + "1", true, result, error));
        CHECK(!scientificCalculate(std::string(100, '(') + "1" + std::string(100, ')'), true,
                                   result, error));
        TimeValue value;
        value.present = 1000;
        value.ratePercent = 1;
        value.periods = 12;
        CHECK(financialCalculate(value, FinancialUnknown::Future, result, error));
        CHECK(std::abs(result + 1126.82503013) < 1e-6);
        value.future = result;
        CHECK(financialCalculate(value, FinancialUnknown::Rate, result, error));
        CHECK(std::abs(result - 1) < 1e-8);
        CHECK(financialCalculate(value, FinancialUnknown::Periods, result, error));
        CHECK(std::abs(result - 12) < 1e-8);
        value.future = 0;
        value.present = 10000;
        CHECK(financialCalculate(value, FinancialUnknown::Payment, result, error));
        CHECK(std::abs(result + 888.48788678) < 1e-6);
        value.payment = result;
        CHECK(financialCalculate(value, FinancialUnknown::Rate, result, error));
        CHECK(std::abs(result - 1) < 1e-8);
        value.ratePercent = 0;
        value.payment = -1000;
        value.future = 2000;
        CHECK(financialCalculate(value, FinancialUnknown::Periods, result, error) && result == 12);
        value.ratePercent = -100;
        CHECK(!financialCalculate(value, FinancialUnknown::Future, result, error));
        value = {};
        value.present = 0;
        value.payment = -100;
        value.future = 100;
        value.periods = 1;
        CHECK(!financialCalculate(value, FinancialUnknown::Rate, result, error));
        std::mt19937 rng(28);
        for (unsigned i = 0; i < 500; ++i) {
            TimeValue v;
            v.periods = 1 + rng() % 100;
            v.ratePercent = (rng() % 1000) / 100.0;
            v.present = 100 + rng() % 100000;
            v.payment = rng() % 1000;
            v.beginning = rng() % 2;
            CHECK(financialCalculate(v, FinancialUnknown::Future, v.future, error));
            CHECK(financialCalculate(v, FinancialUnknown::Present, result, error));
            CHECK(std::abs(result - v.present) < 1e-7 * v.present);
            CHECK(financialCalculate(v, FinancialUnknown::Rate, result, error));
            CHECK(std::abs(result - v.ratePercent) < 1e-6);
        }
        std::cout << "Portuguese casing, scientific domains, 500 TVM roundtrips and themes OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
