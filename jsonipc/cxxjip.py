# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

from lxml import etree as ET # python3-lxml
import subprocess, sys, os, re

counter = 1001
includes = []
indent = 0

def p (*args):
  if indent:
    print ('  ' * indent, end = '')
  print (*args)

# Class representing an entire CC AST
class CCTree:
  classes = ('Class', 'Struct')
  def __init__ (self, xmltext):
    self.xmltext = xmltext
    self.root = ET.fromstring (self.xmltext)
    self.idcache = None
  def by_id (self, nodeid):
    if not self.idcache:
      self.idcache = {}
      for child in self.root:   # <File id="f21" .../>
        cid = child.attrib.get ('id')
        if cid:
          self.idcache[cid] = child
    return self.idcache.get (nodeid, None)
  def from_file (self, node):
    fileid = node.attrib.get ('file', '')
    if fileid:                  # <File id="f21" name="/usr/include/stdint.h"/>
      cfile = self.by_id (fileid)
      if cfile != None and cfile.tag == 'File':
        name = cfile.attrib.get ('name', '')
        if name:
          return name
    return None
  def from_absfile (self, node):
    filename = self.from_file (node)
    return os.path.abspath (filename) if filename else None
  def fqns (self, node):
    context = self.by_id (node.attrib.get ('context', ''))
    if context != None:
      cname = context.attrib.get ('name')
      if cname == '::' and context.tag == 'Namespace':
        return ''
      return self.fqns (context) + '::' + context.attrib.get ('name')
    return ''
  def fqname (self, node):
    name = node.attrib.get ('name')
    fqns = self.fqns (node)
    return fqns + '::' + name
  def root_namespace (self):
    for child in self.root:     # Namespace id="_1" name="::"
      if child.tag == 'Namespace' and child.attrib.get ('name') == '::':
        return child
  def list_namespaces (self):
    rootns = self.root_namespace()
    rootid = rootns.attrib.get ('id')
    retlist = []
    for child in self.root:     # <Namespace id="_7" name="Outer" context="_1" members="_13 _14 _15"/>
      if child.tag == 'Namespace' and child.attrib.get ('context') == rootid:
        retlist.append (child)
    return retlist
  def list_enums (self, namespace):
    # note, this *should* only pick up scoped enums, but
    # castxml-0.2.0 lacks the Enumeration scoped="1" attribute
    nsid = namespace.attrib.get ('id')
    retlist = []
    for child in self.root:     # <Enumeration id="_13" name="EE" context="_7" location="f1:3" file="f1" line="3" size="32" align="32">
      if child.tag == 'Enumeration' and child.attrib.get ('context') == nsid:
        retlist.append (child)
    return retlist
  def list_enumvalues (self, enum):
    retlist = []
    for child in enum:  # <EnumValue name="A" init="0"/>
      if child.tag == 'EnumValue':
        retlist.append (child)
    return retlist
  def list_classes (self, namespace):
    nsid = namespace.attrib.get ('id')
    retlist = []
    for child in self.root:     # <Class id="_14" name="CC" context="_7" bases="_14 private:_15"  location="f1:9" file="f1" line="9" members="_22 _23 _24 _25 _26" size="8" align="8"/>
      if child.tag in self.classes and child.attrib.get ('context') == nsid:
        retlist.append (child)
    return retlist
  def find_class (self, cid):
    classnode = self.by_id (cid)
    if (classnode != None and   # <Class id="_14" name="CC" context="_7" bases="_14 private:_15"  location="f1:9" file="f1" line="9" members="_22 _23 _24 _25 _26" size="8" align="8"/>
        classnode.tag in self.classes):
      return classnode
  def find_field (self, fid):
    fieldnode = self.by_id (fid)
    if (fieldnode != None and  # <Field id="_6197" name="y" type="_1740" context="_2269" access="public" location="f102:12" file="f102" line="12" offset="32"/>
        fieldnode.tag == 'Field'):
      return fieldnode
  def find_method (self, mid):
    methodnode = self.by_id (mid)
    if (methodnode != None and  # <Method id="_23" name="foo" returns="_46" context="_14" access="private" location="f1:10" file="f1" line="10" mangled="_ZN5Outer2C13fooEv"/>
        methodnode.tag == 'Method'):
      return methodnode
  def access_public (self, node):
    access = node.attrib.get ('access', '')
    return access != 'protected' and access != 'private'
  def is_static (self, node):
    static = node.attrib.get ('static', '')
    return static == "1"
  def is_const (self, node):
    return node.attrib.get ('const', '') == '1'
  def argument_count (self, node):
    args = 0
    for child in node:
      if child.tag == 'Argument':
        args += 1
    return args
  def returns_void (self, node):
    rettype = self.by_id (node.attrib.get ('returns', ''))
    if (rettype != None and rettype.tag == 'FundamentalType' and
        rettype.attrib.get ('name', '') == "void"):
      return True
    return False
  def method_kind (self, node):
    if self.returns_void (node):
      if self.argument_count (node) == 1 and not self.is_const (node):
        return 's' # setter
      else:
        return 'f' # function
    else: # non void return
      if self.argument_count (node) == 0 and self.is_const (node):
        return 'g' # getter
      else:
        return 'f' # function

def parse_file (ifile):
  args = [ 'castxml', '-std=gnu++17', '-o', '/dev/stdout', '--castxml-output=1' ]
  for inc in includes:
    args += [ '-I', inc ]
  args.append (ifile)
  process = subprocess.Popen (args, stdout = subprocess.PIPE)
  out, err = process.communicate()
  if process.returncode:
    raise Exception ('%s: failed with status (%d), full command:\n  %s' % (args[0], process.returncode, ' '.join (args)))
  return out

