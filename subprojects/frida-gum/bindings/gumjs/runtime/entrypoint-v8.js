import './core.js';
import './error-handler-v8.js';

Script.load = async (name, source) => {
  Script._load(name, source);
  return await import(name);
};
