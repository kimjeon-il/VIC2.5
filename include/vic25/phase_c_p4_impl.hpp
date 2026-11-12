#pragma once
// Included after IntegratedWorld has been defined in phase_c.hpp.
// P4 keeps its own election calendar and non-national results; the existing
// country_elections_ remains the only current national parliamentary seat ledger.
namespace vic25::phasec {

inline void IntegratedWorld::p4_register_party(P4Party party) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P4 party registration outside valid political stage");
    ensure(!party.id.empty()&&!party.country.empty()&&
           governments_.contains(party.country)&&
           party.organization_ppm>=1&&party.organization_ppm<=1'000'000&&
           !p4_parties_.contains(party.id),
           "P4 invalid or duplicate real party");
    const auto id=party.id;
    p4_parties_.emplace(id,std::move(party));
    events_.push_back("p4-party-registered:"+id);
}

inline void IntegratedWorld::p4_set_group_affinity(
        const std::string& party,const std::string& group,
        int affinity_ppm) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P4 affiliation outside political stage");
    ensure(p4_parties_.contains(party)&&interest_groups_.contains(group)&&
           affinity_ppm>=0&&affinity_ppm<=1'000'000,
           "P4 affiliation requires real party and P3 interest group");
    const auto key=std::make_pair(party,group);
    if(affinity_ppm==0)p4_affinities_.erase(key);
    else p4_affinities_[key]=affinity_ppm;
    events_.push_back("p4-nonexclusive-group-party-affinity:"+party);
}

inline void IntegratedWorld::p4_seed_contest(P4Contest contest) {
    ensure(stage_==-1&&commit_id_==0&&
           p1_constitutions_.contains(contest.country)&&
           !contest.id.empty()&&!p4_contests_.contains(contest.id)&&
           !contest.law_id.empty()&&
           contest.term_months>=1&&contest.term_months<=240&&
           contest.turnout_ppm>=0&&contest.turnout_ppm<=1'000'000&&
           contest.next_poll_day>=0&&
           contest.regular_poll_day==contest.next_poll_day&&
           contest.last_poll_day==-1&&contest.seats>=1&&
           contest.seats<=100'000&&contest.active&&
           static_cast<unsigned>(contest.office)<=
               static_cast<unsigned>(P4Office::local_head)&&
           static_cast<unsigned>(contest.method)<=
               static_cast<unsigned>(P4BallotMethod::plurality)&&
           static_cast<unsigned>(contest.term_mode)<=
               static_cast<unsigned>(P4TermMode::fixed_cycle),
           "P4 invalid or unsourced independent election schedule");
    const auto& order=p1_constitution(contest.country);
    if(contest.office==P4Office::legislature) {
        ensure(order.legislature!=P1LegislatureKind::none&&
               (contest.jurisdiction.empty()||contest.jurisdiction=="national")&&
               contest.seats==governments_.at(contest.country).chamber_seats,
               "P4 national parliamentary contest must match real chamber");
        // Current P1 is unicameral. Do not let two contests overwrite one
        // authoritative C legislature; bicameral support needs P1/P4 expansion.
        for(const auto& [id,existing]:p4_contests_) {
            (void)id;
            ensure(existing.country!=contest.country||
                   existing.office!=P4Office::legislature,
                   "P4 duplicate national chamber without bicameral authority");
        }
    } else if(contest.office==P4Office::upper_chamber) {
        ensure(order.legislature==P1LegislatureKind::bicameral&&
               (contest.jurisdiction.empty()||contest.jurisdiction=="national"),
               "P4 upper election requires bicameral constitutional legislature");
        for(const auto& [other_id,other]:p4_contests_) {
            (void)other_id;
            ensure(other.country!=contest.country||
                   other.office!=P4Office::upper_chamber,
                   "P4 duplicate upper chamber for one national constitution");
        }
    } else if(contest.office==P4Office::local ||
              contest.office==P4Office::local_head) {
        ensure(sovereignty_.contains(contest.jurisdiction)&&
               sovereignty_.at(contest.jurisdiction)==contest.country,
               "P4 local election must use existing sovereign region");
        if(contest.office==P4Office::local_head)
            ensure(contest.seats==1&&
                   contest.method==P4BallotMethod::plurality,
                   "P4 local head has one elected office");
    } else {
        ensure(contest.seats==1&&
               contest.method==P4BallotMethod::plurality&&
               (order.governance==P1GovernancePrinciple::presidential_republic||
                order.governance==P1GovernancePrinciple::parliamentary_republic||
                order.governance==P1GovernancePrinciple::council_republic),
               "P4 elected head requires republican constitutional office");
    }
    if(contest.method==P4BallotMethod::plurality)
        ensure(contest.seats==1,
               "P4 plurality needs one seat per registered district");
    const auto id=contest.id;
    p4_due_days_[contest.next_poll_day].insert(id);
    p4_contests_.emplace(id,std::move(contest));
}

