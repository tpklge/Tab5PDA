#pragma once
#include "pda/organizer.hpp"
#include "pda/service.hpp"
namespace tab5_pda {
struct ScheduleEntry {
    uint32_t id;
    std::string start, end, occurrence;
};
std::vector<ScheduleEntry> scheduleEntries(const pda::State &, const std::string &day, int days,
                                           bool &truncated);
bool moveAppointment(pda::Service &, uint32_t id, const std::string &occurrence,
                     const std::string &localTarget, std::string &error);
} // namespace tab5_pda
