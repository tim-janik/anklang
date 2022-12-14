<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-TREESELECTOR-ITEM
  An element representing an entry of a B-TREESELECTOR, which allows selections.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-treeselector-item {
    user-select: none;
    & > span div:focus { outline: $b-focus-outline; }
    b-contextmenu & > span div:focus {
      background-color: $b-menu-active-bg; color: $b-menu-active-fg; outline: none;
    }
    margin: $b-menu-vpad 0;
    &[disabled], &[disabled] * {
      color: $b-menu-disabled;
    }
  }
  .b-treeselector ul { // /* beware, adds styles via parent */
    list-style-type: none;
    margin: 0; padding: 0;
    margin-left: 1em;
    //* li { margin: 0; padding: 0; } */
    .b-treeselector-leaf {
      cursor: pointer; user-select: none;
      margin-left: 0;
      div { text-overflow: ellipsis; white-space: nowrap; overflow-x: hidden; }
    }
    .b-treeselector-caret {
      cursor: pointer; user-select: none;
      position: relative;
      &::before {
	content: "\25B8";
	position: absolute;
	left: -0.8em;
	color: white;
	display: inline;
	transition: all .1s ease;
      }
    }
    .b-treeselector-active > .b-treeselector-caret::before {
      content: "\25B9";
      transform: rotate(90deg);
    }
    .b-treeselector-nested { display: none; }
    .b-treeselector-active > .b-treeselector-nested { display: block; }
  }
</style>

<template>
  <component
    :is="li_or_div()"
    ref="el"
    :uri.prop="uri" :check_isactive.prop="u => true"
    :disabled="isdisabled() ? 1 : null"
    :class="{ 'b-treeselector-active': css_active() }"
    class="b-treeselector-item" >
    <span class="b-treeselector-leaf"
	  v-if="!(entries && entries.length)">
      <div tabindex="0" @click="leaf_click1" @keydown="leaf_keydown"
      >{{ label }}</div></span>
    <span class="b-treeselector-caret"
	  v-if="entries && entries.length" @click="caret_click" >
      <div tabindex="0" @click="caret_click" @keydown="caret_keydown"
      >{{ label }}</div></span>
    <ul class="b-treeselector-nested" ref="nested"
	v-if="entries && entries.length" >
      <b-treeselector-item
	  v-for="entry in entries"
          :entries="entry.entries"
          :label="entry.label"
          :uri="entry.uri"
          :disabled="entry.disabled ? 1 : null"
	  :key="entry.label + ';' + entry.uri"
      ></b-treeselector-item>
    </ul>
  </component>
</template>

<script>
import * as ContextMenu from './contextmenu.js';

export default {
  sfc_template,
  props: { label: 	{ default: '' },
	   uri:		{ default: undefined },
	   entries:	{ default: _ => [] },
  },
  emits: { click: uri => ContextMenu.valid_uri (uri), },
  data: function() { return {
    is_active: undefined,
    click_stamp: 0, };
  },
  inject: {
      treedata: { from: 'b-treeselector.treedata',
		  default: { defaultcollapse: true }, },
  },
  computed: {
    menudata() {
      return ContextMenu.provide_menudata (this.$refs.el);
    }
  },
  methods: {
    css_active() {
      if (this.entries && this.entries.length && // non-leaf
	  // this.entries.length < 2 &&
	  this.is_active === undefined)
	return true; // this.treedata.defaultcollapse;
      return !!this.is_active;
    },
    caret_keydown: function (event) {
      if (Util.match_key_event (event, 'ArrowRight'))
	{
	  if (!this.is_active)
	    this.is_active = true;
	  else if (this.$refs.nested)
	    {
	      const nodes = Util.list_focusables (this.$refs.nested); // selector for focussable elements
	      if (nodes && nodes.length)
		nodes[0].focus();
	    }
	  event.preventDefault();
	}
      if (Util.match_key_event (event, 'ArrowLeft'))
	{
	  if (this.is_active)
	    {
	      this.is_active = false;
	      event.preventDefault();
	    }
	  else
	    this.leaf_keydown (event);
	}
    },
    leaf_keydown (event) {
      if (Util.match_key_event (event, 'ArrowLeft'))
	{
	  if (this.$el.parentElement && this.$el.parentElement.tagName == 'UL' &&
	      this.$el.parentElement.parentElement && this.$el.parentElement.parentElement.tagName == 'LI' &&
	      this.$el.parentElement.parentElement.parentElement &&
	      this.$el.parentElement.parentElement.parentElement.tagName == 'UL')
	    {
	      const nodes = Util.list_focusables (this.$el.parentElement.parentElement); // parent LI
	      if (nodes && nodes.length)
		nodes[0].focus();
	    }
	  event.preventDefault();
	}
    },
    caret_click: function() {
      this.is_active = !this.css_active();
      // event.preventDefault(); event.stopPropagation();
    },
    leaf_click1 (event) {
      if (this.isdisabled())
	return;
      Util.prevent_event (event);
      // ignore duplicate click()-triggers, permit once per frame
      if (Util.frame_stamp() === this.click_stamp)
	return;
      this.click_stamp = Util.frame_stamp();
      // trigger via focus/keyboard activation
      this.$refs.el.click();
    },
    isdisabled ()	{ return false; },
    li_or_div: function() { return 'li'; },
  },
};
</script>
