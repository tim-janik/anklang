<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-DIALOG
  A dialog that captures focus and provides a modal shield that dims everything else.
  A *close* event is emitted on clicks outside of the dialog area,
  if *Escape* is pressed or the default *Close* button is actiavted.
  ## Props:
  *shown*
  : A boolean to control the visibility of the dialog, suitable for `v-model:shown` bindings.
  ## Events:
  *update:shown*
  : An *update:shown* event with value `false` is emitted when the "Close" button activated.
  ## Slots:
  *header*
  : The *header* slot can be used to hold the modal dialog title.
  *default*
  : The *default* slot holds the main content.
  *footer*
  : By default, the *footer* slot holds a *Close* button which emits the *close* event.
  ## CSS Classes:
  *b-dialog*
  : The *b-dialog* CSS class contains styling for the actual dialog contents.
  *b-dialog-modalshield*
  : This CSS class contains styling for masking content under the dialog.
</docs>

<style lang="scss">
@import 'mixins.scss';
.b-dialog-modalshield {
  position: fixed; top: 0; left: 0; bottom: 0; right: 0;
  width: 100%; height: 100%;
  display: flex;
  background: $b-style-modal-overlay;
  transition: opacity 0.3s ease;
  pointer-events: all;
}
.b-dialog {
  display: flex;
  margin: auto;
  max-height: 100%; max-width: 100%;

  min-width: 16em; padding: 0;
  border: 2px solid $b-modal-bordercol; border-radius: 5px;
  color: $b-modal-foreground; background: $b-modal-background;
  padding: 1em;
  @include b-popup-box-shadow;
  /* fix vscrolling: https://stackoverflow.com/a/33455342 */
  justify-content: space-between;
  overflow: auto;

  button, push-button {
    @include b-singlebutton;
    margin: 0 1em;
    border-radius: 3px;
  }
  .-footer {
    button, push-button {
      &:first-child { margin-left: 1px; }
      &:last-child  { margin-right: 1px; }
    }
  }
  .-header {
    font-size: 1.5em; font-weight: bold;
    justify-content: space-evenly;
    padding-bottom: 0.5em;
    border-bottom: 1px solid $b-modal-bordercol;
  }
  .-header, .-footer {
    align-self: stretch;
    align-items: center; text-align: center;
  }
  .-footer {
    justify-content: space-evenly;
    padding-top: 1em;
    border-top: 1px solid $b-modal-bordercol;
    &.-empty { display: none; }
  }
  .-body {
    padding: 1em 1em;
    align-self: stretch;
  }
}

/* fading frames: [0] "v-{enter|leave}-from v-{enter|leave}-active" [1] "v-{enter|leave}-to v-{enter|leave}-active" [N] ""
 * https://v3.vuejs.org/guide/transitions-enterleave.html#transition-classes
 */
.b-dialog-modalshield {
  &.-fade-enter-active, &.-fade-leave-active {
    &, .b-dialog {
      transition: all 0.3s ease;
    }
  }
  &.-fade-enter-from, &.-fade-leave-to {
    & { opacity: 0; }
    .b-dialog { transform: scale(0.1); }
  }
}

</style>

<template>
  <transition name="-fade">
    <div class="b-dialog-modalshield" v-if='shown' >
      <v-flex class="b-dialog" @click.stop ref='dialog' >

	<h-flex class="-header">
	  <slot name="header">
	    Modal Dialog
	  </slot>
	</h-flex>

	<v-flex class="-body">
	  <slot name="default"></slot>
	</v-flex>

	<h-flex class="-footer" :class="footerclass" ref="footer">
	  <slot name="footer"/>
	</h-flex>

      </v-flex>
    </div>
  </transition>
</template>

<script>
export default {
  sfc_template,
  props:     { shown: { type: Boolean },
	       exclusive: { type: Boolean },
	       bwidth: { default: '9em' }, },
  emits: { 'update:shown': null, 'close': null },
  data() { return {
    footerclass: '',
  }; },
  mounted () {
    this.$forceUpdate(); // force updated() after mounted()
  },
  updated() {
    if (this.$refs.dialog && !this.undo_shield)
      { // newly shown
	this.undo_shield = Util.setup_shield_element (this.$el, this.$refs.dialog, this.close.bind (this));
	// autofocus
	const autofocus_element = this.$refs.dialog.querySelector ('[autofocus]:not([disabled]):not([display="none"])');
	autofocus_element?.focus();
      }
    if (!this.$refs.dialog && this.undo_shield)
      { // newly hidden
	this.undo_shield();
	this.undo_shield = null;
      }
    if (this.$refs.dialog)
      {
	this.footerclass = this.$refs.footer && this.$refs.footer.innerHTML ? '' : '-empty';
	const sel = Util.vm_scope_selector (this);
	const css = [];
	if (this.bwidth) {
	  css.push (`${sel}.b-dialog .-footer button      { min-width: ${this.bwidth}; }`);
	  css.push (`${sel}.b-dialog .-footer push-button { min-width: ${this.bwidth}; }`);
	}
	Util.vm_scope_style (this, css.join ('\n'));
      }
  },
  unmount() {
    this.undo_shield?.();
  },
  methods: {
    close (event) {
      this.$emit ('update:shown', false); // shown = false
      if (this.shown)
	this.$emit ('close');
    },
  },
};
</script>
