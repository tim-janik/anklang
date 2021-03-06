<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-FED-OBJECT
  A field-editor for object input.
  A copy of the input value is edited, update notifications are provided via
  an `input` event.
  ## Properties:
  *value*
  : Object with properties to be edited.
  *readonly*
  : Make this component non editable for the user.
  *debounce*
  : Delay in milliseconds for `input` event notifications.
  ## Events:
  *input*
  : This event is emitted whenever the value changes through user input or needs to be constrained.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-fed-object		{
    .b-fed-object-clear	{
      font-size: 1.1em; font-weight: bolder;
      color: #888; background: none; padding: 0 0.1em 0;
      outline: none; border: 1px solid rgba(0,0,0,0); border-radius: $b-button-radius;
      &:hover			{ color: #eb4; }
      &:active			{ color: #3bf; }

    }
    > * { //* avoid visible overflow for worst-case resizing */
      min-width: 0;
      overflow-wrap: break-word;
    }
    .-field {
      align-items: center;
      margin-top: 0.5em;
      &:first-child { margin-top: 0; }
    }
  }
</style>

<!-- field = [ ident, iscomponent, label, attrs, o, handler ] -->
<template>
  <c-grid class="b-fed-object" style="grid-gap: 0.6em 0.5em;">
    <template v-for="group in gprops" :key="'group-' + group.name" >

      <h-flex class="-field" style="grid-column: 1 / span 3" >
	<span style="flex-grow: 0; font-weight: bold" v-if="group.name" >{{ group.name }}</span>
	<hr style="flex-grow: 1; margin-left: 0.5em; min-width: 5em" v-if="group.name" />
      </h-flex>

      <template v-for="prop in group.props" :key="'fed-' + prop.ident_" >
	<span style="grid-column: 1; padding: 0 1em 0; text-align: left" :data-bubble="prop.blurb_"
	>{{ prop.label_ }}</span>
	<span style="text-align: right" :data-bubble="prop.blurb_" >
	  <component :is="prop.ctype_" v-bind="prop.attrs_" :class="'b-fed-object--' + prop.ident_"
				       :value="prop.fetch_()" @input="prop.apply_" v-if="prop.ctype_ != 'b-choice'" ></component>
	  <b-choice v-bind="prop.attrs_" :class="'b-fed-object--' + prop.ident_"
		    :value="prop.fetch_()" @update:value="prop.apply_ ($event)" :choices="prop.value_.choices"
		    v-if="prop.ctype_ == 'b-choice'" ></b-choice>
	</span>
	<span>
	  <button class="b-fed-object-clear" tabindex="-1" @click="prop.reset()" > ⊗  </button></span>
      </template>

    </template>
  </c-grid>
</template>

<script>
import * as Util from '../util.js';
const empty_list = Object.freeze ([]);

async function list_fields (proplist) {
  const disconnectors = [];
  const groups = {}, attrs = {
    min: 0, max: 0, step: 0,
    readonly: false,
    allowfloat: true,
  };
  const awaits = [];
  for (const prop of proplist)
    {
      let xprop = { ident_: prop.identifier(),
		    hints_: prop.hints(),
		    is_numeric_: prop.is_numeric(),
		    label_: prop.label(),
		    nick_: prop.nick(),
		    unit_: prop.unit(),
		    group_: prop.group(),
		    blurb_: prop.blurb(),
		    description_: prop.description(),
		    min_: prop.get_min(),
		    max_: prop.get_max(),
		    step_: prop.get_step(),
		    has_choices_: false,
		    ctype_: undefined,
		    attrs_: attrs,
		    apply_: (...a) => {
		      xprop.set_value (a[0]);
		    },
		    value_: Vue.reactive ({ val: undefined, num: 0, text: '', choices: empty_list }),
		    update_: async () => {
		      const value_ = { val: xprop.get_value(),
				       num: xprop.get_normalized(),
				       text: xprop.get_text(),
				       choices: xprop.has_choices_ ? xprop.choices() : empty_list,
		      };
		      Object.assign (xprop.value_, await Util.object_await_values (value_));
		      if (this.augment)
			await this.augment (xprop);
		    },
		    fetch_: () => xprop.is_numeric_ ? xprop.value_.num : xprop.value_.text,
		    __proto__: prop,
      };
      const disconnect = xprop.on ('change', _ => xprop.update_());
      disconnectors.push (disconnect);
      xprop = await Util.object_await_values (xprop);
      xprop.has_choices_ = xprop.hints_.search (/:choice:/) >= 0;
      awaits.push (xprop.update_()); // relies on xprop.has_choices_
      if (!groups[xprop.group_])
	groups[xprop.group_] = [];
      groups[xprop.group_].push (xprop);
    }
  await Promise.all (awaits);
  const grouplist = []; // [ { name, props: [richprop, ...] }, ... ]
  for (const k of Object.keys (groups))
    {
      grouplist.push ({ name: k, props: groups[k] });
      for (const xprop of groups[k])
	{
	  let ctype = '';	// component type
	  if (xprop.hints_.search (/:range:/) >= 0)
	    {
	      ctype = 'b-fed-number';
	      if ((xprop.min_ | xprop.max_) != 0)
		xprop.attrs_ = Object.assign ({}, xprop.attrs_, { min: xprop.min_, max: xprop.max_ });
	    }
	  else if (xprop.hints_.search (/:bool:/) >= 0)
	    ctype = 'b-fed-switch';
	  else if (xprop.has_choices_)
	    ctype = 'b-choice';
	  else
	    ctype = 'b-fed-text';
	  xprop.ctype_ = ctype;
	  Object.freeze (xprop);
	}
      Object.freeze (groups[k]);
    }
  this.replace_disconnectors (disconnectors);
  return Object.freeze (grouplist);
}

function data () {
  const data = {
    gprops: { default: [], getter: c => this.list_fields (this.value), },
  };
  return this.observable_from_getters (data, () => this.value);
}

export default {
  sfc_template,
  data,
  props: {
    readonly:	{ default: false, },
    augment:    { type: Function, },
    debounce:	{ default: 0, },
    value:	{ required: true, },
    default:	{},
  },
  emits: { input: null, },
  unmounted() {
    this.replace_disconnectors();
  },
  methods: {
    list_fields,
    replace_disconnectors (new_disconnectors) {
      if (this.disconnectors)
	while (this.disconnectors.length)
	  this.disconnectors.pop().call();
      this.disconnectors = new_disconnectors;
    },
    apply_field (fieldname, value) {
      const o = this.object;
      o[fieldname] = value;
      if (this.emit_update_ == undefined)
	this.emit_update_ = Util.debounce (() => this.$emit ('input', this.object),
					   { wait: this.debounce,
					     restart: true,
					   });
      this.emit_update_();
    },
  },
};
</script>
