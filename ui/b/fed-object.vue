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
      -font-size: 1.1em; -font-weight: bolder;
      color: #888; background: none; padding: 0 0 0 0.5em; margin: 0;
      outline: none; border: 1px solid rgba(0,0,0,0); border-radius: $b-button-radius;
      &:hover			{ color: #eb4; }
      &:active			{ color: #3bf; }

    }
    > * { //* avoid visible overflow for worst-case resizing */
      min-width: 0;
      overflow-wrap: break-word;
    }
    .-group {
      align-items: center;
      margin-top: 0.5em;
      &:first-child { margin-top: 0; }
      white-space: nowrap;
      .-label { text-overflow: ellipsis; overflow: hidden; }
    }
    .-flabel {
      padding: 0 0.1em 0 0;
      min-width: 3em;
      white-space: nowrap;
      text-overflow: ellipsis;
      overflow: hidden;
    }
    .-field {
      justify-self: flex-end;
      justify-content: space-between;
      white-space: nowrap;
      max-width: 32em;
      width: 50vw;
    }
    .-value {
      width: 100%;
    }
  }
</style>

<!-- field = [ ident, iscomponent, label, attrs, o, handler ] -->
<template>
  <c-grid class="b-fed-object" style="grid-gap: 0.6em 0.5em;">
    <template v-for="group in gprops" :key="'group-' + group.name" >

      <h-flex class="-group" style="grid-column: 1 / span 3" >
	<span class="-label" style="flex-grow: 0; font-weight: bold" v-if="group.name" >{{ group.name }}</span>
	<hr style="flex-grow: 1; margin-left: 0.5em; min-width: 5em" v-if="group.name" />
      </h-flex>

      <template v-for="prop in group.props" :key="'fed-' + prop.ident_" >
	<span class="-flabel" style="grid-column: 1" :data-bubble="prop.blurb_"
	>{{ prop.label_ }}</span>
	<h-flex class="-field" style="grid-column: 2 / span 2" >
	  <span class="-value" style="text-align: right" :data-bubble="prop.blurb_" >
	    <component :is="prop.ctype_" v-bind="prop.attrs_" :class="'b-fed-object--' + prop.ident_"
					 :value="prop.fetch_()" @input="prop.apply_" v-if="prop.ctype_.startsWith ('b-fed-')" ></component>
	    <b-textinput v-bind="prop.attrs_" :class="'b-fed-object--' + prop.ident_"
			 :value="prop.fetch_()" @input="prop.apply_ ($event.target.value)"
			 v-if="prop.ctype_ === 'b-textinput'" ></b-textinput>
	    <b-choice v-bind="prop.attrs_" :class="'b-fed-object--' + prop.ident_"
		      :value="prop.fetch_()" @update:value="prop.apply_ ($event)" :choices="prop.value_.choices"
		      v-if="prop.ctype_ === 'b-choice'" ></b-choice>
	  </span>
	  <span>
	    <span class="b-fed-object-clear" @click="prop.reset()" > ⊗  </span></span>
	</h-flex>
      </template>

    </template>
  </c-grid>
</template>

<script>
import * as Util from '../util.js';

async function list_fields (proplist) {
  const disconnectors = [];
  const groups = {}, attrs = {
    min: 0, max: 0, step: 0,
    readonly: false,
    allowfloat: true,
  };
  const pending_xprops = [];
  for (const prop of proplist)
    {
      const augment = async xprop => {
	if (!xprop.attrs_)
	  xprop.attrs_ = attrs;
	if (this.augment)
	  await this.augment (xprop);
      };
      pending_xprops.push (Util.extend_property (prop, cb => disconnectors.push (cb), augment));
    }
  for (const pending_prop of pending_xprops)
    {
      const xprop = await pending_prop;
      if (!groups[xprop.group_])
	groups[xprop.group_] = [];
      groups[xprop.group_].push (xprop);
    }
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
	    ctype = 'b-textinput';
	  xprop.ctype_ = ctype;
	  Object.freeze (xprop);
	}
      Object.freeze (groups[k]);
    }
  this.replace_disconnectors (disconnectors);
  this.fetchingprops = false;
  return Object.freeze (grouplist);
}

function data () {
  const data = {
    gprops: { default: [], getter: c => this.list_fields (this.value), },
    fetchingprops: true,
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
  inject: { b_dialog_resizers: { default: [] }, },
  emits: { input: null, },
  mounted () {
    this.resizing = () => this.fetchingprops;
    this.b_dialog_resizers.push (this.resizing);
  },
  unmounted() {
    this.replace_disconnectors();
    Util.array_remove (this.b_dialog_resizers, this.resizing);
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
