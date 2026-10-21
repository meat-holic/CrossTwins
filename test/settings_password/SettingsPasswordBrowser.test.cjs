const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const html = fs.readFileSync(path.join(__dirname, '../../src/network/html/SettingsPage.html'), 'utf8');
function productionFunction(name) {
  const start = html.search(new RegExp('  (?:async )?function ' + name + '\\('));
  assert.ok(start >= 0, name);
  const rest = html.slice(start);
  const next = rest.slice(1).search(/\n  (?:async )?function /);
  return next < 0 ? rest : rest.slice(0, next + 1);
}
function browser(configured = true) {
  const controls = {
    'setting-koPassword': { value: '', dataset: {} },
    'setting-unrelated': { checked: false, dataset: {} },
    saveBtn: { disabled: true, textContent: '' },
  };
  const state = {
    allSettings: [{ key: 'koPassword', name: '비밀번호', type: 'string', value: null, hasPassword: configured },
                  { key: 'unrelated', type: 'toggle', value: 0 }],
    originalValues: { koPassword: null, unrelated: 0 },
    document: { getElementById: id => controls[id] },
    handleSettingChanged() {}, showMessage() {}, showWebToast() {}, posted: null, reloads: 0,
  };
  state.fetch = async (_, request) => { state.posted = JSON.parse(request.body); return { ok: true }; };
  state.loadSettings = async () => { state.reloads++; };
  const context = vm.createContext(state);
  for (const name of ['escapeHtml', 'renderControl', 'getValue', 'markChanged', 'findSetting', 'getControl',
                      'clearPassword', 'getControlValue', 'saveSettings']) vm.runInContext(productionFunction(name), context);
  return { state, controls };
}

function serverBrowser(confirm) {
  const fixture=browser(); const {state,controls}=fixture;
  state.allSettings.push({key:'koServerUrl',name:'서버 URL',type:'string',value:'https://old.invalid'},
                         {key:'koServerType',name:'서버 종류',type:'enum',value:0,options:['CrossPoint','KOSync','Other']});
  Object.assign(state.originalValues,{koServerUrl:'https://old.invalid',koServerType:0});
  controls['setting-koServerUrl']={value:'https://new.invalid',dataset:{}};
  controls['setting-koServerType']={value:'0',dataset:{}};
  state.confirmations=[];
  state.window={confirm:message=>{state.confirmations.push(message);return confirm;}};
  return fixture;
}
test('URL change confirms and posts the selected existing profile without inference',async()=>{
  const {state}=serverBrowser(true);await state.saveSettings();
  assert.equal(state.posted.koServerUrl,'https://new.invalid');
  assert.equal(state.posted.koServerType,0);
  assert.equal(state.confirmations.length,1);
  assert.ok(state.confirmations[0].includes('CrossPoint'));
});
test('profile choice and URL are sent together; cancel posts neither',async()=>{
  const {state,controls}=serverBrowser(true);controls['setting-koServerType'].value='1';await state.saveSettings();
  assert.equal(state.posted.koServerType,1);assert.equal(state.posted.koServerUrl,'https://new.invalid');
  const cancelled=serverBrowser(false);await cancelled.state.saveSettings();
  assert.equal(cancelled.state.posted,null);assert.equal(cancelled.state.originalValues.koServerUrl,'https://old.invalid');
  assert.equal(cancelled.controls.saveBtn.disabled,false);
});
test('configured secret is never reflected into HTML, even if a value is accidentally supplied', () => {
  const { state } = browser();
  const field = { ...state.allSettings[0], value: 'must-not-reflect' };
  const rendered = state.renderControl(field);
  assert.ok(!rendered.includes('must-not-reflect'));
  assert.ok(rendered.includes('type="password"'));
  assert.ok(rendered.includes('value=""'));
  assert.ok(rendered.includes('clearPassword'));
});
test('unconfigured password is empty and needs no clear button', () => {
  const { state } = browser(false);
  assert.ok(!state.renderControl(state.allSettings[0]).includes('clearPassword'));
  assert.equal(state.getValue(state.allSettings[0]), undefined);
});
test('unrelated edit posts no password', async () => {
  const { state, controls } = browser();
  controls['setting-unrelated'].checked = true;
  await state.saveSettings();
  assert.deepEqual(state.posted, { unrelated: 1 });
  assert.equal(state.originalValues.koPassword, null);
});
test('new password posts explicitly, then leaves no secret in UI/original state', async () => {
  const { state, controls } = browser();
  controls['setting-koPassword'].value = 'new-secret';
  controls['setting-koPassword'].dataset.changed = 'true';
  await state.saveSettings();
  assert.deepEqual(state.posted, { koPassword: 'new-secret' });
  assert.equal(controls['setting-koPassword'].value, '');
  assert.equal(state.originalValues.koPassword, null);
  assert.equal(state.getValue(state.allSettings[0]), undefined);
  assert.equal(state.reloads, 1);
  assert.equal(state.allSettings[0].hasPassword, true);
});
test('clear is explicit and posts the empty string', async () => {
  const { state } = browser();
  state.clearPassword('koPassword');
  await state.saveSettings();
  assert.deepEqual(state.posted, { koPassword: '' });
  assert.equal(state.allSettings[0].hasPassword, false);
});
test('password is cleared from UI even if redacted reload fails', async () => {
  const { state, controls } = browser();
  state.loadSettings = async () => { throw new Error('offline'); };
  controls['setting-koPassword'].value = 'new-secret';
  controls['setting-koPassword'].dataset.changed = 'true';
  await state.saveSettings();
  assert.equal(controls['setting-koPassword'].value, '');
  assert.equal(state.originalValues.koPassword, null);
});
test('failed POST does not falsely report an accepted password update', async () => {
  const { state, controls } = browser();
  state.fetch = async () => ({ ok: false, text: async () => 'failed' });
  controls['setting-koPassword'].value = 'retry-secret';
  controls['setting-koPassword'].dataset.changed = 'true';
  await state.saveSettings();
  assert.equal(state.reloads, 0);
  assert.equal(state.originalValues.koPassword, null);
});