def skip_identifier (ident):
  if ident.startswith ('_') or ident.endswith ('_'):
    return True
  return False

def generate_jsonipc (cctree, namespaces, absfile = None):
  global indent, counter
  # namespaces
  for ns in cctree.list_namespaces():
    nsname = ns.attrib.get ('name')
    if not (nsname in namespaces): continue
    p ('// namespace', nsname)
    # enums
    for en in cctree.list_enums (ns):
      ename = en.attrib.get ('name')
      fqename = cctree.fqname (en)
      ident = 'enum_%d' % counter ; counter += 1
      p ('::Jsonipc::Enum<', fqename, '> ' + ident + ';')
      p (ident);
      indent += 1
      for ev in cctree.list_enumvalues (en):
        evname = ev.attrib.get ('name')
        p ('.set', '(%s::%s, "%s")' % (fqename, evname, evname))
      p (';')
      indent -= 1
    # classes
    for cl in cctree.list_classes (ns):
      clname = cl.attrib.get ('name')
      if skip_identifier (clname):
        continue
      if absfile:
        if absfile != cctree.from_absfile (cl):
          # print ('SKIP:', clname, absfile, cctree.from_absfile (cl), file = sys.stderr)
          continue
      fqclname = cctree.fqname (cl)
      # classify methods and fields
      fields = {}
      methods = {}
      for mid in cl.attrib.get ('members', '').split (' '):
        ff = cctree.find_field (mid)    # Field
        if ff != None:
          fname = ff.attrib.get ('name')
          if not skip_identifier (fname) and cctree.access_public (ff) and not cctree.is_static (ff):
            fields[fname] = (ff, 'v') # variable
          continue
        mf = cctree.find_method (mid)   # Method
        if mf == None or not cctree.access_public (mf) or cctree.is_static (mf): continue
        mname = mf.attrib.get ('name')
        if skip_identifier (mname): continue
        kind = cctree.method_kind (mf)
        prev = methods.get (mname, '')
        if prev:
          if ((prev[1] == 'g' and kind == 's') or (prev[1] == 's' and kind == 'g')):
            methods[mname] = (mf, 'gs') # seen getter and setter
          else:
            methods[mname] = (mf, 'D') # DUPLICATE
        else:
          methods[mname] = (mf, kind)
      # Class or Serializable
      struct = 'Serializable' if fields and not methods else 'Class'
      ident = struct.lower() + '_%d' % counter ; counter += 1
      p ('::Jsonipc::%s<' % struct, fqclname, '> ' + ident + ';')
      bases = []
      for cid in cl.attrib.get ('bases', '').split (' '):
        bclass = cctree.find_class (cid)
        bname = bclass.attrib.get ('name', '') if bclass else ''
        if not bname or skip_identifier (bname):
          continue
        if absfile and absfile != cctree.from_absfile (bclass):
          continue
        bases += [ bclass ]
      details = fields or methods or bases
      if details:
        p (ident)
      indent += 1
      # assign bases
      if struct == 'Class':
        for bclass in bases:
          bclass = cctree.find_class (cid)
          if bclass != None:
            basefqname = cctree.fqname (bclass)
            base_nslist = basefqname.split ('::')
            if len (base_nslist) >= 2 and base_nslist[0] == '' and base_nslist[1] in namespaces:
              p ('.inherit<', basefqname, '>()')
      # assign fields
      for fname, (ff, kind) in fields.items():
        if kind == 'v':
          p ('.set', '("%s", &%s::%s)' % (fname, fqclname, fname))
      # assign methods
      for mname, (mf, kind) in methods.items():
        # print (mname, kind , file = sys.stderr)
        if kind == 'gs':
          p ('.set', '("%s", &%s::%s, &%s::%s)' % (mname, fqclname, mname, fqclname, mname))
        elif kind in ('f', 's', 'g'):
          p ('.set', '("%s", &%s::%s)' % (mname, fqclname, mname))
      if details:
        p (';')
      indent -= 1

def file_identifier (fname):
  fname = os.path.basename (fname)
  return re.sub ('[^a-z0-9]', '_', fname, flags = re.I)

files = []
namespaces = []
i = 1
while i < len (sys.argv):
  if sys.argv[i].startswith ('-I'):
    if len (sys.argv[i]) > 2:
      includes.append (sys.argv[i][2:])
    else:
      i += 1
      if i < len (sys.argv):
        includes.append (sys.argv[i])
  elif sys.argv[i].startswith ('-N'):
    if len (sys.argv[i]) > 2:
      namespaces.append (sys.argv[i][2:])
    else:
      i += 1
      if i < len (sys.argv):
        namespaces.append (sys.argv[i])
  else:
    files.append (sys.argv[i])
  i += 1

if len (files) == 0:
  print ("Usage: %s -Iincludes -Nnamespace <cxxfile>" % sys.argv[0], file = sys.stderr)
  sys.exit (7)

for f in files:
  absfile = os.path.abspath (f)
  xmlout = parse_file (f)
  xmlout = xmlout.decode ('utf8')
  #open ("xmlcxx", "w").write (xmlout)
  cctree = CCTree (xmlout)
  p ('static void')
  p ('jsonipc_4_%s()' % file_identifier (f))
  p ('{')
  indent += 1
  generate_jsonipc (cctree, namespaces, absfile)
  indent -= 1
  p ('}')
