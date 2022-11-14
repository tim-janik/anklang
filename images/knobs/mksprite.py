#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import re, xml.etree.ElementTree as ET
import sys, math, argparse
import multiprocessing, subprocess

blurb = """
Script for generating a PNG sprite image from rotated SVG knob.
Elements with 'rcw_' or 'ccw_' ID prefixes are rotated.
"""

# Abort with helpful message
def die (*args):
  print (*args, file = sys.stderr)
  sys.exit (-1)

# Setup CLI parsr
def cli_parser():
  p = argparse.ArgumentParser (description = blurb)
  a = p.add_argument
  a ('-b', type = str,
     help = "Color to draw bidirectional Arc")
  a ('-u', type = str,
     help = "Color to draw unirectional Arc")
  a ('-n', type = int, default = 7 + 6 * 44,
     help = "Steps")
  a ('-w', type = int, default = 64,
     help = "Sprite width")
  a ('-s', type = int, default = 12,
     help = "Stroke width")
  a ('-g', type = int, default = 4,
     help = "Gap width")
  a ('-q', default = False, action = 'store_true',
     help = "Be mostly quiet")
  a ('--sprite', default = False, action = 'store_true',
     help = "Generate sprite by image concatenation")
  a ('--gif', default = False, action = 'store_true',
     help = "Generate animated GIF from images")
  a ('inputimage', type = str,
     help = "Image containing a rotatable SVG")
  a ('outputname', type = str,
     help = "Base name for output files")
  return p

# parse args
args = cli_parser().parse_args()
if not args.q:
  print ("ARGS:", args)

# Parse args
if len (sys.argv) < 2:
  die ("Usage: %s <SVGFILE>" % sys.argv[0])
svg_file = args.inputimage

# SVG namespace helper
def et_register_namespaces (filename): # bad hack, defeats per file namespaces...
  namespaces = dict ([node for _, node in ET.iterparse (filename, events = ['start-ns'])])
  for ns in namespaces:
    ET.register_namespace (ns, namespaces[ns])

# Load and parse SVG file
et_register_namespaces (svg_file)
tree = ET.parse (svg_file)
root = tree.getroot()
if not 'viewBox' in root.attrib:
  die (svg_file + ':', "SVG lacks viewBox definition")
dims = [float (n) for n in root.attrib['viewBox'].split (' ')]
svg_width, svg_height = dims[2], dims[3]
assert svg_width > 0
assert svg_height > 0
if not args.q:
  print ("%s: size: %gx%g" % (svg_file, svg_width, svg_height))

# Navigate SVG elements
def elements_by_id_prefix (prefix, parent = root):
  elements = []
  for node in parent:
    nid = node.attrib.get ("id")
    if nid != None and nid.startswith (prefix):
      elements.append (node)
    elements += elements_by_id_prefix (prefix, node)
  return elements

def debug_name (node):
  s = '<'
  s += re.sub ('.*}', '', node.tag)
  s += ' id="%s"' % node.attrib.get ('id')
  s += '/>'
  return s

def transform_type (nodetag):
  return 'gradientTransform' if nodetag.endswith ('Gradient') else 'transform'

def assign_transforms (angle = 67.5, x = 256, y = 256, first = False):
  # rotate clockwise via transform
  for node in elements_by_id_prefix ("rcw_"):
    transformtype = transform_type (node.tag)
    t = node.attrib.get (transformtype)
    if first and t:
      raise (AttributeError ("Element already has a", transformtype, debug_name (node)))
    node.attrib[transformtype] = "rotate(%f %f %f)" % (angle, x, y)
  # rotate counter clockwise via transform
  for node in elements_by_id_prefix ("ccw_"):
    transformtype = transform_type (node.tag)
    t = node.attrib.get (transformtype)
    if first and t:
      raise (AttributeError ("Element already has a", transformtype, debug_name (node)))
    node.attrib[transformtype] = "rotate(-%f %f %f)" % (angle, x, y)

def polar_to_cartesian (cx, cy, radius, angle_deg):
  angle_rad = angle_deg * math.pi / 180.0;
  return [ cx + radius * math.cos (angle_rad), cy + radius * math.sin (angle_rad) ]

