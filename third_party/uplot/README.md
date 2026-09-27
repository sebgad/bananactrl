# uPlot 1.6.32

https://github.com/leeoniya/uPlot, MIT licence (see LICENSE). Used by `data/graphs.html`.

Files in `data/` (served from LittleFS, so the graphs work without internet, e.g. in SoftAP mode):

- `uPlot.min.js.gz` = `gzip -9n dist/uPlot.iife.min.js` (served with `Content-Encoding: gzip`; the minified
  file keeps its licence banner)
- `uPlot.min.css`   = `dist/uPlot.min.css`

Update: `npm pack uplot`, unpack, regenerate both files as above.
