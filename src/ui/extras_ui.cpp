#include "tab_ui.hpp"
#include <algorithm>
#include <cstdio>
#include <sstream>
namespace tab5_pda {
namespace {
constexpr uint32_t navy = 0x16324f;
int noteApp(pda::RecordKind k) {
    return k == pda::RecordKind::Contact       ? 1
           : k == pda::RecordKind::Appointment ? 2
           : k == pda::RecordKind::Task        ? 3
           : k == pda::RecordKind::Note        ? 4
           : k == pda::RecordKind::Anniversary ? 5
                                               : 6;
}
bool suffix(const std::string &s, const std::string &end) {
    return s.size() >= end.size() && s.substr(s.size() - end.size()) == end;
}
} // namespace
bool Ui::shelfDraft() {
    if (mode_ == Mode::Edit)
        capture();
    auto draft = draft_.active ? draft_ : service_.state().draft;
    if (!draft.active) {
        message("Nenhum rascunho para guardar", true);
        return false;
    }
    std::string text, error;
    if (!encodeDraft(draft, service_.state(), text, error)) {
        message(error, true);
        return false;
    }
    auto name = activeShelf_.empty()
                    ? "draft-" +
                          (draft.record.id ? std::to_string(draft.record.id)
                                           : "new-" + std::to_string(service_.generation()) + "-" +
                                                 std::to_string(platform_.nowMs())) +
                          ".pda"
                    : activeShelf_;
    if (!service_.writeFile(name, text)) {
        message(service_.error(), true);
        return false;
    }
    if (service_.state().draft.active && !service_.clearDraft()) {
        message(service_.error(), true);
        return false;
    }
    draft_ = {};
    dirty_ = false;
    activeShelf_.clear();
    message("Rascunho guardado: " + name);
    return true;
}
void Ui::drawSegment(InkPoint a, InkPoint b, const InkStroke &stroke) {
    if (!inkCanvas_)
        return;
    if (a.x == b.x && a.y == b.y) {
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color = lv_color_hex(stroke.color);
        dot.bg_opa = LV_OPA_COVER;
        dot.radius = stroke.width;
        int x = std::max(0, int(a.x) - stroke.width / 2),
            y = std::max(0, int(a.y) - stroke.width / 2);
        lv_canvas_draw_rect(inkCanvas_, x, y, std::min<int>(stroke.width, 900 - x),
                            std::min<int>(stroke.width, 420 - y), &dot);
        return;
    }
    lv_point_t points[2] = {{a.x, a.y}, {b.x, b.y}};
    lv_draw_line_dsc_t style;
    lv_draw_line_dsc_init(&style);
    style.color = lv_color_hex(stroke.color);
    style.width = stroke.width;
    style.round_start = style.round_end = true;
    lv_canvas_draw_line(inkCanvas_, points, 2, &style);
}
void Ui::inkTouch(lv_event_t *event) {
    auto *ui = static_cast<Ui *>(lv_event_get_user_data(event));
    if (ui->locked_ || ui->mode_ != Mode::Drawing)
        return;
    auto code = lv_event_get_code(event);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ui->inkPressed_ = false;
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING)
        return;
    auto *input = lv_indev_get_act();
    if (!input)
        return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    lv_area_t area;
    lv_obj_get_coords(ui->inkCanvas_, &area);
    int x = point.x - area.x1, y = point.y - area.y1;
    if (x < 0 || x >= 900 || y < 0 || y >= 420) {
        ui->inkPressed_ = false;
        return;
    }
    std::size_t count = 0;
    for (const auto &s : ui->ink_)
        count += s.points.size();
    if (count >= 20000 || ui->ink_.size() >= 2048)
        return;
    ui->inkDirty_ = true;
    ui->activity();
    InkPoint p{int16_t(x), int16_t(y)};
    if (code == LV_EVENT_PRESSED || !ui->inkPressed_) {
        ui->ink_.push_back({ui->inkColor_, uint8_t(ui->inkColor_ == 0xffffff ? 14 : 3), {p}});
        ui->inkPressed_ = true;
        ui->drawSegment(p, p, ui->ink_.back());
    } else {
        auto &stroke = ui->ink_.back();
        auto last = stroke.points.back();
        if (last.x == p.x && last.y == p.y)
            return;
        stroke.points.push_back(p);
        ui->drawSegment(last, p, stroke);
    }
}
void Ui::renderInk() {
    inkPixels_.resize(900 * 420);
    inkCanvas_ = lv_canvas_create(body_);
    lv_obj_set_pos(inkCanvas_, 44, 112);
    lv_canvas_set_buffer(inkCanvas_, inkPixels_.data(), 900, 420, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(inkCanvas_, lv_color_white(), LV_OPA_COVER);
    lv_obj_add_flag(inkCanvas_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(inkCanvas_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(inkCanvas_, inkTouch, LV_EVENT_ALL, this);
    for (const auto &stroke : ink_)
        for (std::size_t i = 0; i < stroke.points.size(); ++i)
            drawSegment(stroke.points[i ? i - 1 : 0], stroke.points[i], stroke);
}
void Ui::renderExtras() {
    if (mode_ == Mode::Drawing) {
        button(body_, 12, 8, 150, 46, "Salvar desenho", Command::DrawSave);
        button(body_, 174, 8, 150, 46, "Desfazer traço", Command::DrawUndo);
        button(body_, 336, 8, 150, 46, "Limpar tela", Command::DrawClear);
        button(body_, 498, 8, 150, 46, "Preto", Command::DrawColor, 0);
        button(body_, 660, 8, 150, 46, "Azul", Command::DrawColor, 0x2563eb);
        button(body_, 822, 8, 162, 46, "Borracha", Command::DrawColor, 0xffffff);
        label(body_, 12, 70, 960,
              "Desenho livre — " +
                  (attachmentId_ ? "anexo de #" + std::to_string(attachmentId_)
                                 : "arquivo independente") +
                  ". Salvar grava no microSD.",
              navy);
        renderInk();
        button(body_, 12, 554, 220, 50, "Voltar às ferramentas", Command::Extras);
        return;
    }
    if (mode_ == Mode::FileDelete) {
        label(body_, 12, 28, 960, "Apagar arquivo do microSD? " + fileName_, 0xa52834);
        label(body_, 12, 90, 960, "O registro vinculado permanece; o anexo deixará de abrir.");
        button(body_, 12, 180, 300, 56, "Confirmar exclusão do arquivo",
               Command::FileRemoveConfirm);
        button(body_, 330, 180, 240, 56, "Cancelar", Command::Files);
        return;
    }
    if (mode_ == Mode::FileView) {
        label(body_, 12, 18, 960, fileName_, navy);
        button(body_, 12, 62, 220, 46, "Voltar aos arquivos", Command::Files);
        button(body_, 244, 62, 220, 46, "Apagar arquivo", Command::FileRemove);
        button(body_, 476, 62, 240, 46, "Usar na sincronização", Command::SyncPreview, 1);
        auto *view = panel(body_, 12, 128, 976, 478);
        lv_obj_add_flag(view, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(view, LV_DIR_VER);
        if (suffix(fileName_, ".csv")) {
            std::vector<std::vector<std::string>> rows;
            std::string error;
            if (previewTable(fileText_, rows, error)) {
                lv_obj_set_scroll_dir(view, LV_DIR_ALL);
                label(view, 12, 8, 940, "Prévia: até 100 linhas de dados e 12 colunas.");
                auto *table = lv_table_create(view);
                lv_obj_set_pos(table, 12, 44);
                std::size_t columns = 0;
                for (const auto &row : rows)
                    columns = std::max(columns, row.size());
                lv_table_set_col_cnt(table, columns);
                lv_table_set_row_cnt(table, rows.size());
                lv_obj_set_style_text_font(table, &pda_font_16, 0);
                for (std::size_t c = 0; c < columns; ++c)
                    lv_table_set_col_width(table, c, std::max(150, 920 / int(columns)));
                for (std::size_t r = 0; r < rows.size(); ++r)
                    for (std::size_t c = 0; c < rows[r].size(); ++c)
                        lv_table_set_cell_value(table, r, c, rows[r][c].c_str());
            } else
                label(view, 12, 12, 940, error + "\n" + fileText_.substr(0, 32768));
        } else
            label(view, 12, 12, 940, fileText_.substr(0, 32768));
        return;
    }
    if (mode_ == Mode::SyncPreview) {
        label(body_, 12, 18, 960,
              "Sincronização: " + std::to_string(syncPlan_.conflicts.size()) +
                  " conflitos pendentes",
              navy);
        auto *list = panel(body_, 12, 68, 976, 420);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        int y = 8;
        for (std::size_t i = 0; i < std::min<std::size_t>(syncPlan_.conflicts.size(), 32); ++i) {
            auto &c = syncPlan_.conflicts[i];
            auto *local = pda::findRecord(service_.state(), c.id);
            label(list, 12, y, 940,
                  "#" + std::to_string(c.id) + " Local: " + (local ? local->title : "ausente") +
                      " | Recebido: " + (c.deletion ? "exclusão" : c.remote.title));
            button(list, 12, y + 34, 420, 46, "Manter local", Command::SyncKeep, i);
            button(list, 446, y + 34, 420, 46, "Usar recebido", Command::SyncRemote, i);
            y += 104;
        }
        if (syncPlan_.conflicts.empty())
            label(list, 12, 12, 940,
                  "Nenhum conflito pendente. Aplicar preserva as configurações locais e grava os "
                  "registros em uma transação.");
        button(body_, 12, 520, 300, 56, "Aplicar sincronização", Command::SyncApply);
        button(body_, 328, 520, 240, 56, "Cancelar sincronização", Command::Extras, 7);
        return;
    }
    button(body_, 12, 8, 220, 46, "Ferramentas", Command::Extras);
    button(body_, 244, 8, 220, 46, "Voltar ao PDA", Command::Cancel);
    if (extrasPage_ == 0) {
        label(body_, 12, 78, 960, "Cadernos, anexos e conexão com o computador", navy);
        const char *names[] = {
            "Cadernos e rascunhos", "Gravador de voz",     "Desenho livre", "Arquivos do microSD",
            "Central de lembretes", "Transferência Wi-Fi", "Sincronização", "Assistente opcional"};
        const Command commands[] = {Command::Notebook, Command::Extras,    Command::Draw,
                                    Command::Files,    Command::Reminders, Command::Share,
                                    Command::Extras,   Command::Assistant};
        const uint32_t args[] = {UINT32_MAX, 2, 0, 0, 0, 0, 7, 0};
        button(body_, 12, 500, 952, 56, "Planilha, documentos, livros, galeria e finanças",
               Command::Productivity);
        for (int i = 0; i < 8; ++i)
            button(body_, 12 + i % 3 * 326, 124 + i / 3 * 122, 310, 96, names[i], commands[i],
                   args[i]);
        return;
    }
    if (extrasPage_ == 1) {
        button(body_, 12, 70, 180, 46, "Todas as notas", Command::Notebook, 0);
        button(body_, 204, 70, 180, 46, "Favoritas", Command::Notebook, 1);
        button(body_, 396, 70, 180, 46, "Modelo: reunião", Command::Template, 0);
        button(body_, 588, 70, 180, 46, "Modelo: lista", Command::Template, 1);
        button(body_, 780, 70, 204, 46, "Modelo: diário", Command::Template, 2);
        notebooks_ = {"", ""};
        std::set<std::string> folders;
        for (const auto &r : service_.state().records)
            if (!pda::trashed(r) && r.kind == pda::RecordKind::Note) {
                auto f = pda::value(r.fields, "notebook");
                if (!f.empty())
                    folders.insert(f);
            }
        for (const auto &f : folders)
            notebooks_.push_back(f);
        fileEntries_.clear();
        for (const auto &f : platform_.files())
            if (f.name.rfind("draft-", 0) == 0 && suffix(f.name, ".pda"))
                fileEntries_.push_back(f);
        auto *list = panel(body_, 12, 134, 976, 470);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        int y = 8;
        label(list, 12, y, 940, "Cadernos:");
        y += 30;
        const auto total = std::max(fileEntries_.size(), notebooks_.size() - 2);
        filesPage_ = std::clamp(filesPage_, 0, total ? int((total - 1) / 16) : 0);
        for (std::size_t i = 2 + filesPage_ * 16;
             i < std::min(notebooks_.size(), std::size_t(2 + filesPage_ * 16 + 16)); ++i) {
            button(list, 12, y, 920, 46, notebooks_[i], Command::Notebook, i);
            y += 54;
        }
        label(list, 12, y, 940, "Rascunhos independentes no microSD:");
        y += 34;

        button(list, 12, y, 220, 44, "Página -", Command::FileNext, 0);
        button(list, 244, y, 220, 44, "Página +", Command::FileNext, 1);
        y += 54;
        for (std::size_t i = filesPage_ * 16;
             i < std::min(fileEntries_.size(), std::size_t(filesPage_ * 16 + 16)); ++i) {
            std::string data, error;
            pda::Draft draft;
            std::string title = "Rascunho indisponível";
            if (service_.readFile(fileEntries_[i].name, data) &&
                decodeDraft(data, service_.state(), draft, error))
                title = draft.record.title.empty() ? "Rascunho sem título" : draft.record.title;
            button(list, 12, y, 690, 46, title, Command::ResumeShelf, i);
            button(list, 714, y, 218, 46, "Excluir rascunho", Command::DeleteShelf, i);
            y += 54;
        }
        return;
    }
    if (extrasPage_ == 2) {
        label(body_, 12, 86, 960,
              "WAV PCM mono 16 kHz — limite de cinco minutos. O arquivo fica no microSD.");
        auto *linked = pda::findRecord(service_.state(), attachmentId_);
        label(body_, 12, 130, 960, linked ? "Vinculado: " + linked->title : "Gravação avulsa");
        button(body_, 12, 198, 280, 64, "Iniciar gravação", Command::AudioRecord);
        button(body_, 308, 198, 280, 64, "Parar e salvar áudio", Command::AudioStop);
        button(body_, 604, 198, 380, 64, "Reproduzir anexo", Command::AudioPlay);
        button(body_, 12, 280, 460, 56, "Vincular arquivo atual", Command::FileAttach);
        button(body_, 488, 280, 496, 56, "Reproduzir arquivo atual", Command::AudioPlay, 1);
        label(body_, 12, 356, 960,
              suffix(fileName_, ".wav")
                  ? fileName_
                  : "Selecione um WAV em Arquivos para vinculá-lo a uma nota ou compromisso.");
        extrasLive_ = label(body_, 12, 412, 960, platform_.audioStatus(), navy);
        return;
    }
    if (extrasPage_ == 4) {
        label(body_, 12, 78, 960,
              "Arquivos em /Tab5PDA — toque para abrir. WAV e desenhos usam visualizadores "
              "próprios.");
        button(body_, 12, 118, 220, 44, "Página -", Command::FileNext, 0);
        button(body_, 244, 118, 220, 44, "Página +", Command::FileNext, 1);
        fileEntries_.clear();
        for (const auto &f : platform_.files())
            if (publicFile(f.name))
                fileEntries_.push_back(f);
        filesPage_ = std::clamp(filesPage_, 0,
                                fileEntries_.empty() ? 0 : int((fileEntries_.size() - 1) / 16));
        auto *list = panel(body_, 12, 178, 976, 426);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        int y = 8;
        for (std::size_t i = filesPage_ * 16;
             i < std::min(fileEntries_.size(), std::size_t(filesPage_ * 16 + 16)); ++i) {
            button(list, 12, y, 940, 50,
                   fileEntries_[i].name + " — " + std::to_string(fileEntries_[i].bytes) + " bytes",
                   Command::FileOpen, i);
            y += 58;
        }
        return;
    }
    if (extrasPage_ == 5) {
        const char *captions[] = {"Adiamento (min)", "Silêncio de (HH:MM)", "Até (HH:MM)",
                                  "Som do lembrete"};
        auto start = pda::value(service_.state().settings.fields, "quietStart"),
             end = pda::value(service_.state().settings.fields, "quietEnd");
        auto time = [](const std::string &s) {
            if (s.empty())
                return std::string{};
            char text[8];
            auto n = std::stoi(s);
            std::snprintf(text, sizeof(text), "%02d:%02d", n / 60, n % 60);
            return std::string(text);
        };
        const std::string values[] = {
            pda::value(service_.state().settings.fields, "snooze", "5"), time(start), time(end),
            pda::value(service_.state().settings.fields, "alarmTone", "1200")};
        for (int i = 0; i < 4; ++i) {
            label(body_, 12 + i * 244, 76, 228, captions[i]);
            if (i == 3) {
                auto tone = std::stoi(values[i]);
                unsigned selected = tone == 0 ? 0 : tone == 800 ? 1 : tone == 1600 ? 3 : 2;
                auto *control = dropdown(body_, 12 + i * 244, 112,
                                         "Silencioso\nSuave\nPadrão\nAgudo", selected);
                lv_obj_set_width(control, 228);
                lv_obj_set_height(control, 54);
                inputs_.push_back(control);
            } else
                inputs_.push_back(input(body_, 12 + i * 244, 112, 228, 54, values[i], 8));
            inputLimits_.push_back(8);
            inputHeights_.push_back(54);
        }
        button(body_, 12, 184, 300, 46, "Salvar preferências de aviso", Command::ReminderSave);
        reminderEntries_ = pda::dueAlarms(service_.state(), platform_.localMinute());
        label(body_, 12, 252, 960,
              "Lembretes pendentes: " + std::to_string(reminderEntries_.size()) +
                  ". Silêncio afeta o som; o aviso visual continua.",
              navy);
        auto *list = panel(body_, 12, 294, 976, 310);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        for (std::size_t i = 0; i < std::min<std::size_t>(64, reminderEntries_.size()); ++i) {
            auto *r = pda::findRecord(service_.state(), reminderEntries_[i].id);
            button(list, 12, 8 + i * 58, 940, 50,
                   (r ? r->title : "Registro removido") + " — " + reminderEntries_[i].occurrence,
                   Command::ReminderOpen, i);
        }
        editor_ = inputs_.front();
        return;
    }
    if (extrasPage_ == 6) {
        label(
            body_, 12, 80, 960,
            "Sessão de transferência pelo navegador. Conecte o Tab5 e o computador à mesma rede.");
        button(body_, 12, 150, 420, 64, sharing_ ? "Encerrar sessão Wi-Fi" : "Abrir sessão Wi-Fi",
               Command::Share, 1);
        label(body_, 12, 252, 960, extrasStatus_, navy);
        label(body_, 12, 340, 960,
              "O navegador pede o código mostrado acima. Recebidos ficam em incoming-... e não "
              "substituem a agenda.\nImporte arquivos na área de Backup ou revise a sincronização "
              "no aparelho.\nA sessão é encerrada ao bloquear o PDA. Conexão HTTP indicada para "
              "rede local confiável.");
        return;
    }
    if (extrasPage_ == 7) {
        label(body_, 12, 78, 960, "Arquivo de sincronização recebido (na pasta /Tab5PDA)");
        editor_ = input(body_, 12, 112, 952, 56, syncFile_, 80);
        inputs_.push_back(editor_);
        inputLimits_.push_back(80);
        inputHeights_.push_back(56);
        button(body_, 12, 202, 420, 60, "Preparar prévia da sincronização", Command::SyncPreview);
        label(body_, 12, 304, 960,
              "Compare o arquivo recebido com o PDA e a última base sincronizada. Mudanças "
              "independentes são combinadas.\nAlterações concorrentes e colisões de ID pedem "
              "escolha explícita; nenhuma é sobrescrita automaticamente.\nAntes de sincronizar, "
              "salve ou guarde o rascunho ativo. Anexos são transferidos separadamente.\nA "
              "exportação para o computador é sync-export.jsonl, criada ao abrir a sessão Wi-Fi.");
        return;
    }
    if (extrasPage_ == 8) {
        label(body_, 12, 70, 960, "Servidor local do assistente (http://...:8787/assistant)");
        inputs_.push_back(input(body_, 12, 102, 660, 50, assistantURL_, 180));
        inputLimits_.push_back(180);
        inputHeights_.push_back(50);
        inputs_.push_back(input(body_, 688, 102, 276, 50, assistantToken_, 128, false, true));
        inputLimits_.push_back(128);
        inputHeights_.push_back(50);
        label(body_, 12, 170, 960,
              "Pedido — somente este texto e a hora local são enviados ao servidor configurado");
        inputs_.push_back(input(body_, 12, 204, 952, 230, assistantPrompt_, 4096, true));
        inputLimits_.push_back(4096);
        inputHeights_.push_back(230);
        button(body_, 12, 458, 280, 56, "Salvar servidor e código", Command::AssistantSave);
        button(body_, 308, 458, 280, 56, "Gerar prévia", Command::AssistantAsk);
        extrasLive_ = label(body_, 12, 542, 960, extrasStatus_, navy);
        editor_ = inputs_.back();
        field_ = inputs_.size() - 1;
        return;
    }
}
bool Ui::handleExtras(Command c, uint32_t a) {
    if (mode_ == Mode::Drawing && inkDirty_ &&
        (c == Command::Extras || c == Command::App || c == Command::Cancel)) {
        handleExtras(Command::DrawSave, 0);
        if (inkDirty_)
            return true;
    }
    if (mode_ == Mode::Extras && extrasPage_ == 8 && inputs_.size() == 3 &&
        (c == Command::App || c == Command::Cancel || c == Command::Extras)) {
        assistantURL_ = inputValue(inputs_[0]);
        assistantToken_ = inputValue(inputs_[1]);
        assistantPrompt_ = inputValue(inputs_[2]);
    }
    if (c == Command::Extras) {
        if (mode_ == Mode::Edit) {
            capture();
            persistDraft();
        }
        if (selectedRecord() && (selectedRecord()->kind == pda::RecordKind::Note ||
                                 selectedRecord()->kind == pda::RecordKind::Appointment))
            attachmentId_ = selected_;
        else
            attachmentId_ = 0;
        app_ = 12;
        mode_ = Mode::Extras;
        extrasPage_ = a;
        usbEnabled_ = false;
        render();
        return true;
    }
    if (c == Command::Notebook) {
        if (a != UINT32_MAX && a < notebooks_.size()) {
            favoritesOnly_ = a == 1;
            notebookFilter_ = a >= 2 ? notebooks_[a] : "";
            app_ = 4;
            mode_ = Mode::Normal;
            listPage_ = 0;
            render();
        } else {
            mode_ = Mode::Extras;
            extrasPage_ = 1;
            filesPage_ = 0;
            render();
        }
        return true;
    }
    if (c == Command::Favorite) {
        auto *r = pda::findRecord(service_.state(), a);
        if (!r)
            return true;
        auto value = pda::value(r->fields, "favorite") == "1" ? "0" : "1";
        std::string error;
        bool ok = patchRecord(service_, a, "favorite", value, error);
        if (ok) {
            if (draft_.record.id == a)
                draft_.record.fields["favorite"] = value;
            render();
        }
        message(ok ? "Favorita atualizada" : error, !ok);
        return true;
    }
    if (c == Command::Template) {
        template_ = a == 1 ? 1 : a;
        app_ = 4;
        dailyFilter_.clear();
        startEdit();
        return true;
    }
    if (c == Command::Shelf || c == Command::ShelfContinue) {
        auto nextApp = pendingApp_;
        auto pending = pendingRecord_;
        bool existing = pendingExisting_;
        if (!shelfDraft())
            return true;
        if (c == Command::ShelfContinue) {
            pendingCreate_ = false;
            app_ = nextApp;
            startEdit(existing ? &pending : nullptr);
        } else {
            mode_ = Mode::Extras;
            extrasPage_ = 1;
            render();
        }
        return true;
    }
    if (c == Command::ResumeShelf && a < fileEntries_.size()) {
        if (draft_.active || service_.state().draft.active) {
            if (!shelfDraft())
                return true;
        }
        std::string data, error;
        pda::Draft draft;
        if (!service_.readFile(fileEntries_[a].name, data) ||
            !decodeDraft(data, service_.state(), draft, error)) {
            message(error.empty() ? service_.error() : error, true);
            return true;
        }
        activeShelf_ = fileEntries_[a].name;
        draft_ = draft;
        dirty_ = true;
        app_ = noteApp(draft.record.kind);
        field_ = draft.field;
        mode_ = Mode::Edit;
        lastEdit_ = platform_.nowMs();
        render();
        return true;
    }
    if (c == Command::DeleteShelf && a < fileEntries_.size()) {
        fileName_ = fileEntries_[a].name;
        mode_ = Mode::FileDelete;
        render();
        return true;
    }
    if (c == Command::Draw) {
        attachmentId_ = a;
        ink_.clear();
        inkDirty_ = false;
        fileName_.clear();
        if (auto *r = pda::findRecord(service_.state(), a)) {
            fileName_ = pda::value(r->fields, "drawing");
            if (!fileName_.empty()) {
                std::string text, error;
                if (!service_.readFile(fileName_, text) || !decodeInk(text, ink_, error)) {
                    message(error.empty() ? service_.error() : error, true);
                    return true;
                }
            }
        }
        mode_ = Mode::Drawing;
        inkPressed_ = false;
        render();
        return true;
    }
    if (c == Command::DrawUndo || c == Command::DrawClear || c == Command::DrawColor) {
        if (c == Command::DrawUndo && !ink_.empty())
            ink_.pop_back();
        if (c == Command::DrawClear)
            ink_.clear();
        if (c != Command::DrawColor)
            inkDirty_ = true;
        if (c == Command::DrawColor)
            inkColor_ = a;
        render();
        return true;
    }
    if (c == Command::DrawSave) {
        auto name = "drawing-" + std::to_string(attachmentId_) + "-" +
                    platform_.randomSalt().substr(0, 8) + "-" + std::to_string(platform_.nowMs()) +
                    ".draw";
        if (!service_.writeFile(name, encodeInk(ink_))) {
            message(service_.error(), true);
            return true;
        }
        std::string error;
        bool ok = true;
        if (attachmentId_)
            ok = patchRecord(service_, attachmentId_, "drawing", name, error);
        if (ok) {
            inkDirty_ = false;
            fileName_ = name;
            if (draft_.record.id == attachmentId_)
                draft_.record.fields["drawing"] = name;
        }
        message(ok ? "Desenho salvo: " + name : "Arquivo salvo; vínculo falhou: " + error, !ok);
        return true;
    }
    if (c == Command::AudioRecord) {
        if (platform_.audioStatus().rfind("Gravando:", 0) == 0) {
            message("Pare a gravação atual antes de iniciar outra", true);
            return true;
        }
        audioAttachment_ = attachmentId_;
        auto name = "audio-" + std::to_string(attachmentId_) + "-" +
                    std::to_string(platform_.nowMs()) + ".wav";
        bool ok = platform_.audioRecord(name);
        if (ok)
            fileName_ = name;
        message(ok ? "Gravação iniciada" : platform_.audioStatus(), !ok);
        return true;
    }
    if (c == Command::AudioStop) {
        platform_.audioStop();
        message(platform_.audioStatus());
        return true;
    }
    if (c == Command::AudioPlay) {
        auto *r = pda::findRecord(service_.state(), attachmentId_);
        auto name = a ? fileName_ : r ? pda::value(r->fields, "audio") : fileName_;
        bool ok = !name.empty() && platform_.audioPlay(name);
        message(ok ? "Reproduzindo áudio" : platform_.audioStatus(), !ok);
        return true;
    }
    if (c == Command::Files) {
        mode_ = Mode::Extras;
        extrasPage_ = 4;
        filesPage_ = 0;
        render();
        return true;
    }
    if (c == Command::FileNext) {
        filesPage_ = std::max(0, filesPage_ + (a ? 1 : -1));
        render();
        return true;
    }
    if (c == Command::FileOpen && a < fileEntries_.size()) {
        fileName_ = fileEntries_[a].name;
        if (suffix(fileName_, ".jpg") || suffix(fileName_, ".jpeg") || suffix(fileName_, ".png") ||
            suffix(fileName_, ".bmp")) {
            if (openPicture(fileName_)) {
                mode_ = Mode::Productivity;
                productPage_ = 4;
                productListing_ = false;
                render();
                message("Imagem aberta do microSD");
            }
            return true;
        }
        if (suffix(fileName_, ".wav")) {
            platform_.audioPlay(fileName_);
            mode_ = Mode::Extras;
            extrasPage_ = 2;
            render();
            return true;
        }
        if (!service_.readFile(fileName_, fileText_)) {
            message(service_.error(), true);
            return true;
        }
        if (suffix(fileName_, ".draw")) {
            std::string error;
            if (!decodeInk(fileText_, ink_, error)) {
                message(error, true);
                return true;
            }
            inkDirty_ = false;
            mode_ = Mode::Drawing;
            render();
            return true;
        }
        mode_ = Mode::FileView;
        render();
        return true;
    }
    if (c == Command::FileAttach) {
        if (!attachmentId_ || (!suffix(fileName_, ".wav") && !suffix(fileName_, ".draw"))) {
            message("Abra as ferramentas a partir da nota ou compromisso de destino e selecione um "
                    "anexo",
                    true);
            return true;
        }
        auto files = platform_.files();
        if (std::none_of(files.begin(), files.end(),
                         [&](const auto &f) { return f.name == fileName_; })) {
            message("Arquivo de anexo indisponível", true);
            return true;
        }
        std::string error;
        auto key = suffix(fileName_, ".wav") ? "audio" : "drawing";
        bool ok = patchRecord(service_, attachmentId_, key, fileName_, error);
        if (ok && draft_.record.id == attachmentId_)
            draft_.record.fields[key] = fileName_;
        message(ok ? "Anexo vinculado ao registro" : error, !ok);
        return true;
    }
    if (c == Command::FileRemove) {
        mode_ = Mode::FileDelete;
        render();
        return true;
    }
    if (c == Command::FileRemoveConfirm) {
        platform_.audioStop();
        bool allowed = publicFile(fileName_) || fileName_.rfind("draft-", 0) == 0;
        bool ok = allowed && platform_.removeFile(fileName_);
        if (ok) {
            mode_ = Mode::Extras;
            extrasPage_ = 4;
            render();
        }
        message(ok ? "Arquivo removido" : "Não foi possível remover o arquivo", !ok);
        return true;
    }
    if (c == Command::Reminders) {
        mode_ = Mode::Extras;
        extrasPage_ = 5;
        render();
        return true;
    }
    if (c == Command::ReminderOpen && a < reminderEntries_.size()) {
        alarm_ = reminderEntries_[a];
        beforeAlarm_ = Mode::Extras;
        mode_ = Mode::Alarm;
        render();
        return true;
    }
    if (c == Command::ReminderSave) {
        if (mode_ != Mode::Extras || extrasPage_ != 5 || inputs_.size() != 4)
            return true;
        auto settings = service_.state().settings;
        settings.fields["snooze"] = inputValue(inputs_[0]);
        const int tones[] = {0, 800, 1200, 1600};
        settings.fields["alarmTone"] = std::to_string(tones[lv_dropdown_get_selected(inputs_[3])]);
        if (inputValue(inputs_[1]).empty() != inputValue(inputs_[2]).empty()) {
            message("Informe os dois horários ou deixe ambos vazios", true);
            return true;
        }
        for (int i = 1; i <= 2; ++i) {
            auto value = inputValue(inputs_[i]);
            auto key = i == 1 ? "quietStart" : "quietEnd";
            if (value.empty())
                settings.fields.erase(key);
            else {
                pda::CivilMinute t;
                if (value.size() != 5 || !pda::parseCivilMinute("2000-01-01 " + value, t)) {
                    message("Horário silencioso: HH:MM ou vazio", true);
                    return true;
                }
                settings.fields[key] = std::to_string(t.hour * 60 + t.minute);
            }
        }
        bool ok = service_.saveSettings(settings);
        message(ok ? "Preferências de aviso salvas" : service_.error(), !ok);
        return true;
    }
    if (c == Command::Share) {
        mode_ = Mode::Extras;
        extrasPage_ = 6;
        usbEnabled_ = false;
        if (a) {
            sharing_ = !sharing_;
            if (sharing_) {
                std::string error;
                auto text = syncExport(service_.state(), error);
                if (!error.empty() || !service_.writeFile("sync-export.jsonl", text)) {
                    sharing_ = false;
                    message(error.empty() ? service_.error() : error, true);
                    return true;
                }
            }
            extrasStatus_ = platform_.transfer(sharing_);
            if (extrasStatus_.rfind("http://", 0) != 0)
                sharing_ = false;
        }
        render();
        return true;
    }
    if (c == Command::SyncPreview) {
        if (draft_.active || service_.state().draft.active) {
            message("Salve ou guarde o rascunho ativo antes de sincronizar", true);
            return true;
        }
        if (a)
            syncFile_ = fileName_;
        else if (editor_)
            syncFile_ = inputValue(editor_);
        std::string text, error;
        pda::State remote, base;
        if (!service_.readFile(syncFile_, text)) {
            message(service_.error(), true);
            return true;
        }
        if (!pda::importJsonl(text, remote, error)) {
            message(error, true);
            return true;
        }
        auto files = platform_.files();
        bool exists = std::any_of(files.begin(), files.end(),
                                  [](const auto &f) { return f.name == "sync-base.jsonl"; });
        if (exists &&
            (!service_.readFile("sync-base.jsonl", text) || !pda::importJsonl(text, base, error))) {
            message(error.empty() ? service_.error() : error, true);
            return true;
        }
        if (!planSync(service_.state(), remote, base, syncPlan_, error)) {
            message(error, true);
            return true;
        }
        syncGeneration_ = service_.generation();
        mode_ = Mode::SyncPreview;
        render();
        return true;
    }
    if (c == Command::SyncKeep || c == Command::SyncRemote) {
        std::string error;
        resolveSync(syncPlan_, a, c == Command::SyncRemote, error);
        render();
        return true;
    }
    if (c == Command::SyncApply) {
        if (mode_ != Mode::SyncPreview || syncGeneration_ != service_.generation()) {
            message("A base mudou após a prévia; prepare a sincronização novamente", true);
            return true;
        }
        if (!syncPlan_.conflicts.empty()) {
            message("Resolva todos os conflitos primeiro", true);
            return true;
        }
        if (!service_.restore(syncPlan_.candidate)) {
            message(service_.error(), true);
            return true;
        }
        std::string error;
        auto baseline = syncExport(service_.state(), error);
        bool ok = error.empty() && service_.writeFile("sync-base.jsonl", baseline) &&
                  service_.writeFile("sync-export.jsonl", baseline);
        mode_ = Mode::Extras;
        extrasPage_ = 7;
        render();
        message(ok ? "Sincronização aplicada no microSD"
                   : "Registros salvos; falha ao atualizar base de sincronização: " +
                         service_.error(),
                !ok);
        return true;
    }
    if (c == Command::Assistant) {
        if (assistantURL_.empty()) {
            std::string config;
            if (service_.readFile("assistant.cfg", config)) {
                std::istringstream in(config);
                std::getline(in, assistantURL_);
                std::getline(in, assistantToken_);
            }
        }
        mode_ = Mode::Extras;
        extrasPage_ = 8;
        render();
        return true;
    }
    if (c == Command::AssistantAsk || c == Command::AssistantSave) {
        if (mode_ != Mode::Extras || extrasPage_ != 8 || inputs_.size() != 3)
            return true;
        assistantURL_ = inputValue(inputs_[0]);
        assistantToken_ = inputValue(inputs_[1]);
        assistantPrompt_ = inputValue(inputs_[2]);
        if (assistantURL_.rfind("http://", 0) != 0 || assistantToken_.empty() ||
            assistantToken_.find('\n') != std::string::npos) {
            message("Informe URL HTTP do servidor local e código de acesso", true);
            return true;
        }
        if (c == Command::AssistantSave) {
            bool ok =
                service_.writeFile("assistant.cfg", assistantURL_ + "\n" + assistantToken_ + "\n");
            message(ok ? "Servidor salvo no microSD" : service_.error(), !ok);
        } else {
            bool ok = platform_.askAssistant(assistantURL_, assistantToken_, assistantPrompt_);
            extrasStatus_ = ok ? "Aguardando o servidor; nenhum registro foi alterado"
                               : "Não foi possível consultar. Confira Wi-Fi, URL e servidor.";
            if (extrasLive_)
                lv_label_set_text(extrasLive_, extrasStatus_.c_str());
        }
        return true;
    }
    return false;
}
void Ui::tickExtras() {
    if (sharing_ && !platform_.transferActive()) {
        sharing_ = false;
        extrasStatus_ = "Sessão encerrada; abra uma nova após conectar ao Wi-Fi";
    }

    std::string name;
    if (platform_.audioFinished(name)) {
        std::string error;
        if (audioAttachment_) {
            bool ok = patchRecord(service_, audioAttachment_, "audio", name, error);
            if (ok && draft_.record.id == audioAttachment_)
                draft_.record.fields["audio"] = name;
            message(ok ? "Áudio vinculado: " + name : "Áudio salvo; vínculo falhou: " + error, !ok);
        }
    }
    if (extrasLive_ && mode_ == Mode::Extras && extrasPage_ == 2)
        lv_label_set_text(extrasLive_, platform_.audioStatus().c_str());
    if (!locked_ && mode_ == Mode::Extras && extrasPage_ == 8) {
        std::string text, error;
        if (platform_.assistantResult(text, error)) {
            if (!error.empty()) {
                message(error, true);
                return;
            }
            pda::State proposed;
            if (!pda::importJsonl(text, proposed, error)) {
                message("Prévia do assistente inválida: " + error, true);
                return;
            }
            bool allowed = proposed.records.size() <= 16 && !proposed.records.empty();
            for (const auto &r : proposed.records)
                if ((r.kind != pda::RecordKind::Note && r.kind != pda::RecordKind::Task &&
                     r.kind != pda::RecordKind::Appointment &&
                     r.kind != pda::RecordKind::Expense) ||
                    r.fields.count("audio") || r.fields.count("drawing") ||
                    r.fields.count("parent") || r.fields.count("contact") || pda::trashed(r))
                    allowed = false;
            if (!allowed) {
                message("O assistente propôs campos ou operações não permitidos", true);
                return;
            }
            pendingImports_ = proposed.records;
            for (auto &r : pendingImports_) {
                r.id = 0;
                r.fields.erase("parent");
                r.fields.erase("contact");
                r.fields.erase("trashed");
                r.fields.erase("trashRoot");
            }
            exchangeFile_ = "assistente (prévia)";
            mode_ = Mode::ImportPreview;
            render();
        }
    }
}
} // namespace tab5_pda
