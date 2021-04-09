<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-CHOICE
  This element provides a choice popup to choose from a set of options.
  It supports the Vue
  [v-model](https://vuejs.org/v2/guide/components-custom-events.html#Customizing-Component-v-model)
  protocol by emitting an `input` event on value changes and accepting inputs via the `value` prop.
  ## Props:
  *value*
  : Integer, the index of the choice value to be displayed.
  *choices*
  : List of choices: `[ { icon, label, subject }... ]`
  ## Events:
  *update:value (value)*
  : Value change notification event, the first argument is the new value.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-choice {
    display: flex; position: relative;
    margin: 0;
    white-space: nowrap;
    user-select: none;
    &.b-choice-big {
      justify-content: left; text-align: left;
      padding: .1em 0;
      width: 16em;
    }
    &.b-choice-small {
      justify-content: center; text-align: center;
      padding: 0;
    }
  }
  .b-choice-label {
    align-self: center;
    min-width: 0; //* allow ellipsized text in flex child */
    margin: 0;
    white-space: nowrap; overflow: hidden;
    .b-choice-big & {
      flex-grow: 1;
      justify-content: space-between;
      padding: $b-button-radius 0 $b-button-radius .5em;
    }
    .b-choice-small & {
      width: 100%; height: 1.33em;
      justify-content: center;
      padding: 2px;
    }
    @include b-style-outset();
  }
  .b-choice-nick {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .b-choice-arrow {
    flex: 0 0 auto; width: 1em;
    margin: 0 0 0 .5em;
    .b-choice-small & {
      display: none;
    }
  }
</style>

<template>
  <h-flex class="b-choice" ref="bchoice" :class="classlist" :data-tip="bubbletip()"
	  @pointerdown="pointerdown" @keydown="keydown" >
    <h-flex class="b-choice-label" ref="pophere" tabindex="0" >
      <span class="b-choice-nick">{{ nick() }}</span>
      <span class="b-choice-arrow" > ⬍ <!-- ▼ ▽ ▾ ▿ ⇕ ⬍ ⇳ --> </span>
    </h-flex>
    <b-contextmenu ref="cmenu" @click="menuactivation" @close="menugone" >
      <b-menutitle v-if="title" > {{ title }} </b-menutitle>

      <b-menuitem v-for="e in mchoices" :uri="e.uri" :key="e.uri" >
	<b>{{ e.label }}</b>
	<span v-if="e.subject" >{{ e.subject }}</span>
      </b-menuitem>
    </b-contextmenu>
  </h-flex>
</template>

<script>
export default {
  sfc_template,
  props: { value:   { default: false },
	   title:   { type: String, },
	   small:   { default: false },
	   indexed: { default: false },
	   choices: { default: [] }, },
  emits: { 'update:value': null, },
  data: () => ({
    value_: 0,
    buttondown_: false,
  }),
  computed: {
    classlist() {
      return this.small ? 'b-choice-small' : 'b-choice-big';
    },
    ivalue() {
      if (this.indexed)
	return this.value >>> 0;
      for (let i = 0; i < this.mchoices.length; i++)
	if (this.mchoices[i].uri == this.value)
	  return i;
      return 999e99;
    },
    mchoices() {
      const mchoices = [];
      for (let i = 0; i < this.choices.length; i++) {
	const c = Object.assign ({}, this.choices[i]);
	c.uri = this.indexed ? i : c.uri || c.ident;
	mchoices.push (c);
      }
      return mchoices;
    }, },
  beforeUnmount () {
    window.removeEventListener ('pointerup', this.pointerup);
  },
  methods: {
    dom_update() {
      if (!this.$el) // we need a second Vue.render() call for DOM alterations
	return this.$forceUpdate();
      this.value_ = this.value;
    },
    bubbletip() {
      App.zmove(); // force changes to be picked up
      const n = this.ivalue;
      if (!this.mchoices || n >= this.mchoices.length)
	return "";
      const c = this.mchoices[n];
      let tip = "**" + c.label + "**";
      if (c.subject)
	tip += " " + c.subject;
      return tip;
    },
    nick() {
      const n = this.ivalue;
      if (!this.mchoices || n >= this.mchoices.length)
	return "";
      const c = this.mchoices[n];
      return c.label ? c.label : '';
    },
    menuactivation (uri) {
      // close popup to remove focus guards
      this.$refs.cmenu.close();
      this.$emit ('update:value', uri);
    },
    pointerdown (ev) {
      this.$refs.pophere.focus();
      // trigger only on primary button press
      if (!this.buttondown_ && ev.buttons == 1)
	{
	  this.buttondown_ = true;
	  ev.preventDefault();
	  ev.stopPropagation();
	  this.$refs.cmenu.popup (event, { origin: this.$refs.pophere });
	}
    },
    menugone () {
      this.buttondown_ = false;
    },
    keydown (ev) {
      // allow selection changes with UP/DOWN while menu is closed
      if (event.keyCode == Util.KeyCode.DOWN || event.keyCode == Util.KeyCode.UP)
	{
	  ev.preventDefault();
	  ev.stopPropagation();
	  let n = this.ivalue;
	  if (this.mchoices)
	    {
	      n += event.keyCode == Util.KeyCode.DOWN ? +1 : -1;
	      if (n >= 0 && n < this.mchoices.length)
		this.$emit ('update:value', this.mchoices[n].uri);
	    }
	}
      else if (event.keyCode == Util.KeyCode.ENTER)
	{
	  ev.preventDefault();
	  ev.stopPropagation();
	  this.$refs.cmenu.popup (event, { origin: this.$refs.pophere });
	}
    },
  },
};

</script>
