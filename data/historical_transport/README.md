# VIC2.5 historical physical transport evidence (1801 onward)

This directory contains **observations**, not fictional transport capacity.

## Included now

- `rail_openings.tsv`: 8 independently cited early railway sections across
  North America, Europe, South America, Asia and Africa; exact dates are
  opening evidence, NOT full rail geometry or confirmed goods services.
  Only the Japan Shimbashi–Yokohama section has an independently published
  freight-service start date in this initial sample (15 September 1873).
- `observed_freight_metrics.tsv`: four historical flatboat freight-rate and
  passage-time **observations** for the Louisville–New Orleans downstream
  corridor, reported by Klein and Matthews from Haites, Mak and Walton.
  Original units and reported period/ranges are kept. The observations do
  not provide the maximum monthly line capacity or a validated conversion
  into VIC2.5's game-currency unit.
- `scripts/compile_historical_rail_lifetimes.py`: stdlib-only date and
  independent GIS crosswalk validator. Without a proven province-polygon
  geometry join it creates **no** game-world link. It can generate the
  existing `physical_line_lifetimes.tsv` contract for already verified
  `geographic_links.tsv` from the older E/G pipeline.

## To make a real service playable

A line's opening alone is not a ticket, a freight service, a monthly
capacity or a contract. Continue to use the existing
`scripts/build_verified_transport_links.py` and
`include/vic25/verified_transport_seed.hpp` pipeline:

1. Match a historical **actual track/route geometry** to actual Phase E
   province polygons and retain its source_feature / geometry provenance.
2. Import independently documented freight carrier, service start and end
   (game months), **positive physical monthly capacity**, nonnegative
   freight charge in an explicitly chosen game accounting unit and positive
   real freight transit days. Each record must carry its source.
3. Cross-check each service against historical physical-line validity,
   territorial membership and operational evidence. Only after these checks
   should `seed_transport_edge` create an active dated G transport link.
4. A route whose service charge, capacity or time is missing remains
   **NOT_TRADABLE**, even when its tracks are certain.

Run the stdlib chronology/source check with:

```sh
python3 scripts/compile_historical_rail_lifetimes.py
```

To emit actual E/G physical validity bounds after the independent GIS join:

```sh
python3 scripts/compile_historical_rail_lifetimes.py \
  --links path/to/geographic_links.tsv \
  --crosswalk path/to/checked_rail_line_crosswalk.tsv \
  --output path/to/physical_line_lifetimes.tsv
```

The crosswalk columns are `link_id,line_id,alignment_source_url`
(tab-delimited). The underlying `geographic_links.tsv` must already
identify real from/to Phase E province IDs and a rail feature.

## Source inventory for further historical geographic coverage

- US 1826–1911 railway lines and multi-modal 1830–1920 transport networks:
  https://www.nber.org/research/data/transportation-economics-21st-century-rail-data
- European yearly track geometry 1834–1922, RShapes:
  https://icr.ethz.ch/data/rshapes/
  **CC BY-NC-SA 4.0**: redistribution/commercial licensing review required.
- Railway History Database, country-specific lines/facts and paths:
  https://github.com/railwayhistory/database
  Facts CC BY 4.0, geometry paths ODbL.
- UK coastal voyage records (c.1830–1845) and 1851/1861/1881 rail networks:
  https://www.campop.geog.cam.ac.uk/research/occupations/datasets/catalogues/transport/
- Historical global shipping observations, CLIWOC:
  https://datashare.ed.ac.uk/items/7754a148-b2fa-43b4-b413-d86e5febd429
- Physical historical line shapes may also be reconciled with
  https://www.openhistoricalmap.org/ ; current OSM line geometry does not
  independently establish 19th-century services.

## Known gaps

These eight sections are **not a global transport network**. There is no
complete continent-wide province crosswalk, no machine-ingested world-scale
rail/road/river/sea/air service timetable, and no globally documented
freight rate, transport-day, volume or capacity table in this repository.

Avoid substituting scheduled passenger counts for cargo tonnage, average
historical throughput for maximum route capacity, dollars per ton for the
game-currency freight_per_unit, or a dated track for a dated freight service.
