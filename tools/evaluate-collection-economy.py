"""Compare candidate harvest economics using the actual Core; no device/save access."""
import csv
import html
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "evidence/harvest-v56"
OUT.mkdir(parents=True, exist_ok=True)
CONFIG = ROOT / "tests/spring/collection-economy-candidates.json"
cfg = json.loads(CONFIG.read_text())
binary = ROOT / "tests/.build/collection-economy-audit"
binary.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(["clang++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-I", str(ROOT / "vendor/xiaozhi-esp32/main"),
                str(ROOT / "tests/spring/collection_economy_audit.cc"), "-o", str(binary)], check=True)
rows = []
for profile in cfg["profiles"]:
    for scene in cfg["scenarios"]:
        args = [profile["normal_moss"], profile["drought_moss"], cfg["bonus_percent"],
                scene["hits"], cfg["particles"], scene["region"], scene["seconds"],
                scene["water"], scene["energy"], scene["scale"], scene["special"],
                cfg["capture_real_seconds"]]
        actual = json.loads(subprocess.check_output([str(binary), *map(str, args)], text=True))
        rows.append(dict(profile=profile["id"], scenario=scene["id"], result=actual,
                         game_seconds_per_real_second=scene["scale"], simulated=True))


def find(profile, scene):
    return next(x["result"] for x in rows if x["profile"] == profile and x["scenario"] == scene)


comparisons = []
for p in cfg["profiles"]:
    name = p["id"]
    a = find(name, "drought_auto_3demo_minutes")
    m = find(name, "drought_manual_3demo_minutes")
    comparisons.append(dict(
        profile=name, auto_water=a["final"]["water"], manual_water=m["final"]["water"],
        difference=m["final"]["water"] - a["final"]["water"],
        auto_shortage_seconds=a["shortage_seconds"], manual_shortage_seconds=m["shortage_seconds"],
        manual_water_bonus=m["manual_bonus_gain"][1],
        note="Fixed 30 game minutes = 3 demo minutes; includes upfront costs, grace and delayed bonus.",
    ))
    a6 = find(name, "drought_auto_6demo_minutes")
    m6 = find(name, "drought_manual_6demo_minutes")
    comparisons[-1]["longer_window"] = dict(
        demo_minutes=6, auto_water=a6["final"]["water"], manual_water=m6["final"]["water"],
        auto_shortage_seconds=a6["shortage_seconds"], manual_shortage_seconds=m6["shortage_seconds"],
        auto_base_gain=a6["base_gain"], manual_base_gain=m6["base_gain"],
        manual_bonus_gain=m6["manual_bonus_gain"],
    )
report = dict(config=str(CONFIG.relative_to(ROOT)), scenarios=len(rows),
              resource_order=["energy", "water", "fiber", "food"],
              production_numbers_changed=False, formal_bonus_implemented=False,
              comparisons=comparisons, rows=rows,
              caveats=[
                  "Candidate yields and fractional bonuses exist only in this host runner.",
                  "Base is credited by existing Core once; bonus is simulated after six real seconds.",
                  "Not a pending-base-resource batch or crash-recoverable bonus implementation.",
                  "Fractional carry is discarded when full; it is not a sixth resource.",
                  "Offline rows use unchanged Core 12h recovery cap; candidate overrides do not run in Sync.",
                  "No growth assistance, no additional maintenance fee, no facility/rescue strategy simulated.",
              ])
