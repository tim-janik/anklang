// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

// Support require() in nodejs or rely on browserified window.require
const require = globalThis.require || (await import ("module")).createRequire (import.meta.url.replace (/^.*?:\/\/\//, '/'));

const CssColor = require ('css-color-converter'); // browserified or node_modules

import * as Z from 'zcam-js';
const { clamp } = Z;

export const color_names = {
};

export const color_values = Array.from (new Set (Object.values (color_names)).values());

/// Calculate sRGB from `hue, saturation, value`.
export function srgb_from_hsv (hue,		// 0…360: 0=red, 120=green, 240=blue
			       saturation,	// 0…1
			       value)		// 0…1
{
  saturation = clamp (saturation, 0, 1);
  value = clamp (value, 0, 1);
  const center = Math.floor (hue / 60);
  const frac = hue / 60 - center;
  const v1s = value * (1 - saturation);
  const vsf = value * (1 - saturation * frac);
  const v1f = value * (1 - saturation * (1 - frac));
  let r, g, b;
  switch (center) {
    case 6:
    case 0: // red
      r = value;
      g = v1f;
      b = v1s;
      break;
    case 1: // red + green
      r = vsf;
      g = value;
      b = v1s;
      break;
    case 2: // green
      r = v1s;
      g = value;
      b = v1f;
      break;
    case 3: // green + blue
      r = v1s;
      g = vsf;
      b = value;
      break;
    case 4: // blue
      r = v1f;
      g = v1s;
      b = value;
      break;
    case 5: // blue + red
      r = value;
      g = v1s;
      b = vsf;
      break;
  }
  return {r,g,b};
}

/// Calculate `{ hue, saturation, value }` from `srgb`.
export function hsv_from_srgb (srgb) {
  const {r,g,b} = Z.srgb_from (srgb);
  const value = Math.max (r, g, b);
  const delta = value - Math.min (r, g, b);
  const saturation = value == 0 ? 0 : delta / value;
  let hue = 0;
  if (saturation > 0)
    {
      if (r == value)
	{
	  hue = 0 + 60 * (g - b) / delta;
	  if (hue <= 0)
	    hue += 360;
	}
      else if (g == value)
	hue = 120 + 60 * (b - r) / delta;
      else // b == value
	hue = 240 + 60 * (r - g) / delta;
    }
  return { hue, saturation, value };
}

const sRGB_viewing_conditions = {
  Fs: Z.ZCAM_DIM,       // DIM comes closest to CIELAB L* in ZCAM and CIECAM97
  Yb: 9.1,              // Background luminance factor so Jz=50 yields #777777
  La: 80,               // La = Lw * Yb / 100; Safdar21, ZCAM, a colour appearance model
  Xw: Z.ZCAM_D65.x, Yw: Z.ZCAM_D65.y, Zw: Z.ZCAM_D65.z,
};
const default_gamut = new Z.Gamut (sRGB_viewing_conditions);
const atsamples = [0, 0.005, 0.01, 0.02, 0.038, 0.05, 0.062, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.938, 0.95, 0.962, 0.98, 0.99, 0.995, 1];

const ZHSV_EXP = 1.7; // exponent for ZHSV to approximate saturation distribution of HSV
const ZHSL_EXP = 1.7; // exponent for ZHSL to approximate saturation distribution of HSL

/// Class with logic and spline approximations to calculate ZHSV.
export class HueSaturation {
  constructor (huelike, gamut = undefined) {
    let hz = huelike + 0;
    if (isNaN (hz))
      hz = isNaN (huelike.Hz) ? huelike.hz : Z.zcam_hue_angle (huelike.Hz);
    while (hz > 360) hz -= 360;
    while (hz < 0)   hz += 360;
    this.gamut = gamut || default_gamut;
    this.zcusp = this.gamut.find_cusp (hz); // s=1 v=1 l=cusp.Jz/100
    this.value = undefined;
    this.zv0 = undefined;	// zcam @ v=0…1 s=0
    this.zv1 = undefined;	// zcam @ v=0…1 s=1
    this.jz2cz = undefined;	// maximize_Cz (Jz) under hue
    this.z1s = undefined;	// zcam @ v=1   s=0…1
    this.cz2jz = undefined;	// maximize_Jz (Cz) under hue
    this.lightness = undefined;
  }
  make_splines() {
    const zcusp = this.zcusp, viewing = zcusp.viewing, gamut = this.gamut;
    const cxs = [], cys = [];
    // approximate Jz = maximize_Jz (Cz) under hue
    for (const s of atsamples) {
      const Cz = zcusp.Cz * s;
      const z1s = { hz: zcusp.hz, Cz, Jz: zcusp.Jz, viewing };
      z1s.Jz = gamut.maximize_Jz (z1s);
      cxs.push (Cz);
      cys.push (z1s.Jz);
    }
    this.cz2jz = new Z.CubicSpline (cxs, cys);
    // approximate Cz = maximize_Cz (Jz) under hue for 0…zcusp.Jz
    cxs.length = 0;
    cys.length = 0;
    for (const v of atsamples) {
      const Jz = zcusp.Jz * v;
      const zjp = { hz: zcusp.hz, Jz, Cz: 0, viewing };
      zjp.Cz = gamut.maximize_Cz (zjp);
      cxs.push (Jz);
      cys.push (zjp.Cz);
    }
    this.jz2cz = new Z.CubicSpline (cxs, cys);
    // approximate Cz = maximize_Cz (Jz) under hue for zcusp.Jz…100
    cxs.length = 0;
    cys.length = 0;
    for (const v of atsamples) {
      const Jz = zcusp.Jz + (100 - zcusp.Jz) * v;
      const zjp = { hz: zcusp.hz, Jz, Cz: 0, viewing };
      zjp.Cz = gamut.maximize_Cz (zjp);
      cxs.push (Jz);
      cys.push (zjp.Cz);
    }
    this.jz2cz.add_segment (cxs, cys);
  }
  set_value (value) {
    const zcusp = this.zcusp, viewing = zcusp.viewing;
    this.value = clamp (value, 0, 1);
    this.zv0 = { hz: zcusp.hz, Jz: this.value * 100, Cz: 0, viewing }; // zcam @ s=0 v=0…1
    this.zv1 = undefined;
    this.z1s = undefined;
  }
  ensure_zv1() {
    if (this.zv1) return;
    const zcusp = this.zcusp, viewing = zcusp.viewing, gamut = this.gamut;
    this.zv1 = { hz: zcusp.hz, Jz: zcusp.Jz * this.value, Cz: 0, viewing };
    if (!this.jz2cz)
      this.zv1.Cz = gamut.maximize_Cz (this.zv1); // zcam @ s=1 v=0…1
    else
      this.zv1.Cz = this.jz2cz.splint (this.zv1.Jz);
  }
  ensure_z1s() {
    if (this.z1s) return;
    const zcusp = this.zcusp, viewing = zcusp.viewing, gamut = this.gamut;
    const s = this.saturation ** (1/ZHSV_EXP); // adjustment to match sRGB-HSV saturation
    this.z1s = { hz: zcusp.hz, Cz: zcusp.Cz * s, Jz: zcusp.Jz, viewing };
    if (!this.cz2jz)
      this.z1s.Jz = gamut.maximize_Jz (this.z1s); // zcam @ s=0…1 v=1
    else
      this.z1s.Jz = this.cz2jz.splint (this.z1s.Cz);
    return s;
  }
  zcam_from_zhsv (saturation) {
    const zcusp = this.zcusp, viewing = zcusp.viewing;
    this.ensure_zv1(); // needs this.value
    this.saturation = clamp (saturation, 0, 1);
    this.z1s = undefined; // invalidated when this.saturation changes
    this.ensure_z1s(); // needs this.saturation
    // assume v=1, construct from hue and saturation
    const zcam = { hz: this.z1s.hz, Jz: this.z1s.Jz, Cz: this.z1s.Cz, viewing };
    // translating cusp along value yields zv1
    const rJz = this.zv1.Jz / zcusp.Jz;
    const rCz = this.zv1.Cz / zcusp.Cz;
    // apply zv1/cusp ratios
    zcam.Jz *= rJz;
    zcam.Cz *= rCz;
    return zcam;
  }
  zhsv_saturation_from_Cz (Cz) {
    this.ensure_zv1(); // needs this.value
    const s = Cz / this.zv1.Cz;
    const saturation = clamp (s ** ZHSV_EXP, 0, 1);
    return saturation;
  }
  srgb_from_zhsv (saturation) {
    const zcam = this.zcam_from_zhsv (saturation);
    const {r,g,b} = Z.srgb_from_zcam_8bit (zcam, this.gamut.viewing);
    return {r,g,b};
  }
  srgb_from_zhsl (saturation, lightness) {
    const zcam = this.zcam_from_zhsl (saturation, lightness);
    const {r,g,b} = Z.srgb_from_zcam_8bit (zcam, this.gamut.viewing);
    return {r,g,b};
  }
  zcam_from_zhsl (saturation, lightness) {
    const zcusp = this.zcusp, viewing = zcusp.viewing, gamut = this.gamut;
    lightness = clamp (lightness, 0, 1);
    const zcam = { hz: zcusp.hz, Jz: lightness * 100, Cz: 0, viewing }; // zcam @ s=0 l=0…1
    saturation = clamp (saturation, 0, 1);
    if (saturation > 0) {
      if (!this.jz2cz)
	zcam.Cz = gamut.maximize_Cz (zcam); // zcam @ s=1 l=0…1
      else
	zcam.Cz = this.jz2cz.splint (zcam.Jz);
      zcam.Cz *= saturation ** (1/ZHSL_EXP);
    }
    return zcam;
  }
}

/// Calculate `{ hue, saturation, lightness }` from `srgb`.
export function zhsl_from (srgb, gamut = undefined) {
  gamut = gamut || default_gamut;
  const {r,g,b} = Z.srgb_from (srgb);
  const zcam = Z.zcam_from_srgb ({r,g,b}, gamut.viewing);
  const hue = zcam.hz;
  const lightness = zcam.Jz * 0.01;
  const maxCz = zcam.Cz > 0 ? gamut.maximize_Cz (zcam) : 1;
  const s = zcam.Cz / maxCz;
  const saturation = s ** ZHSL_EXP;
  return {hue, saturation, lightness};
}

/// Calculate sRGB from `{ hue, saturation, value }`.
export function srgb_from_zhsv (hue, saturation, value, gamut = undefined) {
  const hso = new HueSaturation (hue, gamut);
  hso.set_value (value);
  return hso.srgb_from_zhsv (saturation);
}

/// Calculate hexadecimal color from `{ hue, saturation, value }`.
export function hex_from_zhsv (hue, saturation, value, gamut = undefined) {
  return Z.srgb_hex (srgb_from_zhsv (hue, saturation, value, gamut));
}

/// Calculate `{ hue, saturation, value }` from `srgb`.
export function zhsv_from (srgb, gamut = undefined) {
  gamut = gamut || default_gamut;
  const {r,g,b} = Z.srgb_from (srgb);
  const pz = Z.zcam_from_srgb ({r,g,b}, gamut.viewing);
  const hue = pz.hz;
  const hso = new HueSaturation (hue, gamut);
  let va = 0, vb = 1;
  while (Math.abs (va - vb) > 0.001) {
    const vc = (va + vb) * 0.5;
    hso.set_value (vc);
    const s = hso.zhsv_saturation_from_Cz (pz.Cz);
    const zc = hso.zcam_from_zhsv (s);
    if (zc.Jz > pz.Jz)
      vb = vc;
    else
      va = vc;
  }
  const value = (va + vb) * 0.5;
  hso.set_value (value);
  const saturation = hso.zhsv_saturation_from_Cz (pz.Cz);
  return { hue, saturation, value };
}

/// Calculate sRGB from `{ hue, saturation, value }`.
export function srgb_from_zhsl (hue, saturation, lightness, gamut = undefined) {
  const hso = new HueSaturation (hue, gamut);
  return hso.srgb_from_zhsl (saturation, lightness);
}

/// Calculate hexadecimal color from `{ hue, saturation, value }`.
export function hex_from_zhsl (hue, saturation, lightness, gamut = undefined) {
  return Z.srgb_hex (srgb_from_zhsl (hue, saturation, lightness, gamut));
}

export function zhsl (hz, s, l) {
  return hex_from_zhsl (parseFloat (hz), parseFloat (s), parseFloat (l));
}

export function zHsl (Hz, s, l) {
  return hex_from_zhsl (Z.zcam_hue_angle (parseFloat (Hz)), parseFloat (s), parseFloat (l));
}

export function zhsv (hz, s, v) {
  return hex_from_zhsv (parseFloat (hz), parseFloat (s), parseFloat (v));
}

export function zHsv (Hz, s, v) {
  return hex_from_zhsv (Z.zcam_hue_angle (parseFloat (Hz)), parseFloat (s), parseFloat (v));
}

const d255 = 1 / 255;

/// Yield [ R, G, B, A ] from `color`.
function color2rgba (color) {
  try {
    const c = CssColor.fromString (color);
    const [r,g,b,a] = c.toRgbaArray();
    return [r * d255, g * d255, b * d255, a !== undefined ? a : 1];
  } catch (ex) {
    throw new Error ("CSS: invalid color: " + color + "\n" + ex);
  }
}

function css_rgba ([r,g,b,a]) {
  const rp = clamp (r * 100, 0, 100).toFixed (1);
  const gp = clamp (g * 100, 0, 100).toFixed (1);
  const bp = clamp (b * 100, 0, 100).toFixed (1);
  const ap = a === undefined ? '' : ',' + a.toFixed (4).replace (/([0-9])0+$/, '$1');
  return `rgb(${rp}%,${gp}%,${bp}%${ap})`;
}

const z_assignops = [ '=', '+=', '-=', '*=', '/=' ];

// Modify value according to op
function value_assignops (value, op, num, perc, nmax) {
  const n = perc ? num * 0.01 : num;
  switch (op) {
    case '=':  return perc ? n * nmax() : n;
    case '+=': return value + (perc ? n * value : n);
    case '-=': return value - (perc ? n * value : n);
    case '*=': return value * n;
    case '/=': return Math.abs (n) > 0 ? value / n : 999999999;
  }
}

/// Re-assign a specific color property.
function zmod_assignop (col, prop, op, num, perc = false) {
  // RGBA mods
  if (prop == 'a' || prop == 'alpha') {
    col.alpha = value_assignops (col.alpha, op, num, perc, () => 1);
    return;
  }
  // ZCAM mods
  const gamut = default_gamut, viewing = gamut.viewing;
  if (!col.zcam)
    col.zcam = gamut.zcam (col.rgb);
  const zcam = col.zcam;
  const zmaxCz = () => ({ viewing, Jz: zcam.Jz, hz: zcam.hz,
			  Cz: gamut.maximize_Cz (col.zcam) });
  if (prop.toLowerCase() == 'jz')
    col.zcam = { viewing, Cz: zcam.Cz, hz: zcam.hz,
		 Jz: value_assignops (Z.zcam_ensure_Jz (zcam).Jz, op, num, perc,
				      () => 100) };
  else if (prop.toLowerCase() == 'cz')
    col.zcam = { viewing, Jz: zcam.Jz, hz: zcam.hz,
		 Cz: value_assignops (Z.zcam_ensure_Cz (zcam).Cz, op, num, perc,
				      () => zmaxCz().Cz) };
  else if (prop.toLowerCase() == 'mz') {
    col.zcam = { viewing, Jz: zcam.Jz, hz: zcam.hz,
		 Mz: value_assignops (Z.zcam_ensure_Mz (zcam).Mz, op, num, perc,
				      () => Z.zcam_ensure_Mz (zmaxCz()).Mz) };
    col.zcam = Z.zcam_ensure_Cz (col.zcam);
  } else if (prop.toLowerCase() == 'sz') {
    col.zcam = { viewing, Jz: zcam.Jz, hz: zcam.hz,
		 Sz: value_assignops (Z.zcam_ensure_Sz (zcam).Sz, op, num, perc,
				      () => Z.zcam_ensure_Sz (zmaxCz()).Sz) };
    col.zcam = Z.zcam_ensure_Cz (col.zcam);
  } else if (prop == 'hz' || prop == 'hZ')
    col.zcam = { viewing, Jz: zcam.Jz, Cz: zcam.Cz,
		 hz: value_assignops (Z.zcam_ensure_hz (zcam).hz, op, num, perc,
				      () => 360) };
  else if (prop == 'Hz' || prop == 'HZ') {
    col.zcam = { viewing, Jz: zcam.Jz, Cz: zcam.Cz,
		 Hz: value_assignops (Z.zcam_ensure_Hz (zcam).Hz, op, num, perc,
				      () => 400) };
    col.zcam = Z.zcam_ensure_hz (col.zcam);
  } else
    console.warn ("CSS: invalid zmod: ", prop, op, num, perc ? '%' : '');
  return zcam;
}

const split_ops = /\s*([=*\/&|!+-]+)\s*|\s+/;

/// Apply a variety of modifications to an input color.
export function zmod (colorlike, ...mods) {
  const gamut = default_gamut;
  const col = { alpha: 1, rgba: undefined, zcam: undefined,  };
  // support hz=[0-9] as Cusp
  if (colorlike.indexOf ('=') >= 2) {
    const toks = colorlike.trim().split (split_ops);
    if (toks.length == 3 && toks[0].toLowerCase() === 'hz' && toks[1] === '=') {
      col.rgb = { r: 0.42, g: 0.15, b: 0.30 }; // ca Jz=30 Cz=16, hz in tok[2]
      zmod_assignop (col, toks[0], toks[1], parseFloat (toks[2]), toks[2].indexOf ('%') >= 0);
      if (!col.zcam)
	col.zcam = gamut.zcam (col.rgb);
      col.zcam = gamut.find_cusp (col.zcam.hz);
    } else if (toks.length == 3 && z_assignops.includes (toks[1])) {
      col.rgb = { r: 0, g: 0, b: 0 };
      zmod_assignop (col, toks[0], toks[1], parseFloat (toks[2]), toks[2].indexOf ('%') >= 0);
    }
  }
  // support hex colors and names
  if (col.rgb === undefined) {
    const rgba = color2rgba (colorlike);
    col.rgb = { r: rgba[0], g: rgba[1], b: rgba[2] };
    col.alpha = rgba[3];
  }
  // apply zmods
  for (const mod of mods) {
    const toks = mod.trim().split (split_ops);
    if (toks.length == 3 && z_assignops.includes (toks[1]))
      zmod_assignop (col, toks[0], toks[1], parseFloat (toks[2]), toks[2].indexOf ('%') >= 0);
    else
      console.warn ("CSS: invalid zmod: " + mod);
  }
  // RGBA output, clamp Cz to gamut
  if (col.zcam)
    return hex_from_zcam (col.zcam, col.alpha);
  let rgba = [ col.rgb.r, col.rgb.g, col.rgb.b, col.alpha ];
  rgba = rgba.map (v => clamp (v, 0, 1));
  if (rgba[3] === 1)
    rgba.splice (3);
  return true ? Z.srgb_hex (rgba) : css_rgba (rgba);
}

/// Find zmod() for a color.
export function zmod4 (colorlike)
{
  const rgba = color2rgba (colorlike);
  const gamut = default_gamut;
  const zcam = gamut.zcam ({ r: rgba[0], g: rgba[1], b: rgba[2] });
  const fix = n => n.toFixed (3);
  return `zmod (Jz=${fix (zcam.Jz)},Cz=${fix (zcam.Cz)},hz=${fix (zcam.hz)})`;
}

/// Find zcam() for a color.
export function zcam4 (colorlike)
{
  const rgba = color2rgba (colorlike);
  const gamut = default_gamut;
  return gamut.zcam ({ r: rgba[0], g: rgba[1], b: rgba[2] });
}

/// Interpolate between 2 colors.
export function zlerp (c1 = '#000', c2 = '#fff', t = 0.5)
{
  const perc = 'string' === typeof t && t.indexOf ('%') >= 0;
  t = parseFloat (t);
  t = Math.min (1, Math.max (0, perc ? t * 0.01 : t));
  const k = 1 - t;
  const rgba1 = color2rgba (c1);
  const rgba2 = color2rgba (c2);
  const gamut = default_gamut;
  const zc1 = gamut.zcam ({ r: rgba1[0], g: rgba1[1], b: rgba1[2] });
  const zc2 = gamut.zcam ({ r: rgba2[0], g: rgba2[1], b: rgba2[2] });
  const izazbz1 = Z.Izazbz_from_zcam (zc1);
  const izazbz2 = Z.Izazbz_from_zcam (zc2);
  const izazbz = { Iz: izazbz1.Iz * k + izazbz2.Iz * t,
		   az: izazbz1.az * k + izazbz2.az * t,
		   bz: izazbz1.bz * k + izazbz2.bz * t };
  const zcam = Z.zcam_from_Izazbz (izazbz);
  return hex_from_zcam (zcam, rgba1[3] * k + rgba2[3] * t);
}

/// Yield a grey tone with CIELAB lightness.
export function lgrey (lightness) {
  const gamut = default_gamut;
  const perc = lightness.indexOf ('%') >= 0;
  const Jz = parseFloat (lightness) * (perc ? 1 : 100);
  return gamut.zcam_hex ({ Cz: 0, hz: 0, Jz: clamp (Jz, 0, 100) });
}

// RGBA output, clamp Cz to gamut
export function hex_from_zcam (zcam, alpha = 1) {
  const gamut = default_gamut;
  let rgb = gamut.contains (zcam);
  if (!rgb.inside)
    rgb = gamut.contains (gamut.clamp_chroma (zcam));
  let rgba = [ rgb.r, rgb.g, rgb.b, alpha ];
  rgba = rgba.map (v => clamp (v, 0, 1));
  if (rgba[3] === 1)
    rgba.splice (3);
  return Z.srgb_hex (rgba);
}
