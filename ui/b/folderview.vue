<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-FOLDERVIEW
  A browser view for the selection of path components.
  ## Properties:
  *entries*
  : List of Ase.Entry objects.
  ## Emits
  *click (entry)*
  : This event is emitted whenever an entry was activated via keyboard, mouse or touch.
  *select (entry)*
  : This event is emitted whenever an entry gets focus.
</docs>

<style lang="scss">
  .b-folderview {
    @include scrollbar-hover-area;
    input {
      outline-width: 0; border: none; border-radius: $b-button-radius;
      text-align: left;
      padding-left: $b-button-radius; padding-right: $b-button-radius;
      @include b-style-inset;
      &:focus		{ box-shadow: $b-focus-box-shadow; }
    }
    $b-scrollarea-background: $b-base-background;
    $b-scrollarea-frame: #202020;
    $b-scrollarea-foreground: $b-base-foreground;
    .b-folderview-scrollarea { //* wrapped */
      display: grid;
      grid-gap: 2px;
      grid-template-rows: repeat(auto-fit, 1.5em);
      grid-auto-columns: max-content;
      grid-auto-flow: column;
      justify-items: flex-start; justify-content: flex-start;
      flex-grow: 1; overflow-y: hidden;
      margin: 0; padding: 0;
      overflow-x: scroll; overflow-y: hidden;
      background: $b-scrollarea-background; color: $b-scrollarea-foreground;
      border: 1px solid $b-scrollarea-frame;
    }
    .-entry {
      display: inline-flex;
      button { flex-grow: 1; }
      /* an unwrapped button inside `flex-flow: column wrap` tends to cut off the focus frame */
    }
    button.b-folderview-entry {
      .b-icon {
	height: 100%; align-items: center; justify-content: center; vertical-align: middle;
	min-width: 1.5em; margin: 0;
	@include b-font-weight-bold();
	.-icon-folder { color: #bba460; }
	padding: 0;
      }
      min-width: 10em;
      display: inline-block; white-space: pre-wrap;
      overflow-wrap: break-word;
      text-align: left;
      margin: 0; padding: 0.1em 0.2em 0.1em 0;
      border: none; border-radius: 0; text-decoration: none;
      background: unset; color: unset;
      font: unset;
      cursor: pointer;
      -webkit-appearance: none; -moz-appearance: none;
      &:active { border: none; }
      display: inline;
      @include b-focus-outline;
    }
    span.b-folderview-meta {
      text-align: right;
      padding: 0;
    }
    .-spin-wrapper {
      div {
	font-size: 150px;
	animation: kf-rotate360 2s linear infinite;
      }
    }
  }
@keyframes kf-rotate360 {
  from { transform: rotate(0deg); }
  to   { transform: rotate(360deg); } }
</style>

<template>
  <v-flex class="b-folderview">
    <c-grid class="b-folderview-scrollarea" data-subfocus="*" ref="entrycontainer" >

      <template v-for="e in filtered_entries()" :key="e.label" >
	<h-flex class="-entry">
	  <button class="b-folderview-entry" @keydown="Util.keydown_move_focus"
		  @focus="emit_select ($event, e)"
	          @click="emit_key_click ($event, e)" @dblclick="emit_click ($event, e)" >
	    <b-icon :ic="fmt_icon (e)" :iconclass="icon_cssclass (e)" ></b-icon>
	    {{ e.label }}
	    <!--span class="b-folderview-meta">{{ fmt_size (e) }}   {{ Util.fmt_date (e.mtime) }}</span-->
	  </button>
	</h-flex>
      </template>

      <h-flex class="-spin-wrapper"
	      v-if="0 == entries.length"
	      style="height: 100%; width: 100%; text-align: center;
		     align-items: center; justify-content: center">
	<div style="text-align: center" >
	  ⥁ </div> </h-flex>

    </c-grid>
  </v-flex>
</template>

<script>
import * as Util from "../util.js";
import { hex, basename, dirname, displayfs, displaybasename, displaydirname } from '../strings.js';

export default {
  sfc_template,
  props: { entries:     { default: [] }, },
  emits: { click: null, select: null },
  methods: {
    clear_entries() { // performance hack
      const children = this.$refs?.entrycontainer?.children;
      if (!children || children.length < 1)
	return;
      for (let i = children.length - 1; i >= 0; i--) {
	const child = children[i];
	if (child.classList.contains ('-entry'))
	  child.parentElement.removeChild (child);
	// this only works when changing entries=[], manually removing
	// HtmlElement children is faster than Vue3's diffing.
      }
    },
    dom_update() {
    },
    fmt_size (e) {
      return e.size < 0 ? '   ' : e.size;
    },
    fmt_icon (e) {
      if (e.type == Ase.ResourceType.FOLDER)
	return "mi-folder";
      else
	return "fa-file-o";
      // return "mi-description";
      // return "mi-text_snippet";
    },
    icon_cssclass (e) {
      if (e.type == Ase.ResourceType.FOLDER)
	return "-icon-folder";
      else
	return "-icon-other";
    },
    filtered_entries() {
      let e = this.entries;
      // e = e.slice (0, 500); // limit number of entries
      e = e.filter (a => a.label && (a.label == '..' || a.label[0] != '.'));
      e.sort (function (a, b) {
	if (a.type != b.type)
	  return a.type > b.type ? -1 : +1;
	if (a.mtime != b.mtime)
	  ; //  return a.mtime - b.mtime;
	const al = a.label.toLowerCase(), bl = b.label.toLowerCase();
	if (al != bl)
	  return al < bl ? -1 : +1;
	if (a.label != b.label)
	  return a.label < b.label ? -1 : +1;
	return 0;
      });
      return e;
    },
    emit_select (event, entry) {
      this.$emit ('select', entry);
    },
    emit_click (event, entry) {
      this.$emit ('click', entry);
    },
    emit_key_click (event, entry) {
      if (event.detail === 0) // focus + ENTER caused click
	this.$emit ('click', entry);
    },
  },
};
</script>
