// Unit-test the local preview event handlers with a fake canvas, no browser access.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const manifest=JSON.parse(fs.readFileSync('assets/art-v1/device/manifest.json','utf8'));const sizes=Object.fromEntries(manifest.assets.map(a=>[a.name,a]));
const drawn=[];const texts=[];const events=new Map();const frames=[];
const context=new Proxy({fillText(t){texts.push(String(t))},drawImage(...args){drawn.push(args[0].file);for(const v of args.slice(1))assert(Number.isFinite(v))},measureText(t){return{width:t.length*12}}},{get(t,k){if(k in t)return t[k];return()=>{}},set(t,k,v){t[k]=v;return true}});
function control(value){return{value,checked:false,addEventListener(k,v){events.set(this.id+':'+k,v)}}}
const action=control('auto');action.id='action';const voice=control('');voice.id='voice';voice.checked=true;const incoming=control('');incoming.id='incoming';const status={textContent:''};
function canvas(id){return{id,getContext(){return context},getBoundingClientRect(){return{left:0,top:0,width:480,height:480}},addEventListener(k,v){events.set(id+':'+k,v)}}}
const initial=canvas('initial'),grown=canvas('grown'),canopy=canvas('canopy');const elements={action,voice,incoming,status,initial,grown,canopy};
class FakeImage{set src(v){this.file=v;const name=v.split("/").pop().replace(".png","");const info=sizes[name]||({Room:{width:480,height:480},'Grass-integrated':{width:480,height:480},WindowNear:{width:960,height:90},Grass:{width:480,height:480},Planter:{width:74,height:49},Tree0:{width:480,height:480},Tree1:{width:480,height:480},Tree2:{width:480,height:480}})[name];assert(info,"Unknown preview image: "+v);assert(fs.existsSync("assets/art-v1/"+v));this.width=info.width;this.height=info.height;this.onload()}}
const sandbox={window:{DEVICE_ART:JSON.parse(fs.readFileSync('assets/art-v1/device/manifest.json','utf8'))},document:{querySelector(s){return elements[s.slice(1)]}},performance:{now(){return 5000}},Image:FakeImage,requestAnimationFrame(cb){frames.push(cb)},console,Math,Promise};
vm.createContext(sandbox);vm.runInContext(fs.readFileSync('assets/art-v1/square-preview.js','utf8'),sandbox);
function click(x,y){events.get('initial:pointerup')({clientX:x,clientY:y})}
function inspect(expression){return vm.runInContext(expression,sandbox)}
(async()=>{await new Promise(setImmediate);assert(frames.length);frames.shift()(6000);assert(status.textContent.includes('本地交互预览'));assert(drawn.some(f=>f.endsWith('/WindowNear.png')));assert(!drawn.some(f=>f.endsWith('/Far.png')),'Far landscape must remain static');
click(170,34);assert.equal(inspect('states[0].popup.kind'),'resource');assert.equal(inspect('states[0].popup.index'),1);inspect('draw(canvases[0],states[0],1000)');assert(texts.includes('雨窝苔露珠产出'));assert(!texts.some(t=>t.includes('水池')));click(430,100);assert.equal(inspect('states[0].popup'),null);
click(70,310);assert.equal(inspect('states[0].popup.kind'),'plant');click(356,311);assert.equal(inspect('states[0].chosen'),2);click(145,372);assert.equal(inspect('states[0].next[0]'),2);assert.equal(inspect('states[0].popup'),null);
for(const [index,x] of [70,151,232].entries()){click(x,310);assert.equal(inspect('states[0].popup.index'),index);click(331,372)}
click(430,300);assert.equal(inspect('states[0].popup.kind'),'tree');click(331,372);click(440,36);assert.equal(inspect('states[0].popup.kind'),'log');click(145,372);assert.equal(inspect('states[0].popup.kind'),'demo');click(331,372);
incoming.checked=true;events.get('incoming:change')();assert.equal(action.value,'speak');assert(inspect('states[0].subtitle.includes("新叶")'));click(50,402);assert.equal(inspect('states[0].popup.kind'),'call');click(145,372);assert.equal(incoming.checked,false);assert.equal(inspect('states[0].popup'),null);
assert.equal(inspect('states.length'),3);for(let i=0;i<3;i++){assert.equal(inspect(`states[${i}].treeStage`),i);inspect(`states[${i}].popup={kind:'tree'};draw(canvases[${i}],states[${i}],1200);states[${i}].popup=null`)}assert(texts.includes('第三阶段 · 树冠铺满天花板'));
texts.length=0;action.value='listen';inspect('draw(canvases[0],states[0],1200)');action.value='speak';inspect('draw(canvases[0],states[0],1400)');assert(!texts.some(t=>t.includes('聆听中')||t.includes('说话中')));
console.log('Preview handler tests passed: resources, popover outside-close, plant next-cycle choice, tree/log/demo, incoming radio/opening, no robot status text; three tree stages and resized plant hit areas.');})().catch(e=>{console.error(e);process.exitCode=1});
