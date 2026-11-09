#pragma once
namespace vic25::phasec {
inline P1LegislativePowers IntegratedWorld::p1_legislative_powers(
        const std::string& country)const {
    const auto it=p1_legislative_powers_.find(country);
    if(it!=p1_legislative_powers_.end())return it->second;
    P1LegislativePowers p;
    if(p1_constitution(country).governance==
       P1GovernancePrinciple::presidential_republic)
        p.veto=P1VetoRule::suspensive;
    return p;
}
inline void IntegratedWorld::p1_set_legislative_powers(
        const std::string& country,P1LegislativePowers p) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 constitutional power change outside political stage");
    ensure(p1_constitutions_.contains(country)&&
           p.override_ppm>=500'000&&p.override_ppm<=1'000'000&&
           p.assent_days>=1&&p.assent_days<=365,
           "P1 invalid legislative powers");
    p1_legislative_powers_[country]=p;
}
inline void IntegratedWorld::p2_open_assent(const std::string& id) {
    const auto& b=p2_bills_.at(id);
    if(p1_legislative_powers(b.country).veto!=P1VetoRule::none)
        p2_assents_.try_emplace(id,P2Assent{id,b.country,-1,-1,false,false,false});
}
inline void IntegratedWorld::p2_sign_bill(
        const std::string& id,const std::string& head,std::int64_t day) {
    require_stage({9});
    const auto& b=p2_bills_.at(id);
    ensure(b.status==P2BillStatus::approved&&
           governments_.at(b.country).head_of_state==head,
           "P2 invalid presidential signature");
    auto& a=p2_assents_.at(id);
    ensure(!a.vetoed&&!a.signed_by_head,"P2 already decided");
    a.signed_by_head=true;a.deadline=-1;
    events_.push_back("p2-executive-assent:"+id);
}
inline void IntegratedWorld::p2_veto_bill(
        const std::string& id,const std::string& head,std::int64_t day) {
    require_stage({9});
    const auto& b=p2_bills_.at(id);
    ensure(b.status==P2BillStatus::approved&&
           governments_.at(b.country).head_of_state==head&&
           p1_legislative_powers(b.country).veto!=P1VetoRule::none,
           "P2 veto unavailable");
    auto& a=p2_assents_.at(id);
    ensure(!a.vetoed&&!a.signed_by_head,"P2 already decided");
    a.vetoed=true;a.veto_day=day;a.deadline=-1;
    events_.push_back("p2-executive-veto:"+id);
}
inline void IntegratedWorld::p2_override_veto(
        const std::string& id,const std::vector<std::string>& lower,
        const std::vector<std::string>& upper,std::int64_t day) {
    require_stage({9});
    const auto& b=p2_bills_.at(id);
    auto& a=p2_assents_.at(id);
    const auto p=p1_legislative_powers(b.country);
    ensure(b.status==P2BillStatus::approved&&a.vetoed&&!a.overridden&&
           p.veto==P1VetoRule::suspensive&&
           p1_constitution(b.country).legislature!=P1LegislatureKind::none&&
           !p4_parliament_dissolved(b.country),
           "P2 veto is not overridable");
    const auto threshold=[&](People seats)->People {
        return static_cast<People>(
           (static_cast<std::uint64_t>(seats)*p.override_ppm+999'999)/1'000'000);
    };
    const auto& chamber=legislature_for(b.country);
    ensure(coalition_seats(lower,chamber)>=threshold(chamber.total_seats),
           "P2 lower chamber override lacks votes");
    if(p1_constitution(b.country).legislature==P1LegislatureKind::bicameral) {
        const auto contest=p2_upper_chambers_.at(b.country);
        const auto& senate=p4_nonparliament_results_.at(contest);
        std::set<std::string> unique;People votes=0;
        for(const auto& party:upper) {
            ensure(unique.insert(party).second&&senate.seats.contains(party),
                   "P2 invalid senate override bloc");
            votes=phaseb::add_checked(votes,senate.seats.at(party));
        }
        ensure(votes>=threshold(p4_contests_.at(contest).seats),
               "P2 senate override lacks votes");
    }
    a.overridden=true;
    events_.push_back("p2-veto-overridden:"+id);
}
inline bool IntegratedWorld::p2_has_assent_work()const {
    for(const auto& [id,a]:p2_assents_)
        if(p2_bills_.at(id).status==P2BillStatus::approved&&
           !a.signed_by_head&&!a.vetoed&&!a.overridden)return true;
    return false;
}
inline void IntegratedWorld::p2_assent_due(std::int64_t day) {
    require_stage({9});
    for(auto& [id,a]:p2_assents_)
        if(p2_bills_.at(id).status==P2BillStatus::approved&&
           !a.signed_by_head&&!a.vetoed&&!a.overridden) {
            if(a.deadline<0)
                a.deadline=day+p1_legislative_powers(a.country).assent_days;
            if(a.deadline<=day) {
                a.signed_by_head=true;
                events_.push_back("p2-assent-deadline:"+id);
            }
        }
}
inline void IntegratedWorld::p2_cancel_bill(
        const std::string& id,std::int64_t day) {
    require_stage({9});
    auto& b=p2_bills_.at(id);
    ensure(b.status==P2BillStatus::submitted&&day>=b.introduced_day,
           "P2 only pending bills may be withdrawn");
    b.status=P2BillStatus::rejected;
    if(const auto it=p2_enactments_.find(id);it!=p2_enactments_.end()) {
        p2_retry_after_[it->second.law_key]=day+730;
        if(it->second.next_review_day>=0)
            p2_review_due_[it->second.next_review_day].erase(id);
        it->second.next_review_day=-1;
    }
    p2_negotiation_watch_.erase(id);
    events_.push_back("p2-bill-withdrawn:"+id);
}
} // namespace vic25::phasec
