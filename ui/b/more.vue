<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-MORE
  Indicator for adding or dropping new UI elements.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-more {
    display: flex;
    height: 90%;
    align-items: center;
    justify-content: space-evenly;
    padding: 0 0.2em;
    margin: 0 0.2em;
    border-radius: 3px;
    font-weight: bold;
    font-size: 120%;
    cursor: default; user-select: none;
    border: 1px solid #bbb1;
    color: #bbb;
    &:hover { color: #fff; }
  }
</style>

<template>
  <v-flex class="b-more" >
    +
  </v-flex>
</template>

<script>
export default {
  sfc_template,
  props: {
  },
  methods: {
  },
};
</script>
