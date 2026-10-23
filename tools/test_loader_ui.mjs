// SPDX-License-Identifier: GPL-3.0-or-later
// Dependency-free tests of the actual embedded UI script and native status contract.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const root = new URL('../opg3d/src/main/cpp/loader/', import.meta.url);
const html = readFileSync(new URL('ui.html', root), 'utf8');
const source = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const native = readFileSync(new URL('strings.h', root), 'utf8');
const keys = [...native.matchAll(/= "(status_\w+)"/g)].map(x => x[1]);
assert.equal(keys.length, 16);
assert.match(readFileSync(new URL('loader.cpp', root), 'utf8'), /\\"messageKey\\":/);
class Element {
  textContent = ''; children = []; dataset = {}; style = {}; value = ''; disabled = false;
  append(child) { this.children.push(child); }
  replaceChildren() { this.children = []; this.textContent = ''; }
  setAttribute(name, value) { this[name] = value; }
}
async function boot(language) {
  const elements = new Map();
  const get = id => { if (!elements.has(id)) elements.set(id, new Element()); return elements.get(id); };
  const labels = [...html.matchAll(/data-t="([^"]+)"/g)].map(x => Object.assign(new Element(), {dataset: {t: x[1]}}));
  const aria = [...html.matchAll(/data-t-aria="([^"]+)"/g)].map(x => Object.assign(new Element(), {dataset: {tAria: x[1]}}));
  const document = {documentElement: {}, body: new Element(), getElementById: get,
    createElement: () => new Element(), querySelectorAll: s => s === '[data-t]' ? labels : aria};
  let state = {messageKey: keys[0], busy: false, percent: 0, launched: false, servers: []};
  let fail = false;
  const requests = [];
  const context = vm.createContext({document, navigator: {language}, Intl, setTimeout: () => {},
    fetch: async (path, options) => { requests.push({path, options}); if (fail) throw Error('offline'); return {ok: true, json: async () => state}; }});
  vm.runInContext(source, context);
  await vm.runInContext('poll()', context);
  return {get, labels, aria, document, requests, context,
    set: value => { state = {...state, ...value}; }, fail: value => { fail = value; },
    run: code => vm.runInContext(code, context), poll: () => vm.runInContext('poll()', context)};
}
for (const language of ['en-US', 'en-GB', 'ru-RU', 'RU-ru', 'de-DE', undefined]) {
  const ui = await boot(language);
  const lang = language?.toLowerCase().startsWith('ru') ? 'ru' : 'en';
  assert.equal(ui.document.documentElement.lang, lang);
  const dictionary = JSON.parse(ui.run('JSON.stringify(T)'));
  assert.deepEqual(Object.keys(dictionary.ru).sort(), Object.keys(dictionary.en).sort());
  assert.deepEqual(Object.keys(dictionary.en).filter(k => k.startsWith('status_')).sort(), [...keys].sort());
  for (const value of Object.values(dictionary.en)) assert.doesNotMatch(value, /[\u0400-\u04ff]/);
  for (const key of keys) {
    assert.equal(typeof dictionary[lang][key], 'string');
    assert.ok(dictionary[lang][key].length > 0);
    ui.set({messageKey: key}); await ui.poll();
    assert.equal(ui.get('message').textContent, dictionary[lang][key]);
    if (lang === 'en') assert.doesNotMatch(ui.get('message').textContent, /[\u0400-\u04ff]/);
  }
  for (const label of ui.labels) assert.equal(label.textContent, dictionary[lang][label.dataset.t]);
  assert.equal(ui.aria[0]['aria-label'], dictionary[lang].port);
  assert.equal(ui.run("region('Local')"), dictionary[lang].local);
  assert.equal(ui.run("region('ZZ')"), dictionary[lang].unknown);
  assert.equal(ui.run("region('SE')"), lang === 'en' ? 'Sweden' : 'Швеция');
  for (const key of [undefined, '__proto__', 'missing']) {
    ui.set({messageKey: key}); await ui.poll();
    assert.equal(ui.get('message').textContent, dictionary[lang].status_unavailable);
  }
  ui.set({servers: [{endpoint:'192.168.1.10:8080',country:'Local',name:'LAN',online:true,compatible:true,version:'23.1.3'}], busy:true, percent:42});
  await ui.poll(); assert.equal(ui.get('progress').value, 42); assert.equal(ui.get('progress').hidden, false);
  assert.equal(ui.get('play').disabled, true); assert.equal(ui.get('scan').disabled, true);
  assert.equal(ui.get('servers').children[0].children[1].children[1].textContent, dictionary[lang].local);
  ui.set({busy:false}); await ui.poll(); assert.equal(ui.get('play').disabled, false);
  await ui.get('play').onclick(); assert.ok(ui.requests.some(x => x.path === 'play' && x.options.body === '192.168.1.10:8080'));
  ui.get('host').value=' example.org '; ui.get('port').value=' 8080 ';
  await ui.get('check').onclick(); assert.ok(ui.requests.some(x => x.path === 'probe' && x.options.body === 'example.org:8080'));
  ui.fail(true); await ui.poll(); assert.equal(ui.get('message').textContent, dictionary[lang].status_unavailable);
  await ui.get('scan').onclick(); assert.equal(ui.get('message').textContent, dictionary[lang].offline);
  ui.fail(false); ui.set({launched:true}); await ui.poll(); assert.equal(ui.document.body.style.opacity, '.3');
}
console.log('PASS launcher UI: 16 statuses, RU/EN parity, locale fallback, accessibility, regions, actions and failures');
