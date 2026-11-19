#include "vic25/phase_c.hpp"
#include "vic25/phase_c_p2_catalog_115.hpp"
#include <stdexcept>
#include <string>
using namespace vic25;
static void check(bool yes,const char* why) {
    if(!yes)throw std::runtime_error(why);
}
int main() {
    check(phasec::p2_catalog_115().laws.size()==148,"law count");
    check(phasec::p2_catalog_115().amendments.size()==71,"amendment count");
    phasec::IntegratedWorld c{24000};
    c.seed_region("R","A");
    c.seed_actor("A",100);
    c.seed_actor("FIRM",100);
    c.seed_government("A","HEAD","PM",5,3);
    phasec::P2BaseLaw law;
    law.id="ECONOMY";law.country="A";law.domain="economic_system";
    law.base_choice="law_industry_banned";law.scope="national";
    law.legal_basis="fixture:canonical-1.15";
    c.p2_seed_base(law);
    check(c.p2_catalog_group_active("A","economic_system"),
          "adopted economic law missing");
    check(c.p2_catalog_modifier_value(
          "A","building_nationalization_investment_return_add")==0.5,
          "source economic law modifier missing");
    check(c.p2_modifier_ppm("A",
          "building_nationalization_investment_return_add")==500000,
          "source 1.15 effect not converted to fixed point");
    check(c.p2_region_modifier_value("R",
          "building_nationalization_investment_return_add")==0.5,
          "law effect not scoped to actual sovereign");
    check(c.p2_catalog_modifier_value(
          "A","country_government_dividends_reinvestment_add")==0.25,
          "source 1.15 reinvestment not visible");
    c.validate();
}
