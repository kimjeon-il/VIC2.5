// 1.15.0 amendment sponsor effects apply to a real sponsoring interest group only.
inline void p2_catalog_append_sponsor_effects(std::vector<P2CatalogModifier>& m) {
  m.push_back({"amendment_preserved_bureaucratic_caste","interest_group_pol_str_mult",0.15,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase","interest_group_pol_str_mult",0.2,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_approval_increase","interest_group_pol_str_mult",0.2,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_approval_increase","interest_group_in_government_approval_add",2,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase_negotiation_1","interest_group_pol_str_mult",0.2,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase_negotiation_1","interest_group_in_government_approval_add",1,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase_negotiation_2","interest_group_pol_str_mult",0.3,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase_negotiation_2","interest_group_in_government_approval_add",2,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase_negotiation_3","interest_group_pol_str_mult",0.4,false,false,true});
  m.push_back({"amendment_generic_sponsor_polstr_increase_negotiation_3","interest_group_in_government_approval_add",3,false,false,true});
}
