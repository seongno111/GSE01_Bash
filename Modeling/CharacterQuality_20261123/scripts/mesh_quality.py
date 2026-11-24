"""Shared geometry construction for the character quality revision."""
import sys
from math import cos, sin, pi
from pathlib import Path

import bpy
import bmesh
from mathutils import Matrix, Vector

SCRIPTS=Path(__file__).resolve().parents[2]/'HeroWeaponAnimations_v1/scripts'
sys.path.insert(0,str(SCRIPTS))
from hero_model import TailoredMesh


def material(name,color,metal=0,rough=.8):
    key='MAT_CQ_'+name
    m=bpy.data.materials.get(key) or bpy.data.materials.new(key)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    bsdf=m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value=(*color,1)
    bsdf.inputs['Metallic'].default_value=metal
    bsdf.inputs['Roughness'].default_value=rough
    return m


def palette():
    return {
        'cloth':material('AshenWool',(.065,.080,.086)),
        'panel':material('WovenFold',(.102,.116,.115)),
        'dark':material('SeamShadow',(.029,.037,.041)),
        'robe':material('ClericGrey',(.105,.116,.122)),
        'stole':material('FadedOxblood',(.130,.053,.046)),
        'leather':material('WaxedLeather',(.105,.068,.040)),
        'sole':material('BootSole',(.028,.025,.023)),
        'iron':material('ForgedIron',(.125,.160,.181),.70,.50),
        'edge':material('WornSteel',(.245,.277,.291),.74,.43),
        'brass':material('TarnishedBrass',(.228,.163,.064),.62,.63),
        'stitch':material('LinenStitch',(.253,.208,.146)),
        'skin':material('AshenSkin',(.282,.261,.223)),
        'bone':material('WeatheredBone',(.305,.295,.236)),
        'flesh':material('CharredHide',(.121,.075,.080)),
        'horn':material('HornKeratin',(.181,.164,.123)),
        'rot':material('BlightedGrowth',(.140,.166,.076)),
    }


class Mesh(TailoredMesh):
    def ellipsoid(self,center,scale,weight,color,sides=12,rows=6):
        center=Vector(center)
        loops=[]
        for i in range(rows+1):
            phi=-pi/2+pi*i/rows
            ring=max(.025,cos(phi))
            loops.append([center+Vector((scale[0]*ring*cos(2*pi*j/sides),
                         scale[1]*ring*sin(2*pi*j/sides),scale[2]*sin(phi))) for j in range(sides)])
        self.loft(loops,[weight]*len(loops),[color],True)

    def fit(self,obj):
        old=obj.data
        data=bpy.data.meshes.new(obj.name+'_Quality')
        data.from_pydata(self.vertices,[],self.faces)
        keys=list(dict.fromkeys(self.materials))
        for k in keys:data.materials.append(self.palette[k])
        for poly,k in zip(data.polygons,self.materials):poly.material_index=keys.index(k)
        data.update()
        obj.data=data
        obj.matrix_world=Matrix.Identity(4)
        obj.vertex_groups.clear()
        groups={}
        for i,w in enumerate(self.weights):
            if isinstance(w,str):w={w:1}
            total=sum(w.values())
            for bone,value in w.items():
                if value<=0:continue
                if bone not in groups:groups[bone]=obj.vertex_groups.new(name=bone)
                groups[bone].add([i],value/total,'REPLACE')
        bm=bmesh.new();bm.from_mesh(data)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(data);bm.free()
        if old.users==0:bpy.data.meshes.remove(old)

    def append(self,obj):
        """Combine sculpted detail into an existing named mesh and its skin."""
        old=Mesh(self.palette)
        mats={m.name:m for m in obj.data.materials if m}
        old.palette.update(mats)
        groups={g.index:g.name for g in obj.vertex_groups}
        for v in obj.data.vertices:
            old.vertex(v.co,{groups[g.group]:g.weight for g in v.groups if g.weight>0})
        for face in obj.data.polygons:
            old.face(tuple(face.vertices),obj.data.materials[face.material_index].name)
        offset=len(old.vertices)
        old.vertices.extend(self.vertices);old.weights.extend(self.weights)
        old.faces.extend(tuple(i+offset for i in face) for face in self.faces)
        old.materials.extend(self.materials)
        old.fit(obj)


