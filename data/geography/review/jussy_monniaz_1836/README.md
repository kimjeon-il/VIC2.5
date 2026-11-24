# Jussy–Monniaz land frontier review, 1836 reference

A roughly 75 m, open archival trace near Monniaz, between the vicinities
of modern markers 134 and 135. The historical sides are Swiss Geneva
and the Kingdom of Sardinia, not present-day France. This is a bounded
source review, not a cadastral determination, campaign province, polygon,
or transport crossing. It has no province IDs and is not loaded in play.

## Evidence and reuse

- [Treaty of Turin, 16 March 1816](https://silgeneve.ch/legis/data/rsg_a1_07.htm),
  Article I, and [Geneva State Archives, treaty annex plans no. 7](https://archives-etat-ge.ch/page_de_base/13-le-trace-des-nouvelles-frontieres/).
  The text distinguishes Swiss Jussy from the Sardinian territory ceded
  to link it to the rest of Geneva. The delimitation plans implement the
  treaty and the boundary-marking minutes of 20 October 1819.
- Source of the traced graphic: État de Genève, Direction de l'information
  du territoire, [SITG treaty annex 07 mosaic](https://sitg.ge.ch/donnees/histo-carte-traite-turin-paris-annexe-07-1819),
  assembled and georeferenced from 33 sheets held by the Geneva State
  Archives (Savoie 37 annex). [Le Brécaillon, PDF p. 84](https://museemilitaire.ch/wp-content/uploads/2019/07/brec36.pdf#page=84)
  explains that continuous red ink marks the frontier on these 1:2,500
  plans, except stream thalwegs, which use small crosses. Only the
  visible red stroke on one sheet was sampled, not the adjacent grey
  stroke, shaded field limits, sheet overlaps, or blank portions.
- [Official crop request](https://raster.sitg.ge.ch/arcgis/rest/services/HISTO_CARTE_TRAITE_TURIN_PARIS_ANNEXE_07_1819/ImageServer/exportImage?bbox=2511000%2C1121000%2C2513450%2C1125000&bboxSR=2056&size=1225%2C2000&imageSR=2056&format=jpgpng&interpolation=RSP_BilinearInterpolation&f=html).
  Extraction: 2026-11-24 UTC; the displayed extent matches the requested
  extent. Processing: bounded bilinear raster export, manual sampling in
  LV95, then conversion to WGS84. The PNG itself is not bundled. The
  [SITG open-data terms](https://sitg.ge.ch/ressources/conditions-utilisation-donnees)
  apply to the mosaic and separately to the survey layer; they are not
  an attribution or licence for standalone archive photos.
- Comparison only: État de Genève, [SITG surveyed boundary points](https://sitg.ge.ch/donnees/cad-pointfixe-bf),
  group 802, numbers 134 and 135, extracted 2026-11-24 UTC and compared in
  LV95. The red-stroke samples at the ends are 3.39 m and 2.39 m from
  those modern points, respectively. No fitting or snapping used the
  modern data, and separate historic marker inscriptions were not read
  from this crop. These two differences are not a position-error bound.

`source_pixels.json` records zero-based raster column and row picks. For
column `c` and row `r`, the pixel centre is
`E = 2511000 + 2*(c+.5)`, `N = 1125000 - 2*(r+.5)`.
Reproject those points from EPSG:2056 to EPSG:4326 in longitude/latitude
order to recover `frontier.geojson` (stored at eight decimals for
serialization, not accuracy). The unsnapped LV95 line is 74.93 m long.
The export's 2 m cell size is not the accuracy of the archival map or
its georeferencing. No complete frontier or date-continuity claim is made.
The [modern Le Chambet correction at markers 188–194](https://www.legifrance.gouv.fr/jorf/id/JORFTEXT000000436720)
is outside this marker-based sample; it is not backdated.
