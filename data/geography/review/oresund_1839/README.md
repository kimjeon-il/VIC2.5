# Øresund, 1839 chart review

This bounded historical chart reconstruction is a geography review fixture.
It is not a campaign or default map, a modern survey, an exact 1836 coastline,
or a maritime jurisdiction boundary. No archival scans are bundled.

## Source and registration

Gustaf af Klint, *Karta öfver Sundet emellan Sverige och Dannemark*,
Stockholm, 1839. Bibliothèque nationale de France, Cartes et plans,
GE SH 19 PF 8 DIV 2 P 24,
[Gallica btv1b53169983h](https://gallica.bnf.fr/ark:/12148/btv1b53169983h).
The [Wikimedia Commons source item](https://commons.wikimedia.org/wiki/File:Karta_%C3%B6fver_Sundet_emellan_Sverige_och_Dannemark_(...)_%C3%96stersj%C3%B6n_och_Kattegat_-_af_Gustaf_af_Klint_-_btv1b53169983h.jpg)
records the public-domain status of the historical work. The source JPEG
(8,312 × 11,814 pixels) has SHA-256
`08e8bbb1a9fcb7fa385756333b9789d59dc23113b701c93f0694d4394511a0ca`.
The outlines were traced and clipped from the chart; harbour symbols and
stippled shoals are not interpreted as independent land polygons.

`source_pixels.json` preserves review-image pixel coordinates and cut bounds
(1,105 × 1,570, x right and y down). Its coordinates are not longitude and
latitude. The generator uses a local planar approximation of the engraved
graticule. The chart puts its Ferro meridian 20°30′ west of Paris;
[IGN's conventional Paris meridian](https://geodesie.ign.fr/quelle-est-la-difference-entre-le-meridien-de-greenwich-et-le-meridien-de-paris)
is 2°20′14.025″ east of Greenwich. This is approximate registration, not a
rigorous datum transformation. The indicative discrepancies at Kronborg,
Kärnan and St Ibb are 137 m, 424 m and 808 m, respectively. These checks use
representative positions, not surveyed points or a maximum error bound.
Seven stored decimals are for stable shared edges, not positional accuracy.

The [1821 Klint/Tisch chart](https://gallica.bnf.fr/ark:/12148/btv1b53169981m)
corroborates the broad layout but is not an independent survey.
[Von Beck's Helsingør map](https://commons.wikimedia.org/wiki/File:Helsing%C3%B8r_(1824-1849).jpg)
(drawn 1824, catalogue range 1824–1849, CC BY 4.0) and the
[Swedish IIW 201 sheet](https://sok.riksarkivet.se/bildvisning/K0035788_00001)
(1812–1820, Public Domain Mark) are independent comparison sources, not
sources used to construct these polygons. They do not yet confirm every
segment as of 1836. In particular, the ports and chart-symbol sections
are retained at chart scale rather than treated as precisely surveyed.

## What the files mean

- `land.geojson`: three local cutouts, North Zealand (DK, ≈125.83 km²),
  West Scania (SE, ≈193.03 km²), and Ven (SE, ≈6.99 km²).
- `water.geojson`: one connected part of Øresund, touching all three
  lands. Ven is a hole in the water polygon. No Danish–Swedish water
  boundary is drawn. Neither land pair shares an edge.
- `scope.geojson`: the review window. Its outer edges are crop limits, never
  historical borders.
- `engine-witness.csv`: three local IDs, land tags, rounded geodesic areas
  and one local sea reference (101). These are not canonical province IDs
  or a shipping network. No carrier, capacity, cost, or service is asserted.

The land tags are grounded separately from the shoreline: the
[1658 Treaty of Roskilde](https://danmarkshistorien.lex.dk/Roskildefreden%2C_26._februar_1658)
for Scania and the [Landskrona city account](https://www.mynewsdesk.com/se/landskrona_stad/pressreleases/jubileum-paa-ven-2010-393609)
for Ven's status in the 1660 peace.

Rebuild into a separate folder from the repository root, with no GIS or
network dependency:

```sh
python scripts/build_oresund_review.py --output build/oresund-review
python tests/test_oresund_review.py
```

For the independent GIS check, compare the union of the four polygons
with the cut; no gaps, positive-area overlaps, or erroneous land edges
are expected. The water polygon should contain one hole, and both sides
of Ven and the narrow strait must remain open. This checks geometry,
not period navigation.
