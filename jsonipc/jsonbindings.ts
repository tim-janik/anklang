// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import * as fs from 'fs';
import * as path from 'path';

// == Clang SymbolGraph Types ==
interface SymbolIdentifier {
  precise: string;
  interfaceLanguage: string;
}
interface SymbolKind {
  identifier: string;
  displayName: string;
}
interface SymbolName {
  title: string;
}
interface DeclarationFragment {
  kind: string;
  spelling: string;
  preciseIdentifier?: string;
}
interface FunctionSignature {
  returns: DeclarationFragment[];
  parameters?: {
    name: string;
    declarationFragments: DeclarationFragment[];
  }[];
}
interface Symbol {
  identifier: SymbolIdentifier;
  kind: SymbolKind;
  names: SymbolName;
  pathComponents: string[];
  accessLevel: 'public' | 'private' | 'protected';
  functionSignature?: FunctionSignature; // optional functionSignature
}
interface Relationship {
  kind: 'memberOf' | 'inheritsFrom' | string;
  source: string;
  target: string;
}
interface SymbolGraph {
  symbols: Symbol[];
  relationships: Relationship[];
}

// == Parser Types ==
interface HasPath {
  pathComponents: string[];
}
interface EnumDetails extends HasPath {
  cases: string[];
}
interface StructureDetails extends HasPath {
  preciseId: string;
  methods: Symbol[];
  fields: Symbol[];
}

// Cache relations
class SymbolGraphNavigator {
  public readonly symbols: Symbol[];
  public readonly symbolMap = new Map<string, Symbol>();
  public readonly memberOfMap = new Map<string, string[]>();
  public readonly inheritanceMap = new Map<string, string>();

  constructor (symbolGraph: SymbolGraph)
  {
    this.symbols = symbolGraph.symbols;
    for (const symbol of symbolGraph.symbols)
      this.symbolMap.set (symbol.identifier.precise, symbol);

    for (const rel of symbolGraph.relationships) {
      if (rel.kind === 'memberOf') {
        if (!this.memberOfMap.has (rel.target))
          this.memberOfMap.set (rel.target, []);
        this.memberOfMap.get (rel.target)!.push (rel.source);
      } else if (rel.kind === 'inheritsFrom') {
        if (!this.inheritanceMap.has (rel.source))
          this.inheritanceMap.set (rel.source, rel.target);
      }
    }
  }
}

// Parser for Enum Types
function parseEnums (navigator: SymbolGraphNavigator): { [enumName: string]: EnumDetails }
{
  const allEnums: { [enumName: string]: EnumDetails } = {};
  for (const symbol of navigator.symbols) {
    if (symbol.kind.identifier === 'c++.enum') {
      const enumName = symbol.names.title;
      const caseIds = navigator.memberOfMap.get (symbol.identifier.precise) || [];
      const cases = caseIds.map (id => navigator.symbolMap.get (id)?.names.title).filter ((n): n is string => !!n);
      allEnums[enumName] = { pathComponents: symbol.pathComponents, cases };
    }
  }
  return allEnums;
}

// Helper to identify inner "internal" classes
function findNestedClassIdentifiers (navigator: SymbolGraphNavigator): Set<string>
{
  const nestedClassIds = new Set<string>();
  for (const symbol of navigator.symbols) {
    if ((symbol.kind.identifier === 'c++.class' || symbol.kind.identifier === 'c++.struct') &&
        symbol.pathComponents.length > 2) {
      nestedClassIds.add (symbol.identifier.precise);
    }
  }
  return nestedClassIds;
}

