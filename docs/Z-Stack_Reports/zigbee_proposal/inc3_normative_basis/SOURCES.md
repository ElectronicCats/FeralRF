# Increment 3 normative source register

No remote normative document is stored in this directory. No hash is recorded
for a remote source. The third-party location consulted for IEEE text is
recorded as provenance only; it is not identified as the normative owner.

## INC3-IEEE-IDENTITY

- **Title:** IEEE Std 802.15.4-2015, *IEEE Standard for Low-Rate Wireless Networks*
- **Owner/publisher:** IEEE
- **Revision/date:** 2015 edition; published 2016-04-22
- **URL:** <https://standards.ieee.org/ieee/802.15.4/5788/>
- **Source type:** official standard identity/version record
- **Authority level:** OFFICIAL_REVISION_HISTORY
- **Sections used:** publication identity, supersession, corrigendum, and errata links
- **What it establishes:** the exact IEEE edition identified by Zigbee R22 and
  its relationship to Cor 1-2018.
- **What it does NOT establish:** the detailed MAC field layout or PAN-ID
  presence rule.
- **Access/provenance note:** official IEEE Standards Association record;
  the complete normative text remains subject to IEEE access and licensing.

## INC3-IEEE-2015-TEXT

- **Title:** IEEE Std 802.15.4-2015, *IEEE Standard for Low-Rate Wireless Networks*
- **Owner/publisher:** IEEE
- **Revision/date:** 2015 edition
- **Consulted location:** <https://device.report/m/b392bb03aacab1923d8ead9cae63167a6cc78b4596c6226654388f427233a5d2.pdf>
- **Source type:** consulted third-party mirror of a licensed IEEE Xplore copy
- **Authority level:** FINAL_NORMATIVE applies to IEEE Std 802.15.4-2015,
  not to the mirror or hosting site
- **Sections used:** Figure 7-1; §7.2.1.1 and Table 7-1; §7.2.1.5;
  §§7.2.1.6–7.2.1.10; Tables 7-3 and 7-4
- **What it establishes:** Data=0b001; version-0 legacy compression behavior;
  Destination PAN present and Source PAN omitted for the approved compressed
  short/short row; two-octet short addresses; sequence present; IE absent; and
  MAC field order.
- **What it does NOT establish:** authorization to redistribute the standard,
  generic parser coverage, Zigbee classification, or project support for
  other legal IEEE layouts.
- **Access/provenance note:** the consulted file displays IEEE Xplore licensed
  copy markings. It was read for this gate but was not downloaded into or
  retained by the repository. The normative authority is IEEE.

## INC3-IEEE-COR1

- **Title:** IEEE 802.15.4-2015/Cor 1-2018, *Corrigendum 1*
- **Owner/publisher:** IEEE
- **Revision/date:** Cor 1-2018; published 2018-07-09
- **URL:** <https://standards.ieee.org/ieee/802.15.4-2015_Cor_1/7059/>
- **Source type:** official corrigendum identity/history record
- **Authority level:** OFFICIAL_REVISION_HISTORY
- **Sections used:** corrigendum identity and publication history
- **What it establishes:** the final corrigendum's identity, relationship to
  IEEE 802.15.4-2015, and publication date.
- **What it does NOT establish:** the detailed correction or support for
  extended addresses and later-edition layouts.
- **Access/provenance note:** no corrigendum PDF is stored locally and no
  remote-file hash is claimed.

## INC3-IEEE-COR1-HISTORY

- **Title:** IEEE 802 Executive Committee Cor 1 submission history
- **Owner/publisher:** IEEE 802
- **Revision/date:** submission discussion; 2017-03-15
- **URL:** <https://grouper.ieee.org/groups/802/secmail/msg21043.html>
- **Source type:** official IEEE revision-process evidence
- **Authority level:** OFFICIAL_REVISION_HISTORY
- **Sections used:** stated scope and working-group chair explanation
- **What it establishes:** the correction concerns transmission of the 64-bit
  MAC address and restores the method used by earlier editions. The approved
  16-bit short/short PAN-presence row is outside that correction.
- **What it does NOT establish:** generic extended-address parser coverage or
  every editorial change in the final corrigendum.
- **Access/provenance note:** official IEEE 802 mailing-list archive; used only
  to establish revision history. No attachment is stored in the repository.

## INC3-ZIGBEE-R22

- **Title:** *Zigbee Specification*, document 05-3474-22
- **Owner/publisher:** Zigbee Alliance (now Connectivity Standards Alliance)
- **Revision/date:** Revision 22 1.0; 2017-04-19
- **URL:** <https://zigbeealliance.org/wp-content/uploads/2021/10/docs-05-3474-22-0csg-zigbee-specification-1.pdf>
- **Source type:** Zigbee normative specification
- **Authority level:** ZIGBEE_NORMATIVE_CONSTRAINT
- **Sections used:** Annex D §§D.4, D.5, and D.10
- **What it establishes:** the IEEE 802.15.4-2015 relationship; version-0
  unsecured MAC Data behavior; short/intra-PAN assumptions; and a two-octet
  FCS for the selected Zigbee 2.4 GHz profile.
- **What it does NOT establish:** the complete version-0 PAN-presence
  algorithm independently of IEEE §7.2.1.5, generic IEEE support, or Zigbee
  provenance for arbitrary IEEE frames.
- **Access/provenance note:** linked public specification; copyright and
  access terms remain with its owner. The document is not copied here.

## INC3-PROJECT-PROPOSAL

- **Title:** *FeralRF Native Zigbee Integration Proposal*
- **Owner/publisher:** FeralRF project
- **Revision/date:** project revision 1.2; 2026-11-04
- **Location:** ../FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md
- **Source type:** project requirements and architecture
- **Authority level:** PROJECT_ANALYSIS
- **Sections used:** R-MAC-01, R-RAW-01, R-ERR-01, R-SEC-01, normative profile,
  parser boundaries, and validation mapping
- **What it establishes:** project objectives, conservative failure behavior,
  raw preservation, and the requirement not to infer Zigbee identity.
- **What it does NOT establish:** IEEE wire-format rules.
- **Access/provenance note:** repository-local project document.

## INC3-PROJECT-SOURCES

- **Title:** *FeralRF Native Zigbee Integration Sources*
- **Owner/publisher:** FeralRF project
- **Revision/date:** accessed for this increment on 2026-11-10
- **Location:** ../FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md
- **Source type:** project source and evidence register
- **Authority level:** PROJECT_ANALYSIS
- **Sections used:** IEEE-D01, CSA-D01, and requirement coverage entries
- **What it establishes:** source identity, intended authority ordering, and
  known access/licensing limits.
- **What it does NOT establish:** normative MAC offsets by itself.
- **Access/provenance note:** repository-local source register.

## Secondary cross-checks

No TI, Wireshark, Scapy, or other implementation source establishes any rule
in this reduced profile. None was required to clear the gate.