def bevel(obj,width=.003):
    bpy.context.view_layer.objects.active=obj
    obj.hide_set(False)
    mod=obj.modifiers.new('Forged edges','BEVEL')
    mod.width=width;mod.segments=1;mod.limit_method='ANGLE';mod.angle_limit=.65
    bpy.ops.object.modifier_move_up(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)


def humanoid_feet(obj,rig):
    """Give a joined creature mesh articulated toes or contoured layered boots."""
    bones=rig.data.bones
    scale=bones['head'].tail_local.z/1.84
    group={g.index:g.name for g in obj.vertex_groups}
    removed={v.index for v in obj.data.vertices
             if any(group[g.group].startswith('foot_') and g.weight>.8 for g in v.groups)}
    if not removed:return
    data=obj.data
    bm=bmesh.new();bm.from_mesh(data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm,geom=[bm.verts[i] for i in removed],context='VERTS')
    bm.to_mesh(data);bm.free()
    mesh=Mesh(palette())
    bare=any(k in obj.name for k in ('Hollow','Azar','Mutated','Echo'))
    for side in 'lr':
        name='foot_'+side
        x=bones[name].head_local.x
        outline=[(-.048,-.185),(.048,-.185),(.061,-.145),(.062,-.072),
                 (.050,.063),(.030,.079),(-.034,.079),(-.052,.059),(-.058,-.080)]
        loops=[]
        for z,w,f in [(0,.91,1),(.014,1,1),(.030,.97,1),(.075,.92,.97),(.112,.85,.70),(.168,.77,.30)]:
            loops.append([(x+xx*w*scale,yy*(f if yy<0 else .96)*scale,z*scale) for xx,yy in outline])
        mesh.loft(loops,[name]*6,['dark','horn','flesh','flesh','dark','flesh'] if bare else
                  ['sole','sole','leather','dark','leather','dark'])
        if bare:
            for j in range(3):
                a=Vector((x+(j-1)*.032*scale,-.165*scale,.042*scale))
                mesh.limb(a,a+Vector((0,-.080,-.028))*scale,
                          [(0,.014*scale,.012*scale,0),(.55,.011*scale,.009*scale,0),(1,.002*scale,.002*scale,0)],
                          [name]*3,['horn','bone'],7)
        else:
            for z,y in [(.087,-.07),(.118,-.041)]:
                mesh.cord([(x-.033*scale,y*scale,z*scale),(x+.033*scale,y*scale,(z+.003)*scale)],
                          .004*scale,name,'brass')
    mesh.append(obj)