// Parser for Classes and Records
function parseStructures (navigator: SymbolGraphNavigator, blacklistPreciseIds: Set<string>): { [name: string]: StructureDetails }
{
  const allStructures: { [name: string]: StructureDetails } = {};
  const methodKind = 'c++.method';
  const fieldKinds = new Set (['c++.property', 'c++.var']);

  for (const symbol of navigator.symbols) {
    if (symbol.kind.identifier === 'c++.class' || symbol.kind.identifier === 'c++.struct') {
      if (blacklistPreciseIds.has (symbol.identifier.precise))
        continue;

      const structureName = symbol.names.title;
      const memberIds = navigator.memberOfMap.get (symbol.identifier.precise) || [];
      const publicMethods: Symbol[] = [];
      const publicFields: Symbol[] = [];

      for (const memberId of memberIds) {
        const memberSymbol = navigator.symbolMap.get (memberId);
        if (memberSymbol?.accessLevel === 'public') {
          if (memberSymbol.kind.identifier === methodKind &&
              memberSymbol.names.title !== structureName &&
              memberSymbol.names.title.search (/^operator\b/) !== 0 &&
              !memberSymbol.names.title.startsWith ('~')) {
            let dependsOnBlacklistedClass = false;
            if (memberSymbol.functionSignature) {
              for (const frag of memberSymbol.functionSignature.returns) {
                if (frag.preciseIdentifier && blacklistPreciseIds.has (frag.preciseIdentifier)) {
                  dependsOnBlacklistedClass = true;
                  break;
                }
              }
              if (!dependsOnBlacklistedClass && memberSymbol.functionSignature.parameters) {
                for (const param of memberSymbol.functionSignature.parameters) {
                  for (const frag of param.declarationFragments) {
                    if (frag.preciseIdentifier && blacklistPreciseIds.has (frag.preciseIdentifier)) {
                      dependsOnBlacklistedClass = true;
                      break;
                    }
                  }
                  if (dependsOnBlacklistedClass) break;
                }
              }
            }
            if (!dependsOnBlacklistedClass) {
              publicMethods.push (memberSymbol);
            }
          } else if (fieldKinds.has (memberSymbol.kind.identifier)) {
            publicFields.push (memberSymbol);
          }
        }
      }

      if (publicMethods.length > 0 || publicFields.length > 0 || navigator.inheritanceMap.has (symbol.identifier.precise))
        allStructures[structureName] = {
          preciseId: symbol.identifier.precise,
          pathComponents: symbol.pathComponents,
          methods: publicMethods,
          fields: publicFields
        };
    }
  }
  return allStructures;
}

// Code Generator for C++ Enum Registration
function generateEnumRegistration (enums: { [name: string]: EnumDetails }): string
{
  let outputLines: string[] = [];
  for (const name in enums) {
    const details = enums[name];
    if (details.cases.length === 0) continue;

    const fullyQualifiedName = `::${details.pathComponents.join ('::')}`;
    const variableName = `enum__${details.pathComponents.join ('_')}`;
    const setLines = details.cases.map (caseName => `.set (${fullyQualifiedName}::${caseName}, "${caseName}")`);

    let block = `  ::Jsonipc::Enum< ${fullyQualifiedName} > ${variableName};\n`;
    block += `  ${variableName}\n    ` + setLines.join ('\n    ') + '\n    ;';
    outputLines.push (block);
  }
  return outputLines.join ('\n');
}

// Code Generator for C++ Record (Serializable) Registration
function generateRecordRegistration (records: { [name: string]: StructureDetails }): string
{
  let outputLines: string[] = [];
  for (const name in records) {
    const details = records[name];
    const fullyQualifiedName = `::${details.pathComponents.join ('::')}`;
    const variableName = `serializable__${details.pathComponents.join ('_')}`;
    const setLines = details.fields.map (field => `.set ("${field.names.title}", &${fullyQualifiedName}::${field.names.title})`);

    if (setLines.length === 0) continue;

    let block = `  ::Jsonipc::Serializable< ${fullyQualifiedName} > ${variableName};\n`;
    block += `  ${variableName}\n    ` + setLines.join ('\n    ') + '\n    ;';
    outputLines.push (block);
  }
  return outputLines.join ('\n');
}

