#include "tab_ui.hpp"
#include <algorithm>
#include <cstdio>
namespace tab5_pda {
namespace {
constexpr uint32_t navy = 0x16324f, red = 0xa52834;
int appFor(pda::RecordKind k) {
    return k == pda::RecordKind::Contact       ? 1
           : k == pda::RecordKind::Appointment ? 2
           : k == pda::RecordKind::Task        ? 3
           : k == pda::RecordKind::Note        ? 4
           : k == pda::RecordKind::Anniversary ? 5
                                               : 6;
}
std::string addDays(const std::string &day, int days) {
    pda::CivilMinute t;
    if (!pda::parseCivilMinute(day + " 00:00", t))
        return {};
    auto result =
        pda::formatCivilMinute(pda::unixToCivil(pda::civilToUnix(t, 0) + days * 86400LL, 0));
    pda::CivilMinute checked;
    return pda::parseCivilMinute(result, checked) ? result.substr(0, 10) : day;
}
std::string localDue(const pda::Record &r, const pda::Settings &s) {
    pda::CivilMinute t;
    if (!pda::parseCivilMinute(r.when, t))
        return {};
    auto offset = std::stoi(pda::value(r.fields, "offset", std::to_string(s.utcOffsetMinutes)));
    return pda::formatCivilMinute(
        pda::unixToCivil(pda::civilToUnix(t, offset), s.utcOffsetMinutes));
}
} // namespace
void Ui::renderToday() {
    auto now = platform_.localMinute();
    label(body_, 12, 12, 960,
          now.empty() ? "Ajuste a hora para ver prazos e compromissos"
                      : pda::displayMinute(now, service_.state().settings),
          navy);
    button(body_, 12, 52, 226, 46, "Novo compromisso", Command::QuickNew, 2);
    button(body_, 250, 52, 226, 46, "Nova tarefa", Command::QuickNew, 3);
    button(body_, 488, 52, 226, 46, "Nova nota", Command::QuickNew, 4);
    button(body_, 726, 52, 258, 46, "Agenda semanal", Command::ScheduleMode, 2);
    auto *events = panel(body_, 12, 112, 472, 428);
    auto *tasks = panel(body_, 496, 112, 488, 428);
    lv_obj_add_flag(events, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(events, LV_DIR_VER);
    lv_obj_add_flag(tasks, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tasks, LV_DIR_VER);
    label(events, 10, 8, 450, "Próximos compromissos", navy);
    std::vector<std::pair<std::string, uint32_t>> upcoming;
    std::vector<const pda::Record *> pending;
    for (const auto &r : service_.state().records) {
        if (pda::trashed(r))
            continue;
        if (r.kind == pda::RecordKind::Task && !r.completed)
            pending.push_back(&r);
        if (r.kind != pda::RecordKind::Appointment && r.kind != pda::RecordKind::Anniversary)
            continue;
        auto from = now;
        bool civil =
            r.kind == pda::RecordKind::Anniversary || pda::value(r.fields, "allDay") == "1";
        int offset = std::stoi(pda::value(
            r.fields, "offset", std::to_string(service_.state().settings.utcOffsetMinutes)));
        pda::CivilMinute t;
        if (!civil && pda::parseCivilMinute(from, t))
            from = pda::formatCivilMinute(pda::unixToCivil(
                pda::civilToUnix(t, service_.state().settings.utcOffsetMinutes), offset));
        auto next = pda::nextOccurrence(r, from);
        if (!civil && pda::parseCivilMinute(next, t))
            next = localDue(pda::Record{r.id, r.kind, r.title, "", next, false, r.fields},
                            service_.state().settings);
        if (!next.empty())
            upcoming.push_back({next, r.id});
    }
    std::sort(upcoming.begin(), upcoming.end());
    for (std::size_t i = 0; i < std::min<std::size_t>(12, upcoming.size()); ++i) {
        auto *r = pda::findRecord(service_.state(), upcoming[i].second);
        button(events, 8, 40 + i * 72, 450, 64,
               pda::displayMinute(upcoming[i].first, service_.state().settings) + "\n" + r->title,
               Command::OpenRecord, r->id);
    }
    if (upcoming.empty())
        label(events, 10, 54, 450, "Nenhum compromisso próximo.");
    std::sort(pending.begin(), pending.end(), [&](auto *a, auto *b) {
        auto da = localDue(*a, service_.state().settings),
             db = localDue(*b, service_.state().settings);
        bool lateA = !now.empty() && !da.empty() && da < now,
             lateB = !now.empty() && !db.empty() && db < now;
        if (lateA != lateB)
            return lateA;
        auto pa = pda::value(a->fields, "priority", "2"),
             pb = pda::value(b->fields, "priority", "2");
        if (pa != pb)
            return pa < pb;
        return da == db ? a->id < b->id : da < db;
    });
    label(tasks, 10, 8, 466, "Tarefas pendentes: " + std::to_string(pending.size()), navy);
    for (std::size_t i = 0; i < std::min<std::size_t>(12, pending.size()); ++i) {
        auto *r = pending[i];
        auto due = localDue(*r, service_.state().settings);
        bool late = !now.empty() && !due.empty() && due < now;
        button(tasks, 8, 40 + i * 72, 68, 64, "[ ]", Command::ToggleChild, r->id);
        button(tasks, 84, 40 + i * 72, 390, 64,
               (late ? "ATRASADA: " : "") + r->title + "\n" +
                   pda::displayMinute(due, service_.state().settings),
               Command::OpenRecord, r->id, late ? red : navy);
    }
    if (pending.empty())
        label(tasks, 10, 54, 460, "Nenhuma tarefa pendente.");
    button(body_, 12, 554, 280, 52, "Retomar rascunho", Command::Resume);
    button(body_, 308, 554, 280, 52, "Reapresentar avisos", Command::AlarmShow);
    button(body_, 604, 554, 380, 52, "Ver todas as tarefas", Command::App, 3);
    lastMinute_ = now;
}
void Ui::renderSchedule() {
    pda::CivilMinute date;
    if (!pda::parseCivilMinute(calendarDay_ + " 00:00", date))
        calendarDay_ = "2000-01-01";
    button(body_, 12, 8, 98, 46, "Mês", Command::ScheduleMode, 0);
    button(body_, 118, 8, 110, 46, "Semana", Command::ScheduleMode, 2);
    button(body_, 236, 8, 98, 46, "Dia", Command::ScheduleMode, 1);
    button(body_, 346, 8, 110, 46, "Anterior", Command::ScheduleMove, 0);
    button(body_, 464, 8, 110, 46, "Próximo", Command::ScheduleMove, 1);
    label(body_, 590, 22, 390, calendarDay_, navy);
    auto *selected = selectedRecord();
    label(body_, 12, 72, 490,
          moveRecord_ ? "Toque num horário de destino; Cancelar encerra."
          : selected  ? "Selecionado: " + selected->title
                      : "Toque no evento para abrir ações ou num horário livre para criar.");
    if (moveRecord_)
        button(body_, 772, 62, 210, 46, "Cancelar reagendamento", Command::Reschedule, 0);
    else if (selected && !scheduleOccurrence_.empty()) {
        button(body_, 514, 62, 240, 46, "Abrir compromisso", Command::OpenRecord, selected_);
        button(body_, 772, 62, 210, 46, "Reagendar", Command::Reschedule, 1);
    }
    bool truncated = false;
    int days = scheduleMode_ == 2 ? 7 : 1;
    // A weekly view begins on Monday, including when the chosen date is Sunday.
    std::string first = calendarDay_;
    if (days == 7) {
        pda::parseCivilMinute(first + " 00:00", date);
        int weekday = (pda::civilToUnix(date, 0) / 86400 + 3) % 7;
        first = addDays(first, -weekday);
    }
    scheduleEntries_ = scheduleEntries(service_.state(), first, days, truncated);
    auto *grid = panel(body_, 12, 150, 976, 460, 0xedf2f7);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    int width = 900 / days;
    for (int d = 0; d < days; ++d) {
        auto day = addDays(first, d);
        const char *weekdays[] = {"Seg", "Ter", "Qua", "Qui", "Sex", "Sáb", "Dom"};
        label(body_, 76 + d * width, 120, width - 4,
              std::string(days == 7 ? weekdays[d] : "Dia") + " " + day.substr(8, 2) + "/" +
                  day.substr(5, 2),
              navy);
        for (int hour = 0; hour < 24; ++hour) {
            if (d == 0) {
                char h[8];
                std::snprintf(h, sizeof(h), "%02d:00", hour);
                label(grid, 2, 42 + hour * 80, 60, h, navy);
            }
            auto *slot = button(grid, 64 + d * width, 38 + hour * 80, width - 4, 76, "",
                                Command::ScheduleSlot, d * 24 + hour, 0xcbd5e1);
            lv_obj_set_style_border_width(slot, 1, 0);
        }
        struct Block {
            std::size_t index;
            int start, end, lane;
        };
        std::vector<Block> blocks;
        std::vector<int> ends;
        for (std::size_t i = 0; i < scheduleEntries_.size(); ++i) {
            const auto &e = scheduleEntries_[i];
            auto dayEnd = addDays(day, 1) + " 00:00";
            if (e.start >= dayEnd || (e.end <= day + " 00:00" && e.start != e.end))
                continue;
            if (e.start == e.end && e.start.substr(0, 10) != day)
                continue;
            auto minutes = [](const std::string &s) {
                return std::stoi(s.substr(11, 2)) * 60 + std::stoi(s.substr(14, 2));
            };
            int start = e.start.substr(0, 10) < day ? 0 : minutes(e.start);
            int end = e.end.substr(0, 10) > day ? 1440 : minutes(e.end);
            end = std::max(start + 30, end);
            int lane = 0;
            while (lane < int(ends.size()) && ends[lane] > start)
                ++lane;
            if (lane == int(ends.size()))
                ends.push_back(end);
            else
                ends[lane] = end;
            blocks.push_back({i, start, std::min(end, 1440), lane});
        }
        int lanes = std::max(1, int(ends.size()));
        // Up to three lanes remain large enough to touch. Extra overlaps are listed below the grid.
        for (const auto &b : blocks) {
            if (b.lane >= 3)
                continue;
            auto &e = scheduleEntries_[b.index];
            auto *r = pda::findRecord(service_.state(), e.id);
            int laneWidth = (width - 6) / std::min(lanes, 3);
            button(grid, 64 + d * width + b.lane * laneWidth, 38 + b.start * 80 / 60, laneWidth - 2,
                   std::max(38, (b.end - b.start) * 80 / 60 - 2),
                   e.start.substr(11) + " " + r->title, Command::ScheduleSelect, b.index);
        }
    }
    int bottom = 38 + 24 * 80;
    label(grid, 12, bottom, 940,
          truncated ? "Muitos eventos: limite visual de 4096 ocorrências; filtre por dia."
                    : "Eventos do período — toque para abrir:",
          truncated ? red : navy);
    for (std::size_t i = 0; i < std::min<std::size_t>(scheduleEntries_.size(), 128); ++i) {
        auto &e = scheduleEntries_[i];
        auto *r = pda::findRecord(service_.state(), e.id);
        button(grid, 12, bottom + 34 + i * 54, 940, 46, e.start + " — " + r->title,
               Command::ScheduleSelect, i);
    }
    // Keep the current daytime useful on first display; the entire day remains scrollable.
    lv_obj_update_layout(grid);
    lv_obj_scroll_to_y(grid, 38 + 7 * 80, LV_ANIM_OFF);
}
void Ui::captureExchange() {
    if (inputs_.size() != 2)
        return;
    exchangeFile_ = inputValue(inputs_[0]);
    exchangeFormat_ = lv_dropdown_get_selected(inputs_[1]);
}
void Ui::renderPriority() {
    if (mode_ == Mode::MoveConfirm) {
        auto *r = pda::findRecord(service_.state(), moveRecord_);
        label(body_, 12, 28, 960, r ? "Reagendar: " + r->title : "Compromisso indisponível", navy);
        label(body_, 12, 82, 960,
              "De " + scheduleOccurrence_ + " para " + moveTarget_ +
                  " (hora local).\nA duração será preservada. Em séries, somente esta ocorrência "
                  "será alterada.");
        bool truncated = false;
        auto events = scheduleEntries(service_.state(), moveTarget_.substr(0, 10), 7, truncated);
        int conflicts = 0;
        pda::CivilMinute target, a, b, start;
        pda::parseCivilMinute(moveTarget_, target);
        auto begin = pda::civilToUnix(target, 0), end = begin;
        if (r && pda::parseCivilMinute(r->when, a) &&
            pda::parseCivilMinute(pda::value(r->fields, "until", r->when), b))
            end += pda::civilToUnix(b, 0) - pda::civilToUnix(a, 0);
        for (const auto &e : events) {
            if (e.id == moveRecord_ && e.occurrence == scheduleOccurrence_)
                continue;
            if (!pda::parseCivilMinute(e.start, start) || !pda::parseCivilMinute(e.end, b))
                continue;
            auto x = pda::civilToUnix(start, 0), y = pda::civilToUnix(b, 0);
            if ((begin == end && begin == x) || (begin < y && end > x) ||
                (x == y && x >= begin && x < end))
                ++conflicts;
        }
        label(body_, 12, 180, 960,
              conflicts ? "ATENÇÃO: " + std::to_string(conflicts) + " sobreposição(ões) no destino."
                        : "Nenhuma sobreposição encontrada no destino.",
              conflicts ? red : navy);
        button(body_, 12, 260, 340, 56, "Confirmar reagendamento", Command::MoveConfirm);
        button(body_, 368, 260, 240, 56, "Cancelar", Command::Cancel);
        return;
    }
    if (mode_ == Mode::Trash) {
        button(body_, 12, 8, 220, 46, "Voltar ao backup", Command::Cancel);
        button(body_, 244, 8, 240, 46, "Desfazer exclusão", Command::TrashUndo);
        label(body_, 12, 68, 960,
              "Lixeira: Restaurar recupera; Apagar exige confirmação. Os itens usam a capacidade "
              "da base.");
        auto *list = panel(body_, 12, 110, 976, 500);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        int y = 8;
        std::vector<const pda::Record *> deleted;
        for (const auto &r : service_.state().records)
            if (pda::trashed(r))
                deleted.push_back(&r);
        listPage_ = std::clamp(listPage_, 0, deleted.empty() ? 0 : int((deleted.size() - 1) / 32));
        button(list, 8, y, 200, 44, "Página -", Command::ListPage, 0);
        button(list, 220, y, 200, 44, "Página +", Command::ListPage, 1);
        y += 56;
        auto first = std::size_t(listPage_ * 32);
        for (std::size_t i = first; i < std::min(deleted.size(), first + 32); ++i) {
            auto *r = deleted[i];
            label(list, 8, y + 14, 490, "#" + std::to_string(r->id) + " " + r->title);
            button(list, 506, y, 210, 46, "Restaurar", Command::TrashRestore, r->id);
            button(list, 728, y, 210, 46, "Apagar", Command::TrashPurge, r->id, red);
            y += 58;
        }
        if (deleted.empty())
            label(list, 12, 80, 940, "Lixeira vazia.");
        return;
    }
    if (mode_ == Mode::Purge) {
        label(body_, 12, 30, 960, "Apagar definitivamente este item da lixeira?", red);
        auto *r = selectedRecord();
        label(body_, 12, 90, 960, r ? r->title : "");
        button(body_, 12, 170, 300, 56, "Confirmar exclusão definitiva", Command::TrashPurgeConfirm,
               selected_);
        button(body_, 328, 170, 240, 56, "Voltar à lixeira", Command::Trash);
        return;
    }
    if (mode_ == Mode::ImportPreview) {
        label(body_, 12, 20, 960,
              "Importação: " + std::to_string(pendingImports_.size()) + " novos registros de " +
                  exchangeFile_,
              navy);
        label(body_, 12, 66, 960,
              "A base atual será mantida. Reimportar o mesmo arquivo cria duplicatas.");
        auto *list = panel(body_, 12, 108, 976, 370);
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        int y = 8;
        for (const auto &r : pendingImports_) {
            label(list, 12, y, 940, r.title + "  " + r.when);
            y += 30;
            if (y > 7000)
                break;
        }
        button(body_, 12, 510, 300, 56, "Confirmar importação", Command::ExchangeConfirm);
        button(body_, 328, 510, 240, 56, "Cancelar importação", Command::Exchange);
        return;
    }
    button(body_, 12, 8, 220, 46, "Voltar ao backup", Command::Cancel);
    label(body_, 12, 78, 960, "Nome do arquivo (na pasta /Tab5PDA)");
    inputs_.push_back(input(body_, 12, 110, 700, 56, exchangeFile_, 80));
    inputLimits_.push_back(80);
    inputHeights_.push_back(56);
    label(body_, 12, 190, 960, "Formato");
    inputs_.push_back(
        dropdown(body_, 12, 222,
                 "Calendário (.ics)\nContatos (.vcf)\nDespesas (.csv)\nNota (.txt)\nNota (.md)",
                 exchangeFormat_));
    inputLimits_.push_back(0);
    inputHeights_.push_back(56);
    button(body_, 12, 310, 280, 56, "Exportar para microSD", Command::ExchangeExport);
    button(body_, 308, 310, 280, 56, "Prévia da importação", Command::ExchangePreview);
    button(body_, 604, 310, 360, 56, "Restaurar backup por nome", Command::DailyRestore, 1);
    label(body_, 12, 398, 960,
          "ICS: datas locais/UTC, recorrência simples. Fusos nomeados e regras complexas são "
          "recusados.\nVCF: contatos UTF-8. CSV: cabeçalho gerado pelo app. TXT/MD: primeira linha "
          "é o título.\nBackups diários: daily-AAAA-MM-DD.jsonl. A restauração substitui a base e "
          "pede confirmação.\nExportações normais não incluem PIN nem senha Wi-Fi. Arquivos "
          "existentes com o mesmo nome são substituídos.");
    editor_ = inputs_.front();
}
bool Ui::handlePriority(Command c, uint32_t a) {
    if (c == Command::ListPage) {
        listPage_ = std::max(0, listPage_ + (a ? 1 : -1));
        render();
        return true;
    }
    if (c == Command::OpenRecord) {
        auto *r = pda::findRecord(service_.state(), a);
        if (!r || pda::trashed(*r))
            return true;
        app_ = appFor(r->kind);
        taskFilter_ = 0;
        selected_ = a;
        mode_ = Mode::Normal;
        dailyFilter_.clear();
        query_.clear();
        listPage_ = 0;
        render();
        return true;
    }
    if (c == Command::QuickNew) {
        app_ = a;
        dailyFilter_.clear();
        startEdit();
        return true;
    }
    if (c == Command::TaskFilter) {
        taskFilter_ = (taskFilter_ + 1) % 5;
        listPage_ = 0;
        render();
        return true;
    }
    if (c == Command::ToggleChild || c == Command::Toggle) {
        if (c == Command::Toggle)
            a = selected_;
        auto *before = pda::findRecord(service_.state(), a);
        auto previous = before ? before->when : std::string{};
        bool ok = service_.toggleTask(a, platform_.localMinute());
        if (ok && draft_.active && draft_.record.id == a) {
            auto *current = pda::findRecord(service_.state(), a);
            if (draft_.record.when == previous)
                draft_.record.when = current->when;
            draft_.record.completed = current->completed;
        }
        if (ok)
            render();
        message(ok ? "Tarefa atualizada no microSD" : service_.error(), !ok);
        return true;
    }
    if (c == Command::NewSubtask) {
        auto *r = pda::findRecord(service_.state(), a);
        if (!r || pda::trashed(*r))
            return true;
        subtaskParent_ = std::to_string(a);
        startEdit();
        return true;
    }
    if (c == Command::ScheduleMode) {
        if (app_ != 10)
            calendarDay_ = platform_.localMinute().substr(0, 10);
        app_ = 10;
        mode_ = Mode::Normal;
        scheduleMode_ = a;
        moveRecord_ = 0;
        selected_ = 0;
        scheduleOccurrence_.clear();
        if (calendarDay_.empty())
            calendarDay_ = platform_.localMinute().empty() ? "2000-01-01"
                                                           : platform_.localMinute().substr(0, 10);
        render();
        return true;
    }
    if (c == Command::ScheduleMove) {
        calendarDay_ = addDays(calendarDay_, (a ? 1 : -1) * (scheduleMode_ == 2 ? 7 : 1));
        render();
        return true;
    }
    if (c == Command::ScheduleSelect && a < scheduleEntries_.size()) {
        if (moveRecord_) {
            moveTarget_ = scheduleEntries_[a].start;
            mode_ = Mode::MoveConfirm;
        } else {
            selected_ = scheduleEntries_[a].id;
            scheduleOccurrence_ = scheduleEntries_[a].occurrence;
        }
        render();
        return true;
    }
    if (c == Command::Reschedule) {
        moveRecord_ = a ? selected_ : 0;
        render();
        return true;
    }
    if (c == Command::ScheduleSlot) {
        auto first = calendarDay_;
        pda::CivilMinute t;
        if (scheduleMode_ == 2 && pda::parseCivilMinute(first + " 00:00", t))
            first = addDays(first, -int((pda::civilToUnix(t, 0) / 86400 + 3) % 7));
        char time[12];
        std::snprintf(time, sizeof(time), " %02u:00", a % 24);
        auto target = addDays(first, a / 24) + time;
        if (moveRecord_) {
            moveTarget_ = target;
            mode_ = Mode::MoveConfirm;
            render();
        } else {
            app_ = 2;
            dailyFilter_ = target.substr(0, 10);
            newWhen_ = target;
            startEdit();
        }
        return true;
    }
    if (c == Command::MoveConfirm) {
        std::string error;
        bool ok = moveAppointment(service_, moveRecord_, scheduleOccurrence_, moveTarget_, error);
        if (ok) {
            moveRecord_ = 0;
            mode_ = Mode::Normal;
            render();
        }
        message(ok ? "Compromisso reagendado no microSD" : error, !ok);
        return true;
    }
    if (c == Command::Trash) {
        mode_ = Mode::Trash;
        listPage_ = 0;
        usbEnabled_ = false;
        render();
        return true;
    }
    if (c == Command::TrashRestore || c == Command::TrashUndo) {
        auto id = c == Command::TrashUndo ? lastTrashed_ : a;
        bool ok = id && service_.trashRecord(id, true);
        if (ok) {
            lastTrashed_ = 0;
            render();
        }
        message(ok   ? "Registro restaurado"
                : id ? service_.error()
                     : "Nenhuma exclusão recente para desfazer",
                !ok);
        return true;
    }
    if (c == Command::TrashPurge) {
        selected_ = a;
        mode_ = Mode::Purge;
        render();
        return true;
    }
    if (c == Command::TrashPurgeConfirm) {
        auto *r = pda::findRecord(service_.state(), a);
        bool ok = r && pda::trashed(*r) && service_.deleteRecord(a);
        if (ok) {
            mode_ = Mode::Trash;
            render();
        }
        message(ok ? "Registro apagado definitivamente" : service_.error(), !ok);
        return true;
    }
    if (c == Command::Exchange) {
        mode_ = Mode::Exchange;
        usbEnabled_ = false;
        pendingImports_.clear();
        render();
        return true;
    }
    if (c == Command::DailyRestore) {
        if (!a) {
            exchangeFile_ = "daily-" + platform_.localMinute().substr(0, 10) + ".jsonl";
            mode_ = Mode::Exchange;
            usbEnabled_ = false;
            render();
        } else {
            captureExchange();
            std::string text, error;
            pda::State checked;
            if (!service_.readFile(exchangeFile_, text)) {
                message(service_.error(), true);
                return true;
            }
            if (!pda::importJsonl(text, checked, error)) {
                message(error, true);
                return true;
            }
            restoreFile_ = exchangeFile_;
            mode_ = Mode::ConfirmRestore;
            render();
        }
        return true;
    }
    if (c == Command::ExchangeExport || c == Command::ExchangePreview) {
        captureExchange();
        std::string text, error;
        auto format = static_cast<ExchangeFormat>(exchangeFormat_);
        if (c == Command::ExchangeExport) {
            // Notes are separate editable files, so each can be imported independently.
            if (exchangeFormat_ >= 3) {
                int count = 0;
                for (const auto &r : service_.state().records)
                    if (!pda::trashed(r) && r.kind == pda::RecordKind::Note) {
                        auto name = "note-" + std::to_string(r.id) +
                                    (exchangeFormat_ == 3 ? ".txt" : ".md");
                        auto data = (exchangeFormat_ == 4 ? "# " : "") + r.title + "\n" + r.detail;
                        if (!service_.writeFile(name, data)) {
                            message(service_.error(), true);
                            return true;
                        }
                        ++count;
                    }
                message(std::to_string(count) + " notas exportadas em note-ID" +
                        (exchangeFormat_ == 3 ? ".txt" : ".md"));
                return true;
            }
            const char *extensions[] = {".ics", ".vcf", ".csv"};
            const std::string extension = extensions[exchangeFormat_];
            if (exchangeFile_.size() <= extension.size() ||
                exchangeFile_.substr(exchangeFile_.size() - extension.size()) != extension) {
                message("A exportação exige extensão " + extension, true);
                return true;
            }
            text = exportRecords(service_.state(), format, error);
            if (!error.empty()) {
                message(error, true);
                return true;
            }
            bool ok = service_.writeFile(exchangeFile_, text);
            message(ok ? "Exportação verificada: " + exchangeFile_ : service_.error(), !ok);
        } else {
            if (!service_.readFile(exchangeFile_, text)) {
                message(service_.error(), true);
                return true;
            }
            if (!parseRecords(text, format, service_.state().settings, pendingImports_, error)) {
                message(error, true);
                return true;
            }
            mode_ = Mode::ImportPreview;
            usbEnabled_ = false;
            render();
        }
        return true;
    }
    if (c == Command::ExchangeConfirm) {
        if (mode_ != Mode::ImportPreview || pendingImports_.empty())
            return true;
        std::string error;
        if (mergeRecords(service_, pendingImports_, error)) {
            pendingImports_.clear();
            mode_ = Mode::Normal;
            render();
            message("Importação gravada no microSD");
        } else
            message(error, true);
        return true;
    }
    return false;
}
} // namespace tab5_pda
