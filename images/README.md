# AnklangIcons - Cursors & icon font for Anklang

SVG based cursor and icon drawings for the Anklang project.

## SCSS Cursor Variables

Cursors from [Anklang/images/cursors/](https://github.com/tim-janik/anklang/tree/master/images/cursors/)
are transformed into SCSS variables holding `CSS cursor` values. For instance:

```SCSS
// SCSS cursor use:
@import 'anklangicons/AnklangCursors.scss';
* { cursor: $anklang-cursor-pen; }
// CSS result:
* { cursor: url(data:image/svg+xml;base64,Abc…xyZ) 3 28, default; }
```

The cursors are SVG drawings, layed out at 32x32 pixels,
and `anklangicons.sh` contains the hotspot coordinates.

## Icon font

Icons from [Anklang/images/icons/](https://github.com/tim-janik/anklang/tree/master/images/icons/)
are transformed into a WOFF2 font and a CSS file `AnklangIcons.css` to be used like this:

```HTML
<!-- HEAD: Load AnklangIcons, preload woff2 for speedups -->
<link rel="stylesheet" href="/assets/AnklangIcons.css" crossorigin>
<link rel="preload"    href="/assets/AnklangIcons.woff2" as="font" type="font/woff2" crossorigin>
<!-- BODY: Display engine1.svg -->
<i class="AnklangIcons-engine1"></i>
```

The icons are SVG drawings and layed out at 24x24 pixels.

The required set of build tools is fairly involved. Building makes use of Inkscape,
ImageMagick and various SVG/font related npm packages, so prebuilt artifacts are
uploaded and can be fetched from:
	https://github.com/tim-janik/anklang/releases/tag/buildassets-v0

## LICENSE

Unless noted otherwise, the AnklangIcons source code forms are licensed
[MPL-2.0](http://mozilla.org/MPL/2.0)
