"""Assemble the actual GLB renders for review."""
import json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

P=Path(__file__).resolve().parents[1]
V=P/'previews'
font=lambda n:ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',n)
bg=(23,26,32);fg=(235,225,206)
def sheet(filename,items,columns,width=300):
    height=round(width*520/440)
    rows=(len(items)+columns-1)//columns
    canvas=Image.new('RGB',(width*columns,(height+38)*rows),bg)
    draw=ImageDraw.Draw(canvas)
    for i,(key,label) in enumerate(items):
        x=i%columns*width;y=i//columns*(height+38)
        with Image.open(V/(key+'.png')) as image:
            canvas.paste(image.convert('RGB').resize((width,height),Image.Resampling.LANCZOS),(x,y+38))
        draw.text((x+12,y+7),label,font=font(18),fill=fg)
    canvas.save(V/filename,quality=94)

sheet('npc_lineup.jpg',[('hero','주인공 · 비율 기준'),('priest','성직자'),('knight','기사'),
      ('adventurer','모험가'),('raider','약탈자'),('merchant','상인')],6,260)
sheet('monster_lineup.jpg',[
    ('WitheredHollow','메마른 망자'),('TaintedRaider','오염된 약탈자'),('FieldWarden','경작지 수문장'),
    ('MutatedHuman','변이체'),('DeathEcho','죽음의 잔재'),('FallenPaladin','타락한 성기사'),
    ('FallenPriest','타락한 사제'),('HighDemonAzar','대공 아자르'),
    ('AshWolf','잿니 늑대'),('BlightBear','역병 등곰'),('ThornStag','가시관 사슴')],4,300)
for key in ('knight','AshWolf','FallenPaladin','HighDemonAzar'):
    frames=[Image.open(path).convert('RGB') for path in sorted((V/'_frames'/key).glob('*.png'))]
    assert len(frames)==20,key
    frames[0].save(V/(key+'_Walk.gif'),save_all=True,append_images=frames[1:],duration=60,loop=0,optimize=False)
print('Composed NPC and monster comparison sheets and walk previews')
