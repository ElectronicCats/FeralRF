# Increment 3 reduced IEEE MAC profile

This directory records the normative basis and project boundary for the first
bounded IEEE 802.15.4 MAC parser. It does not contain or redistribute a copy of
IEEE Std 802.15.4.

    Inc3 profile:
    IEEE_802_15_4_2015_R22_LEGACY_DATA_MINIMAL

    Normative gate:
    GO_REDUCED_INC3

    Coverage:
    Version-0 unsecured Data,
    short/short addressing,
    PAN compression=1,
    sequence present,
    no IEs,
    explicit FCS policy.

## Supported row

The parser accepts a frame as structurally supported only when every field
below matches:

| FCF field | Required value |
|---|---|
| Frame Version | 0 |
| Frame Type | Data (0b001) |
| Destination Addressing Mode | short (0b10) |
| Source Addressing Mode | short (0b10) |
| PAN ID Compression | 1 |
| Sequence Number Suppression | 0 |
| IE Present | 0 |
| Security Enabled | 0 |

The resulting field order is:

    FCF
    Sequence Number
    Destination PAN ID
    Destination Short Address
    Source Short Address
    MAC Payload
    FCS, only when explicit capture policy says PRESENT

The MAC header is therefore exactly nine octets: 2 + 1 + 2 + 2 + 2.
Destination PAN ID is present; Source PAN ID is omitted. Both addresses are
two-octet short addresses.

## Why coverage is intentionally reduced

The pre-implementation gate established deterministic offsets for this single
legacy Zigbee row. It did not approve the complete version-specific PAN-ID
presence tables, IE layouts, auxiliary security header, or other frame
formats. The parser consequently rejects legal-but-out-of-profile encodings as
unsupported instead of guessing their conditional offsets.

This profile is not generic IEEE 802.15.4 support.

Unsupported by project policy:

- Frame Versions 1 and 2;
- Beacon, ACK, MAC Command, Multipurpose, and other non-Data types;
- extended, source-only, destination-only, or absent addressing;
- PAN ID Compression equal to zero;
- Sequence Number Suppression equal to one;
- IE Present equal to one;
- MAC Security Enabled equal to one;
- auxiliary security headers, MICs, and decryption;
- FCS calculation or verification;
- Zigbee classification and all NWK or higher-layer parsing.

## Evidence, scope, and implementation

**Normative facts** establish Data frame encoding, short-address width, the
legacy compressed-PAN rule, sequence and IE requirements, field order, and the
two-octet Zigbee 2.4 GHz FCS. The individual sources and their limitations are
registered in [SOURCES.md](SOURCES.md).

**Project scope decisions** accept only the exact row above, require explicit
PHY/PHR/FCS/schema context, and treat every other FCF layout as unsupported.
This deliberately leaves much legal IEEE traffic unparsed.

**Implementation behavior** uses bounded reads, retains the immutable
RawCapture, reports structured context/truncation/unsupported issues, and
reserves the final two octets only for FCS=PRESENT. FCS=ABSENT reserves
nothing. FCS=UNKNOWN returns missing_context without a payload span. A
structurally supported result does not assert that CRC metadata or FCS bytes
have been validated.

## Remaining normative limitations

Before expanding this profile, a new gate must establish the exact applicable
rules for every added frame version, type, addressing/PAN combination,
sequence-suppression case, IE boundary, or MAC-security structure. Later IEEE
editions must not silently replace the IEEE 802.15.4-2015 baseline used by
Zigbee R22.

