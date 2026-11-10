#pragma once
// Three judicial-law levels. Staff/funding comes from the existing P1
// administration and policing services, never a new court payroll.
namespace vic25::phasec {
inline P1JudiciaryView IntegratedWorld::p1_judiciary_view(
        const std::string& country)const {
    ensure(governments_.contains(country),
           "P1 judiciary needs an actual government");
    P1JudiciaryView result;
    result.country=country;
    // Pre-constitutional legacy fixtures retain the original economy.
    if(!p1_constitutions_.contains(country))return result;
    for(const auto& [slot,id]:p2_active_base_) {
        (void)slot;
        const auto& law=p2_laws_.at(id);
        if(law.country!=country||law.domain!="judicial_system"||
           (!law.scope.empty()&&law.scope!="national"))continue;
        result.defined_by_law=true;
        if(law.base_choice=="executive_subordinate")
            result.kind=P1JudiciaryKind::executive_subordinate;
        else if(law.base_choice=="limited_independence")
            result.kind=P1JudiciaryKind::limited_independence;
        else if(law.base_choice=="independent")
            result.kind=P1JudiciaryKind::independent;
        else ensure(false,"P1 unknown enacted judicial choice");
        break;
    }
    // No judicial-system law is a *data gap*, not a presumption of
    // executive control. Preserve legacy full payroll payment behavior.
    if(!result.defined_by_law)return result;
    for(const auto& [id,link]:p1_institutions_) {
        if(link.country!=country||
           (link.kind!=P1InstitutionKind::administration&&
            link.kind!=P1InstitutionKind::policing))continue;
        const auto capacity=p1_institution_capacity(id);
        if(capacity.usable_slots<=0)continue;
        if(link.kind==P1InstitutionKind::administration)
            result.staffed_administration=true;
        if(link.kind==P1InstitutionKind::policing)
            result.staffed_policing=true;
    }
    const int base=result.kind==P1JudiciaryKind::executive_subordinate?
        400'000:result.kind==P1JudiciaryKind::limited_independence?
        650'000:850'000;
    result.enforceable_contract_ppm=std::min(1'000'000,
        base+(result.staffed_administration?75'000:0)+
             (result.staffed_policing?75'000:0));
    return result;
}
inline void IntegratedWorld::p1_observe_legal_disputes(std::int64_t today) {
    require_stage({9});
    // At most one country-level event per ~30-day period. Existing
    // overdue wage claims are the only evidence; no trial docket.
    if(today<0||today%30!=0)return;
    std::set<std::string> countries;
    for(const auto& [id,claim]:wage_arrears_) {
        (void)id;
        if(claim.outstanding<=0||claim.issued_day>today||
           today-claim.issued_day<60||claim.country.empty()||
           !p1_constitutions_.contains(claim.country))continue;
        const auto court=p1_judiciary_view(claim.country);
        if(court.defined_by_law&&
           court.kind!=P1JudiciaryKind::independent)
            countries.insert(claim.country);
    }
    for(const auto& country:countries) {
        const auto id="P1:legal-dispute:"+country+":"+
                      std::to_string(today/30);
        p1_political_events_.try_emplace(id,P1PoliticalEvent{
            id,country,"overdue-real-wage-creditors",
            P1PoliticalEventKind::actual_legal_dispute,today,false});
        events_.push_back("p1-actual-wage-rights-political-dispute:"+country);
    }
}
} // namespace vic25::phasec
