import Vue from 'vue';
import Vuex from 'vuex';

import frida from './modules/frida';
import fridaBus from './plugins/frida';

Vue.use(Vuex);

export default new Vuex.Store({
  modules: {
    frida
  },
  plugins: [
    fridaBus()
  ]
});
