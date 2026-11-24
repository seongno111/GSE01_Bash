"""Create review sheets from the actual exported corruption skins."""
import json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

P=Path(__file__).resolve().parents[1]
V=P/'previews'
catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))
font=lambda n:ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',n)
for suffix,file,size in [('', 'corrupted_forms.jpg',(300,335)),('_fusion','fusion_details.jpg',(384,312))]:
    width,height=size
    canvas=Image.new('RGB',(width*4,(height+38)*2),(23,25,32))
    draw=ImageDraw.Draw(canvas)
    for i,module in enumerate(catalog['modules']):
        x=i%4*width;y=i//4*(height+38)
        with Image.open(V/(module['weapon_kind']+suffix+'.png')) as image:
            canvas.paste(image.convert('RGB').resize(size,Image.Resampling.LANCZOS),(x,y+38))
        label=module['name_ko']+(' · 왼팔 융합' if module['side']=='l' else ' · 오른팔 융합')
        draw.text((x+12,y+7),label,font=font(18),fill=(237,223,213))
    canvas.save(V/file,quality=94)
clips={'longsword':'HERO_AttackLongsword','warhammer':'HERO_Hammer_SkillGroundSlam','bow':'HERO_Bow_Draw'}
specs={s['name']:s for s in catalog['common_clips']+catalog['weapon_clips']}
for kind,clip in clips.items():
    frames=[Image.open(path).convert('RGB') for path in sorted((V/'_frames'/kind).glob('*.png'))]
    assert len(frames)==25
    delay=max(20,round(specs[clip]['duration_seconds']*1000/(len(frames)-1)/10)*10)
    frames[0].save(V/(kind+'_motion.gif'),save_all=True,append_images=frames[1:],duration=delay,loop=0,optimize=False)
print('Composed eight-form and fusion detail sheets with three motion previews')
