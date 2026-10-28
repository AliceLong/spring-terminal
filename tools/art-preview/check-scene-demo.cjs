// Offline canvas QA; no browser automation or image-asset modification.
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../assets/art-v2-20261018');
const demo=require(path.join(root,'scene-demo.js'));
const assets=JSON.parse(fs.readFileSync(path.join(root,'manifest.json'),'utf8')).assets;
for(const item of Object.keys(demo.config)){
  let carried=0;
  for(let t=0;t<=26;t+=1/30){
    const s=demo.sample(item,t);assert(Number.isFinite(s.x)&&Number.isFinite(s.y));
    assert(assets.some(a=>a.id===s.actor));
    if(t>10&&t<14.5){assert(s.object);assert(Math.abs(Math.abs(s.object.x-s.x)-24)<1e-6);carried++;}
    if(t>=20.5){assert(s.completed);assert(s.furniture);}
  }
  assert(carried>100);
  assert.equal(demo.sample(item,4).state,'REACT');
  assert.equal(demo.sample(item,8).state,'INSPECT');
  assert.equal(demo.sample(item,17).state,'BUILD');
}
assert.equal(demo.sample('cotton',24).state,'SLEEP');
assert(demo.sample('bottle_cap',2).object.y<demo.sample('bottle_cap',2.7).object.y);
for(let i=1;i<demo.phases.length;i++)assert.equal(demo.phases[i-1][1],demo.phases[i][0]);
console.log('Timeline checks passed: drop, discover, inspect, attached carry, construction, completed furniture, sleep.');
async function render(){
  const canvasModule=process.env.DESK_CANVAS_MODULE;
  if(!canvasModule)return;
  const {createCanvas,loadImage,GlobalFonts}=require(canvasModule);
  if(fs.existsSync('/System/Library/Fonts/Hiragino Sans GB.ttc'))GlobalFonts.registerFromPath('/System/Library/Fonts/Hiragino Sans GB.ttc','Hiragino Sans GB');
  const images={};await Promise.all(assets.map(async a=>images[a.id]=await loadImage(path.join(root,a.file))));
  const bg=await loadImage(path.join(root,'scene-background.png'));
  const sheet=createCanvas(1440,1920),ctx=sheet.getContext('2d');
  const times=[2.45,8.2,12.3,24.7];
  const out=path.join(root,'qa');fs.mkdirSync(out,{recursive:true});
  for(const [column,item] of Object.keys(demo.config).entries()){
    for(const [row,t] of times.entries()){
      const c=createCanvas(480,480),draw=demo.renderer(c.getContext('2d'),assets,images,bg);draw(demo.sample(item,t));
      ctx.drawImage(c,column*480,row*480);
    }
  }
  fs.writeFileSync(path.join(out,'scene-contact-sheet.png'),await sheet.encode('png'));
  console.log('Rendered 12 scene checkpoints into qa/scene-contact-sheet.png');
}
render().catch(e=>{console.error(e);process.exitCode=1;});