# Create SVG path describing an ARC with `radius` around `cx,cy` from `d0` to `d1`
def arc_path (cx, cy, radius, d0, d1):
  p0 = polar_to_cartesian (cx, cy, radius, d0)
  p1 = polar_to_cartesian (cx, cy, radius, d1)
  xrotate = 0
  clockwise = 1
  large = 1 if abs (d1 - d0) > 180 else 0
  path = [
    "M", p0[0], p0[1],
    "A", radius, radius, xrotate, large, clockwise, p1[0], p1[1]
    ]
  return " ".join ([("0" if not isinstance (n, str) and abs (n) < 1e-15 else str (n)) for n in path])

# Create SVG Arc element
def svg_arc (cx, cy, radius, d0, d1, stroke, color):
  e = ET.Element ('svg:path')
  e.attrib['d'] = arc_path (cx, cy, radius, d0, d1)
  #e.attrib['style'] = 'fill:none;fill-rule:evenodd;stroke:#880000;stroke-miterlimit:4;stroke-dasharray:none;stroke-opacity:1'
  e.attrib['fill'] = 'none'
  e.attrib['stroke'] = color if d0 != d1 else 'none'
  e.attrib['stroke-width'] = str (stroke)
  return e

arc_color = args.u or args.b
stroke = args.s
gap = args.g # stroke/2
radius = 90 + 5/2 + gap + stroke/2
step = 270 / (args.n -1) # 270 / (185/2 * 2*3.14159265358979)
dstart = -225 if args.u else -90

if not args.q:
  print ('Transform SVG in steps:')

def frange (begin, end = None, step = None):
  if end == None: end = begin + 1.0
  if not step:    step = 1.0 if end > begin else -1.0
  i, f = 0, float (begin)
  while (step < 0 and f > end) or (step > 0 and f < end):
    yield f
    i += 1
    f = float (begin + i * step)

fnames = []
for a in frange (0, 270 + 0.00005, step):
  assign_transforms (a, svg_width/2, svg_height/2)
  if arc_color:
    dpos = -225 + a
    arc = svg_arc (svg_width/2, svg_height/2, radius, min (dpos, dstart), max (dpos, dstart), stroke, arc_color)
    root.append (arc) # PAINT surrounding arc
  fnames += [ args.outputname + ".tmp-%04.0f" % (10 * a) ]
  fname = fnames[-1] + ".svg"
  if not args.q:
    print (fname + ' ', end = '', flush = True)
  tree.write (fname)
  if arc_color:
    root.remove (arc)
if not args.q:
  print()

if not args.q:
  print ('Render pixel images:')
maxprocs = multiprocessing.cpu_count()
procs = []
for fname in fnames:
  cmd = [ 'rsvg-convert', '--width=' + str (args.w), '--keep-aspect-ratio', fname + '.svg', '--output=' + fname + '.png' ] # '-b', '#000'
  procs.append (subprocess.Popen (cmd))
  if len (procs) >= maxprocs:
    procs.pop (0).wait()
  if not args.q:
    print (fname + '.png ', end = '', flush = True)
while len (procs):
  procs.pop (0).wait()
if not args.q:
  print()

if args.sprite:
  sprite_name = args.outputname + '.png' # svg_file.rstrip ('.svg') + '.png'
  if not args.q:
    print ("Generate sprite:", sprite_name)
  cmd = [ 'convert' ]
  cmd += [f + '.png' for f in fnames]
  cmd += [ '-append', sprite_name ]
  subprocess.Popen (cmd).wait()

if args.gif:
  gif_name = args.outputname + '.gif' # svg_file.rstrip ('.svg') + '.gif'
  if not args.q:
    print ("Make GIF:", gif_name)
  cmd = [ 'convert', '-delay', '20', '-loop', '0' ]
  cmd += [f + '.png' for f in fnames]
  cmd += [ gif_name ]
  subprocess.Popen (cmd).wait()
if not args.q:
  print ('OK.')
