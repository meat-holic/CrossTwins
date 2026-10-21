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
  return rest.slice(0, next + 1);
}
function browser() {
 const controls = { 'setting-fontFamily':{value:'0',dataset:{}},'setting-fontSize':{value:'1',dataset:{},disabled:false},saveBtn:{disabled:false,textContent:''} };
 const state={allSettings:[{key:'fontFamily',type:'enum',value:0,options:['가','나']},{key:'fontSize',type:'enum',value:1,options:['10 pt','14 pt']}],originalValues:{fontFamily:0,fontSize:1},document:{getElementById:id=>controls[id]},reloads:0,showWebToast(){}};
 state.fetch=async(_,request)=>{state.posted=JSON.parse(request.body);return {ok:true};};state.loadSettings=async()=>{state.reloads++;};
 const context=vm.createContext(state);
 for(const name of ['getValue','findSetting','getControl','getControlValue','setControlValue','markChanged','isQuickResumeSleepScreenSelected','syncQuickResumeTimeoutForSleepScreen','updateSettingsVisibility','handleSettingChanged','saveSettings']) vm.runInContext(productionFunction(name),context);
 return {state,controls};
}
test('family change disables stale sizes and saves only family before reload',async()=>{
 const {state,controls}=browser();controls['setting-fontFamily'].value='1';state.handleSettingChanged('fontFamily');
 assert.equal(controls['setting-fontSize'].disabled,true);assert.equal(state.getValue(state.allSettings[1]),undefined);
 await state.saveSettings();assert.deepEqual(state.posted,{fontFamily:1});assert.equal(state.reloads,1);
});
test('switching back to original family reenables its existing size list',()=>{
 const {state,controls}=browser();controls['setting-fontFamily'].value='1';state.handleSettingChanged('fontFamily');controls['setting-fontFamily'].value='0';state.handleSettingChanged('fontFamily');
 assert.equal(controls['setting-fontSize'].disabled,false);assert.equal(state.getValue(state.allSettings[1]),1);
});
test('size-only edit remains an index and does not change the family',async()=>{
 const {state,controls}=browser();controls['setting-fontSize'].value='0';await state.saveSettings();assert.deepEqual(state.posted,{fontSize:0});assert.equal(state.reloads,0);
});
test('unadopted PluginHost is absent from the settings page',()=>{
 assert.equal(html.includes('PluginHost.js.inc'),false);assert.equal(html.includes("loadPlugins('settings')"),false);assert.equal(html.includes('id="plugin-container"'),false);
});
