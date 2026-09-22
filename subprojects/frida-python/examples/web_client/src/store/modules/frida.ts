import { Module } from 'vuex';

interface FridaState {
  processes: Process[],
}

type Process = [number, string];

const fridaModule: Module<FridaState, any> = {
  state: {
    processes: []
  },

  mutations: {
  }
};

export default fridaModule;
