#pragma once
// Constitutional assent is distinct from Victoria 3 enactment success and
// distinct from bicameral P4's authoritative seat results.
namespace vic25::phasec {
inline P1LegislativeChecks IntegratedWorld::p1_legislative_checks(
        const std::string& country)const {
    if(const auto it=p1_legislative_checks_.find(country);
       it!=p1_legislative_checks_.end())return it->second;
    const auto& c=p1_constitution(country);
    P1LegislativeChecks out;
    if(c.legislature==P1LegislatureKind::none) {
        out.parliamentary_initiative=false;
        return out;
    }
    switch(c.governance) {
      case P1GovernancePrinciple::presidential_republic:
        out.veto=P1VetoMode::qualified;
        break;
      case P1GovernancePrinciple::parliamentary_republic:
      case P1GovernancePrinciple::council_republic:
        out.veto=P1VetoMode::none;
        break;
      case P1GovernancePrinciple::monarchy:
        if(c.power==P1PowerDistribution::autocracy)
            out.veto=P1VetoMode::absolute;
        else if(p1_formation_rule(c)==P1FormationRule::head_appointment)
            out.veto=P1VetoMode::suspensive;
        else out.veto=P1VetoMode::none;
        break;
      case P1GovernancePrinciple::theocracy:
      case P1GovernancePrinciple::chiefdom:
        out.veto=P1VetoMode::suspensive;
        break;
    }
    if(out.veto==P1VetoMode::suspensive)out.override_ppm=500'001;
    return out;
}
inline void IntegratedWorld::p1_seed_legislative_checks(
        const std::string& country,P1LegislativeChecks config) {
    ensure(stage_==-1&&commit_id_==0&&
           p1_constitutions_.contains(country)&&
           !p1_legislative_checks_.contains(country)&&
           static_cast<unsigned>(config.veto)<=
              static_cast<unsigned>(P1VetoMode::absolute)&&
           config.override_ppm>=500'001&&
           config.override_ppm<=1'000'000&&
           config.assent_days>=1&&config.assent_days<=90&&
           config.override_days>=1&&config.override_days<=180&&
           (p1_constitution(country).legislature!=P1LegislatureKind::none||
             (!config.parliamentary_initiative&&
              config.veto==P1VetoMode::none)),
           "P1 illegal constitutional legislative checks");
    p1_legislative_checks_[country]=config;
}
inline bool IntegratedWorld::p2_has_assent_due(std::int64_t today)const {
    return p2_assent_due_.contains(today);
}
inline void IntegratedWorld::p2_rebuild_assent_index() {
    p2_assent_due_.clear();
    for(const auto& [id,a]:p2_assents_) {
        ensure(id==a.bill_id&&p2_bills_.contains(id)&&
               p2_bills_.at(id).country==a.country&&
               a.started_day>=0&&a.decision_deadline>=a.started_day&&
               a.override_ppm>=500'001&&a.override_ppm<=1'000'000,
               "P2 invalid saved constitutional assent");
        if(a.status==P2AssentStatus::pending)
            p2_assent_due_[a.decision_deadline].insert(id);
        else if(a.status==P2AssentStatus::vetoed) {
            ensure(a.override_deadline>a.veto_day,
                   "P2 veto has no lawful reconsideration deadline");
            p2_assent_due_[a.override_deadline].insert(id);
        }
    }
}
inline void IntegratedWorld::p2_begin_assent(const std::string& bill_id,
                                            std::int64_t today) {
    require_stage({9});
    const auto& b=p2_bills_.at(bill_id);
    ensure(b.status==P2BillStatus::approved&&today>=b.introduced_day,
           "P2 assent may begin only after all required legal votes");
    if(p2_assents_.contains(bill_id))return;
    const auto checks=p1_legislative_checks(b.country);
    const auto head=governments_.at(b.country).head_of_state;
    p2_assents_.emplace(bill_id,P2Assent{
        bill_id,b.country,head,checks.veto,P2AssentStatus::pending,
        checks.override_ppm,today,today+checks.assent_days,-1,-1});
    p2_assent_due_[today+checks.assent_days].insert(bill_id);
    events_.push_back("p2-national-head-assent-pending:"+bill_id);
}
inline void IntegratedWorld::p2_veto_bill(
        const std::string& bill_id,const std::string& head,
        std::int64_t today) {
    require_stage({9});
    p2_begin_assent(bill_id,today);
    auto& a=p2_assents_.at(bill_id);
    ensure(a.status==P2AssentStatus::pending&&
           (a.veto==P1VetoMode::qualified||
            a.veto==P1VetoMode::suspensive||
            a.veto==P1VetoMode::absolute)&&
           governments_.at(a.country).head_of_state==head&&
           today>=a.started_day&&today<=a.decision_deadline,
           "P2 national head has no effective veto authority");
    p2_assent_due_[a.decision_deadline].erase(bill_id);
    if(p2_assent_due_[a.decision_deadline].empty())
        p2_assent_due_.erase(a.decision_deadline);
    a.status=P2AssentStatus::vetoed;
    a.veto_day=today;
    if(a.veto!=P1VetoMode::absolute) {
        const auto config=p1_legislative_checks(a.country);
        a.override_deadline=today+config.override_days;
        p2_assent_due_[a.override_deadline].insert(bill_id);
    }
    events_.push_back("p2-national-head-veto:"+bill_id);
}
inline void IntegratedWorld::p2_override_veto(
        const std::string& bill_id,
        const std::vector<std::string>& lower_parties,
        const std::vector<std::string>& upper_parties,std::int64_t today) {
    require_stage({9});
    auto& a=p2_assents_.at(bill_id);
    ensure(a.status==P2AssentStatus::vetoed&&
           a.veto!=P1VetoMode::absolute&&
           today>=a.veto_day&&today<=a.override_deadline&&
           !p4_parliament_dissolved(a.country),
           "P2 veto cannot be overridden outside its legislative window");
    const auto& constitution=p1_constitution(a.country);
    ensure(constitution.legislature!=P1LegislatureKind::none&&
           country_elections_.contains(a.country),
           "P2 veto override requires the current real parliament");
    const auto threshold=[&](People seats) {
        ensure(seats>0&&seats<=100'000,
               "P2 override needs a genuine elected chamber");
        const auto ppm=static_cast<People>(
            p2_bills_.at(bill_id).action==P2BillAction::enact_or_replace_base&&
            p2_bills_.at(bill_id).proposed_base.tier==
                P2NormTier::constitution?
            std::max(a.override_ppm,666'667):a.override_ppm);
        return (seats*ppm+999'999)/1'000'000;
    };
    const auto& lower=country_elections_.at(a.country);
    ensure(coalition_seats(lower_parties,lower)>=
               threshold(lower.total_seats),
           "P2 veto override lacks lower-house special majority");
    if(constitution.legislature==P1LegislatureKind::bicameral) {
        ensure(p2_upper_chambers_.contains(a.country)&&
               !upper_parties.empty(),
               "P2 bicameral veto override lacks upper house");
        const auto contest=p2_upper_chambers_.at(a.country);
        ensure(p4_nonparliament_results_.contains(contest),
               "P2 veto override requires actual upper election");
        const auto& upper=p4_nonparliament_results_.at(contest);
        ensure(upper.day==p4_contests_.at(contest).last_poll_day,
               "P2 veto override cannot use an outdated Senate election");
        std::set<std::string> seen;People yes=0;
        for(const auto& party:upper_parties){
            ensure(seen.insert(party).second&&upper.seats.contains(party),
                   "P2 invalid Senate override party");
            yes=phaseb::add_checked(yes,upper.seats.at(party));
        }
        ensure(yes>=threshold(p4_contests_.at(contest).seats),
               "P2 veto override lacks Senate special majority");
    } else ensure(upper_parties.empty(),
                  "P2 non-bicameral country has no Senate votes");
    p2_assent_due_[a.override_deadline].erase(bill_id);
    if(p2_assent_due_[a.override_deadline].empty())
        p2_assent_due_.erase(a.override_deadline);
    a.status=P2AssentStatus::overridden;
    events_.push_back("p2-parliament-overrode-national-head:"+bill_id);
    // Mandatory publication requires no discretionary second signature.
    p2_promulgate_bill(bill_id,governments_.at(a.country).head_of_state,today);
}
inline void IntegratedWorld::p2_process_assent_due(std::int64_t today) {
    require_stage({9});
    const auto it=p2_assent_due_.find(today);
    if(it==p2_assent_due_.end())return;
    const auto due=it->second;
    p2_assent_due_.erase(it);
    for(const auto& bill_id:due) {
        auto& a=p2_assents_.at(bill_id);
        auto& b=p2_bills_.at(bill_id);
        if(a.status==P2AssentStatus::pending) {
            ensure(a.decision_deadline==today,
                   "P2 assent review outside scheduled day");
            if(a.veto==P1VetoMode::absolute) {
                a.status=P2AssentStatus::expired;
                b.status=P2BillStatus::rejected;
                events_.push_back("p2-absolute-assent-withheld:"+bill_id);
            } else {
                // No veto by the legal deadline constitutes assent, even if
                // the formal head declines to sign.
                p2_promulgate_bill(bill_id,
                    governments_.at(a.country).head_of_state,today);
                events_.push_back("p2-expired-signing-period-auto-promulgated:"+bill_id);
            }
        } else if(a.status==P2AssentStatus::vetoed) {
            ensure(a.override_deadline==today,
                   "P2 stale parliamentary reconsideration deadline");
            a.status=P2AssentStatus::expired;
            b.status=P2BillStatus::rejected;
            events_.push_back("p2-veto-upheld-after-reconsideration:"+bill_id);
        }
    }
}
} // namespace vic25::phasec
