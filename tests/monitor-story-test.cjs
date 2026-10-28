const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
class El{constructor(tag){this.tag=tag;this.children=[];this.textContent='';}append(...xs){this.children.push(...xs)}replaceChildren(...xs){this.children=xs}get childElementCount(){return this.children.length}}
const nodes={};const document={getElementById:id=>nodes[id]??=(new El('div')),createElement:t=>new El(t)};
const context=vm.createContext({document,setTimeout(){},fetch:async()=>({ok:true,json:async()=>({connection:{live:false},snapshot:null})}),setupArchive(){},updateArchiveStatus(){},refreshArchive(){},currentMemory:null,currentArchive:null});
vm.runInContext(fs.readFileSync('monitor/app.js','utf8'),context);
const flatten=e=>[e.textContent,...e.children.map(flatten)].join(' ');
const snapshot=JSON.parse(fs.readFileSync('evidence/monitor-host-snapshot.json'));
context.data={connection:{live:true,age_seconds:0,frames:1,port:'test'},snapshot};vm.runInContext('render(data)',context);let text=flatten(nodes.modules);assert(text.includes('暂无独立设备字段'));
const replay=JSON.parse(fs.readFileSync('evidence/story-v46/replay.json'));
snapshot.world=replay.at(-1).state;
context.data={connection:{live:true,age_seconds:0,frames:2,port:'test'},snapshot};vm.runInContext('render(data)',context);text=flatten(nodes.modules);assert(text.includes('V4.6设备剧情'));assert(text.includes('头顶新叶'));assert(text.includes('已长出')||text.includes('是'));assert(text.includes('本次启动 · 剧情查询'));assert(!text.includes('语言进度关联树成长'));
console.log('Monitor: legacy fallback and actual V4.6 world/story data render; no invented cloud state.');
