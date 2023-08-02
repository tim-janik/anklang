<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PRO-INPUT
  A property input element, usually for numbers, toggles or menus.
  ## Properties:
  *value*
  : Contains the input being edited.
  *readonly*
  : Make this component non editable for the user.
</docs>

<style lang="scss">
  .b-pro-input        {
    display: flex; justify-content: center;
    .b-pro-input-ldiv[class]::before { content: "\200b"; /* zero width character to force line height */ }
    .b-pro-input-toggle { height: 2em; }
    .b-pro-input-choice { height: 2em; width: 2.3em; }
    .b-pro-input-span {
      pointer-events: none; user-select: none;
      max-width: 100%;
      white-space: nowrap;
      text-align: center;
      margin-right: -999em; /* avoid widening parent */
      overflow: hidden;
      z-index: 9;
    }
    &.b-pro-input:hover .b-pro-input-span { overflow: visible; }
  }
</style>

<template>
  <v-flex class="b-pro-input tabular-nums" :data-bubble="bubble()" >
    <b-toggle class="b-pro-input-toggle" v-if="type() == 'toggle'" label=""
	      :value="get_num()" @valuechange="set_num ($event.target.value)" />
    <b-choiceinput class="b-pro-input-choice" v-if="type() == 'choice'" small="1" indexed="1"
		   :prop="prop" :choices="choices"
		   :value="get_ident()" @valuechange="set_index ($event.target.index())" />
    <span   class="b-pro-input-span" v-if="labeled && !!nick">{{ nick }}</span>
  </v-flex>
</template>

<script>
function pro_input_data () {
  const data = {
    // ident:   { default: '', getter: async c => await this.prop.identifier(), },
    // group:   { default: '', getter: async c => await this.prop.group(), },
    is_numeric: { default:  1, getter: async c => await this.prop.is_numeric(), },
    label:      { default: '', getter: async c => await this.prop.label(), },
    choices:    { default: [], getter: async c => Object.freeze (await this.prop.choices()), },
    nick:       { default: '', getter: async c => await this.prop.nick(), },
    unit:       { default: '', getter: async c => await this.prop.unit(), },
    hints:      { default: '', getter: async c => await this.prop.hints(), },
    blurb:	{ default: '', getter: async c => await this.prop.blurb(), },
    vmin:       { getter: async c => await this.prop.get_min(), },
    vmax:       { getter: async c => await this.prop.get_max(), },
    vstep:      { getter: async c => await this.prop.get_step(), },
    vnum:       { getter: async c => await this.prop.get_normalized(),
		  notify: n => { this.vnotify.n1 = n; return this.prop.on ("notify", this.vnotify); }, },
    vtext:      { getter: async c => await this.prop.get_text(),
		  notify: n => this.vnotify.n2 = n, },
  };
  this.vnotify = () => { // use *1* callback for vnum and vtext
    if (this.vnotify.n1)
      this.vnotify.n1();
    if (this.vnotify.n2)
      this.vnotify.n2();
  };
  return this.observable_from_getters (data, () => this.prop);
}

export default {
  sfc_template,
  props: {
    prop:       { default: null, },
    labeled:	{ type: Boolean, default: true, },
    readonly:	{ type: Boolean, default: false, },
  },
  data() { return pro_input_data.call (this); },
  created() {
    this.hints_ = undefined;
  },
  methods: {
    bubble() {
      let b = '';
      if (this.label && this.blurb)
	b += '###### '; // label to title
      if (this.label)
	b += this.label;
      if (this.unit)
	b += '  (**' + this.unit + '**)';
      if (this.label || this.unit)
	b += '\n';
      if (this.blurb)
	b += this.blurb;
      App.zmove(); // force changes to be picked up
      return b;
    },
    type () {
      if (this.is_numeric)
	{
	  const hints = ':' + this.hints + ':';
	  if (hints.search (/:toggle:/) >= 0)
	    return 'toggle';
	  if (hints.search (/:choice:/) >= 0)
	    return 'choice';
	}
      return '';
    },
    reset_num () {
      if (this.readonly)
	return;
      this.update_hints();
      this.prop.reset();
    },
    set_num (nv) {
      if (this.readonly)
	return;
      this.update_hints();
      this.prop.set_normalized (nv);
    },
    get_num() {
      if (this.vnum === undefined)
	return 0;
      this.update_hints();
      return this.vnum;
    },
    update_hints()
    {
      if (this.hints == this.hints_)
	return;
      this.hints_ = this.hints;
    },
    set_index (v) {
      if (this.choices === undefined || this.choices.length < 1)
	return;
      const max = this.choices.length - 1;
      this.prop.set_normalized (v / max);
    },
    get_index() {
      if (this.choices === undefined || this.choices.length < 1)
	return 0;
      const max = this.choices.length - 1;
      return this.vnum * max;
    },
    get_ident() {
      if (this.choices === undefined || this.choices.length < 1)
	return '';
      const max = this.choices.length - 1;
      const n = 0| this.vnum * max;
      return n < this.choices.length ? this.choices[n].ident : '';
    },
    is_bidir() {
      this.bidir_ = this.hints.search (/:bidir:/) >= 0;
      return this.bidir_;
    },
  },
};
</script>
