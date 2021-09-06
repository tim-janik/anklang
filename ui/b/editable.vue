<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  ## b-editable - Display an editable span.
  ### Methods:
  - **activate()** - Show input field and start editing.
  ### Props:
  - **selectall** - Set to `true` to automatically select all editable text.
  - **clicks** - Set to 1 or 2 to activate for single or double click.
  ### Events:
  - **change** - Emitted when an edit ends successfully.
</docs>

<style lang="scss">
@import 'mixins.scss';

.b-editable {
  position: relative;

  .b-editable-overlay {
    position: absolute;
    display: none;
    display: inline-block;
    top: 0px;
    left: 0;
    right: 1.5px;
    bottom: 0;
    width: 100%;
    height: 100%;
    border: 0;
    padding: 0;
    margin: 0;
    text-align: inherit;
    background: #000;
    color: inherit;
    font: inherit;
    z-index: 2;
    outline: 0;
    box-sizing: content-box;
    vertical-align: middle;
    line-height: 1;
    white-space: nowrap;
    letter-spacing: inherit;
  }
}
</style>

<template>

  <span class="b-editable"
  ><input
      class="b-editable-overlay" v-if="!!editable"
      ref="input"
      @keydown.stop="input_keydown"
      @change.stop="input_change"
      @blur.stop="input_blur"
      @contextmenu.stop="0"
    /><slot /></span>
</template>

<script>
function data () {
  const data = {
    editable: false,
  };
  return this.observable_from_getters (data, () => this);
}

export default {
  sfc_template,
  props: {
    clicks:    { default: 1, },
    selectall: { default: false, type: Boolean, },
  },
  data,
  methods:  {
    dom_create() {
      const activate = ev => { this.activate(); ev.stopPropagation(); };
      const clicks = this.clicks |0;
      if (clicks == 1)
	this.$el.addEventListener ("click", activate);
      else if (clicks == 2)
	this.$el.addEventListener ("dblclick", activate);
    },
    activate() {
      this.editable = true;
    },
    dom_update() {
      const input = this.$refs.input;
      if (input && input.valid_change === undefined)
	{
	  input.value = this.$el.innerText;
	  if (this.selectall)
	    input.select();
	  input.focus();
	  if (input == document.activeElement)
	    {
	      // setup input
	      input.valid_change = 1;
	    }
	}
    },
    input_keydown (ev) {
      const input = this.$refs.input;
      const esc = Util.match_key_event (ev, 'Escape');
      if (esc)
	input.valid_change = 0;
      if (esc || Util.match_key_event (event, 'Enter'))
	input.blur();
    },
    input_change (ev) {
      const input = this.$refs.input;
      if (input.valid_change)
	input.valid_change = 2;
    },
    input_blur (ev) {
      const input = this.$refs.input;
      if (input.valid_change == 2)
	this.$emit ('change', input.value);
      input.valid_change = 0;
      // give async 'change' handlers some time for updates
      input.disabled = true;
      setTimeout (() => this.editable = false, 17 * 3);
    },
  },
};
</script>
