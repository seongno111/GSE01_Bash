"""Tailor the hero mesh and bind skeleton to a five-head figure."""
import bpy
from math import cos, sin, pi
from mathutils import Matrix, Vector

HEIGHT=1.84
HEAD_CHIN=1.475
HOOD_TOP=1.933
FOOT_X=.118
LAMP_SCALE=.74


def material(name, color, roughness=.85, metal=0):
    m=bpy.data.materials.new('MAT_HERO_'+name)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    shader=m.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value=(*color,1)
    shader.inputs['Roughness'].default_value=roughness
    shader.inputs['Metallic'].default_value=metal
    return m


class TailoredMesh:
    def __init__(self, palette):
        self.palette=palette
        self.vertices=[]
        self.weights=[]
        self.faces=[]
        self.materials=[]

    def vertex(self, point, weights):
        self.vertices.append(tuple(point))
        self.weights.append({weights:1} if isinstance(weights,str) else weights)
        return len(self.vertices)-1

    def face(self, indices, color):
        self.faces.append(tuple(indices))
        self.materials.append(color)

    def loft(self, loops, weights, colors, caps=True):
        ring_ids=[[self.vertex(p,w) for p in loop] for loop,w in zip(loops,weights)]
        sides=len(loops[0])
        if caps:self.face(reversed(ring_ids[0]),colors[0])
        for i,(a,b) in enumerate(zip(ring_ids,ring_ids[1:])):
            color=colors[min(i,len(colors)-1)]
            for j in range(sides):
                k=(j+1)%sides
                self.face((a[j],a[k],b[k],b[j]),color(j) if callable(color) else color)
        if caps:self.face(ring_ids[-1],colors[-1])

    def z_loft(self, rings, weights, colors, sides=12):
        self.loft([[(x+rx*cos(2*pi*j/sides),y+ry*sin(2*pi*j/sides),z)
                    for j in range(sides)] for x,y,z,rx,ry in rings],weights,colors)

    def limb(self, a, b, rings, weights, colors, sides=10):
        a,b=Vector(a),Vector(b)
        axis=(b-a).normalized()
        front=Vector((0,-1,0))
        front=(front-axis*front.dot(axis)).normalized()
        across=front.cross(axis)
        loops=[]
        for t,width,depth,fold in rings:
            center=a.lerp(b,t)+front*fold
            loops.append([center+across*(width*cos(2*pi*j/sides))+front*(depth*sin(2*pi*j/sides))
                          for j in range(sides)])
        self.loft(loops,weights,colors)

    def cord(self, points, radius, weight, color):
        for a,b in zip(points,points[1:]):
            if (Vector(b)-Vector(a)).length>.00001:
                self.limb(a,b,[(0,radius,radius,0),(1,radius,radius,0)],
                          [weight,weight],[color],sides=5)

    def plate(self, outline, depth, weight, fill, rim):
        center=sum((Vector(p) for p in outline),Vector())/len(outline)
        loops=[[Vector(v)+Vector((0,depth,0)) for v in outline],outline,
               [center+(Vector(v)-center)*.76+Vector((0,-depth*.30,0)) for v in outline]]
        self.loft(loops,[weight]*3,[rim,rim,fill])

    def fit(self, obj):
        old=obj.data
        mesh=bpy.data.meshes.new(old.name+'_Tailored')
        mesh.from_pydata(self.vertices,[],self.faces)
        keys=list(dict.fromkeys(self.materials))
        for key in keys:mesh.materials.append(self.palette[key])
        for polygon,key in zip(mesh.polygons,self.materials):polygon.material_index=keys.index(key)
        mesh.update()
        obj.data=mesh
        obj.matrix_world=Matrix.Identity(4)
        obj.vertex_groups.clear()
        groups={}
        for i,weight in enumerate(self.weights):
            total=sum(weight.values())
            for bone,value in weight.items():
                if not value:continue
                if bone not in groups:groups[bone]=obj.vertex_groups.new(name=bone)
                groups[bone].add([i],value/total,'REPLACE')
        bpy.data.meshes.remove(old)
        mesh.name=obj.name+'_GEO'
        # Consistent outward normals for the closed shells and stitched pieces.
        import bmesh
        bm=bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(mesh)
        bm.free()


