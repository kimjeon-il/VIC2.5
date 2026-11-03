#include "vic25/ledgers.hpp"
#include "vic25/monthly.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vic25;

template <class Fn> bool rejects(Fn fn) {
    try { fn(); } catch (const std::invalid_argument&) { return true; }
    return false;
}

int main() {
    Pop service{"service", {"a", "culture", "religion", "soldier", "employed", "low", "citizen"}, {}};
    CohortKey source{-300, Sex::male, Health::well, 0, MilitaryStatus::active_service};
    add_people(service, source, 1000);
    assert(rejects([&] { apply_battle_loss(service, source, {1001, 0, 0}); }));
    assert(service.population() == 1000);
    apply_battle_loss(service, source, {50, 180, 70});
    assert(service.population() == 950);
    CohortKey injured = source; injured.health = Health::temporary_injury;
    CohortKey impaired = source; impaired.health = Health::long_term_impairment;
    assert(service.cohorts.at(source) == 700);
    assert(service.cohorts.at(injured) == 180);
    assert(service.cohorts.at(impaired) == 70);
    Pop discharged{"discharged", {"a", "culture", "religion", "soldier", "unemployed", "low", "citizen"}, {}};
    transfer_people(service, discharged, injured, 20);
    assert(service.population() == 930 && discharged.population() == 20);
    assert(rejects([&] { transfer_people(service, discharged, injured, 1000); }));

    FinancialLedger finance;
    finance.add_account("bank_loans", "bank", AccountType::asset);
    finance.add_account("bank_deposits", "bank", AccountType::liability);
    finance.add_account("pop_deposit", "pop", AccountType::asset);
    finance.add_account("pop_loan", "pop", AccountType::liability);
    finance.post({"loan-1", "bank-originated credit", {
        {"bank_loans", 100, 0}, {"bank_deposits", 0, 100},
        {"pop_deposit", 100, 0}, {"pop_loan", 0, 100}
    }});
    assert(finance.balanced());
    assert(finance.balance("pop_deposit") == 100);
    assert(finance.balance("bank_deposits") == -100);
    assert(rejects([&] { finance.post({"bad", "unbalanced", {{"bank_loans", 3, 0}, {"bank_deposits", 0, 2}}}); }));
    assert(finance.balanced());
    assert(rejects([&] { finance.post({"loan-1", "duplicate", {{"bank_loans", 1, 0}, {"bank_deposits", 0, 1}}}); }));

    InventoryLedger inventory;
    InventoryKey farm{"farm", "region-1", "grain"};
    InventoryKey household{"household", "region-1", "grain"};
    inventory.initialize(farm, 1000);
    inventory.produce(farm, 100);
    inventory.transfer(farm, household, 250);
    inventory.consume(household, 200);
    assert(inventory.stock(farm) == 850);
    assert(inventory.stock(household) == 50);
    assert(inventory.conserved("grain"));
    assert(rejects([&] { inventory.transfer(farm, household, 1000); }));

    OwnershipLedger ownership;
    ownership.issue("factory", "investors", 100);
    ownership.transfer("factory", "investors", "workers", 25);
    assert(ownership.units("factory", "workers") == 25);
    assert(ownership.conserved("factory"));

    EmploymentLedger jobs;
    jobs.upsert({"service", "government", "soldier", 900, 20});
    assert(jobs.within_population(service));
    jobs.upsert({"service", "government", "soldier", 931, 20});
    assert(!jobs.within_population(service)); // A real hiring engine MUST reject this.

    MonthlyPipeline clock;
    std::vector<MonthStage> seen;
    clock.advance([&](MonthStage stage) { seen.push_back(stage); });
    assert(seen.size() == 12);
    for (std::size_t i=0;i<seen.size();++i) assert(seen[i] == MONTH_STAGES[i]);
    assert(clock.month_index() == 1);
}