(OUT / "economy-candidates.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
with (OUT / "economy-candidates.csv").open("w") as f:
    writer = csv.writer(f)
    writer.writerow(["profile", "scenario", "game_second", "water", "energy", "fiber", "food", "parts"])
    for row in rows:
        for s in row["result"].get("samples", []):
            writer.writerow([row["profile"], row["scenario"], s["second"],
                             *[s[k] for k in ("water", "energy", "fiber", "food", "parts")]])


def curve(auto, manual):
    paths = []
    end = auto[-1]["second"]
    for samples, color, dash in ((auto, "#607968", "6 4"), (manual, "#2887b3", "")):
        points = " ".join(f'{35+s["second"]/end*570:.1f},{170-s["water"]/20*140:.1f}' for s in samples)
        paths.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="3" stroke-dasharray="{dash}"/>')
    return '<svg viewBox="0 0 640 200" role="img" aria-label="三分钟演示干旱水库存对比">' + \
           '<path d="M35 20V170H610" stroke="#b5bba7" fill="none"/>' + "".join(paths) + \
           '<text x="35" y="195" font-size="12">0分钟</text><text x="565" y="195" font-size="12">3分钟</text></svg>'


parts = ['<!doctype html><meta charset="utf-8"><title>V5.6 接取与经营候选审计</title>',
         '<style>body{font:16px/1.65 system-ui;background:#f5f2e8;color:#253e33;max-width:1040px;margin:32px auto;padding:16px}article{background:white;padding:24px;border-radius:16px;margin:24px 0}table{border-collapse:collapse;width:100%}td,th{text-align:left;padding:9px;border-bottom:1px solid #ddd}svg{width:100%}small{color:#65756a}</style>',
         '<h1>接取原型与经济候选：实际Core隔离模拟</h1>',
         '<p>所有候选只在电脑测试世界运行，没有改写设备库存或正式产量。蓝线：完整手动，绿虚线：自动。六秒接取收益延迟、生产预扣费用及自动收获等待均包含。</p>',
         '<article><h2>首先确认的边界</h2><p>正常一轮600游戏秒；十倍演示约60现实秒。三分钟演示不是三分钟正常游戏。现有生产已收费，不另加每分钟4水。手动额外收益按20%计算，低产量用有界余数累计，不能把干旱2水直接加成3水而称作20%。</p>',
         '<p>离线24小时仍走原Core最多12小时恢复；下面离线行只验证当前恢复路径，不代表候选产量已接入离线结算。</p></article>']
for p, comparison in zip(cfg["profiles"], comparisons):
    a = find(p["id"], "drought_auto_3demo_minutes")
    m = find(p["id"], "drought_manual_3demo_minutes")
    parts.extend([f'<article><h2>{html.escape(p["id"])}：苔正常{p["normal_moss"]}、干旱{p["drought_moss"]}</h2>',
                  curve(a["samples"], m["samples"]),
                  f'<p>末水量：自动{comparison["auto_water"]}，完整手动{comparison["manual_water"]}，差{comparison["difference"]}。实际获得的额外水{comparison["manual_water_bonus"]}。</p>',
                  f'<p>真实缺投入累计：自动{comparison["auto_shortage_seconds"]}游戏秒，手动{comparison["manual_shortage_seconds"]}游戏秒。低库存可能因为下一轮投入已经扣除，不能直接看成作物已经停产。</p>',
                  '<table><tr><th>情境</th><th>末库存 水/能/纤/食/件</th><th>缺投入秒数</th><th>额外收益 能/水/纤/食</th></tr>'])
    for row in rows:
        if row["profile"] != p["id"]:
            continue
        result = row["result"]
        end = result["final"]
        numbers = "/".join(str(end[k]) for k in ("water", "energy", "fiber", "food", "parts"))
        parts.append(f'<tr><td>{html.escape(row["scenario"])}</td><td>{numbers}</td><td>{result.get("shortage_seconds", "恢复上限12h")}</td><td>{html.escape(str(result.get("manual_bonus_gain", "无手动收益")))}</td></tr>')
    parts.append('</table></article>')
parts.append('<article><h2>接入门槛</h2><p>本报告验证收益方向与缺口，不证明游玩乐趣。先真机验证六物件接取及通话声音；再决定基础立即入库还是延迟批次。新批次与余数必须在同一持久事务里提交，之后再把选定候选接入正式经济和离线路径。</p></article>')
(OUT / "economy-report.html").write_text("".join(parts))
print(json.dumps(dict(scenarios=len(rows), comparisons=comparisons), ensure_ascii=False))
