#pragma once
// All catalogue choices use the authoritative P2 law and derived 1.15
// modifier catalogue. A new choice never inherits stale old-law effects.
namespace vic25::phasec {
inline std::optional<P2BaseLaw> IntegratedWorld::p2_catalogue_reform_template(
        const std::string& country,const std::string& domain,
        const std::string& desired,const std::string& bill_id,
        const std::string& basis)const {
    const auto group=politics::bare_group(domain);
    const auto choice=politics::bare_law(desired);
    const auto& cat=politics::catalogue();
    if(bill_id.empty()||basis.empty()||!governments_.contains(country)||
       !p1_constitutions_.contains(country)||!cat.has(group,choice))
        return std::nullopt;
    const P2BaseLaw* previous=nullptr;
    for(const auto& [slot,id]:p2_active_base_) {
        (void)slot;
        const auto& law=p2_laws_.at(id);
        if(law.country==country&&politics::bare_group(law.domain)==group) {
            if(previous)return std::nullopt; // ambiguous historical law slot
            previous=&law;
        }
    }
    // Preserve known historical baseline instead of inventing an opening law.
    if(!previous||politics::bare_law(previous->base_choice)==choice)
        return std::nullopt;
    const auto banned=cat.disallowed.find(choice);
    for(const auto& [slot,id]:p2_active_base_) {
        (void)slot;
        const auto& existing=p2_laws_.at(id);
        if(existing.country!=country||
           politics::bare_group(existing.domain)==group)continue;
        const auto other=politics::bare_law(existing.base_choice);
        const auto inverse=cat.disallowed.find(other);
        if((banned!=cat.disallowed.end()&&banned->second.contains(other))||
           (inverse!=cat.disallowed.end()&&inverse->second.contains(choice)))
            return std::nullopt;
    }
    if(group=="governance_principles"&&
       p1_constitution(country).legislature==P1LegislatureKind::none&&
       (choice=="parliamentary_republic"||choice=="council_republic"||
        choice=="social_monarchy"))
        return std::nullopt;
    // An enacted tax change must not retroactively invalidate an actual rate.
    if(const auto cap=politics::law_clause(
           group,choice,P2ClauseKind::income_tax_max_ppm);
       cap&&income_tax_ppm(country)>*cap)
        return std::nullopt;
    P2BaseLaw proposal=*previous;
    proposal.id=bill_id+":norm";
    proposal.base_choice=choice;
    proposal.legal_basis=basis;
    proposal.clauses.clear();
    // Office terms are independent continuity rules, not ideological
    // economic bonuses. Retain only those required for real P4 mandates.
    for(const auto& clause:previous->clauses) {
        switch(clause.kind) {
          case P2ClauseKind::term_months:
          case P2ClauseKind::term_cycle_mode:
          case P2ClauseKind::dissolution_authorized:
          case P2ClauseKind::max_consecutive_terms:
          case P2ClauseKind::max_lifetime_terms:
          case P2ClauseKind::dissolution_max_delay_days: {
            auto retained=clause;
            retained.id=proposal.id+":"+clause.id;
            proposal.clauses.push_back(std::move(retained));
            break;
          }
          default:break;
        }
    }
    try {p2_validate_base(proposal);}
    catch(const std::exception&) {return std::nullopt;}
    return proposal;
}
inline void IntegratedWorld::p2_propose_catalogue_reform(
        const std::string& country,const std::string& group,
        const std::string& choice,const std::string& proposer,
        const std::string& basis,const std::string& bill_id,
        std::int64_t today) {
    require_stage({9});
    const auto law=p2_catalogue_reform_template(
        country,group,choice,bill_id,basis);
    ensure(law.has_value(),"P2 catalogue reform is not admissible");
    P2Bill bill;
    bill.id=bill_id;bill.country=country;bill.proposer=proposer;
    bill.legal_basis=basis;bill.proposed_base=*law;
    bill.effective_day=-1;
    p2_submit_bill(std::move(bill),today);
}
} // namespace vic25::phasec
