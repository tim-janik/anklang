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
  : List of choices: `[ { icon, label, blurb }... ]`
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
  .b-choice-current {
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
  .b-choice-contextmenu {
    .b-choice-label { display: block; white-space: pre-line; }
    .b-choice-line1,
    .b-choice-line2 { display: block; white-space: pre-line; font-size: 90%; color: $b-style-fg-secondary; }
    .b-choice-line3 { display: block; white-space: pre-line; font-size: 90%; color: $b-style-fg-notice; }
    .b-choice-line4 { display: block; white-space: pre-line; font-size: 90%; color: $b-style-fg-warning; }
    .b-menuitem {
      &:focus, &.active, &:active {
	.b-choice-line1, .b-choice-line2, .b-choice-line3,
	.b-choice-line4 { filter: $b-style-fg-filter; } //* adjust to inverted menuitem */
      } }
    .b-menuitem {
      white-space: pre-line;
    }
  }
</style>

<template>
  <h-flex class="b-choice" ref="bchoice" :class="classlist" :data-tip="data_tip()"
	  @pointerdown="pointerdown" @keydown="keydown" >
    <h-flex class="b-choice-current" ref="pophere" tabindex="0" >
      <span class="b-choice-nick">{{ nick() }}</span>
      <span class="b-choice-arrow" > ⬍ <!-- ▼ ▽ ▾ ▿ ⇕ ⬍ ⇳ --> </span>
    </h-flex>
    <b-contextmenu class="b-choice-contextmenu" ref="cmenu" @click="menuactivation" @close="menugone" >
      <b-menutitle v-if="title" > {{ title }} </b-menutitle>

      <b-menuitem v-for="e in mchoices" :uri="e.uri" :key="e.uri" :ic="e.icon" >
	<span class="b-choice-label" :class='e.labelclass' v-if="e.label"   > {{ e.label }}
	  <!--span >{{ '[' + e.ident + ']' }}</span--> </span>
	<span class="b-choice-line1" :class='e.line1class' v-if="e.blurb"   > {{ e.blurb }} </span>
	<span class="b-choice-line2" :class='e.line2class' v-if="e.line2"   > {{ e.line2 }} </span>
	<span class="b-choice-line3" :class='e.line3class' v-if="e.notice"  > {{ e.notice }} </span>
	<span class="b-choice-line4" :class='e.line4class' v-if="e.warning" > {{ e.warning }} </span>
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
	   prop:    { default: null, },
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
    data_tip() {
      const n = this.ivalue;
      let tip = "**CLICK** Select Choice";
      if (!this.prop?.label_ || !this.mchoices || n >= this.mchoices.length)
	return tip;
      const c = this.mchoices[n];
      let val = "**" + this.prop.label_ + "** ";
      val += c.label;
      return val + " " + tip;
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
