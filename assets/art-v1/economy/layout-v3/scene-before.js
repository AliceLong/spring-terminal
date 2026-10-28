// Local art review only: no firmware assets, save data or economic rules change.
window.GARDEN_LAYOUT_V2 = {
  paths: Object.fromEntries(['tree','pool','bird-home','animal-home'].map(k=>[k,'economy/layout-v2/'+k+'.png'])),
  draw(c,s,images,assets,{showcase=false,stage=2}={}) {
    function sprite(id,x,y,k=1){const a=assets[id],im=images[id];c.drawImage(im,x+a.offset[0]*k,y+a.offset[1]*k,im.width*k,im.height*k)}
    function layer(id,x,y,w,h){c.drawImage(images['Layout_'+id],x,y,w,h)}
    const e=s.economy,built=i=>showcase||e.facilities[i].built,tree=showcase?stage:s.tree.stage;
    sprite('Background',0,0);
    c.save();c.beginPath();for(const p of [[56,90,75,204],[151,90,179,204],[353,90,70,204]])c.roundRect(...p,9);c.clip();sprite('Near',0,204);c.restore();
    // Roots share the same floor line. Canopy may intentionally leave the screen.
    if(tree===0)sprite('TreeYoung',450,402,.82);
    else if(tree===1)layer('tree',302,117,207,285);
    else layer('tree',221,-7,309,409);
    // Utilities form a vertical group on the left, clear of the central walking lane.
    if(built(2))sprite('Facility_solar',22,137,1.48);
    if(built(3))sprite('Facility_battery',26,215,1.12);
    if(built(5))sprite('Facility_bench',24,291,1.4);
    if(built(1))sprite('Facility_rain',96,279,1.2);
    if(built(6))sprite('Facility_care',139,326,.8);
    if(built(7)){
      // Young trees use a support branch/post until their own trunk can carry homes.
      if(tree===0){c.fillStyle='#795e3e';c.fillRect(447,197,8,177)}
      layer('bird-home',385,tree===0?197:161,83,57);
      layer('animal-home',413,252,68,62);
      if(showcase||(e.animals&1))sprite('Animal_bird',417,tree===0?209:173,.85);
      if(showcase||(e.animals&2))sprite('Animal_mouse',435,286,.78);
    }
    // Storage tank is now the stone basin; collection remains a separate utility.
    if(built(0))layer('pool',322,316,145,124);
    sprite('Robot_idle_0',235,400,.76);
    // Foreground planting boxes, with the walking lane behind them.
    s.plants.forEach((p,i)=>{
      const x=s.plants.length===4?120+65*i:135+81*i;
      sprite('Planter',x-35,413,1.1);
      sprite('Plant_'+p.id+'_'+(showcase||p.ready?'mature':p.progress>=.5?'growing':'seedling'),x,420,.83);
    });
  }
};