def npc_parts(rig,objects,pal):
    """Rebuild long body parts, leaving the reusable slot system intact."""
    bone=lambda n:rig.data.bones[n].head_local.copy()
    for o in objects:
        n=o.name
        mesh=Mesh(pal)
        if n.startswith('NPC_TORSO_'):
            armor='_01_' in n;priest='_02_' in n
            color='iron' if armor else 'robe' if priest else 'cloth'
            rings=[(0,.015,.855,.172,.104),(0,.013,.945,.175,.112),
                   (0,.011,1.055,.155,.113),(0,.005,1.195,.183,.125),
                   (0,.005,1.325,.211,.116),(0,-.004,1.409,.171,.088),
                   (0,-.014,1.458,.072,.067),(0,-.016,1.489,.059,.058)]
            weights=['hips','hips','spine',{'spine':.3,'chest':.7},'chest','chest','neck','neck']
            mesh.z_loft(rings,weights,[color,'dark',color,color,color,color,'skin','skin'],16)
            mesh.z_loft([(0,-.002,1.408,.126,.085),(0,-.012,1.438,.104,.08),
                         (0,-.016,1.463,.087,.071)],['chest']*3,['dark',color,color],16)
            if armor:
                for z,width,height in [(1.305,.170,.145),(1.151,.151,.091),(.995,.160,.052)]:
                    outline=[(-width,-.123,z+height),(0,-.148,z+height+.025),(width,-.123,z+height),
                             (width*.88,-.143,z),(0,-.157,z-.016),(-width*.88,-.143,z)]
                    mesh.plate(outline,.008,'chest' if z>1.16 else 'spine','iron','edge')
                for sign in (-1,1):
                    for z in (1.19,1.27,1.36):
                        mesh.ellipsoid((sign*.14,-.143,z),(.008,.004,.008),'chest','brass',6,3)
            elif priest:
                for sign in (-1,1):
                    mesh.plate([(sign*.125,-.108,1.416),(sign*.083,-.131,1.397),
                                (sign*.066,-.127,.877),(sign*.136,-.112,.873)],
                               .005,{'spine':.60,'chest':.40},'stole','brass')
                mesh.cord([(-.12,-.103,1.409),(0,-.150,1.294),(.12,-.103,1.409)],.004,'chest','brass')
                mesh.ellipsoid((0,-.154,1.291),(.018,.008,.025),'chest','brass',8,4)
            else:
                mesh.cord([(-.09,-.105,1.408),(0,-.130,1.365),(.09,-.105,1.408)],.004,'chest','stitch')
                for z in (1.10,1.20,1.30):
                    mesh.ellipsoid((0,-.125,z),(.007,.004,.007),'spine' if z<1.22 else 'chest','brass',6,3)
            mesh.fit(o)
        elif n.startswith(('NPC_UPPER_ARM_','NPC_FOREARM_','NPC_THIGH_','NPC_SHIN_')):
            side='l' if '_L_' in n else 'r'
            sign=1 if side=='l' else -1
            armor='_01_' in n;priest='_02_' in n
            if 'UPPER_ARM_' in n:
                a,b='upper_arm_'+side,'forearm_'+side
                sizes=[(-.12,.069,.065,0),(.16,.080,.072,0),(.45,.066,.062,.003),(.73,.056,.053,0),(1.10,.054,.050,0)]
            elif 'FOREARM_' in n:
                a,b='forearm_'+side,'hand_'+side
                sizes=[(-.12,.057,.053,0),(.20,.063,.058,0),(.46,.058,.051,.003),(.78,.047,.043,0),(1.10,.041,.037,0)]
                if priest:sizes=[(t,w*(1+.30*t),d*(1+.30*t),f) for t,w,d,f in sizes]
            elif 'THIGH_' in n:
                a,b='thigh_'+side,'shin_'+side
                sizes=[(-.12,.079,.082,0),(.17,.087,.078,0),(.43,.073,.069,0),(.75,.061,.060,.002),(1.1,.056,.054,0)]
            else:
                a,b='shin_'+side,'foot_'+side
                sizes=[(-.11,.059,.054,0),(.15,.060,.056,0),(.40,.055,.049,0),(.73,.044,.040,0),(1.1,.041,.038,0)]
            col='iron' if armor else 'robe' if priest else 'leather' if 'THIGH_' in n or 'SHIN_' in n else 'cloth'
            mesh.limb(bone(a),bone(b),sizes,[a]*len(sizes),['dark',col,col,col,'dark'],12)
            if armor:
                start,end=bone(a),bone(b)
                for j,t in enumerate((.18,.39,.60)):
                    pos=start.lerp(end,t)
                    mesh.limb(pos,pos+(end-start)*.13,
                              [(0,sizes[1][1]*1.05,sizes[1][2]*1.10,.002),(1,sizes[1][1],sizes[1][2],.003)],
                              [a]*2,['edge','iron'],12)
                if 'UPPER_ARM_' in n:
                    mesh.ellipsoid(bone(a)+Vector((sign*.015,0,.006)),(.112,.108,.091),a,'iron',12,5)
                    mesh.ellipsoid(bone(a)+Vector((sign*.028,-.015,.026)),(.117,.110,.044),a,'edge',12,4)
            else:
                for t in (.24,.76):
                    start=bone(a).lerp(bone(b),t)
                    w=sizes[1][1]*(1-.25*t)
                    mesh.limb(start,start+(bone(b)-bone(a))*.05,[(0,w*1.08,w*.95,0),(1,w*1.08,w*.95,0)],
                              [a]*2,['dark','leather' if not priest else 'stole'],12)
            mesh.fit(o)
        elif n.startswith('NPC_HAND_'):
            side='l' if '_L_' in n else 'r'
            a='hand_'+side;p=bone(a);sign=1 if side=='l' else -1
            mesh.limb(p,p+Vector((sign*.006,-.008,-.115)),[(0,.038,.033,0),(.24,.044,.036,0),(.62,.044,.035,0),(.94,.034,.028,.006)],
                      [a]*4,['leather','dark','leather','dark'],10)
            for j in range(4):
                x=p.x+(j-1.5)*.016
                mesh.limb((x,-.028,p.z-.075),(x,-.035,p.z-.120),
                          [(0,.009,.010,0),(.55,.010,.01,0),(1,.007,.008,0)],[a]*3,['leather','dark'],6)
            mesh.limb(p+Vector((-sign*.033,-.006,-.015)),p+Vector((-sign*.034,-.036,-.066)),
                      [(0,.018,.015,0),(.55,.015,.014,0),(1,.011,.010,0)],[a]*3,['leather'],7)
            mesh.fit(o)
        elif n.startswith('NPC_FOOT_'):
            side='l' if '_L_' in n else 'r';a='foot_'+side;x=bone(a).x
            outline=[(-.048,-.185),(.048,-.185),(.061,-.145),(.062,-.072),
                     (.050,.063),(.030,.079),(-.034,.079),(-.052,.059),(-.058,-.080)]
            loops=[]
            for z,w,f in [(0,.91,1),(.014,1,1),(.030,.97,1),(.075,.92,.97),(.112,.85,.70),(.168,.77,.30)]:
                loops.append([(x+xx*w,yy*(f if yy<0 else .96),z) for xx,yy in outline])
            mesh.loft(loops,[a]*6,['sole','sole','leather','dark','leather','dark'])
            for z,y in [(.088,-.07),(.118,-.041),(.145,-.025)]:
                mesh.cord([(x-.033,y,z),(x+.033,y,z+.003)],.004,a,'stitch')
            mesh.fit(o)
        elif n.startswith('NPC_PELVIS_'):
            col='iron' if '_01_' in n else 'leather'
            mesh.z_loft([(0,.015,.862,.182,.113),(0,.015,.897,.188,.115),
                         (0,.015,.944,.179,.115),(0,.015,.959,.171,.111)],
                        ['hips']*4,['dark',col,col,'dark'],16)
            mesh.plate([(-.043,-.106,.948),(.043,-.106,.948),(.049,-.111,.905),(-.049,-.111,.905)],
                       .009,'hips','brass','edge')
            mesh.fit(o)
        elif n.startswith('NPC_SKIRT_'):
            rings=[(0,.015,.924,.180,.115),(0,.018,.818,.198,.138),(0,.025,.641,.234,.171),
                   (0,.032,.440,.268,.208),(0,.037,.229,.294,.239),(0,.039,.075,.314,.258)]
            loops=[]
            for j,(x,y,z,rx,ry) in enumerate(rings):
                loops.append([(x+(rx+.010*cos(k*pi/2))*cos(k*pi/8),
                              y+(ry+.010*cos(k*pi/2))*sin(k*pi/8),z+.009*(j/5)*cos(k*1.7)) for k in range(16)])
            # Assign a matching cloth bone around the circumference at each ring.
            ids=[]
            for j,loop in enumerate(loops):
                row=[]
                t=j/(len(loops)-1)
                for k,pt in enumerate(loop):
                    x,y,_=pt
                    direction='skirt_l' if x>abs(y) else 'skirt_r' if -x>abs(y) else 'skirt_back' if y>0 else 'skirt_front'
                    row.append(mesh.vertex(pt,{'hips':1-.92*t,direction:.92*t}))
                ids.append(row)
            for j in range(len(ids)-1):
                for k in range(16):
                    kk=(k+1)%16
                    mesh.face((ids[j][k],ids[j][kk],ids[j+1][kk],ids[j+1][k]),'robe' if k%3 else 'panel')
            mesh.face(tuple(reversed(ids[0])),'dark');mesh.face(ids[-1],'dark')
            # The cloth is a closed shell, with a distinct hem and frontal panel.
            mesh.plate([(-.064,-.108,.92),(.064,-.108,.92),(.085,-.224,.27),(.073,-.250,.099),
                        (-.073,-.250,.099),(-.085,-.224,.27)],.004,{'hips':.32,'skirt_front':.68},'stole','brass')
            mesh.fit(o)
        else:
            if n.startswith(('NPC_HEAD_','NPC_HAIR_','NPC_HEADGEAR_','NPC_ACCESSORY_')):
                bevel(o,.0028 if 'HEAD_' in n or 'HAIR_' in n else .0035)
            if n.startswith('NPC_HEADGEAR_'):
                center=sum((v.co for v in o.data.vertices),Vector())/len(o.data.vertices)
                for sign in (-1,1):
                    for z in (1.59,1.69):
                        mesh.ellipsoid((sign*.162,-.083,z),(.006,.006,.006),'head','brass',6,3)
                mesh.append(o)
