#!/usr/bin/env python3
from pathlib import Path
import json,sys,re
rows=[]
for path in sorted(Path(sys.argv[1]).glob("*.c.gcov")):
    executed=total=taken=branches=0
    for l in path.read_text().splitlines():
        if l.startswith("branch"):
            branches+=1
            m=re.search(r"taken (\d+)",l)
            if m and int(m[1])>0:taken+=1
        elif ":" in l:
            tag=l.split(":")[0].strip()
            if tag!="-":
                total+=1
                if tag not in ("#####","====="):executed+=1
    if not total: continue
    row={"module":path.name,"lines_hit":executed,"lines":total,
         "line_percent":round(100*executed/total,2),
         "branches_taken":taken,"branches":branches,
         "branch_percent":round(100*taken/branches,2) if branches else 100}
    rows.append(row)
    print(row)
Path(sys.argv[1],"summary.json").write_text(json.dumps(rows,indent=2))
assert len(rows)==3 and all(r["line_percent"]>=90 and r["branch_percent"]>=80 for r in rows)
print("PASS coverage targets: >=90% lines, >=80% branches in App/Devices")
