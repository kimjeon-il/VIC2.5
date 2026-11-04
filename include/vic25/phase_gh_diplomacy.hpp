#pragma once
// Phase H extended diplomatic execution over the C/D/E/G staged world.
// Legal clauses produce effective rights or verified C/E actions only;
// neither diplomatic approval nor influence creates goods, people, or money.

namespace vic25::phasegh {

inline void Stage::queue_package_effects_if_ratified(Treaty& treaty) {
    at(9);
    const bool accepted=treaty.parties.empty()?
              (treaty.ratified_a&&treaty.ratified_b):
              std::all_of(treaty.parties.begin(),treaty.parties.end(),
                [&](const std::string& p){
                    return treaty.ratifications.contains(p);
                });
    if(!accepted)return;
    for(auto& clause:treaty.clauses) {
        if(clause.queued)continue;
        if(clause.kind==ClauseKind::territorial_cession) {
            require(clause.province>0&&clause.receiving_admin>0&&
                    spatial_.spatial().provinces.contains(clause.province)&&
                    spatial_.spatial().units.contains(clause.receiving_admin)&&
                    spatial_.spatial().units.at(clause.receiving_admin).country==
                        clause.beneficiary,
                    "H_ERR_INVALID_TERRITORIAL_CONCESSION");
            require(phase_c().sovereignty(
              spatial_.spatial().provinces.at(clause.province).region)==
              clause.obligor,"H_ERR_TERRITORY_GRANTOR_NOT_SOVEREIGN");
            spatial_.queue_transfer("H:CESSION:"+treaty.id+":"+clause.id,
                              {clause.province},clause.beneficiary,
                              clause.receiving_admin);
        } else if(clause.kind==ClauseKind::reparations) {
            require(clause.amount>0&&!clause.obligor.empty()&&
                    !clause.beneficiary.empty(),
                    "H_ERR_INVALID_REPARATION_TERM");
            const std::string ref="H:REPARATION:"+treaty.id+":"+clause.id;
            phase_c().sign_reparation_treaty(ref,clause.obligor,
                                             clause.beneficiary,clause.amount);
            diplomacy_.reparation_links.insert(ref);
        } else if(clause.kind==ClauseKind::protectorate) {
            const std::string ref="H:PROTECTORATE:"+treaty.id+":"+clause.id;
            require(!diplomacy_.protectorates.contains(ref)&&
                    clause.obligor!=clause.beneficiary,
                    "H_ERR_DUPLICATE_PROTECTORATE");
            diplomacy_.protectorates.emplace(ref,Protectorate{ref,
                      clause.beneficiary,clause.obligor,
                      phase_c().month()+1,false,true});
        }
        // Tariff/embargo/quota/guarantee/cultural/research/aid/military-access
        // clauses are legal permissions/obligations, not automatic resource
        // movement. Stage0 effective-date queries and explicit fulfilment
        // operations below handle them; military war/peace remains C-M owned.
        clause.queued=true;
    }
}
inline void Stage::propose_treaty_package(
     const std::string& id,const std::set<std::string>& parties,
     std::vector<TreatyClause> clauses,const std::string& proposer,
     const std::string& prime) {
    at(9);
    require(!id.empty()&&parties.size()>=2&&parties.contains(proposer)&&
            !clauses.empty()&&!diplomacy_.treaties.contains(id),
            "H_ERR_INVALID_MULTI_CLAUSE_TREATY");
    for(const auto& party:parties)(void)phase_c().government(party);
    leader(phase_c(),proposer,prime);
    allow_foreign_policy(proposer);
    std::set<std::string> names;
    for(auto& c:clauses) {
        require(!c.id.empty()&&names.insert(c.id).second&&
                parties.contains(c.obligor)&&parties.contains(c.beneficiary)&&
                c.obligor!=c.beneficiary&&
                (c.expires<0||c.expires>=phase_c().month()+1)&&c.amount>=0,
                "H_ERR_INVALID_CLAUSE");
        c.effective=std::max(c.effective,phase_c().month()+1);
        require(c.expires<0||c.expires>=c.effective,
                "H_ERR_INVALID_CLAUSE_PERIOD");
        if(c.kind==ClauseKind::territorial_cession)
            require(c.province>0&&c.receiving_admin>0,
                    "H_ERR_MISSING_PROVINCE_OR_NEW_ADMIN");
        if(c.kind==ClauseKind::trade_quota)
            require(c.amount>0&&!c.good.empty(),"H_ERR_INVALID_TRADE_QUOTA");
        if(c.kind==ClauseKind::tariff_exemption ||
           c.kind==ClauseKind::trade_embargo)
            require(!c.good.empty(),"H_ERR_MISSING_GOOD_SCOPE");
    }
    auto it=parties.begin();
    const std::string first=*it++;
    const std::string second=*it;
    Treaty t{id,first,second,"",phase_c().month(),
                   phase_c().month()+1,false,false,false};
    t.parties=parties;
    t.ratifications.insert(proposer);
    t.ratified_a=(proposer==first);
    t.ratified_b=(proposer==second);
    t.clauses=std::move(clauses);
    diplomacy_.treaties.emplace(id,std::move(t));
    ++diplomacy_.revision;
}
inline void Stage::ratify_package_member(const std::string& id,
                                 const std::string& country,
                                 const std::string& prime) {
    at(9);
    auto& treaty=diplomacy_.treaties.at(id);
    require(!treaty.parties.empty()&&treaty.parties.contains(country)&&
            !treaty.active && !treaty.ratifications.contains(country),
            "H_ERR_INVALID_PACKAGE_RATIFICATION");
    leader(phase_c(),country,prime);
    allow_foreign_policy(country);
    treaty.ratifications.insert(country);
    if(country==treaty.country_a)treaty.ratified_a=true;
    if(country==treaty.country_b)treaty.ratified_b=true;
    treaty.effective_month=std::max(treaty.effective_month,
                                    phase_c().month()+1);
    queue_package_effects_if_ratified(treaty);
    ++diplomacy_.revision;
}
inline void Stage::terminate_treaty(const std::string& id,
                         const std::string& requesting_country,
                         const std::string& prime) {
    at(9);
    auto& t=diplomacy_.treaties.at(id);
    leader(phase_c(),requesting_country,prime);
    require(!t.parties.empty()&&t.parties.contains(requesting_country)&&
            t.active&& !t.pending_termination,
            "H_ERR_ILLEGAL_TREATY_TERMINATION");
    // Termination signatures are distinct from the signatures that originally
    // ratified the treaty; old ratification must NOT count as new consent.
    require(t.termination_approvals.insert(requesting_country).second,
            "H_ERR_DUPLICATE_TERMINATION_CONSENT");
    if(t.termination_approvals.size()==t.parties.size())
        t.pending_termination=true;
    ++diplomacy_.revision;
}
inline Quantity Stage::lawful_trade_quantity(
            const std::string& seller_country,
            const std::string& buyer_country,
            const std::string& good,Quantity requested) const {
    require(requested>=0,"G_ERR_NEGATIVE_QUANTITY");
    Quantity allowed=requested;
    for(const auto& [treaty_id,t]:diplomacy_.treaties) {
        if(!t.active)continue;
        for(const auto& c:t.clauses) {
            if(c.good!=good||c.effective>phase_c().month()||
               (c.expires>=0&&phase_c().month()>c.expires))
                continue;
            const bool covered=(c.obligor==seller_country&&
                                c.beneficiary==buyer_country)||
                               (c.obligor==buyer_country&&
                                c.beneficiary==seller_country);
            if(!covered)continue;
            if(c.kind==ClauseKind::trade_embargo)return 0;
            if(c.kind!=ClauseKind::trade_quota)continue;
            const auto key=std::make_tuple(treaty_id,c.id,buyer_country,
                                           phase_c().month());
            const Quantity already=diplomacy_.quota_used.contains(key)?
                                           diplomacy_.quota_used.at(key):0;
            require(c.amount>=already,"H_ERR_QUOTA_OVERSPENT");
            allowed=std::min(allowed,static_cast<Quantity>(c.amount-already));
        }
    }
    return allowed;
}
inline void Stage::reserve_trade_quota(const std::string& seller_country,
                 const std::string& buyer_country,
                 const std::string& good,Quantity amount) {
    at(1);
    require(amount>0,"G_ERR_INVALID_QUOTA_RESERVATION");
    for(const auto& [treaty_id,t]:diplomacy_.treaties) {
        if(!t.active)continue;
        for(const auto& c:t.clauses) {
            if(c.kind!=ClauseKind::trade_quota||c.good!=good||
               c.effective>phase_c().month()||
               (c.expires>=0&&phase_c().month()>c.expires))
                continue;
            if(!((c.obligor==seller_country&&c.beneficiary==buyer_country)||
                 (c.obligor==buyer_country&&c.beneficiary==seller_country)))
                 continue;
            const auto key=std::make_tuple(treaty_id,c.id,buyer_country,
                                         phase_c().month());
            const Quantity used=diplomacy_.quota_used.contains(key)?
                                     diplomacy_.quota_used.at(key):0;
            require(c.amount>=used&&amount<=c.amount-used,
                    "H_ERR_QUOTA_EXCEEDED");
            diplomacy_.quota_used[key]=phaseb::add_checked(used,amount);
            ++diplomacy_.revision;
        }
    }
}
inline void Stage::configure_organization(const std::string& id,
                      OrganizationRule rule,const std::string& prime) {
    at(9);
    auto& org=diplomacy_.organizations.at(id);
    require(!org.members.empty()&&rule.quorum>0&&rule.yes_required>0&&
            rule.quorum<=org.members.size()&&
            rule.yes_required<=org.members.size(),
            "H_ERR_INVALID_ORGANIZATION_RULE");
    const auto sponsor=*org.members.begin();
    leader(phase_c(),sponsor,prime);
    for(const auto& veto:rule.veto_members)
        require(org.members.contains(veto),"H_ERR_INVALID_VETO_MEMBER");
    org.governance=std::move(rule);
    ++diplomacy_.revision;
}
inline void Stage::hold_organization_vote(
               const std::string& org_id,const std::string& id,
               const std::string& subject,
               const std::map<std::string,VoteChoice>& ballots,
               const std::string& proposer,const std::string& prime) {
    at(9);
    auto& org=diplomacy_.organizations.at(org_id);
    leader(phase_c(),proposer,prime);
    require(org.active&&org.members.contains(proposer)&&!id.empty()&&
            !subject.empty()&&!org.decisions.contains(id),
            "H_ERR_INVALID_ORGANIZATION_VOTE");
    require(ballots.size()>=org.governance.quorum,
            "H_ERR_QUORUM_NOT_MET");
    std::size_t yes=0;
    for(const auto& [member,vote]:ballots) {
        require(org.members.contains(member),"H_ERR_INELIGIBLE_VOTER");
        if(vote==VoteChoice::yes)++yes;
    }
    bool veto=false;
    for(const auto& member:org.governance.veto_members) {
        auto it=ballots.find(member);
        if(it!=ballots.end()&&it->second==VoteChoice::no)veto=true;
    }
    const bool adopted=yes>=org.governance.yes_required&&!veto;
    org.decisions.emplace(id,OrganizationVote{id,org_id,subject,
                                  ballots,adopted,phase_c().month()+1});
    ++diplomacy_.revision;
}
inline void Stage::admit_organization_member(
                const std::string& org_id,const std::string& member,
                const std::string& decision,const std::string& candidate_prime) {
    at(9);
    auto& org=diplomacy_.organizations.at(org_id);
    leader(phase_c(),member,candidate_prime);
    allow_foreign_policy(member);
    const auto& vote=org.decisions.at(decision);
    require(org.active&&vote.adopted&&
            vote.subject=="admit:"+member&&!org.members.contains(member),
            "H_ERR_MEMBERSHIP_ADMISSION_DENIED");
    org.pending_admissions[phase_c().month()+1].insert(member);
    ++diplomacy_.revision;
}
inline void Stage::leave_organization(const std::string& org_id,
                   const std::string& member,const std::string& prime) {
    at(9);
    auto& org=diplomacy_.organizations.at(org_id);
    leader(phase_c(),member,prime);
    require(org.active&&org.members.contains(member),
            "H_ERR_NOT_MEMBER");
    org.pending_exits[phase_c().month()+1].insert(member);
    ++diplomacy_.revision;
}
inline void Stage::fund_cultural_exchange(
                  const std::string& id,const std::string& sponsor,
                  const std::string& recipient,Money amount,
                  const std::string& sponsor_prime) {
    at(5);
    leader(phase_c(),sponsor,sponsor_prime);
    require(!id.empty()&&sponsor!=recipient&&amount>0,
            "H_ERR_INVALID_CULTURAL_GRANT");
    for(const auto& e:diplomacy_.funded_exchanges)
        require(e.event_id!=id,"H_ERR_DUPLICATE_CULTURAL_GRANT");
    phase_c().pay(sponsor,recipient,amount);
    diplomacy_.funded_exchanges.push_back({id,sponsor,recipient,amount,
                                          phase_c().month()});
    ++diplomacy_.revision;
}
inline void Stage::dispatch_military_assistance(
               const std::string& id,const std::string& donor,
               const std::string& recipient,const std::string& good,
               Id origin,Id destination,Quantity amount,Money unit_value,
               Month duration,const std::string& donor_prime) {
    at(7);
    leader(phase_c(),donor,donor_prime);
    require(!id.empty()&&donor!=recipient&&!good.empty()&&amount>0&&
            unit_value>0&&origin>0&&destination>0&&
            spatial_.spatial().provinces.contains(origin)&&
            spatial_.spatial().provinces.contains(destination),
            "H_ERR_INVALID_MILITARY_ASSISTANCE");
    for(const auto& a:diplomacy_.military_assistance)
        require(a.event_id!=id,"H_ERR_DUPLICATE_MILITARY_ASSISTANCE");
    const auto& src=spatial_.spatial().provinces.at(origin).region;
    const auto& dst=spatial_.spatial().provinces.at(destination).region;
    require(phase_c().sovereignty(src)==donor&&
            phase_c().sovereignty(dst)==recipient,
            "H_ERR_INVALID_MILITARY_AID_JURISDICTION");
    const auto value=phaseb::mul_checked(amount,unit_value);
    const std::string shipment="H:AID:"+id;
    if(origin==destination)phase_c().phase_g_transfer_goods(
                                   donor,recipient,src,good,amount);
    else {
        require(duration>0,"H_ERR_UNSUPPORTED_INSTANT_MILITARY_AID");
        phase_c().ship(shipment,donor,recipient,src,dst,good,amount,duration);
    }
    diplomacy_.military_assistance.push_back({id,donor,recipient,good,
                      origin==destination?"":shipment,amount,value,
                      phase_c().month()});
    ++diplomacy_.revision;
}
inline void Stage::submit_crisis_position(
            const std::string& crisis,const std::string& actor,
            const std::string& stance,bool conditional,const std::string& prime) {
    at(9);
    leader(phase_c(),actor,prime);
    auto& c=diplomacy_.crises.at(crisis);
    require(!stance.empty()&&c.status!=CrisisStatus::settled&&
            c.status!=CrisisStatus::withdrawn,
            "H_ERR_CRISIS_CLOSED");
    c.positions[actor]={actor,stance,conditional,phase_c().month()};
    ++diplomacy_.revision;
}
inline void Stage::offer_crisis_compromise(
                  const std::string& crisis,CrisisProposal offer,
                  const std::string& prime) {
    at(9);
    auto& c=diplomacy_.crises.at(crisis);
    leader(phase_c(),offer.proposer,prime);
    require(!offer.id.empty()&&!c.proposals.contains(offer.id)&&
            (c.status==CrisisStatus::active||
             c.status==CrisisStatus::mediation||
             c.status==CrisisStatus::frozen)&&
            (offer.proposer==c.claimant||offer.proposer==c.respondent||
             offer.proposer==c.mediator)&&
            offer.compensation>=0 &&
            (offer.compensation==0||
             ((offer.payer==c.claimant||offer.payer==c.respondent)&&
              (offer.recipient==c.claimant||offer.recipient==c.respondent)&&
              offer.payer!=offer.recipient)),
            "H_ERR_INVALID_CRISIS_OFFER");
    offer.month=phase_c().month();
    c.proposals.emplace(offer.id,std::move(offer));
    ++diplomacy_.revision;
}
inline void Stage::respond_crisis_offer(
           const std::string& crisis,const std::string& proposal,
           const std::string& actor,bool accept,const std::string& prime) {
    at(9);
    leader(phase_c(),actor,prime);
    auto& c=diplomacy_.crises.at(crisis);
    require(actor==c.claimant||actor==c.respondent,
            "H_ERR_NOT_CRISIS_PARTY");
    auto& offer=c.proposals.at(proposal);
    require(c.status!=CrisisStatus::settled&&
            c.status!=CrisisStatus::withdrawn,
            "H_ERR_CRISIS_CLOSED");
    if(!accept) {
        offer.approvers.erase(actor);
        ++diplomacy_.revision;
        return;
    }
    require(offer.approvers.insert(actor).second,
            "H_ERR_DUPLICATE_CRISIS_RESPONSE");
    if(offer.approvers.contains(c.claimant)&&
       offer.approvers.contains(c.respondent)) {
        if(offer.compensation>0) {
            const auto key="H:CRISIS-REPARATION:"+c.id+":"+offer.id;
            phase_c().sign_reparation_treaty(key,offer.payer,
                                             offer.recipient,offer.compensation);
            diplomacy_.reparation_links.insert(key);
        }
        if(offer.freeze_claim)
            spatial_.queue_claim("H:CRISIS-CLAIM:"+c.id+":"+offer.id,
                         c.province,c.claimant,false);
        c.status=offer.freeze_claim?CrisisStatus::settled:
                                   CrisisStatus::partly_settled;
    }
    ++diplomacy_.revision;
}
inline void Stage::freeze_crisis(const std::string& id,
                      const std::string& claimant_prime,
                      const std::string& respondent_prime) {
    at(9);
    auto& c=diplomacy_.crises.at(id);
    leader(phase_c(),c.claimant,claimant_prime);
    leader(phase_c(),c.respondent,respondent_prime);
    require(c.status==CrisisStatus::active||
            c.status==CrisisStatus::mediation||
            c.status==CrisisStatus::partly_settled,
            "H_ERR_CRISIS_CANNOT_FREEZE");
    c.status=CrisisStatus::frozen;
    ++diplomacy_.revision;
}
inline void Stage::disburse_treaty_aid(const std::string& treaty_id,
                const std::string& clause_id,Money amount) {
    at(5);
    auto& treaty=diplomacy_.treaties.at(treaty_id);
    require(treaty.active && amount>0,"H_ERR_INACTIVE_AID_TERM");
    for(auto& term:treaty.clauses) {
        if(term.id!=clause_id)continue;
        require((term.kind==ClauseKind::financial_aid ||
                 term.kind==ClauseKind::cultural_exchange ||
                 term.kind==ClauseKind::research_cooperation)&&
                term.effective<=phase_c().month()&&
                (term.expires<0||phase_c().month()<=term.expires)&&
                term.fulfilled<=term.amount&&
                amount<=term.amount-term.fulfilled,
                "H_ERR_AID_TERM_EXHAUSTED");
        phase_c().pay(term.obligor,term.beneficiary,amount);
        term.fulfilled=phaseb::add_checked(term.fulfilled,amount);
        if(term.kind==ClauseKind::cultural_exchange)
            diplomacy_.funded_exchanges.push_back(
                {"H:CULTURE:"+treaty_id+":"+clause_id+":"+
                 std::to_string(term.fulfilled),term.obligor,
                 term.beneficiary,amount,phase_c().month()});
        ++diplomacy_.revision;
        return;
    }
    require(false,"H_ERR_UNKNOWN_AID_TERM");
}
inline InfluenceView World::assess_influence(const std::string& source,
                         const std::string& target) const {
    InfluenceView v=trade_influence(source,target);
    if(source==target)return v;
    // This measured model is deliberately not a scalar score. A trade price
    // paid in another currency is not silently added to TEST currency totals.
    for(const auto& evidence:diplomacy_.military_assistance) {
        if(evidence.provider!=source||evidence.receiver!=target)continue;
        bool delivered=evidence.shipment_id.empty();
        if(!delivered)delivered=core_.core().core().
                  phase_g_shipment(evidence.shipment_id).delivered;
        if(!delivered)continue;
        v.military_known=true;
        v.military_goods_delivered=phaseb::add_checked(
                         v.military_goods_delivered,evidence.quantity);
        v.military_material_value=phaseb::add_checked(
                         v.military_material_value,evidence.market_value);
    }
    for(const auto& e:diplomacy_.funded_exchanges)
        if(e.sponsor==source&&e.beneficiary==target) {
            v.cultural_known=true;
            v.cultural_funding=phaseb::add_checked(v.cultural_funding,e.funded);
        }
    for(const auto& [id,t]:diplomacy_.treaties) {
        (void)id;
        if(!t.active)continue;
        if(t.parties.contains(source)&&t.parties.contains(target)||
           (t.parties.empty() &&
            ((t.country_a==source&&t.country_b==target)||
             (t.country_b==source&&t.country_a==target)))) {
            ++v.political_cooperation_actions;
            v.political_known=true;
        }
    }
    for(const auto& [id,o]:diplomacy_.organizations) {
        (void)id;
        if(!o.active||!o.members.contains(source)||!o.members.contains(target))
            continue;
        for(const auto& [key,decision]:o.decisions) {
            (void)key;
            if(!decision.adopted)continue;
            auto sa=decision.ballots.find(source),ta=decision.ballots.find(target);
            if(sa!=decision.ballots.end()&&ta!=decision.ballots.end()&&
               sa->second==VoteChoice::yes&&ta->second==VoteChoice::yes) {
                ++v.political_cooperation_actions;
                v.political_known=true;
            }
        }
    }
    return v;
}
inline CrisisAdvice World::advise_crisis(const std::string& id,
                                  const std::string& actor) const {
    const auto& c=diplomacy_.crises.at(id);
    (void)core_.core().core().government(actor);
    CrisisAdvice advice;
    auto q=core_.spatial().claims.find(c.province);
    advice.has_underlying_dispute=q!=core_.spatial().claims.end()&&
                                   q->second.contains(c.claimant);
    advice.can_mediate=actor!=c.claimant&&actor!=c.respondent&&
                         c.status!=CrisisStatus::settled;
    const auto exposure=trade_influence(actor,c.respondent);
    advice.observed_commercial_exposure=exposure.bilateral_imports;
    // Existing C-M integration has no authoritative Conflict/EntryToWar
    // registry. This evaluation may not invent military commitments.
    advice.lawful_war_entry_supported=false;
    advice.recommendation=advice.can_mediate?"offer_mediation":
          (advice.has_underlying_dispute?"consider_negotiated_settlement":
                                        "verify_underlying_claim");
    return advice;
}
} // namespace vic25::phasegh
