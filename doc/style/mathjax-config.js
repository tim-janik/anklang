"use strict";

window.MathJax = {
  options: {
    enableMenu: true,		// allow menu to allow copy-and-paste
    menuOptions: {
      settings: {
	explorer: false,
	texHints: true,        // put TeX-related attributes on MathML
	semantics: false,      // put original format in <semantic> tag in MathML
	zoom: 'Click',         // or 'Click' or 'DoubleClick' as zoom trigger
	zscale: '250%',        // zoom scaling factor
	renderer: 'SVG',       // or 'SVG'
	alt: false,            // true if ALT required for zooming
	cmd: false,            // true if CMD required for zooming
	ctrl: false,           // true if CTRL required for zooming
	shift: false,          // true if SHIFT required for zooming
	scale: 1,              // scaling factor for all math
	collapsible: false,    // true if complex math should be collapsible
	inTabOrder: false,     // true if tabbing includes math
      },
    },
  },
  startup: {
    pageReady: () => {	// Called when MathJax and page are ready
      MathJax.startup.defaultPageReady ();
    },
    ready: () => {	// Called when components are loaded
      // MathJax is loaded, but not yet initialized
      MathJax.startup.defaultReady ();
      // MathJax is initialized, and the initial typeset is queued
      MathJax.startup.promise.then (() => { /* MathJax initial typesetting complete */ });
      // Disable submenus with missing components
      const m = MathJax.startup.document.menu.menu;
      for (const id of [ "Accessibility", "Settings", "Language" ])
	m.findID (id).disable();
    },
  },
};

// Stale MathJax settings can break rendering
localStorage.clear();
window.addEventListener ('beforeunload', _ => localStorage.clear());