// Code Generator for C++ Class Registration
function generateClassRegistration (classes: { [name: string]: StructureDetails }, navigator: SymbolGraphNavigator): string
{
  let outputLines: string[] = [];
  for (const name in classes) {
    const details = classes[name];
    const fullyQualifiedName = `::${details.pathComponents.join ('::')}`;
    const variableName = `class__${details.pathComponents.join ('_')}`;
    // New Class
    let block = `  ::Jsonipc::Class< ${fullyQualifiedName} > ${variableName};\n`;
    block += `  ${variableName}\n`;
    const registrationLines: string[] = [];
    // Inheritance
    const parentId = navigator.inheritanceMap.get (details.preciseId);
    if (parentId) {
      const parentSymbol = navigator.symbolMap.get (parentId);
      if (parentSymbol)
        registrationLines.push (`.inherit< ::${parentSymbol.pathComponents.join ('::')} >()`);
    }
    // Fields
    details.fields.forEach (field => {
      // registrationLines.push (`.set ("${field.names.title}", &${fullyQualifiedName}::${field.names.title})`);
    });
    // Methods
    const processedMethods = new Set<string>();
    // Pass 1: Detect get_*/set_* pairs
    const potentialGetters = new Map<string, Symbol>();
    const potentialSetters = new Map<string, Symbol>();
    for (const method of details.methods) {
      const signature = method.functionSignature;
      if (!signature) continue;
      const isGetter = !signature.parameters || signature.parameters.length === 0;
      const isSetter = signature.parameters && signature.parameters.length === 1;
      if (isGetter && method.names.title.startsWith ('get_') && method.names.title.length > 4)
        potentialGetters.set (method.names.title.substring (4), method);
      else if (isSetter && method.names.title.startsWith ('set_') && method.names.title.length > 4)
	potentialSetters.set (method.names.title.substring (4), method);
    }
    // Generate getter+setter registrations
    for (const [baseName, getterSymbol] of potentialGetters.entries()) {
      const setterSymbol = potentialSetters.get (baseName);
      if (!setterSymbol) continue; // TODO: future feature
      registrationLines.push (`.set ("${baseName}", &${fullyQualifiedName}::${getterSymbol.names.title}, &${fullyQualifiedName}::${setterSymbol.names.title})`);
      processedMethods.add (getterSymbol.names.title);
      processedMethods.add (setterSymbol.names.title);
    }
    // Pass 2: Handle overloaded and regular methods
    const methodsByName = new Map<string, Symbol[]>();
    for (const method of details.methods) {
      // TODO: skip? if (processedMethods.has (method.names.title)) continue;
      if (!methodsByName.has (method.names.title))
        methodsByName.set (method.names.title, []);
      methodsByName.get (method.names.title)!.push (method);
    }
    // Generate single_method or getter+setter registrations
    for (const [methodName, symbols] of methodsByName.entries()) {
      if (symbols.length === 1) // simple method
        registrationLines.push (`.set ("${methodName}", &${fullyQualifiedName}::${methodName})`);
      else if (symbols.length === 2) {
        // Overloaded method, check for getter/setter
        const getter = symbols.find (s => s.functionSignature && !s.functionSignature.parameters?.length);
        const setter = symbols.find (s => s.functionSignature && s.functionSignature.parameters?.length === 1);
        if (getter && setter)
          registrationLines.push (`.set ("${methodName}", &${fullyQualifiedName}::${methodName}, &${fullyQualifiedName}::${methodName})`);
      }
    }
    if (registrationLines.length > 0) {
      block += '    ' + registrationLines.join ('\n    ') + '\n    ;';
      outputLines.push (block);
    }
  }
  return outputLines.join ('\n\n');
}

function die (...args: any[])
{
  const script = path.basename (process.argv[1]);
  console.error (`${script}:`, ...args);
  process.exit (1);
}

// Main Script
function main()
{
  const args = process.argv.slice (2); // skip node-exe and script-file
  let filePath = '';
  let cxx_gen = false;
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '--cxx')
      cxx_gen = true;
    else if (!filePath)
      filePath = arg;
    else
      die (`Error: Unknown argument:`, arg);
  }
  if (!filePath)
    die ("Error: Missing JSON file");

  const fileContent = fs.readFileSync (filePath, 'utf-8');
  const symbolGraph: SymbolGraph = JSON.parse (fileContent);
  const navigator = new SymbolGraphNavigator (symbolGraph);

  // 1. PARSE
  const foundEnums = parseEnums (navigator);
  const blacklistedClassIds = findNestedClassIdentifiers (navigator);
  const allStructures = parseStructures (navigator, blacklistedClassIds);

  // 2. CATEGORIZE
  const classes: { [name: string]: StructureDetails } = {};
  const records: { [name: string]: StructureDetails } = {};
  for (const name in allStructures) {
    const structure = allStructures[name];
    // A 'Class' has methods. A 'Record' (Serializable) only has fields.
    if (structure.methods.length > 0) {
      classes[name] = structure;
    } else if (structure.fields.length > 0) {
      records[name] = structure;
    } else if (navigator.inheritanceMap.has (structure.preciseId)) {
      // It might be a base class with no direct members of its own.
      classes[name] = structure;
    }
  }

  // 3. GENERATE & PRINT CXX
  if (cxx_gen) {
    const input_file = path.basename (filePath);
    const functionName = `jsonipc_for_${input_file.replace (/[^a-zA-Z0-9]/g, '_')}`;
    const enumCode = generateEnumRegistration (foundEnums);
    const recordCode = generateRecordRegistration (records);
    const classCode = generateClassRegistration (classes, navigator);
    console.log ('');
    console.log (`static void\n${functionName}()\n{`);
    if (enumCode)
      console.log (enumCode);
    if (recordCode)
      console.log ('\n' + recordCode);
    if (classCode)
      console.log ('\n' + classCode);
    console.log ("}");
  }
}

main();
