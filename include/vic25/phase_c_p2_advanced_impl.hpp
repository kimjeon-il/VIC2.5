#pragma once
// P2 advanced legislative contracts. No separate senate seats or P3 POP
// store: both chamber votes and negotiation support read existing ledgers.
namespace vic25::phasec {

inline void IntegratedWorld::p2_register_upper_chamber(
        const std::string& country,const std::string& contest_id) {
    ensure(stage_==-1&&commit_id_==0&&
           p1_constitutions_.contains(country)&&
           p1_constitutions_.at(country).legislature==
               P1LegislatureKind::bicameral&&
           p4_contests_.contains(contest_id)&&
           p4_contests_.at(contest_id).country==country&&
           p4_contests_.at(contest_id).office==P4Office::upper_chamber&&
           !p2_upper_chambers_.contains(country),
           "P2 upper chamber must bind to a real bicameral P4 contest");
    p2_upper_chambers_.emplace(country,contest_id);
}

inline void IntegratedWorld::p2_approve_upper_bill(
        const std::string& bill_id,
        const std::vector<std::string>& supporting_parties) {
    require_stage({9});
    auto& bill=p2_bills_.at(bill_id);
    ensure(bill.status==P2BillStatus::submitted&&
           p1_constitutions_.at(bill.country).legislature==
               P1LegislatureKind::bicameral&&
           p2_upper_chambers_.contains(bill.country)&&
           p2_chamber_decisions_.contains(bill_id)&&
           p2_chamber_decisions_.at(bill_id).lower_passed&&
           !p2_chamber_decisions_.at(bill_id).upper_passed,
           "P2 senate vote requires a prior valid lower-house approval");
    auto& approval=p2_chamber_decisions_.at(bill_id);
    const auto& contest=p4_contests_.at(approval.upper_contest_id);
    const auto& senate=p4_nonparliament_results_.at(contest.id);
    ensure(senate.contest_id==contest.id&&
           senate.day>=0&&senate.day==contest.last_poll_day&&
           country_elections_.contains(bill.country)&&
           country_election_months_.at(bill.country)==approval.lower_poll_day,
           "P2 stale chamber vote after new parliamentary election");
    const auto tier=(bill.action==P2BillAction::enact_or_replace_base)?
        bill.proposed_base.tier:
        p2_laws_.at(bill.action==P2BillAction::attach_amendment?
                bill.proposed_amendment.law_id:
                bill.action==P2BillAction::repeal_base?
                bill.target_id:p2_amendments_.at(bill.target_id).law_id).tier;
    People votes=0;
    std::set<std::string> unique;
    for(const auto& party:supporting_parties) {
        ensure(unique.insert(party).second&&
               senate.seats.contains(party),
               "P2 senate approval references missing or duplicate party bloc");
        votes=phaseb::add_checked(votes,senate.seats.at(party));
    }
    const People quorum=p2_chamber_quorum(
        contest.seats,contest.seats/2+1,tier);
    ensure(quorum>0&&votes>=quorum,
           "P2 insufficient actual upper-house votes for this statute");
    approval.upper_poll_day=senate.day;
    approval.upper_passed=true;
    bill.status=P2BillStatus::approved;
    p2_open_assent(bill_id);
    events_.push_back("p2-upper-chamber-approved:"+bill_id);
}

inline void IntegratedWorld::p2_register_group_demand(P2GroupDemand demand) {
    ensure(stage_==-1&&commit_id_==0&&
           !demand.id.empty()&&!p2_group_demands_.contains(demand.id)&&
           governments_.contains(demand.country)&&
           interest_groups_.contains(demand.interest_group)&&
           !demand.law_domain.empty()&&
           !demand.amendment.id.empty()&&
           !demand.amendment.legal_basis.empty()&&
           (p2_catalog_group(demand.law_domain)?
             (p2_catalog_amendment(demand.amendment.id)!=nullptr&&
              demand.amendment.clauses.empty()):
             !demand.amendment.clauses.empty())&&
           demand.min_population_support_ppm>=0&&
           demand.min_population_support_ppm<=1'000'000,
           "P2 invalid documented interest-group negotiation demand");
    p2_validate_clauses(demand.country,demand.amendment.clauses);
    p2_group_demands_.emplace(demand.id,std::move(demand));
}

inline bool IntegratedWorld::p2_has_negotiable_bills()const {
    if(p2_group_demands_.empty())return false;
    for(const auto& id:p2_negotiation_watch_) {
        const auto& bill=p2_bills_.at(id);
        for(const auto& [demand_id,demand]:p2_group_demands_) {
            if(bill.country!=demand.country||
               bill.proposed_base.domain!=demand.law_domain)continue;
            bool already=false;
            if(const auto it=p2_negotiated_.find(id);
               it!=p2_negotiated_.end())
                for(const auto& accepted:it->second)
                    if(accepted.demand_id==demand_id)already=true;
            if(!already)return true;
        }
    }
    return false;
}

inline void IntegratedWorld::p2_auto_negotiate_pending(
        std::int64_t today) {
    require_stage({9});
    ensure(today>=0,"P2 negotiation requires real calendar day");
    if(!p2_has_negotiable_bills())return;
    // Existing P3 membership is a support indicator, not a new voter or
    // political-power ledger. A group cannot negotiate without real persons.
    std::map<std::string,People> population;
    std::map<std::pair<std::string,std::string>,People> members;
    for(const auto& [slice,row]:people_.slices()) {
        const auto& dem=people_.demographic(row.key.demographic);
        const auto sovereign=sovereignty_.find(dem.region);
        if(sovereign==sovereignty_.end())continue;
        population[sovereign->second]=phaseb::add_checked(
            population[sovereign->second],row.count);
    }
    for(const auto& [key,n]:membership_) {
        if(n<=0||!people_.has_slice(key.first))continue;
        const auto& row=people_.slice(key.first);
        const auto& dem=people_.demographic(row.key.demographic);
        const auto sovereign=sovereignty_.find(dem.region);
        if(sovereign==sovereignty_.end())continue;
        auto& count=members[{sovereign->second,key.second}];
        count=phaseb::add_checked(count,n);
    }
    for(const auto& bill_id:p2_negotiation_watch_) {
        const auto& bill=p2_bills_.at(bill_id);
        const People total=population[bill.country];
        if(total<=0)continue;
        auto& negotiations=p2_negotiated_[bill_id];
        for(const auto& [id,demand]:p2_group_demands_) {
            if(demand.country!=bill.country||
               demand.law_domain!=bill.proposed_base.domain||
               (!demand.amendment.law_id.empty()&&
                demand.amendment.law_id!=bill.proposed_base.id))continue;
            bool accepted=false;
            for(const auto& proposal:negotiations)
                if(proposal.demand_id==id)accepted=true;
            if(accepted)continue;
            // Game laws can negotiate only named 1.15 sponsor concessions.
            // Other 1.15 amendments require their original scripted event /
            // country trigger; do not fabricate them from POP support alone.
            if(const auto* chosen=p2_catalog_law(bill.proposed_base.base_choice);
               chosen&&chosen->group==bill.proposed_base.domain) {
                const auto* defined=p2_catalog_amendment(demand.amendment.id);
                if(!defined||!defined->generic_sponsor||
                   !p2_catalog_amendment_applies(*defined,chosen->id))
                    continue;
            }
            const auto profile=p3_groups_.find(
                {bill.country,demand.interest_group});
            if(profile==p3_groups_.end())continue;
            const auto sentiment=p3_group_view(bill.country,demand.interest_group);
            // A radicalized opponent can refuse compromise; a satisfied
            // group is not forced into negotiating an unwanted reform.
            if(sentiment.radical_ppm>=700'000&&
               p1_group_legal_preference(profile->second)!=
                   bill.proposed_base.base_choice) {
                events_.push_back("p2-ig-refuses-negotiation:"+id);
                continue;
            }
            const People support=members[{bill.country,demand.interest_group}];
            if(support<=0||compare_nonnegative_ratios(
                static_cast<std::uint64_t>(support),
                static_cast<std::uint64_t>(total),
                static_cast<std::uint64_t>(demand.min_population_support_ppm),
                1'000'000)<0)continue;
            auto amendment=demand.amendment;
            amendment.law_id=bill.proposed_base.id;
            // No invented exception may overrule an unspecified office law.
            bool rooted=true,conflict=false;
            for(const auto& clause:amendment.clauses) {
                if(clause.kind==P2ClauseKind::term_months||
                   clause.kind==P2ClauseKind::term_cycle_mode||
                   clause.kind==P2ClauseKind::dissolution_authorized) {
                    bool found=false;
                    for(const auto& base:bill.proposed_base.clauses)
                        if(base.target==clause.target&&base.kind==clause.kind)
                            found=true;
                    rooted=rooted&&found;
                }
                for(const auto& other:negotiations) {
                    if(!amendment.exclusivity_group.empty()&&
                       other.amendment.exclusivity_group==
                           amendment.exclusivity_group)
                        conflict=true;
                    for(const auto& previous:other.amendment.clauses)
                        if(previous.target==clause.target&&
                           previous.kind==clause.kind)
                            conflict=true;
                }
            }
            if(!rooted||conflict)continue;
            negotiations.push_back({bill_id,id,demand.interest_group,
                                    amendment,support,today});
            events_.push_back("p2-p3-negotiated-amendment:"+id);
        }
        if(negotiations.empty())p2_negotiated_.erase(bill_id);
    }
}

} // namespace vic25::phasec