inline std::vector<std::string> IntegratedWorld::p4_due_contests(
        std::int64_t day)const {
    const auto it=p4_due_days_.find(day);
    if(it==p4_due_days_.end())return {};
    return {it->second.begin(),it->second.end()};
}

inline bool IntegratedWorld::p4_has_due_amendment(
        std::int64_t day)const {
    return p4_amendments_.contains(day);
}

inline bool IntegratedWorld::p4_parliament_dissolved(
        const std::string& country)const {
    for(const auto& [id,contest]:p4_contests_) {
        (void)id;
        if(contest.country==country&&
           contest.office==P4Office::legislature&&contest.dissolved)
            return true;
    }
    return false;
}

inline People IntegratedWorld::p4_eligible(
        const std::string& contest_id)const {
    const auto& contest=p4_contests_.at(contest_id);
    People eligible=0;
    for(const auto& [slice,row]:people_.slices()) {
        const auto& d=people_.demographic(row.key.demographic);
        const auto jurisdiction=sovereignty_.find(d.region);
        if(jurisdiction==sovereignty_.end()||
           jurisdiction->second!=contest.country||
           (contest.office==P4Office::local&&
            d.region!=contest.jurisdiction)||
           d.citizenship!="citizen"||
           d.service==phaseb::Service::active||
           d.service==phaseb::Service::pow||
           d.service==phaseb::Service::missing||
           (d.sex==phaseb::Sex::female&&
            p2_rule(contest.country,contest.country,
                P2ClauseKind::female_suffrage_allowed).value_or(1)==0)||
           !p2_citizenship_vote_allowed(contest.country,slice)||
           people_.age_months_slice(slice)<
               static_cast<std::int64_t>(p2_voting_age(contest.country))*12)continue;
        eligible=phaseb::add_checked(eligible,row.count);
    }
    return eligible;
}

inline People IntegratedWorld::p4_scaled(People n,People numerator,
                                         People denominator) {
    ensure(n>=0&&numerator>=0&&denominator>0&&numerator<=denominator,
           "P4 invalid fixed-point proportional rate");
    return phaseb::add_checked(
        phaseb::mul_checked(n/denominator,numerator),
        phaseb::mul_checked(n%denominator,numerator)/denominator);
}

