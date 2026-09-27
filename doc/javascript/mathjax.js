// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// https://squidfunk.github.io/mkdocs-material/reference/math/#mathjax-docsjavascriptsmathjaxjs

window.MathJax = {
  tex: {
    inlineMath: [["\\(", "\\)"]],
    displayMath: [["\\[", "\\]"]],
    processEscapes: true,
    processEnvironments: true
  },
  options: {
    ignoreHtmlClass: ".*|",
    processHtmlClass: "arithmatex"
  }
};

document$.subscribe (() => {
  MathJax.startup.output.clearCache?.();
  MathJax.typesetClear()
  MathJax.texReset()
  MathJax.typesetPromise()
});