def reshape_hero(rig, objects, lantern_light):
    old={b.name:b.matrix_local.copy() for b in rig.data.bones}
    tips={b.name:b.tail_local.copy() for b in rig.data.bones}
    heads={name:m.translation.copy() for name,m in old.items()}
    new={
        'root':(0,0,0),'hips':(0,.018,.934),'spine':(0,.015,1.035),
        'chest':(0,.009,1.226),'neck':(0,-.014,1.437),'head':(0,-.019,1.489),
        'jaw':(0,-.088,1.558),'skirt_front':(0,-.113,.948),
        'skirt_back':(0,.154,.975),'skirt_l':(.154,.046,.958),
        'skirt_r':(-.154,.046,.958),'tail':(0,.151,.945),'tail_tip':(0,.426,.77),
    }
    for s,sign in [('l',1),('r',-1)]:
        new.update({
            'clavicle_'+s:(0,-.004,1.416),'upper_arm_'+s:(sign*.224,0,1.405),
            'forearm_'+s:(sign*.300,.016,1.133),'hand_'+s:(sign*.335,.002,.878),
            'weapon_'+s:(sign*.339,.000,.830),
            'thigh_'+s:(sign*.101,.013,.879),'shin_'+s:(sign*.111,-.011,.486),
            'foot_'+s:(sign*FOOT_X,0,.113),'toe_'+s:(sign*FOOT_X,-.156,.047),
        })
    for name in ('lamp_pivot','lamp_body'):
        new[name]=Vector(new['weapon_l'])+(heads[name]-heads['weapon_l'])*LAMP_SCALE
    new={n:Vector(p) for n,p in new.items()}
    children={'hips':'spine','spine':'chest','chest':'neck','neck':'head','tail':'tail_tip'}
    for s in 'lr':
        for a,b in [('clavicle','upper_arm'),('upper_arm','forearm'),('forearm','hand'),
                    ('thigh','shin'),('shin','foot'),('foot','toe')]:
            children[a+'_'+s]=b+'_'+s
    light_local=lantern_light.matrix_world.translation.copy() if lantern_light else None
    bpy.context.view_layer.objects.active=rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for b in rig.data.edit_bones:b.use_connect=False
    for b in rig.data.edit_bones:
        n=b.name
        b.head=new[n]
        if n=='head':b.tail=(0,-.018,HEIGHT)
        elif n.startswith('hand_'):b.tail=new[n]+(tips[n]-heads[n])*.80
        elif n in children:b.tail=new[children[n]]
        elif n.startswith('skirt_'):b.tail=new[n]+(tips[n]-heads[n])*.92
        elif n.startswith('lamp_'):b.tail=new[n]+(tips[n]-heads[n])*LAMP_SCALE
        else:b.tail=new[n]+(tips[n]-heads[n])
        b.align_roll(old[n].to_3x3().col[2])
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.context.view_layer.update()

    palette={k:bpy.data.materials['MAT_HERO_'+k] for k in
             ('cloth','cloth_worn','leather','leather_dark','skin','iron','edge','brass')}
    palette.update(panel=material('ash_twilled_cloth',(.062,.080,.102)),
                   dark=material('cloak_shadow',(.024,.035,.046)),
                   seam=material('linen_stitch',(.176,.139,.094)),
                   boot=material('boot_waxed_leather',(.067,.046,.032)),
                   sole=material('boot_sole',(.029,.023,.019)))
    bone=lambda n:new[n]

    def locate(prefix):
        return next(o for o in objects if o.name.startswith('HERO_'+prefix))

    def adapt(obj, fn):
        matrix=obj.matrix_world.copy()
        for v in obj.data.vertices:v.co=fn(matrix@v.co)
        obj.matrix_world=Matrix.Identity(4)
        obj.data.update()

    def bevel(obj, width):
        bpy.context.view_layer.objects.active=obj
        modifier=obj.modifiers.new('Cut edges','BEVEL')
        modifier.width=width
        modifier.segments=1
        modifier.limit_method='ANGLE'
        modifier.angle_limit=.62
        bpy.ops.object.modifier_move_up(modifier=modifier.name)
        bpy.ops.object.modifier_apply(modifier=modifier.name)

    adapt(locate('HEAD_'),lambda p:(p.x*.50,p.y*.58-.02,HEAD_CHIN+(p.z-1.21)*(HEIGHT-HEAD_CHIN)/.58))
    hood=locate('HEADGEAR_')
    adapt(hood,lambda p:(p.x*(.53-.057*max(0,(p.z-1.70)/.26)),
                         p.y*.53-.019,1.456+(p.z-1.175)*(HOOD_TOP-1.456)/.785))
    bevel(hood,.0045)
    for s,sign in [('l',1),('r',-1)]:
        shoulder=locate('PAULDRON_'+s.upper()+'_')
        name='upper_arm_'+s
        adapt(shoulder,lambda p:bone(name)+Vector(((p.x-heads[name].x)*.79,
                                                    (p.y-heads[name].y)*.86,(p.z-heads[name].z)*.68)))
        bevel(shoulder,.003)
    for obj in [o for o in objects if 'WEAPON_' in o.name]:
        side='l' if 'WEAPON_L_' in obj.name else 'r'
        name='weapon_'+side
        scale=LAMP_SCALE if side=='l' else .88
        adapt(obj,lambda p:bone(name)+(p-heads[name])*scale)
    if lantern_light:
        matrix=lantern_light.matrix_world.copy()
        matrix.translation=bone('weapon_l')+(light_local-heads['weapon_l'])*LAMP_SCALE
        lantern_light.matrix_world=matrix
        lantern_light.data.energy*=.74

    mesh=TailoredMesh(palette)
    torso=[(0,.019,.848,.167,.100),(0,.019,.897,.186,.109),(0,.017,.985,.173,.108),
           (0,.013,1.061,.156,.112),(0,.006,1.154,.173,.128),(0,.001,1.264,.195,.126),
           (0,.008,1.365,.212,.116),(0,.000,1.425,.159,.092),(0,-.015,1.463,.073,.069),
           (0,-.016,1.488,.059,.059)]
    weights=['hips','hips','hips',{'hips':.25,'spine':.75},'spine',
             {'spine':.28,'chest':.72},'chest','chest',{'chest':.8,'neck':.2},'neck']
    mesh.z_loft(torso,weights,['dark','panel','panel','cloth','panel','panel','cloth','cloth_worn','skin','skin'],16)
    mesh.z_loft([(0,-.004,1.414,.125,.088),(0,-.014,1.435,.104,.086),
                 (0,-.015,1.459,.096,.075)],['chest']*3,['dark','cloth_worn','cloth'],14)
    mesh.cord([(-.107,-.087,1.433),(0,-.113,1.408),(.107,-.087,1.433)],.006,'chest','cloth_worn')
    mesh.plate([(-.026,-.114,1.411),(.026,-.114,1.411),(.019,-.12,1.378),(-.019,-.12,1.378)],
               .008,'chest','brass','iron')
    mesh.fit(locate('TORSO_'))

    mesh=TailoredMesh(palette)
    mesh.z_loft([(0,.015,.891,.191,.113),(0,.016,.907,.194,.115),(0,.015,.959,.182,.115),
                 (0,.014,.972,.174,.113)],['hips']*4,['sole','leather_dark','leather','leather_dark'],16)
    mesh.plate([(-.046,-.106,.958),(.044,-.106,.958),(.052,-.109,.919),
                (.037,-.113,.902),(-.04,-.112,.902),(-.052,-.109,.918)],.01,'hips','iron','brass')
    mesh.cord([(-.026,-.12,.931),(.034,-.12,.931)],.0035,'hips','brass')
    mesh.fit(locate('PELVIS_'))

    mesh=TailoredMesh(palette)
    path=[(-.158,-.091,.968),(-.086,-.110,1.068),(.006,-.134,1.182),
          (.087,-.125,1.304),(.163,-.095,1.399)]
    strap_weights=['hips',{'hips':.25,'spine':.75},'spine',{'spine':.25,'chest':.75},'chest']
    for i,(a,b) in enumerate(zip(path,path[1:])):
        a,b=Vector(a),Vector(b)
        side=Vector((b.z-a.z,0,-(b.x-a.x))).normalized()*.019
        rings=[[p-side,p+side,p+side+Vector((0,.008,0)),p-side+Vector((0,.008,0))] for p in (a,b)]
        mesh.loft(rings,strap_weights[i:i+2],['leather_dark'])
        mesh.cord([a-side*.76,b-side*.76],.0025,strap_weights[i],'seam')
    mesh.plate([(.053,-.139,1.278),(.093,-.129,1.257),(.113,-.122,1.286),(.069,-.13,1.308)],.007,
               {'chest':.75,'spine':.25},'leather','brass')
    mesh.fit(locate('HARNESS_'))

    mesh=TailoredMesh(palette)
    mesh.z_loft([(.179,.061,.834,.044,.035),(.187,.065,.862,.064,.045),
                 (.185,.064,.951,.061,.046),(.176,.061,.978,.046,.039)],
                ['hips']*4,['leather_dark','leather','leather_dark','leather_dark'],8)
    mesh.plate([(.136,.015,.95),(.225,.015,.95),(.213,.013,.898),(.151,.013,.892)],.007,'hips','leather','sole')
    mesh.cord([(.174,.006,.912),(.187,.006,.912)],.006,'hips','brass')
    mesh.fit(locate('WAIST_'))

    for s,sign in [('l',1),('r',-1)]:
        label=s.upper()
        upper,fore,hand,thigh,shin,foot=[k+'_'+s for k in ('upper_arm','forearm','hand','thigh','shin','foot')]
        mesh=TailoredMesh(palette)
        mesh.limb(bone(upper),bone(fore),[(-.08,.068,.066,0),(.20,.076,.070,0),
                  (.50,.064,.059,0),(.76,.056,.052,.004),(1.09,.052,.049,.001)],
                  [upper,upper,upper,{upper:.8,fore:.2},{upper:.45,fore:.55}],
                  ['dark','cloth','panel','cloth','cloth'],12)
        mesh.fit(locate('UPPER_ARM_'+label+'_'))
        mesh=TailoredMesh(palette)
        mesh.limb(bone(fore),bone(hand),[(-.11,.058,.051,0),(.12,.058,.056,0),(.28,.062,.052,.003),
                  (.62,.049,.045,.006),(.85,.049,.043,0),(1.04,.040,.036,0)],
                  [{upper:.34,fore:.66},fore,fore,fore,fore,{fore:.83,hand:.17}],
                  ['cloth','leather_dark','leather','leather','sole','boot'],12)
        for t in (.30,.76):
            mesh.limb(bone(fore),bone(hand),[(t-.034,.063 if t<.5 else .052,.059 if t<.5 else .050,0),
                      (t+.032,.062 if t<.5 else .052,.058 if t<.5 else .050,0)],
                      [fore,fore],['iron','leather_dark'],10)
        x=sign*.325
        mesh.plate([(x-.038,-.046,1.087),(x+.03,-.046,1.081),(x+.034,-.044,.974),
                    (x,-.051,.943),(x-.035,-.044,.968)],.008,fore,'iron','edge')
        mesh.fit(locate('FOREARM_'+label+'_'))
        mesh=TailoredMesh(palette)
        palm=bone(hand)
        mesh.limb(palm,palm+Vector((sign*.007,-.007,-.106)),[(0,.039,.034,0),(.27,.044,.036,0),
                  (.59,.046,.037,0),(.84,.040,.032,.006),(1,.031,.027,.008)],
                  [hand]*5,['sole','boot','leather_dark','leather_dark'],10)
        for j in range(4):
            x=palm.x+(j-1.5)*.016
            mesh.limb((x,-.032,.811),(x,-.031,.783),[(0,.009,.009,0),(1,.007,.007,0)],
                      [hand,hand],['leather_dark'],5)
        mesh.limb((palm.x-sign*.031,-.012,.853),(palm.x-sign*.033,-.04,.805),
                  [(0,.017,.015,0),(.56,.015,.014,0),(1,.010,.010,0)],[hand]*3,['boot'],7)
        mesh.fit(locate('HAND_'+label+'_'))
        mesh=TailoredMesh(palette)
        mesh.limb(bone(thigh),bone(shin),[(-.09,.081,.084,0),(.12,.085,.080,0),
                  (.37,.074,.069,0),(.64,.063,.063,0),(.84,.057,.056,.004),(1.09,.055,.052,.003)],
                  [{'hips':.35,thigh:.65},thigh,thigh,thigh,{thigh:.83,shin:.17},{thigh:.45,shin:.55}],
                  ['leather_dark','leather','leather','leather','boot','leather_dark'],12)
        mesh.fit(locate('THIGH_'+label+'_'))
        mesh=TailoredMesh(palette)
        mesh.limb(bone(shin),bone(foot),[(-.12,.057,.053,0),(.12,.057,.053,0),(.33,.059,.052,-.003),
                  (.57,.052,.045,-.003),(.83,.045,.039,0),(1.04,.040,.037,0)],
                  [{thigh:.49,shin:.51},shin,shin,shin,shin,{shin:.80,foot:.20}],
                  ['leather_dark','leather','boot','leather_dark','leather_dark','boot'],12)
        x=bone(shin).x
        mesh.plate([(x-.046,-.058,.521),(x+.045,-.058,.521),(x+.053,-.059,.482),
                    (x+.035,-.057,.430),(x,-.059,.416),(x-.039,-.057,.437),(x-.051,-.06,.482)],
                   .013,{thigh:.24,shin:.76},'iron','edge')
        mesh.fit(locate('SHIN_'+label+'_'))
        mesh=TailoredMesh(palette)
        outline=[(-.044,-.190),(.043,-.190),(.059,-.160),(.063,-.091),(.051,.052),
                 (.035,.075),(-.036,.075),(-.051,.052),(-.058,-.083),(-.057,-.154)]
        loops=[]
        for z,w,front,back in [(0,.92,1,1),(.014,1,1,1),(.027,.97,1,1),
                               (.064,.93,.99,.98),(.105,.85,.72,.94),(.151,.76,.34,.92),
                               (.176,.81,.31,.94)]:
            loops.append([(sign*FOOT_X+x*w,y*(front if y<0 else back),z) for x,y in outline])
        mesh.loft(loops,[foot]*len(loops),['sole','sole','boot','leather_dark','boot','leather_dark','boot'])
        mesh.cord([(sign*FOOT_X-.042,-.083,.101),(sign*FOOT_X+.042,-.083,.101)],.006,foot,'leather')
        mesh.fit(locate('FOOT_'+label+'_'))

    mesh=TailoredMesh(palette)
    columns=17
    rows=[(1.427,.181,.096),(1.384,.237,.123),(1.273,.257,.144),(1.13,.242,.145),
          (.98,.251,.158),(.824,.284,.188),(.665,.317,.222),(.505,.347,.267),(.347,.368,.302),
          (.222,.390,.344)]
    layers=[]
    for rear in (0,1):
        layer=[]
        for row,(z,width,depth) in enumerate(rows):
            row_ids=[]
            t=row/(len(rows)-1)
            for j in range(columns):
                u=2*j/(columns-1)-1
                folds=(.006+.023*t)*cos(u*5.2*pi+.67*t)+.014*t*cos(u*2*pi)
                zz=z+.048*u*u*t
                if row==len(rows)-1:
                    zz+=.034*cos(j*2.17)+.022*sin(j*1.31)
                w={'chest':max(0,1-t*2.7),'hips':max(0,1-abs(t-.41)*2.2)*.58}
                h=max(0,(t-.32)/.68)
                w['skirt_back']=h*(1-abs(u)*.72)
                w['skirt_l' if u>=0 else 'skirt_r']=h*abs(u)*.86
                point=(width*u,depth-.048*u*u+folds+.008*rear,zz)
                row_ids.append(mesh.vertex(point,w))
            layer.append(row_ids)
        layers.append(layer)
    a,b=layers
    for r in range(len(rows)-1):
        for j in range(columns-1):
            face=(a[r][j],a[r+1][j],a[r+1][j+1],a[r][j+1])
            mesh.face(face,'cloth' if j%4 in (0,1) else 'panel')
            mesh.face((b[r][j+1],b[r+1][j+1],b[r+1][j],b[r][j]),'dark')
        for j in (0,columns-1):mesh.face((a[r][j],a[r+1][j],b[r+1][j],b[r][j]),'cloth_worn')
    for r in (0,len(rows)-1):
        for j in range(columns-1):mesh.face((a[r][j],b[r][j],b[r][j+1],a[r][j+1]),'cloth_worn')
    mesh.fit(locate('CAPE_'))
    return old
