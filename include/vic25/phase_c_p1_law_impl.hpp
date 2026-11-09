#pragma once
// P1 is the sole constitutional authority. A P2 law choice updates this
// same P1 record only at Stage 0 after lawful assent and promulgation.
namespace vic25::phasec {
inline void IntegratedWorld::p2_validate_constitution_choice(
        const P2BaseLaw& law)const {
    if(law.domain!="governance_principles"&&
       law.domain!="distribution_of_power"&&
       law.domain!="legislature_structure")return;
    ensure(law.scope.empty()||law.scope=="national",
           "P1 constitutional changes cannot be local");
    ensure(law.tier!=P2NormTier::local_ordinance&&
           p1_constitutions_.contains(law.country),
           "P1 constitution reform needs a real national P1 charter");
    auto candidate=p1_constitutions_.at(law.country);
    const auto& choice=law.base_choice;
    if(law.domain=="governance_principles") {
        if(choice=="chiefdom")candidate.governance=P1GovernancePrinciple::chiefdom;
        else if(choice=="monarchy")candidate.governance=P1GovernancePrinciple::monarchy;
        else if(choice=="theocracy")candidate.governance=P1GovernancePrinciple::theocracy;
        else if(choice=="presidential_republic")
            candidate.governance=P1GovernancePrinciple::presidential_republic;
        else if(choice=="parliamentary_republic")
            candidate.governance=P1GovernancePrinciple::parliamentary_republic;
        else if(choice=="council_republic")
            candidate.governance=P1GovernancePrinciple::council_republic;
        else ensure(false,"P1 unsupported governance law choice");
    } else if(law.domain=="distribution_of_power") {
        if(choice=="autocracy")candidate.power=P1PowerDistribution::autocracy;
        else if(choice=="oligarchy")candidate.power=P1PowerDistribution::oligarchy;
        else if(choice=="landed_vote"||choice=="landed_voting")
            candidate.power=P1PowerDistribution::landed_vote;
        else if(choice=="wealth_vote"||choice=="wealth_voting")
            candidate.power=P1PowerDistribution::wealth_vote;
        else if(choice=="universal_vote"||choice=="universal_suffrage")
            candidate.power=P1PowerDistribution::universal_vote;
        else if(choice=="single_party"||choice=="single_party_state")
            candidate.power=P1PowerDistribution::single_party;
        else if(choice=="technocracy")
            candidate.power=P1PowerDistribution::technocracy;
        else ensure(false,"P1 unsupported distribution of power law choice");
    } else {
        if(choice=="none")candidate.legislature=P1LegislatureKind::none;
        else if(choice=="unicameral")candidate.legislature=P1LegislatureKind::unicameral;
        else if(choice=="bicameral")candidate.legislature=P1LegislatureKind::bicameral;
        else ensure(false,"P1 unsupported legislature structure law");
        if(candidate.legislature!=P1LegislatureKind::none) {
            bool lower=false,upper=false;
            for(const auto& [id,contest]:p4_contests_) {
                (void)id;
                if(contest.country!=law.country)continue;
                if(contest.office==P4Office::legislature)lower=true;
                if(contest.office==P4Office::upper_chamber)upper=true;
            }
            ensure(lower&&
                   (candidate.legislature!=P1LegislatureKind::bicameral||upper),
                   "P1 legislature reform needs real seeded P4 chamber contests");
        }
    }
    candidate.legal_basis=law.legal_basis;
    ensure(valid_p1_constitution(candidate),
           "P1 reformed constitution is incompatible with legal legislature");
}
inline void IntegratedWorld::p2_apply_constitution_choice(
        const P2BaseLaw& law) {
    require_stage({0});
    if(law.domain!="governance_principles"&&
       law.domain!="distribution_of_power"&&
       law.domain!="legislature_structure")return;
    p2_validate_constitution_choice(law);
    auto& charter=p1_constitutions_.at(law.country);
    if(law.domain=="governance_principles") {
        const auto& choice=law.base_choice;
        if(choice=="chiefdom")charter.governance=P1GovernancePrinciple::chiefdom;
        else if(choice=="monarchy")charter.governance=P1GovernancePrinciple::monarchy;
        else if(choice=="theocracy")charter.governance=P1GovernancePrinciple::theocracy;
        else if(choice=="presidential_republic")
            charter.governance=P1GovernancePrinciple::presidential_republic;
        else if(choice=="parliamentary_republic")
            charter.governance=P1GovernancePrinciple::parliamentary_republic;
        else charter.governance=P1GovernancePrinciple::council_republic;
    } else if(law.domain=="distribution_of_power") {
        const auto& choice=law.base_choice;
        if(choice=="autocracy")charter.power=P1PowerDistribution::autocracy;
        else if(choice=="oligarchy")charter.power=P1PowerDistribution::oligarchy;
        else if(choice=="landed_vote"||choice=="landed_voting")
            charter.power=P1PowerDistribution::landed_vote;
        else if(choice=="wealth_vote"||choice=="wealth_voting")
            charter.power=P1PowerDistribution::wealth_vote;
        else if(choice=="universal_vote"||choice=="universal_suffrage")
            charter.power=P1PowerDistribution::universal_vote;
        else if(choice=="single_party"||choice=="single_party_state")
            charter.power=P1PowerDistribution::single_party;
        else charter.power=P1PowerDistribution::technocracy;
    } else {
        const auto& choice=law.base_choice;
        charter.legislature=choice=="none"?P1LegislatureKind::none:
            choice=="bicameral"?P1LegislatureKind::bicameral:
                                 P1LegislatureKind::unicameral;
        auto& government=governments_.at(law.country);
        if(charter.legislature==P1LegislatureKind::none) {
            government.chamber_seats=0;government.quorum=0;
            p1_coalitions_.at(law.country).ruling_parties.clear();
        } else {
            // Reopen only real, already seeded legislative contests.
            for(auto& [id,contest]:p4_contests_) {
                (void)id;
                if(contest.country!=law.country||
                   contest.office!=P4Office::legislature)continue;
                government.chamber_seats=contest.seats;
                government.quorum=contest.seats/2+1;
                break;
            }
        }
        for(auto& [id,contest]:p4_contests_) {
            (void)id;
            if(contest.country!=law.country)continue;
            if(contest.office==P4Office::legislature)
                contest.active=charter.legislature!=P1LegislatureKind::none;
            if(contest.office==P4Office::upper_chamber)
                contest.active=charter.legislature==P1LegislatureKind::bicameral;
        }
        p4_rebuild_due_index();
    }
    charter.legal_basis=law.legal_basis;
    // A replacement charter invalidates ad-hoc veto overrides inherited
    // from the previous regime; otherwise presidential vetoes could survive
    // the enactment of a parliamentary republic.
    p1_legislative_checks_.erase(law.country);
    ensure(valid_p1_constitution(charter),"P1 invalid ratified charter");
    ++governments_.at(law.country).revision;
    events_.push_back("p2-constitutional-law-effective-in-p1:"+law.country);
}
} // namespace vic25::phasec
