#include "tab_ui.hpp"
#include <algorithm>
#include <cstdio>
#include <sstream>
namespace tab5_pda {
namespace {
constexpr std::size_t docLimit = 128 * 1024;
bool ends(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}
std::size_t byteAt(const std::string &s, unsigned character) {
    std::size_t p = 0;
    while (p < s.size() && character) {
        ++p;
        while (p < s.size() && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80)
            ++p;
        --character;
    }
    return p;
}
std::string envelope(const std::vector<LedgerEntry> &rows, uint64_t generation) {
    auto data = ledgerCsv(rows);
    auto protectedData = std::to_string(generation) + "\n" + data;
    return "PDA-LEDGER-2\n" + std::to_string(generation) + "\n" +
           std::to_string(pda::crc32(reinterpret_cast<const uint8_t *>(protectedData.data()),
                                     protectedData.size())) +
           "\n" + data;
}
bool unframe(const std::string &data, std::vector<LedgerEntry> &out, uint64_t &generation) {
    std::istringstream in(data);
    std::string magic, gen, crc;
    if (!std::getline(in, magic) || (magic != "PDA-LEDGER-1" && magic != "PDA-LEDGER-2") ||
        !std::getline(in, gen) || !std::getline(in, crc))
        return false;
    auto number = [](const std::string &s, uint64_t &n) {
        if (s.empty() || s.size() > 20)
            return false;
        n = 0;
        for (char c : s) {
            if (c < '0' || c > '9' || n > (UINT64_MAX - (c - '0')) / 10)
                return false;
            n = n * 10 + c - '0';
        }
        return true;
    };
    uint64_t sum;
    if (!number(gen, generation) || !generation || !number(crc, sum))
        return false;
    auto pos = std::size_t(in.tellg());
    if (pos > data.size())
        return false;
    auto csv = data.substr(pos);
    auto protectedData = magic == "PDA-LEDGER-2" ? gen + "\n" + csv : csv;
    std::string error;
    return sum == pda::crc32(reinterpret_cast<const uint8_t *>(protectedData.data()),
                             protectedData.size()) &&
           loadLedger(csv, out, error);
}
} // namespace
void Ui::captureProductivity() {
    if (mode_ != Mode::Productivity)
        return;
    if (productPage_ == 1 || productPage_ == 2) {
        if (!inputs_.empty()) {
            productName_ = inputValue(inputs_[0]);
            if (productPage_ == 1)
                sheetName_ = productName_;
            else
                documentName_ = productName_;
            if (productPage_ == 1 && inputs_.size() == 2) {
                sheetEditCell_ = sheetCell_;
                sheetEdit_ = inputValue(inputs_[1]);
            }
        }
        if (productPage_ == 2 && productEditor_)
            documentText_ = lv_textarea_get_text(productEditor_);
    }
    if (productPage_ == 6 && inputs_.size() == 2) {
        documentFind_ = inputValue(inputs_[0]);
        documentReplace_ = inputValue(inputs_[1]);
    }
    if (productPage_ == 5 && !inputs_.empty())
        financeMonth_ = inputValue(inputs_[0]);
    if ((productPage_ == 7 || productPage_ == 8) && inputs_.size() == 8) {
        financeDraftPage_ = productPage_;
        financeDraft_.clear();
        for (unsigned i = 0; i < inputs_.size(); ++i)
            financeDraft_.push_back(i < 6 ? inputValue(inputs_[i])
                                          : std::to_string(lv_dropdown_get_selected(inputs_[i])));
    }
}
bool Ui::openPicture(const std::string &name) {
    std::vector<uint16_t> pixels;
    std::string error;
    if (!platform_.picture(name, pixels, error) || pixels.size() != 900 * 420) {
        message(error.empty() ? "Imagem decodificada com tamanho inválido" : error, true);
        return false;
    }
    lv_img_cache_invalidate_src(&galleryImage_);
    galleryPixels_ = std::move(pixels);
    galleryName_ = name;
    return true;
}
bool Ui::saveProductDraft() {
    if (sheetDirty_ && !service_.writeFile("sheet-recovery.csv", sheet_.csv())) {
        message(service_.error(), true);
        return false;
    }
    if (!documentDirty_)
        return true;
    bool ok = service_.writeFile("document-recovery.txt", documentText_);
    if (!ok)
        message(service_.error(), true);
    return ok;
}
void Ui::productChanged(lv_event_t *event) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
    if (ui->changingDocument_)
        return;
    auto text = std::string(lv_textarea_get_text(lv_event_get_target(event)));
    if (text == ui->documentText_)
        return;
    auto now = ui->platform_.nowMs();
    if (ui->documentUndo_.empty() || now - ui->documentUndoAt_ > 750) {
        ui->documentUndo_.push_back(ui->documentText_);
        if (ui->documentUndo_.size() > 8)
            ui->documentUndo_.erase(ui->documentUndo_.begin());
        ui->documentUndoAt_ = now;
    }
    ui->documentText_ = text;
    ui->documentDirty_ = true;
}
void Ui::sheetTouched(lv_event_t *event) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
    uint16_t row, col;
    lv_table_get_selected_cell(lv_event_get_target(event), &row, &col);
    if (row && col && row <= 20 && col <= 8)
        ui->commands_.push_back({Command::SheetCell, (row - 1) * 8 + col - 1});
}
void Ui::readerPosition(bool save) {
    if (bookName_.empty())
        return;
    auto name = "reading-" +
                std::to_string(pda::crc32(reinterpret_cast<const uint8_t *>(bookName_.data()),
                                          bookName_.size())) +
                ".cfg";
    auto checksum = std::to_string(
        pda::crc32(reinterpret_cast<const uint8_t *>(bookText_.data()), bookText_.size()));
    if (save) {
        std::string text = bookName_ + "\n" + checksum + "\n" + std::to_string(readerPage_) + "\n";
        for (auto page : readerMarks_)
            text += std::to_string(page) + " ";
        if (!service_.writeFile(name, text))
            message(service_.error(), true);
    } else {
        readerPage_ = 0;
        readerMarks_.clear();
        std::string text;
        if (!service_.readFile(name, text))
            return;
        std::istringstream in(text);
        std::string stored, crc;
        if (!std::getline(in, stored) || stored != bookName_ || !std::getline(in, crc) ||
            crc != checksum)
            return;
        unsigned page;
        if (!(in >> page) || page >= readingPages(bookText_))
            return;
        readerPage_ = page;
        while (in >> page) {
            if (page < readingPages(bookText_) && readerMarks_.size() < 16)
                readerMarks_.push_back(page);
        }
    }
}
void Ui::loadFinance() {
    if (!service_.writable()) {
        financeOkay_ = false;
        message("microSD indisponível para o livro financeiro", true);
        return;
    }
    ledger_.clear();
    ledgerGeneration_ = 0;
    ledgerBank_ = -1;
    financeOkay_ = true;
    bool any = false;
    for (int bank = 0; bank < 2; ++bank) {
        auto name = bank ? "finance-b.dat" : "finance-a.dat";
        std::string text;
        std::vector<LedgerEntry> rows;
        uint64_t generation;
        pda::ReadResult status;
        // An unreadable bank may be newer than the readable bank. Do not permit
        // writes until both banks can be inspected or confirmed missing.
        if (!service_.readFile(name, text, &status)) {
            if (status == pda::ReadResult::Error)
                financeOkay_ = false;
            continue;
        }
        any = true;
        if (unframe(text, rows, generation) &&
            (ledgerBank_ < 0 || generation > ledgerGeneration_)) {
            ledger_ = std::move(rows);
            ledgerGeneration_ = generation;
            ledgerBank_ = bank;
        }
    }
    if (any && ledgerBank_ < 0) {
        financeOkay_ = false;
        message("Livro financeiro inválido; preserve os arquivos finance-a/b.dat", true);
    }
}
bool Ui::saveFinance(const std::vector<LedgerEntry> &next) {
    std::string error;
    if (!financeOkay_ || ledgerGeneration_ == UINT64_MAX || !validLedger(next, error)) {
        message(error.empty() ? "Livro financeiro indisponível" : error, true);
        return false;
    }
    int bank = ledgerBank_ == 0 ? 1 : 0;
    if (!service_.writeFile(bank ? "finance-b.dat" : "finance-a.dat",
                            envelope(next, ledgerGeneration_ + 1))) {
        message(service_.error(), true);
        return false;
    }
    ledger_ = next;
    ledgerBank_ = bank;
    ++ledgerGeneration_;
    return true;
}
void Ui::renderProductivity() {
    button(body_, 12, 8, 210, 46, "Novos aplicativos", Command::Productivity);
    button(body_, 234, 8, 180, 46, "Voltar ao PDA", Command::Cancel);
    button(body_, 772, 8, 212, 46, "Fechar teclado", Command::KeyboardHide);
    if (productListing_) {
        fileEntries_.clear();
        for (const auto &f : platform_.files()) {
            bool match = productPage_ == 1 ? ends(f.name, ".csv") && f.name != "finance-export.csv"
                         : productPage_ == 4 ? ends(f.name, ".jpg") || ends(f.name, ".jpeg") ||
                                                   ends(f.name, ".png") || ends(f.name, ".bmp")
                                             : ends(f.name, ".txt") || ends(f.name, ".md");
            if (match && f.name != "sheet-recovery.csv" && f.name != "document-recovery.txt")
                fileEntries_.push_back(f);
        }
        productFilePage_ = std::clamp(
            productFilePage_, 0, fileEntries_.empty() ? 0 : int((fileEntries_.size() - 1) / 16));
        button(body_, 12, 70, 210, 46, "Página -", Command::ProductFile, UINT32_MAX);
        button(body_, 234, 70, 210, 46, "Página +", Command::ProductFile, UINT32_MAX - 1);
        auto *list = panel(body_, 12, 134, 976, 474);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        for (std::size_t i = productFilePage_ * 16;
             i < std::min(fileEntries_.size(), std::size_t(productFilePage_ * 16 + 16)); ++i)
            button(list, 12, 8 + (i - productFilePage_ * 16) * 58, 940, 50, fileEntries_[i].name,
                   Command::ProductFile, i);
        return;
    }
    if (productPage_ == 0) {
        const char *titles[] = {"Miniplanilha", "Documentos", "Livros e marcadores",
                                "Galeria de imagens", "Finanças pessoais"};
        for (int i = 0; i < 5; ++i)
            button(body_, 12 + i % 2 * 490, 94 + i / 2 * 164, 474, 144, titles[i],
                   Command::Productivity, i + 1);
        return;
    }
    if (productPage_ == 1) {
        label(body_, 12, 68, 180, "Arquivo CSV");
        inputs_.push_back(input(body_, 12, 94, 630, 50, productName_, 80));
        inputLimits_.push_back(80);
        inputHeights_.push_back(50);
        button(body_, 660, 94, 150, 50, "Abrir CSV", Command::ProductLoad);
        button(body_, 822, 94, 162, 50, "Salvar CSV", Command::ProductSave);
        auto *table = lv_table_create(body_);
        lv_obj_set_pos(table, 12, 160);
        lv_obj_set_size(table, 630, 444);
        lv_obj_set_style_text_font(table, &pda_font_16, 0);
        lv_obj_set_style_bg_color(table, lv_color_hex(0xffffff), LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(table, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(table, lv_color_hex(0xc6d2dc), LV_PART_ITEMS);
        lv_obj_set_style_pad_hor(table, 10, LV_PART_ITEMS);
        lv_obj_set_style_pad_ver(table, 8, LV_PART_ITEMS);
        lv_table_set_col_cnt(table, 9);
        lv_table_set_row_cnt(table, 21);
        lv_table_set_col_width(table, 0, 48);
        for (int c = 0; c < 8; ++c) {
            lv_table_set_col_width(table, c + 1, 108);
            char name[2] = {char('A' + c), 0};
            lv_table_set_cell_value(table, 0, c + 1, name);
        }
        for (int r = 0; r < 20; ++r) {
            lv_table_set_cell_value(table, r + 1, 0, std::to_string(r + 1).c_str());
            for (int c = 0; c < 8; ++c) {
                auto &raw = sheet_.cells[r * 8 + c];
                std::string value = raw;
                if (!raw.empty() && raw.front() == '=') {
                    double n;
                    std::string error;
                    char text[40];
                    if (sheet_.value(r * 8 + c, n, error)) {
                        std::snprintf(text, sizeof(text), "%.10g", n);
                        value = text;
                    } else
                        value = "#ERRO";
                }
                lv_table_set_cell_value(table, r + 1, c + 1, value.c_str());
            }
        }
        lv_obj_add_event_cb(table, sheetTouched, LV_EVENT_VALUE_CHANGED, this);
        char cell[16];
        std::snprintf(cell, sizeof(cell), "Célula %c%d", 'A' + sheetCell_ % 8, 1 + sheetCell_ / 8);
        label(body_, 660, 172, 310, cell);
        inputs_.push_back(
            input(body_, 660, 212, 310, 62,
                  sheetEditCell_ == sheetCell_ ? sheetEdit_ : sheet_.cells[sheetCell_], 128));
        inputLimits_.push_back(128);
        inputHeights_.push_back(62);
        button(body_, 660, 292, 310, 56, "Aplicar célula", Command::SheetApply);
        button(body_, 660, 364, 310, 56, "Nova planilha", Command::ProductNew);
        label(body_, 660, 444, 310,
              "20 linhas × 8 colunas\nEx.: =A1+B1*2\n=SUM(A1:B5)\n=SOMA(A1:A5)\nCSV guarda as "
              "fórmulas.");
        editor_ = inputs_[1];
        field_ = 1;
        return;
    }
    if (productPage_ == 2) {
        button(body_, 426, 8, 156, 46, "Salvar", Command::ProductSave);
        button(body_, 594, 8, 166, 46, "Desfazer", Command::DocUndo);
        label(body_, 12, 68, 220, "Nome do documento");
        inputs_.push_back(input(body_, 12, 94, 630, 50, productName_, 80));
        inputLimits_.push_back(80);
        inputHeights_.push_back(50);
        button(body_, 660, 94, 150, 50, "Abrir texto", Command::ProductLoad);
        button(body_, 822, 94, 162, 50, "Novo texto", Command::ProductNew);
        const char *names[] = {"Copiar", "Recortar", "Colar", "Localizar", "Substituir"};
        const Command commands[] = {Command::DocCopy, Command::DocCut, Command::DocPaste,
                                    Command::DocFind, Command::DocReplace};
        for (int i = 0; i < 5; ++i)
            button(body_, 12 + i * 196, 158, 184, 46, names[i], commands[i]);
        productEditor_ = input(body_, 12, 220, 952, 380, documentText_, docLimit, true);
        lv_textarea_set_text_selection(productEditor_, true);
        lv_obj_add_event_cb(productEditor_, productChanged, LV_EVENT_VALUE_CHANGED, this);
        inputs_.push_back(productEditor_);
        inputLimits_.push_back(docLimit);
        inputHeights_.push_back(380);
        editor_ = productEditor_;
        field_ = 1;
        return;
    }
    if (productPage_ == 6) {
        label(body_, 12, 76, 940, "Localizar");
        inputs_.push_back(input(body_, 12, 112, 952, 58, documentFind_, 256));
        label(body_, 12, 194, 940, "Substituir por");
        inputs_.push_back(input(body_, 12, 230, 952, 58, documentReplace_, 256));
        inputLimits_ = {256, 256};
        inputHeights_ = {58, 58};
        button(body_, 12, 330, 300, 58, "Localizar no documento", Command::DocSearch);
        button(body_, 328, 330, 300, 58, "Substituir todas", Command::DocReplace, 1);
        button(body_, 644, 330, 320, 58, "Voltar ao documento", Command::Productivity, 2);
        editor_ = inputs_[0];
        field_ = 0;
        return;
    }
    if (productPage_ == 3) {
        button(body_, 426, 8, 334, 46, "Abrir livro TXT / Markdown", Command::ProductLoad);
        label(body_, 12, 72, 960,
              bookName_.empty()
                  ? "Escolha um arquivo. A leitura é texto simples, sem renderização Markdown."
                  : bookName_ + " — página " + std::to_string(readerPage_ + 1) + "/" +
                        std::to_string(readingPages(bookText_)));
        auto *page = panel(body_, 12, 114, 976, 420);
        lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        label(page, 16, 14, 940, readingPage(bookText_, readerPage_));
        button(body_, 12, 552, 220, 50, "Anterior", Command::ReaderMove, 0);
        button(body_, 244, 552, 220, 50, "Próxima", Command::ReaderMove, 1);
        button(body_, 476, 552, 238, 50, "Marcar / desmarcar", Command::ReaderMark);
        button(body_, 726, 552, 258, 50, "Ir ao marcador", Command::ReaderBookmark);
        return;
    }
    if (productPage_ == 4) {
        button(body_, 426, 8, 334, 46, "Abrir galeria", Command::ProductLoad);
        label(body_, 12, 72, 960, galleryName_);
        if (!galleryPixels_.empty()) {
            galleryImage_ = {};
            galleryImage_.header.cf = LV_IMG_CF_TRUE_COLOR;
            galleryImage_.header.w = 900;
            galleryImage_.header.h = 420;
            galleryImage_.data_size = galleryPixels_.size() * 2;
            galleryImage_.data = reinterpret_cast<const uint8_t *>(galleryPixels_.data());
            auto *img = lv_img_create(body_);
            lv_img_set_src(img, &galleryImage_);
            lv_obj_set_pos(img, 44, 114);
        }
        button(body_, 12, 552, 460, 50, "Imagem anterior", Command::GalleryMove, 0);
        button(body_, 488, 552, 496, 50, "Próxima imagem", Command::GalleryMove, 1);
        return;
    }
    if (productPage_ == 5) {
        label(body_, 12, 70, 220, "Mês (AAAA-MM)");
        inputs_.push_back(input(body_, 12, 102, 220, 50, financeMonth_, 7));
        inputLimits_.push_back(7);
        inputHeights_.push_back(50);
        button(body_, 244, 102, 220, 50, "Atualizar resumo", Command::Productivity, 5);
        button(body_, 476, 102, 238, 50, "Novo lançamento", Command::FinanceNew);
        button(body_, 726, 102, 258, 50, "Exportar CSV", Command::FinanceExport);
        button(body_, 12, 168, 460, 50, "Lançar recorrências do mês", Command::FinancePost);
        button(body_, 488, 168, 496, 50, "Definir orçamento mensal", Command::FinanceBudget);
        auto *view = panel(body_, 12, 234, 976, 370);
        lv_obj_add_flag(view, LV_OBJ_FLAG_SCROLLABLE);
        std::string summary = "Saldos do mês por conta e moeda:\n";
        for (const auto &total : ledgerTotals(ledger_, financeMonth_))
            summary += total.first + ": " + pda::formatMoney(total.second) + "\n";
        summary += "\nOrçamentos do mês:\n";
        for (const auto &b : ledger_)
            if (b.budget && b.day.substr(0, 7) == financeMonth_) {
                int64_t spent = 0;
                for (const auto &e : ledger_)
                    if (!e.budget && e.day.substr(0, 7) == financeMonth_ &&
                        e.currency == b.currency &&
                        (b.category.empty() || e.category == b.category) && e.cents < 0)
                        spent -= e.cents;
                summary += (b.category.empty() ? "Todas as categorias" : b.category) + " / " +
                           b.currency + ": " + pda::formatMoney(spent) + " de " +
                           pda::formatMoney(b.cents) + "\n";
            }
        summary += "\nLançamentos (receita + / despesa -):\n";
        for (const auto &e : ledger_)
            if (!e.budget && e.day.substr(0, 7) == financeMonth_)
                summary += e.day + " " + e.account + " / " + e.title + " / " +
                           pda::formatMoney(e.cents) + " " + e.currency + "\n";
        label(view, 12, 12, 940, summary);
        editor_ = inputs_[0];
        field_ = 0;
        return;
    }
    if (productPage_ == 7 || productPage_ == 8) {
        const char *captions[] = {"Descrição", "Data (AAAA-MM-DD)", "Conta", "Valor (positivo)",
                                  "Moeda",     "Categoria"};
        const char *defaults[] = {"", "", "Carteira", "", "BRL", ""};
        for (int i = 0; i < 6; ++i) {
            int x = 12 + i % 2 * 490, y = 76 + i / 2 * 100;
            label(body_, x, y, 474, captions[i]);
            std::string value =
                i == 1 ? (platform_.localMinute().empty() ? "2000-01-01"
                                                          : platform_.localMinute().substr(0, 10))
                       : defaults[i];
            if (productPage_ == 8 && i == 1)
                value = financeMonth_ + "-01";
            if (financeDraftPage_ == productPage_ && financeDraft_.size() == 8)
                value = financeDraft_[i];
            inputs_.push_back(input(body_, x, y + 28, 460, 54, value, i == 0 ? 80 : 40));
            if (i == 0)
                caseInput(inputs_.back(), TextCase::Sentence);
            if (i == 2 || i == 5)
                caseInput(inputs_.back(), TextCase::Words);
            if (i == 4)
                caseInput(inputs_.back(), TextCase::Upper);
            inputLimits_.push_back(i == 0 ? 80 : 40);
            inputHeights_.push_back(54);
            if (i == 1)
                pickerButton(body_, x, y + 28, inputs_.back(), "ledgerDate");
        }
        label(body_, 12, 390, 460, "Tipo");
        bool resume = financeDraftPage_ == productPage_ && financeDraft_.size() == 8;
        inputs_.push_back(dropdown(body_, 12, 420, "Despesa\nReceita",
                                   resume && financeDraft_[6] == "1" ? 1 : 0));
        label(body_, 502, 390, 460, "Repetição");
        inputs_.push_back(dropdown(body_, 502, 420, "Não repetir\nMensal",
                                   resume && financeDraft_[7] == "1" ? 1 : 0));
        inputLimits_.insert(inputLimits_.end(), 2, 0);
        inputHeights_.insert(inputHeights_.end(), 2, 56);
        button(body_, 12, 514, 460, 60,
               productPage_ == 8 ? "Salvar orçamento" : "Salvar lançamento", Command::FinanceSave);
        button(body_, 488, 514, 496, 60, "Cancelar lançamento", Command::FinanceCancel);
        editor_ = inputs_[0];
        field_ = 0;
        return;
    }
}
bool Ui::handleProductivity(Command c, uint32_t a) {
    if (mode_ == Mode::Productivity &&
        (c == Command::App || c == Command::Cancel || c == Command::Extras)) {
        captureProductivity();
        if (!saveProductDraft())
            return true;
    }
    if (c == Command::Productivity) {
        if (a > 8)
            return true;
        captureProductivity();
        if ((productPage_ == 2 || productPage_ == 1) && a != unsigned(productPage_) &&
            !saveProductDraft())
            return true;
        mode_ = Mode::Productivity;
        productPage_ = a;
        if (a == 1)
            productName_ = sheetName_;
        if (a == 2)
            productName_ = documentName_;
        productListing_ = false;
        usbEnabled_ = false;
        if (a == 1 && !sheetLoaded_) {
            sheetLoaded_ = true;
            auto files = platform_.files();
            if (std::any_of(files.begin(), files.end(),
                            [](const auto &f) { return f.name == "sheet-recovery.csv"; })) {
                std::string text, error;
                if (service_.readFile("sheet-recovery.csv", text) && sheet_.load(text, error)) {
                    sheetDirty_ = true;
                    productName_ = "recovered-sheet.csv";
                }
            }
        }
        if (a == 1 && !ends(productName_, ".csv"))
            productName_ = "sheet.csv";
        if (a == 2 && (ends(productName_, ".csv") || productName_.empty()))
            productName_ = "document.txt";
        if (a == 2 && !documentLoaded_) {
            documentLoaded_ = true;
            auto files = platform_.files();
            if (std::any_of(files.begin(), files.end(),
                            [](const auto &f) { return f.name == "document-recovery.txt"; })) {
                std::string text;
                if (service_.readFile("document-recovery.txt", text) && text.size() <= docLimit &&
                    pda::validUtf8(text)) {
                    documentText_ = text;
                    documentDirty_ = true;
                }
            }
        }
        if (a == 5) {
            if (financeMonth_.empty())
                financeMonth_ = platform_.localMinute().empty()
                                    ? "2000-01"
                                    : platform_.localMinute().substr(0, 7);
            loadFinance();
        }
        render();
        if (a == 5)
            message(financeOkay_ ? "Livro financeiro pronto no microSD"
                                 : "Livro financeiro indisponível; preserve finance-a/b.dat",
                    !financeOkay_);
        return true;
    }
    if (mode_ != Mode::Productivity)
        return false;
    // Late or repeated button events can arrive after a render changes the form.
    // Validate the current page before using any of its input pointers.
    if (c >= Command::ProductFile && c <= Command::FinanceCancel) {
        bool allowed = false;
        if (productListing_)
            allowed = c == Command::ProductFile;
        else if (c == Command::ProductFile)
            allowed = productPage_ == 4;
        else if (c == Command::ProductLoad)
            allowed = productPage_ >= 1 && productPage_ <= 4;
        else if (c == Command::ProductSave || c == Command::ProductNew)
            allowed = productPage_ == 1 || productPage_ == 2;
        else if (c == Command::SheetCell || c == Command::SheetApply)
            allowed = productPage_ == 1 && inputs_.size() == 2;
        else if (c >= Command::DocCopy && c <= Command::DocFind)
            allowed = productPage_ == 2;
        else if (c == Command::DocReplace)
            allowed = a ? productPage_ == 6 : productPage_ == 2;
        else if (c == Command::DocSearch)
            allowed = productPage_ == 6;
        else if (c >= Command::ReaderMove && c <= Command::ReaderBookmark)
            allowed = productPage_ == 3;
        else if (c == Command::GalleryMove)
            allowed = productPage_ == 4;
        else if (c == Command::FinanceSave || c == Command::FinanceCancel)
            allowed = (productPage_ == 7 || productPage_ == 8) && inputs_.size() == 8;
        else if (c >= Command::FinanceNew && c <= Command::FinanceExport)
            allowed = productPage_ == 5;
        if (!allowed)
            return true;
    }
    if (c == Command::ProductLoad) {
        captureProductivity();
        if ((productPage_ == 2 && documentDirty_) || (productPage_ == 1 && sheetDirty_)) {
            message("Salve o arquivo atual antes de abrir outro", true);
            return true;
        }
        productListing_ = true;
        productFilePage_ = 0;
        render();
        return true;
    }
    if (c == Command::ProductFile) {
        if (a >= UINT32_MAX - 1) {
            productFilePage_ += a == UINT32_MAX ? -1 : 1;
            render();
            return true;
        }
        if (a >= fileEntries_.size())
            return true;
        auto name = fileEntries_[a].name;
        std::string text, error;
        if (productPage_ == 4) {
            if (!openPicture(name)) {
                return true;
            }
        } else {
            if (!service_.readFile(name, text)) {
                message(service_.error(), true);
                return true;
            }
            text = normalizeDocumentText(text);
            if (!pda::validUtf8(text)) {
                message("Arquivo não é texto UTF-8 válido", true);
                return true;
            }
            if (productPage_ == 1) {
                if (!sheet_.load(text, error)) {
                    message(error, true);
                    return true;
                }
            } else if (productPage_ == 2) {
                if (text.size() > docLimit) {
                    message("Documento maior que 128 KiB", true);
                    return true;
                }
                documentText_ = text;
                documentUndo_.clear();
                documentDirty_ = false;
            } else {
                if (text.size() > 512 * 1024) {
                    message("Livro maior que 512 KiB", true);
                    return true;
                }
                bookName_ = name;
                bookText_ = text;
                readerPosition(false);
            }
        }
        if (productPage_ == 1) {
            sheetLoaded_ = true;
            sheetDirty_ = false;
            sheetEditCell_ = -1;
        }
        productName_ = name;
        if (productPage_ == 1)
            sheetName_ = name;
        if (productPage_ == 2)
            documentName_ = name;
        productListing_ = false;
        render();
        message("Arquivo aberto do microSD");
        return true;
    }
    if (c == Command::ProductSave) {
        captureProductivity();
        bool sheet = productPage_ == 1;
        if (!safeName(productName_) || productName_ == "sheet-recovery.csv" ||
            productName_ == "document-recovery.txt" ||
            (sheet ? !ends(productName_, ".csv") || productName_ == "finance-export.csv"
                   : !ends(productName_, ".txt") && !ends(productName_, ".md"))) {
            message("Use nome válido com extensão do aplicativo", true);
            return true;
        }
        auto text = sheet ? sheet_.csv() : documentText_;
        if (!sheet && (text.size() > docLimit || !pda::validUtf8(text))) {
            message("Texto inválido ou maior que 128 KiB", true);
            return true;
        }
        if (!service_.writeFile(sheet ? "sheet-recovery.csv" : "document-recovery.txt", text)) {
            message(service_.error(), true);
            return true;
        }
        bool ok = service_.writeFile(productName_, text);
        if (ok && sheet) {
            sheetDirty_ = false;
            platform_.removeFile("sheet-recovery.csv");
        }
        if (ok && !sheet) {
            documentDirty_ = false;
            platform_.removeFile("document-recovery.txt");
        }
        message(ok ? "Arquivo verificado no microSD" : service_.error(), !ok);
        return true;
    }
    if (c == Command::ProductNew) {
        captureProductivity();
        if ((productPage_ == 2 && documentDirty_) || (productPage_ == 1 && sheetDirty_)) {
            message("Salve o arquivo atual antes de criar outro", true);
            return true;
        }
        if (productPage_ == 1) {
            sheet_ = {};
            sheetDirty_ = true;
            sheetEditCell_ = -1;
            productName_ = "sheet.csv";
        } else {
            if (!saveProductDraft())
                return true;
            documentUndo_.push_back(documentText_);
            if (documentUndo_.size() > 8)
                documentUndo_.erase(documentUndo_.begin());
            documentText_.clear();
            documentDirty_ = true;
            productName_ = "document.txt";
        }
        render();
        return true;
    }
    if (c == Command::SheetCell) {
        captureProductivity();
        sheetCell_ = a < 160 ? a : 0;
        sheetEditCell_ = -1;
        render();
        return true;
    }
    if (c == Command::SheetApply) {
        captureProductivity();
        auto value = inputValue(inputs_[1]);
        if (value.size() > 128 || !pda::validUtf8(value)) {
            message("Célula: até 128 bytes UTF-8", true);
            return true;
        }
        sheet_.cells[sheetCell_] = value;
        sheetEditCell_ = -1;
        sheetDirty_ = true;
        render();
        return true;
    }
    if (c == Command::DocUndo) {
        captureProductivity();
        if (!documentUndo_.empty()) {
            documentText_ = documentUndo_.back();
            documentUndo_.pop_back();
            documentDirty_ = true;
            render();
        }
        return true;
    }
    if (c == Command::DocCopy || c == Command::DocCut || c == Command::DocPaste) {
        captureProductivity();
        if (!productEditor_)
            return true;
        auto *label = lv_textarea_get_label(productEditor_);
        auto start = lv_label_get_text_selection_start(label),
             end = lv_label_get_text_selection_end(label);
        bool selected =
            start != LV_DRAW_LABEL_NO_TXT_SEL && end != LV_DRAW_LABEL_NO_TXT_SEL && start != end;
        auto first = selected ? byteAt(documentText_, std::min(start, end)) : 0,
             last = selected ? byteAt(documentText_, std::max(start, end)) : documentText_.size();
        if (c == Command::DocCopy) {
            clipboard_ = documentText_.substr(first, last - first);
            message("Texto copiado");
            return true;
        }
        if (c == Command::DocCut && !selected) {
            message("Selecione um trecho para recortar", true);
            return true;
        }
        auto text = documentText_;
        if (c == Command::DocCut) {
            clipboard_ = text.substr(first, last - first);
            text.erase(first, last - first);
        } else {
            if (!selected) {
                first = byteAt(text, lv_textarea_get_cursor_pos(productEditor_));
                last = first;
            }
            text.replace(first, last - first, clipboard_);
        }
        if (text.size() > docLimit) {
            message("Documento maior que 128 KiB", true);
            return true;
        }
        documentUndo_.push_back(documentText_);
        if (documentUndo_.size() > 8)
            documentUndo_.erase(documentUndo_.begin());
        documentText_ = std::move(text);
        documentDirty_ = true;
        render();
        return true;
    }
    if (c == Command::DocFind || (c == Command::DocReplace && !a)) {
        captureProductivity();
        productPage_ = 6;
        render();
        return true;
    }
    if (c == Command::DocSearch || (c == Command::DocReplace && a)) {
        captureProductivity();
        if (documentFind_.empty()) {
            message("Informe o texto a localizar", true);
            return true;
        }
        auto position = documentText_.find(documentFind_);
        if (position == std::string::npos) {
            message("Texto não encontrado", true);
            return true;
        }
        if (c == Command::DocReplace) {
            auto changed = documentText_;
            std::size_t at = 0;
            while ((at = changed.find(documentFind_, at)) != std::string::npos) {
                changed.replace(at, documentFind_.size(), documentReplace_);
                at += documentReplace_.size();
                if (changed.size() > docLimit) {
                    message("Substituição excede 128 KiB", true);
                    return true;
                }
            }
            documentUndo_.push_back(documentText_);
            if (documentUndo_.size() > 8)
                documentUndo_.erase(documentUndo_.begin());
            documentText_ = std::move(changed);
            documentDirty_ = true;
        }
        productPage_ = 2;
        render();
        unsigned chars = 0;
        for (std::size_t i = 0; i < position; ++i)
            if ((static_cast<unsigned char>(documentText_[i]) & 0xc0) != 0x80)
                ++chars;
        lv_textarea_set_cursor_pos(productEditor_, chars);
        message(c == Command::DocSearch ? "Trecho localizado" : "Substituições aplicadas");
        return true;
    }
    if (c == Command::ReaderMove || c == Command::ReaderMark || c == Command::ReaderBookmark) {
        if (bookName_.empty())
            return true;
        if (c == Command::ReaderMove)
            readerPage_ =
                std::clamp<int>(int(readerPage_) + (a ? 1 : -1), 0, readingPages(bookText_) - 1);
        if (c == Command::ReaderMark) {
            auto it = std::find(readerMarks_.begin(), readerMarks_.end(), readerPage_);
            if (it != readerMarks_.end())
                readerMarks_.erase(it);
            else if (readerMarks_.size() < 16)
                readerMarks_.push_back(readerPage_);
        }
        if (c == Command::ReaderBookmark && !readerMarks_.empty()) {
            readerPage_ = readerMarks_[bookmarkIndex_ % readerMarks_.size()];
            ++bookmarkIndex_;
        }
        render();
        readerPosition(true);
        return true;
    }
    if (c == Command::GalleryMove) {
        fileEntries_.clear();
        for (const auto &f : platform_.files())
            if (ends(f.name, ".jpg") || ends(f.name, ".jpeg") || ends(f.name, ".png") ||
                ends(f.name, ".bmp"))
                fileEntries_.push_back(f);
        if (fileEntries_.empty())
            return true;
        auto it = std::find_if(fileEntries_.begin(), fileEntries_.end(),
                               [&](const auto &f) { return f.name == galleryName_; });
        int index = 0;
        if (it != fileEntries_.end())
            index =
                (std::distance(fileEntries_.begin(), it) + (a ? 1 : int(fileEntries_.size()) - 1)) %
                fileEntries_.size();
        return handleProductivity(Command::ProductFile, index);
    }
    if (c == Command::FinanceNew || c == Command::FinanceBudget) {
        captureProductivity();
        if (!financeOkay_) {
            message("Livro financeiro indisponível", true);
            return true;
        }
        productPage_ = c == Command::FinanceNew ? 7 : 8;
        render();
        return true;
    }
    if (c == Command::FinanceCancel) {
        financeDraft_.clear();
        financeDraftPage_ = 0;
        productPage_ = 5;
        render();
        return true;
    }
    if (c == Command::FinanceSave) {
        auto next = ledger_;
        LedgerEntry entry;
        entry.id = 1;
        for (const auto &e : next)
            entry.id = std::max(entry.id, e.id + 1);
        entry.title = inputValue(inputs_[0]);
        entry.day = inputValue(inputs_[1]);
        entry.account = inputValue(inputs_[2]);
        entry.currency = inputValue(inputs_[4]);
        entry.category = inputValue(inputs_[5]);
        if (!pda::parseMoney(inputValue(inputs_[3]), entry.cents) || entry.cents < 0) {
            message("Informe valor positivo com até duas casas decimais", true);
            return true;
        }
        entry.budget = productPage_ == 8;
        entry.monthly = !entry.budget && lv_dropdown_get_selected(inputs_[7]) == 1;
        if (!entry.budget && lv_dropdown_get_selected(inputs_[6]) == 0)
            entry.cents = -entry.cents;
        if (entry.budget) {
            entry.day = financeMonth_ + "-01";
            entry.title = "Orçamento";
            auto it = std::find_if(next.begin(), next.end(), [&](const auto &e) {
                return e.budget && e.day == entry.day && e.currency == entry.currency &&
                       e.category == entry.category;
            });
            if (it != next.end()) {
                entry.id = it->id;
                *it = entry;
            } else
                next.push_back(entry);
        } else
            next.push_back(entry);
        if (saveFinance(next)) {
            financeDraft_.clear();
            financeDraftPage_ = 0;
            productPage_ = 5;
            render();
            message("Lançamento financeiro confirmado no microSD");
        }
        return true;
    }
    if (c == Command::FinancePost) {
        captureProductivity();
        auto next = ledger_;
        std::string error;
        if (!postMonthly(next, financeMonth_, error)) {
            message(error, true);
            return true;
        }
        if (saveFinance(next)) {
            render();
            message("Recorrências do mês confirmadas");
        }
        return true;
    }
    if (c == Command::FinanceExport) {
        bool ok = financeOkay_ && service_.writeFile("finance-export.csv", ledgerCsv(ledger_));
        message(ok ? "Exportado: finance-export.csv" : service_.error(), !ok);
        return true;
    }
    return false;
}
} // namespace tab5_pda
