#pragma once
// Diplomatic capacity is a read-only projection over H's ACTIVE treaty and
// protectorate contracts, C's source-qualified rank and P2/P3 modifiers.
// It is not a new diplomatic-currency wallet. The 1.15 rules allow negative
// capacity; existing agreements continue but impose documented penalties.
namespace vic25::phasegh {
inline DiplomaticCapacityView World::diplomatic_capacity(
        const std::string& country)const {
    const auto& c=core_.core().core();
    (void)c.government(country);
    DiplomaticCapacityView out;
    out.country=country;
    out.rank=c.p1_power_rank(country);
    out.rank_observed=out.rank!=phasec::P1PowerRank::unobserved;
    out.rank_bonus=phasec::p1power::rank_influence(out.rank);
    const auto add=phaseb::add_checked(
        c.p2_modifier_ppm(country,"country_influence_add"),
        c.p3_trait_modifier_ppm(country,"country_influence_add"));
    out.legal_bonus=add/1'000'000;
    const auto gross=std::max<std::int64_t>(0,phaseb::add_checked(
        phasec::p1power::capacity_base,
        phaseb::add_checked(out.rank_bonus,out.legal_bonus)));
    const auto multiplier=std::clamp<std::int64_t>(
        phaseb::add_checked(1'000'000,phaseb::add_checked(
            c.p2_modifier_ppm(country,"country_influence_mult"),
            c.p3_trait_modifier_ppm(country,"country_influence_mult"))),
        0,5'000'000);
    const auto generated=phaseb::mul_checked(gross,multiplier)/1'000'000;
    std::int64_t commitments=0;
    const auto charge=[&](std::int64_t cost) {
        commitments=phaseb::add_checked(commitments,cost);
    };
    const auto month=c.month();
    // Explicit source: 1.15 /common/diplomatic_actions/19_subject_protectorate
    // cost=10, maintenance_paid_by=first_country (principal).
    for(const auto& [id,pact]:diplomacy_.protectorates) {
        (void)id;
        if(pact.active&&pact.principal==country)charge(10);
    }
    // The H treaty model has explicit obligor/beneficiary. Only clauses
    // mapping unambiguously to a 1.15 treaty article and payer are charged.
    // Unsupported/ambiguous clauses stay visible as unpriced commitments.
    for(const auto& [id,treaty]:diplomacy_.treaties) {
        (void)id;
        if(!treaty.active||treaty.effective_month>month)continue;
        if(treaty.clauses.empty()&&
           (treaty.country_a==country||treaty.country_b==country||
            treaty.parties.contains(country))) {
            ++out.unpriced_commitments;
            continue;
        }
        for(const auto& clause:treaty.clauses) {
            if(clause.effective>month||
               (clause.expires>=0&&clause.expires<month))continue;
            std::string payer;
            std::int64_t cost=0;
            switch(clause.kind) {
                case ClauseKind::tariff_exemption:
                    // 1.15 treaty article no_tariffs: cost 10, target pays.
                    payer=clause.beneficiary;cost=10;break;
                case ClauseKind::military_access:
                    // 1.15 military_access: cost 25, target pays.
                    payer=clause.beneficiary;cost=25;break;
                case ClauseKind::defense_guarantee:
                    // 1.15 guarantee_independence: cost 50, source pays.
                    payer=clause.obligor;cost=50;break;
                default:
                    if(clause.obligor==country||clause.beneficiary==country)
                        ++out.unpriced_commitments;
                    continue;
            }
            if(payer==country)charge(cost);
        }
    }
    // H's simplified bilateral daily-tariff contract has no recorded
    // directed treaty-article payer. 1.15 no_tariffs charges the target;
    // never guess which party that is for a symmetrical historic H treaty.
    for(const auto& [id,pact]:diplomacy_.daily_treaties) {
        (void)id;
        if(pact.active&&(pact.country_a==country||pact.country_b==country))
            ++out.unpriced_commitments;
    }
    // There is no H power-bloc leader identity yet; the documented 1.15
    // power-bloc cost (500) must not be billed to every organization member.
    for(const auto& [id,org]:diplomacy_.organizations) {
        (void)id;
        if(org.active&&org.members.contains(country))
            ++out.unpriced_commitments;
    }
    out.ongoing_pacts=commitments;
    out.influence=phasec::p1power::flow(
        generated,commitments,
        out.rank_observed&&out.unpriced_commitments==0);
    const auto prestige=c.p1_prestige(country);
    out.prestige_observed=prestige.observed;
    out.prestige_before=prestige.partial_total;
    // 1.15 diplomatic_overreach static modifier: at 2x active Influence
    // usage, prestige multiplier reaches -50%. Do not infer a *final*
    // prestige score from incomplete military/GDP or unpriced H treaties.
    if(prestige.complete&&out.influence.complete) {
        out.prestige_complete=true;
        const auto reduction=out.influence.shortage_ppm/2;
        out.prestige_after=phaseb::mul_checked(
            prestige.total,1'000'000-reduction)/1'000'000;
    }
    return out;
}
} // namespace vic25::phasegh
