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
		    :value="prop.fetch_()" @update:value="prop.apply_ ($event)" :choices="prop.choices_"
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

async function editable_object () {
  // provide an editable and reactive clone of this.value
  let o = { __typedata__: undefined, __fieldhooks__: undefined, }, td;
  for (const p in this.value)
    if (p[0] != '_' && typeof (this.value[p]) != "function")
      o[p] = this.value[p];
  // determine __typedata__
  if (Array.isArray (this.value.__typedata__))
    td = Util.map_from_kvpairs (this.value.__typedata__);
  else if (typeof (this.value.__typedata__) == "function")
    td = Util.map_from_kvpairs (this.value.__typedata__());
  else if (typeof (this.value.__typedata__) == "object")
    {
      td = {};
      Util.assign_forin (td, this.value.__typedata__);
    }
  else
    {
      const fields = [];
      for (let p in o)
	fields.push (p);
      td = { fields: fields.join (';'), };
    }
  o.__typedata__ = td;
  // transport __fieldhooks__
  o.__fieldhooks__ = Object.freeze (Object.assign ({}, this.value.__fieldhooks__ || {}));
  return o;
}

async function list_fields (proplist) {
  const groups = {}, attrs = {
    min: 0, max: 0, step: 0,
    readonly: false,
    allowfloat: true,
  };
  const awaits = [];
  Object.freeze (attrs);
  for (const prop of proplist)
    {
      let xprop = { ident_: prop.identifier(),
		    numeric_: prop.is_numeric(),
		    label_: prop.label(),
		    nick_: prop.nick(),
		    unit_: prop.unit(),
		    hints_: prop.hints(),
		    group_: prop.group(),
		    blurb_: prop.blurb(),
		    description_: prop.description(),
		    min_: prop.get_min(),
		    max_: prop.get_max(),
		    step_: prop.get_step(),
		    ctype_: undefined,
		    attrs_: attrs,
		    apply_: (...a) => {
		      xprop.set_value (a[0]);
		      debug ('APPLY', a[0]);
		      xprop.update_(); // FIXME
		    },
		    value_: Vue.reactive ({ val: undefined, num: 0, text: '' }),
		    update_: async () => {
		      const v = xprop.get_value(), n = xprop.get_normalized(), t = xprop.get_text();
		      xprop.value_.val = await v;
		      xprop.value_.num = await n;
		      xprop.value_.text = await t;
		    },
		    fetch_: () => xprop.numeric_ ? xprop.value_.num : xprop.value_.text,
		    __proto__: prop,
      };
      awaits.push (xprop.update_());
      xprop = await Util.object_await_values (xprop);
      if (!groups[xprop.group_])
	groups[xprop.group_] = [];
      groups[xprop.group_].push (xprop);
      const discon_ = xprop.on ('change', _ => xprop.update_());
      this.dom_ondestroy (discon_);
      if (this.augment)
	awaits.push (this.augment (xprop));
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
	  else if (xprop.hints_.search (/:choice:/) >= 0)
	    {
	      ctype = 'b-choice';
	      xprop.choices_ = await xprop.choices();
	    }
	  else
	    ctype = 'b-fed-text';
	  xprop.ctype_ = ctype;
	  Object.freeze (xprop);
	}
      Object.freeze (groups[k]);
    }
  return Object.freeze (grouplist);
}

function component_data () {
  const data = {
    gprops: { default: [], getter: c => list_fields.call (this, this.value), },
  };
  return this.observable_from_getters (data, () => this.value);
}

export default {
  sfc_template,
  data() { return component_data.call (this); },
  props: {
    readonly:	{ default: false, },
    augment:    { type: Function, },
    debounce:	{ default: 0, },
    value:	{ required: true, },
    default:	{},
  },
  emits: { input: null, },
  methods: {
    list_fields() {
      return this.gprops;
      // FIXME
      if (!this.object)
	return [];
      const o = this.object, td = o.__typedata__, fieldhooks = o.__fieldhooks__ || {};
      const field_typedata = unfold_properties (td); // { foo: { label: 'Foo' }, bar: { step: '5' }, etc }
      const fields = td.fields ? td.fields.split (';') : [];
      const groupmap = {};
      const grouplist = [];
      for (let fieldname of fields)
	{
	  const field_data = field_typedata[fieldname];
	  const attrs = {};
	  for (let p of ['min', 'max', 'step'])
	    if (field_data[p] != undefined)
	      attrs[p] = field_data[p];
	  if (this.readonly || (':' + field_data.hints + ':').indexOf ('ro') >= 0)
	    attrs.readonly = true;
	  const handler = (v) => this.apply_field (fieldname, v);
	  let label = td[fieldname + '.label'] || fieldname;
	  let ct = '';			// component type
	  const ft = typeof (o[fieldname]); // FIXME: use td
	  if (ft == "number")
	    {
	      ct = 'b-fed-number';
	      if (o[fieldname] != 0 | o[fieldname]) // not int // FIXME: use td
		attrs.allowfloat = true;
	      // min max
	    }
	  else if (ft == "boolean")
	    ct = 'b-fed-switch';
	  else if (ft == "string")
	    {
	      const picklistitems = fieldhooks[fieldname + '.picklistitems'];
	      if (picklistitems)
		{
		  attrs.picklistitems = picklistitems;
		  attrs.title = label + " " + "Selection";
		  ct = 'b-fed-picklist';
		}
	      else
		ct = 'b-fed-text';
	    }
	  if (ct)
	    {
	      const group = field_data.group || "Settings";
	      let groupfields;
	      if (groupmap[group] == undefined)
		{
		  groupfields = [];
		  const newgroup = [ group, groupfields ];
		  groupmap[group] = newgroup;
		  grouplist.push (newgroup);
		}
	      else
		groupfields = groupmap[group][1];
	      const blurb = td[fieldname + '.blurb'] || undefined;
	      groupfields.push ({ ident: fieldname,
				  ctype: ct,
				  label: label,
				  attrs: attrs,
				  odata: o,
				  blurb: blurb,
				  apply: handler });
	    }
	}
      return grouplist; // [ [ 'Group', [ field1, field2 ], [ 'Other', [ field3, field4 ] ] ]
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

function unfold_properties (nestedpropobject) {
  // turn { a.a: 1, a.b.c: 2, d.e: 3 } into: { a: { a: 1, b.c: 2 }, d: { e: 3 } }
  let oo = {};
  window.nestedpropobject=nestedpropobject;
  for (let p in nestedpropobject) {
    const parts = p.split ('.');
    if (parts.length > 1) {
      const stem = parts[0];
      parts.shift(); // pop stem
      if (oo[stem] == undefined)
	oo[stem] = {};
      oo[stem][parts.join ('.')] = nestedpropobject[p];
    }
  }
  return oo;
}
</script>
