#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_d.hpp"
#include <cassert>

int main() {
    using namespace vic25;
    phasec::IntegratedWorld core{24000};
    core.seed_region("REGION","A");
    core.seed_actor("A",1000);
    core.seed_government("A","HEAD","PM",5,3);
    core.seed_p1_constitution("A",{
        phasec::P1GovernancePrinciple::monarchy,
        phasec::P1PowerDistribution::autocracy,
        phasec::P1LegislatureKind::none,
        "constitution:start"});
    phased::DomainState domain;
    phased::ResearchProject research;
    research.id="RESEARCH";research.holder="A";
    research.region="REGION";research.tech_id="STEEL";
    research.required_evidence=100;
    phased::KnowledgeContact contact;
    contact.id="CONTACT";contact.target="A";
    contact.tech="STEEL";contact.required_instruction=100;
    assert(phased::effective_research_evidence(research,domain,core)==100);
    assert(phased::effective_contact_instruction(contact,domain,core)==100);
    phasec::P2BaseLaw education;
    education.id="EDUCATION:1";education.country="A";
    education.domain="education_system";education.scope="national";
    education.base_choice="law_terakoya";
    education.legal_basis="fixture:education";
    core.p2_seed_base(std::move(education));
    assert(phased::effective_research_evidence(research,domain,core)==112);
    phasec::P2BaseLaw economy;
    economy.id="ECONOMY:1";economy.country="A";
    economy.domain="economic_system";economy.scope="national";
    economy.base_choice="law_industry_banned";
    economy.legal_basis="fixture:economy";
    core.p2_seed_base(std::move(economy));
    // Only existing industry recipes confer the production-technology tag.
    phased::IndustryRecipe recipe;
    recipe.id="RECIPE";recipe.site="SITE";recipe.tech_id="STEEL";
    domain.recipes.emplace("RECIPE",recipe);
    assert(phased::effective_research_evidence(research,domain,core)==154);
    assert(phased::effective_contact_instruction(contact,domain,core)==134);
    assert(core.p2_catalog_modifier_value(
        "A","country_tech_research_speed_mult")==-0.1);
    assert(core.p2_catalog_modifier_value(
        "A","country_production_tech_research_speed_mult")==-0.25);
}
