# Icon source and exports

`icons.svg` is the maintained vector source sheet. The artwork for an icon often consists
of several sibling SVG paths, so exporting an individual path or ID does not
produce the complete icon.

The sheet contains only runtime icons, arranged as 50x50 cells on an 8-column
grid beginning at `(0,0)`. Export frames use semantic `icon-frame-*` IDs and
artwork groups use matching `art-*` IDs.

Each transparent export frame has a readable ID and carries its output path
directly, for example:

```xml
<rect id="icon-frame-resume" data-icon-path="resume_icon.png" ... />
```

`tools/export_svg_icons.py` discovers those frames directly from the SVG and
asks Inkscape to crop to each frame while rendering the full sheet, so sibling
paths remain visible. There is no separate manifest to keep synchronized.

Examples:

```powershell
python tools/export_svg_icons.py --size 10
python tools/export_svg_icons.py --size 20
python tools/export_svg_icons.py --size 30
python tools/export_svg_icons.py --size 40
python tools/export_svg_icons.py --size 50
python tools/export_svg_icons.py --size 60
```

By default these produce `assets/icons/icons_10`, `icons_20`, and so on.

After exporting the PNG sets, rebuild the runtime atlases with:

```powershell
python tools/pack_icon_atlas.py
```

This creates exact-size atlases from 10 through 60 pixels. They share one fixed
UV layout. Since menu icons render at `10 * menuScale` physical pixels, integer
menu scales select a matching atlas without resampling.
Outputs preserve the original asset folder structure. Add a new icon by drawing
it inside a square export frame, giving that frame a semantic ID such as
`icon-frame-my-new-icon`, and setting `data-icon-path="my_new_icon.png"` on
that frame. Old absolute
`inkscape:export-filename` attributes are legacy metadata and are not used.

The runtime atlas also contains the raster `header_icon` and `vb_cartridge`.
The cartridge tiers are generated from `assets/icons/cartridge.png` with
nearest-neighbor scaling so its pixel art remains sharp.
