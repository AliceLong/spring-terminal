// Art-direction preview. The real economic replay remains read-only.
window.GARDEN_LAYOUT_V2 = {
  paths: {
    room:'economy/layout-v3/room.png', rack:'economy/layout-v3/rack.png',
    barrel:'economy/layout-v3/barrel.png', planter:'economy/layout-v3/planter.png',
    tree:'economy/layout-v2/tree.png',pool:'economy/layout-v2/pool.png',
    'bird-home':'economy/layout-v2/bird-home.png','animal-home':'economy/layout-v2/animal-home.png'
  },
  draw(c,s,images,assets,{showcase=false,stage=2}={}) {
    function sprite(id,x,y,k=1){const a=assets[id],im=images[id];c.drawImage(im,x+a.offset[0]*k,y+a.offset[1]*k,im.width*k,im.height*k)}
    function layer(id,x,y,w,h){c.drawImage(images['Layout_'+id],x,y,w,h)}
    const e=s.economy,built=i=>showcase||e.facilities[i].built,tree=showcase?stage:s.tree.stage;
    layer('room',0,0,480,480);
    if(tree===0)sprite('TreeYoung',446,335,1.05);
    else if(tree===1)layer('tree',314,58,218,285);
    else layer('tree',238,-29,311,383);
    // The rack is intentionally furniture-sized: 112 x 270, rather than 40px icons.
    if(showcase||(built(2)&&built(3)&&built(5)))layer('rack',-12,76,139,274);
    else {
      if(built(2))sprite('Facility_solar',15,93,1.9);
      if(built(3))sprite('Facility_battery',18,181,1.6);
      if(built(5))sprite('Facility_bench',10,250,2.15);
    }
    if(built(1))layer('barrel',103,207,99,135);
    if(built(7)){
      if(tree===0){c.fillStyle='#87653b';c.fillRect(456,124,9,191)}
      layer('bird-home',372,107,110,76);
      layer('animal-home',396,184,96,88);
      if(showcase||(e.animals&1))sprite('Animal_bird',414,123,1.15);
      if(showcase||(e.animals&2))sprite('Animal_mouse',434,235,1.05);
    }
    if(built(0))layer('pool',304,264,166,129);
    // Robot walks on the middle floor, visibly behind but above the foreground beds.
    sprite('Robot_idle_0',233,357,.88);
    const count=s.plants.length;
    s.plants.forEach((p,i)=>{
      const side=count===4&&i===3;
      const x=side?442:137+94*i,y=side?390:393;
      layer('planter',x-48,y-21,96,66);
      sprite('Plant_'+p.id+'_'+(showcase||p.ready?'mature':p.progress>=.5?'growing':'seedling'),x,y,.84);
    });
    if(built(6))sprite('Facility_care',422,345,1.1);
  }
};
