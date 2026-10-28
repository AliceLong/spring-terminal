// Local scene composition only; no device assets or economy state are mutated.
window.GARDEN_LAYOUT_V2 = {
  paths: {
    room:'economy/layout-v3/room.png',
    'rack-empty':'economy/layout-v3/equipment-split/rack-empty.png',
    solar:'economy/layout-v3/equipment-split/solar.png',
    battery:'economy/layout-v3/equipment-split/battery.png',
    bench:'economy/layout-v3/equipment-split/bench.png',
    barrel:'economy/layout-v3/barrel.png',planter:'economy/layout-v3/planter.png',
    tree:'economy/layout-v2/tree.png',pool:'economy/layout-v2/pool.png',
    'bird-home':'economy/layout-v2/bird-home.png','animal-home':'economy/layout-v2/animal-home.png'
  },
  regions: [],
  draw(c,s,images,assets,{showcase=false,stage=2,renderRobot=true}={}) {
    this.regions=[];
    function sprite(id,x,y,k=1){const a=assets[id],im=images[id];c.drawImage(im,x+a.offset[0]*k,y+a.offset[1]*k,im.width*k,im.height*k)}
    function layer(id,x,y,w,h){c.drawImage(images['Layout_'+id],x,y,w,h)}
    const region=(name,x,y,w,h,detail)=>this.regions.push({name,x,y,w,h,detail});
    const e=s.economy,built=i=>showcase||e.facilities[i].built,tree=showcase?stage:s.tree.stage;
    layer('room',0,0,480,480);
    // Rear roots stay anchored to the rear floor; canopy grows upward and left.
    if(tree===0)sprite('TreeYoung',451,346,1.18);
    else if(tree===1)layer('tree',307,72,226,274);
    else layer('tree',252,-16,300,362);
    region('大树',359,70,121,274,['小树苗','成树','满顶树冠'][tree]);
    // Shelving is scenery; each device has its own asset, unlock bit and hit target.
    layer('rack-empty',-7,129,147,229);
    region('空设备架',6,133,125,222,'固定摆放结构，不是一项合并建设');
    if(built(2)){
      layer('solar',13,82,103,86);
      region('集能板',16,84,98,79,'独立建设 · 收集能源');
    }
    if(built(3)){
      layer('battery',24,184,78,64);
      region('蓄能箱',24,184,78,64,'独立建设 · 提高能源容量');
    }
    if(built(5)){
      layer('bench',13,267,100,68);
      region('工作台',13,267,100,68,'独立建设 · 拆解回收物');
    }
    if(built(1)){
      layer('barrel',107,219,95,139);
      region('集雨器',112,222,84,133,'漏斗集水桶，与储水池分开');
    }
    if(built(7)){
      // When the tree is young the homes sit lower on a shared support.
      const birdY=tree===0?145:127,homeY=tree===0?221:206;
      if(tree===0){c.fillStyle='#87653b';c.fillRect(451,158,8,184)}
      layer('bird-home',370,birdY,106,73);
      layer('animal-home',394,homeY,85,78);
      if(showcase||(e.animals&1))sprite('Animal_bird',411,birdY+15,1.15);
      if(showcase||(e.animals&2))sprite('Animal_mouse',428,homeY+46,1.05);
      region('树上鸟窝',386,birdY+10,86,57,'寻雨雀的住所');
      region('树干小窝',402,homeY+12,75,62,'拾荒鼠的住所');
    }
    if(built(0)){
      layer('pool',313,277,161,121);
      region('储水池',316,302,153,91,'石砌储水池 · 竹管出水口');
    }
    if(built(6)){
      sprite('Facility_care',432,365,1.02);
      region('照料装置',432,365,44,43,'辅助植物照料');
    }
    // Open middle lane: feet remain above the foreground bed silhouettes.
    if(renderRobot){
      sprite('Robot_idle_0',246,369,.92);
      region('园丁',209,268,80,101,'中间活动区，前排留给种植箱');
    }
    s.plants.forEach((p,i)=>{
      const side=i===3,x=side?446:130+104*i,y=side?401:407;
      layer('planter',x-51,y-18,102,62);
      sprite('Plant_'+p.id+'_'+(showcase||p.ready?'mature':p.progress>=.5?'growing':'seedling'),x,y,.88);
      region(`${i+1}号种植位`,x-49,y-72,98,114,'前排木种植箱');
    });
  },
  pick(x,y){return this.regions.slice().reverse().find(r=>x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)}
};
