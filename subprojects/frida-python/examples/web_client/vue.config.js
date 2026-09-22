const path = require('path');

module.exports = {
  configureWebpack: {
    resolve: {
      alias: {
        'abstract-socket': path.resolve(__dirname, 'src', 'shims', 'abstract-socket.js'),
        'x11': path.resolve(__dirname, 'src', 'shims', 'x11.js')
      }
    }
  }
};
