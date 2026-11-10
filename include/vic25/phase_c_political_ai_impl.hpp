#pragma once
// National political AI issues ordinary lawful P1/P2 orders. It never
// creates votes, changes laws directly, or invents legislative templates.
namespace vic25::phasec {
inline void IntegratedWorld::p1_mark_player_country(
        const std::string& country,bool controlled) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "political player control requires a lawful stage");
    ensure(governments_.contains(country),
           "political player country must have a real government");
    if(controlled)p1_player_countries_.insert(country);
    else p1_player_countries_.erase(country);
}
inline bool IntegratedWorld::p1_has_ai_review(std::int64_t today)const {
    for(const auto& [country,g]:governments_) {
        (void)g;
        if(p1_player_countries_.contains(country)||
           !p1_constitutions_.contains(country))continue;
        const auto it=p1_political_agendas_.find(country);
        if(it==p1_political_agendas_.end()||it->second.next_review_day<=today)
            return true;
        for(const auto& [id,c]:p4_contests_)
            if(c.country==country&&c.office==P4Office::legislature&&
               c.last_poll_day>it->second.last_election_day)
                return true;
    }
    return false;
}
inline void IntegratedWorld::p1_autonomous_politics(std::int64_t today) {
    require_stage({9});
    for(const auto& [country,g]:governments_) {
        if(p1_player_countries_.contains(country)||
           !p1_constitutions_.contains(country))continue;
        auto& agenda=p1_political_agendas_[country];
        if(agenda.country.empty())agenda.country=country;
        bool election_changed=false;
        std::int64_t latest_election=agenda.last_election_day;
        for(const auto& [id,c]:p4_contests_)
            if(c.country==country&&c.office==P4Office::legislature&&
               c.last_poll_day>latest_election) {
                election_changed=true;
                latest_election=c.last_poll_day;
            }
        if(agenda.next_review_day>today&&!election_changed)continue;
        agenda.last_review_day=today;
        agenda.last_election_day=latest_election;
        const auto order=p1_authority(country);
        // AI may initiate a single real-bloc confidence vote when its cabinet
        // has already lost the recorded chamber quorum. No individual MPs
        // or extra parliamentary point ledger are created.
        if(order.assembly_confidence_required&&
           !p1_coalitions_.at(country).caretaker&&
           country_elections_.contains(country)&&
           !p4_parliament_dissolved(country)) {
            const auto& election=country_elections_.at(country);
            const auto& ruling=p1_coalitions_.at(country).ruling_parties;
            People loyal_seats=0,opposition_seats=0;
            std::vector<std::string> opposition;
            for(const auto& [party,seats]:election.seats) {
                if(std::find(ruling.begin(),ruling.end(),party)!=ruling.end())
                    loyal_seats=phaseb::add_checked(loyal_seats,seats);
                else if(seats>0) {
                    opposition.push_back(party);
                    opposition_seats=phaseb::add_checked(opposition_seats,seats);
                }
            }
            if(loyal_seats<g.quorum&&
               opposition_seats>election.total_seats/2) {
                p1_vote_no_confidence(country,opposition,today);
                events_.push_back("p1-ai-recorded-no-confidence:"+country);
            }
        }
        // Respond to a real warning using an affordable P1 mandate, rather
        // than increasing governmental clout or manufacturing loyalists.
        for(const auto& [crisis_id,crisis]:p5_crises_) {
            if(crisis.country!=country||
               crisis.stage!=P5CrisisStage::warning||
               !p3_movements_.contains(crisis.movement)||
               !p3_movements_.at(crisis.movement).active)continue;
            bool ordered=false;
            for(const auto& [id,m]:p1_mandates_) {
                (void)id;
                if(m.country==country&&m.target==crisis.movement&&
                   m.active&&(m.kind==P1PoliticalMandateKind::bolster_movement||
                             m.kind==P1PoliticalMandateKind::suppress_movement))
                    ordered=true;
            }
            if(ordered||p1_authority_budget(country).available<200)continue;
            const auto& movement=p3_movements_.at(crisis.movement);
            bool governing_support=false;
            for(const auto& group:movement.groups) {
                const auto& ruling=p1_coalitions_.at(country).ruling_groups;
                if(std::find(ruling.begin(),ruling.end(),group)!=ruling.end())
                    governing_support=true;
            }
            P1PoliticalMandate mandate;
            mandate.id="P1:AI:crisis:"+crisis_id;
            mandate.country=country;mandate.target=crisis.movement;
            mandate.kind=governing_support?
                P1PoliticalMandateKind::bolster_movement:
                P1PoliticalMandateKind::suppress_movement;
            if(!p1_mandates_.contains(mandate.id)) {
                p1_activate_mandate(std::move(mandate),today);
                events_.push_back("p1-ai-lawful-political-movement-response:"+crisis_id);
            }
        }
        if(order.assembly_confidence_required&&
           country_elections_.contains(country)&&
           !p4_parliament_dissolved(country)&&
           (p1_coalitions_.at(country).caretaker||
            p1_legitimacy(country).score<30||election_changed)&&
           !p1_pending_[today+1].contains(country)) {
            const auto& election=country_elections_.at(country);
            std::vector<std::pair<std::string,People>> parties;
            for(const auto& [id,seats]:election.seats)
                if(seats>0&&p4_parties_.contains(id)&&
                   p4_parties_.at(id).country==country&&
                   p4_parties_.at(id).active)
                    parties.push_back({id,seats});
            std::sort(parties.begin(),parties.end(),
                [](const auto& a,const auto& b){
                    return a.second!=b.second?a.second>b.second:
                           a.first<b.first;
                });
            P1Coalition coalition;
            People seats=0;
            for(const auto& [id,n]:parties) {
                if(seats>=g.quorum)break;
                coalition.ruling_parties.push_back(id);
                seats=phaseb::add_checked(seats,n);
            }
            if(seats>=g.quorum&&g.quorum>0) {
                for(const auto& [key,official]:p4_official_affiliations_)
                    if(official.country==country&&
                       std::find(coalition.ruling_parties.begin(),
                                 coalition.ruling_parties.end(),official.party)!=
                                 coalition.ruling_parties.end())
                        coalition.ruling_groups.push_back(key.second);
                std::sort(coalition.ruling_groups.begin(),
                          coalition.ruling_groups.end());
                coalition.ruling_groups.erase(std::unique(
                    coalition.ruling_groups.begin(),
                    coalition.ruling_groups.end()),coalition.ruling_groups.end());
                // An ousted cabinet cannot reappoint itself; prefer a
                // registered and POP-anchored party head as successor.
                std::string successor;
                if(!p1_lost_confidence_.contains(country)&&
                   coalition.ruling_parties==
                       p1_coalitions_.at(country).ruling_parties)
                    successor=g.prime_minister;
                if(successor.empty())
                    for(const auto& party:coalition.ruling_parties) {
                        const auto& candidate=p4_parties_.at(party).head_candidate;
                        if(!candidate.empty()&&candidate!=g.prime_minister&&
                           person_anchors_.contains(candidate)&&
                           p1_character_available(candidate)) {
                            successor=candidate;break;
                        }
                    }
                if(!successor.empty()) {
                    schedule_p1_cabinet_daily(country,g.head_of_state,
                        successor,std::move(coalition),today+1,
                        "ai-lawful-confidence-coalition");
                    events_.push_back("p1-ai-parliamentary-coalition:"+country);
                }
            }
        }
        // A passed three-phase process still requires lawful chamber votes.
        bool pending_enactment=false;
        for(const auto& [bill_id,bill]:p2_bills_) {
            if(bill.country!=country||
               (bill.status!=P2BillStatus::submitted&&
                bill.status!=P2BillStatus::approved))continue;
            pending_enactment=true;
            if(bill.status==P2BillStatus::approved) {
                if(p1_legislative_powers(country).veto!=P1VetoRule::none&&
                   p2_assents_.contains(bill_id)) {
                    const auto& assent=p2_assents_.at(bill_id);
                    if(assent.vetoed&&!assent.overridden) {
                        const auto powers=p1_legislative_powers(country);
                        if(powers.veto!=P1VetoRule::suspensive||
                           !country_elections_.contains(country)||
                           p4_parliament_dissolved(country))continue;
                        const auto& chamber=country_elections_.at(country);
                        const auto& ruling=p1_coalitions_.at(country).ruling_parties;
                        const auto required=[&](People seats)->People {
                            return static_cast<People>(
                                (static_cast<std::uint64_t>(seats)*
                                 powers.override_ppm+999'999)/1'000'000);
                        };
                        bool enough=true;
                        for(const auto& party:ruling)
                            if(!chamber.seats.contains(party))enough=false;
                        if(!enough||coalition_seats(ruling,chamber)<
                           required(chamber.total_seats))continue;
                        std::vector<std::string> senate_support;
                        if(p1_constitution(country).legislature==
                           P1LegislatureKind::bicameral) {
                            if(!p2_upper_chambers_.contains(country))continue;
                            const auto senate_id=p2_upper_chambers_.at(country);
                            if(!p4_nonparliament_results_.contains(senate_id))continue;
                            const auto& upper=p4_nonparliament_results_.at(senate_id);
                            People senate_votes=0;
                            for(const auto& party:ruling)
                                if(upper.seats.contains(party)) {
                                    senate_support.push_back(party);
                                    senate_votes=phaseb::add_checked(
                                        senate_votes,upper.seats.at(party));
                                }
                            if(senate_votes<required(p4_contests_.at(senate_id).seats))
                                continue;
                        }
                        p2_override_veto(bill_id,ruling,senate_support,today);
                    }
                    if(!assent.signed_by_head&&!assent.overridden) {
                        const auto character=p1_characters_.find(g.head_of_state);
                        const auto domain=bill.action==
                            P2BillAction::enact_or_replace_base?
                            bill.proposed_base.domain:std::string{};
                        const auto desired=bill.action==
                            P2BillAction::enact_or_replace_base?
                            bill.proposed_base.base_choice:std::string{};
                        bool opposes=false;
                        if(character!=p1_characters_.end()&&
                           character->second.active) {
                            const auto preference=character->second.law_preferences.find(domain);
                            opposes=preference!=character->second.law_preferences.end()&&
                                    preference->second!=desired;
                        }
                        if(opposes)p2_veto_bill(bill_id,g.head_of_state,today);
                        else p2_sign_bill(bill_id,g.head_of_state,today);
                        if(opposes)continue;
                    }
                }
                p2_promulgate_bill(bill_id,g.head_of_state,today);
                events_.push_back("p1-ai-legal-promulgation:"+bill_id);
                continue;
            }
            if(!p2_enactment_ready(bill_id))continue;
            std::vector<std::string> votes,upper_votes;
            bool feasible=true;
            if(order.legislature_exists) {
                if(!country_elections_.contains(country)||
                   p4_parliament_dissolved(country))feasible=false;
                else {
                    votes=p1_coalitions_.at(country).ruling_parties;
                    const auto& chamber=country_elections_.at(country);
                    for(const auto& party:votes)
                        if(!chamber.seats.contains(party))feasible=false;
                    const auto tier=bill.action==P2BillAction::enact_or_replace_base?
                       bill.proposed_base.tier:
                       p2_laws_.at(bill.action==P2BillAction::attach_amendment?
                          bill.proposed_amendment.law_id:
                          bill.action==P2BillAction::repeal_base?
                          bill.target_id:
                          p2_amendments_.at(bill.target_id).law_id).tier;
                    const People quorum=tier==P2NormTier::constitution?
                        chamber.total_seats-chamber.total_seats/3:g.quorum;
                    if(feasible&&coalition_seats(votes,chamber)<quorum)
                        feasible=false;
                }
            } else if(!order.direct_executive)feasible=false;
            if(p1_constitution(country).legislature==
               P1LegislatureKind::bicameral) {
                // Upper chamber is checked before approving the lower, so a
                // missing election cannot abort and roll back the day.
                if(!p2_upper_chambers_.contains(country))feasible=false;
                else {
                    const auto id=p2_upper_chambers_.at(country);
                    if(!p4_nonparliament_results_.contains(id))feasible=false;
                    else {
                        const auto& upper=p4_nonparliament_results_.at(id);
                        People voted=0;
                        for(const auto& p:votes)
                            if(upper.seats.contains(p)&&
                               upper.seats.at(p)>0) {
                                upper_votes.push_back(p);
                                voted=phaseb::add_checked(voted,upper.seats.at(p));
                            }
                        const auto tier=bill.action==
                            P2BillAction::enact_or_replace_base?
                            bill.proposed_base.tier:
                            p2_laws_.at(bill.action==P2BillAction::attach_amendment?
                                bill.proposed_amendment.law_id:
                                bill.action==P2BillAction::repeal_base?
                                bill.target_id:
                                p2_amendments_.at(bill.target_id).law_id).tier;
                        const People required=tier==P2NormTier::constitution?
                            p4_contests_.at(id).seats-
                                p4_contests_.at(id).seats/3:
                            p4_contests_.at(id).seats/2+1;
                        if(voted<required)feasible=false;
                    }
                }
            }
            if(!feasible)continue;
            p2_approve_bill(bill_id,votes);
            if(p1_constitution(country).legislature==
               P1LegislatureKind::bicameral)
                p2_approve_upper_bill(bill_id,upper_votes);
            if(p1_legislative_powers(country).veto!=P1VetoRule::none)
                p2_sign_bill(bill_id,g.head_of_state,today);
            p2_promulgate_bill(bill_id,g.head_of_state,today);
            events_.push_back("p1-ai-actual-bill-approved:"+bill_id);
        }
        if(pending_enactment) {agenda.next_review_day=today+7;continue;}
        // Evaluate existing 1.15 catalogue law alternatives at event/agenda
        // review time, not every simulation day. Both ideology clout and
        // movement support come from existing P3/POP registrations.
        if(p1_legitimacy(country).score>=25) {
            std::string best_group,best_choice;
            std::int64_t best_score=0;
            const auto& catalogue=politics::catalogue();
            for(const auto& [slot,law_id]:p2_active_base_) {
                (void)slot;
                const auto& law=p2_laws_.at(law_id);
                if(law.country!=country||
                   !catalogue.has_group(law.domain))continue;
                const auto group=politics::bare_group(law.domain);
                const auto options=catalogue.by_group.find(group);
                if(options==catalogue.by_group.end())continue;
                for(const auto& choice:options->second) {
                    const auto candidate_id="AI:bill:"+country+":"+
                        std::to_string(today);
                    if(!p2_catalogue_reform_template(
                         country,group,choice,candidate_id,
                         "ai-catalogue:"+group+":"+choice))continue;
                    const auto political=p2_political_support(country,group,choice);
                    std::int64_t backing=0;
                    for(const auto& [id,movement]:p3_movements_) {
                        if(!movement.active||movement.country!=country||
                           movement.goal!=P3MovementGoal::legal_reform||
                           politics::bare_group(movement.issue_domain)!=group||
                           politics::bare_law(movement.desired_choice)!=choice)
                            continue;
                        const auto evidence=p3_movement_view(id);
                        // Only documented POP-backed movements can outweigh
                        // a hostile legislature; zero supporters add nothing.
                        if(evidence.supporters>0)
                            backing=std::max(backing,
                                2*static_cast<std::int64_t>(evidence.support_ppm)+
                                evidence.activism_ppm);
                    }
                    if(!political.observed&&backing<=0)continue;
                    const std::int64_t score=
                        3*static_cast<std::int64_t>(political.governing_support_ppm)+
                        2*static_cast<std::int64_t>(political.support_ppm)-
                        3*static_cast<std::int64_t>(political.oppose_ppm)+backing;
                    if(score<=0||score<=best_score)continue;
                    best_score=score;best_group=group;best_choice=choice;
                }
            }
            if(!best_group.empty()) {
                const auto bill_id="AI:bill:"+country+":"+std::to_string(today);
                const auto basis="ai-catalogue:"+best_group+":"+best_choice;
                p2_propose_catalogue_reform(country,best_group,best_choice,
                    g.prime_minister,basis,bill_id,today);
                agenda.last_bill_id=bill_id;
                agenda.last_reform_choice=best_choice;
                events_.push_back("p1-ai-catalogue-reform-proposed:"+bill_id);
            }
        }
        agenda.next_review_day=today+30;
    }
}
} // namespace vic25::phasec
