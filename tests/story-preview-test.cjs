const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),path=require('node:path');
const root='assets/art-v1/story-v46';
class El{constructor(){this.value='';this.children=[];this.events={};}append(...xs){this.children.push(...xs)}replaceChildren(...xs){this.children=xs;this.value=xs[0]?.value??this.value}addEventListener(k,f){this.events[k]=f}}
const nodes={};const document={getElementById:id=>nodes[id]??=new El(),createElement:()=>new El()};
const context=vm.createContext({document,window:{}});vm.runInContext(fs.readFileSync(path.join(root,'replay-data.js'),'utf8'),context);nodes.route=new El();nodes.route.value='1';vm.runInContext(fs.readFileSync(path.join(root,'preview.js'),'utf8'),context);
for(const route of ['1','2','3']){nodes.route.value=route;nodes.route.events.change();for(const o of nodes.step.children){nodes.step.events.change({target:{value:o.value}});assert(nodes.chapter.textContent);assert(nodes.facts.children.length===5)}}
for(const b of nodes['screen-buttons'].children){b.events.click();assert(fs.existsSync(path.join(root,nodes.screen.src)))}
const replay=context.window.STORY_REPLAY;for(const route of [1,2,3]){const r=replay.filter(x=>x.route===route);assert(r.some(x=>x.node===22));assert(r.at(-1).state.story.new_leaf)}
console.log('Story preview: all3 actual-core routes, every recorded step and8 native screens navigate; referenced assets exist.');
