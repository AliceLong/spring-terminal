"""Compare actual-Core fixtures with proposal arithmetic; never touches firmware/save."""
import csv,html,json,math
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'evidence/v56'
out.mkdir(parents=True,exist_ok=True)
actual=json.loads((out/'economy-actual-core.json').read_text());cfg=json.loads((root/'tests/spring/v56-proposal-prototype.json').read_text())
rows=[]
for mode in ('auto','manual'):
    stock=cfg['initial_water'];yield_=math.floor(cfg[f'normal_{mode}_yield']*cfg['drought_factor'])
    rows.append(dict(mode=mode,minute=0,water=stock))
    for minute in range(1,cfg['drought_minutes']+1):
        stock+=yield_-cfg['maintenance_water_per_minute'];rows.append(dict(mode=mode,minute=minute,water=stock))
assert [x['water'] for x in rows if x['mode']=='auto']==[8,6,4,2]
assert [x['water'] for x in rows if x['mode']=='manual']==[8,7,6,5]
summary=dict(prototype=rows,drought_manual_bonus_percent=(math.floor(cfg['normal_manual_yield']*.5)/math.floor(cfg['normal_auto_yield']*.5)-1)*100,
    compounded_20pct_yield_10pct_acceleration_percent=(1.2/.9-1)*100,actual_offline_cap_seconds=actual['offline24h']['actually_simulated_seconds'])
(out/'economy-evaluation-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
with (out/'economy-traces.csv').open('w') as f:
    w=csv.writer(f);w.writerow(['scenario','second','water','energy','fiber','food','parts'])
    for s in actual['scenarios']:
        for r in s['samples']:w.writerow([s['name']]+[r[k] for k in ('second','water','energy','fiber','food','parts')])
def plot(samples,series,width=620,height=180):
    end=max(s['second'] for s in samples);cap=max(10,max(s[k] for s in samples for k,_ in series));lines=[]
    for key,color in series:
        pts=' '.join(f'{30+s["second"]/end*(width-50):.1f},{height-25-s[key]/cap*(height-40):.1f}' for s in samples)
        lines.append(f'<polyline fill="none" stroke="{color}" stroke-width="2" points="{pts}"/>')
    return f'<svg viewBox="0 0 {width} {height}" role="img" aria-label="资源随时间曲线"><path d="M30 10V{height-25}H{width-10}" fill="none" stroke="#bac4b8"/><text x="30" y="{height-4}" font-size="11">0分</text><text x="{width-60}" y="{height-4}" font-size="11">{end//60}分</text>'+''.join(lines)+'</svg>'
parts=['<!doctype html><meta charset="utf-8"><title>V5.6 经营与收获审计</title><style>body{max-width:960px;margin:36px auto;padding:16px;font:16px/1.7 system-ui;background:#f5f3e9;color:#243c32}article{background:white;padding:22px;margin:20px 0;border-radius:16px}svg{width:100%}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:10px;border-bottom:1px solid #ddd}small{color:#5d6d63}code{font-size:13px}</style><h1>V5.6：收获值得强化，压力数值需要重标</h1><p>数据来自隔离测试世界中的当前 Core。未修改设备、正式存档或生产配置。固定地区测试主动固定了环境；两动物测试从已有设施、两只动物及食物8开始，不包含建设与救助过程。</p><article><h2>主要结论</h2><p>目前手动收获主要改善时机：首笔资源 610秒，对比自动 650秒；本次两小时末库存相同。雨窝苔不参与现有轻晃收获，因此当前轻晃不能在干旱中额外补水。</p><p>新稿的60秒周期与现有600秒周期不能混用。正常5→6是20%奖励，但干旱整数化2→3是50%。若再叠加10%生长加速，20%额外产出会放大为约33.3%的单位时间产出提升。</p><p>基础成本已在生产开始扣除。不要叠加“4水/分钟”作为正式维护费。先做基础产出一次性结算＋至多20%精细奖励；生长加速另轮验证。</p></article><h2>实际 Core 资源曲线</h2><p>蓝：水　金：能源　绿：藤纤维　棕：食物。测试不主动批准建设或探索。</p>']
for s in actual['scenarios']:
    end=s['samples'][-1]
    parts.append(f'<article><h3>{html.escape(s["name"])}</h3>'+plot(s['samples'],[('water','#3c91bb'),('energy','#c98a22'),('fiber','#619259'),('food','#936d4d')])+f'<p>末库存：水{end["water"]}，能源{end["energy"]}，纤维{end["fiber"]}，食物{end["food"]}，零件{end["parts"]}。手动收获{s["manual_harvests"]}次。首次收获{s["first_harvest_seconds"]}秒。</p><small>水≤4累计{s["low_water_seconds"]}秒；有未投入槽{s["unpaid_slot_seconds"]}秒（含正常换轮间隙，不能直接视为经营死锁）。收获溢出{s["harvest_overflow"]}，顺序为能源、水、纤维、食物。</small></article>')
parts.append('<article><h2>草案三分钟干旱数学复核</h2><p>这组为独立简化算式，不是ESP32实际循环。假定每分钟产出与维护同轮结算。</p><table><tr><th>分钟</th><th>自动水</th><th>完整手动水</th></tr>')
for minute in range(4):parts.append(f'<tr><td>{minute}</td><td>{rows[minute]["water"]}</td><td>{rows[minute+4]["water"]}</td></tr>')
parts.append('</table><p>算式成立，但维护结算顺序、起始投入、自动45秒等待和整数奖励均未包含；不能据此认定真实经营已平衡。</p></article><article><h2>离线边界</h2><p>当前一次24小时离线恢复只模拟前12小时（43200秒），不是完整24小时。持续在线24小时、分段推进24小时，与一次24小时恢复是不同测试。</p></article>')
(out/'economy-report.html').write_text(''.join(parts))
print('Actual-Core traces, prototype math, CSV and standalone report written. No game changes.')
