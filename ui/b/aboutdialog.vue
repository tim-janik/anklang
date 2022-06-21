<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-ABOUTDIALOG
  A modal [b-dialog] that displays version information about Anklang.
  ## Events:
  *close*
  : A *close* event is emitted once the "Close" button activated.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-aboutdialog c-grid {
    max-width: 100%;
    & > * { //* avoid visible overflow for worst-case resizing */
      overflow-wrap: break-word;
      min-width: 0; }
  }
  .b-aboutdialog .b-aboutdialog-header {
    grid-column: 1;
    text-align: right; vertical-align: top; font-weight: bold;
    padding-right: .5em; min-width: 15em; }
  .b-aboutdialog .b-aboutdialog-field {
    grid-column: 2;
    overflow-wrap: break-word;
    display: inline-block; white-space: pre-wrap; }
</style>

<template>
  <b-dialog class="b-aboutdialog" :shown="shown" @update:shown="$emit ('update:shown', $event)" >
    <template v-slot:header>
      <div>About ANKLANG</div>
    </template>
    <slot></slot>
    <c-grid>
      <template v-for="p in info_pairs" :key="'hf' + p[0]">
	<span class="b-aboutdialog-header" >{{ p[0] }}</span>
	<span class="b-aboutdialog-field"  >{{ p[1] }}</span>
      </template>
    </c-grid>
    <template v-slot:footer>
      <div><button autofocus @click="$emit ('update:shown', false)" > Close </button></div>
    </template>
  </b-dialog>
</template>

<script>
async function about_pairs() {
  const user_agent = navigator.userAgent.replace (/([)0-9]) ([A-Z])/gi, '$1\n$2');
  let array = [
    [ 'Anklang:',		CONFIG.buildid + ' (' + CONFIG.revdate.split (' ')[0] + ')' ],
    [ 'SoundEngine:',		await Ase.server.get_version() ],
    [ 'CLAP:',			await Ase.server.get_clap_version() ],
    [ 'Vorbis:',		await Ase.server.get_vorbis_version() ],
    [ 'Vuejs:',			Vue.version ],
    [ 'User Agent:',		user_agent ],
  ];
  if (window.Electron)
    {
      const operating_system = window.Electron.platform + ' ' + window.Electron.arch + ' (' + window.Electron.os_release + ')';
      const parray = [
	[ 'Electron:',          window.Electron.versions.electron ],
	[ 'Chrome:',            window.Electron.versions.chrome ],
	[ 'Node.js:',           window.Electron.versions.node ],
	[ 'Libuv:',             window.Electron.versions.uv ],
	[ 'V8:',                window.Electron.versions.v8 ],
	[ 'OS:',		operating_system ],
      ];
      array = Array.prototype.concat (array, parray);
    }
  return array;
}

export default {
  sfc_template,
  props: {
    shown: { default: false, type: Boolean },
  },
  emits: { 'update:shown': null, },
  data() { return {
    info_pairs: [],
  }; },
  async created() {
    this.info_pairs = await about_pairs();
  },
};
</script>
