#pragma once
// C-P2 implementation: current legal rules are derived from one active base
// per (sovereign, domain, jurisdiction), plus nonconflicting amendments.
// No independent demographic, financial, seat or institution state.
namespace vic25::phasec {

inline bool IntegratedWorld::p2_valid_clause(const P2Clause& c) {
    if(c.id.empty()||c.target.empty()||
       static_cast<unsigned>(c.kind)>
           static_cast<unsigned>(P2ClauseKind::peaceful_strike_allowed))
        return false;
    switch(c.kind) {
      case P2ClauseKind::term_months:
        return c.value>=1&&c.value<=240;
      case P2ClauseKind::max_consecutive_terms:
      case P2ClauseKind::max_lifetime_terms:
        return c.value>=0&&c.value<=20;
      case P2ClauseKind::term_cycle_mode:
      case P2ClauseKind::dissolution_authorized:
      case P2ClauseKind::public_healthcare_right:
      case P2ClauseKind::conscription_authorized:
      case P2ClauseKind::female_suffrage_allowed:
      case P2ClauseKind::political_association_allowed:
      case P2ClauseKind::peaceful_strike_allowed:
        return c.value==0||c.value==1;
      case P2ClauseKind::public_institution_level:
        return c.value>=0&&c.value<=5;
      case P2ClauseKind::minimum_monthly_contract_wage:
        return c.value>=0&&c.value<=1'000'000'000;
      case P2ClauseKind::dissolution_max_delay_days:
        return c.value>=1&&c.value<=365;
      case P2ClauseKind::voting_age_years:
        return c.value>=0&&c.value<=120;
      case P2ClauseKind::income_tax_min_ppm:
      case P2ClauseKind::income_tax_max_ppm:
      case P2ClauseKind::maximum_import_tariff_ppm:
        return c.value>=0&&c.value<=1'000'000;
    }
    return false;
}

inline void IntegratedWorld::p2_validate_clauses(
        const std::string& country,const std::vector<P2Clause>& clauses)const {
    std::set<std::pair<std::string,P2ClauseKind>> keys;
    std::set<std::string> ids;
    for(const auto& c:clauses) {
        ensure(p2_valid_clause(c)&&ids.insert(c.id).second&&
               keys.insert({c.target,c.kind}).second,
               "P2 invalid, duplicate or untyped legal provision");
        if(c.kind==P2ClauseKind::voting_age_years||
           c.kind==P2ClauseKind::income_tax_min_ppm||
           c.kind==P2ClauseKind::income_tax_max_ppm||
           c.kind==P2ClauseKind::public_healthcare_right||
           c.kind==P2ClauseKind::conscription_authorized||
           c.kind==P2ClauseKind::minimum_monthly_contract_wage||
           c.kind==P2ClauseKind::maximum_import_tariff_ppm||
           c.kind==P2ClauseKind::female_suffrage_allowed||
           c.kind==P2ClauseKind::political_association_allowed||
           c.kind==P2ClauseKind::peaceful_strike_allowed) {
            ensure(c.target==country,
                   "P2 sovereign economic, civil or military rule needs its country");
        } else if(c.kind==P2ClauseKind::public_institution_level) {
            const auto it=p1_institutions_.find(c.target);
            ensure(it!=p1_institutions_.end()&&it->second.country==country,
                   "P2 institution level must reference real P1 public service");
        } else {
            const auto it=p4_contests_.find(c.target);
            ensure(it!=p4_contests_.end()&&it->second.country==country,
                   "P2 office provision must reference actual P4 elected office");
        }
    }
}

inline void IntegratedWorld::p2_validate_base(const P2BaseLaw& law)const {
    ensure(!law.id.empty()&&!law.country.empty()&&!law.domain.empty()&&
           !law.base_choice.empty()&&!law.legal_basis.empty()&&
           governments_.contains(law.country)&&
           p1_constitutions_.contains(law.country)&&
           static_cast<unsigned>(law.tier)<=
               static_cast<unsigned>(P2NormTier::local_ordinance),
           "P2 base law lacks legal authority or declared choice");
    if(law.tier==P2NormTier::local_ordinance) {
        ensure(!law.scope.empty()&&sovereignty_.contains(law.scope)&&
               sovereignty_.at(law.scope)==law.country,
               "P2 local ordinance requires real territorial jurisdiction");
    } else {
        ensure(law.scope.empty()||law.scope=="national",
               "P2 national law cannot impersonate regional sovereignty");
    }
    p2_validate_clauses(law.country,law.clauses);
    if(law.tier==P2NormTier::local_ordinance) {
        for(const auto& clause:law.clauses) {
            ensure(clause.target!=law.country,
                   "P2 local ordinance cannot legislate nationwide tax or suffrage");
            const auto it=p4_contests_.find(clause.target);
            ensure((clause.kind==P2ClauseKind::public_institution_level&&
                    p1_institutions_.contains(clause.target)&&
                    public_institutions_.at(clause.target).region==law.scope)||
                   (it!=p4_contests_.end()&&
                    (it->second.office==P4Office::local||
                     it->second.office==P4Office::local_head)&&
                    it->second.jurisdiction==law.scope),
                   "P2 local ordinance cannot change national elected offices");
        }
    }
}

inline void IntegratedWorld::p2_check_amendment(const P2Amendment& amendment)const {
    const auto it=p2_laws_.find(amendment.law_id);
    ensure(!amendment.id.empty()&&!amendment.law_id.empty()&&
           !amendment.legal_basis.empty()&&it!=p2_laws_.end()&&
           !amendment.clauses.empty(),
           "P2 amendment must belong to existing base law");
    p2_validate_clauses(it->second.country,amendment.clauses);
    // Mirrored P4 mandate fields need a base value to restore on repeal.
    for(const auto& clause:amendment.clauses) {
        if(clause.kind!=P2ClauseKind::term_months&&
           clause.kind!=P2ClauseKind::term_cycle_mode&&
           clause.kind!=P2ClauseKind::dissolution_authorized)continue;
        const bool rooted=std::any_of(it->second.clauses.begin(),
            it->second.clauses.end(),[&](const P2Clause& base){
                return base.target==clause.target&&base.kind==clause.kind;
            });
        ensure(rooted,
               "P2 office amendment requires underlying base provision");
    }
    for(const auto& other_id:p2_active_amendments_) {
        if(other_id==amendment.id)continue;
        const auto& other=p2_amendments_.at(other_id);
        if(other.law_id!=amendment.law_id)continue;
        ensure(amendment.exclusivity_group.empty()||
               amendment.exclusivity_group!=other.exclusivity_group,
               "P2 legally exclusive amendments cannot coexist");
        for(const auto& c:amendment.clauses)
            for(const auto& old:other.clauses)
                ensure(c.target!=old.target||c.kind!=old.kind,
                       "P2 duplicate substantive override across amendments");
    }
}

inline void IntegratedWorld::p2_seed_base(P2BaseLaw law) {
    ensure(stage_==-1&&commit_id_==0,"P2 law must seed before simulation");
    p2_validate_base(law);
    const auto slot=std::make_tuple(law.country,law.domain,law.scope);
    ensure(!p2_laws_.contains(law.id)&&!p2_active_base_.contains(slot),
           "P2 mutually exclusive base law already exists in this slot");
    const auto id=law.id;
    p2_laws_.emplace(id,std::move(law));
    p2_active_base_.emplace(slot,id);
    p2_sync_country_rules(p2_laws_.at(id).country,{},-1);
}

inline void IntegratedWorld::p2_seed_amendment(P2Amendment amendment) {
    ensure(stage_==-1&&commit_id_==0&&
           !p2_amendments_.contains(amendment.id)&&
           p2_is_active_base(amendment.law_id),
           "P2 amendment seed requires an active base law");
    p2_check_amendment(amendment);
    const auto id=amendment.id, country=p2_laws_.at(amendment.law_id).country;
    p2_amendments_.emplace(id,std::move(amendment));
    p2_active_amendments_.insert(id);
    p2_sync_country_rules(country,{},-1);
}

inline bool IntegratedWorld::p2_is_active_base(const std::string& id)const {
    const auto it=p2_laws_.find(id);
    if(it==p2_laws_.end())return false;
    const auto key=std::make_tuple(it->second.country,
                                    it->second.domain,it->second.scope);
    const auto active=p2_active_base_.find(key);
    return active!=p2_active_base_.end()&&active->second==id;
}

inline bool IntegratedWorld::p2_amendment_active(const std::string& id)const {
    return p2_active_amendments_.contains(id);
}

inline std::optional<std::int64_t> IntegratedWorld::p2_rule(
        const std::string& country,const std::string& target,
        P2ClauseKind kind)const {
    std::optional<std::int64_t> value;
    int priority=1000;
    for(const auto& [slot,id]:p2_active_base_) {
        (void)slot;
        const auto& base=p2_laws_.at(id);
        if(base.country!=country)continue;
        std::optional<std::int64_t> found;
        for(const auto& clause:base.clauses)
            if(clause.target==target&&clause.kind==kind)
                found=clause.value;
        for(const auto& amendment_id:p2_active_amendments_) {
            const auto& amendment=p2_amendments_.at(amendment_id);
            if(amendment.law_id!=id)continue;
            for(const auto& clause:amendment.clauses)
                if(clause.target==target&&clause.kind==kind)
                    found=clause.value;
        }
        if(!found)continue;
        const int rank=static_cast<int>(base.tier);
        ensure(rank!=priority,
               "P2 ambiguous equal-rank active legal provisions");
        if(rank<priority) {
            priority=rank;
            value=found;
        }
    }
    return value;
}

inline int IntegratedWorld::p2_voting_age(const std::string& country)const {
    const auto n=p2_rule(country,country,P2ClauseKind::voting_age_years);
    return n?static_cast<int>(*n):voting_age_years_;
}

inline P2TermView IntegratedWorld::p2_term(
        const std::string& contest_id)const {
    const auto& contest=p4_contests_.at(contest_id);
    P2TermView term;
    term.contest_id=contest_id;
    term.legal_basis=contest.law_id;
    term.term_months=contest.term_months;
    term.cycle_mode=contest.term_mode;
    term.dissolution_authorized=contest.dissolution_authorized;
    const auto rule=[&](P2ClauseKind kind){
        return p2_rule(contest.country,contest_id,kind);
    };
    if(const auto n=rule(P2ClauseKind::term_months)) {
        term.has_legal_term=true;
        term.term_months=static_cast<int>(*n);
    }
    if(const auto n=rule(P2ClauseKind::term_cycle_mode)) {
        term.cycle_mode=static_cast<P4TermMode>(*n);
        term.has_legal_term=true;
    }
    if(const auto n=rule(P2ClauseKind::max_consecutive_terms))
        term.max_consecutive_terms=static_cast<int>(*n);
    if(const auto n=rule(P2ClauseKind::max_lifetime_terms))
        term.max_lifetime_terms=static_cast<int>(*n);
    if(const auto n=rule(P2ClauseKind::dissolution_authorized)) {
        term.dissolution_authorized=*n==1;
        term.has_legal_term=true;
    }
    if(const auto n=rule(P2ClauseKind::dissolution_max_delay_days))
        term.dissolution_max_delay_days=static_cast<int>(*n);
    return term;
}

inline bool IntegratedWorld::p2_candidate_eligible(
        const std::string& contest_id,const std::string& candidate)const {
    const auto& c=p4_contests_.at(contest_id);
    if(candidate.empty())return false;
    const auto rule=p2_term(contest_id);
    int cumulative=0,consecutive=0;
    const auto personal=p4_person_terms_.find({contest_id,candidate});
    if(personal!=p4_person_terms_.end()) {
        // Named representatives, senators and local heads use the same
        // persistent real-person counter, keyed by actual office contest.
        cumulative=personal->second.total_terms;
        consecutive=personal->second.incumbent?
            personal->second.consecutive_terms:0;
    } else if(c.office==P4Office::head_of_state||
              c.office==P4Office::local_head) {
        // Legacy unnamed-head saves did not have POP-anchored nominations.
        bool stopped=false;
        for(auto it=p4_history_.rbegin();it!=p4_history_.rend();++it) {
            if(it->contest_id!=contest_id)continue;
            if(it->office_holder==candidate) {
                ++cumulative;
                if(!stopped)++consecutive;
            } else stopped=true;
        }
    }
    return (rule.max_lifetime_terms==0||cumulative<rule.max_lifetime_terms)&&
           (rule.max_consecutive_terms==0||
            consecutive<rule.max_consecutive_terms);
}

inline void IntegratedWorld::p2_sync_country_rules(
        const std::string& country,
        const std::vector<P2ElectionTransition>& transitions,
        std::int64_t today) {
    std::map<std::string,P2ElectionTransition> prescribed;
    std::set<std::string> applied;
    for(const auto& transition:transitions) {
        const auto it=p4_contests_.find(transition.contest_id);
        ensure(it!=p4_contests_.end()&&it->second.country==country&&
               !prescribed.contains(transition.contest_id)&&
               static_cast<unsigned>(transition.mode)<=
                   static_cast<unsigned>(P2TermTransition::incumbent_as_well),
               "P2 transition refers to unknown or duplicate elected office");
        if(transition.mode==P2TermTransition::incumbent_as_well)
            ensure(transition.next_poll_day>today,
                   "P2 cannot retroactively rewrite incumbent election");
        else ensure(transition.next_poll_day==-1,
                    "P2 future-only term change must preserve current poll");
        prescribed.emplace(transition.contest_id,transition);
    }
    for(auto& [id,contest]:p4_contests_) {
        if(contest.country!=country)continue;
        const auto before=p2_term(id);
        if(before.has_legal_term) {
            if(prescribed.contains(id))applied.insert(id);
            int best_rank=1000;
            std::string source_law;
            for(const auto& [slot,law_id]:p2_active_base_) {
                (void)slot;
                const auto& law=p2_laws_.at(law_id);
                if(law.country!=country)continue;
                bool supplies_office=false;
                const auto inspect=[&](const std::vector<P2Clause>& rules) {
                    for(const auto& rule:rules)
                        if(rule.target==id&&
                           (rule.kind==P2ClauseKind::term_months||
                            rule.kind==P2ClauseKind::term_cycle_mode||
                            rule.kind==P2ClauseKind::dissolution_authorized))
                            supplies_office=true;
                };
                inspect(law.clauses);
                for(const auto& amendment_id:p2_active_amendments_)
                    if(p2_amendments_.at(amendment_id).law_id==law_id)
                        inspect(p2_amendments_.at(amendment_id).clauses);
                if(supplies_office&&static_cast<int>(law.tier)<best_rank) {
                    best_rank=static_cast<int>(law.tier);
                    source_law=law_id;
                }
            }
            if(!source_law.empty())contest.law_id=source_law;
            contest.term_months=before.term_months;
            contest.term_mode=before.cycle_mode;
            contest.dissolution_authorized=before.dissolution_authorized;
            // P4 owns historical office tenure and scheduled poll dates;
            // P2 only changes a poll if the act explicitly applies to incumbents.
            if(const auto tr=prescribed.find(id);
               tr!=prescribed.end()&&
               tr->second.mode==P2TermTransition::incumbent_as_well) {
                ensure(!contest.dissolved,
                       "P2 incumbent term override conflicts with pending dissolution");
                auto day=p4_due_days_.find(contest.next_poll_day);
                if(day!=p4_due_days_.end()) {
                    day->second.erase(id);
                    if(day->second.empty())p4_due_days_.erase(day);
                }
                contest.next_poll_day=tr->second.next_poll_day;
                contest.regular_poll_day=tr->second.next_poll_day;
                p4_due_days_[contest.next_poll_day].insert(id);
            }
        }
    }
    ensure(applied.size()==prescribed.size(),
           "P2 incumbent transition must be grounded in active term law");
}

inline void IntegratedWorld::p2_submit_bill(P2Bill bill,std::int64_t today) {
    require_stage({9});
    ensure(today>=0&&!bill.id.empty()&&!bill.country.empty()&&
           bill.proposer==governments_.at(bill.country).prime_minister&&
           !bill.legal_basis.empty()&&
           bill.introduced_day==-1&&bill.promulgated_day==-1&&
           bill.status==P2BillStatus::submitted&&
           (bill.effective_day==-1||bill.effective_day>today)&&
           !p2_bills_.contains(bill.id),
           "P2 unauthorized, retroactive or duplicate bill");
    ensure(static_cast<unsigned>(bill.action)<=
               static_cast<unsigned>(P2BillAction::repeal_base),
           "P2 invalid legal bill operation");
    std::set<std::string> affected_contests;
    for(const auto& transition:bill.transitions) {
        const auto contest=p4_contests_.find(transition.contest_id);
        ensure(contest!=p4_contests_.end()&&
               contest->second.country==bill.country&&
               affected_contests.insert(transition.contest_id).second&&
               static_cast<unsigned>(transition.mode)<=
                   static_cast<unsigned>(P2TermTransition::incumbent_as_well),
               "P2 bill has invalid or duplicate elected office transition");
        if(transition.mode==P2TermTransition::incumbent_as_well)
            ensure(transition.next_poll_day>bill.effective_day,
                   "P2 incumbent term cannot expire before law effectiveness");
        else ensure(transition.next_poll_day==-1,
                    "P2 future-only law must preserve incumbent election");
    }
    if(bill.action==P2BillAction::enact_or_replace_base) {
        p2_validate_base(bill.proposed_base);
        p2_validate_constitution_choice(bill.proposed_base);
        ensure(bill.proposed_base.country==bill.country&&
               !p2_laws_.contains(bill.proposed_base.id),
               "P2 replacement law must belong to one real sovereign");
        for(const auto& [id,previous]:p2_bills_) {
            (void)id;
            ensure((previous.status==P2BillStatus::effective||
                     previous.status==P2BillStatus::rejected)||
                   previous.action!=P2BillAction::enact_or_replace_base||
                   std::make_tuple(previous.proposed_base.country,
                                   previous.proposed_base.domain,
                                   previous.proposed_base.scope)!=
                       std::make_tuple(bill.proposed_base.country,
                                       bill.proposed_base.domain,
                                       bill.proposed_base.scope),
                   "P2 simultaneous replacement bills for one legal slot");
        }
    } else {
        const auto target=(bill.action==P2BillAction::attach_amendment)?
            bill.proposed_amendment.law_id:bill.target_id;
        ensure(!target.empty(),"P2 law amendment or repeal needs existing target");
        if(bill.action==P2BillAction::attach_amendment) {
            ensure(!p2_amendments_.contains(bill.proposed_amendment.id)&&
                   p2_is_active_base(target)&&
                   p2_laws_.at(target).country==bill.country,
                   "P2 cannot attach amendment to inactive foreign law");
            p2_check_amendment(bill.proposed_amendment);
        } else if(bill.action==P2BillAction::repeal_amendment) {
            ensure(p2_amendment_active(target)&&
                   p2_laws_.at(p2_amendments_.at(target).law_id).country==
                       bill.country,
                   "P2 may repeal only an active national amendment");
        } else {
            ensure(p2_is_active_base(target)&&
                   p2_laws_.at(target).country==bill.country,
                   "P2 cannot repeal inactive or foreign base law");
            const auto& domain=p2_laws_.at(target).domain;
            ensure(domain!="governance_principles"&&
                   domain!="distribution_of_power"&&
                   domain!="legislature_structure",
                   "P2 fundamental constitutional law must be replaced, not removed");
        }
    }
    bill.introduced_day=today;
    const auto id=bill.id;
    if(bill.action==P2BillAction::enact_or_replace_base)
        p2_negotiation_watch_.insert(id);
    const auto key=bill.country+":"+
        std::to_string(static_cast<int>(bill.action))+":"+
        (bill.action==P2BillAction::enact_or_replace_base?
           bill.proposed_base.domain+":"+bill.proposed_base.scope+":"+
           bill.proposed_base.base_choice:
         bill.action==P2BillAction::attach_amendment?
           bill.proposed_amendment.id:bill.target_id);
    ensure(!p2_retry_after_.contains(key)||p2_retry_after_.at(key)<=today,
           "P2 reform is under the two-year cooldown");
    const auto domain=bill.action==P2BillAction::enact_or_replace_base?
        bill.proposed_base.domain:std::string{};
    const int review_days=domain=="governance_principles"?200:
       (domain=="distribution_of_power"||domain=="economic_system"||
        domain=="slavery")?150:100;
    const auto country=bill.country;
    p2_bills_.emplace(id,std::move(bill));
    p2_enactments_.emplace(id,P2Enactment{
        id,country,key,1,0,0,0,today+review_days,-1,false});
    p2_review_due_[today+review_days].insert(id);
    events_.push_back("p2-bill-introduced");
}

inline void IntegratedWorld::p2_approve_bill(
        const std::string& bill_id,
        const std::vector<std::string>& voting_parties) {
    require_stage({9});
    auto& bill=p2_bills_.at(bill_id);
    ensure(bill.status==P2BillStatus::submitted&&
           p2_enactment_ready(bill_id)&&
           bill.proposer==governments_.at(bill.country).prime_minister,
           "P2 rejected unauthorized legislative sponsor");
    const auto order=p1_authority(bill.country);
    const auto tier=(bill.action==P2BillAction::enact_or_replace_base)?
        bill.proposed_base.tier:
        p2_laws_.at(bill.action==P2BillAction::attach_amendment?
                bill.proposed_amendment.law_id:
                bill.action==P2BillAction::repeal_base?
                bill.target_id:p2_amendments_.at(bill.target_id).law_id).tier;
    if(order.legislature_exists) {
        ensure(!p4_parliament_dissolved(bill.country),
               "P2 dissolved parliament cannot enact constitutional laws");
        const auto& legislature=legislature_for(bill.country);
        const People votes=coalition_seats(voting_parties,legislature);
        const People quorum=(tier==P2NormTier::constitution)?
            legislature.total_seats-legislature.total_seats/3:
            governments_.at(bill.country).quorum;
        ensure(votes>=quorum&&quorum>0,
               "P2 bill lacks constitutionally required parliamentary approval");
    } else {
        ensure(order.direct_executive&&voting_parties.empty(),
               "P2 no parliament and no legal direct authority");
    }
    if(p1_constitutions_.at(bill.country).legislature==
            P1LegislatureKind::bicameral) {
        ensure(p2_upper_chambers_.contains(bill.country)&&
               country_election_months_.contains(bill.country)&&
               !p2_chamber_decisions_.contains(bill.id),
               "P2 bicameral bill requires registered elected upper house");
        p2_negotiation_watch_.erase(bill.id);
        p2_chamber_decisions_[bill.id]=P2ChamberDecision{
            bill.id,p2_upper_chambers_.at(bill.country),
            country_election_months_.at(bill.country),-1,true,false};
        events_.push_back("p2-lower-chamber-approved:"+bill.id);
    } else {
        p2_negotiation_watch_.erase(bill.id);
        bill.status=P2BillStatus::approved;
        events_.push_back("p2-law-approved:"+bill.id);
    }
}

inline void IntegratedWorld::p2_promulgate_bill(
        const std::string& bill_id,const std::string& incumbent_head,
        std::int64_t today) {
    require_stage({9});
    auto& bill=p2_bills_.at(bill_id);
    ensure(bill.status==P2BillStatus::approved&&
           today>=bill.introduced_day&&
           governments_.at(bill.country).head_of_state==incumbent_head,
           "P2 invalid approval, promulgator or effective date");
    if(bill.effective_day<=today)bill.effective_day=today+1;
    bill.promulgated_day=today;
    bill.status=P2BillStatus::promulgated;
    p2_due_[bill.effective_day].insert(bill_id);
    events_.push_back("p2-bill-promulgated:"+bill_id);
}

inline bool IntegratedWorld::p2_has_due(std::int64_t day)const {
    return p2_due_.contains(day);
}

inline void IntegratedWorld::phase_time_apply_p2(std::int64_t today) {
    require_stage({0});
    const auto due=p2_due_.find(today);
    if(due==p2_due_.end())return;
    const auto bill_ids=due->second;
    for(const auto& id:bill_ids) {
        auto& bill=p2_bills_.at(id);
        ensure(bill.status==P2BillStatus::promulgated&&
               bill.effective_day==today,
               "P2 unpromulgated law cannot become effective");
        std::string law_id;
        std::string amendment_id;
        if(bill.action==P2BillAction::enact_or_replace_base) {
            const auto& next=bill.proposed_base;
            const auto slot=std::make_tuple(next.country,next.domain,next.scope);
            const auto old=p2_active_base_.find(slot);
            if(old!=p2_active_base_.end()) {
                const auto& previous=p2_laws_.at(old->second);
                for(const auto& clause:previous.clauses) {
                    if(clause.kind!=P2ClauseKind::term_months&&
                       clause.kind!=P2ClauseKind::term_cycle_mode&&
                       clause.kind!=P2ClauseKind::dissolution_authorized)continue;
                    const bool retained=std::any_of(next.clauses.begin(),
                        next.clauses.end(),[&](const P2Clause& replacement){
                            return replacement.target==clause.target&&
                                   replacement.kind==clause.kind;
                        });
                    ensure(retained,
                           "P2 replacement must specify continuing office rules");
                }
                // Prior amendments remain in the historical registry, not
                // silently attached to a different base law.
                for(auto it=p2_active_amendments_.begin();
                    it!=p2_active_amendments_.end();) {
                    if(p2_amendments_.at(*it).law_id==old->second)
                        it=p2_active_amendments_.erase(it);
                    else ++it;
                }
            }
            ensure(!p2_laws_.contains(next.id),
                   "P2 law version/identifier collision");
            law_id=next.id;
            p2_laws_.emplace(next.id,next);
            p2_active_base_[slot]=next.id;
            // The constitutional choice is part of Stage 0's same atomic
            // transaction, not a secondary calendar or title ledger.
            p2_apply_constitution_choice(next);
            if(const auto negotiated=p2_negotiated_.find(id);
               negotiated!=p2_negotiated_.end()) {
                for(const auto& accepted:negotiated->second) {
                    const auto& amendment=accepted.amendment;
                    ensure(!p2_amendments_.contains(amendment.id)&&
                           amendment.law_id==next.id,
                           "P2 negotiated provision missing its promised base");
                    p2_check_amendment(amendment);
                    p2_amendments_.emplace(amendment.id,amendment);
                    p2_active_amendments_.insert(amendment.id);
                    events_.push_back(
                        "p2-group-compromise-effective:"+accepted.demand_id);
                }
            }
        } else if(bill.action==P2BillAction::attach_amendment) {
            const auto& amendment=bill.proposed_amendment;
            ensure(p2_is_active_base(amendment.law_id)&&
                   !p2_amendments_.contains(amendment.id),
                   "P2 amendment base changed while bill awaited effectiveness");
            p2_check_amendment(amendment);
            amendment_id=amendment.id;
            law_id=amendment.law_id;
            p2_amendments_.emplace(amendment.id,amendment);
            p2_active_amendments_.insert(amendment.id);
        } else if(bill.action==P2BillAction::repeal_amendment) {
            ensure(p2_active_amendments_.contains(bill.target_id),
                   "P2 amendment already repealed");
            amendment_id=bill.target_id;
            law_id=p2_amendments_.at(bill.target_id).law_id;
            p2_active_amendments_.erase(bill.target_id);
        } else {
            ensure(p2_is_active_base(bill.target_id),
                   "P2 base law has already been replaced");
            law_id=bill.target_id;
            const auto& base=p2_laws_.at(law_id);
            for(const auto& clause:base.clauses)
                if(clause.kind==P2ClauseKind::term_months||
                   clause.kind==P2ClauseKind::term_cycle_mode||
                   clause.kind==P2ClauseKind::dissolution_authorized)
                    throw std::invalid_argument(
                        "P2 election mandate requires replacement law, not a vacuum");
            for(auto it=p2_active_amendments_.begin();
                it!=p2_active_amendments_.end();) {
                if(p2_amendments_.at(*it).law_id==law_id)
                    it=p2_active_amendments_.erase(it);
                else ++it;
            }
            p2_active_base_.erase(
                std::make_tuple(base.country,base.domain,base.scope));
        }
        p2_sync_country_rules(bill.country,bill.transitions,today);
        const auto lowest=p2_rule(bill.country,bill.country,
                                 P2ClauseKind::income_tax_min_ppm).value_or(0);
        const auto highest=p2_rule(bill.country,bill.country,
                                 P2ClauseKind::income_tax_max_ppm).value_or(1'000'000);
        ensure(lowest<=highest,
               "P2 incompatible legal national income-tax bounds");
        // Legal limits do not create revenues: tax is collected only by C.
        ensure(income_tax_ppm(bill.country)>=lowest&&
               income_tax_ppm(bill.country)<=highest,
               "P2 new tax ceiling requires explicit lawful rate adjustment");
        bill.status=P2BillStatus::effective;
        p2_history_.push_back({id,bill.country,law_id,amendment_id,
                               bill.legal_basis,bill.action,today});
        events_.push_back("p2-law-effective:"+id);
    }
    p2_due_.erase(today);
}

inline void IntegratedWorld::p2_rebuild_due_index() {
    p2_due_.clear();
    p2_negotiation_watch_.clear();
    for(const auto& [id,bill]:p2_bills_) {
        if(bill.status==P2BillStatus::promulgated)
            p2_due_[bill.effective_day].insert(id);
        if(bill.status==P2BillStatus::submitted&&
           bill.action==P2BillAction::enact_or_replace_base&&
           (!p2_chamber_decisions_.contains(id)||
            !p2_chamber_decisions_.at(id).lower_passed))
            p2_negotiation_watch_.insert(id);
    }
}

} // namespace vic25::phasec