inline P4Ballot IntegratedWorld::p4_autonomous_ballot(
        const std::string& contest_id)const {
    const auto& contest=p4_contests_.at(contest_id);
    P4Ballot ballot;
    std::vector<const P4Party*> candidates;
    for(const auto& [id,party]:p4_parties_) {
        (void)id;
        const auto nominees=p4_nominations_.find({contest_id,party.id});
        const std::string head=(nominees!=p4_nominations_.end()&&
                                !nominees->second.empty())?
            nominees->second.front():party.head_candidate;
        if(party.country==contest.country&&party.active&&party.registered&&
           (contest.office!=P4Office::head_of_state&&
            contest.office!=P4Office::local_head||
            (!head.empty()&&p2_candidate_eligible(contest_id,head))))
            candidates.push_back(&party);
    }
    ensure(!candidates.empty()&&candidates.size()<=128,
           "P4 requires legally registered real competing parties");
    if(p1_constitution(contest.country).power==P1PowerDistribution::single_party)
        ensure(candidates.size()==1,"P4 single-party law forbids multiple candidates");
    for(const auto* party:candidates)
        ballot.votes.emplace(party->id,0);
    // Build a single index per election, not a world-wide membership scan
    // for every POP and party (the election is event-driven, not daily).
    std::map<Id,std::vector<std::pair<std::string,People>>> slice_groups;
    for(const auto& [membership,n]:membership_)
        if(n>0)slice_groups[membership.first].push_back({
            membership.second,n});
    for(const auto& [slice,row]:people_.slices()) {
        const auto& d=people_.demographic(row.key.demographic);
        const auto jurisdiction=sovereignty_.find(d.region);
        if(jurisdiction==sovereignty_.end()||
           jurisdiction->second!=contest.country||
           (contest.office==P4Office::local&&
            d.region!=contest.jurisdiction)||
           d.citizenship!="citizen"||
           d.service==phaseb::Service::active||
           d.service==phaseb::Service::pow||
           d.service==phaseb::Service::missing||
           (d.sex==phaseb::Sex::female&&
            p2_rule(contest.country,contest.country,
                P2ClauseKind::female_suffrage_allowed).value_or(1)==0)||
           !p2_citizenship_vote_allowed(contest.country,slice)||
           people_.age_months_slice(slice)<
               static_cast<std::int64_t>(p2_voting_age(contest.country))*12)continue;
        ballot.eligible=phaseb::add_checked(ballot.eligible,row.count);
        const People participating=p4_scaled(row.count,contest.turnout_ppm,1'000'000);
        ballot.registered=phaseb::add_checked(ballot.registered,row.count);
        ballot.cast=phaseb::add_checked(ballot.cast,participating);
        if(participating==0)continue;
        std::vector<std::pair<std::string,People>> weights;
        People weight_sum=0;
        for(const auto* party:candidates) {
            People weight=party->organization_ppm;
            const auto groups=slice_groups.find(slice);
            if(groups!=slice_groups.end())
                for(const auto& [group,n]:groups->second) {
                    const auto affinity=p4_affinities_.find({party->id,group});
                    if(affinity==p4_affinities_.end())continue;
                    // Interests change relative preference, never headcount.
                    const People weighted=p4_scaled(affinity->second,n,row.count);
                    weight=std::min<People>(5'000'000,
                        phaseb::add_checked(weight,weighted));
                }
            // Persistent movements and issue salience affect relative
            // party selection, but never create votes or enforce 1 IG=1 party.
            const auto opinion=p3_attitudes_.find(slice);
            if(opinion!=p3_attitudes_.end()) {
                for(const auto& [movement_id,movement]:p3_movements_) {
                    (void)movement_id;
                    if(!movement.active||movement.country!=contest.country||
                       (!movement.region.empty()&&movement.region!=d.region)||
                       std::find(movement.parties.begin(),
                                 movement.parties.end(),party->id)==
                                 movement.parties.end())continue;
                    const auto stance=opinion->second.issues.find(
                        movement.issue_domain);
                    if(stance==opinion->second.issues.end()||
                       stance->second.preference_ppm<=0)continue;
                    const auto interest=static_cast<People>(
                        stance->second.preference_ppm)*
                        stance->second.importance_ppm/1'000'000;
                    weight=std::min<People>(5'000'000,
                        phaseb::add_checked(weight,interest/4));
                }
            }
            // Official IG affiliation and campaign momentum change a
            // real voter's preference weight, never electorate size.
            if(const auto campaign=p4_campaigns_.find({contest_id,party->id});
               campaign!=p4_campaigns_.end())
                weight=std::min<People>(5'000'000,
                    phaseb::add_checked(weight,
                        p4_scaled(party->organization_ppm,
                                  campaign->second.momentum_ppm,1'000'000)));
            if(const auto groups=slice_groups.find(slice);
               groups!=slice_groups.end())
                for(const auto& [group,n]:groups->second) {
                    const auto official=p4_official_affiliations_.find(
                        {contest.country,group});
                    if(official!=p4_official_affiliations_.end()&&
                       official->second.party==party->id)
                        weight=std::min<People>(5'000'000,
                            phaseb::add_checked(weight,
                                p4_scaled(100'000,n,row.count)));
                }
            const auto character=p1_characters_.find(party->head_candidate);
            if(character!=p1_characters_.end()&&
               p1_character_available(character->first)&&
               character->second.country==contest.country) {
                const auto favor=character->second.popularity_ppm;
                const auto adjustment=static_cast<People>(
                   party->organization_ppm)*favor/1'000'000;
                weight=std::clamp<People>(weight+adjustment,1,5'000'000);
            }
            weights.emplace_back(party->id,weight);
            weight_sum=phaseb::add_checked(weight_sum,weight);
        }
        ensure(weight_sum>0&&weight_sum<=640'000'000,
               "P4 election weight overflow or missing party");
        struct Remainder {std::string id;People remainder{};};
        std::vector<Remainder> rem;
        People allocated=0;
        for(const auto& [id,weight]:weights) {
            const People share=phaseb::add_checked(
                phaseb::mul_checked(participating/weight_sum,weight),
                phaseb::mul_checked(participating%weight_sum,weight)/
                    weight_sum);
            const People residue=phaseb::mul_checked(
                participating%weight_sum,weight)%weight_sum;
            ballot.votes.at(id)=phaseb::add_checked(ballot.votes.at(id),share);
            allocated=phaseb::add_checked(allocated,share);
            rem.push_back({id,residue});
        }
        std::sort(rem.begin(),rem.end(),
            [](const auto& a,const auto& b) {
                if(a.remainder!=b.remainder)return a.remainder>b.remainder;
                return a.id<b.id;
            });
        ensure(participating-allocated>=0 &&
               participating-allocated<=static_cast<People>(rem.size()),
               "P4 proportional allocation failed");
        for(People i=0;i<participating-allocated;++i)
            ++ballot.votes.at(rem[static_cast<std::size_t>(i)].id);
    }
    return ballot;
}

inline void IntegratedWorld::p4_hold_contest(
        const std::string& id,std::int64_t today,
        std::int64_t next_regular_day,P4Ballot ballot) {
    require_stage({9});
    auto& contest=p4_contests_.at(id);
    ensure(contest.active&&contest.next_poll_day==today&&
           next_regular_day>today&&p4_due_days_.contains(today)&&
           p4_due_days_.at(today).contains(id),
           "P4 election outside actual valid term");
    const auto eligible=p4_eligible(id);
    ensure(ballot.eligible==eligible&&eligible>=ballot.registered&&
           ballot.registered>=ballot.cast&&
           ballot.cast>=ballot.invalid&&ballot.invalid>=0&&
           ballot.registered>0&&ballot.cast>ballot.invalid,
           "P4 ballot exceeds eligible actual POP");
    const People valid=ballot.cast-ballot.invalid;
    People sum=0;
    const auto& order=p1_constitution(contest.country);
    std::size_t permitted=0;
    for(const auto& [pid,party]:p4_parties_)
        if(party.country==contest.country&&party.registered&&party.active) {
            const auto nominees=p4_nominations_.find({id,party.id});
            const std::string head=(nominees!=p4_nominations_.end()&&
                                    !nominees->second.empty())?
                nominees->second.front():party.head_candidate;
            if(contest.office!=P4Office::head_of_state&&
               contest.office!=P4Office::local_head||
               (!head.empty()&&p2_candidate_eligible(id,head)))
                ++permitted;
        }
    if(order.power==P1PowerDistribution::single_party)
        ensure(permitted==1,"P4 unlawful multi-party election");
    for(const auto& [party,votes]:ballot.votes) {
        ensure(votes>=0&&p4_parties_.contains(party)&&
               p4_parties_.at(party).country==contest.country&&
               p4_parties_.at(party).active&&p4_parties_.at(party).registered&&
               (contest.office!=P4Office::head_of_state&&
                contest.office!=P4Office::local_head||
                ([&]{
                    const auto nominees=p4_nominations_.find({id,party});
                    const std::string head=(nominees!=p4_nominations_.end()&&
                                            !nominees->second.empty())?
                        nominees->second.front():
                        p4_parties_.at(party).head_candidate;
                    return !head.empty()&&p2_candidate_eligible(id,head);
                }())),
               "P4 ballots for unregistered party or foreign contest");
        sum=phaseb::add_checked(sum,votes);
    }
    ensure(sum==valid&&sum>0&&
           ballot.votes.size()==permitted,
           "P4 ballot must conserve valid votes for every party");
    P4Result result;
    result.contest_id=id;result.law_id=contest.law_id;result.day=today;
    result.eligible=eligible;result.registered=ballot.registered;
    result.cast=ballot.cast;result.invalid=ballot.invalid;
    result.valid=valid;result.votes=std::move(ballot.votes);
    result.early=contest.dissolved;
    for(const auto& [party,n]:result.votes) {
        (void)n;
        result.seats.emplace(party,0);
    }
    for(People seat=0;seat<contest.seats;++seat) {
        std::string chosen;
        for(const auto& [party,n]:result.votes) {
            if(chosen.empty()){chosen=party;continue;}
            const int comparison=(contest.method==P4BallotMethod::plurality)?
                compare_nonnegative_ratios(
                    static_cast<std::uint64_t>(n),1,
                    static_cast<std::uint64_t>(result.votes.at(chosen)),1):
                compare_nonnegative_ratios(
                    static_cast<std::uint64_t>(n),
                    static_cast<std::uint64_t>(result.seats.at(party)+1),
                    static_cast<std::uint64_t>(result.votes.at(chosen)),
                    static_cast<std::uint64_t>(result.seats.at(chosen)+1));
            if(comparison>0||(comparison==0&&party<chosen))chosen=party;
        }
        ensure(!chosen.empty(),"P4 election cannot allocate empty ballot");
        ++result.seats.at(chosen);
    }
    for(const auto& [party,seats]:result.seats) {
        if(result.winning_party.empty()||
           seats>result.seats.at(result.winning_party)||
           (seats==result.seats.at(result.winning_party)&&
            result.votes.at(party)>result.votes.at(result.winning_party)))
            result.winning_party=party;
    }
    if(contest.office==P4Office::legislature) {
        // Reuse the authoritative national seat result, not a second
        // mutable parliamentary legislature. Exact method is identical.
        if(contest.method==P4BallotMethod::list_dhondt ||
           contest.seats==1) {
            hold_country_election(contest.country,result.registered,
                                  result.invalid,result.votes);
        }
        const auto& authoritative=country_elections_.at(contest.country);
        ensure(authoritative.seats==result.seats,
               "P4 and C must agree on the parliamentary seat allocation");
        if(p1_authority(contest.country).assembly_confidence_required)
            p1_coalitions_.at(contest.country).caretaker=true;
    } else if(contest.office==P4Office::head_of_state) {
        const auto& chosen=p4_parties_.at(result.winning_party);
        const auto nomination=p4_nominations_.find({id,result.winning_party});
        result.office_holder=(nomination!=p4_nominations_.end()&&
                              !nomination->second.empty())?
            nomination->second.front():chosen.head_candidate;
        ensure(!result.office_holder.empty(),"P4 election winner lacks real nominee");
        auto& next=p1_pending_[today+1][contest.country];
        ensure(!next.replace_head,"P4 duplicate national head of state transition");
        if(next.country.empty()) {
            next.country=contest.country;
            next.coalition=p1_coalitions_.at(contest.country);
            next.legal_basis=contest.law_id;
        }
        next.replace_head=true;
        next.new_head=result.office_holder;
    }
    if(contest.office==P4Office::local_head) {
        const auto& chosen=p4_parties_.at(result.winning_party);
        const auto nomination=p4_nominations_.find({id,result.winning_party});
        result.office_holder=(nomination!=p4_nominations_.end()&&
                              !nomination->second.empty())?
                            nomination->second.front():chosen.head_candidate;
        ensure(!result.office_holder.empty(),
               "P4 elected local head needs named nominee");
    }
    p4_record_elected_persons(result);
    // History is immutable audit. For national parliament, country_elections_
    // remains authoritative for current seats. Local/head results are held
    // separately by contest ID, never used as national legislative votes.
    p4_history_.push_back(result);
    if(contest.office!=P4Office::legislature)
        p4_nonparliament_results_[id]=std::move(result);
    const std::int64_t upcoming=(contest.dissolved&&
        contest.term_mode==P4TermMode::fixed_cycle)?
        contest.regular_poll_day:next_regular_day;
    ensure(upcoming>today,"P4 early election must have lawful next cycle");
    p4_due_days_.at(today).erase(id);
    if(p4_due_days_.at(today).empty())p4_due_days_.erase(today);
    contest.last_poll_day=today;
    contest.next_poll_day=upcoming;
    contest.regular_poll_day=upcoming;
    contest.dissolved=false;
    p4_due_days_[upcoming].insert(id);
    events_.push_back("p4-contest-counted:"+id);
}

inline void IntegratedWorld::p4_dissolve(
        const std::string& id,const std::string& incumbent_head,
        const std::string& legal_basis,std::int64_t today,
        std::int64_t early_poll_day) {
    require_stage({9});
    auto& contest=p4_contests_.at(id);
    const auto legal_term=p2_term(id);
    ensure((legal_term.dissolution_max_delay_days==0||
            (early_poll_day>today&&
             early_poll_day-today<=legal_term.dissolution_max_delay_days))&&
           contest.office==P4Office::legislature&&
           contest.dissolution_authorized&&
           !contest.dissolved&&contest.active&&
           !legal_basis.empty()&&
           p1_constitutions_.contains(contest.country)&&
           p1_authority(contest.country).legislature_exists&&
           governments_.at(contest.country).head_of_state==incumbent_head&&
           today>=0&&early_poll_day>today&&
           early_poll_day<contest.next_poll_day,
           "P4 unlawful dissolution or election date");
    auto& old=p4_due_days_.at(contest.next_poll_day);
    old.erase(id);
    if(old.empty())p4_due_days_.erase(contest.next_poll_day);
    contest.next_poll_day=early_poll_day;
    contest.dissolved=true;
    if(p1_authority(contest.country).assembly_confidence_required)
        p1_coalitions_.at(contest.country).caretaker=true;
    p4_due_days_[early_poll_day].insert(id);
    events_.push_back("p4-parliament-dissolved:"+id);
}

inline void IntegratedWorld::p4_schedule_term_amendment(
        P4TermAmendment law,const std::string& current_prime,
        const std::vector<std::string>& supporting_parties,
        std::int64_t today) {
    require_stage({9});
    const auto& contest=p4_contests_.at(law.contest_id);
    for(const auto& [slot,law_id]:p2_active_base_) {
        (void)slot;
        const auto& base=p2_laws_.at(law_id);
        bool owns_term=false;
        const auto checks=[&](const std::vector<P2Clause>& clauses){
            for(const auto& c:clauses)
                if(c.target==law.contest_id&&
                   (c.kind==P2ClauseKind::term_months||
                    c.kind==P2ClauseKind::term_cycle_mode))
                    owns_term=true;
        };
        checks(base.clauses);
        for(const auto& id:p2_active_amendments_)
            if(p2_amendments_.at(id).law_id==law_id)
                checks(p2_amendments_.at(id).clauses);
        ensure(base.country!=contest.country||!owns_term,
               "P4 law superseded by P2; use a P2 legislative bill");
    }
    ensure(today>=0&&law.effective_day>today&&
           law.term_months>=1&&law.term_months<=240&&
           law.next_poll_day>law.effective_day&&
           !law.law_id.empty()&&
           static_cast<unsigned>(law.term_mode)<=
               static_cast<unsigned>(P4TermMode::fixed_cycle)&&
           governments_.at(contest.country).prime_minister==current_prime,
           "P4 invalid constitutional term amendment");
    const auto formation=p1_authority(contest.country);
    if(!formation.direct_executive) {
        const auto& election=country_elections_.at(contest.country);
        ensure(coalition_seats(supporting_parties,election)>=
                   governments_.at(contest.country).quorum&&
               !p4_parliament_dissolved(contest.country),
               "P4 constitutional amendment lacks lawful parliamentary quorum");
    } else {
        ensure(supporting_parties.empty(),
               "P4 direct executive may not fabricate parliamentary votes");
    }
    for(const auto& [when,amendments]:p4_amendments_)
        for(const auto& pending:amendments)
            ensure(pending.contest_id!=law.contest_id,
                   "P4 competing constitutional term amendment");
    p4_amendments_[law.effective_day].push_back(std::move(law));
    events_.push_back("p4-term-amendment-lawfully-scheduled");
}

inline void IntegratedWorld::phase_time_apply_p4_amendments(
        std::int64_t today) {
    require_stage({0});
    const auto it=p4_amendments_.find(today);
    if(it==p4_amendments_.end())return;
    for(const auto& law:it->second) {
        auto& contest=p4_contests_.at(law.contest_id);
        contest.law_id=law.law_id;
        contest.term_months=law.term_months;
        contest.term_mode=law.term_mode;
        if(law.applies_to_incumbent) {
            auto& old=p4_due_days_.at(contest.next_poll_day);
            old.erase(contest.id);
            if(old.empty())p4_due_days_.erase(contest.next_poll_day);
            contest.next_poll_day=law.next_poll_day;
            contest.regular_poll_day=law.next_poll_day;
            contest.dissolved=false;
            p4_due_days_[law.next_poll_day].insert(contest.id);
        }
        events_.push_back("p4-constitutional-election-calendar-effective:"+contest.id);
    }
    p4_amendments_.erase(it);
}

inline void IntegratedWorld::p4_rebuild_due_index() {
    p4_due_days_.clear();
    for(const auto& [id,contest]:p4_contests_)
        if(contest.active)p4_due_days_[contest.next_poll_day].insert(id);
}

inline P4CompetitionView IntegratedWorld::p4_competition(
        const std::string& country)const {
    P4CompetitionView result;
    std::vector<const P4Result*> recent;
    for(auto it=p4_history_.rbegin();it!=p4_history_.rend();++it) {
        const auto& contest=p4_contests_.at(it->contest_id);
        if(contest.country==country&&
           contest.office==P4Office::legislature) {
            recent.push_back(&*it);
            if(recent.size()==3)break;
        }
    }
    result.observed_elections=static_cast<std::int32_t>(recent.size());
    if(recent.empty())return result;
    const auto& latest=*recent.front();
    People chamber=0,top=0,second=0;
    std::string leading_party;
    People concentration=0;
    for(const auto& [party,seats]:latest.seats) {
        chamber=phaseb::add_checked(chamber,seats);
        if(seats>top) {second=top;top=seats;leading_party=party;}
        else if(seats>second)second=seats;
    }
    if(chamber==0)return result;
    for(const auto& [party,seats]:latest.seats) {
        (void)party;
        const auto basis=p4_scaled(10'000,seats,chamber);
        concentration=phaseb::add_checked(concentration,
            phaseb::mul_checked(basis,basis));
    }
    if(concentration>0)
        result.effective_parties_x100=
            static_cast<std::int32_t>(10'000'000'000LL/concentration);
    result.leading_share_percent=p1_percent(top,chamber);
    result.two_leaders_share_percent=
        p1_percent(phaseb::add_checked(top,second),chamber);
    if(p1_constitution(country).power==P1PowerDistribution::single_party) {
        result.pattern=P4Competition::legally_single_party;
        return result;
    }
    bool sustained=recent.size()>=2&&result.leading_share_percent>=55;
    for(const auto* previous:recent) {
        People total=0;
        for(const auto& [party,seats]:previous->seats) {
            (void)party;
            total=phaseb::add_checked(total,seats);
        }
        sustained=sustained&&total>0&&
            previous->seats.contains(leading_party)&&
            previous->seats.at(leading_party)>total/2;
    }
    if(sustained)result.pattern=P4Competition::dominant_party;
    else if(result.two_leaders_share_percent>=85&&
            latest.seats.size()>=2)
        result.pattern=P4Competition::two_party;
    else result.pattern=P4Competition::multi_party;
    return result;
}

} // namespace vic25::phasec
