import os,re,sys,json
pk={}
for repo in ("libs","win"):
    base=f"db/{repo}"
    for d in os.listdir(base):
        f=os.path.join(base,d,"desc")
        if not os.path.exists(f): continue
        txt=open(f,encoding="utf-8").read()
        fields={}
        for m in re.finditer(r"%([A-Z]+)%\n(.*?)(?:\n\n|\Z)",txt,re.S):
            fields[m.group(1)]=m.group(2).split("\n")
        name=fields["NAME"][0]
        pk[name]=dict(repo=repo,file=fields["FILENAME"][0],deps=fields.get("DEPENDS",[]),groups=fields.get("GROUPS",[]),provides=fields.get("PROVIDES",[]),size=int(fields.get("CSIZE",["0"])[0]))
prov={}
for n,p in pk.items():
    for x in p["provides"]: prov[re.split(r"[<>=]",x)[0]]=n
grp={}
for n,p in pk.items():
    for g in p["groups"]: grp.setdefault(g,[]).append(n)
print("3ds-dev group:",sorted(grp.get("3ds-dev",[])))
want=grp.get("3ds-dev",[])+sys.argv[1:]
seen=[];miss=[]
def add(n):
    n=re.split(r"[<>=]",n)[0]
    if n in pk: pass
    elif n in prov: n=prov[n]
    else: miss.append(n); return
    if n in seen: return
    seen.append(n)
    for d in pk[n]["deps"]: add(d)
for w in want: add(w)
tot=sum(pk[n]["size"] for n in seen)
print("install:",len(seen),"pkgs, %.1f MB compressed"%(tot/1e6))
for n in sorted(seen): print(" ",pk[n]["repo"],pk[n]["file"])
print("missing deps:",sorted(set(miss)))
json.dump([[pk[n]["repo"],pk[n]["file"]] for n in seen],open(os.environ.get("PLAN_OUT","plan.json"),"w"))
